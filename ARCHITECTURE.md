# ARCHITECTURE — AINE

## Principio fundamental

AINE no emula CPU. Apple Silicon es ARM64 — la misma arquitectura que Android. El código nativo de una app Android corre directamente en el procesador del Mac. AINE solo traduce las **llamadas a APIs**: Android APIs → macOS APIs.

```
┌─────────────────────────────────────────┐
│           App Android (.apk)            │
│     DEX bytecode + libs ARM64           │
└──────────────────┬──────────────────────┘
                   │
┌──────────────────▼──────────────────────┐
│         ART Runtime (portado)           │
│   Interpreta DEX, JIT, GC, JNI         │
└──────────────────┬──────────────────────┘
                   │
┌──────────────────▼──────────────────────┐
│      Android Framework (Java)           │
│  Activity, View, Service, BroadcastRcv  │
└──────────────────┬──────────────────────┘
                   │
┌──────────────────▼──────────────────────┐
│         aine-shim.dylib                 │  ← inyectada via DYLD_INSERT_LIBRARIES
│  epoll→kqueue | ashmem→shm | /proc→Mach │
│  futex→pthread | eventfd→pipe           │
└──────────────────┬──────────────────────┘
                   │
         ┌─────────┴──────────┐
         │                    │
┌────────▼────────┐  ┌────────▼──────────┐
│  aine-binder    │  │    aine-hals       │
│  Binder → Mach  │  │  ANGLE, CoreAudio │
│  IPC messages   │  │  NSEvent, AVFnd.  │
└────────┬────────┘  └────────┬──────────┘
         │                    │
┌────────▼────────────────────▼──────────┐
│            macOS XNU (ARM64)           │
│      Mach kernel + libSystem           │
└────────────────────────────────────────┘
```

---

## Modelo de procesos

AINE crea un proceso macOS independiente por cada app Android:

```
macOS process tree:

aine-launcher (PID 1021)      ← daemon principal, siempre vivo
├── aine-system_server (1022) ← servicios Android (AMS, PMS, WMS...)
├── aine-binder-daemon (1023) ← router Binder sobre Mach
│
├── com.example.app1 (2041)   ← app Android A = proceso macOS
│     threads: [main, RenderThread, Binder-1, Binder-2]
│     dylibs: aine-shim.dylib, libandroid.so, libEGL.so → ANGLE
│     window: NSWindow + CAMetalLayer
│
└── com.example.app2 (2055)   ← app Android B = proceso macOS
      threads: [main, RenderThread, Binder-1]
      ...
```

Cada proceso de app:
- Arranca via `posix_spawn()` (no `fork()` — incompatible con frameworks Apple)
- Recibe `aine-shim.dylib` inyectada via `DYLD_INSERT_LIBRARIES`
- Posee su propia `NSWindow` con un `CAMetalLayer` para rendering
- Se comunica con `aine-system_server` via `aine-binder-daemon` (Mach messages)

---

## Componentes

### aine-shim (`src/aine-shim/`)

Dylib inyectada en cada proceso de app. Intercepta con `__attribute__((used)) __attribute__((section("__DATA,__interpose")))` las llamadas de bajo nivel.

**Tabla de traducciones clave:**

| Android/Linux | macOS/XNU | Notas |
|---|---|---|
| `epoll_create/ctl/wait` | `kqueue/kevent` | Ver Darling como referencia |
| `eventfd` | `pipe` + contador atómico | Semántica simplificada suficiente |
| `timerfd_create` | `dispatch_source_t` | GCD como backend |
| `inotify_*` | `FSEvents` + `kqueue` | Para file watching |
| `open("/dev/binder")` | socket Mach a `aine-binder-daemon` | fd falso devuelto |
| `open("/proc/self/maps")` | `mach_vm_region()` iterativo | Runtime generado |
| `ashmem ioctl` | `shm_open` + `mmap` | Para IPC memory |
| `futex(WAIT/WAKE)` | `pthread_cond` + hashmap | Indexado por dirección |
| `clone()` con flags | `posix_spawn` | Para procesos ligeros |
| `prctl(PR_SET_NAME)` | `pthread_setname_np` | Diferente signatura |

