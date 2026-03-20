/* inputflinger.h — AINE input event queue (Android InputFlinger equivalent) */
#ifndef AINE_INPUTFLINGER_H
#define AINE_INPUTFLINGER_H

#include "input_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Push one event into the queue (producer side, called from NSEvent handler) */
void aine_input_push(const AineInputEvent *ev);

/* Non-blocking dequeue — returns 1 if event was dequeued, 0 if empty */
int  aine_input_poll(AineInputEvent *ev);

/* Blocking dequeue with timeout_ms (-1 = wait forever, 0 = non-blocking) */
int  aine_input_poll_wait(AineInputEvent *ev, int timeout_ms);

/* Number of pending events */
int  aine_input_pending(void);

/* Discard all queued events */
void aine_input_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* AINE_INPUTFLINGER_H */
