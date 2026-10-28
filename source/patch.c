#include "patch.h"
#include "config.h"
#include "utils/logger.h"
#include "mission_trace.h"
#include "performance.h"
#include "presentation.h"

#include <kubridge.h>
#include <so_util/so_util.h>
#include <psp2/kernel/clib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

extern so_module so_mod;
static unsigned state_generation;
static so_hook state_change_hook;
static void *(*state_app)(void);

static int vita_device_profile(void *settings) {
    // Use the game's middle device profile at native resolution. Android's
    // phone heuristics otherwise enable the most expensive effects on Vita.
    // Enemy counts, simulation, and gameplay timing are left to the game.
    return 1;
}

const char *game_state_name(void) {
    uint8_t *glf=state_app ? state_app() : NULL;
    if (!glf) return "";
    uint8_t *app=glf-48;
    uintptr_t *node=*(uintptr_t **)(app+36);
    if (!node || node==(uintptr_t *)(app+32) || !node[2]) return "";
    return (const char *)node[2]+4;
}

unsigned game_state_generation(void) { return state_generation; }

static void application_state_changed(void *app, void *previous, void *next) {
    // OnEnter can warm shader programs without rendering the new state's
    // first image. Let the main loop distinguish that transition iteration.
    ++state_generation;
    SO_CONTINUE(int,state_change_hook,app,previous,next);
}
static void game_console_print(const char *format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    l_info("[ENGINE] %s", message);
}

// Linux ARM kuser helpers are function pointers stored in writable data in
// this binary. Vita has no Linux high-vector page.
static void android_memory_barrier(void) {
    __sync_synchronize();
}

static int android_cmpxchg(int old_value, int new_value, volatile int *ptr) {
    return __sync_val_compare_and_swap(ptr, old_value, new_value) != old_value;
}

static void patch_kuser_literals(uintptr_t base, size_t size) {
    for (size_t i = 0; i + sizeof(uint32_t) <= size; i += sizeof(uint32_t)) {
        uint32_t *word = (uint32_t *)(base + i);
        uint32_t replacement;
        if (*word == 0xFFFF0FA0)
            replacement = (uintptr_t)android_memory_barrier;
        else if (*word == 0xFFFF0FC0)
            replacement = (uintptr_t)android_cmpxchg;
        else
            continue;
#ifdef USE_KUBRIDGE
        kuKernelCpuUnrestrictedMemcpy(word, &replacement, sizeof(replacement));
#else
        sceClibMemcpy(word, &replacement, sizeof(replacement));
#endif
        l_info("Replaced Linux kuser helper at SO offset 0x%X", (unsigned)(base + i - so_mod.load_addr));
    }
}

// DRM Policy nativeAllow hook
static so_hook GDRMPolicy_nativeAllow_hook;
static int GDRMPolicy_nativeAllow_patched(void *env, void *clazz, int a1, int a2, int a3) {
    l_info("GDRMPolicy_nativeAllow called -> forcing ALLOWED (1)");
    return 1; // Always allow
}

// ZombiesGame::SetInternet hook
static so_hook ZombiesGame_SetInternet_hook;
static void ZombiesGame_SetInternet_patched(void *this, bool enable) {
    l_info("ZombiesGame::SetInternet called with %d -> setting offline mode", enable);
    SO_CONTINUE(int, ZombiesGame_SetInternet_hook, this, 0);
}

void so_patch(void) {
    l_info("Applying Bionic ARM patches...");
    mission_trace_init();
    performance_init();
    presentation_init();
    uintptr_t device_profile=so_symbol(&so_mod,"_ZN12GameSettings17GetAndroidProfileEv");
    if (device_profile) hook_addr(device_profile,(uintptr_t)vita_device_profile);
    state_app=(void *)so_symbol(&so_mod,"_ZN3glf3App11GetInstanceEv");
    uintptr_t state_changed=so_symbol(&so_mod,"_ZN11Application13OnStateChangeEP6IStateIS_ES2_");
    if (state_changed)
        state_change_hook=hook_addr(state_changed,(uintptr_t)application_state_changed);

    patch_kuser_literals(so_mod.text_base, so_mod.text_size);
    for (int i = 0; i < so_mod.n_data; ++i)
        patch_kuser_literals(so_mod.data_base[i], so_mod.data_size[i]);

#ifdef DEBUG_SOLOADER
    uintptr_t console = so_symbol(&so_mod, "_ZN3glf7Console7PrintlnEPKcz");
    if (console) hook_addr(console, (uintptr_t)game_console_print);
#endif

    // Hook DRM check
    uintptr_t gdrm_allow = so_symbol(&so_mod, "Java_com_gameloft_android_ANMP_GloftZOHM_installer_GDRMPolicy_nativeAllow");
    if (gdrm_allow) {
        l_info("Hooking GDRMPolicy_nativeAllow at 0x%08X", (unsigned int)gdrm_allow);
        GDRMPolicy_nativeAllow_hook = hook_addr(gdrm_allow, (uintptr_t)&GDRMPolicy_nativeAllow_patched);
    }

    // Hook SetInternet
    uintptr_t set_internet = so_symbol(&so_mod, "_ZN11ZombiesGame11SetInternetEb");
    if (set_internet) {
        l_info("Hooking ZombiesGame::SetInternet at 0x%08X", (unsigned int)set_internet);
        ZombiesGame_SetInternet_hook = hook_addr(set_internet, (uintptr_t)&ZombiesGame_SetInternet_patched);
    }
}

