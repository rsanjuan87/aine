// aine-dalvik/handler.h — Android Handler/Looper event queue
// Implements postDelayed() semantics: schedule a DEX Runnable to fire after
// a given delay using a monotonic timer + nanosleep.
//
// Single-threaded cooperative model:
//   - Runnables are stored in a priority queue indexed by fire time.
//   - handler_drain() sleeps until the next callback is due, then calls
//     its run() method via the interpreter.
//   - Called explicitly by interp_run_main() in Activity mode after onResume.
#pragma once
#include <stdint.h>
#include "heap.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque interpreter forward-declaration (defined in interp.h) */
typedef struct AineInterp AineInterp;

/* Schedule a Runnable to fire after delay_ms milliseconds.
 * runnable must have class_desc set (done automatically by new-instance). */
void handler_post_delayed(AineObj *runnable, int64_t delay_ms);

/* Fire all pending Runnables whose deadline has passed.
 * Sleeps (nanosleep) until each fires in order.
 * Returns after max_ms milliseconds have elapsed or the queue is empty. */
void handler_drain(AineInterp *interp, int64_t max_ms);

/* Return number of pending callbacks. */
int handler_pending(void);

/* Discard all pending callbacks (called on process exit / reset). */
void handler_clear(void);

#ifdef __cplusplus
}
#endif
