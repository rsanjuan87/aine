# AINE multiplataforma: macOS + Linux

## Principio fundamental

AINE es un fork de ATL, no un reemplazo. El objetivo es que **una sola base de código** compile y funcione en ambas plataformas:

```
vendor/atl/          ← ATL upstream (submódulo, solo lectura)
src/
├── aine-shim/       ← Syscall translation
│   ├── macos/       ← Implementaciones XNU (epoll→kqueue, etc.)
│   ├── linux/       ← Pass-through o thin wrappers sobre Linux nativo
│   └── common/      ← Código compartido (tablas, utilidades)
├── aine-binder/     ← Binder IPC
│   ├── macos/       ← Sobre Mach messages
│   ├── linux/       ← Basado en el Binder userspace de ATL
│   └── common/      ← Protocolo Binder (compartido)
└── aine-hals/       ← HAL bridges
    ├── macos/       ← Metal, CoreAudio, AVFoundation, NSEvent
    └── linux/       ← Mesa, PipeWire/ALSA, evdev, V4L2
```

## Por qué mantener Linux

1. **Desarrollo más rápido:** Linux tiene mejor tooling para desarrollo de bajo nivel (strace, perf, valgrind). Puedes iterar en la lógica de ART y Binder en Linux antes de portar a macOS.
2. **CI más barato:** GitHub Actions tiene runners Linux gratuitos. macOS runners cuestan ~10x más.
3. **Contribuidores:** La mayoría de desarrolladores de sistemas trabajan en Linux. Mantener Linux abre el proyecto a más colaboradores.
4. **Base de tests:** Los tests que pasan en Linux son la línea base. Si algo falla en macOS pero no en Linux, sabes que el problema es en la capa XNU, no en la lógica Android.
5. **ATL compatibility:** Facilita cherry-pick desde ATL — si el código de ATL funciona en AINE Linux, hay más probabilidad de que la adaptación macOS sea correcta.

## Modelo de compilación condicional

### CMake: detección de plataforma

```cmake
# cmake/platform.cmake
if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(AINE_PLATFORM "macos-arm64")
    set(AINE_PLATFORM_MACOS TRUE)
    message(STATUS "AINE platform: macOS ARM64 (Apple Silicon) — full target")

elseif(APPLE)
    set(AINE_PLATFORM "macos-x86_64")
    set(AINE_PLATFORM_MACOS TRUE)
    message(WARNING "AINE platform: macOS x86_64 — limited support (no native ARM libs)")

elseif(UNIX AND NOT APPLE)
    set(AINE_PLATFORM "linux")
    set(AINE_PLATFORM_LINUX TRUE)
    message(STATUS "AINE platform: Linux — ATL-compatible mode")

else()
    message(FATAL_ERROR "AINE: plataforma no soportada")
endif()
```

### C/C++: guards de plataforma

```c
// src/aine-shim/epoll.c

#if defined(__APPLE__)
// ── macOS: epoll sobre kqueue ──────────────────────────────────────────────
#include <sys/event.h>

int epoll_create1(int flags) {
    return kqueue(); // kqueue es el equivalente en XNU
}
// ... resto de la implementación XNU

#elif defined(__linux__)
// ── Linux: epoll existe nativamente ───────────────────────────────────────
// En Linux no necesitamos shimear epoll — existe en el kernel.
// Este archivo es prácticamente vacío en Linux.
// Solo proveemos wrappers para logging/debugging si AINE_DEBUG está activo.

#ifdef AINE_DEBUG
#include <sys/epoll.h>
// Wrappers de debug opcionales — igual que ATL pero con logging AINE
#endif // AINE_DEBUG

#endif // __APPLE__ / __linux__
```

### CMakeLists: selección de fuentes por plataforma

