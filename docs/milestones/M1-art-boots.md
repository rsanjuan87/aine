# M1 — ART arranca en macOS

**Target:** Meses 1–3 (tras M0)
**Criterio de éxito:** `dalvikvm` ejecuta un `.dex` de "hola mundo" y devuelve el string en stdout.

---

## El momento de la verdad

M1 es el primer milestone técnico real. Cuando `HelloWorld.dex` corra en macOS via ART de AINE, habrás demostrado que el concepto es viable. Es el equivalente de "Wine ejecuta el primer .exe".

## Implementaciones reales de aine-shim

### epoll → kqueue

```c
// src/aine-shim/epoll.c
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "aine-shim.h"

// Tabla de fds epoll (epfd → kqueue fd)
#define MAX_EPOLL_FDS 1024
static int epoll_to_kqueue[MAX_EPOLL_FDS];
static struct { epoll_data_t data; } epoll_userdata[MAX_EPOLL_FDS][1024];

int epoll_create1(int flags) {
    int kq = kqueue();
    if (kq < 0) return -1;
    // (void)flags; — EPOLL_CLOEXEC ignorado por ahora
    return kq;
}

int epoll_create(int size) {
    (void)size;
    return epoll_create1(0);
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    struct kevent kev;
    int filter = 0;

    if (event && (event->events & EPOLLIN))  filter = EVFILT_READ;
    if (event && (event->events & EPOLLOUT)) filter = EVFILT_WRITE;

    switch (op) {
        case EPOLL_CTL_ADD:
        case EPOLL_CTL_MOD:
            EV_SET(&kev, fd, filter, EV_ADD | EV_ENABLE, 0, 0,
                   event ? (void*)(uintptr_t)event->data.fd : NULL);
            break;
        case EPOLL_CTL_DEL:
            EV_SET(&kev, fd, filter, EV_DELETE, 0, 0, NULL);
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    return kevent(epfd, &kev, 1, NULL, 0, NULL);
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    struct timespec ts, *tsp = NULL;
    if (timeout >= 0) {
        ts.tv_sec  = timeout / 1000;
        ts.tv_nsec = (timeout % 1000) * 1000000L;
        tsp = &ts;
    }
    struct kevent kevents[maxevents];
    int n = kevent(epfd, NULL, 0, kevents, maxevents, tsp);
    if (n < 0) return -1;

    for (int i = 0; i < n; i++) {
        events[i].events = 0;
        if (kevents[i].filter == EVFILT_READ)  events[i].events |= EPOLLIN;
        if (kevents[i].filter == EVFILT_WRITE) events[i].events |= EPOLLOUT;
        if (kevents[i].flags & EV_EOF)         events[i].events |= EPOLLHUP;
        if (kevents[i].flags & EV_ERROR)       events[i].events |= EPOLLERR;
        events[i].data.fd = (int)(uintptr_t)kevents[i].udata;
    }
    return n;
}
```

### /proc/self/maps → mach_vm_region

```c
// src/aine-shim/proc.c
#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

// Genera un archivo temporal con el contenido de /proc/self/maps
// usando mach_vm_region() iterativamente
int aine_generate_proc_maps(void) {
    char tmppath[] = "/tmp/aine-maps-XXXXXX";
    int fd = mkstemp(tmppath);
    if (fd < 0) return -1;
    unlink(tmppath); // El fd sigue válido tras unlink

    mach_port_t task = mach_task_self();
    mach_vm_address_t addr = 0;
    mach_vm_size_t size = 0;
    uint32_t depth = 1;

    while (1) {
        struct vm_region_submap_info_64 info;
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;

        kern_return_t kr = mach_vm_region_recurse(
            task, &addr, &size, &depth,
            (vm_region_recurse_info_t)&info, &count);

        if (kr != KERN_SUCCESS) break;

        // Formato: start-end perms offset dev inode pathname
        char perms[5] = "----";
        if (info.protection & VM_PROT_READ)    perms[0] = 'r';
        if (info.protection & VM_PROT_WRITE)   perms[1] = 'w';
        if (info.protection & VM_PROT_EXECUTE) perms[2] = 'x';
        perms[3] = 'p';

        char line[256];
        snprintf(line, sizeof(line),
            "%llx-%llx %s 00000000 00:00 0\n",
            (unsigned long long)addr,
            (unsigned long long)(addr + size),
            perms);
        write(fd, line, strlen(line));

        addr += size;
        size = 0;
    }

    lseek(fd, 0, SEEK_SET);
    return fd;
}

// Interpose open() para interceptar /proc paths
int aine_open(const char *path, int flags, ...) {
    if (path && strncmp(path, "/proc/self/maps", 15) == 0) {
        return aine_generate_proc_maps();
    }
    if (path && strncmp(path, "/proc/self/", 11) == 0) {
        // Otros /proc paths: devolver fd vacío por ahora
        return open("/dev/null", O_RDONLY);
    }
    // Llamada original
    va_list args;
    va_start(args, flags);
    int mode = va_arg(args, int);
    va_end(args);
    return open(path, flags, mode);
}
```

