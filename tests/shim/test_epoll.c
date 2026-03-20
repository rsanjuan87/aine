// AINE: epoll shim tests — epoll→kqueue (M1)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "../../src/aine-shim/include/sys/epoll.h"

#define ASSERT(cond, msg) \
    do { if (!(cond)) { fprintf(stderr, "FAIL: %s (errno=%d)\n", msg, errno); return 1; } \
         else { fprintf(stdout, "PASS: %s\n", msg); } } while(0)

static int test_epoll_create(void) {
    int epfd = epoll_create(1);
    ASSERT(epfd >= 0, "epoll_create returns valid fd");
    close(epfd);
    return 0;
}

static int test_epoll_create1(void) {
    int epfd = epoll_create1(0);
    ASSERT(epfd >= 0, "epoll_create1(0) returns valid fd");
    close(epfd);
    return 0;
}

static int test_epoll_ctl_add_del(void) {
    int epfd = epoll_create1(0);
    ASSERT(epfd >= 0, "epoll_ctl_add_del: create epfd");

    int pipefd[2];
    ASSERT(pipe(pipefd) == 0, "epoll_ctl_add_del: create pipe");

    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = pipefd[0];
    ASSERT(epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &ev) == 0,
           "epoll_ctl EPOLL_CTL_ADD");
    ASSERT(epoll_ctl(epfd, EPOLL_CTL_DEL, pipefd[0], NULL) == 0,
           "epoll_ctl EPOLL_CTL_DEL");

    close(pipefd[0]); close(pipefd[1]); close(epfd);
    return 0;
}

static int test_epoll_wait_readable(void) {
    int epfd = epoll_create1(0);
    int pipefd[2];
    ASSERT(pipe(pipefd) == 0, "epoll_wait_readable: create pipe");

    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = pipefd[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &ev);

    write(pipefd[1], "x", 1);

    struct epoll_event events[4];
    int n = epoll_wait(epfd, events, 4, 100);
    ASSERT(n == 1, "epoll_wait returns 1 ready fd");
    ASSERT(events[0].events & EPOLLIN, "epoll_wait reports EPOLLIN");
    ASSERT(events[0].data.fd == pipefd[0], "epoll_wait returns correct fd");

    close(pipefd[0]); close(pipefd[1]); close(epfd);
    return 0;
}

static int test_epoll_wait_timeout(void) {
    int epfd = epoll_create1(0);
    int pipefd[2];
    pipe(pipefd);

    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = pipefd[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &ev);

    struct epoll_event events[1];
    int n = epoll_wait(epfd, events, 1, 10);
    ASSERT(n == 0, "epoll_wait times out when no data");

    close(pipefd[0]); close(pipefd[1]); close(epfd);
    return 0;
}

int main(void) {
    int failures = 0;
    failures += test_epoll_create();
    failures += test_epoll_create1();
    failures += test_epoll_ctl_add_del();
    failures += test_epoll_wait_readable();
    failures += test_epoll_wait_timeout();
    if (failures == 0) printf("\nAll epoll tests PASSED\n");
    return failures;
}
