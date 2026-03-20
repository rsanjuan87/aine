/*
 * aine-hals/libandroid/looper.c — ALooper stubs
 *
 * Minimal event loop stubs. For real event processing, AINE will plug
 * in a platform-native run loop (NSRunLoop on macOS) in F8+.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* Android ALooper event IDs */
#define ALOOPER_EVENT_INPUT   1
#define ALOOPER_EVENT_OUTPUT  2
#define ALOOPER_EVENT_ERROR   4
#define ALOOPER_EVENT_HANGUP  8
#define ALOOPER_EVENT_INVALID 16

#define ALOOPER_POLL_WAKE     (-1)
#define ALOOPER_POLL_CALLBACK (-2)
#define ALOOPER_POLL_TIMEOUT  (-3)
#define ALOOPER_POLL_ERROR    (-4)

#define ALOOPER_PREPARE_ALLOW_NON_CALLBACKS 1

typedef struct ALooper {
    int placeholder;
} ALooper;

static ALooper s_main_looper = {0};

__attribute__((visibility("default")))
ALooper *ALooper_prepare(int opts)
{
    (void)opts;
    return &s_main_looper;
}

__attribute__((visibility("default")))
ALooper *ALooper_forThread(void)
{
    return &s_main_looper;
}

__attribute__((visibility("default")))
void ALooper_acquire(ALooper *looper)
{
    (void)looper;
}

__attribute__((visibility("default")))
void ALooper_release(ALooper *looper)
{
    (void)looper;
}

__attribute__((visibility("default")))
int ALooper_pollOnce(int timeoutMillis, int *outFd, int *outEvents, void **outData)
{
    (void)timeoutMillis; (void)outFd; (void)outEvents; (void)outData;
    /* Stub: no-op, return timeout */
    return ALOOPER_POLL_TIMEOUT;
}

__attribute__((visibility("default")))
int ALooper_pollAll(int timeoutMillis, int *outFd, int *outEvents, void **outData)
{
    return ALooper_pollOnce(timeoutMillis, outFd, outEvents, outData);
}

__attribute__((visibility("default")))
void ALooper_wake(ALooper *looper)
{
    (void)looper;
}

__attribute__((visibility("default")))
int ALooper_addFd(ALooper *looper, int fd, int ident, int events,
                  int (*callback)(int fd, int events, void *data), void *data)
{
    (void)looper; (void)fd; (void)ident; (void)events;
    (void)callback; (void)data;
    fprintf(stderr, "[aine-android] ALooper_addFd: stub (fd=%d)\n", fd);
    return 1;
}

__attribute__((visibility("default")))
int ALooper_removeFd(ALooper *looper, int fd)
{
    (void)looper; (void)fd;
    return 1;
}
