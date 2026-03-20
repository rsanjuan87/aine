// AINE: src/aine-shim/linux/passthrough.c
// En Linux, la mayoría de syscalls que Android necesita existen nativamente.
// Este archivo declara explícitamente que usamos las implementaciones del kernel.
// Solo proveemos wrappers para logging cuando AINE_SHIM_DEBUG está activo.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// En Linux, aine-shim no interpone syscalls — son nativas.
// Su rol en Linux es:
//   1. Logging/debugging de llamadas de Android (cuando AINE_DEBUG=1)
//   2. Compatibilidad de API con la interfaz pública de aine-shim.h
//   3. Posibles workarounds menores de comportamiento si se detectan

#ifdef AINE_SHIM_DEBUG
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>

// Wrappers de debug opcionales — solo activos con -DAINE_ENABLE_DEBUG_SHIM=ON
// Permiten ver exactamente qué syscalls invoca Android en Linux para
// comparar con el comportamiento en macOS

void aine_shim_linux_init(void) __attribute__((constructor));
void aine_shim_linux_init(void) {
    if (getenv("AINE_LOG_LEVEL") && strcmp(getenv("AINE_LOG_LEVEL"), "debug") == 0) {
        fprintf(stderr, "[AINE-shim/linux] Debug mode active — logging Android syscalls\n");
    }
}
#endif // AINE_SHIM_DEBUG

// Versión de la capa de shim (útil para diagnóstico)
const char* aine_shim_version(void) {
    return "aine-shim/linux passthrough 0.1.0";
}

int aine_shim_platform_is_macos(void) { return 0; }
int aine_shim_platform_is_linux(void) { return 1; }
