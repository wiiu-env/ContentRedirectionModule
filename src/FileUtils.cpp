#include "FileUtils.h"
#include "FSWrapper.h"
#include "IFSWrapper.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include <coreinit/cache.h>
#include <coreinit/filesystem_fsa.h>
#include <coreinit/thread.h>
#include <malloc.h>
#include <map>
#include <unistd.h>

std::string getFullPathGeneric(std::shared_ptr<LayerInfo> &layerInfo, FSAClientHandle client, const char *path) {
    std::lock_guard<std::mutex> workingDirLock(layerInfo->mutex);

    std::string res;

    if (path[0] != '/' && path[0] != '\\') {
        if (layerInfo->workingDirs.count(client) == 0) {
            DEBUG_FUNCTION_LINE_WARN("No working dir found for client %08X, fallback to \"/\"", client);
            layerInfo->workingDirs[client] = "/";
        }
        res = string_format("%s%s", layerInfo->workingDirs.at(client).c_str(), path);
    } else {
        res = path;
    }

    std::replace(res.begin(), res.end(), '\\', '/');

    return res;
}

void setWorkingDirGeneric(FSAClientHandle client, const char *path, std::mutex &mutex, std::map<FSAClientHandle, std::string> &map) {
    if (!path) {
        DEBUG_FUNCTION_LINE_WARN("Path was NULL");
        return;
    }

    std::lock_guard<std::mutex> workingDirLock(mutex);

    std::string cwd(path);
    if (cwd.empty() || cwd.back() != '/') {
        cwd.push_back('/');
    }
    map[client] = cwd;
    OSMemoryBarrier();
}


std::string getFullPath(std::shared_ptr<LayerInfo> &layerInfo, FSAClientHandle pClient, const char *path) {
    return getFullPathGeneric(layerInfo, pClient, path);
}

void setWorkingDir(std::shared_ptr<LayerInfo> &layerInfo, FSAClientHandle client, const char *path) {
    setWorkingDirGeneric(client, path, layerInfo->workingDirMutex, layerInfo->workingDirs);
}

void clearFSLayer(std::shared_ptr<LayerInfo> &layerInfo) {
    {
        std::lock_guard<std::mutex> workingDirLock(layerInfo->workingDirMutex);
        layerInfo->workingDirs.clear();
    }
    {
        std::lock_guard<std::mutex> layerLock(layerInfo->mutex);
        layerInfo->layers.clear();
    }
}

void clearFSLayers() {
    for (auto &[upid, layerInfo] : sLayerInfoForUPID) {
        clearFSLayer(layerInfo);
    }
}

bool sendMessageToThread(std::shared_ptr<LayerInfo> &layerInfo, FSShimWrapperMessage *param) {
    auto *curThread = &layerInfo->threadData[OSGetCoreId()];
    if (curThread->setup) {
        OSMessage send;
        send.message = param;
        send.args[0] = FS_IO_QUEUE_COMMAND_PROCESS_FS_COMMAND;
        auto res     = OSSendMessage(&curThread->queue, &send, OS_MESSAGE_FLAGS_NONE);
        if (!res) {
            DEBUG_FUNCTION_LINE_ERR("Message Queue for ContentRedirection IO Thread is full");
            OSFatal("ContentRedirectionModule: Message Queue for ContentRedirection IO Thread is full");
        }
        return res;
    } else {
        DEBUG_FUNCTION_LINE_ERR("Thread not setup");
        OSFatal("ContentRedirectionModule: Thread not setup");
    }
    return false;
}

std::map<uint32_t, std::shared_ptr<LayerInfo>> sLayerInfoForUPID;

