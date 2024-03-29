#pragma once
#include "DirInfoEx.h"
#include "FSWrapper.h"
#include <coreinit/filesystem.h>
#include <functional>

class FSWrapperMergeDirsWithParent : public FSWrapper {
public:
    FSWrapperMergeDirsWithParent(const std::string &name,
                                 const std::string &pathToReplace,
                                 const std::string &replaceWithPath,
                                 bool fallbackOnError);

    ~FSWrapperMergeDirsWithParent() override;

    FSError FSOpenDirWrapper(const char *path,
                             FSDirectoryHandle *handle) override;

    FSError FSReadDirWrapper(FSDirectoryHandle handle,
                             FSDirectoryEntry *entry) override;

    FSError FSCloseDirWrapper(FSDirectoryHandle handle) override;

    FSError FSRewindDirWrapper(FSDirectoryHandle handle) override;

    std::shared_ptr<DirInfo> getNewDirHandle() override;

    bool SkipDeletedFilesInReadDir() override;

    uint32_t getLayerId() override {
        return (uint32_t) clientHandle;
    }

private:
    FSAClientHandle clientHandle;

    std::shared_ptr<DirInfoEx> getDirExFromHandle(FSDirectoryHandle handle);
};
