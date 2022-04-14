#include "FSDirReplacements.h"
#include "FSFileReplacements.h"
#include "FileUtils.h"
#include "utils/logger.h"
#include <coreinit/memorymap.h>
#include <coreinit/title.h>
#include <sysapp/title.h>
#include <wums.h>

WUMS_MODULE_EXPORT_NAME("homebrew_content_redirection");
WUMS_USE_WUT_DEVOPTAB();

WUMS_INITIALIZE() {
    initLogging();
    DEBUG_FUNCTION_LINE("Patch functions");
    // we only patch static functions, we don't need re-patch them at every launch
    FunctionPatcherPatchFunction(fs_file_function_replacements, fs_file_function_replacements_size);
    FunctionPatcherPatchFunction(fs_dir_function_replacements, fs_dir_function_replacements_size);
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