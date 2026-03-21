// aine-dalvik/handler.c — Android Handler/Looper cooperative event queue
//
// Implements postDelayed(Runnable, long delayMs) by storing pending callbacks
// in a priority queue ordered by deadline.  handler_drain() drives the loop.
//
// The run() method of each Runnable is invoked via interp_run_runnable() which
// calls exec_method(interp, class_desc, "run", ...) — this handles both plain
// anonymous Runnables and lambda-desugared synthetic classes
// (e.g. MainActivity$$ExternalSyntheticLambda0).

#include "handler.h"
#include "interp.h"

#include <stddef.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

/* ── Priority queue ─────────────────────────────────────────────────────── */
#define MAX_PENDING 128

typedef struct {
    AineObj  *runnable;
    int64_t   fire_ns;    /* CLOCK_MONOTONIC nanoseconds */
} PendingCb;

static PendingCb g_queue[MAX_PENDING];
static int       g_count = 0;

/* ── Time helpers ─────────────────────────────────────────────────────── */
static int64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void sleep_ns(int64_t ns) {
    if (ns <= 0) return;
    struct timespec ts = {
        .tv_sec  = (time_t)(ns / 1000000000LL),
        .tv_nsec = (long)(ns % 1000000000LL)
    };
    nanosleep(&ts, NULL);
}

/* ── Public API ─────────────────────────────────────────────────────────── */
void handler_post_delayed(AineObj *runnable, int64_t delay_ms) {
    if (!runnable) return;
    if (g_count >= MAX_PENDING) {
        fprintf(stderr, "[aine-handler] queue full — dropping callback\n");
        return;
    }
    int64_t fire = now_ns() + delay_ms * 1000000LL;
    g_queue[g_count].runnable = runnable;
    g_queue[g_count].fire_ns  = fire;
    g_count++;
}

int handler_pending(void) { return g_count; }

void handler_clear(void) { g_count = 0; }

void handler_drain(AineInterp *interp, int64_t max_ms)
{
    int64_t deadline = now_ns() + max_ms * 1000000LL;

    while (g_count > 0) {
        /* Find earliest pending callback */
        int earliest = 0;
        for (int i = 1; i < g_count; i++) {
            if (g_queue[i].fire_ns < g_queue[earliest].fire_ns)
                earliest = i;
        }
        PendingCb cb = g_queue[earliest];

        /* If this callback fires after our deadline, stop */
        if (cb.fire_ns > deadline) break;

        /* Sleep until fire time (or now if already past) */
        int64_t wait = cb.fire_ns - now_ns();
        if (wait > 0) sleep_ns(wait);

        /* Remove from queue before invoking (avoid re-entrancy issues) */
        g_queue[earliest] = g_queue[--g_count];

        /* Invoke the Runnable's run() method */
        interp_run_runnable(interp, cb.runnable);
    }
}
