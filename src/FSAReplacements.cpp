#include "FSAReplacements.h"
#include "FileUtils.h"
#include "utils/logger.h"
#include <coreinit/core.h>
#include <coreinit/thread.h>
#include <malloc.h>

static FSError processFSAShimInThread(FSAShimBuffer *shimBuffer) {
    FSError res;

    auto upid = OSGetUPID();
    if (!sLayerInfoForUPID.contains(upid)) {
        DEBUG_FUNCTION_LINE_ERR("invalid UPID %d", upid);
        OSFatal("Tried to start threads for invalid UPID.");
    }

    auto & layerInfo = sLayerInfoForUPID[upid];
    if (layerInfo->threadsRunning) {
        auto param = (FSShimWrapper *) malloc(sizeof(FSShimWrapper));
        if (param == nullptr) {
            DEBUG_FUNCTION_LINE_ERR("Failed to allocate memory for FSShimWrapper");
            OSFatal("ContentRedirectionModule: Failed to allocate memory for FSShimWrapper");
        }

        param->upid = OSGetUPID();
        param->api  = FS_SHIM_API_FSA;
        param->sync = FS_SHIM_TYPE_SYNC;
        param->shim = shimBuffer;

        if (OSGetCurrentThread() == layerInfo->threadData[OSGetCoreId()].thread) {
            res = processShimBufferForFSA(param);
            // No need to clean "param", it has been already free'd in processFSAShimBuffer.
        } else {
            auto message = (FSShimWrapperMessage *) malloc(sizeof(FSShimWrapperMessage));
            if (message == nullptr) {
                DEBUG_FUNCTION_LINE_ERR("Failed to allocate memory for FSShimWrapperMessage");
                OSFatal("ContentRedirectionModule: Failed to allocate memory for FSShimWrapperMessage");
            }
            message->param = param;

            constexpr int32_t messageSize = sizeof(message->messages) / sizeof(message->messages[0]);
            OSInitMessageQueue(&message->messageQueue, message->messages, messageSize);
            if (!sendMessageToThread(layerInfo, message)) {
                DEBUG_FUNCTION_LINE_ERR("Failed to send message to thread");
                OSFatal("ContentRedirectionModule: Failed send message to thread");
            }
            OSMessage recv;
            if (!OSReceiveMessage(&message->messageQueue, &recv, OS_MESSAGE_FLAGS_BLOCKING)) {
                DEBUG_FUNCTION_LINE_ERR("Failed to receive message");
                OSFatal("ContentRedirectionModule: Failed to receive message");
            }
            if (recv.args[0] != FS_IO_QUEUE_SYNC_RESULT) {
                DEBUG_FUNCTION_LINE_ERR("ContentRedirection: Unexpected message in message queue.");
                OSFatal("ContentRedirection: Unexpected message in message queue.");
            }
            res = (FSError) recv.args[1];
            // We only need to clean up "message". "param" has already been free'd by the other thread.
            free(message);
        }
    } else {
        res = FS_ERROR_FORCE_REAL_FUNCTION;
        DEBUG_FUNCTION_LINE_WARN("Threads are not running yet, skip replacement");
    }
    return res;
}

DECL_FUNCTION(FSError, FSAOpenFileEx, FSAClientHandle client, const char *path, const char *mode, FSMode createMode, FSOpenFileFlags openFlag, uint32_t preallocSize, FSAFileHandle *handle) {
    if (handle == nullptr) {
        DEBUG_FUNCTION_LINE_WARN("handle is null.");
        return FS_ERROR_INVALID_BUFFER;
    }
    *handle = -1;

    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestOpenFile(shimBuffer, client, path, mode, createMode, openFlag, preallocSize);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestOpenFile failed");
        return res;
    }
    // Hacky solution to pass the pointer into the other thread.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    auto *hackyBuffer = (uint32_t *) &shimBuffer->response;
    hackyBuffer[1]    = (uint32_t) handle;
    res               = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAOpenFileEx" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAOpenFileEx(client, path, mode, createMode, openFlag, preallocSize, handle);
}

