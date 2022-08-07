#pragma once

#include "IFSWrapper.h"
#include <coreinit/filesystem.h>
#include <functional>
#include <mutex>
#include <romfs_dev.h>
#include <string>

extern std::mutex fsLayerMutex;
extern std::vector<std::unique_ptr<IFSWrapper>> fsLayers;

#define SYNC_RESULT_HANDLER [filename = __FILENAME__, func = __FUNCTION__, line = __LINE__]([[maybe_unused]] std::unique_ptr<IFSWrapper> &layer, FSStatus res) -> FSStatus { \
    DEBUG_FUNCTION_LINE_VERBOSE_EX(filename, func, line, "Sync result was %d", res);                                                                                         \
    return res;                                                                                                                                                              \
}

#define ASYNC_RESULT_HANDLER [c = client, b = block, a = asyncData, filename = __FILENAME__, func = __FUNCTION__, line = __LINE__]([[maybe_unused]] std::unique_ptr<IFSWrapper> &layer, FSStatus res) -> FSStatus { \
    DEBUG_FUNCTION_LINE_VERBOSE_EX(filename, func, line, "Async result was %d", res);                                                                                                                               \
    return send_result_async(c, b, a, res);                                                                                                                                                                         \
}

#define FS_ERROR_FLAG_EXTRA_MASK (FSErrorFlag) 0xFFFF0000
#define FS_ERROR_FLAG_REAL_MASK  (FSErrorFlag) 0x0000FFFF
#define FS_ERROR_FLAG_FORCE_REAL (FSErrorFlag) 0xFEDC0000

static inline FSErrorFlag getRealErrorFlag(FSErrorFlag flag) {
    auto res = flag & FS_ERROR_FLAG_REAL_MASK;
    if (res == 0x0000FFFF) {
        return FS_ERROR_FLAG_ALL;
    }
    return static_cast<FSErrorFlag>(res);
}

static inline FSErrorFlag isForceRealFunction(FSErrorFlag flag) {
    return static_cast<FSErrorFlag>((flag & FS_ERROR_FLAG_EXTRA_MASK) == FS_ERROR_FLAG_FORCE_REAL);
}

void clearFSLayer();

std::string getFullPathForClient(FSClient *pClient, const char *path);

void setWorkingDir(FSClient *client, const char *path);

FSStatus doForLayer(FSClient *client,
                    FSErrorFlag errorMask,
                    const std::function<FSStatus(FSErrorFlag errorMask)> &real_function,
                    const std::function<FSError(std::unique_ptr<IFSWrapper> &layer)> &layer_callback,
                    const std::function<FSStatus(std::unique_ptr<IFSWrapper> &layer, FSStatus)> &result_handler);

FSCmdBlockBody *fsCmdBlockGetBody(FSCmdBlock *cmdBlock);

FSClientBody *fsClientGetBody(FSClient *client);

FSStatus send_result_async(FSClient *client, FSCmdBlock *block, FSAsyncData *asyncData, FSStatus result);

int64_t readIntoBuffer(int32_t handle, void *buffer, size_t size, size_t count);

int64_t writeFromBuffer(int32_t handle, void *buffer, size_t size, size_t count);

int32_t CreateSubfolder(const char *fullpath);