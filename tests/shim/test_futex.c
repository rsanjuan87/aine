// AINE: futex shim tests — futex→pthread_cond (M1)
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

#define ASSERT(cond, msg) \
    do { if (!(cond)) { fprintf(stderr, "FAIL: %s (errno=%d)\n", msg, errno); return 1; } \
         else { fprintf(stdout, "PASS: %s\n", msg); } } while(0)

// AINE: aine_futex declarado en macos/futex.c, linkado via aine-shim
extern long aine_futex(int *uaddr, int futex_op, int val,
                        const struct timespec *timeout, int *uaddr2, int val3);

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

static volatile int shared_val = 0;

typedef struct { int *addr; int wait_val; } thread_args_t;

static void *waiter_thread(void *arg) {
    thread_args_t *a = (thread_args_t *)arg;
    aine_futex(a->addr, FUTEX_WAIT, a->wait_val, NULL, NULL, 0);
    return NULL;
}

static int test_futex_wake(void) {
    int val = 0;
    thread_args_t args = { &val, 0 };
    pthread_t thr;
    pthread_create(&thr, NULL, waiter_thread, &args);
    usleep(10000); // dar tiempo al waiter
    long woken = aine_futex(&val, FUTEX_WAKE, 1, NULL, NULL, 0);
    pthread_join(thr, NULL);
    ASSERT(woken == 1, "futex FUTEX_WAKE wakes 1 waiter");
    return 0;
}

static int test_futex_wait_mismatch(void) {
    // Si el valor no coincide, FUTEX_WAIT debe retornar inmediatamente
    int val = 42;
    long rc = aine_futex(&val, FUTEX_WAIT, 99 /* != 42 */, NULL, NULL, 0);
    ASSERT(rc == 0, "futex FUTEX_WAIT returns immediately on value mismatch");
    return 0;
}

static int test_futex_timeout(void) {
    int val = 0;
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 5000000 /* 5ms */ };
    long rc = aine_futex(&val, FUTEX_WAIT, 0, &ts, NULL, 0);
    // Debe retornar -1 con errno ETIMEDOUT
    ASSERT(rc == -1 && errno == ETIMEDOUT, "futex FUTEX_WAIT times out");
    return 0;
}

int main(void) {
    int failures = 0;
    failures += test_futex_wait_mismatch();
    failures += test_futex_timeout();
    failures += test_futex_wake();
    if (failures == 0) printf("\nAll futex tests PASSED\n");
    return failures;
}
