// AINE: prctl shim tests — B4 pthread_setname_np + PR_SET_NAME (M1)
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#define ASSERT(cond, msg) \
    do { if (!(cond)) { fprintf(stderr, "FAIL: %s (errno=%d)\n", msg, errno); return 1; } \
         else { fprintf(stdout, "PASS: %s\n", msg); } } while(0)

// Declarados en macos/prctl.c, linkados via aine-shim
extern int aine_prctl(int option, unsigned long arg2, unsigned long arg3,
                       unsigned long arg4, unsigned long arg5);
extern int aine_pthread_setname_np(pthread_t thread, const char *name);

#define PR_SET_NAME 15
#define PR_GET_NAME 16

static int test_prctl_set_get_name(void) {
    int rc = aine_prctl(PR_SET_NAME, (unsigned long)"aine-test", 0, 0, 0);
    ASSERT(rc == 0, "prctl PR_SET_NAME returns 0");

    char buf[64] = {0};
    aine_prctl(PR_GET_NAME, (unsigned long)buf, 0, 0, 0);
    ASSERT(strncmp(buf, "aine-test", 9) == 0, "prctl PR_GET_NAME returns set name");

    return 0;
}

static int test_pthread_setname_self(void) {
    // B4: Linux firma es (thread, name) — macOS solo (name)
    int rc = aine_pthread_setname_np(pthread_self(), "aine-shim-test");
    ASSERT(rc == 0, "aine_pthread_setname_np for self returns 0");

    char buf[64] = {0};
    pthread_getname_np(pthread_self(), buf, sizeof(buf));
    ASSERT(strncmp(buf, "aine-shim-test", 14) == 0,
           "thread name set correctly via aine_pthread_setname_np");
    return 0;
}

static int test_prctl_dumpable(void) {
    int rc = aine_prctl(3 /* PR_GET_DUMPABLE */, 0, 0, 0, 0);
    ASSERT(rc == 1, "prctl PR_GET_DUMPABLE returns 1");
    rc = aine_prctl(4 /* PR_SET_DUMPABLE */, 1, 0, 0, 0);
    ASSERT(rc == 0, "prctl PR_SET_DUMPABLE returns 0");
    return 0;
}

static int test_prctl_unknown(void) {
    int rc = aine_prctl(9999, 0, 0, 0, 0);
    ASSERT(rc == -1 && errno == EINVAL, "prctl unknown option returns EINVAL");
    return 0;
}

int main(void) {
    int failures = 0;
    failures += test_prctl_set_get_name();
    failures += test_pthread_setname_self();
    failures += test_prctl_dumpable();
    failures += test_prctl_unknown();
    if (failures == 0) printf("\nAll prctl tests PASSED\n");
    return failures;
}
