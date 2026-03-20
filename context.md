# Contexto: proyecto AINE

Estoy trabajando en AINE (Aine Is No Emulator), una capa de compatibilidad 
para ejecutar apps Android AOSP en macOS nativamente, sin emulación de CPU.
Es conceptualmente igual que Wine pero para Android en Apple Silicon.

## Por qué es posible
Apple Silicon es ARM64 — la misma arquitectura que Android. El código nativo
de las apps ya puede correr directamente. El problema son las APIs:
Android habla Linux, macOS habla Darwin/XNU. AINE traduce unas en otras.

## Base de código
AINE es un fork de ATL (Android Translation Layer), que hace lo mismo pero
en Linux. ATL ya resolvió ART port, Binder userspace y framework Android.
AINE añade la capa macOS: syscall shim Linux→XNU, ANGLE para OpenGL ES→Metal,
Binder sobre Mach IPC, HAL bridges para CoreAudio/AVFoundation/NSEvent.

## Stack
- C11 para src/aine-shim/ (syscall translation, dylib inyectada via DYLD_INSERT_LIBRARIES)
- C++17 para src/aine-binder/ (Binder IPC sobre Mach messages)
- Objective-C++ para src/aine-hals/ (bridges a CoreAudio, AVFoundation, NSEvent)
- SwiftUI para src/aine-launcher/ (app macOS para gestionar APKs)
- CMake + Ninja como build system
- Target: macOS 13+ ARM64 (Apple Silicon)

## Estructura de src/
src/
├── aine-shim/
│   ├── common/      ← código portable (sin #ifdef de plataforma)
│   ├── macos/       ← implementaciones XNU (epoll→kqueue, /proc→Mach, etc.)
│   ├── linux/       ← pass-throughs (Linux ya tiene estas syscalls)
│   └── include/     ← stubs de headers Linux (epoll.h, eventfd.h, etc.)
├── aine-binder/
│   ├── common/      ← protocolo Binder BC_*/BR_* (portable)
│   ├── macos/       ← transporte Mach IPC
│   └── linux/       ← transporte Linux (eventfd nativo + ashmem)
├── aine-hals/
│   ├── macos/       ← Metal, CoreAudio, AVFoundation, NSEvent
│   └── linux/       ← Mesa, PipeWire, evdev
└── aine-launcher/   ← SwiftUI (macOS) / GTK4 (Linux, futuro)

## Reglas críticas
1. NUNCA modificar vendor/atl/ — es upstream de solo lectura
2. NUNCA poner #ifdef __APPLE__ ni #ifdef __linux__ en common/
3. Todo el código platform-específico va en macos/ o linux/
4. C11 para el shim, C++17 para binder y hals
5. Prefijo aine_ en funciones del shim (ej: aine_epoll_create)
6. Comentar con // AINE: cuando se adapta código de ATL o Darling
7. Cada syscall implementada necesita un test en tests/

## Milestone actual: M0 — Toolchain funcional
Objetivo: que cmake -B build compile sin errores (aunque crashee en runtime).
Criterio de éxito: ./scripts/build.sh termina con código 0.

## Bloqueante inmediato: B5
ATL referencia headers Linux que no existen en macOS:
- sys/epoll.h → necesita stub en src/aine-shim/include/sys/epoll.h
- linux/memfd.h → stub en src/aine-shim/include/linux/memfd.h
- sys/inotify.h → stub en src/aine-shim/include/sys/inotify.h
- sys/eventfd.h → stub en src/aine-shim/include/sys/eventfd.h

Los stubs solo necesitan los tipos y constantes — las implementaciones
reales vienen en M1 en src/aine-shim/macos/

## Bloqueante B4 (también en M0)
pthread_setname_np tiene firma diferente:
- Linux:  pthread_setname_np(pthread_t thread, const char *name)
- macOS:  pthread_setname_np(const char *name)  ← solo hilo actual

## Primer paso concreto
Ejecuta en la terminal integrada:
  ./scripts/build.sh 2>&1 | head -50

Pega los primeros errores aquí para que los analice y proponga los stubs
necesarios en src/aine-shim/include/

## Referencias externas clave
- ATL upstream: https://gitlab.com/android_translation_layer/android_translation_layer
- Darling (referencia para epoll→kqueue): https://github.com/darlinghq/darling
- XNU source: https://github.com/apple-oss-distributions/xnu
- Docs del proyecto: docs/blockers.md, docs/milestones/M0-toolchain.md