FSError doForLayer(FSShimWrapper *param) {
    if (!sLayerInfoForUPID.contains(param->upid)) {
        DEBUG_FUNCTION_LINE_ERR("INVALID UPID IN SHIMWRAPPER: %d", param->upid);
        OSFatal("Invalid UPID");
    }
    auto &layerInfo = sLayerInfoForUPID[param->upid];

    std::lock_guard<std::mutex> lock(layerInfo->mutex);
    if (!layerInfo->layers.empty()) {
        uint32_t startIndex = layerInfo->layers.size();
        for (uint32_t i = layerInfo->layers.size(); i > 0; i--) {
            if ((uint32_t) layerInfo->layers[i - 1]->getLayerId() == param->shim->clientHandle) {
                startIndex = i - 1;
                break;
            }
        }

        if (startIndex > 0) {
            for (uint32_t i = startIndex; i > 0; i--) {
                auto &layer = layerInfo->layers[i - 1];
                if (!layer->isActive()) {
                    continue;
                }
                auto layerResult = FS_ERROR_FORCE_PARENT_LAYER;
                auto command     = (FSACommandEnum) param->shim->command;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
                switch (command) {
                    case FSA_COMMAND_OPEN_DIR: {
                        auto *request = &param->shim->request.openDir;
                        auto fullPath = getFullPath(layerInfo, (FSAClientHandle) param->shim->clientHandle, request->path);
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] OpenDir: %s (full path: %s)", layer->getName().c_str(), request->path, fullPath.c_str());
                        // Hacky solution:
                        auto *hackyBuffer = (uint32_t *) &param->shim->response;
                        auto *handlePtr   = (FSDirectoryHandle *) hackyBuffer[1];
                        layerResult       = layer->FSOpenDirWrapper(fullPath.c_str(), handlePtr);
                        break;
                    }
                    case FSA_COMMAND_READ_DIR: {
                        auto *request = &param->shim->request.readDir;
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] ReadDir: %08X", layer->getName().c_str(), request->handle);
                        // Hacky solution:
                        auto *hackyBuffer = (uint32_t *) &param->shim->response;
                        auto *dirEntryPtr = (FSADirectoryEntry *) hackyBuffer[1];
                        layerResult       = layer->FSReadDirWrapper(request->handle, dirEntryPtr);
                        break;
                    }
                    case FSA_COMMAND_CLOSE_DIR: {
                        auto *request = &param->shim->request.closeDir;
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] CloseDir: %08X", layer->getName().c_str(), request->handle);
                        layerResult = layer->FSCloseDirWrapper(request->handle);
                        if (layerResult != FS_ERROR_FORCE_REAL_FUNCTION && layerResult != FS_ERROR_FORCE_PARENT_LAYER) {
                            if (layer->isValidDirHandle(request->handle)) {
                                layer->deleteDirHandle(request->handle);
                            } else {
                                DEBUG_FUNCTION_LINE_ERR("[%s] Expected to delete dirHandle by %08X but it was not found", layer->getName().c_str(), request->handle);
                            }
                        }
                        break;
                    }
                    case FSA_COMMAND_REWIND_DIR: {
                        auto *request = &param->shim->request.rewindDir;
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] RewindDir: %08X", layer->getName().c_str(), request->handle);
                        layerResult = layer->FSRewindDirWrapper(request->handle);
                        break;
                    }
                    case FSA_COMMAND_MAKE_DIR: {
                        auto *request = &param->shim->request.makeDir;
                        auto fullPath = getFullPath(layerInfo, (FSAClientHandle) param->shim->clientHandle, request->path);
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] MakeDir: %s (full path: %s)", layer->getName().c_str(), request->path, fullPath.c_str());
                        layerResult = layer->FSMakeDirWrapper(fullPath.c_str());
                        break;
                    }
                    case FSA_COMMAND_OPEN_FILE: {
                        auto *request = &param->shim->request.openFile;
                        auto fullPath = getFullPath(layerInfo, (FSAClientHandle) param->shim->clientHandle, request->path);
                        // Hacky solution:
                        auto *hackyBuffer = (uint32_t *) &param->shim->response;
                        auto *handlePtr   = (FSFileHandle *) hackyBuffer[1];
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] OpenFile: path %s (full path: %s) mode %s", layer->getName().c_str(), request->path, fullPath.c_str(), request->mode);
                        layerResult = layer->FSOpenFileWrapper(fullPath.c_str(), request->mode, handlePtr);
                        break;
                    }
                    case FSA_COMMAND_CLOSE_FILE: {
                        auto *request = &param->shim->request.closeFile;
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] CloseFile: %08X", layer->getName().c_str(), request->handle);
                        layerResult = layer->FSCloseFileWrapper(request->handle);
                        if (layerResult != FS_ERROR_FORCE_REAL_FUNCTION && layerResult != FS_ERROR_FORCE_PARENT_LAYER) {
                            if (layer->isValidFileHandle(request->handle)) {
                                layer->deleteFileHandle(request->handle);
                            } else {
                                DEBUG_FUNCTION_LINE_ERR("[%s] Expected to delete fileHandle by handle %08X but it was not found", layer->getName().c_str(), request->handle);
                            }
                        }
                        break;
                    }
                    case FSA_COMMAND_GET_INFO_BY_QUERY: {
                        auto *request = &param->shim->request.getInfoByQuery;
                        if (request->type == FSA_QUERY_INFO_STAT) {
                            auto fullPath = getFullPath(layerInfo, (FSAClientHandle) param->shim->clientHandle, request->path);
                            DEBUG_FUNCTION_LINE_VERBOSE("[%s] GetStat: %s (full path: %s)", layer->getName().c_str(), request->path, fullPath.c_str());
                            // Hacky solution:
                            auto *hackyBuffer = (uint32_t *) &param->shim->response;
                            auto *statPtr     = (FSStat *) hackyBuffer[1];
                            layerResult       = layer->FSGetStatWrapper(fullPath.c_str(), statPtr);
                        }
                        break;
                    }
                    case FSA_COMMAND_STAT_FILE: {
                        auto *request = &param->shim->request.statFile;
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] GetStatFile: %08X", layer->getName().c_str(), request->handle);
                        // Hacky solution:
                        auto *hackyBuffer = (uint32_t *) &param->shim->response;
                        auto *statPtr     = (FSStat *) hackyBuffer[1];
                        layerResult       = layer->FSGetStatFileWrapper(request->handle, statPtr);
                        break;
                    }
                    case FSA_COMMAND_READ_FILE: {

                        auto *request = &param->shim->request.readFile;
                        if (request->readFlags == FSA_READ_FLAG_NONE) {
                            DEBUG_FUNCTION_LINE_VERBOSE("[%s] ReadFile: buffer %08X size %08X count %08X handle %08X", layer->getName().c_str(), request->buffer, request->size, request->count, request->handle);
                            layerResult = layer->FSReadFileWrapper(request->buffer, request->size, request->count, request->handle, 0);
                        } else if (request->readFlags == FSA_READ_FLAG_READ_WITH_POS) {
                            DEBUG_FUNCTION_LINE_VERBOSE("[%s] ReadFileWithPos: buffer %08X size %08X count %08X pos %08X handle %08X", layer->getName().c_str(), request->buffer, request->size, request->count, request->pos, request->handle);
                            layerResult = layer->FSReadFileWithPosWrapper(request->buffer, request->size, request->count, request->pos, request->handle, 0);
                        }
                        break;
                    }
                    case FSA_COMMAND_SET_POS_FILE: {

                        auto *request = &param->shim->request.setPosFile;
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] SetPosFile: %08X %08X", layer->getName().c_str(), request->handle, request->pos);
                        layerResult = layer->FSSetPosFileWrapper(request->handle, request->pos);
                        break;
                    }
                    case FSA_COMMAND_GET_POS_FILE: {
                        auto *request = &param->shim->request.getPosFile;
                        // Hacky solution:
                        auto *hackyBuffer = (uint32_t *) &param->shim->response;
                        auto *posPtr      = (FSAFilePosition *) hackyBuffer[1];
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] GetPosFile: %08X", layer->getName().c_str(), request->handle);
                        layerResult = layer->FSGetPosFileWrapper(request->handle, posPtr);
                        break;
                    }
                    case FSA_COMMAND_IS_EOF: {
                        auto *request = &param->shim->request.isEof;
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] IsEof: %08X", layer->getName().c_str(), request->handle);
                        layerResult = layer->FSIsEofWrapper(request->handle);
                        break;
                    }
                    case FSA_COMMAND_TRUNCATE_FILE: {

                        auto *request = &param->shim->request.truncateFile;
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] TruncateFile: %08X", layer->getName().c_str(), request->handle);
                        layerResult = layer->FSTruncateFileWrapper(request->handle);
                        break;
                    }
                    case FSA_COMMAND_WRITE_FILE: {

                        auto *request = &param->shim->request.writeFile;
                        if (request->writeFlags == FSA_WRITE_FLAG_NONE) {
                            DEBUG_FUNCTION_LINE_VERBOSE("[%s] WriteFile: buffer %08X size %08X count %08X handle %08X", layer->getName().c_str(), request->buffer, request->size, request->count, request->handle);
                            layerResult = layer->FSWriteFileWrapper(request->buffer, request->size, request->count, request->handle, 0);
                        } else if (request->writeFlags == FSA_WRITE_FLAG_READ_WITH_POS) {
                            DEBUG_FUNCTION_LINE_VERBOSE("[%s] WriteFileWithPos: buffer %08X size %08X count %08X pos %08X handle %08X", layer->getName().c_str(), request->buffer, request->size, request->count, request->pos, request->handle);
                            layerResult = layer->FSWriteFileWithPosWrapper(request->buffer, request->size, request->count, request->pos, request->handle, 0);
                        }
                        break;
                    }
                    case FSA_COMMAND_REMOVE: {
                        auto *request = &param->shim->request.remove;
                        auto fullPath = getFullPath(layerInfo, (FSAClientHandle) param->shim->clientHandle, request->path);
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Remove: %s (full path: %s)", layer->getName().c_str(), request->path, fullPath.c_str());
                        layerResult = layer->FSRemoveWrapper(fullPath.c_str());
                        break;
                    }
                    case FSA_COMMAND_RENAME: {
                        auto *request    = &param->shim->request.rename;
                        auto fullOldPath = getFullPath(layerInfo, (FSAClientHandle) param->shim->clientHandle, request->oldPath);
                        auto fullNewPath = getFullPath(layerInfo, (FSAClientHandle) param->shim->clientHandle, request->newPath);
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Rename: %s -> %s (full path: %s -> %s)", layer->getName().c_str(), request->oldPath, request->newPath, fullOldPath.c_str(), fullNewPath.c_str());
                        layerResult = layer->FSRenameWrapper(fullOldPath.c_str(), fullNewPath.c_str());
                        break;
                    }
                    case FSA_COMMAND_FLUSH_FILE: {
                        auto *request = &param->shim->request.flushFile;
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] FlushFile: %08X", layer->getName().c_str(), request->handle);
                        layerResult = layer->FSFlushFileWrapper(request->handle);
                        break;
                    }
                    case FSA_COMMAND_CHANGE_DIR: {
                        auto *request = &param->shim->request.changeDir;
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] ChangeDir: %s", layer->getName().c_str(), request->path);
                        setWorkingDir(layerInfo, (FSAClientHandle) param->shim->clientHandle, request->path);
                        // We still want to call the original function.
                        layerResult = FS_ERROR_FORCE_PARENT_LAYER;
                        break;
                    }
                    case FSA_COMMAND_GET_CWD: {
                        DEBUG_FUNCTION_LINE_WARN("FSA_COMMAND_GET_CWD hook not implemented");
                        break;
                    }
                    case FSA_COMMAND_APPEND_FILE: {
                        auto *request = &param->shim->request.appendFile;
                        DEBUG_FUNCTION_LINE_WARN("FSA_COMMAND_APPEND_FILE hook not implemented for handle %08X", request->handle);
                        break;
                    }
                    case FSA_COMMAND_FLUSH_MULTI_QUOTA: {
                        DEBUG_FUNCTION_LINE_WARN("FSA_COMMAND_FLUSH_MULTI_QUOTA hook not implemented");
                        break;
                    }
                    case FSA_COMMAND_OPEN_FILE_BY_STAT: {
                        DEBUG_FUNCTION_LINE_WARN("FSA_COMMAND_OPEN_FILE_BY_STAT hook not implemented");
                        break;
                    }
                    case FSA_COMMAND_CHANGE_OWNER: {
                        DEBUG_FUNCTION_LINE_WARN("FSA_COMMAND_CHANGE_OWNER hook not implemented");
                        break;
                    }
                    case FSA_COMMAND_CHANGE_MODE: {
                        DEBUG_FUNCTION_LINE_WARN("FSA_COMMAND_CHANGE_OWNER hook not implemented");
                        break;
                    }
                    default: {
                        break;
                    }
                }
