// AINE: epoll → kqueue translation
// Referencia: Darling darlinghq/darling src/kernel/emulation/linux/poll/epoll.c
#include "../include/sys/epoll.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

// AINE: almacenamos el último filtro por fd para poder eliminarlo en EPOLL_CTL_DEL
// sin que el caller tenga que pasar event!=NULL
#define EPOLL_MAP_SIZE 4096
typedef struct {
    int     used;
    short   filter; // EVFILT_READ o EVFILT_WRITE
    epoll_data_t data;
} epoll_slot_t;

typedef struct {
    epoll_slot_t slots[EPOLL_MAP_SIZE];
} epoll_ctx_t;

// AINE: tabla global de contextos por kqueue fd (fd puede ser hasta ~rlimit)
// Uso simplificado: malloc por contexto, indexado por epfd % 1024
#define CTX_TABLE_SIZE 1024
static epoll_ctx_t *ctx_table[CTX_TABLE_SIZE];

static epoll_ctx_t *get_or_create_ctx(int epfd) {
    int idx = epfd % CTX_TABLE_SIZE;
    if (!ctx_table[idx]) {
        ctx_table[idx] = calloc(1, sizeof(epoll_ctx_t));
    }
    return ctx_table[idx];
}

int epoll_create(int size) {
    (void)size;
    return kqueue();
}

int epoll_create1(int flags) {
    (void)flags; // AINE: EPOLL_CLOEXEC ignorado — kqueue es close-on-exec por defecto en Darwin
    return kqueue();
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    struct kevent changes[2];
    int nchanges = 0;

    epoll_ctx_t *ctx = get_or_create_ctx(epfd);
    int slot = fd % EPOLL_MAP_SIZE;

    switch (op) {
        case EPOLL_CTL_ADD:
        case EPOLL_CTL_MOD: {
            if (!event) { errno = EINVAL; return -1; }
            // AINE: eliminar filtros antiguos si MOD
            if (op == EPOLL_CTL_MOD && ctx->slots[slot].used) {
                EV_SET(&changes[nchanges++], fd, ctx->slots[slot].filter,
                       EV_DELETE, 0, 0, NULL);
            }
            short filter = (event->events & EPOLLOUT) ? EVFILT_WRITE : EVFILT_READ;
            uint16_t kflags = EV_ADD | EV_ENABLE;
            if (event->events & EPOLLET) kflags |= EV_CLEAR;
            EV_SET(&changes[nchanges++], fd, filter, kflags, 0, 0,
                   (void *)(uintptr_t)event->data.fd);
            ctx->slots[slot].used   = 1;
            ctx->slots[slot].filter = filter;
            ctx->slots[slot].data   = event->data;
            break;
        }
        case EPOLL_CTL_DEL: {
            short filter = ctx->slots[slot].used
                           ? ctx->slots[slot].filter : EVFILT_READ;
            EV_SET(&changes[nchanges++], fd, filter, EV_DELETE, 0, 0, NULL);
            ctx->slots[slot].used = 0;
            break;
        }
        default:
            errno = EINVAL;
            return -1;
    }
    return kevent(epfd, changes, nchanges, NULL, 0, NULL);
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    if (maxevents <= 0) { errno = EINVAL; return -1; }
    struct timespec ts, *tsp = NULL;
    if (timeout >= 0) {
        ts.tv_sec  = timeout / 1000;
        ts.tv_nsec = (timeout % 1000) * 1000000L;
        tsp = &ts;
    }
    struct kevent *kevents = calloc(maxevents, sizeof(struct kevent));
    if (!kevents) return -1;
    int n = kevent(epfd, NULL, 0, kevents, maxevents, tsp);
    if (n < 0) { free(kevents); return -1; }
    for (int i = 0; i < n; i++) {
        events[i].events = 0;
        if (kevents[i].filter == EVFILT_READ)  events[i].events |= EPOLLIN;
        if (kevents[i].filter == EVFILT_WRITE) events[i].events |= EPOLLOUT;
        if (kevents[i].flags & EV_EOF)         events[i].events |= EPOLLHUP;
        if (kevents[i].flags & EV_ERROR)       events[i].events |= EPOLLERR;
        events[i].data.fd = (int)(uintptr_t)kevents[i].udata;
    }
    free(kevents);
    return n;
}