### aine-binder (`src/aine-binder/`)

Reimplementación de Binder en userspace sobre Mach IPC.

```
Proceso App                    aine-binder-daemon
    │                               │
    │  open("/dev/binder")          │
    │  → aine-shim devuelve fd=99   │
    │                               │
    │  ioctl(99, BC_TRANSACTION...) │
    │  → aine-shim convierte a      │
    │    Mach message               │
    │ ─────────────────────────────▶│
    │                               │  lookup destino en tabla
    │                               │  reenvía Mach message a destino
    │ ◀─────────────────────────────│
    │  ioctl devuelve BR_REPLY      │
```

Base: código Binder userspace de ATL con sustitución de:
- `eventfd` → `pipe` + `atomic_int`
- `ashmem` → `shm_open` + `mmap` + `madvise(MADV_FREE)` para UNPIN

### aine-hals (`src/aine-hals/`)

Bridges entre las HAL de Android y los frameworks de macOS.

**Gráficos (ANGLE):**
```
Android app
  │  glDrawArrays(...)
  ▼
libEGL.so (de AINE) → ANGLE backend Metal
  │
  ▼
CAMetalLayer (en NSWindow del proceso)
  │
  ▼
Metal / GPU Apple Silicon
```

**Audio:**
```
AudioFlinger (system_server)
  │  PCM buffers via Binder
  ▼
Audio HAL de AINE
  │  AudioUnit kAudioUnitSubType_DefaultOutput
  ▼
CoreAudio
```

**Input:**
```
NSApplication (main thread)
  │  NSEvent (mouse, keyboard, touch trackpad)
  ▼
aine-input-bridge
  │  convierte a struct input_event de Linux
  ▼
InputFlinger (system_server) via Binder
  │
  ▼
App Android (onTouchEvent, onKeyDown...)
```

### aine-launcher (`src/aine-launcher/`)

App macOS nativa en SwiftUI. Responsabilidades:
- GUI para instalar APKs (drag & drop)
- Lista de apps instaladas con iconos (extraídos del APK)
- Arranque y gestión del ciclo de vida de `aine-system_server` y `aine-binder-daemon`
- Comunicación con los daemons via XPC

---

## Integración con ATL

AINE parte del fork de ATL pero diverge en:

| Componente | ATL | AINE |
|---|---|---|
| Syscall shim | Asume kernel Linux | `aine-shim`: Linux → XNU |
| OpenGL ES | Mesa (libGL) | ANGLE → Metal |
| Binder primitives | `eventfd` + `/dev/ashmem` | `pipe`/atomic + `shm_open` |
| `bionic_translation` | Escrito para glibc | Adaptado para libSystem |
| Superficie gráfica | Wayland/X11 | `NSWindow` + `CAMetalLayer` |
| Proceso de app | `fork()` desde Zygote-like | `posix_spawn()` |
| Build system | soong/ninja (AOSP) | CMake + Ninja |

---

## Decisiones de diseño

**¿Por qué `posix_spawn` y no `fork`?**
`fork()` en macOS tiene problemas serios con el Objective-C runtime, Grand Central Dispatch y el sandbox de App Store. Apple lo desaconseja explícitamente. `posix_spawn` es seguro y es lo que usan todos los launchers de procesos modernos en macOS.

**¿Por qué no usar Virtualization.framework con kernel Linux?**
Sería más simple a corto plazo (el kernel Linux resuelve todas las syscalls de Android) pero introduce latencia de VM, complicaciones con acceso a hardware, y contradice el objetivo de AINE: ejecución nativa sin emulación.

**¿Por qué ANGLE y no una implementación propia de OpenGL ES?**
ANGLE es el trabajo de Google, está en producción en Chrome para macOS, es ARM64-ready, y ya tiene el backend Metal. Implementar OpenGL ES desde cero sería años de trabajo. AINE toma decisiones pragmáticas.

**¿Por qué GPL v3?**
ATL es GPL v3. AINE es un trabajo derivado. Adicionalmente, la GPL atrae colaboradores y hace posible la financiación de NLnet/NGI. Si se necesita comercialización, el dual-licensing es el camino.