#pragma GCC diagnostic pop
                if (layerResult != FS_ERROR_FORCE_PARENT_LAYER) {
                    auto maskedResult = (FSError) ((layerResult & FS_ERROR_REAL_MASK) | FS_ERROR_EXTRA_MASK);
                    auto result       = layerResult >= 0 ? layerResult : maskedResult;

                    if (result < FS_ERROR_OK && result != FS_ERROR_END_OF_FILE && result != FS_ERROR_END_OF_DIR && result != FS_ERROR_CANCELLED) {
                        if (layer->fallbackOnError()) {
                            // Only fallback if FS_ERROR_FORCE_NO_FALLBACK flag is not set.
                            if (static_cast<FSError>(layerResult & FS_ERROR_EXTRA_MASK) != FS_ERROR_FORCE_NO_FALLBACK) {
                                continue;
                            }
                        }
                    }
                    if (param->sync == FS_SHIM_TYPE_SYNC) {
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Return with result %08X %s", layer->getName().c_str(), result, result <= 0 ? FSAGetStatusStr(result) : "");
                        return result;
                    } else if (param->sync == FS_SHIM_TYPE_ASYNC) {
                        // convert to IOSError
                        auto err = (IOSError) result;
                        if (result == FS_ERROR_INVALID_BUFFER) {
                            err = IOS_ERROR_ACCESS;
                        } else if (result == FS_ERROR_INVALID_CLIENTHANDLE) {
                            err = IOS_ERROR_INVALID;
                        } else if (result == FS_ERROR_BUSY) {
                            err = IOS_ERROR_QFULL;
                        }

                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Call async callback :) with result %08X %s", layer->getName().c_str(), err, result <= 0 ? FSAGetStatusStr(result) : "");
                        param->asyncFS.callback(err, &param->asyncFS);

                        return FS_ERROR_OK;
                    } else {
                        // This should never happen.
                        DEBUG_FUNCTION_LINE_ERR("Unknown sync type.");
                        OSFatal("ContentRedirectionModule: Unknown sync type.");
                    }
                } else {
                    DEBUG_FUNCTION_LINE_VERBOSE("[%s] Call parent layer / real function", layer->getName().c_str());
                }
            }
        }
    }
    return FS_ERROR_FORCE_REAL_FUNCTION;
}

