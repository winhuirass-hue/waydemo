// waydemo.c
// Wayland + libdecor + EGL + Runara demo (2026)
//
// build:
// cc waydemo.c -o waydemo \
//   `pkg-config --cflags --libs wayland-client libdecor-0 egl glesv2` \
//   -lrunara
//
// run:
// ./waydemo
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <libdecor.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "deps/runara/include/runara/runara.h"

/* ============================================================
 * App state
 * ============================================================ */

struct app {
    // Wayland
    struct wl_display    *display;
    struct wl_registry   *registry;
    struct wl_compositor *compositor;
    struct wl_surface    *surface;

    // libdecor
    struct libdecor      *decor;
    struct libdecor_frame *frame;

    // EGL
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    struct wl_egl_window *egl_window;

    // Runara
    RnState *rn;
    RnFont  *font_big;
    RnFont  *font_small;

    int width;
    int height;
    bool running;
};

/* ============================================================
 * Wayland registry
 * ============================================================ */

static void
registry_global(void *data,
                struct wl_registry *registry,
                uint32_t name,
                const char *interface,
                uint32_t version)
{
    struct app *app = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        app->compositor =
            wl_registry_bind(registry, name,
                             &wl_compositor_interface, 4);
    }
}

static void
registry_remove(void *data,
                struct wl_registry *registry,
                uint32_t name)
{
    (void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_remove
};

/* ============================================================
 * libdecor frame callbacks
 * ============================================================ */

static void
frame_configure(struct libdecor_frame *frame,
                struct libdecor_configuration *conf,
                void *data)
{
    struct app *app = data;
    int w, h;

    if (libdecor_configuration_get_content_size(conf, frame, &w, &h)) {
        if (w > 0 && h > 0 &&
            (w != app->width || h != app->height)) {

            app->width  = w;
            app->height = h;

            wl_egl_window_resize(app->egl_window, w, h, 0, 0);
            rn_resize_display(app->rn, w, h);
        }
    }

    libdecor_frame_ack_configure(frame, conf);
}

static void
frame_close(struct libdecor_frame *frame, void *data)
{
    (void)frame;
    struct app *app = data;
    app->running = false;
}

static const struct libdecor_frame_interface frame_iface = {
    .configure = frame_configure,
    .close     = frame_close
};

/* ============================================================
 * EGL helpers
 * ============================================================ */

static void
init_egl(struct app *app)
{
    app->egl_display = eglGetDisplay((EGLNativeDisplayType)app->display);
    if (app->egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "EGL: no display\n");
        exit(1);
    }

    if (!eglInitialize(app->egl_display, NULL, NULL)) {
        fprintf(stderr, "EGL: init failed\n");
        exit(1);
    }

    static const EGLint cfg_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLConfig cfg;
    EGLint n;
    eglChooseConfig(app->egl_display, cfg_attribs, &cfg, 1, &n);

    static const EGLint ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    app->egl_context =
        eglCreateContext(app->egl_display, cfg,
                         EGL_NO_CONTEXT, ctx_attribs);

    if (app->egl_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "EGL: context failed\n");
        exit(1);
    }

    app->egl_window =
        wl_egl_window_create(app->surface,
                             app->width, app->height);

    app->egl_surface =
        eglCreateWindowSurface(app->egl_display,
                                cfg,
                                (EGLNativeWindowType)app->egl_window,
                                NULL);

    eglMakeCurrent(app->egl_display,
                   app->egl_surface,
                   app->egl_surface,
                   app->egl_context);
}

/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    struct app app = {
        .width  = 1280,
        .height = 720,
        .running = true
    };

    /* --- Wayland connect --- */
    app.display = wl_display_connect(NULL);
    if (!app.display) {
        fprintf(stderr, "Wayland: cannot connect\n");
        return 1;
    }

    app.registry = wl_display_get_registry(app.display);
    wl_registry_add_listener(app.registry,
                             &registry_listener, &app);
    wl_display_roundtrip(app.display);

    if (!app.compositor) {
        fprintf(stderr, "Wayland: compositor missing\n");
        return 1;
    }

    /* --- Surface --- */
    app.surface = wl_compositor_create_surface(app.compositor);

    /* --- libdecor --- */
    app.decor = libdecor_new(app.display, NULL);
    app.frame = libdecor_decorate(app.decor,
                                  app.surface,
                                  &frame_iface,
                                  &app);

    libdecor_frame_set_title(app.frame, "waydemo: libdecor + runara");
    libdecor_frame_map(app.frame);

    /* --- EGL + Runara --- */
    init_egl(&app);

    app.rn = rn_init(app.width, app.height,
                     (RnGLLoader)eglGetProcAddress);

    app.font_big   = rn_load_font(app.rn, "Lora-Italic.ttf", 36);
    app.font_small = rn_load_font(app.rn, "Lora-Italic.ttf", 22);

    /* --- Main loop --- */
    while (app.running) {
        wl_display_dispatch_pending(app.display);

        glViewport(0, 0, app.width, app.height);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        rn_begin(app.rn);

        rn_text_render(app.rn,
            "Hello from Wayland!",
            app.font_big,
            (vec2s){ 24, 32 },
            RN_WHITE);

        rn_text_render(app.rn,
            "libdecor CSD + Runara renderer\n"
            "No X11 • No GLFW • 2026-ready",
            app.font_small,
            (vec2s){ 24, 90 },
            RN_WHITE);

        rn_rect_render(app.rn,
            (vec2s){ 24, 150 },
            (vec2s){ 240, 120 },
            RN_RED);

        rn_end(app.rn);

        eglSwapBuffers(app.egl_display, app.egl_surface);
    }

    /* --- Shutdown --- */
    rn_free_font(app.rn, app.font_big);
    rn_free_font(app.rn, app.font_small);
    rn_terminate(app.rn);

    libdecor_frame_unref(app.frame);
    libdecor_unref(app.decor);

    wl_egl_window_destroy(app.egl_window);
    eglDestroySurface(app.egl_display, app.egl_surface);
    eglDestroyContext(app.egl_display, app.egl_context);
    eglTerminate(app.egl_display);

    wl_surface_destroy(app.surface);
    wl_display_disconnect(app.display);

    return 0;
}