DECL_FUNCTION(FSError, FSAOpenFile, FSAClientHandle client, const char *path, const char *mode, FSAFileHandle *handle) {
    if (handle == nullptr) {
        DEBUG_FUNCTION_LINE_WARN("handle is null.");
        return FS_ERROR_INVALID_BUFFER;
    }
    *handle = -1;

    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestOpenFile(shimBuffer, client, path, mode, static_cast<FSMode>(0x660), static_cast<FSOpenFileFlags>(0), 0);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestOpenFile failed");
        return res;
    }
    // Hacky solution to pass the pointer into the other thread.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    auto *hackyBuffer = (uint32_t *) &shimBuffer->response;
    hackyBuffer[1]    = (uint32_t) handle;
    res               = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAOpenFile" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAOpenFile(client, path, mode, handle);
}

DECL_FUNCTION(FSError, FSACloseFile, FSAClientHandle client, FSAFileHandle handle) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestCloseFile(shimBuffer, client, handle);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestCloseFile failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSACloseFile" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSACloseFile(client, handle);
}

DECL_FUNCTION(FSError, FSAFlushFile, FSAClientHandle client, FSAFileHandle handle) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestFlushFile(shimBuffer, client, handle);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestFlushFile failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAFlushFile" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAFlushFile(client, handle);
}

DECL_FUNCTION(FSError, FSAGetStat, FSAClientHandle client, const char *path, FSAStat *stat) {
    if (stat == nullptr) {
        DEBUG_FUNCTION_LINE_WARN("stat is null.");
        return FS_ERROR_INVALID_BUFFER;
    }

    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestQueryInfo(shimBuffer, client, path, FSA_QUERY_INFO_STAT);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    // Hacky solution to pass the pointer into the other thread.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    auto *hackyBuffer = (uint32_t *) &shimBuffer->response;
    hackyBuffer[1]    = (uint32_t) stat;
    res               = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAGetStat" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAGetStat(client, path, stat);
}

DECL_FUNCTION(FSError, FSAGetStatFile, FSAClientHandle client, FSAFileHandle handle, FSAStat *stat) {
    if (stat == nullptr) {
        DEBUG_FUNCTION_LINE_WARN("stat is null.");
        return FS_ERROR_INVALID_BUFFER;
    }

    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestStatFile(shimBuffer, client, handle);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    // Hacky solution to pass the pointer into the other thread.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    auto *hackyBuffer = (uint32_t *) &shimBuffer->response;
    hackyBuffer[1]    = (uint32_t) stat;
    res               = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAGetStatFile" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAGetStatFile(client, handle, stat);
}

DECL_FUNCTION(FSError, FSARemove, FSAClientHandle client, const char *path) {
    if (path == nullptr) {
        DEBUG_FUNCTION_LINE_WARN("path is null.");
        return FS_ERROR_INVALID_PARAM;
    }

    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestRemove(shimBuffer, client, path);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSARemove" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSARemove(client, path);
}

DECL_FUNCTION(FSError, FSARename, FSAClientHandle client, const char *oldPath, const char *newPath) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestRename(shimBuffer, client, oldPath, newPath);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSARename" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSARename(client, oldPath, newPath);
}

DECL_FUNCTION(FSError, FSASetPosFile, FSAClientHandle client, FSAFileHandle handle, FSAFilePosition pos) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestSetPos(shimBuffer, client, handle, pos);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSASetPosFile" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSASetPosFile(client, handle, pos);
}

DECL_FUNCTION(FSError, FSATruncateFile, FSAClientHandle client, FSAFileHandle handle) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestTruncate(shimBuffer, client, handle);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSATruncateFile" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSATruncateFile(client, handle);
}

