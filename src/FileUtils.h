#pragma once

#include "IFSWrapper.h"
#include "utils/logger.h"
#include <coreinit/core.h>
#include <coreinit/filesystem.h>
#include <coreinit/filesystem_fsa.h>
#include <functional>
#include <mutex>
#include <string>

struct FSIOThreadData {
    OSThread *thread;
    void *stack;
    OSMessageQueue queue;
    OSMessage messages[0x10];
    bool setup;
    char threadName[0x50];
};

struct AsyncParamFS {
    FSClient *client;
    FSCmdBlock *block;
    FSErrorFlag errorMask;
    FSAsyncData asyncData;
    IOSAsyncCallbackFn callback;
};

typedef enum FSShimSyncType {
    FS_SHIM_TYPE_SYNC  = 1,
    FS_SHIM_TYPE_ASYNC = 2
} FSShimSyncType;

typedef enum FSShimApiType {
    FS_SHIM_API_FS  = 1,
    FS_SHIM_API_FSA = 2
} FSShimApiType;

struct FSShimWrapper {
    FSAShimBuffer *shim;
    AsyncParamFS asyncFS;
    FSShimSyncType sync;
    FSShimApiType api;
};

struct FSShimWrapperMessage {
    FSShimWrapper *param;
    OSMessageQueue messageQueue;
    OSMessage messages[0x1];
};

#define FS_IO_QUEUE_COMMAND_STOP               0x13371337
#define FS_IO_QUEUE_COMMAND_PROCESS_FS_COMMAND 0x42424242
#define FS_IO_QUEUE_SYNC_RESULT                0x43434343

extern bool gThreadsRunning;
extern FSIOThreadData gThreadData[3];
extern std::mutex fsLayerMutex;
extern std::vector<std::unique_ptr<IFSWrapper>> fsLayers;

#define fsaShimPrepareRequestReadFile    ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, uint8_t * buffer, uint32_t size, uint32_t count, uint32_t pos, FSFileHandle handle, FSAReadFlag readFlags))(0x101C400 + 0x436cc))
#define fsaShimPrepareRequestWriteFile   ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, const uint8_t *buffer, uint32_t size, uint32_t count, uint32_t pos, FSFileHandle handle, FSAWriteFlag writeFlags))(0x101C400 + 0x437f4))
#define fsaShimPrepareRequestOpenFile    ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, const char *path, const char *mode, FSMode createMode, FSOpenFileFlags openFlag, uint32_t preallocSize))(0x101C400 + 0x43588))
#define fsaShimPrepareRequestCloseFile   ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, FSFileHandle handle))(0x101C400 + 0x43a00))
#define fsaShimPrepareRequestStatFile    ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, FSFileHandle handle))(0x101C400 + 0x43998))
#define fsaShimPrepareRequestQueryInfo   ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, const char *path, FSAQueryInfoType type))(0x101C400 + 0x44118))
#define fsaShimPrepareRequestSetPos      ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, FSFileHandle handle, FSAFilePosition position))(0x101C400 + 0x43930))
#define fsaShimPrepareRequestGetPos      ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, FSFileHandle handle))(0x101C400 + 0x438fc))
#define fsaShimPrepareRequestIsEof       ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, FSFileHandle handle))(0x101C400 + 0x43964))
#define fsaShimPrepareRequestTruncate    ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, FSFileHandle handle))(0x101C400 + 0x43a34))
#define fsaShimPrepareRequestRemove      ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, const char *))(0x101C400 + 0x43aa8))
#define fsaShimPrepareRequestRename      ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, const char *, const char *))(0x101C400 + 0x43bc0))
#define fsaShimPrepareRequestFlushFile   ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, FSFileHandle handle))(0x101C400 + 0x439cc))
#define fsaShimPrepareRequestChangeMode  ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, const char *path, FSMode mode, FSMode modeMask))(0x101C400 + 0x43ff4))

#define fsaShimPrepareRequestOpenDir     ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, const char *path))(0x101C400 + 0x43458))
#define fsaShimPrepareRequestReadDir     ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, FSDirectoryHandle handle))(0x101C400 + 0x434ec))
#define fsaShimPrepareRequestCloseDir    ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, FSDirectoryHandle handle))(0x101C400 + 0x43554))
#define fsaShimPrepareRequestRewindDir   ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, FSDirectoryHandle handle))(0x101C400 + 0x43520))
#define fsaShimPrepareRequestMakeDir     ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, const char *path, FSMode mode))(0x101C400 + 0x43314))
#define fsaShimPrepareRequestChangeDir   ((FSError(*)(FSAShimBuffer * shim, IOSHandle clientHandle, const char *path))(0x101C400 + 0x43258))

#define fsaDecodeFsaStatusToFsStatus     ((FSStatus(*)(FSError))(0x101C400 + 0x4b148))
#define fsClientHandleFatalError         ((void (*)(FSClientBody *, uint32_t))(0x101C400 + 0x4b34c))
#define fsClientHandleFatalErrorAndBlock ((void (*)(FSClientBody *, uint32_t))(0x101C400 + 0x4cc20))

extern "C" FSError __FSAShimDecodeIosErrorToFsaStatus(IOSHandle handle, IOSError err);

bool sendMessageToThread(FSShimWrapperMessage *param);

void clearFSLayer();

FSError doForLayer(FSShimWrapper *param);

FSError processShimBufferForFS(FSShimWrapper *param);

FSError processShimBufferForFSA(FSShimWrapper *param);

FSCmdBlockBody *fsCmdBlockGetBody(FSCmdBlock *cmdBlock);

FSClientBody *fsClientGetBody(FSClient *client);

FSStatus handleAsyncResult(FSClient *client, FSCmdBlock *block, FSAsyncData *asyncData, FSStatus status);

int64_t readIntoBuffer(int32_t handle, void *buffer, size_t size, size_t count);

int64_t writeFromBuffer(int32_t handle, const void *buffer, size_t size, size_t count);

void startFSIOThreads();
void stopFSIOThreads();