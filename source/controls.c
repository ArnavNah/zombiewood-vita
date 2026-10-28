#include "controls.h"
#include "config.h"
#include "utils/logger.h"

#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/kernel/clib.h>
#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#include <math.h>
#include <stdio.h>
#include <psp2/kernel/processmgr.h>
#include <string.h>
#include <stdlib.h>
#include <vitaGL.h>
#include "patch.h"

extern so_module so_mod;

// Function pointers to game binary
static void (*GL2JNILib_touchEvent)(JNIEnv *env, jclass clazz, jint action, jint x, jint y, jint id) = NULL;
static void (*Game_nativeOnKeyDown)(JNIEnv *env, jclass clazz, jint keyCode) = NULL;
static void (*Game_nativeOnKeyUp)(JNIEnv *env, jclass clazz, jint keyCode) = NULL;
static void *(*get_app)(void);
static void *(*get_input)(void *);
static void *(*get_gamepad)(void *, unsigned);
static void (*raise_touch)(void *, int, int, unsigned, float, float, bool);
static void **flash_manager;
static void (*handle_ctor)(void *, void *);
static void (*handle_dtor)(void *);
static void (*find_character)(void *, void *, const char *, const void *);
static void (*handle_alpha)(void *, float);
static void *(*handle_character)(const void *);
static bool controller_mode = true;
static float cursor_x = 480, cursor_y = 450;
static bool cursor_visible;
static int cross_x, cross_y;

static bool in_level(void) {
    uint8_t *glf = get_app ? get_app() : NULL;
    if (!glf) return false;
    uint8_t *app = glf - 48;
    uintptr_t *node = *(uintptr_t **)(app + 36);
    if (!node || node == (uintptr_t *)(app + 32) || !node[2]) return false;
    return strcmp((char *)node[2] + 4, "GSLevel") == 0;
}

static void update_stick_graphics(void) {
    static void *last_root;
    static unsigned last_generation;
    static bool last_mode;
    static uint64_t next_refresh;
    if (!in_level() || !flash_manager || !*flash_manager || !find_character) {
        last_root=NULL;
        return;
    }
    uint8_t *fx=*(uint8_t **)((uint8_t *)*flash_manager+36);
    uint8_t *player=fx ? *(uint8_t **)(fx+36) : NULL;
    void *root=player ? *(void **)(player+92) : NULL;
    if (!root) return;
    unsigned generation=game_state_generation();
    uint64_t now=sceKernelGetProcessTimeWide();
    if (root==last_root && generation==last_generation && controller_mode==last_mode && now<next_refresh) return;
    static const char *names[] = {"menu_Hud.myJoystick", "menu_Hud.myTrigger"};
    // APK 1.5.3 CharacterHandle is a nontrivial 36-byte value. ARM EABI
    // passes its return slot first and the by-value default indirectly.
    unsigned found=0;
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t empty[9], result[9];
        handle_ctor(empty, NULL);
        find_character(result, *flash_manager, names[i], empty);
        if (handle_character(result)) {
            handle_alpha(result, controller_mode ? 0.0f : 1.0f);
            ++found;
        }
        handle_dtor(result);
        handle_dtor(empty);
    }
    // Do not retain native handles across HUD destruction. Successful lookups
    // need refreshing only on a new root/state/input mode, with a periodic
    // retry for Flash timeline changes. Missing HUDs retry next frame.
    if (found==2) {
        last_root=root;last_generation=generation;last_mode=controller_mode;
        next_refresh=now+2000000;
    }
}

static void screen_touch(JNIEnv *env, jclass clazz, jint action, jint x, jint y, jint pointer) {
    if (!get_app || !get_input || !get_gamepad || !raise_touch) return;
    void *app = get_app();
    void *input = app ? get_input(app) : NULL;
    void *pad = input ? get_gamepad(input,0) : NULL;
    if (!pad) return;
    // Vita coordinates already use the game's logical 960x544 screen.
    // Android's JNI entry applies device-density/orientation conversion again.
    raise_touch(pad,0,action == 1 ? 214 : action == 2 ? 215 : 216,
        pointer,(float)x,(float)y,false);
}