DECL_FUNCTION(FSError, FSAReadFile, FSAClientHandle client, uint8_t *buffer, uint32_t size, uint32_t count, FSAFileHandle handle, uint32_t flags) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestReadFile(shimBuffer, client, buffer, size, count, 0, handle, static_cast<FSAReadFlag>(flags & 0xfffffffe));
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAReadFile" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAReadFile(client, buffer, size, count, handle, flags);
}

DECL_FUNCTION(FSError, FSAReadFileWithPos, FSAClientHandle client, uint8_t *buffer, uint32_t size, uint32_t count, FSAFilePosition pos, FSAFileHandle handle, uint32_t flags) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestReadFile(shimBuffer, client, buffer, size, count, pos, handle, static_cast<FSAReadFlag>(flags | FSA_READ_FLAG_READ_WITH_POS));
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAReadFileWithPos" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAReadFileWithPos(client, buffer, size, count, pos, handle, flags);
}

DECL_FUNCTION(FSError, FSAWriteFile, FSAClientHandle client, const uint8_t *buffer, uint32_t size, uint32_t count, FSAFileHandle handle, uint32_t flags) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestWriteFile(shimBuffer, client, buffer, size, count, 0, handle, static_cast<FSAWriteFlag>(flags & 0xfffffffe));
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAWriteFile" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAWriteFile(client, buffer, size, count, handle, flags);
}

DECL_FUNCTION(FSError, FSAWriteFileWithPos, FSAClientHandle client, uint8_t *buffer, uint32_t size, uint32_t count, FSAFilePosition pos, FSAFileHandle handle, uint32_t flags) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestWriteFile(shimBuffer, client, buffer, size, count, pos, handle, static_cast<FSAWriteFlag>(flags | FSA_WRITE_FLAG_READ_WITH_POS));
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAWriteFileWithPos" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAWriteFileWithPos(client, buffer, size, count, pos, handle, flags);
}

DECL_FUNCTION(FSError, FSAGetPosFile, FSAClientHandle client, FSAFileHandle handle, FSAFilePosition *outPos) {
    if (outPos == nullptr) {
        DEBUG_FUNCTION_LINE_WARN("outPos is null.");
        return FS_ERROR_INVALID_BUFFER;
    }

    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestGetPos(shimBuffer, client, handle);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    // Hacky solution to pass the pointer into the other thread.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    auto *hackyBuffer = (uint32_t *) &shimBuffer->response;
    hackyBuffer[1]    = (uint32_t) outPos;
    res               = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAGetPosFile" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAGetPosFile(client, handle, outPos);
}

DECL_FUNCTION(FSError, FSAIsEof, FSAClientHandle client, FSAFileHandle handle) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestIsEof(shimBuffer, client, handle);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAIsEof" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAIsEof(client, handle);
}

DECL_FUNCTION(FSError, FSAFlushMultiQuota, FSAClientHandle client, const char *path) {
    DEBUG_FUNCTION_LINE("NOT IMPLEMENTED. path %s", path);
    return real_FSAFlushMultiQuota(client, path);
}

DECL_FUNCTION(FSError, FSAFlushQuota, FSAClientHandle client, const char *path) {
    DEBUG_FUNCTION_LINE("NOT IMPLEMENTED. path %s", path);
    return real_FSAFlushQuota(client, path);
}

DECL_FUNCTION(FSError, FSAChangeMode, FSAClientHandle client, const char *path, FSMode permission) {
    DEBUG_FUNCTION_LINE_ERR("NOT IMPLEMENTED path %s permission: %08X", path, permission);
    return real_FSAChangeMode(client, path, permission);
}

DECL_FUNCTION(FSError, FSAOpenFileByStat, FSAClientHandle client, FSAStat *stat, const char *mode, const char *path, FSAFileHandle *outFileHandle) {
    DEBUG_FUNCTION_LINE_ERR("NOT IMPLEMENTED");
    return real_FSAOpenFileByStat(client, stat, mode, path, outFileHandle);
}

