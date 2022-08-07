#include "FSWrapperMergeDirsWithParent.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/filesystem.h>
#include <filesystem>

FSError FSWrapperMergeDirsWithParent::FSOpenDirWrapper(const char *path,
                                                       FSDirectoryHandle *handle) {
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

            if (pFSClient && pCmdBlock) {
                FSDirectoryHandle realHandle = 0;
                DEBUG_FUNCTION_LINE_VERBOSE("[%s] Call real_FSOpenDir for %s with error_flag %08X", getName().c_str(), path, this->getHandle());
                // Call FSOpen with "this" as errorFlag call FSOpen for "parent" layers only.
                if (FSOpenDir(pFSClient, pCmdBlock, path, &realHandle, (FSErrorFlag) this->getHandle()) == FS_STATUS_OK) {
                    dirHandle->realDirHandle = realHandle;
                } else {
                    DEBUG_FUNCTION_LINE_VERBOSE("[%s] Failed to open real dir %s", getName().c_str(), path);
                }
            } else {
                DEBUG_FUNCTION_LINE_ERR("[%s] Global FSClient or FSCmdBlock were null", getName().c_str());
            }
            OSMemoryBarrier();
        }
    }
    return res;
}

bool FSWrapperMergeDirsWithParent::SkipDeletedFilesInReadDir() {
    return false;
}

FSError FSWrapperMergeDirsWithParent::FSReadDirWrapper(FSDirectoryHandle handle, FSDirectoryEntry *entry) {
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
                        OSFatal("Failed to alloc memory for read result");
                    }
                    dirHandle->readResultCapacity = 1;
                }

                if (dirHandle->readResultNumberOfEntries >= dirHandle->readResultCapacity) {
                    auto newCapacity              = dirHandle->readResultCapacity * 2;
                    dirHandle->readResult         = (FSDirectoryEntryEx *) realloc(dirHandle->readResult, newCapacity * sizeof(FSDirectoryEntryEx));
                    dirHandle->readResultCapacity = newCapacity;
                    if (dirHandle->readResult == nullptr) {
                        DEBUG_FUNCTION_LINE_ERR("[%s] Failed to realloc memory for %08X (handle %08X)", getName().c_str(), dirHandle.get(), handle);
                        OSFatal("Failed to alloc memory for read result");
                    }
                }

                memcpy(&dirHandle->readResult[dirHandle->readResultNumberOfEntries].realEntry, entry, sizeof(FSDirectoryEntry));
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
                    if (pFSClient && pCmdBlock) {
                        FSDirectoryEntry realDirEntry;
                        FSStatus readDirResult;
                        while (true) {
                            DEBUG_FUNCTION_LINE_VERBOSE("[%s] Call real_FSReadDir for %08X with error_flag %08X", getName().c_str(), dirHandle->realDirHandle, (uint32_t) this->getHandle());
                            readDirResult = FSReadDir(pFSClient, pCmdBlock, dirHandle->realDirHandle, &realDirEntry, (FSErrorFlag) (uint32_t) this->getHandle());
                            if (readDirResult == FS_STATUS_OK) {
                                bool found       = false;
                                auto nameDeleted = deletePrefix + realDirEntry.name;
                                for (int i = 0; i < dirHandle->readResultNumberOfEntries; i++) {
                                    auto curResult = &dirHandle->readResult[i];

                                    // Don't return file that are "deleted"
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
                                    memcpy(entry, &realDirEntry, sizeof(FSDirectoryEntry));
                                    res = FS_ERROR_OK;
                                    break;
                                }
                            } else if (readDirResult == FS_STATUS_END) {
                                res = FS_ERROR_END_OF_DIR;
                                break;
                            } else {
                                DEBUG_FUNCTION_LINE_ERR("[%s] real_FSReadDir returned an unexpected error: %08X", getName().c_str(), readDirResult);
                                res = FS_ERROR_END_OF_DIR;
                                break;
                            }
                        }
                    } else {
                        DEBUG_FUNCTION_LINE_ERR("[%s] Global FSClient or FSCmdBlock were null", getName().c_str());
                    }
                }
            } else {
                DEBUG_FUNCTION_LINE_ERR("[%s] Unexpected result %d", getName().c_str(), res);
            }
        }
        return res;
    } while (true);
}

