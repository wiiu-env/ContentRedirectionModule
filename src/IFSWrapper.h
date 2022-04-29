#pragma once
#include <coreinit/filesystem.h>
#include <functional>
#include <string>

#define FS_STATUS_FORCE_PARENT_LAYER (FSStatus) 0xFFFF0000
#define FS_STATUS_FORCE_NO_FALLBACK  (FSStatus) 0xFFFE0000

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

class IFSWrapper {
public:
    virtual ~IFSWrapper() = default;
    virtual FSStatus FSOpenDirWrapper(const char *path,
                                      FSDirectoryHandle *handle) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSReadDirWrapper(FSDirectoryHandle handle,
                                      FSDirectoryEntry *entry) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSCloseDirWrapper(FSDirectoryHandle handle) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }


    virtual FSStatus FSRewindDirWrapper(FSDirectoryHandle handle) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSMakeDirWrapper(const char *path) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSOpenFileWrapper(const char *path,
                                       const char *mode,
                                       FSFileHandle *handle) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSCloseFileWrapper(FSFileHandle handle) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSGetStatWrapper(const char *path,
                                      FSStat *stats) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }


    virtual FSStatus FSGetStatFileWrapper(FSFileHandle handle,
                                          FSStat *stats) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSReadFileWrapper(void *buffer,
                                       uint32_t size,
                                       uint32_t count,
                                       FSFileHandle handle,
                                       uint32_t unk1) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSReadFileWithPosWrapper(void *buffer,
                                              uint32_t size,
                                              uint32_t count,
                                              uint32_t pos,
                                              FSFileHandle handle,
                                              int32_t unk1) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSSetPosFileWrapper(FSFileHandle handle,
                                         uint32_t pos) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSGetPosFileWrapper(FSFileHandle handle,
                                         uint32_t *pos) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSIsEofWrapper(FSFileHandle handle) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSTruncateFileWrapper(FSFileHandle handle) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSWriteFileWrapper(uint8_t *buffer,
                                        uint32_t size,
                                        uint32_t count,
                                        FSFileHandle handle,
                                        uint32_t unk1) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSRemoveWrapper(const char *path) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSRenameWrapper(const char *oldPath,
                                     const char *newPath) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual FSStatus FSFlushFileWrapper(FSFileHandle handle) {
        return FS_STATUS_FORCE_PARENT_LAYER;
    }

    virtual bool fallbackOnError() {
        return pFallbackOnError;
    }

    virtual bool isActive() {
        return pIsActive;
    }

    virtual void setActive(bool newValue) {
        pIsActive = newValue;
    }

    [[nodiscard]] virtual std::string getName() const {
        return pName;
    }

    virtual bool isValidDirHandle(FSDirectoryHandle handle) = 0;

    virtual bool isValidFileHandle(FSFileHandle handle) = 0;

    virtual void deleteDirHandle(FSDirectoryHandle handle) = 0;

    virtual void deleteFileHandle(FSFileHandle handle) = 0;

    uint32_t getHandle() {
        return (uint32_t) this;
    }

private:
    bool pIsActive = true;

protected:
    bool pFallbackOnError = false;
    std::string pName;
};
#pragma GCC diagnostic pop
