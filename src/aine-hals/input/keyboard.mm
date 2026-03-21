/*
 * aine-hals/input/keyboard.mm — NSEvent keyboard → Android KeyEvent bridge
 *
 * Registers a global NSEvent monitor so any key event in the AINE app
 * window is translated to AineKeyEvent and pushed to the InputFlinger queue.
 *
 * Requires AppKit (NSEvent). Objective-C++ because it links against C headers.
 */

#import <AppKit/AppKit.h>
#include <time.h>
#include <stdlib.h>

#include "inputflinger.h"
#include "input_keymap.h"

static id s_keyboard_monitor = nil;

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

extern "C" void aine_input_keyboard_start(void)
{
    if (s_keyboard_monitor) return;

    NSEventMask mask = NSEventMaskKeyDown | NSEventMaskKeyUp
                     | NSEventMaskFlagsChanged;

    s_keyboard_monitor = [NSEvent
        addLocalMonitorForEventsMatchingMask:mask
        handler:^NSEvent *(NSEvent *event) {
            AineInputEvent ie;
            ie.kind = AINE_INPUT_KEY;

            unsigned char vk = (unsigned char)[event keyCode];
            ie.key.keycode   = aine_input_vk_to_keycode(vk);
            ie.key.meta_state = aine_input_modifiers_to_meta(
                                    (uint64_t)[event modifierFlags]);
            ie.key.event_time_ns = now_ns();

            switch ([event type]) {
                case NSEventTypeKeyDown:
                    ie.key.action = AKEY_ACTION_DOWN;
                    aine_input_push(&ie);
                    break;
                case NSEventTypeKeyUp:
                    ie.key.action = AKEY_ACTION_UP;
                    aine_input_push(&ie);
                    break;
                case NSEventTypeFlagsChanged:
                    /* Modifier-only events: synthesise DOWN/UP based on state */
                    ie.key.action = ([event modifierFlags] & (1ULL<<16)) ?
                                    AKEY_ACTION_DOWN : AKEY_ACTION_UP;
                    aine_input_push(&ie);
                    break;
                default:
                    break;
            }
            return event; /* pass through to NSWindow */
        }];
}

extern "C" void aine_input_keyboard_stop(void)
{
    if (s_keyboard_monitor) {
        [NSEvent removeMonitor:s_keyboard_monitor];
        s_keyboard_monitor = nil;
    }
}
