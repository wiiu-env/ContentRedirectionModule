#include "FSWrapperMergeDirsWithParent.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/filesystem.h>
#include <filesystem>

FSError FSWrapperMergeDirsWithParent::FSOpenDirWrapper(const char *path,
                                                       FSADirectoryHandle *handle) {
    if (handle == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("[%s] handle was NULL", getName().c_str());
        return FS_ERROR_INVALID_PARAM;
    }

    auto res = FSWrapper::FSOpenDirWrapper(path, handle);
    if (res == FS_ERROR_OK) {
        if (!isValidDirHandle(*handle)) {
            FSWrapper::FSCloseDirWrapper(*handle);
            DEBUG_FUNCTION_LINE_ERR("[%s] No valid dir handle %08X", getName().c_str(), *handle);
            return FS_ERROR_INVALID_DIRHANDLE;
        }
        auto dirHandle = getDirExFromHandle(*handle);
        if (dirHandle != nullptr) {
            dirHandle->readResultCapacity        = 0;
            dirHandle->readResultNumberOfEntries = 0;
            dirHandle->realDirHandle             = 0;

            if (clientHandle) {
                FSADirectoryHandle realHandle = 0;
                DEBUG_FUNCTION_LINE_ERR("[%s] Call FSAOpenDir with %s for parent layer", getName().c_str(), path);
                // Call FSOpen with "this" as errorFlag call FSOpen for "parent" layers only.
                if (FSAOpenDir(clientHandle, path, &realHandle) == FS_ERROR_OK) {
                    dirHandle->realDirHandle = realHandle;
                } else {
                    DEBUG_FUNCTION_LINE_VERBOSE("[%s] Failed to open real dir %s", getName().c_str(), path);
                }
            } else {
                DEBUG_FUNCTION_LINE_ERR("[%s] clientHandle was null", getName().c_str());
            }
            OSMemoryBarrier();
        }
    }
    return res;
}

bool FSWrapperMergeDirsWithParent::SkipDeletedFilesInReadDir() {
    return false;
}

