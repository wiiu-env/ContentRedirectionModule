#include "FSAReplacements.h"
#include "FSReplacements.h"
#include "FileUtils.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "version.h"
#include <content_redirection/redirection.h>
#include <mocha/mocha.h>
#include <sys/unistd.h>
#include <wums.h>

WUMS_MODULE_EXPORT_NAME("homebrew_content_redirection");
WUMS_USE_WUT_DEVOPTAB();
WUMS_DEPENDS_ON(homebrew_functionpatcher);

#define VERSION "v0.2.8"

DECL_FUNCTION(void, OSCancelThread, OSThread *thread) {
    auto upid = OSGetUPID();
    if (!sLayerInfoForUPID.contains(upid)) {
        DEBUG_FUNCTION_LINE_ERR("invalid UPID %d", upid);
        OSFatal("Invalid UPID.");
    }

    auto &layerInfo = sLayerInfoForUPID[upid];
    if (thread == layerInfo->threadData[0].thread || thread == layerInfo->threadData[1].thread || thread == layerInfo->threadData[2].thread) {
        DEBUG_FUNCTION_LINE_INFO("Prevent calling OSCancelThread for ContentRedirection IO Threads");
        return;
    }
    real_OSCancelThread(thread);
}

function_replacement_data_t OSCancelThreadReplacement = REPLACE_FUNCTION(OSCancelThread, LIBRARY_COREINIT, OSCancelThread);


extern ContentRedirectionApiErrorType CRAddFSLayerEx2(CRLayerHandle *handle, const char *layerName, const char *targetPath, const char *replacementPath, FSLayerTypeEx layerType, uint32_t upid);

