#pragma once
#include <coreinit/filesystem_fsa.h>
#include <functional>
#include <string>

#define FS_ERROR_EXTRA_MASK          0xFFF00000
#define FS_ERROR_REAL_MASK           0x000FFFFF
#define FS_ERROR_FORCE_PARENT_LAYER  (FSError) 0xFFE00000
#define FS_ERROR_FORCE_NO_FALLBACK   (FSError) 0xFFD00000
#define FS_ERROR_FORCE_REAL_FUNCTION (FSError) 0xFFC00000

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

class IFSWrapper {
public:
    virtual ~IFSWrapper() = default;
    virtual FSError FSOpenDirWrapper(const char *path,
                                     FSADirectoryHandle *handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSReadDirWrapper(FSADirectoryHandle handle,
                                     FSADirectoryEntry *entry) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSCloseDirWrapper(FSADirectoryHandle handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }


    virtual FSError FSRewindDirWrapper(FSADirectoryHandle handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSMakeDirWrapper(const char *path) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSOpenFileWrapper(const char *path,
                                      const char *mode,
                                      FSAFileHandle *handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSCloseFileWrapper(FSAFileHandle handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSGetStatWrapper(const char *path,
                                     FSAStat *stats) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }


    virtual FSError FSGetStatFileWrapper(FSAFileHandle handle,
                                         FSAStat *stats) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSReadFileWrapper(void *buffer,
                                      uint32_t size,
                                      uint32_t count,
                                      FSAFileHandle handle,
                                      uint32_t unk1) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSReadFileWithPosWrapper(void *buffer,
                                             uint32_t size,
                                             uint32_t count,
                                             uint32_t pos,
                                             FSAFileHandle handle,
                                             int32_t unk1) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSSetPosFileWrapper(FSAFileHandle handle,
                                        uint32_t pos) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSGetPosFileWrapper(FSAFileHandle handle,
                                        uint32_t *pos) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSIsEofWrapper(FSAFileHandle handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSTruncateFileWrapper(FSAFileHandle handle) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSWriteFileWrapper(const uint8_t *buffer,
                                       uint32_t size,
                                       uint32_t count,
                                       FSAFileHandle handle,
                                       uint32_t unk1) {
        return FS_ERROR_FORCE_PARENT_LAYER;
    }

    virtual FSError FSWriteFileWithPosWrapper(const uint8_t *buffer,
                                              uint32_t size,
                                              uint32_t count,
                                              FSAFilePosition pos,
                                              FSAFileHandle handle,
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

    virtual FSError FSFlushFileWrapper(FSAFileHandle handle) {
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

    virtual bool isValidDirHandle(FSADirectoryHandle handle) = 0;

    virtual bool isValidFileHandle(FSAFileHandle handle) = 0;

    virtual void deleteDirHandle(FSADirectoryHandle handle) = 0;

    virtual void deleteFileHandle(FSAFileHandle handle) = 0;

    virtual uint32_t getLayerId() = 0;

    virtual uint32_t getHandle() {
        return (uint32_t) this;
    }

private:
    bool pIsActive = true;

protected:
    bool pFallbackOnError = false;
    std::string pName;
};
#pragma GCC diagnostic pop
