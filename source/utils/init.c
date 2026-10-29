/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021-2022 Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/init.h"
#include "config.h"
#include "java.h"

#include "utils/dialog.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "utils/settings.h"

#include <reimpl/controls.h>

#include <string.h>
#include <sys/stat.h>

#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/kernel/clib.h>
#include <psp2/power.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#include <fios/fios.h>
#include <kubridge.h>

// Base address for the Android .so to be loaded at
#define LOAD_ADDRESS 0x98000000

extern so_module so_mod;

void soloader_init_all() {
	// Launch `app0:configurator.bin` on `-config` init param
    sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
    SceAppUtilAppEventParam eventParam;
    sceClibMemset(&eventParam, 0, sizeof(SceAppUtilAppEventParam));
    sceAppUtilReceiveAppEvent(&eventParam);
    if (eventParam.type == 0x05) {
        char buffer[2048];
        sceAppUtilAppEventParseLiveArea(&eventParam, buffer);
        if (strstr(buffer, "-config"))
            sceAppMgrLoadExec("app0:/configurator.bin", NULL, NULL);
    }

    // Set default overclock values
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

#ifdef USE_SCELIBC_IO
    if (fios_init(DATA_PATH) == 0)
        l_success("FIOS initialized.");
#endif

#ifdef USE_KUBRIDGE
    if (!module_loaded("kubridge")) {
        l_warn("kubridge module query failed. Testing kuKernel memory operations...");
        uint32_t src = 0x12345678, dst = 0;
        if (kuKernelCpuUnrestrictedMemcpy(&dst, &src, sizeof(uint32_t)) < 0 || dst != 0x12345678) {
            hardware_checkpoint("kuBridge unavailable");
            fatal_error("kuBridge is not available. Install kubridge.skprx under *KERNEL, reboot the Vita, and enable unsafe homebrew.");
            return;
        } else {
            l_success("kuKernel memory test passed.");
        }
    } else {
        l_success("kubridge check passed.");
    }
#endif

    if (!file_exists(SO_PATH)) {
        fatal_error("Looks like you haven't installed the data files for this "
                    "port, or they are in an incorrect location. Please make "
                    "sure that you have %s file exactly at that path.", SO_PATH);
    }

    const char *required_assets[] = {OBB_MAIN_PATH, OBB_PATCH_PATH,
        DATA_PATH "swfs.bar", DATA_PATH "assets/colors.conf"};
    for (unsigned i=0; i<sizeof(required_assets)/sizeof(required_assets[0]); ++i) {
        struct stat asset;
        if (stat(required_assets[i], &asset)<0 || asset.st_size<=0) {
            hardware_checkpoint("required game data missing or empty");
            fatal_error("Missing or empty game data: %s. Copy the complete data folder to ux0:.", required_assets[i]);
            return;
        }
    }
    hardware_checkpoint("required data paths checked");

    int so_res = so_file_load(&so_mod, SO_PATH, LOAD_ADDRESS);
    if (so_res < 0) {
        l_fatal("SO could not be loaded: 0x%08X", so_res);
        fatal_error("Error: could not load %s (0x%08X).", SO_PATH, so_res);
    }
    hardware_checkpoint("Android library loaded");

    settings_load();
    l_success("Settings loaded.");

    so_relocate(&so_mod);
    l_success("SO relocated.");

    resolve_imports(&so_mod);
    l_success("SO imports resolved.");

    so_patch();
    l_success("SO patched.");

    so_flush_caches(&so_mod);
    hardware_checkpoint("Android relocations imports and patches ready");
    l_success("SO caches flushed.");

    jni_init();
    java_fields_init();
    hardware_checkpoint("JNI bridge ready");
    l_success("FalsoJNI initialized.");

    so_initialize(&so_mod);
    hardware_checkpoint("Android constructors complete");
    l_success("SO initialized.");

#ifndef NDK_PORT
    controls_init();
    l_success("Controls initialized.");
#endif
}
