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
    FSWrapper(const std::string &name, const std::string &pathToReplace, const std::string &replacePathWith, bool fallbackOnError, bool isWriteable, std::vector<std::string> ignorePaths = {}) {
        this->pName            = name;
        this->pPathToReplace   = pathToReplace;
        this->pReplacePathWith = replacePathWith;
        this->pFallbackOnError = fallbackOnError;
        this->pIsWriteable     = isWriteable;
        this->pCheckIfDeleted  = fallbackOnError;
        this->pIgnorePaths     = std::move(ignorePaths);

        std::replace(pPathToReplace.begin(), pPathToReplace.end(), '\\', '/');
        std::replace(pReplacePathWith.begin(), pReplacePathWith.end(), '\\', '/');
        for (auto &ignorePath : pIgnorePaths) {
            std::replace(ignorePath.begin(), ignorePath.end(), '\\', '/');
        }
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

    FSError FSOpenDirWrapper(const char *path,
                             FSDirectoryHandle *handle) override;


    FSError FSReadDirWrapper(FSDirectoryHandle handle,
                             FSDirectoryEntry *entry) override;

    FSError FSCloseDirWrapper(FSDirectoryHandle handle) override;


    FSError FSMakeDirWrapper(const char *path) override;


    FSError FSRewindDirWrapper(FSDirectoryHandle handle) override;


    FSError FSOpenFileWrapper(const char *path,
                              const char *mode,
                              FSFileHandle *handle) override;

    FSError FSCloseFileWrapper(FSFileHandle handle) override;

    FSError FSGetStatWrapper(const char *path, FSStat *stats) override;

    FSError FSGetStatFileWrapper(FSFileHandle handle,
                                 FSStat *stats) override;

    FSError FSReadFileWrapper(void *buffer,
                              uint32_t size,
                              uint32_t count,
                              FSFileHandle handle,
                              uint32_t unk1) override;

    FSError FSReadFileWithPosWrapper(void *buffer,
                                     uint32_t size,
                                     uint32_t count,
                                     uint32_t pos,
                                     FSFileHandle handle,
                                     int32_t unk1) override;

    FSError FSSetPosFileWrapper(FSFileHandle handle,
                                uint32_t pos) override;

    FSError FSGetPosFileWrapper(FSFileHandle handle,
                                uint32_t *pos) override;

    FSError FSIsEofWrapper(FSFileHandle handle) override;

    FSError FSTruncateFileWrapper(FSFileHandle handle) override;

    FSError FSWriteFileWrapper(const uint8_t *buffer,
                               uint32_t size,
                               uint32_t count,
                               FSFileHandle handle,
                               uint32_t unk1) override;

    FSError FSRemoveWrapper(const char *path) override;

    FSError FSRenameWrapper(const char *oldPath,
                            const char *newPath) override;

    FSError FSFlushFileWrapper(FSFileHandle handle) override;

    uint32_t getLayerId() override {
        return (uint32_t) this;
    }

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
    std::vector<std::string> pIgnorePaths;
    bool pIsWriteable = false;
    std::mutex openFilesMutex;
    std::mutex openDirsMutex;
    std::vector<std::shared_ptr<FileInfo>> openFiles;
    std::vector<std::shared_ptr<DirInfo>> openDirs;
};