```cmake
# src/aine-shim/CMakeLists.txt

set(SHIM_COMMON_SOURCES
    common/logging.c
    common/interpose.c
)

if(AINE_PLATFORM_MACOS)
    set(SHIM_PLATFORM_SOURCES
        macos/epoll.c          # epoll → kqueue
        macos/proc.c           # /proc → mach_vm_region
        macos/futex.c          # futex → pthread_cond
        macos/ashmem.c         # ashmem → shm_open
        macos/eventfd.c        # eventfd → pipe+atomic
        macos/timerfd.c        # timerfd → dispatch_source
        macos/inotify.c        # inotify → FSEvents
        macos/binder-dev.c     # /dev/binder → Mach port
        macos/prctl.c          # prctl Linux-specific options
    )
    set(SHIM_PLATFORM_LIBS "-framework Foundation" "-framework CoreFoundation")

elseif(AINE_PLATFORM_LINUX)
    set(SHIM_PLATFORM_SOURCES
        linux/passthrough.c    # Thin layer — la mayoría pasan directo al kernel
        linux/binder-dev.c     # /dev/binder — puede usar el módulo kernel o userspace
    )
    set(SHIM_PLATFORM_LIBS "")
endif()

add_library(aine-shim SHARED
    ${SHIM_COMMON_SOURCES}
    ${SHIM_PLATFORM_SOURCES}
)
target_link_libraries(aine-shim PRIVATE ${SHIM_PLATFORM_LIBS})
```

## Linux en AINE: qué hereda de ATL y qué es nuevo

### Herencia directa de ATL (sin cambios en Linux)
- `bionic_translation` para glibc → ya funciona en ATL
- ART port para Linux → ya funciona en ATL
- Binder userspace Linux → ya funciona en ATL
- Mesa/OpenGL ES en Linux → ya funciona en ATL
- El 90% del Android framework Java → independiente de plataforma

### Lo nuevo de AINE en Linux
- Build system unificado CMake (ATL usa soong/ninja de AOSP)
- Capa de abstracción `aine-shim` (en Linux es casi un pass-through, útil para logging)
- Sistema de empaquetado/instalación de APKs independiente del path de ATL
- Launcher UI para Linux (GTK o Qt, planificado para M6+)
- Compatibilidad garantizada de comportamiento entre Linux y macOS via test suite compartida

## Diferencias de comportamiento Linux vs macOS

| Componente | Linux | macOS |
|---|---|---|
| Syscalls Android | Nativas del kernel | aine-shim → XNU |
| OpenGL ES | Mesa (libGL) | ANGLE → Metal |
| Audio | AudioFlinger → PipeWire/ALSA | AudioFlinger → CoreAudio |
| Input | evdev directo | NSEvent → input_event |
| Gráficos | Wayland/X11 + SurfaceFlinger | NSWindow + CAMetalLayer |
| Binder kernel | Módulo kernel (opcional) o userspace | Solo userspace (no hay módulo XNU) |
| Proceso de arranque | fork() desde Zygote-like | posix_spawn() |
| Page size | 4KB (standard) | 16KB (Apple Silicon) — requiere fix B1 |

## Test suite compartida

La estrategia de test es: **primero verde en Linux, luego verde en macOS.**

```
tests/
├── shared/              ← Tests que deben pasar en AMBAS plataformas
│   ├── test_binder_protocol.cpp    # Protocolo Binder (independiente de OS)
│   ├── test_parcel.cpp             # Serialización Parcel
│   ├── test_art_basic.sh           # HelloWorld.dex ejecuta
│   └── test_framework_boot.sh     # system_server arranca
├── linux/               ← Tests específicos de Linux
│   ├── test_epoll_native.c         # epoll funciona nativamente
│   └── test_binder_kernel.c        # Binder kernel module (si disponible)
└── macos/               ← Tests específicos de macOS
    ├── test_epoll_kqueue.c          # epoll sobre kqueue
    ├── test_proc_mach.c             # /proc sobre mach_vm_region
    └── test_binder_mach.c          # Binder sobre Mach IPC
```

En CI:
- Linux tests corren en cada PR (runner gratuito)
- macOS tests corren en cada merge a `develop` (runner de pago, usar con moderación)
