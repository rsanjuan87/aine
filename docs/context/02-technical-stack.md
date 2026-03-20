# Contexto técnico: stack y dependencias

## Stack tecnológico

| Capa | Tecnología | Rol |
|---|---|---|
| Runtime Java/DEX | ART (Android Runtime, portado de AOSP) | Ejecuta el bytecode DEX de las apps |
| Framework Android | frameworks/base/ de AOSP | APIs Java que usan las apps |
| Syscall shim | C11, DYLD_INSERT_LIBRARIES | Traduce Linux → XNU en runtime |
| Binder IPC | C++17 sobre Mach messages | IPC entre app y servicios |
| Gráficos | ANGLE (Google) → CAMetalLayer | OpenGL ES → Metal |
| Audio | Objective-C++ → CoreAudio | AudioFlinger → AudioUnit |
| Input | Objective-C → NSEvent | NSEvent → input_event Android |
| Cámara | Objective-C → AVFoundation | camera3 HAL → AVCaptureSession |
| Launcher UI | SwiftUI | App macOS para gestionar APKs |
| Build | CMake 3.22+ + Ninja | Compilación multiplataforma |
| CI | GitHub Actions (macos-14) | Runner Apple Silicon |

## Dependencias externas

### ATL (Android Translation Layer)
- **Repo:** https://gitlab.com/android_translation_layer/android_translation_layer
- **Licencia:** GPL v3
- **Rol:** Base de AINE. ART portado, Binder userspace, framework Android básico.
- **Cómo se usa:** Submódulo en `vendor/atl/`. No se modifica — se adapta en `src/`.

### AOSP (Android Open Source Project)
- **Repo:** https://source.android.com/ (repo tool)
- **Licencia:** Apache 2.0 (mayoritariamente)
- **Rol:** Fuente de ART, libcore, frameworks/base, HAL interfaces.
- **Componentes usados:** `art/`, `libcore/`, `frameworks/base/`, `hardware/interfaces/`

### ANGLE
- **Repo:** https://github.com/google/angle
- **Licencia:** BSD 3-Clause
- **Rol:** Traduce OpenGL ES → Metal. Usado en producción en Chrome para macOS.
- **Integración:** Compilado como dylib, linkado con aine-hals/graphics/

### MoltenVK
- **Repo:** https://github.com/KhronosGroup/MoltenVK
- **Licencia:** Apache 2.0
- **Rol:** Vulkan → Metal. Backend alternativo para apps que usen Vulkan.
- **Estado:** Planificado para M6 (beta).

### Darling
- **Repo:** https://github.com/darlinghq/darling
- **Licencia:** GPL v2/v3
- **Rol:** Solo referencia. Darling hace lo contrario (macOS apps en Linux) pero resolvió problemas análogos: epoll→kqueue, /proc sobre Linux, Mach IPC.
- **IMPORTANTE:** No copiar código de Darling directamente — solo usarlo como referencia de algoritmos.

## Estructura de directorios src/

```
src/
├── aine-shim/                    # Syscall translation layer
│   ├── include/
│   │   ├── linux/                # Stubs de headers Linux
│   │   │   ├── memfd.h
│   │   │   └── ashmem.h
│   │   └── sys/
│   │       ├── epoll.h           # Tipos epoll
│   │       ├── inotify.h
│   │       ├── eventfd.h
│   │       └── timerfd.h
│   ├── epoll.c                   # epoll → kqueue
│   ├── proc.c                    # /proc → mach_vm_region
│   ├── futex.c                   # futex → pthread_cond
│   ├── ashmem.c                  # ashmem → shm_open
│   ├── eventfd.c                 # eventfd → pipe+atomic
│   ├── timerfd.c                 # timerfd → dispatch_source
│   ├── inotify.c                 # inotify → FSEvents
│   ├── binder-dev.c              # /dev/binder → aine-binder socket
│   └── CMakeLists.txt
│
├── aine-binder/                  # Binder IPC sobre Mach
│   ├── binder-daemon.cpp         # Proceso router central
│   ├── binder-client.cpp         # Cliente para uso desde apps
│   ├── service-manager.cpp       # Directorio de servicios
│   ├── parcel.cpp                # Serialización de datos
│   └── CMakeLists.txt
│
├── aine-hals/                    # Hardware Abstraction Layers
│   ├── audio/
│   │   ├── AudioHAL.mm           # AudioFlinger → CoreAudio
│   │   └── CMakeLists.txt
│   ├── graphics/
│   │   ├── GraphicsHAL.mm        # ANGLE + CAMetalLayer
│   │   ├── EGLWindow.mm          # NSWindow con CAMetalLayer
│   │   └── CMakeLists.txt
│   ├── input/
│   │   ├── InputHAL.mm           # NSEvent → input_event
│   │   └── CMakeLists.txt
│   └── camera/
│       ├── CameraHAL.mm          # AVFoundation → camera3
│       └── CMakeLists.txt
│
└── aine-launcher/                # App SwiftUI
    ├── AINEApp.swift
    ├── ContentView.swift
    ├── AppManager.swift           # Gestión de apps instaladas
    ├── APKInstaller.swift         # Extracción e instalación de APKs
    └── Info.plist
```

## Variables de entorno de AINE

| Variable | Descripción | Valor por defecto |
|---|---|---|
| `DYLD_INSERT_LIBRARIES` | Path a aine-shim.dylib | Puesto por el launcher |
| `AINE_PACKAGE_DIR` | Directorio de la app instalada | `~/Library/AINE/packages/[pkg]` |
| `AINE_LOG_LEVEL` | Nivel de log del shim | `info` (debug/info/warn/error) |
| `AINE_LOG_FILE` | Fichero de log | stdout |
| `ANDROID_DATA` | Equivalente a /data de Android | `~/Library/AINE/data` |
| `ANDROID_ROOT` | Equivalente a /system de Android | build dir |

## Diferencias clave con ATL

ATL asume que corre sobre un kernel Linux y tiene acceso a:
- Todos los syscalls de Linux nativamente
- Mesa para OpenGL ES
- Wayland o X11 para gráficos
- glibc como libc del sistema

AINE debe proveer equivalentes para todo esto sobre XNU/macOS:
- `aine-shim`: syscalls Linux → XNU
- ANGLE: OpenGL ES → Metal (reemplaza Mesa)
- NSWindow + CAMetalLayer (reemplaza Wayland/X11)
- Adaptación de bionic_translation para libSystem (reemplaza glibc shim)
