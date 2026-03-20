#pragma once
#include <stdint.h>
#ifndef AINE_PUBLIC
#define AINE_PUBLIC __attribute__((visibility("default")))
#endif
#define EFD_NONBLOCK 04000
#define EFD_CLOEXEC  02000000
AINE_PUBLIC int eventfd(unsigned int initval, int flags);
AINE_PUBLIC int eventfd_read(int fd, uint64_t *value);
AINE_PUBLIC int eventfd_write(int fd, uint64_t value);