DECL_FUNCTION(FSError, FSAAppendFile, FSAClientHandle client, FSAFileHandle fileHandle, uint32_t size, uint32_t count) {
    DEBUG_FUNCTION_LINE_ERR("NOT IMPLEMENTED");
    return real_FSAAppendFile(client, fileHandle, size, count);
}

DECL_FUNCTION(FSError, FSAAppendFileEx, FSAClientHandle client, FSAFileHandle fileHandle, uint32_t size, uint32_t count, uint32_t flags) {
    DEBUG_FUNCTION_LINE_ERR("NOT IMPLEMENTED");
    return real_FSAAppendFileEx(client, fileHandle, size, count, flags);
}

DECL_FUNCTION(FSError, FSAOpenDir, FSAClientHandle client, const char *path, FSADirectoryHandle *dirHandle) {
    if (dirHandle == nullptr) {
        DEBUG_FUNCTION_LINE_WARN("handle is null.");
        return FS_ERROR_INVALID_BUFFER;
    }
    *dirHandle = -1;

    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestOpenDir(shimBuffer, client, path);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    // Hacky solution to pass the pointer into the other thread.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    auto *hackyBuffer = (uint32_t *) &shimBuffer->response;
    hackyBuffer[1]    = (uint32_t) dirHandle;
    res               = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAOpenDir" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAOpenDir(client, path, dirHandle);
}

DECL_FUNCTION(FSError, FSAReadDir, FSAClientHandle client, FSADirectoryHandle dirHandle, FSADirectoryEntry *directoryEntry) {
    if (directoryEntry == nullptr) {
        DEBUG_FUNCTION_LINE_WARN("handle is null.");
        return FS_ERROR_INVALID_BUFFER;
    }

    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestReadDir(shimBuffer, client, dirHandle);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    // Hacky solution to pass the pointer into the other thread.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    auto *hackyBuffer = (uint32_t *) &shimBuffer->response;
    hackyBuffer[1]    = (uint32_t) directoryEntry;
    res               = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAOpenDir" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAReadDir(client, dirHandle, directoryEntry);
}

DECL_FUNCTION(FSError, FSARewindDir, FSAClientHandle client, FSADirectoryHandle dirHandle) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestRewindDir(shimBuffer, client, dirHandle);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAOpenDir" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSARewindDir(client, dirHandle);
}

DECL_FUNCTION(FSError, FSACloseDir, FSAClientHandle client, FSADirectoryHandle dirHandle) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestCloseDir(shimBuffer, client, dirHandle);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSACloseDir" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSACloseDir(client, dirHandle);
}

DECL_FUNCTION(FSError, FSAMakeDir, FSAClientHandle client, const char *path, FSMode mode) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestMakeDir(shimBuffer, client, path, mode);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAMakeDir" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAMakeDir(client, path, mode);
}

DECL_FUNCTION(FSError, FSAChangeDir, FSAClientHandle client, const char *path) {
    auto *shimBuffer = (FSAShimBuffer *) memalign(0x20, sizeof(FSAShimBuffer));
    if (!shimBuffer) {
        DEBUG_FUNCTION_LINE_WARN("FS_ERROR_OUT_OF_RESOURCES");
        return FS_ERROR_OUT_OF_RESOURCES;
    }

    auto res = fsaShimPrepareRequestChangeDir(shimBuffer, client, path);
    if (res != FS_ERROR_OK) {
        free(shimBuffer);
        DEBUG_FUNCTION_LINE_WARN("fsaShimPrepareRequestQueryInfo failed");
        return res;
    }
    res = processFSAShimInThread(shimBuffer);
    free(shimBuffer);
    if (res != FS_ERROR_FORCE_REAL_FUNCTION) {
        return res;
    }

    // Other plugins/modules may override this function as well, so we need to call "real_FSAChangeDir" instead of using
    // the existing shimBuffer (which would be more efficient).
    return real_FSAChangeDir(client, path);
}

