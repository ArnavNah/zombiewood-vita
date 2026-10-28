#include "config.h"
#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "controls.h"
#include "mission_trace.h"
#include "patch.h"
#include "presentation.h"
#include "reimpl/io.h"
#include "performance.h"
#include "reimpl/audio_track.h"
extern GLboolean gl_frame_has_scene(void);

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>
#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#include <pthread.h>
#include <string.h>

int _newlib_heap_size_user = 256 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;
#endif

so_module so_mod;

static void log_engine_state(void) {
    const char *flags[] = {"g_appAlive", "g_appPaused", "g_appInit", "m_timerForResume"};
    for (unsigned i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) {
        uintptr_t address = so_symbol(&so_mod, flags[i]);
        if (address) l_info("[STATE] %s = 0x%08X", flags[i], *(unsigned *)address);
    }
    void *(*get_instance)(void) = (void *)so_symbol(&so_mod, "_ZN3glf3App11GetInstanceEv");
    if (get_instance) {
        void *app = get_instance();
        l_info("[STATE] glf App instance = %p", app);
        if (app) l_info("[STATE] virtual update SO offset = 0x%X", (unsigned)(*(uintptr_t **)app)[9] - (unsigned)so_mod.load_addr);
    }
}

static void *game_thread(void *arg) {
    l_info("Game thread started.");

    io_migrate_legacy_saves();
    io_asset_index_init();
#ifdef PORT_TEST_INPUT
    audio_stream_test();
#endif

    // 1. JNI_OnLoad
    int (*JNI_OnLoad)(void *jvm) = (void *)so_symbol(&so_mod, "JNI_OnLoad");
    if (JNI_OnLoad) {
        l_info("Calling JNI_OnLoad...");
        if (JNI_OnLoad(&jvm) != JNI_VERSION_1_4) {
            l_fatal("JNI_OnLoad failed");
            return NULL;
        }
    }

    // JNI_OnLoad already calls AndroidOS_JNIOnload in Android v1.5.3.

    // The Android installer expects its own Activity and initialized installer
    // state; without that state nativeStart calls exit(0). Vita installs data
    // separately and enters the native game directly.

    // 4. Set paths for data and OBB files
    void (*GL2JNILib_setPaths)(JNIEnv *env, jclass clazz, jstring p1, jstring p2, jstring p3) = 
        (void *)so_symbol(&so_mod, "Java_com_gameloft_android_ANMP_GloftZOHM_GL2JNILib_setPaths");
    if (GL2JNILib_setPaths) {
        l_info("Calling GL2JNILib_setPaths...");
        jstring data = jni->NewStringUTF(&jni, DATA_PATH);
        jstring main_obb = jni->NewStringUTF(&jni, OBB_MAIN_PATH);
        jstring patch_obb = jni->NewStringUTF(&jni, OBB_PATCH_PATH);
        GL2JNILib_setPaths(&jni, (jclass)0x42424242, data, main_obb, patch_obb);
    }

    // Cache Java callbacks and device paths before creating the GL context.
    void (*Game_nativeInit)(JNIEnv *env, jclass clazz) = 
        (void *)so_symbol(&so_mod, "Java_com_gameloft_android_ANMP_GloftZOHM_Game_nativeInit");
    if (Game_nativeInit) {
        l_info("Calling Game_nativeInit...");
        Game_nativeInit(&jni, (jclass)0x42424242);
    }

    gl_preload();
    gl_init();

    // The active renderer in this APK is GL2JNIView (classes Game/at/as).
    // initGL alone is a two-byte no-op in the supplied Android library.
    void (*GL2JNILib_init)(JNIEnv *, jclass) = (void *)so_symbol(&so_mod,
        "Java_com_gameloft_android_ANMP_GloftZOHM_GL2JNILib_init");
    void (*GL2JNILib_getViewSettings)(JNIEnv *, jclass) = (void *)so_symbol(&so_mod,
        "Java_com_gameloft_android_ANMP_GloftZOHM_GL2JNILib_getViewSettings");
    void (*GL2JNILib_resize)(JNIEnv *, jclass, jint, jint) = (void *)so_symbol(&so_mod,
        "Java_com_gameloft_android_ANMP_GloftZOHM_GL2JNILib_resize");
    void (*GL2JNILib_stateChanged)(JNIEnv *, jclass, jboolean) = (void *)so_symbol(&so_mod,
        "Java_com_gameloft_android_ANMP_GloftZOHM_GL2JNILib_stateChanged");
    if (!GL2JNILib_init || !GL2JNILib_getViewSettings || !GL2JNILib_resize || !GL2JNILib_stateChanged) {
        l_fatal("Missing required GL2JNI lifecycle entry points");
        return NULL;
    }
    l_info("Calling GL2JNILib_init...");
    GL2JNILib_init(&jni, (jclass)0x42424242);
    l_info("Calling GL2JNILib_getViewSettings...");
    GL2JNILib_getViewSettings(&jni, (jclass)0x42424242);
    void (*GL2JNILib_setNumExtraContext)(JNIEnv *, jclass, jint) = (void *)so_symbol(&so_mod,
        "Java_com_gameloft_android_ANMP_GloftZOHM_GL2JNILib_setNumExtraContext");
    if (GL2JNILib_setNumExtraContext)
        GL2JNILib_setNumExtraContext(&jni, (jclass)0x42424242, 0);
    GL2JNILib_resize(&jni, (jclass)0x42424242, SCREEN_W, SCREEN_H);
    GL2JNILib_stateChanged(&jni, (jclass)0x42424242, JNI_TRUE);

    void (*GL2JNILib_resume)(JNIEnv *, jclass) = (void *)so_symbol(&so_mod,
        "Java_com_gameloft_android_ANMP_GloftZOHM_GL2JNILib_resume");
    if (GL2JNILib_resume) {
        l_info("Calling GL2JNILib_resume...");
        GL2JNILib_resume(&jni, (jclass)0x42424242);
    }

    // 7. Renderer init and resize to 960x544
    // Signature verified in classes.dex: (IIILjava/lang/String;)V.
    void (*GameRenderer_nativeInit)(JNIEnv *env, jclass clazz, jint format, jint w, jint h, jstring path) =
        (void *)so_symbol(&so_mod, "Java_com_gameloft_android_ANMP_GloftZOHM_GameRenderer_nativeInit");
    if (GameRenderer_nativeInit) {
        l_info("Calling GameRenderer_nativeInit...");
        GameRenderer_nativeInit(&jni, (jclass)0x42424242, 0, SCREEN_W, SCREEN_H,
            jni->NewStringUTF(&jni, DATA_PATH));
    }

    void (*GameRenderer_nativeResize)(JNIEnv *env, jclass clazz, jint w, jint h) = 
        (void *)so_symbol(&so_mod, "Java_com_gameloft_android_ANMP_GloftZOHM_GameRenderer_nativeResize");
    if (GameRenderer_nativeResize) {
        l_info("Calling GameRenderer_nativeResize(960, 544)...");
        GameRenderer_nativeResize(&jni, (jclass)0x42424242, SCREEN_W, SCREEN_H);
    }

    void (*GL2JNILib_step)(JNIEnv *env, jclass clazz) =
        (void *)so_symbol(&so_mod, "Java_com_gameloft_android_ANMP_GloftZOHM_GL2JNILib_step");
    void (*GameRenderer_nativeRender)(JNIEnv *env, jclass clazz) = 
        (void *)so_symbol(&so_mod, "Java_com_gameloft_android_ANMP_GloftZOHM_GameRenderer_nativeRender");

    l_success("Initialization complete! Starting main game loop.");

    const uint32_t frame_delta = 1000000 / DEFAULT_FPS;
    uint64_t frame_deadline = sceKernelGetProcessTimeWide();
    unsigned frames = 0;

    while (1) {
        performance_begin();
        presentation_begin_frame();
        unsigned state_before = game_state_generation();
        if (!frames) l_info("First frame: polling input");
        controls_poll();
        performance_input_done();

        if (frames < 6) { log_engine_state(); l_info("Frame %u: calling GL2 renderer", frames); }
        if (GL2JNILib_step) {
            GL2JNILib_step(&jni, (jclass)0x42424242);
        }

        performance_game_done();
        bool scene_drawn = gl_frame_has_scene();
        GLboolean display_written = gl_trace_frame(frames);
        bool present = presentation_should_present(scene_drawn, display_written,
                             state_before == game_state_generation());
        if (present && strcmp(game_state_name(), "GSLoading")) controls_draw_cursor();
        if (frames < 5) l_info("Frame %u: native renderer returned, calling gl_swap", frames);
        // State transitions can intentionally skip rendering while the next
        // scene initializes. Keep the last loading image in that case rather
        // than exposing an older image from another swapchain buffer.
        if (present || mission_trace_force_present()) {
            gl_swap();
            presentation_after_swap();
        }
        mission_trace_after_present();
        performance_present_done();
        if (frames < 5) l_info("Frame %u: gl_swap completed", frames);
        ++frames;

        frame_deadline += frame_delta;
        uint64_t now=sceKernelGetProcessTimeWide();
        if (now < frame_deadline) sceKernelDelayThread((unsigned)(frame_deadline-now));
        else frame_deadline=now; // A slow frame must not cause a catch-up burst.
        performance_frame_done();
    }

    return NULL;
}

int main(void) {
    soloader_init_all();

    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8 * 1024 * 1024);
    int err = pthread_create(&t, &attr, game_thread, NULL);
    pthread_attr_destroy(&attr);
    if (err) {
        l_fatal("Could not create game thread: %d", err);
        sceKernelExitProcess(1);
    }
    pthread_join(t, NULL);

    sceKernelExitDeleteThread(0);
    return 0;
}