FSError FSWrapperMergeDirsWithParent::FSReadDirWrapper(FSADirectoryHandle handle, FSADirectoryEntry *entry) {
    do {
        auto res = FSWrapper::FSReadDirWrapper(handle, entry);
        if (res == FS_ERROR_OK || res == FS_ERROR_END_OF_DIR) {
            if (!isValidDirHandle(handle)) {
                DEBUG_FUNCTION_LINE_ERR("[%s] No valid dir handle %08X", getName().c_str(), handle);
                return FS_ERROR_INVALID_DIRHANDLE;
            }
            auto dirHandle = getDirExFromHandle(handle);
            if (res == FS_ERROR_OK) {
                if (dirHandle->readResultCapacity == 0) {
                    dirHandle->readResult = (FSDirectoryEntryEx *) malloc(sizeof(FSDirectoryEntryEx));
                    if (dirHandle->readResult == nullptr) {
                        DEBUG_FUNCTION_LINE_ERR("[%s] Failed to alloc memory for %08X (handle %08X)", getName().c_str(), dirHandle.get(), handle);
                        OSFatal("ContentRedirectionModule: Failed to alloc memory for read result");
                    }
                    dirHandle->readResultCapacity = 1;
                }

                if (dirHandle->readResultNumberOfEntries >= dirHandle->readResultCapacity) {
                    auto newCapacity              = dirHandle->readResultCapacity * 2;
                    dirHandle->readResult         = (FSDirectoryEntryEx *) realloc(dirHandle->readResult, newCapacity * sizeof(FSDirectoryEntryEx));
                    dirHandle->readResultCapacity = newCapacity;
                    if (dirHandle->readResult == nullptr) {
                        DEBUG_FUNCTION_LINE_ERR("[%s] Failed to realloc memory for %08X (handle %08X)", getName().c_str(), dirHandle.get(), handle);
                        OSFatal("ContentRedirectionModule: Failed to alloc memory for read result");
                    }
                }

                memcpy(&dirHandle->readResult[dirHandle->readResultNumberOfEntries].realEntry, entry, sizeof(FSADirectoryEntry));
                dirHandle->readResultNumberOfEntries++;

                /**
                 * Read the next entry if this entry starts with deletePrefix. We keep the entry but mark it as deleted.
                 */
                if (std::string_view(entry->name).starts_with(deletePrefix)) {
                    dirHandle->readResult[dirHandle->readResultNumberOfEntries].isMarkedAsDeleted = true;

                    OSMemoryBarrier();
                    continue;
                }

                OSMemoryBarrier();

            } else if (res == FS_ERROR_END_OF_DIR) {
                // Read the real directory.
                if (dirHandle->realDirHandle != 0) {
                    if (clientHandle) {
                        FSADirectoryEntry realDirEntry;
                        FSError readDirResult;
                        while (true) {
                            DEBUG_FUNCTION_LINE_ERR("[%s] Call FSReadDir with %08X for parent layer", getName().c_str(), dirHandle->realDirHandle);
                            readDirResult = FSAReadDir(clientHandle, dirHandle->realDirHandle, &realDirEntry);
                            if (readDirResult == FS_ERROR_OK) {
                                bool found       = false;
                                auto nameDeleted = deletePrefix + realDirEntry.name;
                                for (int i = 0; i < dirHandle->readResultNumberOfEntries; i++) {
                                    auto curResult = &dirHandle->readResult[i];

                                    // Don't return files that are "deleted"
                                    if (strcmp(curResult->realEntry.name, nameDeleted.c_str()) == 0) {
                                        found = true;
                                        break;
                                    }
                                    // Check if this is a new result
                                    if (strcmp(curResult->realEntry.name, realDirEntry.name) == 0 && !curResult->isMarkedAsDeleted) {
                                        found = true;
                                        break;
                                    }
                                }
                                // If it's new we can use it :)
                                if (!found) {
                                    memcpy(entry, &realDirEntry, sizeof(FSADirectoryEntry));
                                    res = FS_ERROR_OK;
                                    break;
                                }
                            } else if (readDirResult == FS_ERROR_END_OF_DIR) {
                                res = FS_ERROR_END_OF_DIR;
                                break;
                            } else {
                                DEBUG_FUNCTION_LINE_ERR("[%s] real_FSReadDir returned an unexpected error: %08X", getName().c_str(), readDirResult);
                                res = FS_ERROR_END_OF_DIR;
                                break;
                            }
                        }
                    } else {
                        DEBUG_FUNCTION_LINE_ERR("[%s] clientHandle was null", getName().c_str());
                    }
                }
            } else {
                DEBUG_FUNCTION_LINE_ERR("[%s] Unexpected result %d", getName().c_str(), res);
            }
        }
        return res;
    } while (true);
}

FSError FSWrapperMergeDirsWithParent::FSCloseDirWrapper(FSADirectoryHandle handle) {
    auto res = FSWrapper::FSCloseDirWrapper(handle);

    if (res == FS_ERROR_OK) {
        if (!isValidDirHandle(handle)) {
            DEBUG_FUNCTION_LINE_ERR("[%s] No valid dir handle %08X", getName().c_str(), handle);
            return FS_ERROR_INVALID_DIRHANDLE;
        }
        auto dirHandle = getDirExFromHandle(handle);
        if (dirHandle->realDirHandle != 0) {
            if (clientHandle) {
                DEBUG_FUNCTION_LINE_ERR("[%s] Call FSCloseDir with %08X for parent layer", getName().c_str(), dirHandle->realDirHandle);
                auto realResult = FSACloseDir(clientHandle, dirHandle->realDirHandle);
                if (realResult == FS_ERROR_OK) {
                    dirHandle->realDirHandle = 0;
                } else {
                    DEBUG_FUNCTION_LINE_ERR("[%s] Failed to close realDirHandle %d: res %d", getName().c_str(), dirHandle->realDirHandle, -1);
                    return realResult == FS_ERROR_CANCELLED ? FS_ERROR_CANCELLED : FS_ERROR_MEDIA_ERROR;
                }
            } else {
                DEBUG_FUNCTION_LINE_ERR("[%s] clientHandle was null", getName().c_str());
            }
        }

        if (dirHandle->readResult != nullptr) {
            free(dirHandle->readResult);
            dirHandle->readResult                = nullptr;
            dirHandle->readResultCapacity        = 0;
            dirHandle->readResultNumberOfEntries = 0;
        }

        OSMemoryBarrier();
    }
    return res;
}

