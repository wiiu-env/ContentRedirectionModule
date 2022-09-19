#include "FSAReplacements.h"
#include "FSReplacements.h"
#include "FileUtils.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include "version.h"
#include <coreinit/cache.h>
#include <coreinit/core.h>
#include <malloc.h>
#include <wums.h>

WUMS_MODULE_EXPORT_NAME("homebrew_content_redirection");
WUMS_USE_WUT_DEVOPTAB();

#define VERSION "v0.2.1"

WUMS_INITIALIZE() {
    initLogging();
    DEBUG_FUNCTION_LINE("Patch functions");
    for (uint32_t i = 0; i < fs_file_function_replacements_size; i++) {
        if (!FunctionPatcherPatchFunction(&fs_file_function_replacements[i], nullptr)) {
            OSFatal("homebrew_content_redirection: Failed to patch function");
        }
    }
    for (uint32_t i = 0; i < fsa_file_function_replacements_size; i++) {
        if (!FunctionPatcherPatchFunction(&fsa_file_function_replacements[i], nullptr)) {
            OSFatal("homebrew_content_redirection: Failed to patch function");
        }
    }

    DEBUG_FUNCTION_LINE("Patch functions finished");
    deinitLogging();
}

WUMS_APPLICATION_STARTS() {
    OSReport("Running ContentRedirectionModule " VERSION VERSION_EXTRA "\n");
    initLogging();
    startFSIOThreads();
}

WUMS_APPLICATION_ENDS() {
    clearFSLayer();

    stopFSIOThreads();

    deinitLogging();
}