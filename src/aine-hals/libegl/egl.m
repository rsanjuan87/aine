/*
 * aine-hals/libegl/egl.m — EGL 1.4 implementation for AINE (macOS ARM64)
 *
 * Backend: Apple Metal (MTLDevice + CAMetalLayer for window surfaces,
 *          IOSurface for offscreen pbuffer surfaces).
 *
 * Design:
 *   EGLDisplay  → AineEGLDisplay  (holds id<MTLDevice>)
 *   EGLConfig   → AineEGLConfig   (colour/depth/renderable attrs)
 *   EGLSurface  → AineEGLSurface  (window=CAMetalLayer | pbuf=IOSurface)
 *   EGLContext  → AineEGLContext   (MTLCommandQueue + state)
 *
 * Thread safety: Basic—each thread may hold its own current context via TLS.
 */

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <IOSurface/IOSurface.h>
#include <CoreVideo/CVPixelBuffer.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "EGL/egl.h"

/* -------------------------------------------------------------------------
 * Thread-local error + current context
 * ---------------------------------------------------------------------- */
static pthread_key_t s_tls_key;
static pthread_once_t s_tls_once = PTHREAD_ONCE_INIT;

typedef struct {
    EGLint      last_error;
    void       *current_ctx;   /* AineEGLContext* */
    void       *current_draw;  /* AineEGLSurface* */
    void       *current_read;  /* AineEGLSurface* */
} AineTLS;

static void tls_destructor(void *p) { free(p); }
static void tls_init(void) { pthread_key_create(&s_tls_key, tls_destructor); }

static AineTLS *tls_get(void)
{
    pthread_once(&s_tls_once, tls_init);
    AineTLS *t = pthread_getspecific(s_tls_key);
    if (!t) {
        t = calloc(1, sizeof(*t));
        t->last_error = EGL_SUCCESS;
        pthread_setspecific(s_tls_key, t);
    }
    return t;
}

static void set_error(EGLint err)  { tls_get()->last_error = err; }
static void clear_error(void)      { tls_get()->last_error = EGL_SUCCESS; }

/* -------------------------------------------------------------------------
 * Internal types
 * ---------------------------------------------------------------------- */
typedef struct {
    id<MTLDevice>  device;
    EGLint         major, minor;
    EGLBoolean     initialized;
} AineEGLDisplay;

/* A single config offering RGBA8 + depth16 + MSAA0, window+pbuffer */
typedef struct {
    EGLint red_size, green_size, blue_size, alpha_size;
    EGLint depth_size, stencil_size;
    EGLint renderable;   /* EGL_OPENGL_ES2_BIT */
    EGLint surface_type; /* EGL_WINDOW_BIT | EGL_PBUFFER_BIT */
    EGLint config_id;
} AineEGLConfig;

typedef enum {
    SURF_WINDOW,
    SURF_PBUFFER,
} SurfKind;

typedef struct {
    SurfKind    kind;
    EGLint      width, height;
    /* window surface */
    CAMetalLayer * __unsafe_unretained layer;
    /* pbuffer surface */
    IOSurfaceRef  iosurface;
} AineEGLSurface;

typedef struct {
    id<MTLCommandQueue> queue;
    EGLint client_version; /* 1 or 2 */
} AineEGLContext;

/* -------------------------------------------------------------------------
 * Singleton display
 * ---------------------------------------------------------------------- */
static AineEGLDisplay s_display;
static EGLBoolean     s_display_used = EGL_FALSE;

/* Default config */
static AineEGLConfig s_config = {
    .red_size   = 8, .green_size = 8, .blue_size = 8, .alpha_size = 8,
    .depth_size = 16, .stencil_size = 8,
    .renderable  = EGL_OPENGL_ES2_BIT,
    .surface_type = EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
    .config_id   = 1,
};

/* =========================================================================
 * EGL API implementation
 * ====================================================================== */

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id)
{
    (void)display_id; /* EGL_DEFAULT_DISPLAY is the only supported value */
    clear_error();
    s_display.initialized = EGL_FALSE;
    s_display_used = EGL_TRUE;
    return &s_display;
}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
    clear_error();
    if (!dpy) { set_error(EGL_BAD_DISPLAY); return EGL_FALSE; }
    AineEGLDisplay *d = (AineEGLDisplay *)dpy;

    if (!d->initialized) {
        d->device = MTLCreateSystemDefaultDevice();
        if (!d->device) {
            fprintf(stderr, "[aine-egl] MTLCreateSystemDefaultDevice failed\n");
            set_error(EGL_NOT_INITIALIZED);
            return EGL_FALSE;
        }
        d->major = 1;
        d->minor = 4;
        d->initialized = EGL_TRUE;
    }
    if (major) *major = d->major;
    if (minor) *minor = d->minor;
    return EGL_TRUE;
}

EGLBoolean eglTerminate(EGLDisplay dpy)
{
    clear_error();
    if (!dpy) { set_error(EGL_BAD_DISPLAY); return EGL_FALSE; }
    AineEGLDisplay *d = (AineEGLDisplay *)dpy;
    d->device = nil;
    d->initialized = EGL_FALSE;
    return EGL_TRUE;
}

