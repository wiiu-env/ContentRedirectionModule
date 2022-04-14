#include "FileUtils.h"
#include "FSWrapper.h"
#include "IFSWrapper.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include <coreinit/cache.h>
#include <coreinit/thread.h>
#include <map>
#include <unistd.h>
#include <wums.h>

std::mutex workingDirMutex;
std::map<FSClient *, std::string> workingDirs;

std::mutex fsLayerMutex;
std::vector<IFSWrapper *> fsLayers;

std::string getFullPathForClient(FSClient *pClient, char *path) {
    std::string res;

    if (path[0] != '/' && path[0] != '\\') {
        if (workingDirs.count(pClient) > 0) {
            res = string_format("%s%s", workingDirs.at(pClient).c_str(), path);
        } else {
            DEBUG_FUNCTION_LINE_ERR("Failed to find working dir for client");
        }
    } else {
        res = path;
    }

    std::replace(res.begin(), res.end(), '\\', '/');

    return res;
}

void setWorkingDir(FSClient *client, const char *path) {
    std::lock_guard<std::mutex> workingDirLock(workingDirMutex);

    workingDirs[client] = path;
    OSMemoryBarrier();
}

void clearFSLayer() {
    {
        std::lock_guard<std::mutex> workingDirLock(workingDirMutex);
        workingDirs.clear();
    }
    {
        std::lock_guard<std::mutex> layerlock(fsLayerMutex);
        for (auto &layer : fsLayers) {
            delete layer;
        }
        fsLayers.clear();
    }
}

// FUN_0204cc20
#define fsClientHandleFatalErrorAndBlock ((void (*)(FSClientBody *, uint32_t))(0x101C400 + 0x4cc20))

FSStatus doForLayer(FSClient *client,
                    FSErrorFlag errorMask,
                    const std::function<FSStatus(FSErrorFlag errorMask)> &real_function,
                    const std::function<FSStatus(IFSWrapper *layer)> &layer_callback,
                    const std::function<FSStatus(IFSWrapper *layer, FSStatus)> &result_handler) {
    FSErrorFlag realErrorMask = errorMask;

    std::lock_guard<std::mutex> lock(fsLayerMutex);
    if (!fsLayers.empty()) {
        uint32_t startIndex = fsLayers.size();
        for (uint32_t i = fsLayers.size(); i > 0; i--) {
            if ((uint32_t) fsLayers[i - 1] == errorMask) {
                startIndex    = i - 1;
                realErrorMask = FS_ERROR_FLAG_ALL;
                break;
            }
        }

        if (startIndex > 0) {
            for (uint32_t i = startIndex; i > 0; i--) {
                auto layer = fsLayers[i - 1];
                if (!layer->isActive()) {
                    continue;
                }
                auto result = layer_callback(layer);

                if (result != FS_STATUS_FORCE_PARENT_LAYER) {
                    if (result < FS_STATUS_OK && result != FS_STATUS_END && result != FS_STATUS_CANCELLED) {
                        if (layer->fallbackOnError()) {
                            // Only fallback if FS_STATUS_FORCE_NO_FALLBACK flag is not set.
                            if (static_cast<FSStatus>(result & 0xFFFF0000) != FS_STATUS_FORCE_NO_FALLBACK) {
                                continue;
                            } else {
                                // Remove FS_STATUS_FORCE_NO_FALLBACK flag.
                                result = static_cast<FSStatus>((result & 0x0000FFFF) | 0xFFFF0000);
                            }
                        }
                    }

                    if (result >= FS_STATUS_OK || result == FS_STATUS_END || result == FS_STATUS_CANCELLED) {
                        DEBUG_FUNCTION_LINE_VERBOSE("Returned %08X by %s", result, layer->getName().c_str());
                        return result_handler(layer, result);
                    }

                    FSErrorFlag errorFlags = FS_ERROR_FLAG_NONE;
                    bool forceError        = false;

                    switch ((int) result) {
                        case FS_STATUS_MAX:
                            errorFlags = FS_ERROR_FLAG_MAX;
                            break;
                        case FS_STATUS_ALREADY_OPEN:
                            errorFlags = FS_ERROR_FLAG_ALREADY_OPEN;
                            break;
                        case FS_STATUS_EXISTS:
                            errorFlags = FS_ERROR_FLAG_EXISTS;
                            break;
                        case FS_STATUS_NOT_FOUND:
                            errorFlags = FS_ERROR_FLAG_NOT_FOUND;
                            break;
                        case FS_STATUS_NOT_FILE:
                            errorFlags = FS_ERROR_FLAG_NOT_FILE;
                            break;
                        case FS_STATUS_NOT_DIR:
                            errorFlags = FS_ERROR_FLAG_NOT_DIR;
                            break;
                        case FS_STATUS_ACCESS_ERROR:
                            errorFlags = FS_ERROR_FLAG_ACCESS_ERROR;
                            break;
                        case FS_STATUS_PERMISSION_ERROR:
                            errorFlags = FS_ERROR_FLAG_PERMISSION_ERROR;
                            break;
                        case FS_STATUS_FILE_TOO_BIG:
                            errorFlags = FS_ERROR_FLAG_FILE_TOO_BIG;
                            break;
                        case FS_STATUS_STORAGE_FULL:
                            errorFlags = FS_ERROR_FLAG_STORAGE_FULL;
                            break;
                        case FS_STATUS_JOURNAL_FULL:
                            errorFlags = FS_ERROR_FLAG_JOURNAL_FULL;
                            break;
                        case FS_STATUS_UNSUPPORTED_CMD:
                            errorFlags = FS_ERROR_FLAG_UNSUPPORTED_CMD;
                            break;
                        case FS_STATUS_MEDIA_NOT_READY:
                        case FS_STATUS_MEDIA_ERROR:
                        case FS_STATUS_CORRUPTED:
                        case FS_STATUS_FATAL_ERROR:
                            forceError = true;
                            break;
                        case FS_STATUS_OK:
                            break;
                    }

                    if (forceError || (realErrorMask != FS_ERROR_FLAG_NONE && (errorFlags & realErrorMask) == 0)) {
                        DEBUG_FUNCTION_LINE_ERR("Transit to Fatal Error");
                        auto clientBody = fsClientGetBody(client);

                        fsClientHandleFatalErrorAndBlock(clientBody, clientBody->lastError);
                        return FS_STATUS_FATAL_ERROR;
                    }
                    DEBUG_FUNCTION_LINE_VERBOSE("%08X Returned %08X by %s ", errorMask, result, layer->getName().c_str());
                    return result_handler(layer, result);
                }
            }
        }
    }

    auto mask = static_cast<FSErrorFlag>((realErrorMask & FS_ERROR_FLAG_REAL_MASK) | FS_ERROR_FLAG_FORCE_REAL);
    return real_function(mask);
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


FSStatus send_result_async(FSClient *client, FSCmdBlock *block, FSAsyncData *asyncData, FSStatus status) {
    if (asyncData->callback != nullptr) {
        if (asyncData->ioMsgQueue != nullptr) {
            DEBUG_FUNCTION_LINE_ERR("callback and ioMsgQueue both set.");
            return FS_STATUS_FATAL_ERROR;
        }
        // userCallbacks are called in the DefaultAppIOQueue.
        asyncData->ioMsgQueue = OSGetDefaultAppIOQueue();
        //DEBUG_FUNCTION_LINE("Force to OSGetDefaultAppIOQueue (%08X)", asyncData->ioMsgQueue);
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

int64_t writeFromBuffer(int32_t handle, void *buffer, size_t size, size_t count) {
    auto sizeToWrite = size * count;
    void *ptr        = buffer;
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