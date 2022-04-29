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

    FSStatus FSOpenDirWrapper(const char *path,
                              FSDirectoryHandle *handle) override;

    FSStatus FSReadDirWrapper(FSDirectoryHandle handle,
                              FSDirectoryEntry *entry) override;

    FSStatus FSCloseDirWrapper(FSDirectoryHandle handle) override;

    FSStatus FSRewindDirWrapper(FSDirectoryHandle handle) override;

    std::shared_ptr<DirInfo> getNewDirHandle() override;

    bool SkipDeletedFilesInReadDir() override;

private:
    void freeFSClient();
    FSClient *pFSClient;
    FSCmdBlock *pCmdBlock;

    std::shared_ptr<DirInfoEx> getDirExFromHandle(FSDirectoryHandle handle);
};