typedef struct {
    uint32_t sce_button;
    uint32_t android_button;
} ButtonMapping;

static ButtonMapping s_mappings[] = {
    { SCE_CTRL_UP,        AKEYCODE_DPAD_UP },
    { SCE_CTRL_DOWN,      AKEYCODE_DPAD_DOWN },
    { SCE_CTRL_LEFT,      AKEYCODE_DPAD_LEFT },
    { SCE_CTRL_RIGHT,     AKEYCODE_DPAD_RIGHT },
    { SCE_CTRL_CROSS,     AKEYCODE_DPAD_CENTER },
    { SCE_CTRL_CIRCLE,    AKEYCODE_BACK },
    // This APK recognizes Xperia L1/R1, but ignores Android BUTTON_X/Y.
    { SCE_CTRL_SQUARE,    AKEYCODE_BUTTON_L1 },
    { SCE_CTRL_TRIANGLE,  AKEYCODE_BUTTON_R1 },
    { SCE_CTRL_L1,        AKEYCODE_BUTTON_L1 },
    { SCE_CTRL_R1,        AKEYCODE_BUTTON_R1 },
    { SCE_CTRL_START,     AKEYCODE_BUTTON_START }
};

static uint32_t s_old_buttons = 0;
static SceTouchData s_touch_old;
static int s_touch_ids[6] = {-1,-1,-1,-1,-1,-1};

static int touch_pointer(unsigned id, bool create) {
    for (int i=0;i<6;i++) if (s_touch_ids[i] == (int)id) return i+2;
    if (create) for (int i=0;i<6;i++) if (s_touch_ids[i] < 0) {s_touch_ids[i]=(int)id;return i+2;}
    return -1;
}

// Stick virtual touch simulation coordinates for dual sticks
#define L_STICK_CENTER_X 110.0f
#define L_STICK_CENTER_Y 475.0f
#define L_STICK_RADIUS   55.0f

#define R_STICK_CENTER_X 850.0f
#define R_STICK_CENTER_Y 475.0f
#define R_STICK_RADIUS   55.0f

#define STICK_DEADZONE   0.20f

static bool s_lstick_active = false;
static bool s_rstick_active = false;
static int last_left_x,last_left_y,last_right_x,last_right_y;

static void normalize_stick(float *x, float *y, float deadzone) {
    float mag = sqrtf((*x * *x) + (*y * *y));
    if (mag < deadzone) {
        *x = 0.0f;
        *y = 0.0f;
        return;
    }
    float factor = (mag - deadzone) / (1.0f - deadzone);
    if (factor > 1.0f) factor = 1.0f;
    *x = (*x / mag) * factor;
    *y = (*y / mag) * factor;
}

void controls_init(void) {
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, 1);

    get_app = (void *)so_symbol(&so_mod,"_ZN3glf3App11GetInstanceEv");
    get_input = (void *)so_symbol(&so_mod,"_ZN3glf3App11GetInputMgrEv");
    get_gamepad = (void *)so_symbol(&so_mod,"_ZN3glf12InputManager10GetGamepadEj");
    raise_touch = (void *)so_symbol(&so_mod,"_ZN3glf7Gamepad15RaiseTouchEventEiNS_14InputEventTypeEjffb");
    GL2JNILib_touchEvent = get_app && get_input && get_gamepad && raise_touch ? screen_touch : NULL;
    Game_nativeOnKeyDown = (void *)so_symbol(&so_mod, "Java_com_gameloft_android_ANMP_GloftZOHM_Game_nativeOnKeyDown");
    Game_nativeOnKeyUp = (void *)so_symbol(&so_mod, "Java_com_gameloft_android_ANMP_GloftZOHM_Game_nativeOnKeyUp");
    flash_manager = (void *)so_symbol(&so_mod, "_ZN13SingletonFastI12FlashManagerE10s_instanceE");
    handle_ctor = (void *)so_symbol(&so_mod, "_ZN7gameswf15CharacterHandleC1EPNS_9CharacterE");
    handle_dtor = (void *)so_symbol(&so_mod, "_ZN7gameswf15CharacterHandleD1Ev");
    find_character = (void *)so_symbol(&so_mod, "_ZN12FlashManager13FindCharacterEPKcN7gameswf15CharacterHandleE");
    handle_alpha = (void *)so_symbol(&so_mod, "_ZN7gameswf15CharacterHandle8setAlphaEf");
    handle_character = (void *)so_symbol(&so_mod, "_ZNK7gameswf15CharacterHandle12getCharacterEv");
    if (!handle_ctor || !handle_dtor || !handle_alpha || !handle_character) find_character = NULL;

    // Engine joystick activation happens after native game initialization.
}

