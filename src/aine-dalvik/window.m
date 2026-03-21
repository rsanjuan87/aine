/* aine-dalvik/window.m — NSApplication bootstrap for --window mode.
 *
 * Creates a native macOS NSWindow (with CAMetalLayer) and runs the Android
 * Activity lifecycle inside it.  The interpreter runs on a background
 * dispatch thread while the main thread pumps the NSRunLoop — this lets
 * NSWindow render, resize, and receive input events normally.
 *
 * Headless-safe: if MTLCreateSystemDefaultDevice() returns NULL (no GPU) or
 * [[NSWindow alloc] init...] fails (no display), the code skips window
 * creation and still runs the Activity lifecycle purely via stderr output.
 * CTtest environments (CI without display) pass this way.
 *
 * Threading model:
 *   main thread  : NSRunLoop pump (AppKit requirement)
 *   global queue : interp_run_main() → onCreate → onResume → handler_drain
 *                  → onPause → onStop → onDestroy → signals semaphore
 *   main thread  : detects semaphore → exits pump loop → closes window → returns
 */

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

#include "interp.h"
#include "window.h"

/* Input monitors — compiled into dalvikvm on macOS */
#include "keyboard.h"
#include "pointer.h"

/* ── Window state ───────────────────────────────────────────────────────── */
static NSWindow * __strong g_window = nil;

/* ── Quit flag: set by window delegate or Activity.finish() ─────────────── */
static atomic_int g_should_quit = ATOMIC_VAR_INIT(0);

/* Exposed to interp.c so the Activity event loop can check it */
int aine_activity_should_finish(void) {
    return atomic_load(&g_should_quit);
}
void aine_activity_request_finish(void) {
    atomic_store(&g_should_quit, 1);
}

/* ── NSWindowDelegate — detect close button ─────────────────────────────── */
@interface AineWindowDelegate : NSObject <NSWindowDelegate>
@end
@implementation AineWindowDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender {
    aine_activity_request_finish();
    return YES;
}
@end

/* ─────────────────────────────────────────────────────────────────────────
 * create_window — best-effort NSWindow + CAMetalLayer.
 * Returns 1 if window was created, 0 if skipped (headless / no Metal).
 * ───────────────────────────────────────────────────────────────────────── */
static int create_window(const char *title)
{
    @try {
        /* Ensure NSApplication exists */
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];

        /* Need a Metal device for meaningful rendering */
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        if (!dev) {
            fprintf(stderr, "[aine-window] no Metal device — window skipped\n");
            return 0;
        }

        NSRect frame = NSMakeRect(100, 100, 800, 600);
        NSWindowStyleMask style = NSWindowStyleMaskTitled
                                | NSWindowStyleMaskClosable
                                | NSWindowStyleMaskMiniaturizable
                                | NSWindowStyleMaskResizable;

        g_window = [[NSWindow alloc] initWithContentRect:frame
                                               styleMask:style
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
        if (!g_window) {
            fprintf(stderr, "[aine-window] NSWindow alloc failed\n");
            return 0;
        }

        /* CAMetalLayer backing */
        NSView *view = [g_window contentView];
        [view setWantsLayer:YES];

        CAMetalLayer *layer = [CAMetalLayer layer];
        layer.device        = dev;
        layer.pixelFormat   = MTLPixelFormatBGRA8Unorm;
        layer.framebufferOnly = NO;
        layer.drawableSize  = CGSizeMake(800, 600);
        [view setLayer:layer];

        NSString *nstitle = title
            ? [NSString stringWithUTF8String:title]
            : @"AINE";
        [g_window setTitle:nstitle];
        [g_window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        /* Delegate: detect close-button → signal quit */
        AineWindowDelegate *delegate = [[AineWindowDelegate alloc] init];
        [g_window setDelegate:delegate];

        /* Start input monitors now that we have a real window */
        aine_input_keyboard_start();
        aine_input_pointer_start((__bridge void *)g_window);

        fprintf(stderr, "[aine-window] window \"%s\" created (800x600)\n",
                title ? title : "AINE");
        return 1;
    }
    @catch (NSException *ex) {
        fprintf(stderr, "[aine-window] window creation exception: %s\n",
                [[ex reason] UTF8String]);
        return 0;
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * aine_window_run — public entry point (must be called from main thread)
 * ───────────────────────────────────────────────────────────────────────── */
int aine_window_run(AineInterp *interp, const char *class_descriptor)
{
    /* Derive a human-readable window title from the class descriptor.
     * E.g. "Lcom/example/MainActivity;" → "MainActivity" */
    char title[64] = "AINE";
    if (class_descriptor) {
        const char *slash = strrchr(class_descriptor, '/');
        const char *src   = slash ? slash + 1 : class_descriptor;
        /* Skip leading 'L' if no slash */
        if (src == class_descriptor && src[0] == 'L') src++;
        size_t len = strlen(src);
        if (len > 0 && src[len - 1] == ';') len--;
        if (len >= sizeof(title)) len = sizeof(title) - 1;
        memcpy(title, src, len);
        title[len] = '\0';
    }

    /* Reset quit flag for this session */
    atomic_store(&g_should_quit, 0);

    /* Best-effort window creation */
    int has_window = create_window(title);

    /* Enable interactive event loop only when a real window exists.
     * In headless / CI environments (no Metal), fall back to drain mode. */
    if (has_window) {
        interp_set_window_mode(1);
    }

    /* Run interpreter on a background dispatch queue */
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block int interp_result = 0;

    dispatch_async(
        dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
            interp_result = interp_run_main(interp, class_descriptor);
            dispatch_semaphore_signal(done);
        });

    /* Pump the main NSRunLoop in 16 ms slices (≈60 fps polling).
     * This keeps NSWindow alive and responsive while the interpreter runs. */
    while (dispatch_semaphore_wait(done, DISPATCH_TIME_NOW) != 0) {
        @autoreleasepool {
            NSDate *until = [NSDate dateWithTimeIntervalSinceNow:0.016];
            [[NSRunLoop mainRunLoop] runUntilDate:until];
        }
    }

    /* Close window (if it was opened) */
    if (g_window) {
        [g_window close];
        g_window = nil;
    }

    /* Stop input monitors */
    aine_input_keyboard_stop();
    aine_input_pointer_stop();

    /* Reset mode flags */
    interp_set_window_mode(0);

    return interp_result;
}
