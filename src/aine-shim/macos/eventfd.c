// AINE: eventfd → pipe + contador atómico
// Linux eventfd: fd que actúa como contador; read() bloquea hasta valor>0
#include "../include/sys/eventfd.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#define EVENTFD_TABLE_SIZE 4096

typedef struct {
    int         used;
    int         pipefd[2];   // [0]=read, [1]=write
    uint64_t    counter;
    pthread_mutex_t lock;
    int         flags;
} eventfd_ctx_t;

static eventfd_ctx_t efd_table[EVENTFD_TABLE_SIZE];

// Devuelve el fd de lectura del pipe como "eventfd fd"
int eventfd(unsigned int initval, int flags) {
    int pipefd[2];
    if (pipe(pipefd) < 0) return -1;

    // Configurar flags
    if (flags & EFD_NONBLOCK) {
        fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
        fcntl(pipefd[1], F_SETFL, O_NONBLOCK);
    }
    if (flags & EFD_CLOEXEC) {
        fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
        fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
    }

    int idx = pipefd[0] % EVENTFD_TABLE_SIZE;
    efd_table[idx].used       = 1;
    efd_table[idx].pipefd[0]  = pipefd[0];
    efd_table[idx].pipefd[1]  = pipefd[1];
    efd_table[idx].counter    = (uint64_t)initval;
    efd_table[idx].flags      = flags;
    pthread_mutex_init(&efd_table[idx].lock, NULL);

    // AINE: si initval>0, escribir dummy byte para que el fd sea readable
    if (initval > 0) {
        uint8_t dummy = 1;
        write(pipefd[1], &dummy, 1);
    }

    return pipefd[0];
}

int eventfd_read(int fd, uint64_t *value) {
    int idx = fd % EVENTFD_TABLE_SIZE;
    if (!efd_table[idx].used || efd_table[idx].pipefd[0] != fd) {
        errno = EBADF; return -1;
    }
    eventfd_ctx_t *ctx = &efd_table[idx];
    pthread_mutex_lock(&ctx->lock);

    // Drenar el pipe
    uint8_t dummy;
    ssize_t n = read(ctx->pipefd[0], &dummy, 1);
    if (n < 0) { pthread_mutex_unlock(&ctx->lock); return -1; }

    *value = ctx->counter;
    ctx->counter = 0;
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

int eventfd_write(int fd, uint64_t value) {
    int idx = fd % EVENTFD_TABLE_SIZE;
    if (!efd_table[idx].used || efd_table[idx].pipefd[0] != fd) {
        errno = EBADF; return -1;
    }
    eventfd_ctx_t *ctx = &efd_table[idx];
    pthread_mutex_lock(&ctx->lock);

    // AINE: desbordamiento → EAGAIN (semántica Linux)
    if (ctx->counter > UINT64_MAX - value) {
        pthread_mutex_unlock(&ctx->lock);
        errno = EAGAIN;
        return -1;
    }
    ctx->counter += value;
    // Escribir dummy byte para despertar readers
    uint8_t dummy = 1;
    write(ctx->pipefd[1], &dummy, 1);
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}
