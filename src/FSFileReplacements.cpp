#include "FSFileReplacements.h"
#include "FileUtils.h"
#include "utils/logger.h"
#include <coreinit/dynload.h>
#include <cstdarg>

DECL_FUNCTION(FSStatus, FSOpenFile, FSClient *client, FSCmdBlock *block, char *path, const char *mode, FSFileHandle *handle, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    if (isForceRealFunction(errorMask)) {
        return real_FSOpenFile(client, block, path, mode, handle, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, p = path, m = mode, h = handle](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSOpenFile(c, b, p, m, h, realErrorMask);
            },
            [f = getFullPathForClient(client, path), m = mode, h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSOpenFileWrapper(f.c_str(), m, h);
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSOpenFileAsync, FSClient *client, FSCmdBlock *block, char *path, const char *mode, FSFileHandle *handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    if (isForceRealFunction(errorMask)) {
        return real_FSOpenFileAsync(client, block, path, mode, handle, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, p = path, m = mode, h = handle, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSOpenFileAsync(c, b, p, m, h, realErrorMask, a);
            },
            [p = getFullPathForClient(client, path), m = mode, h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSOpenFileWrapper(p.c_str(), m, h);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSCloseFile, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSCloseFile(client, block, handle, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSCloseFile(c, b, h, realErrorMask);
            },
            [h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSCloseFileWrapper(h);
            },
            [h = handle, filename = __FILENAME__, func = __FUNCTION__, line = __LINE__](IFSWrapper *layer, FSStatus res) -> FSStatus {
                if (layer->isValidFileHandle(h)) {
                    layer->deleteFileHandle(h);
                } else {
                    DEBUG_FUNCTION_LINE_ERR_LAMBDA(filename, func, line, "Expected to delete fileHandle by handle %08X but it was not found", h);
                }
                DEBUG_FUNCTION_LINE_VERBOSE_EX(filename, func, line, "Sync result %d", res);
                return res;
            });
}

DECL_FUNCTION(FSStatus, FSCloseFileAsync, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSCloseFileAsync(client, block, handle, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSCloseFileAsync(c, b, h, realErrorMask, a);
            },
            [h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSCloseFileWrapper(h);
            },
            [c = client, b = block, h = handle, a = asyncData, filename = __FILENAME__, func = __FUNCTION__, line = __LINE__](IFSWrapper *layer, FSStatus res) -> FSStatus {
                if (layer->isValidFileHandle(h)) {
                    layer->deleteFileHandle(h);
                } else {
                    DEBUG_FUNCTION_LINE_ERR_LAMBDA(filename, func, line, "Expected to delete fileHandle by handle %08X but it was not found", h);
                }
                DEBUG_FUNCTION_LINE_VERBOSE_EX(filename, func, line, "Async result %d", res);
                return send_result_async(c, b, a, res);
            });
}

DECL_FUNCTION(FSStatus, FSGetStat, FSClient *client, FSCmdBlock *block, char *path, FSStat *stats, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    if (isForceRealFunction(errorMask)) {
        return real_FSGetStat(client, block, path, stats, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, p = path, s = stats](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSGetStat(c, b, p, s, realErrorMask);
            },
            [p = getFullPathForClient(client, path), s = stats](IFSWrapper *layer) -> FSStatus {
                return layer->FSGetStatWrapper(p.c_str(), s);
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSGetStatAsync, FSClient *client, FSCmdBlock *block, char *path, FSStat *stats, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    if (isForceRealFunction(errorMask)) {
        return real_FSGetStatAsync(client, block, path, stats, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, p = path, s = stats, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSGetStatAsync(c, b, p, s, realErrorMask, a);
            },
            [p = getFullPathForClient(client, path), s = stats](IFSWrapper *layer) -> FSStatus {
                return layer->FSGetStatWrapper(p.c_str(), s);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSGetStatFile, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSStat *stats, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSGetStatFile(client, block, handle, stats, getRealErrorFlag(errorMask));
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, s = stats](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSGetStatFile(c, b, h, s, realErrorMask);
            },
            [h = handle, s = stats](IFSWrapper *layer) -> FSStatus {
                return layer->FSGetStatFileWrapper(h, s);
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSGetStatFileAsync, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSStat *stats, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSGetStatFileAsync(client, block, handle, stats, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, s = stats, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSGetStatFileAsync(c, b, h, s, realErrorMask, a);
            },
            [h = handle, s = stats](IFSWrapper *layer) -> FSStatus {
                return layer->FSGetStatFileWrapper(h, s);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSReadFile, FSClient *client, FSCmdBlock *block, void *buffer, uint32_t size, uint32_t count, FSFileHandle handle, uint32_t unk1, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSReadFile(client, block, buffer, size, count, handle, unk1, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, s = size, co = count, bu = buffer, u = unk1](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSReadFile(c, b, bu, s, co, h, u, realErrorMask);
            },
            [b = buffer, s = size, c = count, h = handle, u = unk1](IFSWrapper *layer) -> FSStatus {
                return layer->FSReadFileWrapper(b, s, c, h, u);
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSReadFileAsync, FSClient *client, FSCmdBlock *block, void *buffer, uint32_t size, uint32_t count, FSFileHandle handle, uint32_t unk1, FSErrorFlag errorMask,
              FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSReadFileAsync(client, block, buffer, size, count, handle, unk1, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, s = size, co = count, bu = buffer, u = unk1, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSReadFileAsync(c, b, bu, s, co, h, u, realErrorMask, a);
            },
            [b = buffer, s = size, c = count, h = handle, u = unk1](IFSWrapper *layer) -> FSStatus {
                return layer->FSReadFileWrapper(b, s, c, h, u);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSReadFileWithPos, FSClient *client, FSCmdBlock *block, void *buffer, uint32_t size, uint32_t count, uint32_t pos, FSFileHandle handle, uint32_t unk1, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSReadFileWithPos(client, block, buffer, size, count, pos, handle, unk1, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, s = size, co = count, p = pos, bu = buffer, u = unk1](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSReadFileWithPos(c, b, bu, s, co, p, h, u, realErrorMask);
            },
            [b = buffer, s = size, c = count, p = pos, h = handle, u = unk1](IFSWrapper *layer) -> FSStatus {
                return layer->FSReadFileWithPosWrapper(b, s, c, p, h, u);
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSReadFileWithPosAsync, FSClient *client, FSCmdBlock *block, void *buffer, uint32_t size, uint32_t count, uint32_t pos, FSFileHandle handle, int32_t unk1, FSErrorFlag errorMask,
              FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSReadFileWithPosAsync(client, block, buffer, size, count, pos, handle, unk1, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, s = size, co = count, p = pos, bu = buffer, u = unk1, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSReadFileWithPosAsync(c, b, bu, s, co, p, h, u, realErrorMask, a);
            },
            [b = buffer, s = size, c = count, p = pos, h = handle, u = unk1](IFSWrapper *layer) -> FSStatus {
                return layer->FSReadFileWithPosWrapper(b, s, c, p, h, u);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSSetPosFile, FSClient *client, FSCmdBlock *block, FSFileHandle handle, uint32_t pos, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSSetPosFile(client, block, handle, pos, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, p = pos](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSSetPosFile(c, b, h, p, realErrorMask);
            },
            [h = handle, p = pos](IFSWrapper *layer) -> FSStatus {
                return layer->FSSetPosFileWrapper(h, p);
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSSetPosFileAsync, FSClient *client, FSCmdBlock *block, FSFileHandle handle, uint32_t pos, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSSetPosFileAsync(client, block, handle, pos, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, p = pos, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSSetPosFileAsync(c, b, h, p, realErrorMask, a);
            },
            [h = handle, p = pos](IFSWrapper *layer) -> FSStatus {
                return layer->FSSetPosFileWrapper(h, p);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSGetPosFile, FSClient *client, FSCmdBlock *block, FSFileHandle handle, uint32_t *pos, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSGetPosFile(client, block, handle, pos, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, p = pos](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSGetPosFile(c, b, h, p, realErrorMask);
            },
            [h = handle, p = pos](IFSWrapper *layer) -> FSStatus {
                return layer->FSGetPosFileWrapper(h, p);
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSGetPosFileAsync, FSClient *client, FSCmdBlock *block, FSFileHandle handle, uint32_t *pos, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSGetPosFileAsync(client, block, handle, pos, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, p = pos, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSGetPosFileAsync(c, b, h, p, realErrorMask, a);
            },
            [h = handle, p = pos](IFSWrapper *layer) -> FSStatus {
                return layer->FSGetPosFileWrapper(h, p);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSIsEof, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSIsEof(client, block, handle, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSIsEof(c, b, h, realErrorMask);
            },
            [h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSIsEofWrapper(h);
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSIsEofAsync, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSIsEofAsync(client, block, handle, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSIsEofAsync(c, b, h, realErrorMask, a);
            },
            [h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSIsEofWrapper(h);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSWriteFile, FSClient *client, FSCmdBlock *block, uint8_t *buffer, uint32_t size, uint32_t count, FSFileHandle handle, uint32_t unk1, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSWriteFile(client, block, buffer, size, count, handle, unk1, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, bu = buffer, s = size, co = count, h = handle, u = unk1](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSWriteFile(c, b, bu, s, co, h, u, realErrorMask);
            },
            [b = buffer, s = size, c = count, h = handle, u = unk1](IFSWrapper *layer) -> FSStatus {
                return layer->FSWriteFileWrapper(b, s, c, h, u);
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSWriteFileAsync, FSClient *client, FSCmdBlock *block, uint8_t *buffer, uint32_t size, uint32_t count, FSFileHandle handle, uint32_t unk1, FSErrorFlag errorMask,
              FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSWriteFileAsync(client, block, buffer, size, count, handle, unk1, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, bu = buffer, s = size, co = count, h = handle, u = unk1, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSWriteFileAsync(c, b, bu, s, co, h, u, realErrorMask, a);
            },
            [b = buffer, s = size, c = count, h = handle, u = unk1](IFSWrapper *layer) -> FSStatus {
                return layer->FSWriteFileWrapper(b, s, c, h, u);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSTruncateFile, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSTruncateFile(client, block, handle, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSTruncateFile(c, b, h, realErrorMask);
            },
            [h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSTruncateFileWrapper(h);
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSTruncateFileAsync, FSClient *client, FSCmdBlock *block, FSFileHandle handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSTruncateFileAsync(client, block, handle, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSTruncateFileAsync(c, b, h, realErrorMask, a);
            },
            [h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSTruncateFileWrapper(h);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSRemove, FSClient *client, FSCmdBlock *block, char *path, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    if (isForceRealFunction(errorMask)) {
        return real_FSRemove(client, block, path, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, p = path](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSRemove(c, b, p, realErrorMask);
            },
            [p = getFullPathForClient(client, path)](IFSWrapper *layer) -> FSStatus {
                return layer->FSRemoveWrapper(p.c_str());
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSRemoveAsync, FSClient *client, FSCmdBlock *block, char *path, [[maybe_unused]] FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    if (isForceRealFunction(errorMask)) {
        return real_FSRemoveAsync(client, block, path, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, p = path, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSRemoveAsync(c, b, p, realErrorMask, a);
            },
            [p = getFullPathForClient(client, path)](IFSWrapper *layer) -> FSStatus {
                return layer->FSRemoveWrapper(p.c_str());
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSRename, FSClient *client, FSCmdBlock *block, char *oldPath, char *newPath, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s -> %s", oldPath, newPath);
    if (isForceRealFunction(errorMask)) {
        return real_FSRename(client, block, oldPath, newPath, getRealErrorFlag(errorMask));
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, oP = oldPath, nP = newPath](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSRename(c, b, oP, nP, realErrorMask);
            },
            [oP = getFullPathForClient(client, oldPath), nP = getFullPathForClient(client, newPath)](IFSWrapper *layer) -> FSStatus {
                return layer->FSRenameWrapper(oP.c_str(), nP.c_str());
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSRenameAsync,
              FSClient *client,
              FSCmdBlock *block,
              char *oldPath,
              char *newPath,
              FSErrorFlag errorMask,
              FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s -> %s", oldPath, newPath);
    if (isForceRealFunction(errorMask)) {
        return real_FSRenameAsync(client, block, oldPath, newPath, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, oP = oldPath, nP = newPath, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSRenameAsync(c, b, oP, nP, realErrorMask, a);
            },
            [oP = getFullPathForClient(client, oldPath), nP = getFullPathForClient(client, newPath)](IFSWrapper *layer) -> FSStatus {
                return layer->FSRenameWrapper(oP.c_str(), nP.c_str());
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSFlushFile, FSClient *client, FSCmdBlock *block, [[maybe_unused]] FSFileHandle handle, [[maybe_unused]] FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSFlushFile(client, block, handle, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSFlushFile(c, b, h, realErrorMask);
            },
            [h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSFlushFileWrapper(h);
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSFlushFileAsync, FSClient *client, FSCmdBlock *block, [[maybe_unused]] FSFileHandle handle, [[maybe_unused]] FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSFlushFileAsync(client, block, handle, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSFlushFileAsync(c, b, h, realErrorMask, a);
            },
            [h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSFlushFileWrapper(h);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSChangeModeAsync,
              FSClient *client,
              FSCmdBlock *block,
              char *path,
              [[maybe_unused]] FSMode mode,
              [[maybe_unused]] FSMode modeMask,
              [[maybe_unused]] FSErrorFlag errorMask,
              FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_ERR("NOT IMPLEMENTED FSChangeModeAsync %s mode: %08X", path, mode);
    return real_FSChangeModeAsync(client, block, path, mode, modeMask, errorMask, asyncData);
}

DECL_FUNCTION(FSStatus, FSGetFreeSpaceSizeAsync, FSClient *client, FSCmdBlock *block, char *path, [[maybe_unused]] uint64_t *outSize, [[maybe_unused]] FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_ERR("NOT IMPLEMENTED FSGetFreeSpaceSizeAsync %s", path);
    return real_FSGetFreeSpaceSizeAsync(client, block, path, outSize, errorMask, asyncData);
}

function_replacement_data_t fs_file_function_replacements[] = {
        REPLACE_FUNCTION(FSOpenFile, LIBRARY_COREINIT, FSOpenFile),
        REPLACE_FUNCTION(FSOpenFileAsync, LIBRARY_COREINIT, FSOpenFileAsync),

        REPLACE_FUNCTION(FSCloseFile, LIBRARY_COREINIT, FSCloseFile),
        REPLACE_FUNCTION(FSCloseFileAsync, LIBRARY_COREINIT, FSCloseFileAsync),

        REPLACE_FUNCTION(FSGetStat, LIBRARY_COREINIT, FSGetStat),
        REPLACE_FUNCTION(FSGetStatAsync, LIBRARY_COREINIT, FSGetStatAsync),

        REPLACE_FUNCTION(FSGetStatFile, LIBRARY_COREINIT, FSGetStatFile),
        REPLACE_FUNCTION(FSGetStatFileAsync, LIBRARY_COREINIT, FSGetStatFileAsync),

        REPLACE_FUNCTION(FSReadFile, LIBRARY_COREINIT, FSReadFile),
        REPLACE_FUNCTION(FSReadFileAsync, LIBRARY_COREINIT, FSReadFileAsync),

        REPLACE_FUNCTION(FSReadFileWithPos, LIBRARY_COREINIT, FSReadFileWithPos),
        REPLACE_FUNCTION(FSReadFileWithPosAsync, LIBRARY_COREINIT, FSReadFileWithPosAsync),

        REPLACE_FUNCTION(FSSetPosFile, LIBRARY_COREINIT, FSSetPosFile),
        REPLACE_FUNCTION(FSSetPosFileAsync, LIBRARY_COREINIT, FSSetPosFileAsync),

        REPLACE_FUNCTION(FSGetPosFile, LIBRARY_COREINIT, FSGetPosFile),
        REPLACE_FUNCTION(FSGetPosFileAsync, LIBRARY_COREINIT, FSGetPosFileAsync),

        REPLACE_FUNCTION(FSIsEof, LIBRARY_COREINIT, FSIsEof),
        REPLACE_FUNCTION(FSIsEofAsync, LIBRARY_COREINIT, FSIsEofAsync),

        REPLACE_FUNCTION(FSWriteFile, LIBRARY_COREINIT, FSWriteFile),
        REPLACE_FUNCTION(FSWriteFileAsync, LIBRARY_COREINIT, FSWriteFileAsync),

        REPLACE_FUNCTION(FSTruncateFile, LIBRARY_COREINIT, FSTruncateFile),
        REPLACE_FUNCTION(FSTruncateFileAsync, LIBRARY_COREINIT, FSTruncateFileAsync),

        REPLACE_FUNCTION(FSRemove, LIBRARY_COREINIT, FSRemove),
        REPLACE_FUNCTION(FSRemoveAsync, LIBRARY_COREINIT, FSRemoveAsync),

        REPLACE_FUNCTION(FSRename, LIBRARY_COREINIT, FSRename),
        REPLACE_FUNCTION(FSRenameAsync, LIBRARY_COREINIT, FSRenameAsync),

        REPLACE_FUNCTION(FSFlushFile, LIBRARY_COREINIT, FSFlushFile),
        REPLACE_FUNCTION(FSFlushFileAsync, LIBRARY_COREINIT, FSFlushFileAsync),

        REPLACE_FUNCTION(FSChangeModeAsync, LIBRARY_COREINIT, FSChangeModeAsync),

        //REPLACE_FUNCTION(FSGetFreeSpaceSizeAsync, LIBRARY_COREINIT, FSGetFreeSpaceSizeAsync),
        //REPLACE_FUNCTION_VIA_ADDRESS(FSGetFreeSpaceSizeAsync, LIBRARY_COREINIT, 0x0A000000 + 0x0256079c),
};

uint32_t fs_file_function_replacements_size = sizeof(fs_file_function_replacements) / sizeof(function_replacement_data_t);