#include "FSWrapper.h"
#include "FSWrapperMergeDirsWithParent.h"
#include "FileUtils.h"
#include "IFSWrapper.h"
#include "malloc.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include <content_redirection/redirection.h>
#include <coreinit/dynload.h>
#include <mutex>
#include <nn/act.h>
#include <wums/exports.h>

struct AOCTitle {
    WUT_UNKNOWN_BYTES(0x68);
};
WUT_CHECK_SIZE(AOCTitle, 0x68);

bool getAOCPath(std::string &outStr) {
    int32_t (*AOC_Initialize)()                                                                                                            = nullptr;
    int32_t (*AOC_Finalize)()                                                                                                              = nullptr;
    int32_t (*AOC_ListTitle)(uint32_t * titleCountOut, AOCTitle * titleList, uint32_t maxCount, void *workBuffer, uint32_t workBufferSize) = nullptr;
    int32_t (*AOC_OpenTitle)(char *pathOut, AOCTitle *aocTitleInfo, void *workBuffer, uint32_t workBufferSize)                             = nullptr;
    int32_t (*AOC_CalculateWorkBufferSize)(uint32_t count)                                                                                 = nullptr;
    int32_t (*AOC_CloseTitle)(AOCTitle * aocTitleInfo)                                                                                     = nullptr;

    AOCTitle title{};
    char aocPath[256];
    aocPath[0]              = '\0';
    uint32_t outCount       = 0;
    uint32_t workBufferSize = 0;
    void *workBuffer        = nullptr;
    bool result             = false;

    OSDynLoad_Module aoc_handle;
    if (OSDynLoad_Acquire("nn_aoc.rpl", &aoc_handle) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_WARN("OSDynLoad_Acquire failed");
        return false;
    }
    if (OSDynLoad_FindExport(aoc_handle, OS_DYNLOAD_EXPORT_FUNC, "AOC_Initialize", reinterpret_cast<void **>(&AOC_Initialize)) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_WARN("OSDynLoad_FindExport failed");
        goto end;
    }
    if (OSDynLoad_FindExport(aoc_handle, OS_DYNLOAD_EXPORT_FUNC, "AOC_Finalize", reinterpret_cast<void **>(&AOC_Finalize)) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_WARN("OSDynLoad_FindExport failed");
        goto end;
    }
    if (OSDynLoad_FindExport(aoc_handle, OS_DYNLOAD_EXPORT_FUNC, "AOC_OpenTitle", reinterpret_cast<void **>(&AOC_OpenTitle)) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_WARN("OSDynLoad_FindExport failed");
        goto end;
    }
    if (OSDynLoad_FindExport(aoc_handle, OS_DYNLOAD_EXPORT_FUNC, "AOC_ListTitle", reinterpret_cast<void **>(&AOC_ListTitle)) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_WARN("OSDynLoad_FindExport failed");
        goto end;
    }
    if (OSDynLoad_FindExport(aoc_handle, OS_DYNLOAD_EXPORT_FUNC, "AOC_CalculateWorkBufferSize", reinterpret_cast<void **>(&AOC_CalculateWorkBufferSize)) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_WARN("OSDynLoad_FindExport failed");
        goto end;
    }
    if (OSDynLoad_FindExport(aoc_handle, OS_DYNLOAD_EXPORT_FUNC, "AOC_CloseTitle", reinterpret_cast<void **>(&AOC_CloseTitle)) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_WARN("OSDynLoad_FindExport failed");
        goto end;
    }

    AOC_Initialize();

    workBufferSize = AOC_CalculateWorkBufferSize(1);
    workBuffer     = memalign(0x40, workBufferSize);
    if (!workBuffer) {
        DEBUG_FUNCTION_LINE_WARN("Failed to alloc workBuffer");
        goto end;
    }
    if (AOC_ListTitle(&outCount, &title, 1, workBuffer, workBufferSize) < 0) {
        DEBUG_FUNCTION_LINE_WARN("AOC_ListTitle failed");
        goto end;
    }
    if (AOC_OpenTitle(aocPath, &title, workBuffer, workBufferSize) < 0) {
        DEBUG_FUNCTION_LINE_WARN("AOC_OpenTitle failed");
        goto end;
    }

    result = true;
    outStr = aocPath;
    AOC_CloseTitle(&title);
end:
    free(workBuffer);
    AOC_Finalize();
    OSDynLoad_Release(aoc_handle);
    return result;
}