FSCmdBlockBody *fsCmdBlockGetBody(FSCmdBlock *cmdBlock) {
    if (!cmdBlock) {
        return nullptr;
    }

    auto body = (FSCmdBlockBody *) (ROUNDUP((uint32_t) cmdBlock, 0x40));
    return body;
}

FSClientBody *fsClientGetBody(FSClient *client) {
    if (!client) {
        return nullptr;
    }

    auto body    = (FSClientBody *) (ROUNDUP((uint32_t) client, 0x40));
    body->client = client;
    return body;
}

FSStatus handleAsyncResult(FSClient *client, FSCmdBlock *block, FSAsyncData *asyncData, FSStatus status) {
    if (asyncData->callback != nullptr) {
        if (asyncData->ioMsgQueue != nullptr) {
            DEBUG_FUNCTION_LINE_ERR("callback and ioMsgQueue both set.");
            OSFatal("ContentRedirectionModule: callback and ioMsgQueue both set.");
        }
        // userCallbacks are called in the DefaultAppIOQueue.
        asyncData->ioMsgQueue = OSGetDefaultAppIOQueue();
    }

    if (asyncData->ioMsgQueue != nullptr) {
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
        FSAsyncResult *result = &(fsCmdBlockGetBody(block)->asyncResult);
        //DEBUG_FUNCTION_LINE("Send result %d to ioMsgQueue (%08X)", status, asyncData->ioMsgQueue);
        result->asyncData.callback   = asyncData->callback;
        result->asyncData.param      = asyncData->param;
        result->asyncData.ioMsgQueue = asyncData->ioMsgQueue;
        memset(&result->ioMsg, 0, sizeof(result->ioMsg));
        result->ioMsg.data = result;
        result->ioMsg.type = OS_FUNCTION_TYPE_FS_CMD_ASYNC;
        result->client     = client;
        result->block      = block;
        result->status     = status;

        OSMemoryBarrier();
        while (!OSSendMessage(asyncData->ioMsgQueue, (OSMessage *) &(result->ioMsg), OS_MESSAGE_FLAGS_NONE)) {
            DEBUG_FUNCTION_LINE_ERR("Failed to send message");
        }
    }
    return FS_STATUS_OK;
}

