// AINE: src/aine-binder/linux/ashmem-compat.cpp
// En Linux, intenta /dev/ashmem primero (módulo kernel),
// cae back a shm_open si no está disponible.

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

static int g_has_ashmem_kernel = -1; // -1 = no detectado

static int check_ashmem_kernel(void) {
    if (g_has_ashmem_kernel >= 0) return g_has_ashmem_kernel;
    int fd = open("/dev/ashmem", O_RDWR | O_CLOEXEC);
    g_has_ashmem_kernel = (fd >= 0) ? 1 : 0;
    if (fd >= 0) close(fd);
    return g_has_ashmem_kernel;
}

extern "C" {

// Crea una región de memoria compartida anónima de 'size' bytes
// Devuelve un fd mapeado y listo para usar
int aine_ashmem_create(const char *name, size_t size) {
    if (check_ashmem_kernel()) {
        // Usar /dev/ashmem nativo (Linux con módulo binder)
        int fd = open("/dev/ashmem", O_RDWR | O_CLOEXEC);
        if (fd < 0) return -1;
        // ioctl ASHMEM_SET_SIZE = _IOW(0x77, 3, size_t)
        if (ioctl(fd, _IOW(0x77, 3, size_t), size) < 0) {
            close(fd); return -1;
        }
        return fd;
    }
    // Fallback: shm_open (mismo que macOS)
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/aine-binder-%d-%p", getpid(), (void*)name);
    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
    if (fd < 0) return -1;
    ftruncate(fd, (off_t)size);
    shm_unlink(shm_name);
    return fd;
}

} // extern "C"