### futex → pthread_cond

```c
// src/aine-shim/futex.c
// Implementación básica de FUTEX_WAIT y FUTEX_WAKE
// usando un hashmap de (address → pthread_cond)

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_PRIVATE_FLAG 128

#define FUTEX_TABLE_SIZE 256

typedef struct futex_entry {
    uintptr_t addr;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int             waiters;
    struct futex_entry *next;
} futex_entry_t;

static futex_entry_t *futex_table[FUTEX_TABLE_SIZE];
static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;

static futex_entry_t *get_or_create_entry(uintptr_t addr) {
    int bucket = addr % FUTEX_TABLE_SIZE;
    for (futex_entry_t *e = futex_table[bucket]; e; e = e->next) {
        if (e->addr == addr) return e;
    }
    futex_entry_t *e = calloc(1, sizeof(*e));
    e->addr = addr;
    pthread_mutex_init(&e->mutex, NULL);
    pthread_cond_init(&e->cond, NULL);
    e->next = futex_table[bucket];
    futex_table[bucket] = e;
    return e;
}

long aine_futex(int *uaddr, int futex_op, int val,
                const struct timespec *timeout, int *uaddr2, int val3) {
    (void)uaddr2; (void)val3;
    int op = futex_op & ~FUTEX_PRIVATE_FLAG;
    uintptr_t addr = (uintptr_t)uaddr;

    pthread_mutex_lock(&table_lock);
    futex_entry_t *e = get_or_create_entry(addr);
    pthread_mutex_unlock(&table_lock);

    switch (op) {
        case FUTEX_WAIT:
            pthread_mutex_lock(&e->mutex);
            if (*uaddr == val) {
                e->waiters++;
                pthread_cond_wait(&e->cond, &e->mutex);
                e->waiters--;
            }
            pthread_mutex_unlock(&e->mutex);
            return 0;

        case FUTEX_WAKE: {
            pthread_mutex_lock(&e->mutex);
            int woken = 0;
            while (woken < val && e->waiters > 0) {
                pthread_cond_signal(&e->cond);
                woken++;
            }
            pthread_mutex_unlock(&e->mutex);
            return woken;
        }

        default:
            errno = ENOSYS;
            return -1;
    }
}
```

## Ajuste de ART para páginas 16KB

```cmake
# En CMakeLists.txt de art_standalone / aine-art:
target_compile_definitions(art PRIVATE
    PRODUCT_MAX_PAGE_SIZE_SUPPORTED=16384
)
```

Y en el arranque, workaround temporal:
```bash
DYLD_INSERT_LIBRARIES=libaine-shim.dylib \
dalvikvm -Xnoimage-dex2oat -Xusejit:false \
         -cp HelloWorld.dex HelloWorld
```

## Definition of Done — M1

- [ ] `./scripts/run-app.sh --test-art` imprime "AINE: ART Runtime funcional"
- [ ] ART inicializa sin crash (el shim intercepta todas las syscalls necesarias)
- [ ] Tests de shim pasando: `cmake --build build --target test`
- [ ] Documentado en `docs/build-errors-m0.md` qué syscalls adicionales se necesitaron

## Siguiente: M2

Con ART arrancando, M2 implementa Binder sobre Mach IPC.
Ver: `docs/milestones/M2-binder.md`
