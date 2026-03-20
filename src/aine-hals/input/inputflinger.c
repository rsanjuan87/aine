/*
 * aine-hals/input/inputflinger.c — Android InputFlinger event queue for AINE
 *
 * Thread-safe circular buffer of AineInputEvents.  The NSEvent callbacks
 * (producer) push events from the main thread; the app's Looper thread
 * (consumer) calls aine_input_poll() to dequeue events.
 */

#include "inputflinger.h"

#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#define QUEUE_CAPACITY 256

typedef struct {
    AineInputEvent buf[QUEUE_CAPACITY];
    int            head;   /* write index */
    int            tail;   /* read index */
    int            count;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
} AineInputQueue;

static AineInputQueue s_queue;
static pthread_once_t s_once = PTHREAD_ONCE_INIT;

static void queue_init(void)
{
    memset(&s_queue, 0, sizeof(s_queue));
    pthread_mutex_init(&s_queue.lock, NULL);
    pthread_cond_init(&s_queue.cond, NULL);
}

/* Push an event from the producer (NSEvent callback, main thread) */
void aine_input_push(const AineInputEvent *ev)
{
    pthread_once(&s_once, queue_init);
    pthread_mutex_lock(&s_queue.lock);

    if (s_queue.count < QUEUE_CAPACITY) {
        s_queue.buf[s_queue.head] = *ev;
        s_queue.head = (s_queue.head + 1) % QUEUE_CAPACITY;
        s_queue.count++;
        pthread_cond_signal(&s_queue.cond);
    } else {
        /* Queue full — drop oldest event */
        s_queue.tail = (s_queue.tail + 1) % QUEUE_CAPACITY;
        s_queue.buf[s_queue.head] = *ev;
        s_queue.head = (s_queue.head + 1) % QUEUE_CAPACITY;
        /* count stays at QUEUE_CAPACITY */
    }
    pthread_mutex_unlock(&s_queue.lock);
}

/* Non-blocking poll: fills *ev and returns 1 if event available, else 0 */
int aine_input_poll(AineInputEvent *ev)
{
    pthread_once(&s_once, queue_init);
    pthread_mutex_lock(&s_queue.lock);

    int got = 0;
    if (s_queue.count > 0) {
        *ev = s_queue.buf[s_queue.tail];
        s_queue.tail = (s_queue.tail + 1) % QUEUE_CAPACITY;
        s_queue.count--;
        got = 1;
    }
    pthread_mutex_unlock(&s_queue.lock);
    return got;
}

/* Blocking poll with timeout_ms (-1 = wait forever) */
int aine_input_poll_wait(AineInputEvent *ev, int timeout_ms)
{
    pthread_once(&s_once, queue_init);
    pthread_mutex_lock(&s_queue.lock);

    int got = 0;
    if (s_queue.count == 0 && timeout_ms != 0) {
        if (timeout_ms < 0) {
            pthread_cond_wait(&s_queue.cond, &s_queue.lock);
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec  += timeout_ms / 1000;
            ts.tv_nsec += (timeout_ms % 1000) * 1000000LL;
            if (ts.tv_nsec >= 1000000000LL) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000LL;
            }
            pthread_cond_timedwait(&s_queue.cond, &s_queue.lock, &ts);
        }
    }
    if (s_queue.count > 0) {
        *ev = s_queue.buf[s_queue.tail];
        s_queue.tail = (s_queue.tail + 1) % QUEUE_CAPACITY;
        s_queue.count--;
        got = 1;
    }
    pthread_mutex_unlock(&s_queue.lock);
    return got;
}

int aine_input_pending(void)
{
    pthread_once(&s_once, queue_init);
    pthread_mutex_lock(&s_queue.lock);
    int n = s_queue.count;
    pthread_mutex_unlock(&s_queue.lock);
    return n;
}

void aine_input_flush(void)
{
    pthread_once(&s_once, queue_init);
    pthread_mutex_lock(&s_queue.lock);
    s_queue.head = s_queue.tail = s_queue.count = 0;
    pthread_mutex_unlock(&s_queue.lock);
}
