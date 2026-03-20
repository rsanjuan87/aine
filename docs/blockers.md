# Bloqueantes conocidos y estrategias de resolución

Cada bloqueante está clasificado por severidad y tiene una estrategia de resolución concreta.

---

## Estado actual (actualizado — M3 nativo completado)

| Bloqueante | Estado | Milestone |
|------------|--------|-----------|
| B1 — Page size 16KB | ⚠️ Workaround documentado | M1 |
| B2 — Syscalls Linux | ✅ Implementado (aine-shim M1) | M1 |
| B3 — /proc filesystem | ✅ Implementado (proc.c M1) | M1 |
| B4 — pthread_setname_np | ✅ Implementado (prctl.c M1) | M1 |
| B5 — Linux headers en macOS | ✅ Stubs creados | M0 |
| B6 — ART standalone macOS | ✅ **RESUELTO — aine-dalvik nativo** | M1, M3 |
| B7 — Binder IPC | ✅ Unix socket transport (M2) | M2 |
| B8 — Activity lifecycle | ✅ **aine-dalvik nativo, sin adb** | M3 |

---

## B6 — ART standalone para macOS ✅ RESUELTO (aine-dalvik)

**Severidad:** RESUELTO — `aine-dalvik` interpreta DEX nativo en macOS ARM64
**Origen:** ART de AOSP no tiene soporte macOS oficial

### Estado: NATIVO FUNCIONAL ✅

`aine-dalvik` (`src/aine-dalvik/`) es un intérprete Dalvik nativo escrito en C11,
compilado como binario Mach-O ARM64. Ejecuta DEX directamente en macOS sin adb,
sin emulador, sin ELF, sin ART/bionic.

**M1 verificado:**
```
$ ./build/dalvikvm -cp test-apps/HelloWorld/HelloWorld.dex HelloWorld
AINE: ART Runtime funcional
java.version: 0
os.arch: aarch64
```

**M3 verificado (ciclo de vida completo, 6/6 eventos en orden):**
```
$ ./build/dalvikvm -cp test-apps/M3Lifecycle/classes.dex M3LifecycleTest
AINE M3: iniciando ciclo de vida (sin emulador)
Runtime: The Android Project
Arch: aarch64
---
AINE-M3: onCreate
AINE-M3: onStart
AINE-M3: onResume
AINE-M3: [app running...]
AINE-M3: onPause
AINE-M3: onStop
AINE-M3: onDestroy
---
AINE M3: ciclo de vida completado OK
```

### Componentes implementados
- `src/aine-dalvik/dex.c` — parser del formato DEX (header, strings, types, methods, class_data)
- `src/aine-dalvik/heap.c` — heap mínimo sin GC (strings, StringBuilder, user-defined classes)
- `src/aine-dalvik/jni.c` — bridges JNI para java.lang.* y java.io.PrintStream
- `src/aine-dalvik/interp.c` — intérprete de registros Dalvik (opcodes 0x00-0x74+)
- `src/aine-dalvik/main.c` — entry point `dalvikvm -cp <dex> <Class>`

### Notas para el futuro (ART completo, M4+)
Cuando se requiera ART completo (JIT, GC, frameworks Android completos):
- **Opción A:** Compilar ART standalone de AOSP para host macOS (`lunch aosp_arm64-eng`)
- **Opción B:** Port incremental de ART a CMake con aine-shim como capa de syscalls

### Test
```bash
./build/dalvikvm -cp test-apps/HelloWorld/HelloWorld.dex HelloWorld
./build/dalvikvm -cp test-apps/M3Lifecycle/classes.dex M3LifecycleTest
```

---

## B1 — Page size 16KB (CRÍTICO)

**Severidad:** Bloqueante total para AOT compilation
**Origen:** Específico de Apple Silicon + heredado de ATL

### Problema
Apple Silicon usa páginas de memoria de 16KB. Linux y ATL asumen 4KB. ART compila código DEX a archivos `.oat` alineados a 4KB que XNU rechaza mapear en memoria.

### Workaround temporal
```bash
# En el comando de arranque de ART:
dalvikvm -Xnoimage-dex2oat -Xusejit:false -cp app.dex Main
```
Esto fuerza el modo intérprete JIT. Funciona pero es 30-50% más lento.

### Solución permanente
Compilar ART con flag de 16KB:
```cmake
# En CMakeLists.txt de art_standalone:
add_compile_definitions(PRODUCT_MAX_PAGE_SIZE_SUPPORTED=16384)
```
Requiere ART de Android 15+ que ya tiene los parches upstream.