int64_t readIntoBuffer(int32_t handle, void *buffer, size_t size, size_t count) {
    auto sizeToRead = size * count;
    /*
    // https://github.com/decaf-emu/decaf-emu/blob/131aeb14fccff8461a5fd9f2aa5c040ba3880ef5/src/libdecaf/src/cafe/libraries/coreinit/coreinit_fs_cmd.cpp#L2346
    if (sizeToRead > 0x100000) {
        sizeToRead = 0x100000;
    }*/
    void *newBuffer = buffer;
    int32_t curResult;
    int64_t totalSize = 0;
    while (sizeToRead > 0) {

        curResult = read(handle, newBuffer, sizeToRead);
        if (curResult < 0) {
            DEBUG_FUNCTION_LINE_ERR("Reading %08X bytes from handle %08X failed. result %08X errno: %d ", size * count, handle, curResult, errno);
            return -1;
        }
        if (curResult == 0) {
            break;
        }
        newBuffer = (void *) (((uint32_t) newBuffer) + curResult);
        totalSize += curResult;
        sizeToRead -= curResult;
    }
    return totalSize;
}

int64_t writeFromBuffer(int32_t handle, const void *buffer, size_t size, size_t count) {
    auto sizeToWrite = size * count;
    auto *ptr        = buffer;
    int32_t curResult;
    int64_t totalSize = 0;
    while (sizeToWrite > 0) {
        curResult = write(handle, ptr, sizeToWrite);
        if (curResult < 0) {
            DEBUG_FUNCTION_LINE_ERR("Writing %08X bytes from handle %08X failed. result %08X errno: %d ", size * count, handle, curResult, errno);
            return -1;
        }
        if (curResult == 0) {
            break;
        }
        ptr = (void *) (((uint32_t) ptr) + curResult);
        totalSize += curResult;
        sizeToWrite -= curResult;
    }
    return totalSize;
}

