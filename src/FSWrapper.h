#pragma once
#include "DirInfo.h"
#include "FileInfo.h"
#include "IFSWrapper.h"
#include "utils/logger.h"
#include <coreinit/filesystem.h>
#include <coreinit/mutex.h>
#include <functional>
#include <mutex>

class FSWrapper : public IFSWrapper {
public:
    FSWrapper(const std::string &name, const std::string &pathToReplace, const std::string &replacePathWith, bool fallbackOnError, bool isWriteable) {
        this->pName            = name;
        this->pPathToReplace   = pathToReplace;
        this->pReplacePathWith = replacePathWith;
        this->pFallbackOnError = fallbackOnError;
        this->pIsWriteable     = isWriteable;
        this->pCheckIfDeleted  = fallbackOnError;

        std::replace(pPathToReplace.begin(), pPathToReplace.end(), '\\', '/');
        std::replace(pReplacePathWith.begin(), pReplacePathWith.end(), '\\', '/');
    }
    ~FSWrapper() override {
        {
            std::lock_guard<std::mutex> lockFiles(openFilesMutex);
            openFiles.clear();
        }
        {
            std::lock_guard<std::mutex> lockDirs(openDirsMutex);
            openDirs.clear();
        }
    }

    FSStatus FSOpenDirWrapper(const char *path,
                              FSDirectoryHandle *handle) override;


    FSStatus FSReadDirWrapper(FSDirectoryHandle handle,
                              FSDirectoryEntry *entry) override;

    FSStatus FSCloseDirWrapper(FSDirectoryHandle handle) override;


    FSStatus FSMakeDirWrapper(const char *path) override;


    FSStatus FSRewindDirWrapper(FSDirectoryHandle handle) override;


    FSStatus FSOpenFileWrapper(const char *path,
                               const char *mode,
                               FSFileHandle *handle) override;

    FSStatus FSCloseFileWrapper(FSFileHandle handle) override;

    FSStatus FSGetStatWrapper(const char *path, FSStat *stats) override;

    FSStatus FSGetStatFileWrapper(FSFileHandle handle,
                                  FSStat *stats) override;

    FSStatus FSReadFileWrapper(void *buffer,
                               uint32_t size,
                               uint32_t count,
                               FSFileHandle handle,
                               uint32_t unk1) override;

    FSStatus FSReadFileWithPosWrapper(void *buffer,
                                      uint32_t size,
                                      uint32_t count,
                                      uint32_t pos,
                                      FSFileHandle handle,
                                      int32_t unk1) override;

    FSStatus FSSetPosFileWrapper(FSFileHandle handle,
                                 uint32_t pos) override;

    FSStatus FSGetPosFileWrapper(FSFileHandle handle,
                                 uint32_t *pos) override;

    FSStatus FSIsEofWrapper(FSFileHandle handle) override;

    FSStatus FSTruncateFileWrapper(FSFileHandle handle) override;

    FSStatus FSWriteFileWrapper(uint8_t *buffer,
                                uint32_t size,
                                uint32_t count,
                                FSFileHandle handle,
                                uint32_t unk1) override;

    FSStatus FSRemoveWrapper(const char *path) override;

    FSStatus FSRenameWrapper(const char *oldPath,
                             const char *newPath) override;

    FSStatus FSFlushFileWrapper(FSFileHandle handle) override;

protected:
    virtual bool IsFileModeAllowed(const char *mode);

    virtual bool IsPathToReplace(const std::string_view &path);

    std::string GetNewPath(const std::string_view &path);

    std::shared_ptr<DirInfo> getDirFromHandle(FSDirectoryHandle handle);
    std::shared_ptr<FileInfo> getFileFromHandle(FSFileHandle handle);

    bool isValidDirHandle(FSDirectoryHandle handle) override;
    bool isValidFileHandle(FSFileHandle handle) override;

    void deleteDirHandle(FSDirectoryHandle handle) override;
    void deleteFileHandle(FSFileHandle handle) override;

    virtual bool CheckFileShouldBeIgnored(std::string &path);

    virtual std::shared_ptr<FileInfo> getNewFileHandle();
    virtual std::shared_ptr<DirInfo> getNewDirHandle();

    virtual bool SkipDeletedFilesInReadDir();

    bool pCheckIfDeleted = false;

    std::string deletePrefix = ".deleted_";

private:
    std::string pPathToReplace;
    std::string pReplacePathWith;
    bool pIsWriteable = false;
    std::mutex openFilesMutex;
    std::mutex openDirsMutex;
    std::vector<std::shared_ptr<FileInfo>> openFiles;
    std::vector<std::shared_ptr<DirInfo>> openDirs;
};
