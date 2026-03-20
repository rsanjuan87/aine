// AINE: src/aine-binder/linux/eventfd-compat.cpp
// En Linux, eventfd existe nativamente — este archivo solo
// provee la interfaz unificada que usa aine-binder/common/

#include <sys/eventfd.h>
#include <stdint.h>

// Interfaz unificada (misma signatura que en macOS usa pipe+atomic)
extern "C" {

int aine_eventfd_create(void) {
    return eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
}

int aine_eventfd_notify(int fd) {
    uint64_t val = 1;
    return (int)write(fd, &val, sizeof(val));
}

int aine_eventfd_wait(int fd, uint64_t *out) {
    return (int)read(fd, out, sizeof(*out));
}

} // extern "C"