static int32_t fsIOthreadCallback([[maybe_unused]] int argc, const char **argv) {
    auto *magic = ((FSIOThreadData *) argv);

    DEBUG_FUNCTION_LINE_VERBOSE("Hello from IO Thread for core: %d", OSGetCoreId());
    constexpr int32_t messageSize = sizeof(magic->messages) / sizeof(magic->messages[0]);
    OSInitMessageQueue(&magic->queue, magic->messages, messageSize);

    OSMessage recv;
    while (OSReceiveMessage(&magic->queue, &recv, OS_MESSAGE_FLAGS_BLOCKING)) {
        if (recv.args[0] == FS_IO_QUEUE_COMMAND_STOP) {
            DEBUG_FUNCTION_LINE_VERBOSE("Received break command! Stop thread");
            break;
        } else if (recv.args[0] == FS_IO_QUEUE_COMMAND_PROCESS_FS_COMMAND) {
            auto *message = (FSShimWrapperMessage *) recv.message;
            auto *param   = (FSShimWrapper *) message->param;
            FSError res   = FS_ERROR_MEDIA_ERROR;
            auto syncType = param->sync;

            if (param->api == FS_SHIM_API_FS) {
                res = processShimBufferForFS(param);
            } else if (param->api == FS_SHIM_API_FSA) {
                res = processShimBufferForFSA(param);
            } else {
                DEBUG_FUNCTION_LINE_ERR("Incompatible API type %d", param->api);
                OSFatal("ContentRedirectionModule: Incompatible API type");
            }
            // param is free'd at this point!!!
            if (syncType == FS_SHIM_TYPE_SYNC) {
                // For sync messages we can't (and don't need to) free "message", because it contains the queue we're about to use.
                // But this is not a problem because it's sync anyway.
                OSMessage send;
                send.args[0] = FS_IO_QUEUE_SYNC_RESULT;
                send.args[1] = (uint32_t) res;
                if (!OSSendMessage(&message->messageQueue, &send, OS_MESSAGE_FLAGS_NONE)) {
                    DEBUG_FUNCTION_LINE_ERR("Failed to send message");
                    OSFatal("ContentRedirectionModule: Failed to send message");
                }

            } else if (syncType == FS_SHIM_TYPE_ASYNC) {
                // If it's async we need to clean up "message" :)
                free(message);
            }
        }
    }

    return 0;
}

