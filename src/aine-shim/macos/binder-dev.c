// AINE: src/aine-shim/macos/binder-dev.c
// Interceptación de /dev/binder en macOS — cliente Unix socket inline
// No depende de libaine-binder para evitar dependencia circular.
// El daemon (aine-binder-daemon) escucha en /tmp/aine-binder.sock.

#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>

// =============================================================================
// Constantes del protocolo (duplicadas de aine-binder.h para evitar dep)
// =============================================================================

#define AINE_BINDER_SOCKET_PATH "/tmp/aine-binder.sock"
#define AINE_BINDER_MSG_MAX     (64 * 1024)

#define BINDER_WRITE_READ       0xc0306201
#define BINDER_SET_MAX_THREADS  0x40046205
#define BINDER_SET_CONTEXT_MGR  0x40046207
#define BINDER_THREAD_EXIT      0x40046208
#define BINDER_VERSION          0xc0046209
#define BINDER_CURRENT_PROTOCOL_VERSION 8

#define BR_NOOP  0x0000630c
#define BR_ERROR 0x80046300

// =============================================================================
// Tabla de fake fds
// =============================================================================

#define MAX_BINDER_FDS 64

typedef struct {
    int in_use;
    int fake_fd;
} binder_slot_t;

static binder_slot_t   g_bfds[MAX_BINDER_FDS];
static pthread_mutex_t g_bfds_mtx = PTHREAD_MUTEX_INITIALIZER;
static _Atomic int     g_next_bfd  = (int)0xBD000001;

static int bfd_alloc(void) {
    int fd = atomic_fetch_add(&g_next_bfd, 1);
    pthread_mutex_lock(&g_bfds_mtx);
    for (int i = 0; i < MAX_BINDER_FDS; i++) {
        if (!g_bfds[i].in_use) {
            g_bfds[i].in_use = 1;
            g_bfds[i].fake_fd = fd;
            pthread_mutex_unlock(&g_bfds_mtx);
            return fd;
        }
    }
    pthread_mutex_unlock(&g_bfds_mtx);
    return -1;
}

int aine_is_binder_fd(int fd) {
    if ((unsigned int)fd < 0xBD000000u) return 0;
    pthread_mutex_lock(&g_bfds_mtx);
    for (int i = 0; i < MAX_BINDER_FDS; i++) {
        if (g_bfds[i].in_use && g_bfds[i].fake_fd == fd) {
            pthread_mutex_unlock(&g_bfds_mtx);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_bfds_mtx);
    return 0;
}

void bfd_free(int fd) {
    pthread_mutex_lock(&g_bfds_mtx);
    for (int i = 0; i < MAX_BINDER_FDS; i++) {
        if (g_bfds[i].in_use && g_bfds[i].fake_fd == fd) {
            g_bfds[i].in_use = 0;
            break;
        }
    }
    pthread_mutex_unlock(&g_bfds_mtx);
}

// =============================================================================
// Unix socket transport helpers (same protocol as binder-transport.cpp)
// =============================================================================

static int sock_write_all(int fd, const void *buf, size_t n) {
    const uint8_t *p = (const uint8_t *)buf;
    while (n > 0) {
        ssize_t r = write(fd, p, n);
        if (r <= 0) return -1;
        p += r; n -= (size_t)r;
    }
    return 0;
}

static int sock_read_all(int fd, void *buf, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    while (n > 0) {
        ssize_t r = read(fd, p, n);
        if (r <= 0) return -1;
        p += r; n -= (size_t)r;
    }
    return 0;
}

static int sock_connect(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, AINE_BINDER_SOCKET_PATH,
            sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

// Transacción síncrona: envía write_buf al daemon y recibe respuesta.
static int binder_transact(const void *write_buf, uint32_t write_size,
                            void *read_buf,  uint32_t *read_size) {
    int sock = sock_connect();
    if (sock < 0) return -1;

    uint32_t payload = write_size > (uint32_t)AINE_BINDER_MSG_MAX
                     ? (uint32_t)AINE_BINDER_MSG_MAX : write_size;
    if (sock_write_all(sock, &payload, 4) ||
        sock_write_all(sock, write_buf, payload)) {
        close(sock);
        return -1;
    }

    uint32_t n;
    if (sock_read_all(sock, &n, 4)) { close(sock); return -1; }
    if (n > *read_size)              { close(sock); return -1; }
    if (n && sock_read_all(sock, read_buf, n)) { close(sock); return -1; }
    *read_size = n;
    close(sock);
    return 0;
}

// =============================================================================
// Public API (llamada desde proc.c via aine_open)
// =============================================================================

int aine_binder_shim_open(void) {
    if (sock_connect() < 0) {
        fprintf(stderr, "[aine-shim] /dev/binder: daemon not at %s\n",
            AINE_BINDER_SOCKET_PATH);
        errno = ENOENT;
        return -1;
    }
    int fd = bfd_alloc();
    if (fd < 0) { errno = EMFILE; return -1; }
    return fd;
}

int aine_binder_shim_ioctl(int fd, unsigned long request, void *arg) {
    if (!aine_is_binder_fd(fd)) { errno = EBADF; return -1; }

    typedef struct {
        uint64_t write_size, write_consumed;
        uint64_t write_buffer;
        uint64_t read_size, read_consumed;
        uint64_t read_buffer;
    } bwr_t;

    if (request == BINDER_WRITE_READ) {
        bwr_t *bwr = (bwr_t *)arg;
        if (!bwr) { errno = EINVAL; return -1; }

        if (bwr->write_size > 0 && bwr->write_buffer) {
            uint8_t  rbuf[AINE_BINDER_MSG_MAX];
            uint32_t rsize = sizeof(rbuf);
            int r = binder_transact(
                (const void *)(uintptr_t)bwr->write_buffer,
                (uint32_t)bwr->write_size,
                rbuf, &rsize);
            bwr->write_consumed = bwr->write_size;
            if (r < 0) {
                if (bwr->read_size >= 8 && bwr->read_buffer) {
                    uint8_t *rb = (uint8_t *)(uintptr_t)bwr->read_buffer;
                    uint32_t cmd = BR_ERROR; int32_t err = -32;
                    memcpy(rb,     &cmd, 4);
                    memcpy(rb + 4, &err, 4);
                    bwr->read_consumed = 8;
                }
                return 0;
            }
            if (bwr->read_size > 0 && bwr->read_buffer) {
                uint32_t cp = rsize < (uint32_t)bwr->read_size
                            ? rsize : (uint32_t)bwr->read_size;
                memcpy((void *)(uintptr_t)bwr->read_buffer, rbuf, cp);
                bwr->read_consumed = cp;
            }
        } else if (bwr->read_size > 0 && bwr->read_buffer) {
            uint8_t *rb = (uint8_t *)(uintptr_t)bwr->read_buffer;
            if (bwr->read_size >= 4) {
                uint32_t noop = BR_NOOP;
                memcpy(rb, &noop, 4);
                bwr->read_consumed = 4;
            }
        }
        return 0;
    }

    if (request == BINDER_SET_MAX_THREADS ||
        request == BINDER_THREAD_EXIT     ||
        request == BINDER_SET_CONTEXT_MGR) return 0;

    if (request == BINDER_VERSION) {
        if (arg) *(int32_t *)arg = BINDER_CURRENT_PROTOCOL_VERSION;
        return 0;
    }

    errno = ENOTTY;
    return -1;
}

int aine_binder_shim_close(int fd) {
    if (!aine_is_binder_fd(fd)) { errno = EBADF; return -1; }
    bfd_free(fd);
    return 0;
}

// End of binder-dev.c
