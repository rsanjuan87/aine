#pragma once
#include <stdint.h>
#define EFD_NONBLOCK 04000
#define EFD_CLOEXEC  02000000
int eventfd(unsigned int initval, int flags);
int eventfd_read(int fd, uint64_t *value);
int eventfd_write(int fd, uint64_t value);
