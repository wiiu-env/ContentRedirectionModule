#include "FSDirReplacements.h"
#include "FSFileReplacements.h"
#include "FileUtils.h"
#include "utils/logger.h"
#include <wums.h>

WUMS_MODULE_EXPORT_NAME("homebrew_content_redirection");
WUMS_USE_WUT_DEVOPTAB();

WUMS_INITIALIZE() {
    initLogging();
    DEBUG_FUNCTION_LINE("Patch functions");
    for (uint32_t i = 0; i < fs_file_function_replacements_size; i++) {
        if (!FunctionPatcherPatchFunction(&fs_file_function_replacements[i], nullptr)) {
            OSFatal("homebrew_content_redirection: Failed to patch function");
        }
    }
    for (uint32_t i = 0; i < fs_dir_function_replacements_size; i++) {
        if (!FunctionPatcherPatchFunction(&fs_dir_function_replacements[i], nullptr)) {
            OSFatal("homebrew_content_redirection: Failed to patch function");
        }
    }
    DEBUG_FUNCTION_LINE("Patch functions finished");
    deinitLogging();
}

WUMS_APPLICATION_STARTS() {
    initLogging();
}

WUMS_APPLICATION_ENDS() {
    clearFSLayer();
    deinitLogging();
}