#include "FSWrapper.h"
#include "FSWrapperMergeDirsWithParent.h"
#include "FileUtils.h"
#include "IFSWrapper.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include <content_redirection/redirection.h>
#include <mutex>
#include <wums/exports.h>

ContentRedirectionApiErrorType CRAddFSLayer(CRLayerHandle *handle, const char *layerName, const char *replacementDir, FSLayerType layerType) {
    if (!handle || layerName == nullptr || replacementDir == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("CONTENT_REDIRECTION_API_ERROR_INVALID_ARG");
        return CONTENT_REDIRECTION_API_ERROR_INVALID_ARG;
    }
    std::unique_ptr<IFSWrapper> ptr;
    if (layerType == FS_LAYER_TYPE_CONTENT_REPLACE) {
        ptr = make_unique_nothrow<FSWrapper>(layerName, "/vol/content", replacementDir, false, false);
    } else if (layerType == FS_LAYER_TYPE_CONTENT_MERGE) {
        ptr = make_unique_nothrow<FSWrapperMergeDirsWithParent>(layerName, "/vol/content", replacementDir, true);
    } else if (layerType == FS_LAYER_TYPE_SAVE_REPLACE) {
        ptr = make_unique_nothrow<FSWrapper>(layerName, "/vol/save", replacementDir, false, true);
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
    if (remove_locked_first_if(fsLayerMutex, fsLayers, [handle](auto &cur) { return (CRLayerHandle) cur->getHandle() == handle; })) {
        DEBUG_FUNCTION_LINE_ERR("CONTENT_REDIRECTION_API_ERROR_LAYER_NOT_FOUND for handle %08X", handle);
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

    DEBUG_FUNCTION_LINE_ERR("CONTENT_REDIRECTION_API_ERROR_LAYER_NOT_FOUND for handle %08X", handle);
    return CONTENT_REDIRECTION_API_ERROR_LAYER_NOT_FOUND;
}

ContentRedirectionApiErrorType CRGetVersion(ContentRedirectionVersion *outVersion) {
    if (outVersion == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("CONTENT_REDIRECTION_API_ERROR_INVALID_ARG");
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