**Esfuerzo estimado:** 1–2 semanas (build system + verificación)

---

## B2 — Syscalls Linux-only (CRÍTICO)

**Severidad:** Bloqueante total — ART no inicializa
**Origen:** Exclusivo de macOS (ATL corre sobre kernel Linux)

### Problema
Android y ATL usan syscalls que no existen en XNU:
- `epoll_create/ctl/wait` — event loop de I/O
- `eventfd` — notificaciones entre threads
- `timerfd_create/settime/read` — timers de precisión
- `inotify_*` — file system watching
- `signalfd` — signals como file descriptors
- `clone()` con flags Linux-específicos
- `prctl()` con opciones Linux-específicas
- `/proc/self/maps` — necesario para GC de ART
- `/proc/self/status` — estadísticas de memoria
- `ashmem` via `/dev/ashmem` — memoria compartida anónima

### Solución: `aine-shim`
Dylib inyectada via `DYLD_INSERT_LIBRARIES` que intercepta estas llamadas:

```c
// src/aine-shim/epoll.c
// Implementación epoll sobre kqueue

static int aine_epoll_create1(int flags) {
    int kq = kqueue();
    if (kq < 0) return -1;
    // registrar en tabla global de epoll_fds
    epoll_table_insert(kq);
    return kq;
}

// Interpose: sustituye epoll_create1 en runtime
DYLD_INTERPOSE(aine_epoll_create1, epoll_create1)
```

**Referencia:** Darling (darlinghq/darling) tiene implementaciones de referencia para epoll→kqueue y varias syscalls Linux en macOS.

**Esfuerzo estimado:** 6–10 semanas (syscall shim completo)
**Prioridad:** Primera tarea de implementación real tras el toolchain.

---

## B3 — /proc filesystem (CRÍTICO)

**Severidad:** Bloqueante total — GC de ART crashea
**Origen:** Exclusivo de macOS

### Problema
ART lee `/proc/self/maps` en cada ciclo de GC para conocer las regiones de memoria mapeadas. macOS no tiene `/proc`.

### Solución
Interceptar `open("/proc/self/maps")` en `aine-shim` y devolver un fd a un archivo generado en runtime usando la API Mach:

```c
// src/aine-shim/proc.c
int aine_open(const char *path, int flags, ...) {
    if (strcmp(path, "/proc/self/maps") == 0) {
        return aine_generate_proc_maps(); // genera con mach_vm_region()
    }
    if (strcmp(path, "/proc/self/status") == 0) {
        return aine_generate_proc_status(); // genera con task_info()
    }
    // ... otros paths de /proc
    return real_open(path, flags);
}
```

`aine_generate_proc_maps()` llama a `mach_vm_region()` iterativamente para enumerar todas las regiones del proceso y las formatea en el formato exacto de `/proc/self/maps`.

**Esfuerzo estimado:** 1–2 semanas

---

## B4 — bionic_translation: glibc vs libSystem (ALTO)

**Severidad:** Bloqueante parcial
**Origen:** Heredado de ATL

### Problema
ATL usa `bionic_translation` para shimear bionic → glibc. macOS usa `libSystem`, no glibc. Diferencias críticas:
- `pthread_setname_np(thread, name)` en Linux vs `pthread_setname_np(name)` en macOS (sin argumento thread)
- Constantes `errno` diferentes entre Linux y macOS
- `getauxval()` no existe en macOS
- `mallinfo/mallopt` con structs diferentes
- `dlopen` con semántica de namespace diferente

### Solución
Fork de `bionic_translation` → `aine-bionic-translation` con adaptaciones para libSystem:

```c
// src/aine-bionic-translation/pthread.c

// En Linux: pthread_setname_np(pthread_t thread, const char* name)
// En macOS: pthread_setname_np(const char* name) — solo hilo actual
int aine_pthread_setname_np(pthread_t thread, const char* name) {
    if (thread == pthread_self()) {
        return pthread_setname_np(name); // macOS
    }
    // Para otros threads: usar port Mach para enviar el nombre
    // (workaround necesario)
    return 0;
}
```

**Esfuerzo estimado:** 3–4 semanas (auditoría + adaptación)

---

## B5 — ATL no compila en macOS (ALTO)

**Severidad:** Bloqueante parcial — primer problema a resolver
**Origen:** Heredado de ATL

### Problema
ATL tiene hardcodeados:
- Headers Linux: `<linux/memfd.h>`, `<sys/epoll.h>`, `<sys/inotify.h>`
- Pragmas y atributos GCC que clang de Apple rechaza
- Referencias a `libGL` (Mesa) que no existe en macOS
- Paths de sistema asumiendo FHS Linux

