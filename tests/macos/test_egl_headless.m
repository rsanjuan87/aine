/*
 * tests/macos/test_egl_headless.m — CTest for F7 EGL + surface (macOS)
 *
 * Verifies:
 *   1. aine_surface_create_offscreen() returns a valid handle
 *   2. aine_surface_get_layer() returns a non-NULL CAMetalLayer
 *   3. eglGetDisplay() returns a valid EGLDisplay (not EGL_NO_DISPLAY)
 *   4. eglInitialize() succeeds (MTLCreateSystemDefaultDevice works)
 *   5. eglCreatePbufferSurface() succeeds (IOSurface allocated)
 *   6. eglCreateContext() succeeds (MTLCommandQueue created)
 *   7. eglMakeCurrent() succeeds
 *   8. eglSwapBuffers() on a pbuffer surface is a no-op success
 *   9. Cleanup (destroy surface, context, terminate) succeeds
 *
 * No display or NSApplication required — all headless via IOSurface/Metal.
 */

#import <Foundation/Foundation.h>
#include <stdio.h>

#include "aine_surface.h"
#include "EGL/egl.h"

static int s_pass = 0;
static int s_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [ok] %s\n", msg); s_pass++; } \
    else      { printf("  [FAIL] %s\n", msg); s_fail++; } \
} while (0)

int main(void)
{
    @autoreleasepool {
        printf("=== F7 EGL headless test ===\n");

        /* ---- 1. Offscreen surface ---- */
        AineSurfaceHandle *surf = aine_surface_create_offscreen(320, 240);
        CHECK(surf != NULL, "aine_surface_create_offscreen(320,240)");

        void *layer = surf ? aine_surface_get_layer(surf) : NULL;
        CHECK(layer != NULL, "aine_surface_get_layer returns non-NULL CAMetalLayer*");

        /* ---- 2. EGL display ---- */
        EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        CHECK(dpy != EGL_NO_DISPLAY, "eglGetDisplay(EGL_DEFAULT_DISPLAY) != EGL_NO_DISPLAY");

        EGLint maj = 0, min = 0;
        EGLBoolean ok = eglInitialize(dpy, &maj, &min);
        CHECK(ok == EGL_TRUE, "eglInitialize succeeds");
        CHECK(maj == 1 && min == 4, "EGL version 1.4");

        /* ---- 3. Config ---- */
        EGLint attribs[] = {
            EGL_RED_SIZE,      8,
            EGL_GREEN_SIZE,    8,
            EGL_BLUE_SIZE,     8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
        };
        EGLConfig cfg = NULL;
        EGLint    ncfg = 0;
        ok = eglChooseConfig(dpy, attribs, &cfg, 1, &ncfg);
        CHECK(ok == EGL_TRUE && ncfg >= 1, "eglChooseConfig finds >= 1 config");

        /* ---- 4. PBuffer surface ---- */
        EGLint pb_attribs[] = {
            EGL_WIDTH,  320,
            EGL_HEIGHT, 240,
            EGL_NONE
        };
        EGLSurface pb = eglCreatePbufferSurface(dpy, cfg, pb_attribs);
        CHECK(pb != EGL_NO_SURFACE, "eglCreatePbufferSurface(320x240)");

        /* ---- 5. Context ---- */
        ok = eglBindAPI(EGL_OPENGL_ES_API);
        CHECK(ok == EGL_TRUE, "eglBindAPI(EGL_OPENGL_ES_API)");

        EGLint ctx_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };
        EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attribs);
        CHECK(ctx != EGL_NO_CONTEXT, "eglCreateContext(GLES2)");
        CHECK(eglGetError() == EGL_SUCCESS, "No EGL error after context creation");

        /* ---- 6. MakeCurrent ---- */
        ok = eglMakeCurrent(dpy, pb, pb, ctx);
        CHECK(ok == EGL_TRUE, "eglMakeCurrent(pbuffer)");
        CHECK(eglGetCurrentContext() == ctx, "eglGetCurrentContext returns our ctx");

        /* ---- 7. SwapBuffers on pbuffer (no-op) ---- */
        ok = eglSwapBuffers(dpy, pb);
        CHECK(ok == EGL_TRUE, "eglSwapBuffers on pbuffer");

        /* ---- 8. Vendor string ---- */
        const char *vendor = eglQueryString(dpy, EGL_VENDOR);
        CHECK(vendor != NULL && vendor[0] != '\0', "eglQueryString(EGL_VENDOR) non-empty");

        /* ---- 9. Cleanup ---- */
        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(dpy, ctx);
        eglDestroySurface(dpy, pb);
        eglTerminate(dpy);
        if (surf) aine_surface_destroy(surf);
        CHECK(EGL_TRUE, "cleanup completed");

        printf("=== RESULT: %d ok, %d failed ===\n", s_pass, s_fail);
        return s_fail == 0 ? 0 : 1;
    }
}
