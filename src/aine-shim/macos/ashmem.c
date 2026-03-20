// AINE: ashmem → shm_open + mmap
// Android usa /dev/ashmem para memoria compartida anónima en Binder
// En macOS usamos shm_open() con nombres temporales únicos
#include "../include/linux/ashmem.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>

#define ASHMEM_TABLE_SIZE 4096

typedef struct {
    int      used;
    size_t   size;
    char     name[ASHMEM_NAME_LEN];
    char     shm_name[64]; // nombre en /dev/shm
} ashmem_ctx_t;

static ashmem_ctx_t ashmem_table[ASHMEM_TABLE_SIZE];
static atomic_int   ashmem_seq = 0;

// AINE: intercepción de open("/dev/ashmem", ...)
int aine_ashmem_create(void) {
    // Crear fd via shm_open con nombre único
    char shm_name[64];
    int seq = atomic_fetch_add(&ashmem_seq, 1);
    snprintf(shm_name, sizeof(shm_name), "/aine-ashmem-%d-%d", getpid(), seq);

    int fd = shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return -1;

    // Unlink inmediatamente — el fd sigue válido como anónimo
    shm_unlink(shm_name);

    int idx = fd % ASHMEM_TABLE_SIZE;
    ashmem_table[idx].used = 1;
    ashmem_table[idx].size = 0;
    strncpy(ashmem_table[idx].shm_name, shm_name,
            sizeof(ashmem_table[idx].shm_name) - 1);

    return fd;
}

// AINE: ioctl interceptado para ASHMEM_SET_SIZE / ASHMEM_GET_SIZE
int aine_ashmem_ioctl(int fd, unsigned long request, void *arg) {
    int idx = fd % ASHMEM_TABLE_SIZE;
    if (!ashmem_table[idx].used) {
        errno = EBADF;
        return -1;
    }
    ashmem_ctx_t *ctx = &ashmem_table[idx];

    if (request == ASHMEM_SET_SIZE) {
        size_t size = (size_t)(uintptr_t)arg;
        if (ftruncate(fd, (off_t)size) < 0) return -1;
        ctx->size = size;
        return 0;
    }
    if (request == ASHMEM_GET_SIZE) {
        return (int)ctx->size;
    }
    errno = EINVAL;
    return -1;
}
