// AINE: src/aine-shim/linux/binder-dev.c
// En Linux, /dev/binder puede estar disponible via:
//   1. Módulo kernel binder (si está cargado — común en distros con Android support)
//   2. Binder userspace de ATL (vendor/atl/binder/) — siempre disponible
//
// AINE en Linux usa el mismo Binder que ATL.
// Este archivo solo provee la lógica de detección y fallback.

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Detecta si el módulo kernel de Binder está disponible
int aine_linux_binder_kernel_available(void) {
    int fd = open("/dev/binder", O_RDWR | O_CLOEXEC);
    if (fd >= 0) {
        close(fd);
        return 1;
    }
    return 0;
}

// En Linux, aine-binder decide automáticamente:
// - Si /dev/binder existe → usar el módulo kernel (más eficiente)
// - Si no → usar el Binder userspace de ATL
// Esta función es llamada por aine-binder durante la inicialización.
int aine_linux_select_binder_backend(void) {
    if (aine_linux_binder_kernel_available()) {
        fprintf(stderr, "[AINE] Binder: usando módulo kernel\n");
        return 1; // AINE_BINDER_BACKEND_KERNEL
    }
    fprintf(stderr, "[AINE] Binder: kernel no disponible, usando userspace (ATL)\n");
    return 0; // AINE_BINDER_BACKEND_USERSPACE
}
