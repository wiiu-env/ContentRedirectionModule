#include "FSWrapperReplaceSingleFile.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include "utils/utils.h"

#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/filesystem.h>

#include <filesystem>

FSWrapperReplaceSingleFile::FSWrapperReplaceSingleFile(const std::string &name,
                                                       const std::string &fileToReplace,
                                                       const std::string &replaceWithPath,
                                                       const bool fallbackOnError) : FSWrapper(name,
                                                                                               fileToReplace,
                                                                                               replaceWithPath,
                                                                                               fallbackOnError,
                                                                                               false) {
    auto strCpy = fileToReplace;
    std::ranges::replace(strCpy, '\\', '/');
    auto asPath        = std::filesystem::path(strCpy);
    mPathToReplace     = asPath.parent_path();
    mFileNameToReplace = asPath.filename();
    mFullPathToReplace = fileToReplace;

    strCpy = replaceWithPath;
    std::ranges::replace(strCpy, '\\', '/');
    asPath                = std::filesystem::path(strCpy);
    mReplacedWithPath     = asPath.parent_path();
    mReplacedWithFileName = asPath.filename();

    FSAInit();
    this->mClientHandle = FSAAddClient(nullptr);
    if (mClientHandle < 0) {
        DEBUG_FUNCTION_LINE_ERR("[%s] FSAClientHandle failed: %s (%d)", name.c_str(), FSAGetStatusStr(static_cast<FSError>(mClientHandle)), mClientHandle);
        mClientHandle = 0;
    }
}

FSWrapperReplaceSingleFile::~FSWrapperReplaceSingleFile() {
    if (mClientHandle) {
        if (const FSError res = FSADelClient(mClientHandle); res != FS_ERROR_OK) {
            DEBUG_FUNCTION_LINE_ERR("[%s] FSADelClient failed: %s (%d)", pName.c_str(), FSAGetStatusStr(res), res);
        }
        mClientHandle = 0;
    }
}

