// AINE: timerfd → pipe + dispatch_source
// ART usa timerfd para timeouts internos. Implementación con GCD dispatch.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <dispatch/dispatch.h>

#include <time.h>  // AINE: CLOCK_REALTIME, CLOCK_MONOTONIC desde el sistema

// Flags
#define TFD_NONBLOCK  04000
#define TFD_CLOEXEC   02000000

struct itimerspec {
    struct timespec it_interval;
    struct timespec it_value;
};

#define TIMERFD_TABLE_SIZE 4096

typedef struct {
    int              used;
    int              pipefd[2];
    dispatch_source_t source;
    dispatch_queue_t  queue;
    int              clockid;
    uint64_t         expirations;
} timerfd_ctx_t;

static timerfd_ctx_t tfd_table[TIMERFD_TABLE_SIZE];

int aine_timerfd_create(int clockid, int flags) {
    int pipefd[2];
    if (pipe(pipefd) < 0) return -1;

    if (flags & TFD_NONBLOCK) {
        fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
        fcntl(pipefd[1], F_SETFL, O_NONBLOCK);
    }
    if (flags & TFD_CLOEXEC) {
        fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
        fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
    }

    int idx = pipefd[0] % TIMERFD_TABLE_SIZE;
    tfd_table[idx].used       = 1;
    tfd_table[idx].pipefd[0]  = pipefd[0];
    tfd_table[idx].pipefd[1]  = pipefd[1];
    tfd_table[idx].clockid    = clockid;
    tfd_table[idx].expirations = 0;
    tfd_table[idx].source     = NULL;

    return pipefd[0];
}

int aine_timerfd_settime(int fd, int flags,
                         const struct itimerspec *new_value,
                         struct itimerspec *old_value) {
    (void)flags; (void)old_value;
    int idx = fd % TIMERFD_TABLE_SIZE;
    if (!tfd_table[idx].used || tfd_table[idx].pipefd[0] != fd) {
        errno = EBADF; return -1;
    }
    timerfd_ctx_t *ctx = &tfd_table[idx];

    // Cancelar source previo
    if (ctx->source) {
        dispatch_source_cancel(ctx->source);
        dispatch_release(ctx->source);
        ctx->source = NULL;
    }

    uint64_t start_ns = (uint64_t)new_value->it_value.tv_sec * 1000000000ULL
                       + (uint64_t)new_value->it_value.tv_nsec;
    if (start_ns == 0) return 0; // disarm

    uint64_t interval_ns = (uint64_t)new_value->it_interval.tv_sec * 1000000000ULL
                          + (uint64_t)new_value->it_interval.tv_nsec;

    ctx->queue  = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    ctx->source = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
                                         0, 0, ctx->queue);

    int write_fd = ctx->pipefd[1];
    dispatch_source_set_timer(ctx->source,
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)start_ns),
        interval_ns ? interval_ns : DISPATCH_TIME_FOREVER,
        1000000ULL /* 1ms leeway */);

    dispatch_source_set_event_handler(ctx->source, ^{
        ctx->expirations++;
        uint8_t dummy = 1;
        write(write_fd, &dummy, 1);
    });
    dispatch_resume(ctx->source);
    return 0;
}

int aine_timerfd_gettime(int fd, struct itimerspec *curr_value) {
    int idx = fd % TIMERFD_TABLE_SIZE;
    if (!tfd_table[idx].used || tfd_table[idx].pipefd[0] != fd) {
        errno = EBADF; return -1;
    }
    // AINE: implementación mínima — devolver ceros (M1 suficiente)
    memset(curr_value, 0, sizeof(*curr_value));
    return 0;
}
