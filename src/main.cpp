#include "FSAReplacements.h"
#include "FSReplacements.h"
#include "FileUtils.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include "version.h"
#include <wums.h>

WUMS_MODULE_EXPORT_NAME("homebrew_content_redirection");
WUMS_USE_WUT_DEVOPTAB();
WUMS_DEPENDS_ON(homebrew_functionpatcher);

#define VERSION "v0.2.3"

DECL_FUNCTION(void, OSCancelThread, OSThread *thread) {
    if (thread == gThreadData[0].thread || thread == gThreadData[1].thread || thread == gThreadData[2].thread) {
        DEBUG_FUNCTION_LINE_INFO("Prevent calling OSCancelThread for ContentRedirection IO Threads");
        return;
    }
    real_OSCancelThread(thread);
}

function_replacement_data_t OSCancelThreadReplacement = REPLACE_FUNCTION(OSCancelThread, LIBRARY_COREINIT, OSCancelThread);

WUMS_INITIALIZE() {
    initLogging();
    DEBUG_FUNCTION_LINE("Patch functions");
    if (FunctionPatcher_InitLibrary() != FUNCTION_PATCHER_RESULT_SUCCESS) {
        OSFatal("homebrew_content_redirection: FunctionPatcher_InitLibrary failed");
    }

    bool wasPatched;
    for (uint32_t i = 0; i < fs_file_function_replacements_size; i++) {
        wasPatched = false;
        if (FunctionPatcher_AddFunctionPatch(&fs_file_function_replacements[i], nullptr, &wasPatched) != FUNCTION_PATCHER_RESULT_SUCCESS || !wasPatched) {
            OSFatal("homebrew_content_redirection: Failed to patch function");
        }
    }
    for (uint32_t i = 0; i < fsa_file_function_replacements_size; i++) {
        wasPatched = false;
        if (FunctionPatcher_AddFunctionPatch(&fsa_file_function_replacements[i], nullptr, &wasPatched) != FUNCTION_PATCHER_RESULT_SUCCESS || !wasPatched) {
            OSFatal("homebrew_content_redirection: Failed to patch function");
        }
    }
    wasPatched = false;
    if (FunctionPatcher_AddFunctionPatch(&OSCancelThreadReplacement, nullptr, &wasPatched) != FUNCTION_PATCHER_RESULT_SUCCESS || !wasPatched) {
        OSFatal("homebrew_content_redirection: Failed to patch OSCancelThreadReplacement");
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
