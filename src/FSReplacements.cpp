#include "FSReplacements.h"
#include "FileUtils.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include <coreinit/core.h>
#include <coreinit/thread.h>

FSStatus processFSError(FSError fsError, FSClient *client, FSErrorFlag errorMask) {
    auto result = fsError >= 0 ? static_cast<FSStatus>(fsError) : fsaDecodeFsaStatusToFsStatus(fsError);

    if (result >= FS_STATUS_OK || result == FS_STATUS_END || result == FS_STATUS_CANCELLED) {
        return result;
    }

    FSErrorFlag errorFlags = FS_ERROR_FLAG_NONE;
    bool forceError        = false;

    switch ((int32_t) result) {
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

    if (forceError || (errorMask != FS_ERROR_FLAG_NONE && (errorFlags & errorMask) == 0)) {
        DEBUG_FUNCTION_LINE_ERR("Transit to Fatal Error. Error %s (%d)", FSAGetStatusStr(fsError), fsError);
        auto clientBody = fsClientGetBody(client);

        fsClientHandleFatalErrorAndBlock(clientBody, clientBody->lastError);
        return FS_STATUS_FATAL_ERROR;
    }
    return result;
}

void handleAsyncRequestsCallback(IOSError err, void *context) {
    auto *param      = (AsyncParamFS *) context;
    auto *client     = param->client;
    auto *clientBody = fsClientGetBody(client);
    auto *block      = param->block;
    auto *asyncData  = &param->asyncData;
    auto errorMask   = param->errorMask;


    auto fsError  = __FSAShimDecodeIosErrorToFsaStatus(clientBody->clientHandle, err);
    auto fsStatus = processFSError(fsError, client, errorMask);

    handleAsyncResult(client, block, asyncData, fsStatus);
}

bool processFSAShimInThread(FSAShimBuffer *shimBuffer, FSClient *client, FSCmdBlock *block, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    bool res = false;
    if (gThreadsRunning) {
        // we **don't** need to free this in this function.
        auto param = (FSShimWrapper *) malloc(sizeof(FSShimWrapper));
        if (param == nullptr) {
            DEBUG_FUNCTION_LINE_ERR("Failed to allocate memory for FSShimWrapper");
            OSFatal("ContentRedirectionModule: Failed to allocate memory for FSShimWrapper");
        }

        param->api              = FS_SHIM_API_FS;
        param->sync             = FS_SHIM_TYPE_ASYNC;
        param->shim             = shimBuffer;
        param->asyncFS.callback = handleAsyncRequestsCallback;
        // The client and block have to valid during the whole fs operation
        param->asyncFS.client = client;
        param->asyncFS.block  = block;
        // But we need to copy the asyncData as it might be on the stack.
        param->asyncFS.asyncData.param      = asyncData->param;
        param->asyncFS.asyncData.callback   = asyncData->callback;
        param->asyncFS.asyncData.ioMsgQueue = asyncData->ioMsgQueue;
        // Copy by value
        param->asyncFS.errorMask = errorMask;

        if (OSGetCurrentThread() == gThreadData[OSGetCoreId()].thread) {
            processShimBufferForFS(param);
            // because we're doing this in sync, free(param) has already been called at this point.
            res = true;
        } else {
            auto message = (FSShimWrapperMessage *) malloc(sizeof(FSShimWrapperMessage));
            if (message == nullptr) {
                DEBUG_FUNCTION_LINE_ERR("Failed to allocate memory for FSShimWrapperMessage");
                OSFatal("ContentRedirectionModule: Failed to allocate memory for FSShimWrapperMessage");
            }
            message->param = param;
            res            = sendMessageToThread(message);
            // the other thread is call free for us, so we can return early!
        }
    } else {
        DEBUG_FUNCTION_LINE_WARN("Threads are not running yet, skip replacement");
    }
    return res;
}

DECL_FUNCTION(FSStatus, FSReadFileGeneric, FSClient *client, FSCmdBlock *block, uint8_t *buffer, uint32_t size, uint32_t count, uint32_t pos, FSFileHandle handle, FSAReadFlag readFlag, FSErrorFlag errorMask,
              FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    // Ensure size * count is not > 32 bit.
    auto bytes = uint64_t{size} * uint64_t{count};

    if (bytes > 0xFFFFFFFFull) {
        DEBUG_FUNCTION_LINE_ERR("FS doesn't support transaction size >= 4GB.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_PARAM);
        return FS_STATUS_FATAL_ERROR;
    }

    if (((uint32_t) buffer & 0x3f) != 0) {
        DEBUG_FUNCTION_LINE_ERR("buffer must be aligned by FS_IO_BUFFER_ALIGN");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_ALIGNMENT);
        return FS_STATUS_FATAL_ERROR;
    }

    if (fsaShimPrepareRequestReadFile(shimBuffer, clientBody->clientHandle, buffer, size, count, pos, handle, readFlag) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {
            return FS_STATUS_OK;
        }
    }

    return real_FSReadFileGeneric(client, block, buffer, size, count, pos, handle, readFlag, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSWriteFileGeneric, FSClient *client, FSCmdBlock *block, const uint8_t *buffer, uint32_t size, uint32_t count, uint32_t pos, FSFileHandle handle, FSAWriteFlag writeFlag, FSErrorFlag errorMask,
              FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    // Ensure size * count is not > 32 bit.
    auto bytes = uint64_t{size} * uint64_t{count};

    if (bytes > 0xFFFFFFFFull) {
        DEBUG_FUNCTION_LINE_ERR("FS doesn't support transaction size >= 4GB.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_PARAM);
        return FS_STATUS_FATAL_ERROR;
    }

    if (((uint32_t) buffer & 0x3f) != 0) {
        DEBUG_FUNCTION_LINE_ERR("buffer must be aligned by FS_IO_BUFFER_ALIGN");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_ALIGNMENT);
        return FS_STATUS_FATAL_ERROR;
    }

    if (fsaShimPrepareRequestWriteFile(shimBuffer, clientBody->clientHandle, buffer, size, count, pos, handle, writeFlag) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSWriteFileGeneric(client, block, buffer, size, count, pos, handle, writeFlag, errorMask, asyncData);
}
DECL_FUNCTION(FSStatus, FSOpenFileExAsync, FSClient *client, FSCmdBlock *block, const char *path, const char *mode, FSMode createMode, FSOpenFileFlags openFlag, uint32_t preallocSize, FSFileHandle *handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (handle == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("handle is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_BUFFER);
        return FS_STATUS_FATAL_ERROR;
    }

    if (path == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("path is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_PATH);
        return FS_STATUS_FATAL_ERROR;
    }

    if (mode == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("mode is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_PATH);
        return FS_STATUS_FATAL_ERROR;
    }

    if (fsaShimPrepareRequestOpenFile(shimBuffer, clientBody->clientHandle, path, mode, createMode, openFlag, preallocSize) == 0) {
        // Hacky solution to pass the pointer into the other thread.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
        auto *hackyBuffer = (uint32_t *) &shimBuffer->response;
        hackyBuffer[1]    = (uint32_t) handle;
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSOpenFileExAsync(client, block, path, mode, createMode, openFlag, preallocSize, handle, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSCloseFileAsync, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (fsaShimPrepareRequestCloseFile(shimBuffer, clientBody->clientHandle, handle) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSCloseFileAsync(client, block, handle, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSGetStatAsync, FSClient *client, FSCmdBlock *block, const char *path, FSStat *stat, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (path == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("path is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_PATH);
        return FS_STATUS_FATAL_ERROR;
    }

    if (stat == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("stat is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_BUFFER);
        return FS_STATUS_FATAL_ERROR;
    }

    if (fsaShimPrepareRequestQueryInfo(shimBuffer, clientBody->clientHandle, path, FSA_QUERY_INFO_STAT) == 0) {
        // Hacky solution to pass the pointer into the other thread.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
        auto *hackyBuffer = (uint32_t *) &shimBuffer->response;
        hackyBuffer[1]    = (uint32_t) stat;
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSGetStatAsync(client, block, path, stat, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSGetStatFileAsync, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSStat *stat, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (stat == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("stat is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_BUFFER);
        return FS_STATUS_FATAL_ERROR;
    }

    if (fsaShimPrepareRequestStatFile(shimBuffer, clientBody->clientHandle, handle) == 0) {
        // Hacky solution to pass the pointer into the other thread.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
        auto *hackyBuffer = (uint32_t *) &shimBuffer->response;
        hackyBuffer[1]    = (uint32_t) stat;
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSGetStatFileAsync(client, block, handle, stat, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSSetPosFileAsync, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSAFilePosition pos, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (fsaShimPrepareRequestSetPos(shimBuffer, clientBody->clientHandle, handle, pos) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSSetPosFileAsync(client, block, handle, pos, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSGetPosFileAsync, FSClient *client, FSCmdBlock *block, FSFileHandle handle, const FSAFilePosition *pos, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (pos == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("pos is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_BUFFER);
        return FS_STATUS_FATAL_ERROR;
    }

    if (fsaShimPrepareRequestGetPos(shimBuffer, clientBody->clientHandle, handle) == 0) {
        // Hacky solution to pass the pointer into the other thread.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
        auto *hackyBuffer = (uint32_t *) &shimBuffer->response;
        hackyBuffer[1]    = (uint32_t) pos;
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSGetPosFileAsync(client, block, handle, pos, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSIsEofAsync, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (fsaShimPrepareRequestIsEof(shimBuffer, clientBody->clientHandle, handle) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSIsEofAsync(client, block, handle, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSTruncateFileAsync, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (fsaShimPrepareRequestTruncate(shimBuffer, clientBody->clientHandle, handle) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSTruncateFileAsync(client, block, handle, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSRemoveAsync, FSClient *client, FSCmdBlock *block, const char *path, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (path == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("path is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_PATH);
        return FS_STATUS_FATAL_ERROR;
    }

    if (fsaShimPrepareRequestRemove(shimBuffer, clientBody->clientHandle, path) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSRemoveAsync(client, block, path, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSRenameAsync, FSClient *client, FSCmdBlock *block, const char *oldPath, const char *newPath, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (oldPath == nullptr || newPath == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("oldPath or newPath is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_PATH);
        return FS_STATUS_FATAL_ERROR;
    }

    if (fsaShimPrepareRequestRename(shimBuffer, clientBody->clientHandle, oldPath, newPath) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSRenameAsync(client, block, oldPath, newPath, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSFlushFileAsync, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (fsaShimPrepareRequestFlushFile(shimBuffer, clientBody->clientHandle, handle) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSFlushFileAsync(client, block, handle, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSChangeModeAsync, FSClient *client, FSCmdBlock *block, const char *path, FSMode mode, FSMode modeMask, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (path == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("path is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_PATH);
        return FS_STATUS_FATAL_ERROR;
    }

    if (fsaShimPrepareRequestChangeMode(shimBuffer, clientBody->clientHandle, path, mode, modeMask) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSChangeModeAsync(client, block, path, mode, modeMask, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSOpenDirAsync, FSClient *client, FSCmdBlock *block, const char *path, FSDirectoryHandle *handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (path == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("path is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_PATH);
        return FS_STATUS_FATAL_ERROR;
    }

    if (handle == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("handle is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_BUFFER);
        return FS_STATUS_FATAL_ERROR;
    }

    if (fsaShimPrepareRequestOpenDir(shimBuffer, clientBody->clientHandle, path) == 0) {
        // Hacky solution to pass the pointer into the other thread.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
        auto *hackyBuffer = (uint32_t *) &shimBuffer->response;
        hackyBuffer[1]    = (uint32_t) handle;
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSOpenDirAsync(client, block, path, handle, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSReadDirAsync, FSClient *client, FSCmdBlock *block, FSDirectoryHandle handle, FSDirectoryEntry *entry, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (entry == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("entry is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_BUFFER);
        return FS_STATUS_FATAL_ERROR;
    }

    if (fsaShimPrepareRequestReadDir(shimBuffer, clientBody->clientHandle, handle) == 0) {
        // Hacky solution to pass the pointer into the other thread.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
        auto *hackyBuffer = (uint32_t *) &shimBuffer->response;
        hackyBuffer[1]    = (uint32_t) entry;
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSReadDirAsync(client, block, handle, entry, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSCloseDirAsync, FSClient *client, FSCmdBlock *block, FSDirectoryHandle handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (fsaShimPrepareRequestCloseDir(shimBuffer, clientBody->clientHandle, handle) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSCloseDirAsync(client, block, handle, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSRewindDirAsync, FSClient *client, FSCmdBlock *block, FSDirectoryHandle handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (fsaShimPrepareRequestRewindDir(shimBuffer, clientBody->clientHandle, handle) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSRewindDirAsync(client, block, handle, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSMakeDirAsync, FSClient *client, FSCmdBlock *block, const char *path, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (path == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("path is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_PATH);
        return FS_STATUS_FATAL_ERROR;
    }

    if (fsaShimPrepareRequestMakeDir(shimBuffer, clientBody->clientHandle, path, static_cast<FSMode>(0x660)) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSMakeDirAsync(client, block, path, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSChangeDirAsync, FSClient *client, FSCmdBlock *block, const char *path, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    auto *clientBody = fsClientGetBody(client);
    auto *shimBuffer = (FSAShimBuffer *) fsCmdBlockGetBody(block);

    if (path == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("path is null.");
        fsClientHandleFatalError(clientBody, FS_ERROR_INVALID_PATH);
        return FS_STATUS_FATAL_ERROR;
    }

    if (fsaShimPrepareRequestChangeDir(shimBuffer, clientBody->clientHandle, path) == 0) {
        if (processFSAShimInThread(shimBuffer, client, block, errorMask, asyncData)) {

            return FS_STATUS_OK;
        }
    }

    return real_FSChangeDirAsync(client, block, path, errorMask, asyncData);
}

function_replacement_data_t fs_file_function_replacements[] = {
        REPLACE_FUNCTION(FSOpenFileExAsync, LIBRARY_COREINIT, FSOpenFileExAsync),
        REPLACE_FUNCTION(FSCloseFileAsync, LIBRARY_COREINIT, FSCloseFileAsync),
        REPLACE_FUNCTION(FSGetStatAsync, LIBRARY_COREINIT, FSGetStatAsync),
        REPLACE_FUNCTION(FSGetStatFileAsync, LIBRARY_COREINIT, FSGetStatFileAsync),
        REPLACE_FUNCTION_VIA_ADDRESS(FSReadFileGeneric, 0x3201C400 + 0x4ecc0, 0x101C400 + 0x4ecc0),
        REPLACE_FUNCTION(FSSetPosFileAsync, LIBRARY_COREINIT, FSSetPosFileAsync),
        REPLACE_FUNCTION(FSGetPosFileAsync, LIBRARY_COREINIT, FSGetPosFileAsync),
        REPLACE_FUNCTION(FSIsEofAsync, LIBRARY_COREINIT, FSIsEofAsync),
        REPLACE_FUNCTION_VIA_ADDRESS(FSWriteFileGeneric, 0x3201C400 + 0x4eec0, 0x101C400 + 0x4eec0),
        REPLACE_FUNCTION(FSTruncateFileAsync, LIBRARY_COREINIT, FSTruncateFileAsync),
        REPLACE_FUNCTION(FSRemoveAsync, LIBRARY_COREINIT, FSRemoveAsync),
        REPLACE_FUNCTION(FSRenameAsync, LIBRARY_COREINIT, FSRenameAsync),
        REPLACE_FUNCTION(FSFlushFileAsync, LIBRARY_COREINIT, FSFlushFileAsync),
        REPLACE_FUNCTION(FSChangeModeAsync, LIBRARY_COREINIT, FSChangeModeAsync),

        REPLACE_FUNCTION(FSOpenDirAsync, LIBRARY_COREINIT, FSOpenDirAsync),
        REPLACE_FUNCTION(FSReadDirAsync, LIBRARY_COREINIT, FSReadDirAsync),
        REPLACE_FUNCTION(FSCloseDirAsync, LIBRARY_COREINIT, FSCloseDirAsync),
        REPLACE_FUNCTION(FSRewindDirAsync, LIBRARY_COREINIT, FSRewindDirAsync),
        REPLACE_FUNCTION(FSMakeDirAsync, LIBRARY_COREINIT, FSMakeDirAsync),
        REPLACE_FUNCTION(FSChangeDirAsync, LIBRARY_COREINIT, FSChangeDirAsync),
};

uint32_t fs_file_function_replacements_size = sizeof(fs_file_function_replacements) / sizeof(function_replacement_data_t);

FSError processShimBufferForFS(FSShimWrapper *param) {
    FSError result    = doForLayer(param);
    FSStatus fsResult = FS_STATUS_MEDIA_ERROR;
    if (result == FS_ERROR_FORCE_REAL_FUNCTION) {
        if (param->sync == FS_SHIM_TYPE_SYNC) {
            fsResult = FS_STATUS_MEDIA_ERROR;
            DEBUG_FUNCTION_LINE_ERR("SYNC FS API is not supported");
            OSFatal("ContentRedirectionModule: SYNC FS API is not supported");
        } else if (param->sync == FS_SHIM_TYPE_ASYNC) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
            switch ((FSACommandEnum) param->shim->command) {
                case FSA_COMMAND_READ_FILE: {
                    auto *request = &param->shim->request.readFile;
                    fsResult      = real_FSReadFileGeneric(param->asyncFS.client, param->asyncFS.block, request->buffer, request->size, request->count, request->pos, request->handle, request->readFlags, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSReadFileGeneric. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_WRITE_FILE: {
                    auto *request = &param->shim->request.writeFile;
                    fsResult      = real_FSWriteFileGeneric(param->asyncFS.client, param->asyncFS.block, request->buffer, request->size, request->count, request->pos, request->handle, request->writeFlags, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSWriteFileGeneric. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_OPEN_FILE: {
                    auto *request = &param->shim->request.openFile;
                    // Hacky solution. We stored the pointer from the user in the response to use it at this point.
                    auto *hackyBuffer = (uint32_t *) &param->shim->response;
                    auto *handlePtr   = (FSFileHandle *) hackyBuffer[1];
                    fsResult          = real_FSOpenFileExAsync(param->asyncFS.client, param->asyncFS.block, request->path, request->mode, static_cast<FSMode>(request->unk0x290), static_cast<FSOpenFileFlags>(request->unk0x294), request->unk0x298, handlePtr, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSOpenFileExAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_CLOSE_FILE: {
                    auto *request = &param->shim->request.closeFile;
                    fsResult      = real_FSCloseFileAsync(param->asyncFS.client, param->asyncFS.block, request->handle, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSCloseFileAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_SET_POS_FILE: {
                    auto *request = &param->shim->request.setPosFile;
                    fsResult      = real_FSSetPosFileAsync(param->asyncFS.client, param->asyncFS.block, request->handle, request->pos, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSSetPosFileAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_GET_POS_FILE: {
                    auto *request = &param->shim->request.getPosFile;
                    // Hacky solution. We stored the pointer from the user in the response to use it at this point.
                    auto *hackyBuffer = (uint32_t *) &param->shim->response;
                    auto *posPtr      = (FSAFilePosition *) hackyBuffer[1];
                    fsResult          = real_FSGetPosFileAsync(param->asyncFS.client, param->asyncFS.block, request->handle, posPtr, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSGetPosFileAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_STAT_FILE: {
                    auto *request = &param->shim->request.statFile;
                    // Hacky solution. We stored the pointer from the user in the response to use it at this point.
                    auto *hackyBuffer = (uint32_t *) &param->shim->response;
                    auto *statPtr     = (FSStat *) hackyBuffer[1];
                    fsResult          = real_FSGetStatFileAsync(param->asyncFS.client, param->asyncFS.block, request->handle, statPtr, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSGetStatFileAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_GET_INFO_BY_QUERY: {
                    auto *request = &param->shim->request.getInfoByQuery;
                    if (request->type == FSA_QUERY_INFO_STAT) {
                        // Hacky solution. We stored the pointer from the user in the response to use it at this point.
                        auto *hackyBuffer = (uint32_t *) &param->shim->response;
                        auto *statPtr     = (FSStat *) hackyBuffer[1];
                        fsResult          = real_FSGetStatAsync(param->asyncFS.client, param->asyncFS.block, request->path, statPtr, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                        if (fsResult != FS_STATUS_OK) {
                            DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSGetStatAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                            handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                        }
                    } else {
                        DEBUG_FUNCTION_LINE_ERR("Missing real implementation for FSA_COMMAND_GET_INFO_BY_QUERY type %08X", request->type);
                    }
                    break;
                }
                case FSA_COMMAND_IS_EOF: {
                    auto *request = &param->shim->request.isEof;
                    fsResult      = real_FSIsEofAsync(param->asyncFS.client, param->asyncFS.block, request->handle, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSIsEofAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_TRUNCATE_FILE: {
                    auto *request = &param->shim->request.truncateFile;
                    fsResult      = real_FSTruncateFileAsync(param->asyncFS.client, param->asyncFS.block, request->handle, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSTruncateFileAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_REMOVE: {
                    auto *request = &param->shim->request.remove;
                    fsResult      = real_FSRemoveAsync(param->asyncFS.client, param->asyncFS.block, request->path, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSRemoveAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_RENAME: {
                    auto *request = &param->shim->request.rename;
                    fsResult      = real_FSRenameAsync(param->asyncFS.client, param->asyncFS.block, request->oldPath, request->newPath, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSRenameAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_FLUSH_FILE: {
                    auto *request = &param->shim->request.flushFile;
                    fsResult      = real_FSFlushFileAsync(param->asyncFS.client, param->asyncFS.block, request->handle, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSFlushFileAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_CHANGE_MODE: {
                    auto *request = &param->shim->request.changeMode;
                    fsResult      = real_FSChangeModeAsync(param->asyncFS.client, param->asyncFS.block, request->path, (FSMode) request->mode1, (FSMode) request->mode2, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSChangeModeAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_OPEN_DIR: {
                    auto *request = &param->shim->request.openDir;
                    // Hacky solution. We stored the pointer from the user in the response to use it at this point.
                    auto *hackyBuffer = (uint32_t *) &param->shim->response;
                    auto *handlePtr   = (FSDirectoryHandle *) hackyBuffer[1];
                    fsResult          = real_FSOpenDirAsync(param->asyncFS.client, param->asyncFS.block, request->path, handlePtr, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSOpenDirAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_READ_DIR: {
                    auto *request = &param->shim->request.readDir;
                    // Hacky solution. We stored the pointer from the user in the response to use it at this point.
                    auto *hackyBuffer = (uint32_t *) &param->shim->response;
                    auto *entryPtr    = (FSDirectoryEntry *) hackyBuffer[1];
                    fsResult          = real_FSReadDirAsync(param->asyncFS.client, param->asyncFS.block, request->handle, entryPtr, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSReadDirAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_CLOSE_DIR: {
                    auto *request = &param->shim->request.closeDir;
                    fsResult      = real_FSCloseDirAsync(param->asyncFS.client, param->asyncFS.block, request->handle, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSCloseDirAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_REWIND_DIR: {
                    auto *request = &param->shim->request.rewindDir;
                    fsResult      = real_FSRewindDirAsync(param->asyncFS.client, param->asyncFS.block, request->handle, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSRewindDirAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_MAKE_DIR: {
                    auto *request = &param->shim->request.makeDir;
                    fsResult      = real_FSMakeDirAsync(param->asyncFS.client, param->asyncFS.block, request->path, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSMakeDirAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                case FSA_COMMAND_CHANGE_DIR: {
                    auto *request = &param->shim->request.changeDir;
                    fsResult      = real_FSChangeDirAsync(param->asyncFS.client, param->asyncFS.block, request->path, param->asyncFS.errorMask, &param->asyncFS.asyncData);
                    if (fsResult != FS_STATUS_OK) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to submit real_FSChangeDirAsync. Return was %d. Fake actual async result to FS_STATUS_MEDIA_ERROR instead", fsResult);
                        handleAsyncResult(param->asyncFS.client, param->asyncFS.block, &param->asyncFS.asyncData, FS_STATUS_MEDIA_ERROR);
                    }
                    break;
                }
                default: {
                    DEBUG_FUNCTION_LINE_ERR("Missing real implementation for command %08X", param->shim->command);
                    fsResult = FS_STATUS_FATAL_ERROR;
                }
            }
#pragma GCC diagnostic pop
        }
    }
    free(param);
    if (fsResult != FS_STATUS_OK) {
        result = FS_ERROR_MEDIA_ERROR;
    }
    return result;
}