// AINE: futex → pthread_cond translation (Linux futex emulation)
// Solo FUTEX_WAIT y FUTEX_WAKE — suficiente para ART en M1
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define FUTEX_WAIT         0
#define FUTEX_WAKE         1
#define FUTEX_PRIVATE_FLAG 128
#define FUTEX_WAIT_PRIVATE (FUTEX_WAIT | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_PRIVATE (FUTEX_WAKE | FUTEX_PRIVATE_FLAG)

#define FUTEX_TABLE_SIZE 256

typedef struct futex_entry {
    uintptr_t           addr;
    pthread_mutex_t     mutex;
    pthread_cond_t      cond;
    int                 waiters;
    struct futex_entry *next;
} futex_entry_t;

static futex_entry_t      *futex_table[FUTEX_TABLE_SIZE];
static pthread_mutex_t     table_lock = PTHREAD_MUTEX_INITIALIZER;

static futex_entry_t *get_or_create_entry(uintptr_t addr) {
    int bucket = (int)(addr % FUTEX_TABLE_SIZE);
    for (futex_entry_t *e = futex_table[bucket]; e; e = e->next) {
        if (e->addr == addr) return e;
    }
    futex_entry_t *e = calloc(1, sizeof(*e));
    if (!e) return NULL;
    e->addr = addr;
    pthread_mutex_init(&e->mutex, NULL);
    pthread_cond_init(&e->cond, NULL);
    e->next = futex_table[bucket];
    futex_table[bucket] = e;
    return e;
}

// AINE: syscall(SYS_futex, ...) roteado aquí vía interposición de aine-shim
__attribute__((visibility("default")))
long aine_futex(int *uaddr, int futex_op, int val,
                const struct timespec *timeout, int *uaddr2, int val3) {
    (void)uaddr2; (void)val3;
    int op = futex_op & ~FUTEX_PRIVATE_FLAG;
    uintptr_t addr = (uintptr_t)uaddr;

    pthread_mutex_lock(&table_lock);
    futex_entry_t *e = get_or_create_entry(addr);
    pthread_mutex_unlock(&table_lock);
    if (!e) { errno = ENOMEM; return -1; }

    switch (op) {
        case FUTEX_WAIT: {
            pthread_mutex_lock(&e->mutex);
            if (*uaddr != val) {
                pthread_mutex_unlock(&e->mutex);
                return 0;
            }
            e->waiters++;
            int rc;
            if (timeout) {
                // AINE: timeout es relativo en Linux — convertir a absoluto
                struct timespec abs_ts;
                clock_gettime(CLOCK_REALTIME, &abs_ts);
                abs_ts.tv_sec  += timeout->tv_sec;
                abs_ts.tv_nsec += timeout->tv_nsec;
                if (abs_ts.tv_nsec >= 1000000000L) {
                    abs_ts.tv_sec++;
                    abs_ts.tv_nsec -= 1000000000L;
                }
                rc = pthread_cond_timedwait(&e->cond, &e->mutex, &abs_ts);
            } else {
                rc = pthread_cond_wait(&e->cond, &e->mutex);
            }
            e->waiters--;
            pthread_mutex_unlock(&e->mutex);
            if (rc == ETIMEDOUT) { errno = ETIMEDOUT; return -1; }
            return 0;
        }
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