FSError FSWrapperMergeDirsWithParent::FSCloseDirWrapper(FSDirectoryHandle handle) {
    auto res = FSWrapper::FSCloseDirWrapper(handle);

    if (res == FS_ERROR_OK) {
        if (!isValidDirHandle(handle)) {
            DEBUG_FUNCTION_LINE_ERR("[%s] No valid dir handle %08X", getName().c_str(), handle);
            return FS_ERROR_INVALID_DIRHANDLE;
        }
        auto dirHandle = getDirExFromHandle(handle);
        if (dirHandle->realDirHandle != 0) {
            if (pFSClient && pCmdBlock) {
                DEBUG_FUNCTION_LINE_VERBOSE("[%s] Call FSCloseDir for %08X with error_flag %08X (this)", getName().c_str(), dirHandle->realDirHandle, (uint32_t) this->getHandle());
                auto realResult = FSCloseDir(pFSClient, pCmdBlock, dirHandle->realDirHandle, (FSErrorFlag) (uint32_t) this->getHandle());
                if (realResult == FS_STATUS_OK) {
                    dirHandle->realDirHandle = 0;
                } else {
                    DEBUG_FUNCTION_LINE_ERR("[%s] Failed to close realDirHandle %d: res %d", getName().c_str(), dirHandle->realDirHandle, realResult);
                    return realResult == FS_STATUS_CANCELLED ? FS_ERROR_CANCELLED : FS_ERROR_MEDIA_ERROR;
                }
            } else {
                DEBUG_FUNCTION_LINE_ERR("[%s] Global FSClient or FSCmdBlock were null", getName().c_str());
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

FSError FSWrapperMergeDirsWithParent::FSRewindDirWrapper(FSDirectoryHandle handle) {
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
            if (pFSClient && pCmdBlock) {
                DEBUG_FUNCTION_LINE_VERBOSE("[%s] Call real_FSRewindDir for %08X with error_flag %08X (this->getHandle())", getName().c_str(), dirHandle->realDirHandle, (uint32_t) this->getHandle());
                if (FSRewindDir(pFSClient, pCmdBlock, dirHandle->realDirHandle, (FSErrorFlag) (uint32_t) this->getHandle()) == FS_STATUS_OK) {
                    dirHandle->realDirHandle = 0;
                } else {
                    DEBUG_FUNCTION_LINE_ERR("[%s] Failed to rewind dir for realDirHandle %08X", getName().c_str(), dirHandle->realDirHandle);
                }
            } else {
                DEBUG_FUNCTION_LINE_ERR("[%s] Global FSClient or FSCmdBlock were null", getName().c_str());
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
    pFSClient = new (std::nothrow) FSClient;
    pCmdBlock = new (std::nothrow) FSCmdBlock;
    if (pFSClient == nullptr || pCmdBlock == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("[%s] Failed to alloc client or cmdBlock", name.c_str());
        freeFSClient();
    }
    if (FSAddClient(pFSClient, FS_ERROR_FLAG_ALL) != FS_STATUS_OK) {
        DEBUG_FUNCTION_LINE_ERR("[%s] Failed to addClient");
        freeFSClient();
    }
    FSInitCmdBlock(pCmdBlock);
}

FSWrapperMergeDirsWithParent::~FSWrapperMergeDirsWithParent() {
    freeFSClient();
}

void FSWrapperMergeDirsWithParent::freeFSClient() {
    if (pFSClient) {
        FSDelClient(pFSClient, FS_ERROR_FLAG_ALL);
    }
    delete pFSClient;
    delete pCmdBlock;
    pFSClient = nullptr;
    pCmdBlock = nullptr;
}

std::shared_ptr<DirInfoEx> FSWrapperMergeDirsWithParent::getDirExFromHandle(FSDirectoryHandle handle) {
    auto dir = std::dynamic_pointer_cast<DirInfoEx>(getDirFromHandle(handle));

    if (!dir) {
        DEBUG_FUNCTION_LINE_ERR("[%s] dynamic_pointer_cast<DirInfoEx *>(%08X) failed", getName().c_str(), handle);
        OSFatal("dynamic_pointer_cast<DirInfoEx *> failed");
    }
    return dir;
}

std::shared_ptr<DirInfo> FSWrapperMergeDirsWithParent::getNewDirHandle() {
    return make_shared_nothrow<DirInfoEx>();
}