/* Bound API (we only support OPENGL_ES) */
static EGLenum s_bound_api = EGL_OPENGL_ES_API;

EGLBoolean eglBindAPI(EGLenum api)
{
    clear_error();
    if (api != EGL_OPENGL_ES_API) {
        set_error(EGL_BAD_PARAMETER);
        return EGL_FALSE;
    }
    s_bound_api = api;
    return EGL_TRUE;
}

EGLenum eglQueryAPI(void) { return s_bound_api; }

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
                            EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    clear_error();
    if (!dpy) { set_error(EGL_BAD_DISPLAY); return EGL_FALSE; }
    if (!num_config) { set_error(EGL_BAD_PARAMETER); return EGL_FALSE; }

    /* We always return our single config regardless of attrib requests */
    *num_config = 1;
    if (configs && config_size >= 1)
        configs[0] = &s_config;
    return EGL_TRUE;
}

EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config,
                               EGLint attribute, EGLint *value)
{
    clear_error();
    if (!dpy) { set_error(EGL_BAD_DISPLAY); return EGL_FALSE; }
    if (!config || !value) { set_error(EGL_BAD_CONFIG); return EGL_FALSE; }
    AineEGLConfig *c = (AineEGLConfig *)config;
    switch (attribute) {
        case EGL_RED_SIZE:       *value = c->red_size;     break;
        case EGL_GREEN_SIZE:     *value = c->green_size;   break;
        case EGL_BLUE_SIZE:      *value = c->blue_size;    break;
        case EGL_ALPHA_SIZE:     *value = c->alpha_size;   break;
        case EGL_DEPTH_SIZE:     *value = c->depth_size;   break;
        case EGL_STENCIL_SIZE:   *value = c->stencil_size; break;
        case EGL_RENDERABLE_TYPE: *value = c->renderable;  break;
        case EGL_SURFACE_TYPE:   *value = c->surface_type; break;
        case EGL_CONFIG_ID:      *value = c->config_id;    break;
        case EGL_SAMPLES:        *value = 0;               break;
        case EGL_SAMPLE_BUFFERS: *value = 0;               break;
        default:
            set_error(EGL_BAD_ATTRIBUTE);
            return EGL_FALSE;
    }
    return EGL_TRUE;
}

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                   EGLNativeWindowType win,
                                   const EGLint *attrib_list)
{
    (void)attrib_list;
    clear_error();
    if (!dpy) { set_error(EGL_BAD_DISPLAY); return EGL_NO_SURFACE; }
    AineEGLDisplay *d = (AineEGLDisplay *)dpy;
    if (!d->initialized) { set_error(EGL_NOT_INITIALIZED); return EGL_NO_SURFACE; }

    AineEGLSurface *s = calloc(1, sizeof(*s));
    s->kind  = SURF_WINDOW;
    /* win is expected to be a CAMetalLayer* wrapped as void* */
    s->layer = (__bridge CAMetalLayer *)(win);
    if (s->layer) {
        s->width  = (EGLint)s->layer.drawableSize.width;
        s->height = (EGLint)s->layer.drawableSize.height;
        s->layer.device = d->device;
        s->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        s->layer.framebufferOnly = NO;
    }
    return s;
}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
                                    const EGLint *attrib_list)
{
    clear_error();
    if (!dpy) { set_error(EGL_BAD_DISPLAY); return EGL_NO_SURFACE; }
    AineEGLDisplay *d = (AineEGLDisplay *)dpy;
    if (!d->initialized) { set_error(EGL_NOT_INITIALIZED); return EGL_NO_SURFACE; }

    EGLint width = 1, height = 1;
    if (attrib_list) {
        for (const EGLint *p = attrib_list; *p != EGL_NONE; p += 2) {
            if (p[0] == EGL_WIDTH)  width  = p[1];
            if (p[0] == EGL_HEIGHT) height = p[1];
        }
    }

    AineEGLSurface *s = calloc(1, sizeof(*s));
    s->kind   = SURF_PBUFFER;
    s->width  = width;
    s->height = height;

    /* Create IOSurface for offscreen rendering (no window required) */
    NSDictionary *props = @{
        (id)kIOSurfaceWidth:          @(width),
        (id)kIOSurfaceHeight:         @(height),
        (id)kIOSurfaceBytesPerElement:@(4),
        (id)kIOSurfacePixelFormat:    @((uint32_t)kCVPixelFormatType_32BGRA),
    };
    s->iosurface = IOSurfaceCreate((__bridge CFDictionaryRef)props);
    if (!s->iosurface) {
        free(s);
        set_error(EGL_BAD_ALLOC);
        return EGL_NO_SURFACE;
    }
    return s;
}

EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
    clear_error();
    if (!dpy) { set_error(EGL_BAD_DISPLAY); return EGL_FALSE; }
    if (!surface) { set_error(EGL_BAD_SURFACE); return EGL_FALSE; }
    AineEGLSurface *s = (AineEGLSurface *)surface;
    if (s->iosurface) CFRelease(s->iosurface);
    free(s);
    return EGL_TRUE;
}

EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface,
                            EGLint attribute, EGLint *value)
{
    clear_error();
    if (!dpy) { set_error(EGL_BAD_DISPLAY); return EGL_FALSE; }
    AineEGLSurface *s = (AineEGLSurface *)surface;
    switch (attribute) {
        case EGL_WIDTH:  if (value) *value = s->width;  break;
        case EGL_HEIGHT: if (value) *value = s->height; break;
        default: set_error(EGL_BAD_ATTRIBUTE); return EGL_FALSE;
    }
    return EGL_TRUE;
}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config,
                            EGLContext share_context,
                            const EGLint *attrib_list)
{
    (void)share_context; (void)config;
    clear_error();
    if (!dpy) { set_error(EGL_BAD_DISPLAY); return EGL_NO_CONTEXT; }
    AineEGLDisplay *d = (AineEGLDisplay *)dpy;
    if (!d->initialized) { set_error(EGL_NOT_INITIALIZED); return EGL_NO_CONTEXT; }

    EGLint version = 1;
    if (attrib_list) {
        for (const EGLint *p = attrib_list; *p != EGL_NONE; p += 2) {
            if (p[0] == EGL_CONTEXT_CLIENT_VERSION) version = p[1];
        }
    }

    AineEGLContext *ctx = calloc(1, sizeof(*ctx));
    ctx->queue = [d->device newCommandQueue];
    ctx->client_version = version;
    if (!ctx->queue) {
        free(ctx);
        set_error(EGL_BAD_ALLOC);
        return EGL_NO_CONTEXT;
    }
    return ctx;
}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext eglctx)
{
    clear_error();
    if (!dpy) { set_error(EGL_BAD_DISPLAY); return EGL_FALSE; }
    AineEGLContext *ctx = (AineEGLContext *)eglctx;
    if (ctx) { ctx->queue = nil; free(ctx); }
    return EGL_TRUE;
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
                           EGLSurface read, EGLContext eglctx)
{
    clear_error();
    AineTLS *tls = tls_get();
    tls->current_ctx  = eglctx;
    tls->current_draw = draw;
    tls->current_read = read;
    return EGL_TRUE;
}

EGLContext eglGetCurrentContext(void)  { return tls_get()->current_ctx;  }
EGLSurface eglGetCurrentSurface(EGLint which)
{
    AineTLS *t = tls_get();
    return (which == EGL_READ) ? t->current_read : t->current_draw;
}
EGLDisplay eglGetCurrentDisplay(void)  { return &s_display; }

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    clear_error();
    if (!dpy) { set_error(EGL_BAD_DISPLAY); return EGL_FALSE; }
    AineEGLSurface *s = (AineEGLSurface *)surface;
    if (!s) { set_error(EGL_BAD_SURFACE); return EGL_FALSE; }

    if (s->kind == SURF_WINDOW && s->layer) {
        /* Acquire the next drawable and present it via a command buffer */
        id<CAMetalDrawable> drawable = [s->layer nextDrawable];
        if (drawable) {
            AineEGLContext *ctx = (AineEGLContext *)tls_get()->current_ctx;
            if (ctx && ctx->queue) {
                id<MTLCommandBuffer> cmdbuf = [ctx->queue commandBuffer];
                [cmdbuf presentDrawable:drawable];
                [cmdbuf commit];
            }
        }
    }
    /* pbuffer: no-op (offscreen, no presentation needed) */
    return EGL_TRUE;
}

EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
    (void)dpy; (void)interval;
    clear_error();
    return EGL_TRUE;
}

EGLint eglGetError(void) { return tls_get()->last_error; }

const char *eglQueryString(EGLDisplay dpy, EGLint name)
{
    clear_error();
    if (!dpy && name != EGL_CLIENT_APIS) { set_error(EGL_BAD_DISPLAY); return NULL; }
    switch (name) {
        case EGL_VENDOR:      return "AINE Project";
        case EGL_VERSION:     return "1.4 AINE/Metal";
        case EGL_EXTENSIONS:  return "";
        case EGL_CLIENT_APIS: return "OpenGL_ES";
        default:
            set_error(EGL_BAD_PARAMETER);
            return NULL;
    }
}

__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *procname)
{
    (void)procname;
    return NULL; /* Extension functions not currently supported */
}

EGLBoolean eglReleaseThread(void)
{
    AineTLS *t = tls_get();
    t->current_ctx  = NULL;
    t->current_draw = NULL;
    t->current_read = NULL;
    t->last_error   = EGL_SUCCESS;
    return EGL_TRUE;
}

EGLBoolean eglWaitClient(void)  { clear_error(); return EGL_TRUE; }
EGLBoolean eglWaitNative(EGLint engine) { (void)engine; clear_error(); return EGL_TRUE; }
EGLBoolean eglWaitGL(void) { clear_error(); return EGL_TRUE; }