static void poll_touch(void) {
    if (!GL2JNILib_touchEvent) return;

    SceTouchData touch = {0};
    if (sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) < 1) return;
    if (touch.reportNum) { controller_mode = false; cursor_visible = false; }

    for (int i = 0; i < touch.reportNum; i++) {
        int pointer = touch_pointer(touch.report[i].id, true);
        if (pointer < 0) continue;
        int x = (int)((float)touch.report[i].x * 960.0f / 1920.0f);
        int y = (int)((float)touch.report[i].y * 544.0f / 1088.0f);
        int finger_down = 0;

        for (int j = 0; j < s_touch_old.reportNum; j++) {
            if (touch.report[i].id == s_touch_old.report[j].id) {
                finger_down = 1;
                break;
            }
        }

        if (!finger_down) {
            GL2JNILib_touchEvent(&jni, (jclass)0x42424242, 1, x, y, pointer); // DOWN
        } else if (touch.report[i].x != s_touch_old.report[0].x || touch.report[i].y != s_touch_old.report[0].y || s_touch_old.reportNum != 1) {
            GL2JNILib_touchEvent(&jni, (jclass)0x42424242, 2, x, y, pointer); // MOVE
        }
    }

    for (int i = 0; i < s_touch_old.reportNum; i++) {
        int finger_up = 1;
        for (int j = 0; j < touch.reportNum; j++) {
            if (touch.report[j].id == s_touch_old.report[i].id) {
                finger_up = 0;
                break;
            }
        }
        if (finger_up) {
            int pointer = touch_pointer(s_touch_old.report[i].id, false);
            if (pointer < 0) continue;
            int x = (int)((float)s_touch_old.report[i].x * 960.0f / 1920.0f);
            int y = (int)((float)s_touch_old.report[i].y * 544.0f / 1088.0f);
            GL2JNILib_touchEvent(&jni, (jclass)0x42424242, 0, x, y, pointer); // UP
            s_touch_ids[pointer-2] = -1;
        }
    }

    sceClibMemcpy(&s_touch_old, &touch, sizeof(touch));
}