### Solución: stubs de headers

```c
// src/aine-shim/include/linux/memfd.h  ← stub
#pragma once
#define MFD_CLOEXEC     0x0001U
#define MFD_ALLOW_SEALING 0x0002U
// memfd_create no existe en macOS — implementado en aine-shim
```

```c
// src/aine-shim/include/sys/epoll.h  ← stub
#pragma once
#include <stdint.h>
// tipos y constantes — implementación real en aine-shim/epoll.c
#define EPOLLIN  0x001
#define EPOLLOUT 0x004
// ...
```

**Proceso recomendado:**
1. `cmake -B build` en macOS
2. Documentar cada error de compilación
3. Crear stub mínimo para cada header faltante
4. Iterar hasta que compile (aunque crashee en runtime)

**Esfuerzo estimado:** 2–4 semanas

---

## B6 — Binder primitivas Linux (ALTO)

**Severidad:** Bloqueante parcial
**Origen:** Heredado de ATL + específico macOS

### Problema
La implementación Binder de ATL usa en su capa más baja:
- `eventfd` para notificaciones (no existe en macOS)
- `MAP_SHARED` sobre `/dev/ashmem` para buffers de transacción (no existe en macOS)

### Solución
Sustitución quirúrgica en el código Binder de ATL:

```c
// ANTES (ATL):
int notify_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

// DESPUÉS (AINE):
int pipe_fds[2];
pipe2(pipe_fds, O_CLOEXEC | O_NONBLOCK); // O implementado en aine-shim
int notify_fd = pipe_fds[0]; // leer = wait, escribir = notify
```

```c
// ANTES (ATL):
int shm_fd = open("/dev/ashmem", O_RDWR);
ioctl(shm_fd, ASHMEM_SET_SIZE, buffer_size);

// DESPUÉS (AINE):
char shm_name[64];
snprintf(shm_name, sizeof(shm_name), "/aine-binder-%d-%d", getpid(), counter++);
int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
ftruncate(shm_fd, buffer_size);
shm_unlink(shm_name); // unlink inmediato, fd sigue válido
```

**Esfuerzo estimado:** 1–2 semanas

---

## B7 — Mesa/OpenGL ES → ANGLE/Metal (MEDIO)

**Severidad:** Problema significativo con solución directa
**Origen:** Heredado de ATL

### Problema
ATL usa Mesa para OpenGL ES. Mesa no existe en macOS.

### Solución
Integrar ANGLE (Google):

```bash
# Clonar y compilar ANGLE para macOS ARM64
git clone https://chromium.googlesource.com/angle/angle
cd angle
# Seguir docs/DevSetup.md con target macOS + Metal backend
gn gen out/Release --args='
  angle_enable_metal=true
  target_cpu="arm64"
  target_os="mac"
  is_component_build=false
'
ninja -C out/Release
```

ANGLE expone la misma API EGL + OpenGL ES que espera Android. Solo hay que conectar `EGLNativeWindowType` a un `CAMetalLayer`.

**Esfuerzo estimado:** 1–2 semanas

---

## B8 — Cargador .so: ld-android vs dyld (MEDIO)

**Severidad:** Problema significativo
**Origen:** Heredado de ATL

### Problema
Android usa su propio linker (`linker64`) con paths `/data/app/[pkg]/lib/arm64/`. macOS usa `dyld` con semántica diferente.

### Solución
Adaptar el cargador de ATL para macOS:

```c
// Mapear path Android → path AINE local
// /data/app/com.example.app/lib/arm64/libfoo.so
//   → ~/Library/AINE/packages/com.example.app/lib/libfoo.so

void* aine_dlopen_android(const char* android_path, int flags) {
    char macos_path[PATH_MAX];
    aine_map_android_path(android_path, macos_path, sizeof(macos_path));
    return dlopen(macos_path, RTLD_LOCAL | RTLD_NOW);
}
```

**Esfuerzo estimado:** 1 semana

---

## Orden de ataque recomendado

```
Semanas 1-3:   B5 (compilación) → Build system funcional
Semanas 4-10:  B2 (syscalls) + B3 (/proc) → ART inicializa
Semana 11-12:  B1 (page size) → ART compila AOT correcto
Semanas 13-16: B4 (bionic) + B6 (Binder) → system_server arranca
Semanas 17-18: B7 (ANGLE) + B8 (dlopen) → primera app con UI
```

Ninguno de los 8 bloqueantes es un callejón sin salida. Todos tienen solución conocida con código de referencia existente.
