#include "presentation.h"
#include "patch.h"
#include <so_util/so_util.h>
#include <kubridge.h>
#include <psp2/kernel/clib.h>
#include <stdint.h>
#include <string.h>
#include <vitaGL.h>
#include "config.h"

extern so_module so_mod;
extern unsigned gl_scene_draw_serial(void);
static so_hook loading_render_hook;
static bool loading_rendered, waiting_for_level;

static void loading_render(void *self, void *app) {
    unsigned before = gl_scene_draw_serial();
    SO_CONTINUE(int, loading_render_hook, self, app);
    // Render may return without drawing. Shader warmup elsewhere also draws
    // triangles, so only accept geometry issued inside the loading renderer.
    if (gl_scene_draw_serial() != before) loading_rendered = true;
}

void presentation_init(void) {
    uintptr_t render = so_symbol(&so_mod, "_ZN9GSLoading6RenderEP11Application");
    if (render) loading_render_hook = hook_addr(render, (uintptr_t)loading_render);
}

void presentation_begin_frame(void) { loading_rendered = false; }

void presentation_after_swap(void) {
    if (!waiting_for_level) return;
    // The APK requests EGL_BUFFER_PRESERVED and draws partial loading updates.
    // Seed the next back buffer from the submitted front buffer, otherwise
    // rotating through Vita's swapchain exposes the previous menu artwork.
    GLint scissor[4];
    GLboolean enabled = glIsEnabled(GL_SCISSOR_TEST), mask[4];
    glGetIntegerv(GL_SCISSOR_BOX, scissor);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glReadBuffer(GL_FRONT);
    glBlitNamedFramebuffer(0, 0, 0, 0, SCREEN_W, SCREEN_H,
                          0, 0, SCREEN_W, SCREEN_H,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glReadBuffer(GL_BACK);
    glColorMask(mask[0], mask[1], mask[2], mask[3]);
    glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
    if (enabled) glEnable(GL_SCISSOR_TEST);
}

bool presentation_should_present(bool scene_drawn, bool display_written,
                                 bool state_stable) {
    const char *state = game_state_name();
    if (!strcmp(state, "GSLoading")) {
        waiting_for_level = true;
        // Preserve the original background, Flash artwork and timeline.
        // Hold the submitted image when resource setup does not render one.
        return state_stable && loading_rendered;
    }
    if (waiting_for_level && !strcmp(state, "GSLevel")) {
        if (!state_stable || !scene_drawn) return false;
    }
    waiting_for_level = false;
    return state_stable && display_written;
}
