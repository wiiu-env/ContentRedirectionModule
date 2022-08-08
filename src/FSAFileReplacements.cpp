#include "FSFileReplacements.h"
#include "FileUtils.h"
#include "utils/logger.h"
#include <coreinit/filesystem_fsa.h>

DECL_FUNCTION(FSError, FSAOpenFileEx, FSAClientHandle client, const char *path, const char *mode, FSMode createMode, FSOpenFileFlags openFlag, uint32_t preallocSize, FSAFileHandle *handle) {
    DEBUG_FUNCTION_LINE_VERBOSE("path: %s mode: %d", path, mode);

    return doForLayerFSA(
            [c = client, p = path, m = mode, cm = createMode, of = openFlag, pa = preallocSize, h = handle]() -> FSError {
                return real_FSAOpenFileEx(c, p, m, cm, of, pa, h);
            },
            [f = getFullPathForFSAClient(client, path), m = mode, h = handle](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                // FSAFileHandle is just an alias for FSFileHandle
                return layer->FSOpenFileWrapper(f.c_str(), m, h);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSACloseFile, FSAClientHandle client, FSAFileHandle fileHandle) {
    DEBUG_FUNCTION_LINE_VERBOSE("handle: %08X", fileHandle);

    return doForLayerFSA(
            [c = client, h = fileHandle]() -> FSError {
                return real_FSACloseFile(c, h);
            },
            [h = fileHandle](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                // FSAFileHandle is just an alias for FSFileHandle
                return layer->FSCloseFileWrapper(h);
            },
            [h = fileHandle, filename = __FILENAME__, func = __FUNCTION__, line = __LINE__](std::unique_ptr<IFSWrapper> &layer, FSError res) -> FSError {
                if (layer->isValidFileHandle(h)) {
                    layer->deleteFileHandle(h);
                } else {
                    DEBUG_FUNCTION_LINE_ERR_LAMBDA(filename, func, line, "Expected to delete fileHandle by handle %08X but it was not found", h);
                }
                DEBUG_FUNCTION_LINE_VERBOSE_EX(filename, func, line, "Sync result %d", res);
                return res;
            });
}

DECL_FUNCTION(FSError, FSAFlushFile, FSAClientHandle client, FSAFileHandle fileHandle) {
    DEBUG_FUNCTION_LINE_VERBOSE("handle: %08X", fileHandle);
    return doForLayerFSA(
            [c = client, h = fileHandle]() -> FSError {
                return real_FSAFlushFile(c, h);
            },
            [h = fileHandle](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                // FSAFileHandle is just an alias for FSFileHandle
                return layer->FSFlushFileWrapper(h);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSAGetStat, FSAClientHandle client, const char *path, FSAStat *stat) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    return doForLayerFSA(

            [c = client, p = path, s = stat]() -> FSError {
                return real_FSAGetStat(c, p, s);
            },
            [f = getFullPathForFSAClient(client, path), s = stat](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                // FSAStat is just an alias for FSStat
                return layer->FSGetStatWrapper(f.c_str(), s);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSAGetStatFile, FSAClientHandle client, FSAFileHandle fileHandle, FSAStat *stat) {
    DEBUG_FUNCTION_LINE_VERBOSE("handle: %08X", fileHandle);
    return doForLayerFSA(
            [c = client, h = fileHandle, s = stat]() -> FSError {
                return real_FSAGetStatFile(c, h, s);
            },
            [h = fileHandle, s = stat](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                // FSAFileHandle is just an alias for FSFileHandle
                // FSAStat is just an alias for FSStat
                return layer->FSGetStatFileWrapper(h, s);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSARemove, FSAClientHandle client, const char *path) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s", path);
    return doForLayerFSA(
            [c = client, p = path]() -> FSError {
                return real_FSARemove(c, p);
            },
            [f = getFullPathForFSAClient(client, path)](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                return layer->FSRemoveWrapper(f.c_str());
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSARename, FSAClientHandle client, const char *oldPath, const char *newPath) {
    DEBUG_FUNCTION_LINE_VERBOSE("%s %s", oldPath, newPath);
    return doForLayerFSA(
            [c = client, op = oldPath, np = newPath]() -> FSError {
                return real_FSARename(c, op, np);
            },
            [op = getFullPathForFSAClient(client, oldPath), np = getFullPathForFSAClient(client, newPath)](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                return layer->FSRenameWrapper(op.c_str(), np.c_str());
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSASetPosFile, FSAClientHandle client, FSAFileHandle fileHandle, uint32_t pos) {
    DEBUG_FUNCTION_LINE_VERBOSE("handle: %08X pos: %08X", fileHandle, pos);
    return doForLayerFSA(
            [c = client, h = fileHandle, p = pos]() -> FSError {
                return real_FSASetPosFile(c, h, p);
            },
            [h = fileHandle, p = pos](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                return layer->FSSetPosFileWrapper(h, p);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSATruncateFile, FSAClientHandle client, FSAFileHandle fileHandle) {
    DEBUG_FUNCTION_LINE_VERBOSE("%08X", fileHandle);
    return doForLayerFSA(
            [c = client, h = fileHandle]() -> FSError {
                return real_FSATruncateFile(c, h);
            },
            [h = fileHandle](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                return layer->FSTruncateFileWrapper(h);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSAReadFile, FSAClientHandle client, void *buffer, uint32_t size, uint32_t count, FSAFileHandle handle, uint32_t flags) {
    DEBUG_FUNCTION_LINE_VERBOSE("handle: %08X buffer: %08X size: %08X", handle, buffer, size * count);
    return doForLayerFSA(
            [c = client, b = buffer, s = size, co = count, h = handle, f = flags]() -> FSError {
                return real_FSAReadFile(c, b, s, co, h, f);
            },
            [b = buffer, s = size, co = count, h = handle, f = flags](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                return layer->FSReadFileWrapper(b, s, co, h, f);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSAWriteFile, FSAClientHandle client, void *buffer, uint32_t size, uint32_t count, FSAFileHandle handle, uint32_t flags) {
    DEBUG_FUNCTION_LINE_VERBOSE("handle: %08X buffer: %08X size: %08X", handle, buffer, size * count);
    return doForLayerFSA(
            [c = client, b = buffer, s = size, co = count, h = handle, f = flags]() -> FSError {
                return real_FSAWriteFile(c, b, s, co, h, f);
            },
            [b = buffer, s = size, co = count, h = handle, f = flags](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                return layer->FSWriteFileWrapper((uint8_t *) b, s, co, h, f);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSAGetPosFile, FSAClientHandle client, FSAFileHandle handle, uint32_t *outPos) {
    DEBUG_FUNCTION_LINE_VERBOSE("handle: %08X", handle);
    return doForLayerFSA(
            [c = client, h = handle, o = outPos]() -> FSError {
                return real_FSAGetPosFile(c, h, o);
            },
            [h = handle, o = outPos](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                return layer->FSGetPosFileWrapper(h, o);
            },
            SYNC_RESULT_HANDLER_FSA);
}

DECL_FUNCTION(FSError, FSAIsEof, FSAClientHandle client, FSAFileHandle handle) {
    DEBUG_FUNCTION_LINE_VERBOSE("handle: %08X", handle);
    return doForLayerFSA(
            [c = client, h = handle]() -> FSError {
                return real_FSAIsEof(c, h);
            },
            [h = handle](std::unique_ptr<IFSWrapper> &layer) -> FSError {
                return layer->FSIsEofWrapper(h);
            },
            SYNC_RESULT_HANDLER_FSA);
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

DECL_FUNCTION(FSStatus, FSAWriteFileWithPos, FSAClientHandle client, uint8_t *buffer, uint32_t size, uint32_t count, uint32_t pos, FSFileHandle handle, uint32_t flags) {
    DEBUG_FUNCTION_LINE_ERR("NOT IMPLEMENTED. handle %08X size %08X", handle, size * count);
    return real_FSAWriteFileWithPos(client, buffer, size, count, pos, handle, flags);
}

function_replacement_data_t fsa_file_function_replacements[] = {
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
        REPLACE_FUNCTION(FSAWriteFile, LIBRARY_COREINIT, FSAWriteFile),
        REPLACE_FUNCTION(FSAGetPosFile, LIBRARY_COREINIT, FSAGetPosFile),
        REPLACE_FUNCTION(FSAIsEof, LIBRARY_COREINIT, FSAIsEof),

        REPLACE_FUNCTION(FSAFlushMultiQuota, LIBRARY_COREINIT, FSAFlushMultiQuota),
        REPLACE_FUNCTION(FSAFlushQuota, LIBRARY_COREINIT, FSAFlushQuota),
        REPLACE_FUNCTION(FSAChangeMode, LIBRARY_COREINIT, FSAChangeMode),
        REPLACE_FUNCTION(FSAOpenFileByStat, LIBRARY_COREINIT, FSAOpenFileByStat),
        REPLACE_FUNCTION(FSAAppendFile, LIBRARY_COREINIT, FSAAppendFile),
        REPLACE_FUNCTION(FSAAppendFileEx, LIBRARY_COREINIT, FSAAppendFileEx),
        REPLACE_FUNCTION(FSAWriteFileWithPos, LIBRARY_COREINIT, FSAWriteFileWithPos),
};

uint32_t fsa_file_function_replacements_size = sizeof(fsa_file_function_replacements) / sizeof(function_replacement_data_t);