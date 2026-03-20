// AINE: src/aine-binder/macos/binder-client.cpp
// Cliente Binder para macOS — implementa la API de /dev/binder
// aine_open("/dev/binder") → fake fd
// aine_ioctl(BINDER_WRITE_READ) → Mach transact

#include "../include/aine-binder.h"
#include "mach-transport.h"
#include "../common/parcel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdatomic.h>
#include <pthread.h>

// =============================================================================
// Tabla de fake fds
// =============================================================================

#define MAX_BINDER_FDS 64

typedef struct {
    int      in_use;
    int      fake_fd;
} binder_fd_entry_t;

static binder_fd_entry_t g_fds[MAX_BINDER_FDS];
static pthread_mutex_t   g_fds_mutex  = PTHREAD_MUTEX_INITIALIZER;
static _Atomic int       g_next_fd    = 1000000;

static int alloc_fake_fd(void) {
    int fd = atomic_fetch_add(&g_next_fd, 1);
    pthread_mutex_lock(&g_fds_mutex);
    for (int i = 0; i < MAX_BINDER_FDS; i++) {
        if (!g_fds[i].in_use) {
            g_fds[i].in_use  = 1;
            g_fds[i].fake_fd = fd;
            pthread_mutex_unlock(&g_fds_mutex);
            return fd;
        }
    }
    pthread_mutex_unlock(&g_fds_mutex);
    return -1;
}

static int is_binder_fd(int fd) {
    pthread_mutex_lock(&g_fds_mutex);
    for (int i = 0; i < MAX_BINDER_FDS; i++) {
        if (g_fds[i].in_use && g_fds[i].fake_fd == fd) {
            pthread_mutex_unlock(&g_fds_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_fds_mutex);
    return 0;
}

static void free_fake_fd(int fd) {
    pthread_mutex_lock(&g_fds_mutex);
    for (int i = 0; i < MAX_BINDER_FDS; i++) {
        if (g_fds[i].in_use && g_fds[i].fake_fd == fd) {
            g_fds[i].in_use = 0;
            break;
        }
    }
    pthread_mutex_unlock(&g_fds_mutex);
}

// =============================================================================
// API pública
// =============================================================================

extern "C" {

int aine_binder_open(void) {
    // Asegurarse de que el daemon está corriendo
    if (!aine_binder_daemon_is_running()) {
        if (aine_binder_daemon_start() < 0) {
            fprintf(stderr, "[aine-binder] cannot start daemon\n");
            errno = ENOENT;
            return -1;
        }
    }
    int fd = alloc_fake_fd();
    if (fd < 0) { errno = EMFILE; return -1; }
    return fd;
}

int aine_binder_close(int fake_fd) {
    if (!is_binder_fd(fake_fd)) { errno = EBADF; return -1; }
    free_fake_fd(fake_fd);
    return 0;
}

int aine_binder_ioctl(int fake_fd, unsigned long request, void *arg) {
    if (!is_binder_fd(fake_fd)) { errno = EBADF; return -1; }

    if (request == BINDER_WRITE_READ) {
        struct binder_write_read *bwr = (struct binder_write_read *)arg;
        if (!bwr) { errno = EINVAL; return -1; }

        if (bwr->write_size > 0 && bwr->write_buffer) {
            const uint8_t *wbuf = (const uint8_t *)bwr->write_buffer;
            uint32_t       wsz  = (uint32_t)bwr->write_size;

            // sendrecv: enviar al daemon via Unix socket
            static uint8_t recv_buf[AINE_BINDER_MSG_MAX];
            uint32_t recv_size = sizeof(recv_buf);

            int sock = aine_transport_connect();
            int r = -1;
            if (sock >= 0) {
                if (aine_transport_send(sock, wbuf, wsz) == 0)
                    r = aine_transport_recv(sock, recv_buf, &recv_size);
                close(sock);
            }
            bwr->write_consumed = bwr->write_size;

            if (r < 0) {
                // Daemon no respondió — devolver error BR
                if (bwr->read_size >= 8 && bwr->read_buffer) {
                    uint8_t *rbuf = (uint8_t *)bwr->read_buffer;
                    uint32_t cmd  = BR_ERROR;
                    int32_t  err  = -32; // EPIPE
                    memcpy(rbuf,     &cmd, 4);
                    memcpy(rbuf + 4, &err, 4);
                    bwr->read_consumed = 8;
                }
                return 0;
            }

            if (bwr->read_size > 0 && bwr->read_buffer) {
                uint8_t *rbuf = (uint8_t *)bwr->read_buffer;
                uint32_t copy = recv_size < (uint32_t)bwr->read_size
                              ? recv_size : (uint32_t)bwr->read_size;
                memcpy(rbuf, recv_buf, copy);
                bwr->read_consumed = copy;
            }
        } else if (bwr->read_size > 0 && bwr->read_buffer) {
            // Solo read — devolver BR_NOOP
            uint8_t *rbuf = (uint8_t *)bwr->read_buffer;
            if (bwr->read_size >= 4) {
                uint32_t noop = BR_NOOP;
                memcpy(rbuf, &noop, 4);
                bwr->read_consumed = 4;
            }
        }
        return 0;
    }

    if (request == BINDER_SET_MAX_THREADS ||
        request == BINDER_THREAD_EXIT     ||
        request == BINDER_SET_CONTEXT_MGR) {
        return 0;  // no-op
    }

    if (request == BINDER_VERSION) {
        int32_t *ver = (int32_t *)arg;
        if (ver) *ver = BINDER_CURRENT_PROTOCOL_VERSION;
        return 0;
    }

    errno = ENOTTY;
    return -1;
}

} // extern "C"

// =============================================================================
// main() para aine-service — lista servicios registrados en el daemon
// =============================================================================

#ifdef AINE_SERVICE_MAIN
extern "C" int aine_svc_list(int binder_fd, int index, char *out, size_t out_size);
extern "C" int aine_svc_add(int binder_fd, const char *name);

int main(int argc, char *argv[]) {
    int fd = aine_binder_open();
    if (fd < 0) {
        fprintf(stderr, "aine-service: cannot open binder\n");
        return 1;
    }

    if (argc > 1 && strcmp(argv[1], "add") == 0 && argc > 2) {
        // aine-service add <name>
        int r = aine_svc_add(fd, argv[2]);
        if (r == 0)
            printf("[OK] service '%s' registered\n", argv[2]);
        else
            printf("[FAIL] could not register '%s'\n", argv[2]);
        aine_binder_close(fd);
        return r;
    }

    // List all services
    printf("Services registered in aine-binder:\n");
    char name[256];
    int  count = 0;
    for (int i = 0; i < 128; i++) {
        if (aine_svc_list(fd, i, name, sizeof(name)) < 0) break;
        printf("  %d: %s\n", i, name);
        count++;
    }
    if (count == 0)
        printf("  (no services registered)\n");

    aine_binder_close(fd);
    return 0;
}
#endif
