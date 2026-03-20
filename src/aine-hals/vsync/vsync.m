/*
 * aine-hals/vsync/vsync.m — VSYNC source via CVDisplayLink (macOS ARM64)
 *
 * Uses CoreVideo CVDisplayLink (available since macOS 10.4) rather than
 * CADisplayLink which is only macOS 14+.  Fires callback on a private thread.
 */

#import <Foundation/Foundation.h>
#include <CoreVideo/CVDisplayLink.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>

#include "vsync.h"

/* -------------------------------------------------------------------------
 * Internal state
 * ---------------------------------------------------------------------- */

/* Forward-declare struct so the CVDisplayLink callback can reference it */
struct AineVsync {
    CVDisplayLinkRef  link;
    AineVsyncCallback callback;
    void             *userdata;
    int               running;
};

static CVReturn display_link_cb(CVDisplayLinkRef link,
                                const CVTimeStamp *now,
                                const CVTimeStamp *output,
                                CVOptionFlags flags,
                                CVOptionFlags *flags_out,
                                void *ctx)
{
    (void)link; (void)flags; (void)flags_out;
    AineVsync *v = (AineVsync *)ctx;
    if (v && v->callback && v->running) {
        /* Use output timestamp for accurate scheduling (nanoseconds) */
        uint64_t ts_ns = (uint64_t)output->hostTime;
        /* Convert host time to nanoseconds */
        static mach_timebase_info_data_t tb = {0, 0};
        if (tb.denom == 0) mach_timebase_info(&tb);
        ts_ns = ts_ns * tb.numer / tb.denom;
        v->callback(ts_ns, v->userdata);
    }
    return kCVReturnSuccess;
}

AineVsync *aine_vsync_create(AineVsyncCallback cb, void *userdata)
{
    AineVsync *v = calloc(1, sizeof(*v));
    v->callback = cb;
    v->userdata = userdata;
    v->running  = 0;

    CVReturn rv = CVDisplayLinkCreateWithActiveCGDisplays(&v->link);
    if (rv != kCVReturnSuccess) {
        fprintf(stderr, "[aine-vsync] CVDisplayLinkCreateWithActiveCGDisplays failed: %d\n", rv);
        free(v);
        return NULL;
    }
    CVDisplayLinkSetOutputCallback(v->link, display_link_cb, v);
    return v;
}

void aine_vsync_start(AineVsync *v)
{
    if (!v || v->running) return;
    v->running = 1;
    CVDisplayLinkStart(v->link);
    fprintf(stderr, "[aine-vsync] started\n");
}

void aine_vsync_stop(AineVsync *v)
{
    if (!v || !v->running) return;
    v->running = 0;
    CVDisplayLinkStop(v->link);
}

void aine_vsync_destroy(AineVsync *v)
{
    if (!v) return;
    if (v->running) aine_vsync_stop(v);
    CVDisplayLinkRelease(v->link);
    free(v);
}

/* Simple one-shot: sleep until next VSYNC (busy-free, portable fallback) */
void aine_vsync_wait_once(void)
{
    /* ~16.67ms for 60fps */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 16666667 };
    nanosleep(&ts, NULL);
}