ContentRedirectionApiErrorType CRAddFSLayer(CRLayerHandle *handle, const char *layerName, const char *replacementDir, FSLayerType layerType) {
    if (!handle || layerName == nullptr || replacementDir == nullptr) {
        DEBUG_FUNCTION_LINE_WARN("CONTENT_REDIRECTION_API_ERROR_INVALID_ARG");
        return CONTENT_REDIRECTION_API_ERROR_INVALID_ARG;
    }
    std::unique_ptr<IFSWrapper> ptr;
    if (layerType == FS_LAYER_TYPE_CONTENT_REPLACE) {
        DEBUG_FUNCTION_LINE_INFO("Redirecting \"/vol/content\" to \"%s\", mode: \"replace\"", replacementDir);
        ptr = make_unique_nothrow<FSWrapper>(layerName, "/vol/content", replacementDir, false, false);
    } else if (layerType == FS_LAYER_TYPE_CONTENT_MERGE) {
        DEBUG_FUNCTION_LINE_INFO("Redirecting \"/vol/content\" to \"%s\", mode: \"merge\"", replacementDir);
        ptr = make_unique_nothrow<FSWrapperMergeDirsWithParent>(layerName, "/vol/content", replacementDir, true);
    } else if (layerType == FS_LAYER_TYPE_AOC_MERGE || layerType == FS_LAYER_TYPE_AOC_REPLACE) {
        std::string targetPath;
        if (!getAOCPath(targetPath)) {
            DEBUG_FUNCTION_LINE_ERR("(%s) Failed to get the AOC path. Not redirecting /vol/aoc", layerName);
            return CONTENT_REDIRECTION_API_ERROR_INVALID_ARG;
        }
        DEBUG_FUNCTION_LINE_INFO("Redirecting \"%s\" to \"%s\", mode: \"%s\"", targetPath.c_str(), replacementDir, layerType == FS_LAYER_TYPE_AOC_MERGE ? "merge" : "replace");
        if (layerType == FS_LAYER_TYPE_AOC_MERGE) {
            ptr = make_unique_nothrow<FSWrapperMergeDirsWithParent>(layerName, targetPath.c_str(), replacementDir, true);
        } else {
            ptr = make_unique_nothrow<FSWrapper>(layerName, targetPath.c_str(), replacementDir, false, false);
        }
    } else if (layerType == FS_LAYER_TYPE_SAVE_REPLACE) {
        DEBUG_FUNCTION_LINE_INFO("Redirecting \"/vol/save\" to \"%s\", mode: \"replace\"", replacementDir);
        ptr = make_unique_nothrow<FSWrapper>(layerName, "/vol/save", replacementDir, false, true);
    } else if (layerType == FS_LAYER_TYPE_SAVE_REPLACE_FOR_CURRENT_USER) {
        nn::act::Initialize();
        nn::act::PersistentId persistentId = nn::act::GetPersistentId();
        nn::act::Finalize();

        std::string user = string_format("/vol/save/%08X", 0x80000000 | persistentId);

        DEBUG_FUNCTION_LINE_INFO("Redirecting \"%s\" to \"%s\", mode: \"replace\"", user.c_str(), replacementDir);
        ptr = make_unique_nothrow<FSWrapper>(layerName, user, replacementDir, false, true);
    } else {
        DEBUG_FUNCTION_LINE_ERR("CONTENT_REDIRECTION_API_ERROR_UNKNOWN_LAYER_DIR_TYPE: %s %s %d", layerName, replacementDir, layerType);
        return CONTENT_REDIRECTION_API_ERROR_UNKNOWN_FS_LAYER_TYPE;
    }
    if (ptr) {
        DEBUG_FUNCTION_LINE_VERBOSE("Added new layer (%s). Replacement dir: %s Type:%d", layerName, replacementDir, layerType);
        std::lock_guard<std::mutex> lock(fsLayerMutex);
        *handle = (CRLayerHandle) ptr->getHandle();
        fsLayers.push_back(std::move(ptr));
        return CONTENT_REDIRECTION_API_ERROR_NONE;
    }
    DEBUG_FUNCTION_LINE_ERR("Failed to allocate memory");
    return CONTENT_REDIRECTION_API_ERROR_NO_MEMORY;
}

ContentRedirectionApiErrorType CRRemoveFSLayer(CRLayerHandle handle) {
    if (!remove_locked_first_if(fsLayerMutex, fsLayers, [handle](auto &cur) { return (CRLayerHandle) cur->getHandle() == handle; })) {
        DEBUG_FUNCTION_LINE_WARN("CONTENT_REDIRECTION_API_ERROR_LAYER_NOT_FOUND for handle %08X", handle);
        return CONTENT_REDIRECTION_API_ERROR_LAYER_NOT_FOUND;
    }
    return CONTENT_REDIRECTION_API_ERROR_NONE;
}

ContentRedirectionApiErrorType CRSetActive(CRLayerHandle handle, bool active) {
    std::lock_guard<std::mutex> lock(fsLayerMutex);
    for (auto &cur : fsLayers) {
        if ((CRLayerHandle) cur->getHandle() == handle) {
            cur->setActive(active);
            return CONTENT_REDIRECTION_API_ERROR_NONE;
        }
    }

    DEBUG_FUNCTION_LINE_WARN("CONTENT_REDIRECTION_API_ERROR_LAYER_NOT_FOUND for handle %08X", handle);
    return CONTENT_REDIRECTION_API_ERROR_LAYER_NOT_FOUND;
}

ContentRedirectionApiErrorType CRGetVersion(ContentRedirectionVersion *outVersion) {
    if (outVersion == nullptr) {
        return CONTENT_REDIRECTION_API_ERROR_INVALID_ARG;
    }
    *outVersion = 1;
    return CONTENT_REDIRECTION_API_ERROR_NONE;
}

int CRAddDevice(const devoptab_t *device) {
    return AddDevice(device);
}

int CRRemoveDevice(const char *name) {
    return RemoveDevice(name);
}

WUMS_EXPORT_FUNCTION(CRGetVersion);
WUMS_EXPORT_FUNCTION(CRAddFSLayer);
WUMS_EXPORT_FUNCTION(CRRemoveFSLayer);
WUMS_EXPORT_FUNCTION(CRSetActive);
WUMS_EXPORT_FUNCTION(CRAddDevice);
WUMS_EXPORT_FUNCTION(CRRemoveDevice);