#pragma once
#include "DirInfoEx.h"
#include "FSWrapper.h"

#include <coreinit/filesystem.h>

class FSWrapperReplaceSingleFile final : public FSWrapper {
public:
    FSWrapperReplaceSingleFile(const std::string &name,
                               const std::string &fileToReplace,
                               const std::string &replaceWithPath,
                               bool fallbackOnError);

    ~FSWrapperReplaceSingleFile() override;

    FSError FSOpenDirWrapper(const char *path,
                             FSDirectoryHandle *handle) override;

    FSError FSReadDirWrapper(FSDirectoryHandle handle,
                             FSDirectoryEntry *entry) override;

    FSError FSCloseDirWrapper(FSDirectoryHandle handle) override;

    FSError FSRewindDirWrapper(FSDirectoryHandle handle) override;

    bool SkipDeletedFilesInReadDir() override;

    uint32_t getLayerId() override {
        return static_cast<uint32_t>(mClientHandle);
    }


protected:
    [[nodiscard]] std::string GetNewPath(const std::string_view &path) const override;

    std::shared_ptr<DirInfo> getNewDirInfoHandle() override {
        OSFatal("FSWrapperReplaceSingleFile::getNewDirInfoHandle. Not implemented");
        return {};
    }

private:
    bool IsDirPathToReplace(const std::string_view &path) const;

    std::shared_ptr<DirInfoExSingleFile> getDirExFromHandle(FSDirectoryHandle handle);

    FSAClientHandle mClientHandle;
    std::string mPathToReplace;
    std::string mFileNameToReplace;
    std::string mFullPathToReplace;
    std::string mReplacedWithPath;
    std::string mReplacedWithFileName;
};
