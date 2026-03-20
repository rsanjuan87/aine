// AINE: src/aine-binder/macos/mach-transport.cpp (renamed to binder-transport)
// Transporte Unix socket para Binder IPC.
// Reemplaza Mach bootstrap (requiere privilegios en macOS 10.12+).

#include "mach-transport.h"
#include "../include/aine-binder.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

// ─────────────────────────────────────────────────────────────────────────────
// Wire format: [uint32_t size][data bytes]
// ─────────────────────────────────────────────────────────────────────────────

static int write_all(int fd, const void *buf, size_t n) {
    const uint8_t *p = (const uint8_t *)buf;
    while (n > 0) {
        ssize_t r = write(fd, p, n);
        if (r <= 0) return -1;
        p += r; n -= r;
    }
    return 0;
}

static int read_all(int fd, void *buf, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    while (n > 0) {
        ssize_t r = read(fd, p, n);
        if (r <= 0) return -1;
        p += r; n -= r;
    }
    return 0;
}

int aine_transport_send(int sock_fd, const void *data, uint32_t size) {
    if (write_all(sock_fd, &size, 4))  return -1;
    if (size && write_all(sock_fd, data, size)) return -1;
    return 0;
}

int aine_transport_recv(int sock_fd, void *buf, uint32_t *size) {
    uint32_t n;
    if (read_all(sock_fd, &n, 4)) return -1;
    if (n > *size) return -1;          // buffer too small
    if (n && read_all(sock_fd, buf, n)) return -1;
    *size = n;
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Client
// ─────────────────────────────────────────────────────────────────────────────

int aine_transport_connect(void) {
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

// ─────────────────────────────────────────────────────────────────────────────
// Daemon
// ─────────────────────────────────────────────────────────────────────────────

int aine_transport_daemon_listen(void) {
    unlink(AINE_BINDER_SOCKET_PATH);   // remove stale socket

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    // CLOEXEC to avoid fd leaks across fork
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    // Allow re-bind
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, AINE_BINDER_SOCKET_PATH,
            sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 16) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int aine_transport_daemon_accept(int listen_fd) {
    int fd = accept(listen_fd, NULL, NULL);
    if (fd >= 0) fcntl(fd, F_SETFD, FD_CLOEXEC);
    return fd;
}

// End of binder-transport.cpp