void startFSIOThreads() {
    int32_t threadAttributes[] = {OS_THREAD_ATTRIB_AFFINITY_CPU0, OS_THREAD_ATTRIB_AFFINITY_CPU1, OS_THREAD_ATTRIB_AFFINITY_CPU2};
    auto stackSize             = 16 * 1024;
    auto upid                  = OSGetUPID();
    if (!sLayerInfoForUPID.contains(upid)) {
        DEBUG_FUNCTION_LINE_ERR("Tried to start threads for invalid UPID %d", upid);
        OSFatal("Tried to start threads for invalid UPID.");
    }

    auto &layerInfo = sLayerInfoForUPID[upid];
    if (layerInfo->threadsRunning) {
        return;
    }

    int coreId = 0;
    for (int core : threadAttributes) {
        if (upid != 2 && upid != 15 && core == OS_THREAD_ATTRIB_AFFINITY_CPU2) {
            DEBUG_FUNCTION_LINE_ERR("Skip core 2 for non-game UPID %d", upid);
            continue;
        }
        auto *threadData = &layerInfo->threadData[coreId];
        memset(threadData, 0, sizeof(*threadData));
        threadData->setup  = false;
        threadData->thread = (OSThread *) memalign(8, sizeof(OSThread));
        if (!threadData->thread) {
            DEBUG_FUNCTION_LINE_ERR("Failed to allocate threadData");
            OSFatal("ContentRedirectionModule: Failed to allocate IO Thread");
            continue;
        }
        threadData->stack = (uint8_t *) memalign(0x20, stackSize);
        if (!threadData->thread) {
            free(threadData->thread);
            DEBUG_FUNCTION_LINE_ERR("Failed to allocate threadData stack");
            OSFatal("ContentRedirectionModule: Failed to allocate IO Thread stack");
            continue;
        }

        OSMemoryBarrier();

        if (!OSCreateThread(threadData->thread, &fsIOthreadCallback, 1, (char *) threadData, reinterpret_cast<void *>((uint32_t) threadData->stack + stackSize), stackSize, 0, core)) {
            free(threadData->thread);
            free(threadData->stack);
            threadData->setup = false;
            DEBUG_FUNCTION_LINE_ERR("failed to create threadData");
            OSFatal("ContentRedirectionModule: Failed to create threadData");
        }

        strncpy(threadData->threadName, string_format("ContentRedirection IO Thread %d", coreId).c_str(), sizeof(threadData->threadName) - 1);
        OSSetThreadName(threadData->thread, threadData->threadName);
        OSResumeThread(threadData->thread);
        threadData->setup = true;
        coreId++;
    }

    layerInfo->threadsRunning = true;
    OSMemoryBarrier();
}

void stopFSIOThreads() {
    auto upid = OSGetUPID();
    if (!sLayerInfoForUPID.contains(upid)) {
        DEBUG_FUNCTION_LINE_ERR("Tried to start threads for invalid UPID %d", upid);
        OSFatal("Tried to start threads for invalid UPID.");
    }

    auto &layerInfo = sLayerInfoForUPID[upid];
    if (!layerInfo->threadsRunning) {
        return;
    }

    for (auto &curThread : layerInfo->threadData) {
        auto *thread = &curThread;
        if (!thread->setup) {
            continue;
        }
        OSMessage message;
        message.args[0] = FS_IO_QUEUE_COMMAND_STOP;
        OSSendMessage(&thread->queue, &message, OS_MESSAGE_FLAGS_NONE);

        if (OSIsThreadSuspended(thread->thread)) {
            OSResumeThread(thread->thread);
        }

        OSJoinThread(thread->thread, nullptr);
        if (thread->stack) {
            free(thread->stack);
            thread->stack = nullptr;
        }
        if (thread->thread) {
            free(thread->thread);
            thread->thread = nullptr;
        }
    }

    layerInfo->threadsRunning = false;
}