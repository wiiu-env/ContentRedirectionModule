#pragma once
#include <coreinit/filesystem.h>
#include <functional>
#include <string>

#define FS_ERROR_EXTRA_MASK         0xFFF00000
#define FS_ERROR_REAL_MASK          0x000FFFFF
#define FS_ERROR_FORCE_PARENT_LAYER (FSError) 0xFFE0000
#define FS_ERROR_FORCE_NO_FALLBACK  (FSError) 0xFFD0000

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

class IFSWrapper {
public:
    virtual ~IFSWrapper() = default;
    virtual FSError FSOpenDirWrapper(const char *path,
                                     FSDirectoryHandle *handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSReadDirWrapper(FSDirectoryHandle handle,
                                     FSDirectoryEntry *entry) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSCloseDirWrapper(FSDirectoryHandle handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }


    virtual FSError FSRewindDirWrapper(FSDirectoryHandle handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSMakeDirWrapper(const char *path) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSOpenFileWrapper(const char *path,
                                      const char *mode,
                                      FSFileHandle *handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSCloseFileWrapper(FSFileHandle handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSGetStatWrapper(const char *path,
                                     FSStat *stats) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }


    virtual FSError FSGetStatFileWrapper(FSFileHandle handle,
                                         FSStat *stats) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSReadFileWrapper(void *buffer,
                                      uint32_t size,
                                      uint32_t count,
                                      FSFileHandle handle,
                                      uint32_t unk1) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSReadFileWithPosWrapper(void *buffer,
                                             uint32_t size,
                                             uint32_t count,
                                             uint32_t pos,
                                             FSFileHandle handle,
                                             int32_t unk1) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSSetPosFileWrapper(FSFileHandle handle,
                                        uint32_t pos) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSGetPosFileWrapper(FSFileHandle handle,
                                        uint32_t *pos) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSIsEofWrapper(FSFileHandle handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSTruncateFileWrapper(FSFileHandle handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSWriteFileWrapper(uint8_t *buffer,
                                       uint32_t size,
                                       uint32_t count,
                                       FSFileHandle handle,
                                       uint32_t unk1) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSRemoveWrapper(const char *path) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSRenameWrapper(const char *oldPath,
                                    const char *newPath) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSFlushFileWrapper(FSFileHandle handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
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
