#include "FSDirReplacements.h"
#include "FileUtils.h"
#include "IFSWrapper.h"
#include "utils/logger.h"
#include <coreinit/cache.h>
#include <coreinit/filesystem.h>

DECL_FUNCTION(FSStatus, FSOpenDir, FSClient *client, FSCmdBlock *block, char *path, FSDirectoryHandle *handle, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    if (isForceRealFunction(errorMask)) {
        return real_FSOpenDir(client, block, path, handle, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, p = path](FSErrorFlag realErrorMask) -> FSStatus {
                auto res = real_FSOpenDir(c, b, p, h, realErrorMask);
                return res;
            },
            [f = getFullPathForClient(client, path), h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSOpenDirWrapper(f.c_str(), h);
            },
            SYNC_RESULT_HANDLER);
}


DECL_FUNCTION(FSStatus, FSOpenDirAsync, FSClient *client, FSCmdBlock *block, char *path, FSDirectoryHandle *handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    if (isForceRealFunction(errorMask)) {
        return real_FSOpenDirAsync(client, block, path, handle, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, p = path, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSOpenDirAsync(c, b, p, h, realErrorMask, a);
            },
            [f = getFullPathForClient(client, path), h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSOpenDirWrapper(f.c_str(), h);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSReadDir, FSClient *client, FSCmdBlock *block, FSDirectoryHandle handle, [[maybe_unused]] FSDirectoryEntry *entry, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("", errorMask);
    if (isForceRealFunction(errorMask)) {
        return real_FSReadDir(client, block, handle, entry, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, e = entry](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSReadDir(c, b, h, e, realErrorMask);
            },
            [h = handle, e = entry](IFSWrapper *layer) -> FSStatus {
                return layer->FSReadDirWrapper(h, e);
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSReadDirAsync, FSClient *client, FSCmdBlock *block, FSDirectoryHandle handle, FSDirectoryEntry *entry, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE();
    if (isForceRealFunction(errorMask)) {
        return real_FSReadDirAsync(client, block, handle, entry, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, e = entry, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSReadDirAsync(c, b, h, e, realErrorMask, a);
            },
            [h = handle, e = entry](IFSWrapper *layer) -> FSStatus {
                return layer->FSReadDirWrapper(h, e);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSCloseDir, FSClient *client, FSCmdBlock *block, FSDirectoryHandle handle, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSCloseDir(client, block, handle, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSCloseDir(c, b, h, realErrorMask);
            },
            [h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSCloseDirWrapper(h);
            },
            [h = handle, filename = __FILENAME__, func = __FUNCTION__, line = __LINE__](IFSWrapper *layer, FSStatus res) -> FSStatus {
                if (layer->isValidDirHandle(h)) {
                    layer->deleteDirHandle(h);
                } else {
                    DEBUG_FUNCTION_LINE_ERR_LAMBDA(filename, func, line, "Expected to delete dirHandle by %08X but it was not found", h);
                }
                DEBUG_FUNCTION_LINE_VERBOSE_EX(filename, func, line, "Sync result %d", res);
                return res;
            });
}

DECL_FUNCTION(FSStatus, FSCloseDirAsync, FSClient *client, FSCmdBlock *block, FSDirectoryHandle handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE();
    if (isForceRealFunction(errorMask)) {
        return real_FSCloseDirAsync(client, block, handle, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSCloseDirAsync(c, b, h, realErrorMask, a);
            },
            [h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSCloseDirWrapper(h);
            },
            [c = client, b = block, h = handle, a = asyncData, filename = __FILENAME__, func = __FUNCTION__, line = __LINE__](IFSWrapper *layer, FSStatus res) -> FSStatus {
                if (layer->isValidDirHandle(h)) {
                    layer->deleteDirHandle(h);
                } else {
                    DEBUG_FUNCTION_LINE_ERR_LAMBDA(filename, func, line, "Expected to delete dirHandle by %08X but it was not found", h);
                }
                DEBUG_FUNCTION_LINE_VERBOSE_EX(filename, func, line, "Async result %d", res);
                return send_result_async(c, b, a, res);
            });
}

DECL_FUNCTION(FSStatus, FSRewindDir, FSClient *client, FSCmdBlock *block, FSDirectoryHandle handle, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("");
    if (isForceRealFunction(errorMask)) {
        return real_FSRewindDir(client, block, handle, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSRewindDir(c, b, h, realErrorMask);
            },
            [h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSRewindDirWrapper(h);
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSRewindDirAsync, FSClient *client, FSCmdBlock *block, FSDirectoryHandle handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE();
    if (isForceRealFunction(errorMask)) {
        return real_FSRewindDirAsync(client, block, handle, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, h = handle, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSRewindDirAsync(c, b, h, realErrorMask, a);
            },
            [h = handle](IFSWrapper *layer) -> FSStatus {
                return layer->FSRewindDirWrapper(h);
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSMakeDir, FSClient *client, FSCmdBlock *block, char *path, FSErrorFlag errorMask) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    if (isForceRealFunction(errorMask)) {
        return real_FSMakeDir(client, block, path, errorMask);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, p = path](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSMakeDir(c, b, p, realErrorMask);
            },
            [f = getFullPathForClient(client, path)](IFSWrapper *layer) -> FSStatus {
                return layer->FSMakeDirWrapper(f.c_str());
            },
            SYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSMakeDirAsync, FSClient *client, FSCmdBlock *block, char *path, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    if (isForceRealFunction(errorMask)) {
        return real_FSMakeDirAsync(client, block, path, getRealErrorFlag(errorMask), asyncData);
    }
    return doForLayer(
            client,
            errorMask,
            [c = client, b = block, p = path, a = asyncData](FSErrorFlag realErrorMask) -> FSStatus {
                return real_FSMakeDirAsync(c, b, p, realErrorMask, a);
            },
            [f = getFullPathForClient(client, path)](IFSWrapper *layer) -> FSStatus {
                return layer->FSMakeDirWrapper(f.c_str());
            },
            ASYNC_RESULT_HANDLER);
}

DECL_FUNCTION(FSStatus, FSChangeDirAsync, FSClient *client, FSCmdBlock *block, const char *path, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    DEBUG_FUNCTION_LINE_VERBOSE("FSChangeDirAsync %s", path);
    setWorkingDir(client, path);
    return real_FSChangeDirAsync(client, block, path, errorMask, asyncData);
}

function_replacement_data_t fs_dir_function_replacements[] = {
        REPLACE_FUNCTION(FSOpenDir, LIBRARY_COREINIT, FSOpenDir),
        REPLACE_FUNCTION(FSOpenDirAsync, LIBRARY_COREINIT, FSOpenDirAsync),

        REPLACE_FUNCTION(FSReadDir, LIBRARY_COREINIT, FSReadDir),
        REPLACE_FUNCTION(FSReadDirAsync, LIBRARY_COREINIT, FSReadDirAsync),

        REPLACE_FUNCTION(FSCloseDir, LIBRARY_COREINIT, FSCloseDir),
        REPLACE_FUNCTION(FSCloseDirAsync, LIBRARY_COREINIT, FSCloseDirAsync),

        REPLACE_FUNCTION(FSRewindDir, LIBRARY_COREINIT, FSRewindDir),
        REPLACE_FUNCTION(FSRewindDirAsync, LIBRARY_COREINIT, FSRewindDirAsync),

        REPLACE_FUNCTION(FSMakeDir, LIBRARY_COREINIT, FSMakeDir),
        REPLACE_FUNCTION(FSMakeDirAsync, LIBRARY_COREINIT, FSMakeDirAsync),

        REPLACE_FUNCTION(FSChangeDirAsync, LIBRARY_COREINIT, FSChangeDirAsync),
};
uint32_t fs_dir_function_replacements_size = sizeof(fs_dir_function_replacements) / sizeof(function_replacement_data_t);
