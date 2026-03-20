/*
 * aine-hals/surface/surface.m — NSWindow + CAMetalLayer surface manager
 *
 * Creates native macOS windows backed by Metal for Android app rendering.
 * Window surfaces require the main thread and an NSApplication.
 * Offscreen surfaces use IOSurface and work headlessly.
 */

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#include <IOSurface/IOSurface.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "aine_surface.h"

/* -------------------------------------------------------------------------
 * Internal handle
 * ---------------------------------------------------------------------- */
typedef enum { SURF_WINDOW, SURF_OFFSCREEN } SurfaceKind;

struct AineSurfaceHandle {
    SurfaceKind     kind;
    int             width, height;
    /* window */
    NSWindow       * __strong window;
    /* both */
    CAMetalLayer   * __strong layer;
    /* offscreen */
    IOSurfaceRef    iosurface;
    /* metal */
    id<MTLDevice>   device;
};

/* -------------------------------------------------------------------------
 * Window surface
 * ---------------------------------------------------------------------- */

/* AppKit operations must happen on the main thread */
static void create_window_main(AineSurfaceHandle *h)
{
    NSRect frame = NSMakeRect(100, 100, h->width, h->height);
    NSWindowStyleMask style = NSWindowStyleMaskTitled
                            | NSWindowStyleMaskClosable
                            | NSWindowStyleMaskMiniaturizable
                            | NSWindowStyleMaskResizable;
    h->window = [[NSWindow alloc] initWithContentRect:frame
                                            styleMask:style
                                              backing:NSBackingStoreBuffered
                                                defer:NO];
    if (!h->window) return;

    /* Set up CAMetalLayer as the view's backing layer */
    NSView *contentView = [h->window contentView];
    [contentView setWantsLayer:YES];

    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device      = h->device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.drawableSize = CGSizeMake(h->width, h->height);
    layer.framebufferOnly = NO;
    [contentView setLayer:layer];
    h->layer = layer;

    [h->window setTitle:[NSString stringWithUTF8String:
        (h->width > 0 ? "AINE" : "AINE")]]; /* title is set by caller */
    [h->window makeKeyAndOrderFront:nil];
}

AineSurfaceHandle *aine_surface_create_window(int width, int height,
                                              const char *title)
{
    AineSurfaceHandle *h = calloc(1, sizeof(*h));
    h->kind   = SURF_WINDOW;
    h->width  = width;
    h->height = height;
    h->device = MTLCreateSystemDefaultDevice();
    if (!h->device) { free(h); return NULL; }

    if ([NSThread isMainThread]) {
        create_window_main(h);
    } else {
        dispatch_sync(dispatch_get_main_queue(), ^{
            create_window_main(h);
        });
    }

    if (!h->window) { free(h); return NULL; }

    if (title && h->window)
        [h->window setTitle:[NSString stringWithUTF8String:title]];

    return h;
}

/* -------------------------------------------------------------------------
 * Offscreen surface (IOSurface + CAMetalLayer — no display needed)
 * ---------------------------------------------------------------------- */
AineSurfaceHandle *aine_surface_create_offscreen(int width, int height)
{
    AineSurfaceHandle *h = calloc(1, sizeof(*h));
    h->kind   = SURF_OFFSCREEN;
    h->width  = width;
    h->height = height;
    h->device = MTLCreateSystemDefaultDevice();
    if (!h->device) { free(h); return NULL; }

    /* Create backing IOSurface */
    NSDictionary *props = @{
        (id)kIOSurfaceWidth:           @(width),
        (id)kIOSurfaceHeight:          @(height),
        (id)kIOSurfaceBytesPerElement: @(4),
        (id)kIOSurfacePixelFormat:     @((uint32_t)kCVPixelFormatType_32BGRA),
    };
    h->iosurface = IOSurfaceCreate((__bridge CFDictionaryRef)props);
    if (!h->iosurface) { free(h); return NULL; }

    /* CAMetalLayer backed by the IOSurface */
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device         = h->device;
    layer.pixelFormat    = MTLPixelFormatBGRA8Unorm;
    layer.drawableSize   = CGSizeMake(width, height);
    layer.framebufferOnly = NO;
    /* Attach the IOSurface as the layer's contentsFormat backing */
    layer.contents = (__bridge id)h->iosurface;
    h->layer = layer;

    return h;
}

/* -------------------------------------------------------------------------
 * Accessors
 * ---------------------------------------------------------------------- */
void *aine_surface_get_layer(AineSurfaceHandle *h)
{
    return (__bridge void *)h->layer;
}

void aine_surface_get_size(AineSurfaceHandle *h, int *w, int *h_out)
{
    if (w)     *w     = h->width;
    if (h_out) *h_out = h->height;
}

void aine_surface_present(AineSurfaceHandle *h)
{
    if (h->kind != SURF_WINDOW || !h->layer) return;
    /* EGL's eglSwapBuffers handles the actual present via Metal command buffer.
     * For direct callers (e.g. aine-launcher), we can do a simple layer display. */
    dispatch_async(dispatch_get_main_queue(), ^{
        [h->layer setNeedsDisplay];
    });
}

void aine_surface_destroy(AineSurfaceHandle *h)
{
    if (!h) return;
    if (h->kind == SURF_WINDOW && h->window) {
        dispatch_sync(dispatch_get_main_queue(), ^{
            [h->window close];
        });
        h->window = nil;
    }
    h->layer = nil;
    if (h->iosurface) { CFRelease(h->iosurface); h->iosurface = NULL; }
    h->device = nil;
    free(h);
}