FSError FSWrapperReplaceSingleFile::FSOpenDirWrapper(const char *path,
                                                     FSADirectoryHandle *handle) {
    if (!IsDirPathToReplace(path)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    if (handle == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("[%s] handle was NULL", getName().c_str());
        return FS_ERROR_INVALID_PARAM;
    }
    if (const auto dirInfo = make_shared_nothrow<DirInfoExSingleFile>()) {
        dirInfo->handle = (reinterpret_cast<uint32_t>(dirInfo.get()) & 0x0FFFFFFF) | 0x30000000;
        *handle         = dirInfo->handle;
        addDirHandle(dirInfo);
        if (!isValidDirHandle(*handle)) {
            FSWrapper::FSCloseDirWrapper(*handle);
            DEBUG_FUNCTION_LINE_ERR("[%s] No valid dir handle %08X", getName().c_str(), *handle);
            return FS_ERROR_INVALID_DIRHANDLE;
        }
        if (const auto dirHandle = getDirExFromHandle(*handle); dirHandle != nullptr) {
            dirHandle->entryRead        = false;
            dirHandle->entryReadSuccess = false;
            dirHandle->directoryEntry   = {};
            dirHandle->realDirHandle    = 0;

            if (mClientHandle) {
                FSADirectoryHandle realHandle = 0;
                DEBUG_FUNCTION_LINE_VERBOSE("[%s] Call FSAOpenDir with %s for parent layer", getName().c_str(), path);
                if (const FSError err = FSAOpenDir(mClientHandle, path, &realHandle); err == FS_ERROR_OK) {
                    dirHandle->realDirHandle = realHandle;
                } else {
                    DEBUG_FUNCTION_LINE_ERR("[%s] Failed to open real dir %s. %s (%d)", getName().c_str(), path, FSAGetStatusStr(err), err);
                }
            } else {
                DEBUG_FUNCTION_LINE_ERR("[%s] clientHandle was null", getName().c_str());
            }
            OSMemoryBarrier();
        }
    } else {
        DEBUG_FUNCTION_LINE_ERR("[%s] Failed to alloc dir handle", getName().c_str());
        return FS_ERROR_MAX_DIRS;
    }
    return FS_ERROR_OK;
}

FSError FSWrapperReplaceSingleFile::FSReadDirWrapper(const FSADirectoryHandle handle, FSADirectoryEntry *entry) {
    if (!isValidDirHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    const auto dirHandle = getDirExFromHandle(handle);
    if (!dirHandle) {
        DEBUG_FUNCTION_LINE_ERR("[%s] No valid dir handle %08X", getName().c_str(), handle);
        return FS_ERROR_INVALID_DIRHANDLE;
    }
    FSError res = FS_ERROR_OK;
    do {
        if (!dirHandle->entryRead) {
            dirHandle->entryRead = true;
            const auto newPath   = GetNewPath(mFullPathToReplace);

            struct stat path_stat {};

            DEBUG_FUNCTION_LINE_VERBOSE("[%s] dir read of %s (%s)", getName().c_str(), mFullPathToReplace.c_str(), newPath.c_str());
            if (stat(newPath.c_str(), &path_stat) < 0) {
                DEBUG_FUNCTION_LINE_WARN("[%s] Path %s (%s) for dir read not found ", getName().c_str(), mFullPathToReplace.c_str(), newPath.c_str());
                dirHandle->entryReadSuccess = false;
                continue;
            }
            translate_stat(&path_stat, &dirHandle->directoryEntry.info);
            strncpy(dirHandle->directoryEntry.name, mFileNameToReplace.c_str(), sizeof(dirHandle->directoryEntry.name));
            memcpy(entry, &dirHandle->directoryEntry, sizeof(FSADirectoryEntry));

            dirHandle->entryReadSuccess = true;
            OSMemoryBarrier();
        } else {
            // Read the real directory.
            if (dirHandle->realDirHandle != 0) {
                if (mClientHandle) {
                    FSADirectoryEntry realDirEntry;
                    while (true) {
                        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Call FSReadDir with %08X for parent layer", getName().c_str(), dirHandle->realDirHandle);
                        if (const FSError readDirResult = FSAReadDir(mClientHandle, dirHandle->realDirHandle, &realDirEntry); readDirResult == FS_ERROR_OK) {
                            // Skip already read new files
                            if (dirHandle->entryRead && dirHandle->entryReadSuccess && strcmp(dirHandle->directoryEntry.name, realDirEntry.name) == 0) {
                                continue;
                            }

                            // But use new entries!
                            memcpy(entry, &realDirEntry, sizeof(FSADirectoryEntry));
                            res = FS_ERROR_OK;
                            break;
                        } else if (readDirResult == FS_ERROR_END_OF_DIR) {
                            res = FS_ERROR_END_OF_DIR;
                            break;
                        } else {
                            DEBUG_FUNCTION_LINE_ERR("[%s] real_FSReadDir returned an unexpected error: %s (%d)", getName().c_str(), FSAGetStatusStr(readDirResult), readDirResult);
                            res = FS_ERROR_END_OF_DIR;
                            break;
                        }
                    }
                } else {
                    DEBUG_FUNCTION_LINE_ERR("[%s] clientHandle was null", getName().c_str());
                }
            }
        }
        return res;
    } while (true);
}

FSError FSWrapperReplaceSingleFile::FSCloseDirWrapper(const FSADirectoryHandle handle) {
    if (!isValidDirHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    const auto dirHandle = getDirExFromHandle(handle);
    if (dirHandle->realDirHandle != 0) {
        if (mClientHandle) {
            DEBUG_FUNCTION_LINE_VERBOSE("[%s] Call FSCloseDir with %08X for parent layer", getName().c_str(), dirHandle->realDirHandle);
            auto realResult = FSACloseDir(mClientHandle, dirHandle->realDirHandle);
            if (realResult == FS_ERROR_OK) {
                dirHandle->realDirHandle = 0;
            } else {
                DEBUG_FUNCTION_LINE_ERR("[%s] Failed to close realDirHandle %d: res %s (%d)", getName().c_str(), dirHandle->realDirHandle, FSAGetStatusStr(realResult), realResult);
                return realResult == FS_ERROR_CANCELLED ? FS_ERROR_CANCELLED : FS_ERROR_MEDIA_ERROR;
            }
        } else {
            DEBUG_FUNCTION_LINE_ERR("[%s] clientHandle was null", getName().c_str());
        }
    } else {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] dirHandle->realDirHandle was 0", getName().c_str());
    }
    dirHandle->entryRead        = false;
    dirHandle->entryReadSuccess = false;

    OSMemoryBarrier();
    return FS_ERROR_OK;
}

FSError FSWrapperReplaceSingleFile::FSRewindDirWrapper(const FSADirectoryHandle handle) {
    if (!isValidDirHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    const auto dirHandle        = getDirExFromHandle(handle);
    dirHandle->entryRead        = false;
    dirHandle->entryReadSuccess = false;
    if (dirHandle->realDirHandle != 0) {
        if (mClientHandle) {
            DEBUG_FUNCTION_LINE_VERBOSE("[%s] Call FSARewindDir with %08X for parent layer", getName().c_str(), dirHandle->realDirHandle);
            if (const FSError err = FSARewindDir(mClientHandle, dirHandle->realDirHandle); err != FS_ERROR_OK) {
                DEBUG_FUNCTION_LINE_ERR("[%s] Failed to rewind dir for realDirHandle %08X. %s (%d)", getName().c_str(), dirHandle->realDirHandle, FSAGetStatusStr(err), err);
            }
        } else {
            DEBUG_FUNCTION_LINE_ERR("[%s] clientHandle was null", getName().c_str());
        }
    } else {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] dirHandle->realDirHandle was 0", getName().c_str());
    }
    OSMemoryBarrier();

    return FS_ERROR_OK;
}

bool FSWrapperReplaceSingleFile::SkipDeletedFilesInReadDir() {
    return false;
}

bool FSWrapperReplaceSingleFile::IsDirPathToReplace(const std::string_view &path) const {
    return starts_with_case_insensitive(path, mPathToReplace);
}

std::string FSWrapperReplaceSingleFile::GetNewPath(const std::string_view &path) const {
    auto pathCpy = std::string(path);
    SafeReplaceInString(pathCpy, this->mPathToReplace, this->mReplacedWithPath);
    SafeReplaceInString(pathCpy, this->mFileNameToReplace, this->mReplacedWithFileName);
    std::ranges::replace(pathCpy, '\\', '/');

    uint32_t length = pathCpy.size();

    //! clear path of double slashes
    for (uint32_t i = 1; i < length; ++i) {
        if (pathCpy[i - 1] == '/' && pathCpy[i] == '/') {
            pathCpy.erase(i, 1);
            i--;
            length--;
        }
    }

    DEBUG_FUNCTION_LINE_VERBOSE("[%s] Redirect %.*s -> %s", getName().c_str(), int(path.length()), path.data(), pathCpy.c_str());
    return pathCpy;
}

std::shared_ptr<DirInfoExSingleFile> FSWrapperReplaceSingleFile::getDirExFromHandle(FSADirectoryHandle handle) {
    auto dir = std::dynamic_pointer_cast<DirInfoExSingleFile>(getDirFromHandle(handle));

    if (!dir) {
        DEBUG_FUNCTION_LINE_ERR("[%s] dynamic_pointer_cast<DirInfoExSingleFile *>(%08X) failed", getName().c_str(), handle);
        OSFatal("ContentRedirectionModule: dynamic_pointer_cast<DirInfoExSingleFile *> failed");
    }
    return dir;
}