static CRLayerHandle browser_test_handle;
WUMS_INITIALIZE() {
    initLogging();
    DEBUG_FUNCTION_LINE("Patch functions");
    if (FunctionPatcher_InitLibrary() != FUNCTION_PATCHER_RESULT_SUCCESS) {
        OSFatal("homebrew_content_redirection: FunctionPatcher_InitLibrary failed");
    }

    int mochaInitResult;
    if ((mochaInitResult = Mocha_InitLibrary()) != MOCHA_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Mocha_InitLibrary() failed %d", mochaInitResult);
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

    // Give UPID 2 (Wii U Menu) and UPID 15 the same layer
    const auto layerInfoGameMenu = make_shared_nothrow<LayerInfo>();
    sLayerInfoForUPID[2]         = layerInfoGameMenu;
    sLayerInfoForUPID[15]        = layerInfoGameMenu;

    // Fill in for all other UPIDs
    for (int i = 0; i < 16; i++) {
        if (i == 2 || i == 15) {
            continue;
        }
        sLayerInfoForUPID[i] = make_shared_nothrow<LayerInfo>();
    }

    if (CRAddFSLayerEx2(&browser_test_handle, "browser_applet_test", "/vol/content", "fs_applet:/content", FS_LAYER_TYPE_EX_MERGE_DIRECTORY, 8) != CONTENT_REDIRECTION_API_ERROR_NONE) {
        DEBUG_FUNCTION_LINE_ERR("Failed to add applet layer");
    }


    DEBUG_FUNCTION_LINE("Patch functions finished");
    deinitLogging();
}

WUMS_APPLICATION_STARTS() {
    OSReport("Running ContentRedirectionModule " VERSION VERSION_EXTRA "\n");
    initLogging();
    startFSIOThreadsForCurrentUPID();

    if (Mocha_MountFSEx("fs_applet2", "/dev/sdcard01", "/vol/external01", FSA_MOUNT_FLAG_LOCAL_MOUNT, nullptr, 0) != MOCHA_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to mount sd card");
    } else {
        chdir("fs_applet2:/");
        struct stat st = {};
        DEBUG_FUNCTION_LINE_ERR("1");
        stat("fs_applet2:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("2");
        stat("/content/pages/index.html", &st);
        stat("content/pages/index.html", &st);
        chdir("fs_applet2:/content");
        DEBUG_FUNCTION_LINE_ERR("3");
        stat("fs_applet2:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("4");
        stat("/content/pages/index.html", &st);
        stat("pages/index.html", &st);
        chdir("..");
        DEBUG_FUNCTION_LINE_ERR("5");
        stat("fs_applet2:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("6");
        stat("/content/pages/index.html", &st);
        stat("content/pages/index.html", &st);
        chdir(".");
        DEBUG_FUNCTION_LINE_ERR("7");
        stat("fs_applet2:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("8");
        stat("/content/pages/index.html", &st);
        stat("content/pages/index.html", &st);
        chdir("fs_applet2:/content/pages");
        DEBUG_FUNCTION_LINE_ERR("9");
        stat("../pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("10");
        stat("../../content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("11");
        stat("../../content/pages/./index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("12");
        stat("./../../content/pages/./index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("13");
        stat("./../.././content/./pages/index.html", &st);
        stat("..////..////////////////content/./pages/index.html", &st);
        chdir("fs:/");
        Mocha_UnmountFS("fs_applet2");
    }

    if (Mocha_MountFSEx("fs_applet3", "/dev/sdcard01", "/vol/external01\\", FSA_MOUNT_FLAG_LOCAL_MOUNT, nullptr, 0) != MOCHA_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to mount sd card");
    } else {
        chdir("fs_applet3:/");
        struct stat st = {};
        DEBUG_FUNCTION_LINE_ERR("1");
        stat("fs_applet3:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("2");
        stat("/content/pages/index.html", &st);
        stat("content/pages/index.html", &st);
        chdir("fs_applet3:/content");
        DEBUG_FUNCTION_LINE_ERR("3");
        stat("fs_applet3:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("4");
        stat("/content/pages/index.html", &st);
        stat("pages/index.html", &st);
        chdir("..");
        DEBUG_FUNCTION_LINE_ERR("5");
        stat("fs_applet3:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("6");
        stat("/content/pages/index.html", &st);
        stat("content/pages/index.html", &st);
        chdir(".");
        DEBUG_FUNCTION_LINE_ERR("7");
        stat("fs_applet3:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("8");
        stat("/content/pages/index.html", &st);
        stat("content/pages/index.html", &st);
        chdir("fs_applet3:/content/pages");
        DEBUG_FUNCTION_LINE_ERR("9");
        stat("../pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("10");
        stat("../../content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("11");
        stat("../../content/pages/./index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("12");
        stat("./../../content/pages/./index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("13");
        stat("./../.././content/./pages/index.html", &st);
        stat("..////..////////////////content/./pages/index.html", &st);
        stat("..////..////////////////content/./foo/../pages/index.html", &st);
        chdir("fs:/");
        Mocha_UnmountFS("fs_applet3");
    }


    if (Mocha_MountFSEx("fs_applet3", "/dev/sdcard01", R"(\vol\external01\)", FSA_MOUNT_FLAG_LOCAL_MOUNT, nullptr, 0) != MOCHA_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to mount sd card");
    } else {
        chdir("fs_applet3:/");
        struct stat st = {};
        DEBUG_FUNCTION_LINE_ERR("1");
        stat("fs_applet3:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("2");
        stat("/content/pages/index.html", &st);
        stat("content/pages/index.html", &st);
        chdir("fs_applet3:/content");
        DEBUG_FUNCTION_LINE_ERR("3");
        stat("fs_applet3:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("4");
        stat("/content/pages/index.html", &st);
        stat("pages/index.html", &st);
        chdir("..");
        DEBUG_FUNCTION_LINE_ERR("5");
        stat("fs_applet3:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("6");
        stat("/content/pages/index.html", &st);
        stat("content/pages/index.html", &st);
        chdir(".");
        DEBUG_FUNCTION_LINE_ERR("7");
        stat("fs_applet3:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("8");
        stat("/content/pages/index.html", &st);
        stat("content/pages/index.html", &st);
        chdir("fs_applet3:/content/pages");
        DEBUG_FUNCTION_LINE_ERR("9");
        stat("../pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("10");
        stat("../../content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("11");
        stat("../../content/pages/./index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("12");
        stat("./../../content/pages/./index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("13");
        stat("./../.././content/./pages/index.html", &st);
        stat("..////..////////////////content/./pages/index.html", &st);
        stat("..////..////////////////content/./foo/../pages/index.html", &st);
        chdir("fs:/");
        Mocha_UnmountFS("fs_applet3");
    }

    if (Mocha_MountFSEx("fs_applet3", "/dev/sdcard01", R"(\vol/external01\)", FSA_MOUNT_FLAG_LOCAL_MOUNT, nullptr, 0) != MOCHA_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to mount sd card");
    } else {
        chdir("fs_applet3:/");
        struct stat st = {};
        DEBUG_FUNCTION_LINE_ERR("1");
        stat("fs_applet3:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("2");
        stat("/content/pages/index.html", &st);
        stat("content/pages/index.html", &st);
        chdir("fs_applet3:/content");
        DEBUG_FUNCTION_LINE_ERR("3");
        stat("fs_applet3:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("4");
        stat("/content/pages/index.html", &st);
        stat("pages/index.html", &st);
        chdir("..");
        DEBUG_FUNCTION_LINE_ERR("5");
        stat("fs_applet3:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("6");
        stat("/content/pages/index.html", &st);
        stat("content/pages/index.html", &st);
        chdir(".");
        DEBUG_FUNCTION_LINE_ERR("7");
        stat("fs_applet3:/content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("8");
        stat("/content/pages/index.html", &st);
        stat("content/pages/index.html", &st);
        chdir("fs_applet3:/content/pages");
        DEBUG_FUNCTION_LINE_ERR("9");
        stat("../pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("10");
        stat("../../content/pages/index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("11");
        stat("../../content/pages/./index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("12");
        stat("./../../content/pages/./index.html", &st);
        DEBUG_FUNCTION_LINE_ERR("13");
        stat("./../.././content/./pages/index.html", &st);
        stat("..////..////////////////content/./pages/index.html", &st);
        stat("..////..////////////////content/./foo/../pages/index.html", &st);
        chdir("fs:/");
        Mocha_UnmountFS("fs_applet3");
    }
}

WUMS_APPLICATION_ENDS() {
    if (sLayerInfoForUPID.contains(2)) {
        DEBUG_FUNCTION_LINE_ERR("Clear layer for UPID %d", 2);
        clearFSLayer(*sLayerInfoForUPID[2]);
    }

    stopFSIOThreads();

    deinitLogging();
}