function_replacement_data_t fsa_file_function_replacements[] = {
        REPLACE_FUNCTION(FSAOpenFile, LIBRARY_COREINIT, FSAOpenFile),
        REPLACE_FUNCTION(FSAOpenFileEx, LIBRARY_COREINIT, FSAOpenFileEx),
        REPLACE_FUNCTION(FSACloseFile, LIBRARY_COREINIT, FSACloseFile),
        REPLACE_FUNCTION(FSAFlushFile, LIBRARY_COREINIT, FSAFlushFile),
        REPLACE_FUNCTION(FSAGetStat, LIBRARY_COREINIT, FSAGetStat),
        REPLACE_FUNCTION(FSAGetStatFile, LIBRARY_COREINIT, FSAGetStatFile),
        REPLACE_FUNCTION(FSARemove, LIBRARY_COREINIT, FSARemove),
        REPLACE_FUNCTION(FSARename, LIBRARY_COREINIT, FSARename),
        REPLACE_FUNCTION(FSASetPosFile, LIBRARY_COREINIT, FSASetPosFile),
        REPLACE_FUNCTION(FSATruncateFile, LIBRARY_COREINIT, FSATruncateFile),
        REPLACE_FUNCTION(FSAReadFile, LIBRARY_COREINIT, FSAReadFile),
        REPLACE_FUNCTION(FSAReadFileWithPos, LIBRARY_COREINIT, FSAReadFileWithPos),
        REPLACE_FUNCTION(FSAWriteFile, LIBRARY_COREINIT, FSAWriteFile),
        REPLACE_FUNCTION(FSAWriteFileWithPos, LIBRARY_COREINIT, FSAWriteFileWithPos),
        REPLACE_FUNCTION(FSAGetPosFile, LIBRARY_COREINIT, FSAGetPosFile),
        REPLACE_FUNCTION(FSAIsEof, LIBRARY_COREINIT, FSAIsEof),

        REPLACE_FUNCTION(FSAFlushMultiQuota, LIBRARY_COREINIT, FSAFlushMultiQuota),
        REPLACE_FUNCTION(FSAFlushQuota, LIBRARY_COREINIT, FSAFlushQuota),
        REPLACE_FUNCTION(FSAChangeMode, LIBRARY_COREINIT, FSAChangeMode),
        REPLACE_FUNCTION(FSAOpenFileByStat, LIBRARY_COREINIT, FSAOpenFileByStat),
        REPLACE_FUNCTION(FSAAppendFile, LIBRARY_COREINIT, FSAAppendFile),
        REPLACE_FUNCTION(FSAAppendFileEx, LIBRARY_COREINIT, FSAAppendFileEx),

        REPLACE_FUNCTION(FSAOpenDir, LIBRARY_COREINIT, FSAOpenDir),
        REPLACE_FUNCTION(FSAReadDir, LIBRARY_COREINIT, FSAReadDir),
        REPLACE_FUNCTION(FSARewindDir, LIBRARY_COREINIT, FSARewindDir),
        REPLACE_FUNCTION(FSACloseDir, LIBRARY_COREINIT, FSACloseDir),
        REPLACE_FUNCTION(FSAMakeDir, LIBRARY_COREINIT, FSAMakeDir),
        REPLACE_FUNCTION(FSAChangeDir, LIBRARY_COREINIT, FSAChangeDir),
};

uint32_t fsa_file_function_replacements_size = sizeof(fsa_file_function_replacements) / sizeof(function_replacement_data_t);

FSError processShimBufferForFSA(FSShimWrapper *param) {
    auto res = doForLayer(param);
    if (res == FS_ERROR_FORCE_REAL_FUNCTION) {
        if (param->sync == FS_SHIM_TYPE_ASYNC) {
            DEBUG_FUNCTION_LINE_ERR("ASYNC FSA API is not supported");
            OSFatal("ContentRedirectionModule: ASYNC FSA API is not supported");
        }
    }
    free(param);
    return res;
}