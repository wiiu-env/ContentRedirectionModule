#include "FSWrapper.h"
#include "FileUtils.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include <algorithm>
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/filesystem_fsa.h>
#include <cstdio>
#include <filesystem>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>

FSError FSWrapper::FSOpenDirWrapper(const char *path, FSDirectoryHandle *handle) {
    if (path == nullptr) {
        return FS_ERROR_INVALID_PARAM;
    }
    if (!IsPathToReplace(path)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    if (handle == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("[%s] handle was nullptr", getName().c_str());
        return FS_ERROR_INVALID_PARAM;
    }

    FSError result = FS_ERROR_OK;

    auto dirHandle = getNewDirHandle();
    if (dirHandle) {
        DIR *dir;
        auto newPath = GetNewPath(path);

        if ((dir = opendir(newPath.c_str()))) {
            dirHandle->dir    = dir;
            dirHandle->handle = (((uint32_t) dirHandle.get()) & 0x0FFFFFFF) | 0x30000000;
            *handle           = dirHandle->handle;

            dirHandle->path[0] = '\0';
            strncat(dirHandle->path, newPath.c_str(), sizeof(dirHandle->path) - 1);
            {
                std::lock_guard<std::mutex> lock(openDirsMutex);
                openDirs.push_back(dirHandle);
                OSMemoryBarrier();
            }
        } else {
            auto err = errno;
            if (err == ENOENT) {
                DEBUG_FUNCTION_LINE("[%s] Open dir %s (%s) failed. FS_ERROR_NOT_FOUND", getName().c_str(), path, newPath.c_str());
                return FS_ERROR_NOT_FOUND;
            }
            DEBUG_FUNCTION_LINE_ERR("[%s] Open dir %s (%s) failed. errno %d", getName().c_str(), path, newPath.c_str(), err);
            if (err == EACCES) {
                return FS_ERROR_PERMISSION_ERROR;
            } else if (err == ENOTDIR) {
                return FS_ERROR_NOT_DIR;
            } else if (err == ENFILE || err == EMFILE) {
                return FS_ERROR_MAX_DIRS;
            }
            return FS_ERROR_MEDIA_ERROR;
        }
    } else {
        DEBUG_FUNCTION_LINE_ERR("[%s] Failed to alloc dir handle", getName().c_str());
        result = FS_ERROR_MAX_DIRS;
    }
    return result;
}

FSError FSWrapper::FSReadDirWrapper(FSDirectoryHandle handle, FSDirectoryEntry *entry) {
    if (!isValidDirHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    auto dirHandle = getDirFromHandle(handle);

    DIR *dir = dirHandle->dir;

    FSError result = FS_ERROR_END_OF_DIR;
    DEBUG_FUNCTION_LINE_VERBOSE("[%s] readdir %08X (handle %08X)", getName().c_str(), dir, handle);
    do {
        errno                 = 0;
        struct dirent *entry_ = readdir(dir);

        if (entry_) {
            if (SkipDeletedFilesInReadDir() && starts_with_case_insensitive(entry_->d_name, deletePrefix)) {
                DEBUG_FUNCTION_LINE_ERR("Skip file file name %s because of the prefix", entry_->d_name);
                continue;
            }
            entry->name[0] = '\0';
            strncat(entry->name, entry_->d_name, sizeof(entry->name) - 1);
            entry->info.mode = (FSMode) FS_MODE_READ_OWNER;
            if (entry_->d_type == DT_DIR) {
                entry->info.flags = (FSStatFlags) ((uint32_t) FS_STAT_DIRECTORY);
                entry->info.size  = 0;
            } else {
                entry->info.flags = (FSStatFlags) 0;
                if (strcmp(entry_->d_name, ".") == 0 || strcmp(entry_->d_name, "..") == 0) {
                    entry->info.size = 0;
                } else {
#ifdef _DIRENT_HAVE_D_STAT
                    translate_stat(&entry_->d_stat, &entry->info);
#else
                    struct stat sb {};
                    auto path = string_format("%s/%s", dirHandle->path, entry_->d_name);
                    std::replace(path.begin(), path.end(), '\\', '/');

                    uint32_t length = path.size();

                    //! clear path of double slashes
                    for (uint32_t i = 1; i < length; ++i) {
                        if (path[i - 1] == '/' && path[i] == '/') {
                            path.erase(i, 1);
                            i--;
                            length--;
                        }
                    }

                    if (stat(path.c_str(), &sb) >= 0) {
                        translate_stat(&sb, &entry->info);
                    } else {
                        DEBUG_FUNCTION_LINE_ERR("[%s] Failed to stat file (%s) in read dir %08X (dir handle %08X)", getName().c_str(), path.c_str(), dir, handle);
                        result = FS_ERROR_MEDIA_ERROR;
                        break;
                    }
#endif
                }
            }
            result = FS_ERROR_OK;
        } else {
            auto err = errno;
            if (err != 0) {
                DEBUG_FUNCTION_LINE_ERR("[%s] Failed to read dir %08X (handle %08X). errno %d (%s)", getName().c_str(), dir, handle, err, strerror(err));
                result = FS_ERROR_MEDIA_ERROR;
            }
        }
        break;
    } while (true);
    return result;
}

FSError FSWrapper::FSCloseDirWrapper(FSDirectoryHandle handle) {
    if (!isValidDirHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    auto dirHandle = getDirFromHandle(handle);

    DIR *dir = dirHandle->dir;

    FSError result = FS_ERROR_OK;
    DEBUG_FUNCTION_LINE_VERBOSE("[%s] closedir %08X (handle %08X)", getName().c_str(), dir, handle);
    if (closedir(dir) < 0) {
        DEBUG_FUNCTION_LINE_ERR("[%s] Failed to close dir %08X (handle %08X)", getName().c_str(), dir, handle);
        result = FS_ERROR_MEDIA_ERROR;
    }
    dirHandle->dir = nullptr;

    return result;
}

FSError FSWrapper::FSRewindDirWrapper(FSDirectoryHandle handle) {
    if (!isValidDirHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    auto dirHandle = getDirFromHandle(handle);

    DIR *dir = dirHandle->dir;

    DEBUG_FUNCTION_LINE_VERBOSE("[%s] rewinddir %08X (handle %08X)", getName().c_str(), dir, handle);
    rewinddir(dir);

    return FS_ERROR_OK;
}

FSError FSWrapper::FSMakeDirWrapper(const char *path) {
    if (!IsPathToReplace(path)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    if (!pIsWriteable) {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Tried to create dir %s but layer is not writeable", getName().c_str(), path);
        return FS_ERROR_PERMISSION_ERROR;
    }
    auto newPath = GetNewPath(path);

    auto res = mkdir(newPath.c_str(), 0000660);
    if (res < 0) {
        auto err = errno;
        if (err == EACCES) {
            return FS_ERROR_PERMISSION_ERROR;
        } else if (err == EEXIST) {
            return FS_ERROR_ALREADY_EXISTS;
        } else if (err == ENOTDIR) {
            return FS_ERROR_NOT_DIR;
        } else if (err == ENOENT) {
            return FS_ERROR_NOT_FOUND;
        }
        return FS_ERROR_MEDIA_ERROR;
    }
    return FS_ERROR_OK;
}

FSError FSWrapper::FSOpenFileWrapper(const char *path, const char *mode, FSFileHandle *handle) {
    if (!IsPathToReplace(path)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    if (path == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("[%s] path was nullptr", getName().c_str());
        return FS_ERROR_INVALID_PARAM;
    }

    if (mode == nullptr || handle == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("[%s] mode or handle was nullptr", getName().c_str());
        return FS_ERROR_INVALID_PARAM;
    }

    auto newPath = GetNewPath(path);

    if (pCheckIfDeleted && CheckFileShouldBeIgnored(newPath)) {
        return static_cast<FSError>((FS_ERROR_NOT_FOUND & FS_ERROR_REAL_MASK) | FS_ERROR_FORCE_NO_FALLBACK);
    }

    auto result = FS_ERROR_OK;
    int _mode;
    // Map flags to open modes
    if (!IsFileModeAllowed(mode)) {
        OSReport("## WARN ## [%s] Given mode is not allowed %s", getName().c_str(), mode);
        DEBUG_FUNCTION_LINE("[%s] Given mode is not allowed %s", getName().c_str(), mode);
        return FS_ERROR_ACCESS_ERROR;
    }

    if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) {
        _mode = O_RDONLY;
    } else if (strcmp(mode, "r+") == 0) {
        _mode = O_RDWR;
    } else if (strcmp(mode, "w") == 0 || strcmp(mode, "wb") == 0) {
        _mode = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (strcmp(mode, "w+") == 0) {
        _mode = O_RDWR | O_CREAT | O_TRUNC;
    } else if (strcmp(mode, "a") == 0) {
        _mode = O_WRONLY | O_CREAT | O_APPEND;
    } else if (strcmp(mode, "a+") == 0) {
        _mode = O_RDWR | O_CREAT | O_APPEND;
    } else {
        DEBUG_FUNCTION_LINE_ERR("[%s] mode \"%s\" was allowed but is unsupported", getName().c_str(), mode);
        return FS_ERROR_ACCESS_ERROR;
    }

    DEBUG_FUNCTION_LINE_VERBOSE("[%s] Open %s (as %s) mode %s,", getName().c_str(), path, newPath.c_str(), mode);
    int32_t fd = open(newPath.c_str(), _mode);
    if (fd >= 0) {
        auto fileHandle = getNewFileHandle();
        if (fileHandle) {
            std::lock_guard<std::mutex> lock(openFilesMutex);

            fileHandle->handle = (((uint32_t) fileHandle.get()) & 0x0FFFFFFF) | 0x30000000;
            *handle            = fileHandle->handle;
            fileHandle->fd     = fd;

            DEBUG_FUNCTION_LINE_VERBOSE("[%s] Opened %s (as %s) mode %s (%08X), fd %d (%08X)", getName().c_str(), path, newPath.c_str(), mode, _mode, fd, fileHandle->handle);

            openFiles.push_back(fileHandle);

            OSMemoryBarrier();
        } else {
            close(fd);
            DEBUG_FUNCTION_LINE_ERR("[%s] Failed to alloc new fileHandle", getName().c_str());
            result = FS_ERROR_MAX_FILES;
        }
    } else {
        auto err = errno;
        if (err == ENOENT) {
            DEBUG_FUNCTION_LINE_VERBOSE("[%s] File not found %s (%s)", getName().c_str(), path, newPath.c_str());
            result = FS_ERROR_NOT_FOUND;
        } else {
            DEBUG_FUNCTION_LINE("[%s] Open file %s (%s) failed. errno %d ", getName().c_str(), path, newPath.c_str(), err);
            if (err == EACCES) {
                return FS_ERROR_PERMISSION_ERROR;
            } else if (err == ENOENT) {
                result = FS_ERROR_NOT_FOUND;
            } else if (err == EEXIST) {
                result = FS_ERROR_ALREADY_EXISTS;
            } else if (err == EISDIR) {
                result = FS_ERROR_NOT_FILE;
            } else if (err == ENOTDIR) {
                result = FS_ERROR_NOT_DIR;
            } else if (err == ENFILE || err == EMFILE) {
                result = FS_ERROR_MAX_FILES;
            } else {
                result = FS_ERROR_MEDIA_ERROR;
            }
        }
    }

    return result;
}

FSError FSWrapper::FSCloseFileWrapper(FSFileHandle handle) {
    if (!isValidFileHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    auto fileHandle = getFileFromHandle(handle);

    int real_fd = fileHandle->fd;

    FSError result = FS_ERROR_OK;
    DEBUG_FUNCTION_LINE_VERBOSE("[%s] Close %d (handle %08X)", getName().c_str(), real_fd, handle);
    if (close(real_fd) != 0) {
        DEBUG_FUNCTION_LINE_ERR("[%s] Failed to close %d (handle %08X) ", getName().c_str(), real_fd, handle);
        result = FS_ERROR_MEDIA_ERROR;
    }
    fileHandle->fd = -1;
    return result;
}

bool FSWrapper::CheckFileShouldBeIgnored(std::string &path) {
    auto asPath = std::filesystem::path(path);

    if (starts_with_case_insensitive(asPath.filename().c_str(), deletePrefix)) {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Ignore %s, filename starts with %s", getName().c_str(), path.c_str(), deletePrefix.c_str());
        return true;
    }

    auto newDelPath = asPath.replace_filename(deletePrefix + asPath.filename().c_str());
    struct stat buf {};
    if (stat(newDelPath.c_str(), &buf) == 0) {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Ignore %s, file %s exists", getName().c_str(), path.c_str(), newDelPath.c_str());
        return true;
    }
    return false;
}

FSError FSWrapper::FSGetStatWrapper(const char *path, FSStat *stats) {
    if (path == nullptr || stats == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("[%s] path was or stats nullptr", getName().c_str());
        return FS_ERROR_INVALID_PARAM;
    }

    if (!IsPathToReplace(path)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    auto newPath = GetNewPath(path);

    struct stat path_stat {};

    if (pCheckIfDeleted && CheckFileShouldBeIgnored(newPath)) {
        return static_cast<FSError>((FS_ERROR_NOT_FOUND & FS_ERROR_REAL_MASK) | FS_ERROR_FORCE_NO_FALLBACK);
    }

    FSError result = FS_ERROR_OK;

    DEBUG_FUNCTION_LINE_VERBOSE("[%s] stat of %s (%s)", getName().c_str(), path, newPath.c_str());
    if (stat(newPath.c_str(), &path_stat) < 0) {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Path %s (%s) not found ", getName().c_str(), path, newPath.c_str());
        result = FS_ERROR_NOT_FOUND;
    } else {
        translate_stat(&path_stat, stats);
    }
    return result;
}

FSError FSWrapper::FSGetStatFileWrapper(FSFileHandle handle, FSStat *stats) {
    if (!isValidFileHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    auto fileHandle = getFileFromHandle(handle);

    int real_fd = fileHandle->fd;

    struct stat path_stat {};

    FSError result = FS_ERROR_OK;
    DEBUG_FUNCTION_LINE_VERBOSE("[%s] fstat of fd %d (FSFileHandle %08X)", getName().c_str(), real_fd, handle);
    if (fstat(real_fd, &path_stat) < 0) {
        DEBUG_FUNCTION_LINE_ERR("[%s] fstat of fd %d (FSFileHandle %08X) failed", getName().c_str(), real_fd, handle);
        result = FS_ERROR_MEDIA_ERROR;
    } else {
        translate_stat(&path_stat, stats);
    }

    return result;
}

FSError FSWrapper::FSReadFileWrapper(void *buffer, uint32_t size, uint32_t count, FSFileHandle handle, [[maybe_unused]] uint32_t unk1) {
    if (!isValidFileHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    if (size * count == 0) {
        return FS_ERROR_OK;
    }

    if (buffer == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("[%s] buffer is null but size * count is not 0 (It's: %d)", getName().c_str(), size * count);
        return FS_ERROR_INVALID_BUFFER;
    }

    auto fileHandle = getFileFromHandle(handle);
    int real_fd     = fileHandle->fd;

    DEBUG_FUNCTION_LINE_VERBOSE("[%s] Read %u bytes of fd %08X (FSFileHandle %08X) to buffer %08X", getName().c_str(), size * count, real_fd, handle, buffer);
    int64_t read = readIntoBuffer(real_fd, buffer, size, count);

    FSError result;
    if (read < 0) {
        DEBUG_FUNCTION_LINE_ERR("[%s] Read %u bytes of fd %d (FSFileHandle %08X) failed", getName().c_str(), size * count, real_fd, handle);
        auto err = errno;
        if (err == EBADF || err == EROFS) {
            return FS_ERROR_ACCESS_ERROR;
        }
        result = FS_ERROR_MEDIA_ERROR;
    } else {
        result = static_cast<FSError>(((uint32_t) read) / size);
    }

    return result;
}

FSError FSWrapper::FSReadFileWithPosWrapper(void *buffer, uint32_t size, uint32_t count, uint32_t pos, FSFileHandle handle, int32_t unk1) {
    if (!isValidFileHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    DEBUG_FUNCTION_LINE_VERBOSE("[%s] Read from with position.", getName().c_str());
    FSError result;
    if ((result = this->FSSetPosFileWrapper(handle, pos)) != FS_ERROR_OK) {
        return result;
    }

    result = this->FSReadFileWrapper(buffer, size, count, handle, unk1);

    return result;
}

FSError FSWrapper::FSSetPosFileWrapper(FSFileHandle handle, uint32_t pos) {
    if (!isValidFileHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    auto fileHandle = getFileFromHandle(handle);

    FSError result = FS_ERROR_OK;

    int real_fd = fileHandle->fd;

    DEBUG_FUNCTION_LINE_VERBOSE("[%s] lseek fd %d (FSFileHandle %08X) SEEK_SET to position %08X", getName().c_str(), real_fd, handle, pos);
    off_t newPos;
    if ((newPos = lseek(real_fd, (off_t) pos, SEEK_SET)) != pos) {
        DEBUG_FUNCTION_LINE_ERR("[%s] lseek fd %d (FSFileHandle %08X) to position %08X failed", getName().c_str(), real_fd, handle, pos);
        if (newPos < 0) {
            // TODO: read errno
        }
        result = FS_ERROR_MEDIA_ERROR;
    } else {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] pos set to %u for fd %d (FSFileHandle %08X)", getName().c_str(), pos, real_fd, handle);
    }

    return result;
}

FSError FSWrapper::FSGetPosFileWrapper(FSFileHandle handle, uint32_t *pos) {
    if (!isValidFileHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    auto fileHandle = getFileFromHandle(handle);

    FSError result = FS_ERROR_OK;

    int real_fd = fileHandle->fd;

    DEBUG_FUNCTION_LINE_VERBOSE("[%s] lseek fd %08X (FSFileHandle %08X) to get current position for truncation", getName().c_str(), real_fd, handle);
    off_t currentPos = lseek(real_fd, (off_t) 0, SEEK_CUR);
    if (currentPos == -1) {
        DEBUG_FUNCTION_LINE_ERR("[%s] Failed to get current position (res: %lld) of fd (handle %08X) to check EoF", getName().c_str(), currentPos, real_fd, handle);
        result = FS_ERROR_MEDIA_ERROR;
    } else {
        *pos = currentPos;
    }
    return result;
}

FSError FSWrapper::FSIsEofWrapper(FSFileHandle handle) {
    if (!isValidFileHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    auto fileHandle = getFileFromHandle(handle);

    FSError result;

    int real_fd = fileHandle->fd;

    DEBUG_FUNCTION_LINE_VERBOSE("[%s] lseek fd %08X (FSFileHandle %08X) to get current position for EOF detection", getName().c_str(), real_fd, handle);
    off_t currentPos = lseek(real_fd, (off_t) 0, SEEK_CUR);
    DEBUG_FUNCTION_LINE_VERBOSE("[%s] lseek fd %08X (FSFileHandle %08X) to get end position for EOF detection", getName().c_str(), real_fd, handle);
    off_t endPos = lseek(real_fd, (off_t) 0, SEEK_END);

    if (currentPos == -1 || endPos == -1) {
        // TODO: check errno
        DEBUG_FUNCTION_LINE_ERR("[%s] Failed to get current position (res: %lld) or endPos (res: %lld) of fd (handle %08X) to check EoF", getName().c_str(), currentPos, endPos, real_fd, handle);
        result = FS_ERROR_MEDIA_ERROR;
    } else if (currentPos == endPos) {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] FSIsEof END for %d\n", getName().c_str(), real_fd);
        result = FS_ERROR_END_OF_FILE;
    } else {
        lseek(real_fd, currentPos, SEEK_CUR);
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] FSIsEof OK for %d\n", getName().c_str(), real_fd);
        result = FS_ERROR_OK;
    }

    return result;
}

FSError FSWrapper::FSTruncateFileWrapper(FSFileHandle handle) {
    if (!isValidFileHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    if (!pIsWriteable) {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Tried to truncate fd %d (handle %08X) but layer is not writeable", getName().c_str(), getFileFromHandle(handle)->fd, handle);
        return FS_ERROR_ACCESS_ERROR;
    }

    auto fileHandle = getFileFromHandle(handle);

    FSError result = FS_ERROR_OK;

    int real_fd = fileHandle->fd;

    DEBUG_FUNCTION_LINE_VERBOSE("[%s] lseek fd %08X (FSFileHandle %08X) to get current position for truncation", getName().c_str(), real_fd, handle);
    off_t currentPos = lseek(real_fd, (off_t) 0, SEEK_CUR);
    if (currentPos == -1) {
        // TODO check errno
        DEBUG_FUNCTION_LINE_ERR("[%s] Failed to get current position of fd (handle %08X) to truncate file", getName().c_str(), real_fd, handle);
        result = FS_ERROR_MEDIA_ERROR;
    } else {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Truncate fd %08X (FSFileHandle %08X) to %lld bytes ", getName().c_str(), real_fd, handle, currentPos);
        if (ftruncate(real_fd, currentPos) < 0) {
            DEBUG_FUNCTION_LINE_ERR("[%s] ftruncate failed for fd %08X (FSFileHandle %08X) errno %d", getName().c_str(), real_fd, handle, errno);
            result = FS_ERROR_MEDIA_ERROR;
        }
    }

    return result;
}

FSError FSWrapper::FSWriteFileWrapper(const uint8_t *buffer, uint32_t size, uint32_t count, FSFileHandle handle, [[maybe_unused]] uint32_t unk1) {
    if (!isValidFileHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    if (!pIsWriteable) {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Tried to write to fd %d (handle %08X) but layer is not writeable", getName().c_str(), getFileFromHandle(handle)->fd, handle);
        return FS_ERROR_ACCESS_ERROR;
    }
    auto fileHandle = getFileFromHandle(handle);

    FSError result;

    int real_fd = fileHandle->fd;

    DEBUG_FUNCTION_LINE_VERBOSE("[%s] Write %u bytes to fd %08X (FSFileHandle %08X) from buffer %08X", getName().c_str(), count * size, real_fd, handle, buffer);
    auto writeRes = writeFromBuffer(real_fd, buffer, size, count);
    if (writeRes < 0) {
        auto err = errno;
        DEBUG_FUNCTION_LINE_ERR("[%s] Write failed %u bytes to fd %08X (FSFileHandle %08X) from buffer %08X errno %d", getName().c_str(), count * size, real_fd, handle, buffer, err);
        if (err == EFBIG) {
            result = FS_ERROR_FILE_TOO_BIG;
        } else if (err == EACCES) {
            result = FS_ERROR_ACCESS_ERROR;
        } else {
            result = FS_ERROR_MEDIA_ERROR;
        }
    } else {
        result = static_cast<FSError>(((uint32_t) writeRes) / size);
    }

    return result;
}

FSError FSWrapper::FSRemoveWrapper(const char *path) {
    if (!IsPathToReplace(path)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    if (!pIsWriteable) {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Tried to remove %s but layer is not writeable", getName().c_str(), path);
        return FS_ERROR_PERMISSION_ERROR;
    }
    auto newPath = GetNewPath(path);
    DEBUG_FUNCTION_LINE_VERBOSE("[%s] Remove %s (%s)", getName().c_str(), path, newPath.c_str());
    if (remove(newPath.c_str()) < 0) {
        auto err = errno;
        DEBUG_FUNCTION_LINE_ERR("[%s] Rename failed %s (%s) errno %d", getName().c_str(), path, newPath.c_str(), err);
        if (err == ENOTDIR) {
            return FS_ERROR_NOT_DIR;
        } else if (err == EACCES) {
            return FS_ERROR_ACCESS_ERROR;
        } else if (err == EISDIR) {
            return FS_ERROR_NOT_FILE;
        } else if (err == EPERM) {
            return FS_ERROR_PERMISSION_ERROR;
        }
        return FS_ERROR_MEDIA_ERROR;
    }
    return FS_ERROR_OK;
}

FSError FSWrapper::FSRenameWrapper(const char *oldPath, const char *newPath) {
    if (!IsPathToReplace(oldPath) || !IsPathToReplace(newPath)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    if (!pIsWriteable) {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Tried to rename %s to %s but layer is not writeable", getName().c_str(), oldPath, newPath);
        return FS_ERROR_PERMISSION_ERROR;
    }
    auto oldPathRedirect = GetNewPath(oldPath);
    auto newPathRedirect = GetNewPath(newPath);
    DEBUG_FUNCTION_LINE_VERBOSE("[%s] Rename %s (%s) -> %s (%s)", getName().c_str(), oldPath, oldPathRedirect.c_str(), newPath, newPathRedirect.c_str());
    if (rename(oldPathRedirect.c_str(), newPathRedirect.c_str()) < 0) {
        auto err = errno;
        DEBUG_FUNCTION_LINE_ERR("[%s] Rename failed %s (%s) -> %s (%s). errno %d", getName().c_str(), oldPath, oldPathRedirect.c_str(), newPath, newPathRedirect.c_str(), err);
        if (err == ENOTDIR) {
            return FS_ERROR_NOT_DIR;
        } else if (err == EACCES) {
            return FS_ERROR_ACCESS_ERROR;
        } else if (err == EISDIR) {
            return FS_ERROR_NOT_FILE;
        } else if (err == EPERM) {
            return FS_ERROR_PERMISSION_ERROR;
        }
        return FS_ERROR_MEDIA_ERROR;
    }
    return FS_ERROR_OK;
}

FSError FSWrapper::FSFlushFileWrapper(FSFileHandle handle) {
    if (!isValidFileHandle(handle)) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }
    if (!pIsWriteable) {
        DEBUG_FUNCTION_LINE_VERBOSE("[%s] Tried to fsync fd %d (handle %08X)) but layer is not writeable", getName().c_str(), getFileFromHandle(handle)->fd, handle);
        return FS_ERROR_ACCESS_ERROR;
    }

    auto fileHandle = getFileFromHandle(handle);
    int real_fd     = fileHandle->fd;

    DEBUG_FUNCTION_LINE_VERBOSE("[%s] fsync fd %08X (FSFileHandle %08X)", real_fd, handle);
    FSError result = FS_ERROR_OK;
    if (fsync(real_fd) < 0) {
        DEBUG_FUNCTION_LINE_ERR("[%s] fsync failed for fd %08X (FSFileHandle %08X)", getName().c_str(), real_fd, handle);
        auto err = errno;
        if (err == EBADF) {
            result = FS_ERROR_INVALID_FILEHANDLE;
        } else {
            result = FS_ERROR_MEDIA_ERROR;
        }
    }
    return result;
}

bool FSWrapper::IsFileModeAllowed(const char *mode) {
    if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) {
        return true;
    }

    if (pIsWriteable && (strcmp(mode, "r+") == 0 ||
                         strcmp(mode, "w") == 0 ||
                         strcmp(mode, "wb") == 0 ||
                         strcmp(mode, "w+") == 0 ||
                         strcmp(mode, "a") == 0 ||
                         strcmp(mode, "a+") == 0)) {
        return true;
    }

    return false;
}


bool FSWrapper::IsPathToReplace(const std::string_view &path) {
    return starts_with_case_insensitive(path, pPathToReplace);
}

std::string FSWrapper::GetNewPath(const std::string_view &path) {
    auto subStr = path.substr(this->pPathToReplace.length());
    auto res    = string_format("%s%.*s", this->pReplacePathWith.c_str(), int(subStr.length()), subStr.data());

    std::replace(res.begin(), res.end(), '\\', '/');

    uint32_t length = res.size();

    //! clear path of double slashes
    for (uint32_t i = 1; i < length; ++i) {
        if (res[i - 1] == '/' && res[i] == '/') {
            res.erase(i, 1);
            i--;
            length--;
        }
    }

    DEBUG_FUNCTION_LINE_VERBOSE("[%s] Redirect %.*s -> %s", getName().c_str(), int(path.length()), path.data(), res.c_str());
    return res;
}

bool FSWrapper::isValidFileHandle(FSFileHandle handle) {
    std::lock_guard<std::mutex> lock(openFilesMutex);
    return std::ranges::any_of(openFiles, [handle](auto &cur) { return cur->handle == handle; });
}

bool FSWrapper::isValidDirHandle(FSDirectoryHandle handle) {
    std::lock_guard<std::mutex> lock(openDirsMutex);
    return std::ranges::any_of(openDirs, [handle](auto &cur) { return cur->handle == handle; });
}

std::shared_ptr<FileInfo> FSWrapper::getNewFileHandle() {
    return make_shared_nothrow<FileInfo>();
}

std::shared_ptr<DirInfo> FSWrapper::getNewDirHandle() {
    return make_shared_nothrow<DirInfo>();
}

std::shared_ptr<FileInfo> FSWrapper::getFileFromHandle(FSFileHandle handle) {
    std::lock_guard<std::mutex> lock(openFilesMutex);
    for (auto &file : openFiles) {
        if (file->handle == handle) {
            return file;
        }
    }
    DEBUG_FUNCTION_LINE_ERR("[%s] FileInfo for handle %08X was not found. isValidFileHandle check missing?", getName().c_str(), handle);
    OSFatal("ContentRedirectionModule: Failed to find file handle");
    return nullptr;
}

std::shared_ptr<DirInfo> FSWrapper::getDirFromHandle(FSDirectoryHandle handle) {
    std::lock_guard<std::mutex> lock(openDirsMutex);
    for (auto &dir : openDirs) {
        if (dir->handle == handle) {
            return dir;
        }
    }
    DEBUG_FUNCTION_LINE_ERR("[%s] DirInfo for handle %08X was not found. isValidDirHandle check missing?", getName().c_str(), handle);
    OSFatal("ContentRedirectionModule: Failed to find dir handle");
    return nullptr;
}

void FSWrapper::deleteDirHandle(FSDirectoryHandle handle) {
    if (!remove_locked_first_if(openDirsMutex, openDirs, [handle](auto &cur) { return (FSFileHandle) cur->handle == handle; })) {
        DEBUG_FUNCTION_LINE_ERR("[%s] Delete failed because the handle %08X was not found", getName().c_str(), handle);
    }
}

void FSWrapper::deleteFileHandle(FSFileHandle handle) {
    if (!remove_locked_first_if(openFilesMutex, openFiles, [handle](auto &cur) { return (FSFileHandle) cur->handle == handle; })) {
        DEBUG_FUNCTION_LINE_ERR("[%s] Delete failed because the handle %08X was not found", getName().c_str(), handle);
    }
}

bool FSWrapper::SkipDeletedFilesInReadDir() {
    return true;
}
