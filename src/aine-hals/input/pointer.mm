/*
 * aine-hals/input/pointer.mm — NSEvent mouse/trackpad → Android MotionEvent bridge
 *
 * Translates macOS mouse events into AineMotionEvents and pushes them to
 * the InputFlinger queue.  Coordinate system: window top-left = (0,0),
 * matching Android View coordinate space.
 */

#import <AppKit/AppKit.h>
#include <time.h>
#include <stdlib.h>

#include "inputflinger.h"
#include "pointer.h"

static id s_pointer_monitor = nil;
static NSWindow *s_tracked_window = nil;

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Convert NSWindow-relative point (AppKit: origin = bottom-left)
 * to Android-style point (origin = top-left) */
static void flip_coords(NSWindow *win, NSPoint pt, float *out_x, float *out_y)
{
    *out_x = (float)pt.x;
    *out_y = (float)(win.contentView.bounds.size.height - pt.y);
}

void aine_input_pointer_start(void *ns_window)
{
    if (s_pointer_monitor) return;
    s_tracked_window = (__bridge NSWindow *)ns_window;

    NSEventMask mask = NSEventMaskLeftMouseDown
                     | NSEventMaskLeftMouseUp
                     | NSEventMaskLeftMouseDragged
                     | NSEventMaskRightMouseDown
                     | NSEventMaskRightMouseUp
                     | NSEventMaskMouseMoved
                     | NSEventMaskScrollWheel;

    s_pointer_monitor = [NSEvent
        addLocalMonitorForEventsMatchingMask:mask
        handler:^NSEvent *(NSEvent *event) {
            AineInputEvent ie;
            ie.kind = AINE_INPUT_MOTION;
            ie.motion.pointer_id = 0;
            ie.motion.event_time_ns = now_ns();
            ie.motion.scroll_x = 0;
            ie.motion.scroll_y = 0;

            NSPoint loc = [event locationInWindow];
            NSWindow *win = [event window];
            if (!win) win = s_tracked_window;

            float x = (float)loc.x, y = (float)loc.y;
            if (win) flip_coords(win, loc, &x, &y);
            ie.motion.x = x; ie.motion.raw_x = x;
            ie.motion.y = y; ie.motion.raw_y = y;

            switch ([event type]) {
                case NSEventTypeLeftMouseDown:
                    ie.motion.action = AMOTION_ACTION_DOWN;
                    aine_input_push(&ie);
                    break;
                case NSEventTypeLeftMouseUp:
                    ie.motion.action = AMOTION_ACTION_UP;
                    aine_input_push(&ie);
                    break;
                case NSEventTypeLeftMouseDragged:
                case NSEventTypeMouseMoved:
                    ie.motion.action = AMOTION_ACTION_MOVE;
                    aine_input_push(&ie);
                    break;
                case NSEventTypeScrollWheel:
                    ie.motion.action = AMOTION_ACTION_SCROLL;
                    ie.motion.scroll_x = (float)[event scrollingDeltaX];
                    ie.motion.scroll_y = (float)[event scrollingDeltaY];
                    aine_input_push(&ie);
                    break;
                default:
                    break;
            }
            return event;
        }];
}

void aine_input_pointer_stop(void)
{
    if (s_pointer_monitor) {
        [NSEvent removeMonitor:s_pointer_monitor];
        s_pointer_monitor = nil;
    }
    s_tracked_window = nil;
}
