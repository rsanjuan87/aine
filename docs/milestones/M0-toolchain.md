# M0 — Toolchain y entorno funcional

**Target:** Semanas 1–3
**Criterio de éxito:** ATL compila en macOS ARM64 (aunque crashee en runtime)

---

## Contexto

Este milestone no produce nada que ejecute Android. Su único objetivo es que el proyecto AINE compile en macOS sin errores de build. Los errores de runtime vienen en M1.

Es el trabajo más importante y menos glamuroso del proyecto: construir los cimientos del toolchain sobre los que todo lo demás crece.

## Tareas

### T0.1 — Estructura del repositorio

```bash
# Crear el repo
git init aine
cd aine
# Copiar estructura de este proyecto
# Primer commit
git add .
git commit -m "chore: initial AINE project structure"
```

**Criterio:** `git log` muestra el primer commit.

---

### T0.2 — Clonar ATL como submódulo

```bash
git submodule add \
  https://gitlab.com/android_translation_layer/android_translation_layer.git \
  vendor/atl
git submodule update --init
git commit -m "chore: add ATL as submodule"
```

**Importante:** No modificar nada en `vendor/atl/` directamente. Es código upstream, solo lectura.

**Criterio:** `ls vendor/atl/` muestra el código fuente de ATL.

---

### T0.3 — Primer intento de build y documentar errores

```bash
# Intentar compilar ATL en macOS
cd vendor/atl
# Seguir instrucciones de ATL para build con CMake
cmake -B build-macos -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-macos 2>&1 | tee /tmp/atl-macos-errors.txt
```

Documentar cada error en `docs/build-errors-m0.md`. Los errores esperados son:

1. `fatal error: 'sys/epoll.h' file not found`
2. `fatal error: 'linux/memfd.h' file not found`
3. `fatal error: 'sys/inotify.h' file not found`
4. Errores de `pthread_setname_np` (signatura diferente)
5. Referencias a `libGL` / Mesa
6. Referencias a `getauxval()`

**Criterio:** Documento `docs/build-errors-m0.md` con todos los errores clasificados.

---

### T0.4 — Stubs de headers Linux faltantes

Por cada header Linux que falta, crear un stub mínimo en `src/aine-shim/include/`:

```
src/aine-shim/include/
├── linux/
│   ├── memfd.h       ← constantes MFD_*
│   └── ashmem.h      ← constantes ASHMEM_*
└── sys/
    ├── epoll.h       ← tipos y constantes EPOLL*
    ├── inotify.h     ← tipos y constantes IN_*
    ├── eventfd.h     ← constantes EFD_*
    └── timerfd.h     ← constantes TFD_*
```

Ejemplo de stub:
```c
// src/aine-shim/include/sys/epoll.h
#pragma once
// AINE stub: epoll types and constants for macOS
// Implementation in src/aine-shim/epoll.c
#include <stdint.h>

typedef union epoll_data {
    void    *ptr;
    int      fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

struct epoll_event {
    uint32_t     events;
    epoll_data_t data;
} __attribute__((packed));

#define EPOLLIN      0x001
#define EPOLLPRI     0x002
#define EPOLLOUT     0x004
#define EPOLLERR     0x008
#define EPOLLHUP     0x010
#define EPOLLET      (1u << 31)
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

// Declarations — implemented in aine-shim
int epoll_create(int size);
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
```

**Criterio:** ATL compila sin errores de "file not found" para estos headers.

---

### T0.5 — Resolver conflictos de signatura

Algunos problemas no son headers faltantes sino signaturas diferentes:

```c
// src/aine-shim/include/aine-compat.h
#pragma once

// pthread_setname_np: Linux tiene (thread, name), macOS solo (name)
#ifdef __APPLE__
  // No redefinir — implementado en aine-bionic-translation
  #define AINE_PTHREAD_SETNAME_COMPAT 1
#endif
```

**Criterio:** Sin errores de "conflicting types" en la compilación.

---

### T0.6 — CMake para src/aine-shim

```cmake
# src/aine-shim/CMakeLists.txt
cmake_minimum_required(VERSION 3.22)

add_library(aine-shim SHARED
  epoll.c        # epoll → kqueue (stubs en M0, implementación en M1)
  proc.c         # /proc → Mach (stubs en M0)
  futex.c        # futex → pthread (stubs en M0)
  ashmem.c       # ashmem → shm (stubs en M0)
  eventfd.c      # eventfd → pipe (stubs en M0)
)

target_include_directories(aine-shim PUBLIC include)
target_link_libraries(aine-shim PRIVATE "-framework Foundation")
```

En M0, todas las implementaciones son stubs que devuelven `ENOSYS` o valores vacíos. La implementación real viene en M1.

**Criterio:** `libaine-shim.dylib` se genera sin errores.

---

### T0.7 — CI con GitHub Actions

```yaml
# .github/workflows/build.yml
name: AINE Build

on: [push, pull_request]

jobs:
  build-macos:
    runs-on: macos-14  # Apple Silicon
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - name: Setup deps
        run: ./scripts/setup-deps.sh
      - name: Build aine-shim
        run: ./scripts/build.sh aine-shim
      - name: Build aine-binder
        run: ./scripts/build.sh aine-binder
```

**Criterio:** CI verde en GitHub Actions con runner `macos-14`.

---

## Definition of Done — M0

- [ ] `./scripts/build.sh aine-shim` sale con código 0
- [ ] `./scripts/build.sh aine-binder` sale con código 0 (aunque sea con stubs)
- [ ] CI verde en GitHub Actions (macos-14 / Apple Silicon)
- [ ] `docs/build-errors-m0.md` documenta todos los errores encontrados y cómo se resolvieron
- [ ] No hay código modificado en `vendor/atl/` (es upstream, solo lectura)

## Siguiente: M1

Con el toolchain funcionando, M1 implementa el syscall shim real y consigue que ART arranque.
Ver: `docs/milestones/M1-art-boots.md`
