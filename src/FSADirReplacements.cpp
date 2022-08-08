#include "FSDirReplacements.h"
#include "FileUtils.h"
#include "IFSWrapper.h"
#include "utils/logger.h"
#include <coreinit/cache.h>
#include <coreinit/filesystem.h>

DECL_FUNCTION(FSError, FSAOpenDir, FSAClientHandle client, const char *path, FSADirectoryHandle *dirHandle) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    return doForLayerFSA(
            [c = client, p = path, h = dirHandle]() -> FSError {
                return real_FSAOpenDir(c, p, h);
            },
            [p = path, h = dirHandle](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                return layer->FSOpenDirWrapper(p, h);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSAReadDir, FSAClientHandle client, FSADirectoryHandle dirHandle, FSADirectoryEntry *directoryEntry) {
    DEBUG_FUNCTION_LINE_VERBOSE("%08X", dirHandle);
    return doForLayerFSA(
            [c = client, h = dirHandle, de = directoryEntry]() -> FSError {
                return real_FSAReadDir(c, h, de);
            },
            [h = dirHandle, de = directoryEntry](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                return layer->FSReadDirWrapper(h, de);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSARewindDir, FSAClientHandle client, FSADirectoryHandle dirHandle) {
    DEBUG_FUNCTION_LINE_VERBOSE("%08X", dirHandle);
    return doForLayerFSA(
            [c = client, h = dirHandle]() -> FSError {
                return real_FSARewindDir(c, h);
            },
            [h = dirHandle](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                return layer->FSRewindDirWrapper(h);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSACloseDir, FSAClientHandle client, FSADirectoryHandle dirHandle) {
    DEBUG_FUNCTION_LINE_VERBOSE("%08X", dirHandle);
    return doForLayerFSA(
            [c = client, h = dirHandle]() -> FSError {
                return real_FSACloseDir(c, h);
            },
            [h = dirHandle](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                return layer->FSCloseDirWrapper(h);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSAMakeDir, FSAClientHandle client, const char *path, FSMode mode) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    return doForLayerFSA(
            [c = client, p = path, m = mode]() -> FSError {
                return real_FSAMakeDir(c, p, m);
            },
            [p = path, m = mode](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                return layer->FSMakeDirWrapper(p);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSAChangeDir, FSAClientHandle client, const char *path) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    setWorkingDirForFSAClient(client, path);
    return real_FSAChangeDir(client, path);
}

function_replacement_data_t fsa_dir_function_replacements[] = {
        REPLACE_FUNCTION(FSAOpenDir, LIBRARY_COREINIT, FSAOpenDir),
        REPLACE_FUNCTION(FSAReadDir, LIBRARY_COREINIT, FSAReadDir),
        REPLACE_FUNCTION(FSARewindDir, LIBRARY_COREINIT, FSARewindDir),
        REPLACE_FUNCTION(FSACloseDir, LIBRARY_COREINIT, FSACloseDir),
        REPLACE_FUNCTION(FSAMakeDir, LIBRARY_COREINIT, FSAMakeDir),
        REPLACE_FUNCTION(FSAChangeDir, LIBRARY_COREINIT, FSAChangeDir),
};
uint32_t fsa_dir_function_replacements_size = sizeof(fsa_dir_function_replacements) / sizeof(function_replacement_data_t);