FSError FSWrapperMergeDirsWithParent::FSRewindDirWrapper(FSADirectoryHandle handle) {
    auto res = FSWrapper::FSRewindDirWrapper(handle);
    if (res == FS_ERROR_OK) {
        if (!isValidDirHandle(handle)) {
            DEBUG_FUNCTION_LINE_ERR("[%s] No valid dir handle %08X", getName().c_str(), handle);
            return FS_ERROR_INVALID_DIRHANDLE;
        }
        auto dirHandle = getDirExFromHandle(handle);
        if (dirHandle->readResult != nullptr) {
            dirHandle->readResultNumberOfEntries = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
            memset(dirHandle->readResult, 0, sizeof(FSDirectoryEntryEx) * dirHandle->readResultCapacity);
#pragma GCC diagnostic pop
        }

        if (dirHandle->realDirHandle != 0) {
            if (clientHandle) {
                DEBUG_FUNCTION_LINE_ERR("[%s] Call FSARewindDir with %08X for parent layer", getName().c_str(), dirHandle->realDirHandle);
                if (FSARewindDir(clientHandle, dirHandle->realDirHandle) == FS_ERROR_OK) {
                    dirHandle->realDirHandle = 0;
                } else {
                    DEBUG_FUNCTION_LINE_ERR("[%s] Failed to rewind dir for realDirHandle %08X", getName().c_str(), dirHandle->realDirHandle);
                }
            } else {
                DEBUG_FUNCTION_LINE_ERR("[%s] clientHandle was null", getName().c_str());
            }
        }
        OSMemoryBarrier();
    }
    return res;
}

FSWrapperMergeDirsWithParent::FSWrapperMergeDirsWithParent(const std::string &name,
                                                           const std::string &pathToReplace,
                                                           const std::string &replaceWithPath,
                                                           bool fallbackOnError) : FSWrapper(name,
                                                                                             pathToReplace,
                                                                                             replaceWithPath,
                                                                                             fallbackOnError,
                                                                                             false) {
    FSAInit();
    this->clientHandle = FSAAddClient(nullptr);
    if (clientHandle < 0) {
        DEBUG_FUNCTION_LINE_ERR("[%s] FSAClientHandle failed: %s (%d)", name.c_str(), FSAGetStatusStr(static_cast<FSError>(clientHandle)), clientHandle);
        clientHandle = 0;
    }
}

FSWrapperMergeDirsWithParent::~FSWrapperMergeDirsWithParent() {
    if (clientHandle) {
        FSError res;
        if ((res = FSADelClient(clientHandle)) != FS_ERROR_OK) {
            DEBUG_FUNCTION_LINE_ERR("[%s] FSADelClient failed: %s (%d)", FSAGetStatusStr(res), res);
        }
        clientHandle = 0;
    }
}

std::shared_ptr<DirInfoEx> FSWrapperMergeDirsWithParent::getDirExFromHandle(FSADirectoryHandle handle) {
    auto dir = std::dynamic_pointer_cast<DirInfoEx>(getDirFromHandle(handle));

    if (!dir) {
        DEBUG_FUNCTION_LINE_ERR("[%s] dynamic_pointer_cast<DirInfoEx *>(%08X) failed", getName().c_str(), handle);
        OSFatal("ContentRedirectionModule: dynamic_pointer_cast<DirInfoEx *> failed");
    }
    return dir;
}

std::shared_ptr<DirInfo> FSWrapperMergeDirsWithParent::getNewDirHandle() {
    return make_shared_nothrow<DirInfoEx>();
}
