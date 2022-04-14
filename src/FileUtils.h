#pragma once

#include "IFSWrapper.h"
#include <coreinit/filesystem.h>
#include <functional>
#include <mutex>
#include <romfs_dev.h>
#include <string>

extern std::mutex fsLayerMutex;
extern std::vector<IFSWrapper *> fsLayers;

#define SYNC_RESULT_HANDLER [filename = __FILENAME__, func = __FUNCTION__, line = __LINE__]([[maybe_unused]] IFSWrapper *layer, FSStatus res) -> FSStatus { \
    DEBUG_FUNCTION_LINE_VERBOSE_EX(filename, func, line, "Sync result was %d", res);                                                                        \
    return res;                                                                                                                                             \
}


#define ASYNC_RESULT_HANDLER [c = client, b = block, a = asyncData, filename = __FILENAME__, func = __FUNCTION__, line = __LINE__]([[maybe_unused]] IFSWrapper *layer, FSStatus res) -> FSStatus { \
    DEBUG_FUNCTION_LINE_VERBOSE_EX(filename, func, line, "Async result was %d", res);                                                                                                              \
    return send_result_async(c, b, a, res);                                                                                                                                                        \
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

struct WUT_PACKED FSCmdBlockBody { //! FSAsyncResult object used for this command.

    WUT_UNKNOWN_BYTES(0x96C);
    FSAsyncResult asyncResult;
};
WUT_CHECK_OFFSET(FSCmdBlockBody, 0x96C, asyncResult);
WUT_CHECK_SIZE(FSCmdBlockBody, 0x96C + 0x28);

struct WUT_PACKED FSClientBody {
    WUT_UNKNOWN_BYTES(0x1448);
    void *fsm;
    WUT_UNKNOWN_BYTES(0x15E8 - 0x1448 - 0x04);
    uint32_t lastError;
    BOOL isLastErrorWithoutVolume;
    WUT_UNKNOWN_BYTES(0x161C - 0x15E8 - 0x4 - 0x4);
    FSClient *client;
};
WUT_CHECK_OFFSET(FSClientBody, 0x1448, fsm);
WUT_CHECK_OFFSET(FSClientBody, 0x15E8, lastError);
WUT_CHECK_OFFSET(FSClientBody, 0x15EC, isLastErrorWithoutVolume);
WUT_CHECK_OFFSET(FSClientBody, 0x161C, client);
WUT_CHECK_SIZE(FSClientBody, 0x161C + 0x4);

std::string getFullPathForClient(FSClient *pClient, char *path);

void setWorkingDir(FSClient *client, const char *path);

FSStatus doForLayer(FSClient *client,
                    FSErrorFlag errorMask,
                    const std::function<FSStatus(FSErrorFlag errorMask)> &real_function,
                    const std::function<FSStatus(IFSWrapper *layer)> &layer_callback,
                    const std::function<FSStatus(IFSWrapper *layer, FSStatus)> &result_handler);

FSCmdBlockBody *fsCmdBlockGetBody(FSCmdBlock *cmdBlock);

FSClientBody *fsClientGetBody(FSClient *client);

FSStatus send_result_async(FSClient *client, FSCmdBlock *block, FSAsyncData *asyncData, FSStatus result);

int64_t readIntoBuffer(int32_t handle, void *buffer, size_t size, size_t count);

int64_t writeFromBuffer(int32_t handle, void *buffer, size_t size, size_t count);

int32_t CreateSubfolder(const char *fullpath);