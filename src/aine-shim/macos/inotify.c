// AINE: inotify → kqueue EVFILT_VNODE
// Android usa inotify para monitorear cambios en APK dirs, dalvik-cache, etc.
// Solo implementamos los eventos que ART realmente necesita: M1 mínimo viable
#include "../include/sys/inotify.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#define INOTIFY_MAX_WATCHES 128
#define INOTIFY_TABLE_SIZE  1024

typedef struct {
    int      wd;       // watch descriptor
    int      target_fd;// fd del archivo/dir vigilado
    char     path[1024];
    uint32_t mask;
} inotify_watch_t;

typedef struct {
    int              kq;      // kqueue fd
    int              used;
    int              next_wd;
    inotify_watch_t  watches[INOTIFY_MAX_WATCHES];
} inotify_ctx_t;

static inotify_ctx_t inotify_table[INOTIFY_TABLE_SIZE];

int inotify_init(void) {
    return inotify_init1(0);
}

int inotify_init1(int flags) {
    int kq = kqueue();
    if (kq < 0) return -1;

    if (flags & IN_NONBLOCK)
        fcntl(kq, F_SETFL, O_NONBLOCK);
    if (flags & IN_CLOEXEC)
        fcntl(kq, F_SETFD, FD_CLOEXEC);

    int idx = kq % INOTIFY_TABLE_SIZE;
    inotify_table[idx].kq      = kq;
    inotify_table[idx].used    = 1;
    inotify_table[idx].next_wd = 1;
    return kq;
}

int inotify_add_watch(int fd, const char *pathname, uint32_t mask) {
    int idx = fd % INOTIFY_TABLE_SIZE;
    if (!inotify_table[idx].used) { errno = EBADF; return -1; }
    inotify_ctx_t *ctx = &inotify_table[idx];

    // Buscar slot libre
    int slot = -1;
    for (int i = 0; i < INOTIFY_MAX_WATCHES; i++) {
        if (!ctx->watches[i].wd) { slot = i; break; }
    }
    if (slot < 0) { errno = ENOSPC; return -1; }

    int target = open(pathname, O_RDONLY | O_EVTONLY);
    if (target < 0) return -1;

    // Mapear máscara inotify → kqueue vnode flags
    uint32_t vnode_flags = 0;
    if (mask & (IN_MODIFY | IN_CLOSE_WRITE)) vnode_flags |= NOTE_WRITE;
    if (mask & IN_CREATE)                    vnode_flags |= NOTE_WRITE; // dir write = new entry
    if (mask & IN_DELETE)                    vnode_flags |= NOTE_DELETE;
    if (mask & IN_ATTRIB)                    vnode_flags |= NOTE_ATTRIB;
    if (mask & IN_MOVE)                      vnode_flags |= NOTE_RENAME;
    if (!vnode_flags) vnode_flags = NOTE_WRITE | NOTE_DELETE;

    struct kevent kev;
    EV_SET(&kev, target, EVFILT_VNODE, EV_ADD | EV_CLEAR, vnode_flags, 0,
           (void *)(uintptr_t)ctx->next_wd);
    if (kevent(ctx->kq, &kev, 1, NULL, 0, NULL) < 0) {
        close(target);
        return -1;
    }

    int wd = ctx->next_wd++;
    ctx->watches[slot].wd        = wd;
    ctx->watches[slot].target_fd = target;
    ctx->watches[slot].mask      = mask;
    strncpy(ctx->watches[slot].path, pathname,
            sizeof(ctx->watches[slot].path) - 1);
    return wd;
}

int inotify_rm_watch(int fd, int wd) {
    int idx = fd % INOTIFY_TABLE_SIZE;
    if (!inotify_table[idx].used) { errno = EBADF; return -1; }
    inotify_ctx_t *ctx = &inotify_table[idx];

    for (int i = 0; i < INOTIFY_MAX_WATCHES; i++) {
        if (ctx->watches[i].wd == wd) {
            struct kevent kev;
            EV_SET(&kev, ctx->watches[i].target_fd, EVFILT_VNODE,
                   EV_DELETE, 0, 0, NULL);
            kevent(ctx->kq, &kev, 1, NULL, 0, NULL);
            close(ctx->watches[i].target_fd);
            memset(&ctx->watches[i], 0, sizeof(ctx->watches[i]));
            return 0;
        }
    }
    errno = EINVAL;
    return -1;
}