static void poll_gamepad(void) {
    SceCtrlData pad = {0};
    if (sceCtrlPeekBufferPositive(0, &pad, 1) < 1) return;
#if defined(DEBUG_SOLOADER) || defined(PORT_MISSION_TRACE) || defined(PORT_TEST_INPUT)
    static unsigned polls;
    static uint64_t test_until;
    static SceCtrlData test_pad;
    uint64_t now = sceKernelGetProcessTimeWide();
    if (++polls % 15 == 0) {
        FILE *command = fopen(DATA_PATH "control-test.txt","r");
        if (command) {
            unsigned buttons,lx,ly,rx,ry,milliseconds;
            int valid = fscanf(command,"%x %u %u %u %u %u",&buttons,&lx,&ly,&rx,&ry,&milliseconds);
            fclose(command);
            remove(DATA_PATH "control-test.txt");
            if (valid == 6 && lx <= 255 && ly <= 255 && rx <= 255 && ry <= 255 && milliseconds <= 5000) {
                test_pad=(SceCtrlData){.buttons=buttons,.lx=lx,.ly=ly,.rx=rx,.ry=ry};
                test_until=now+(uint64_t)milliseconds*1000;
                l_info("[INPUT TEST] buttons=%X left=%u,%u right=%u,%u duration=%u",buttons,lx,ly,rx,ry,milliseconds);
            }
        }
    }
    if (now < test_until) pad=test_pad;
    if (polls % 15 == 0 && GL2JNILib_touchEvent) {
        static int touch_x, touch_y;
        static uint64_t touch_release;
        if (touch_release && now >= touch_release) {
            GL2JNILib_touchEvent(&jni,(jclass)0x42424242,0,touch_x,touch_y,9);
            touch_release=0;
        }
        FILE *command=fopen(DATA_PATH "touch-test.txt","r");
        if (command) {
            int x,y;
            int valid=fscanf(command,"%d %d",&x,&y);
            fclose(command);
            remove(DATA_PATH "touch-test.txt");
            if (valid == 2 && x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H && !touch_release) {
                touch_x=x; touch_y=y;
                GL2JNILib_touchEvent(&jni,(jclass)0x42424242,1,x,y,9);
                touch_release=now+150000;
            }
        }
    }
#endif
    static SceCtrlData previous;
    if (pad.buttons != previous.buttons || pad.lx != previous.lx || pad.ly != previous.ly || pad.rx != previous.rx || pad.ry != previous.ry) {
        l_info("[INPUT] buttons=%08X left=%u,%u right=%u,%u",pad.buttons,pad.lx,pad.ly,pad.rx,pad.ry);
        previous = pad;
    }

    uint32_t current_buttons = pad.buttons;
    uint32_t pressed_buttons = current_buttons & ~s_old_buttons;
    uint32_t released_buttons = ~current_buttons & s_old_buttons;
    bool level = in_level();
    if ((current_buttons & ~SCE_CTRL_SELECT) || abs((int)pad.lx-128) > 28 || abs((int)pad.ly-128) > 28 ||
        abs((int)pad.rx-128) > 28 || abs((int)pad.ry-128) > 28) controller_mode = true;
    if (pressed_buttons & SCE_CTRL_SELECT) controller_mode = !controller_mode;

    float lx = ((float)pad.lx - 128.0f) / 128.0f;
    float ly = ((float)pad.ly - 128.0f) / 128.0f;
    normalize_stick(&lx, &ly, STICK_DEADZONE);
    if (current_buttons & SCE_CTRL_LEFT) lx = -1;
    if (current_buttons & SCE_CTRL_RIGHT) lx = 1;
    if (current_buttons & SCE_CTRL_UP) ly = -1;
    if (current_buttons & SCE_CTRL_DOWN) ly = 1;
    if (!level) {
        static uint64_t last_poll;
        uint64_t time = sceKernelGetProcessTimeWide();
        float dt = last_poll ? (time-last_poll)/1000000.0f : 1.0f/60.0f;
        if (dt > 0.05f) dt = 0.05f;
        last_poll = time;
        if (lx || ly) {
            cursor_visible = true;
            cursor_x = fminf(950, fmaxf(10, cursor_x + lx*420*dt));
            cursor_y = fminf(534, fmaxf(10, cursor_y + ly*420*dt));
        }
    } else cursor_visible = false;

    for (size_t i = 0; i < sizeof(s_mappings) / sizeof(ButtonMapping); i++) {
        // The cursor belongs to the game; this never moves the Windows mouse.
        if (s_mappings[i].sce_button == SCE_CTRL_CROSS && GL2JNILib_touchEvent) {
            if (pressed_buttons & SCE_CTRL_CROSS) {
                cross_x = !level && cursor_visible ? (int)cursor_x : 480;
                cross_y = !level && cursor_visible ? (int)cursor_y : 450;
                GL2JNILib_touchEvent(&jni,(jclass)0x42424242,1,cross_x,cross_y,8);
            }
            if (released_buttons & SCE_CTRL_CROSS)
                GL2JNILib_touchEvent(&jni,(jclass)0x42424242,0,cross_x,cross_y,8);
            continue;
        }
        if (s_mappings[i].sce_button & (SCE_CTRL_UP|SCE_CTRL_DOWN|SCE_CTRL_LEFT|SCE_CTRL_RIGHT)) continue;
        if (pressed_buttons & s_mappings[i].sce_button) {
            if (Game_nativeOnKeyDown) {
                Game_nativeOnKeyDown(&jni, (jclass)0x42424242, s_mappings[i].android_button);
            }
        }
        if (released_buttons & s_mappings[i].sce_button) {
            if (Game_nativeOnKeyUp) {
                Game_nativeOnKeyUp(&jni, (jclass)0x42424242, s_mappings[i].android_button);
            }
        }
    }

    s_old_buttons = current_buttons;

    // Dual stick simulation
    if (GL2JNILib_touchEvent) {
        if (!level) lx = ly = 0;
        float magnitude = sqrtf(lx*lx + ly*ly);
        if (magnitude > 1) { lx /= magnitude; ly /= magnitude; }

        if (lx != 0.0f || ly != 0.0f) {
            int tx = (int)(L_STICK_CENTER_X + lx * L_STICK_RADIUS);
            int ty = (int)(L_STICK_CENTER_Y + ly * L_STICK_RADIUS);
            if (!s_lstick_active) {
                GL2JNILib_touchEvent(&jni, (jclass)0x42424242, 1, (int)L_STICK_CENTER_X, (int)L_STICK_CENTER_Y, 0);
                s_lstick_active = true;
                last_left_x=(int)L_STICK_CENTER_X;last_left_y=(int)L_STICK_CENTER_Y;
            } else if (tx!=last_left_x || ty!=last_left_y) {
                GL2JNILib_touchEvent(&jni, (jclass)0x42424242, 2, tx, ty, 0);
                last_left_x=tx;last_left_y=ty;
            }
        } else if (s_lstick_active) {
            GL2JNILib_touchEvent(&jni, (jclass)0x42424242, 0, (int)L_STICK_CENTER_X, (int)L_STICK_CENTER_Y, 0);
            s_lstick_active = false;
        }

        float rx = ((float)pad.rx - 128.0f) / 128.0f;
        float ry = ((float)pad.ry - 128.0f) / 128.0f;
        normalize_stick(&rx, &ry, STICK_DEADZONE);
        if (!level) rx = ry = 0;

        if (rx != 0.0f || ry != 0.0f) {
            int tx = (int)(R_STICK_CENTER_X + rx * R_STICK_RADIUS);
            int ty = (int)(R_STICK_CENTER_Y + ry * R_STICK_RADIUS);
            if (!s_rstick_active) {
                GL2JNILib_touchEvent(&jni, (jclass)0x42424242, 1, (int)R_STICK_CENTER_X, (int)R_STICK_CENTER_Y, 1);
                s_rstick_active = true;
                last_right_x=(int)R_STICK_CENTER_X;last_right_y=(int)R_STICK_CENTER_Y;
            } else if (tx!=last_right_x || ty!=last_right_y) {
                GL2JNILib_touchEvent(&jni, (jclass)0x42424242, 2, tx, ty, 1);
                last_right_x=tx;last_right_y=ty;
            }
        } else if (s_rstick_active) {
            GL2JNILib_touchEvent(&jni, (jclass)0x42424242, 0, (int)R_STICK_CENTER_X, (int)R_STICK_CENTER_Y, 1);
            s_rstick_active = false;
        }
    }
}

void controls_poll(void) {
    poll_touch();
    poll_gamepad();
    update_stick_graphics();
}

void controls_draw_cursor(void) {
    if (!cursor_visible || in_level()) return;
    GLint scissor[4], framebuffer;
    GLfloat color[4]; GLboolean mask[4];
    GLboolean enabled = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_SCISSOR_BOX, scissor);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, color);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(1, 0.85f, 0.1f, 1);
    int x = (int)cursor_x, y = SCREEN_H - (int)cursor_y;
    glScissor(x-8, y-1, 17, 3); glClear(GL_COLOR_BUFFER_BIT);
    glScissor(x-1, y-8, 3, 17); glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(color[0], color[1], color[2], color[3]);
    glColorMask(mask[0], mask[1], mask[2], mask[3]);
    glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
    if (!enabled) glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
}
