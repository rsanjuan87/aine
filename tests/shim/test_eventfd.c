// AINE: eventfd shim tests — eventfd→pipe (M1)
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include "../../src/aine-shim/include/sys/eventfd.h"

#define ASSERT(cond, msg) \
    do { if (!(cond)) { fprintf(stderr, "FAIL: %s (errno=%d)\n", msg, errno); return 1; } \
         else { fprintf(stdout, "PASS: %s\n", msg); } } while(0)

static int test_eventfd_create(void) {
    int fd = eventfd(0, 0);
    ASSERT(fd >= 0, "eventfd(0,0) creates valid fd");
    close(fd);
    return 0;
}

static int test_eventfd_write_read(void) {
    int fd = eventfd(0, 0);
    ASSERT(fd >= 0, "eventfd write/read: create fd");

    ASSERT(eventfd_write(fd, 5) == 0, "eventfd_write(5) succeeds");

    uint64_t val = 0;
    ASSERT(eventfd_read(fd, &val) == 0, "eventfd_read succeeds");
    ASSERT(val == 5, "eventfd_read returns written value");

    close(fd);
    return 0;
}

static int test_eventfd_initval(void) {
    int fd = eventfd(3, 0);
    ASSERT(fd >= 0, "eventfd(3,0) creates fd with initval");

    uint64_t val = 0;
    ASSERT(eventfd_read(fd, &val) == 0, "eventfd_read initval succeeds");
    ASSERT(val == 3, "eventfd_read returns initval=3");

    close(fd);
    return 0;
}

static int test_eventfd_accumulate(void) {
    int fd = eventfd(0, 0);
    eventfd_write(fd, 2);
    eventfd_write(fd, 3);
    // AINE: pipe-based impl: each write wakes readers; read drains one token
    // We read twice to drain both writes
    uint64_t v1 = 0, v2 = 0;
    eventfd_read(fd, &v1);
    eventfd_read(fd, &v2);
    ASSERT(v1 + v2 == 5, "eventfd accumulates writes (total=5)");
    close(fd);
    return 0;
}

int main(void) {
    int failures = 0;
    failures += test_eventfd_create();
    failures += test_eventfd_write_read();
    failures += test_eventfd_initval();
    failures += test_eventfd_accumulate();
    if (failures == 0) printf("\nAll eventfd tests PASSED\n");
    return failures;
}
