# AINE — Aine Is No Emulator

> Capa de compatibilidad para ejecutar apps Android (AOSP/FOSS) en macOS de forma nativa, sin emulación de CPU.

---

## ¿Qué es AINE?

AINE es el equivalente de Wine pero para Android en macOS. En lugar de emular el hardware o correr una VM, AINE traduce las llamadas a las APIs de Android en llamadas nativas de macOS, permitiendo que el código de las apps corra directamente en el procesador Apple Silicon (ARM64).

```
App Android (.apk)
       │
       ▼
  ART Runtime  ─── DEX bytecode → código ARM64 nativo
       │
  Android Framework (Java)
       │
  aine-shim ──── intercepta syscalls Linux → XNU
       │         traduce Binder → Mach IPC
       │         OpenGL ES → Metal (via ANGLE)
       ▼
  macOS XNU (ARM64)   ←── mismo ISA que Android, sin traducción de CPU
```

## Estado actual

> **Fase:** Pre-alpha / Setup inicial
> **Basado en:** [Android Translation Layer (ATL)](https://gitlab.com/android_translation_layer/android_translation_layer) (GPL v3)

## Arquitectura en una línea

Cada app Android corre como un **proceso macOS independiente**, visible en Activity Monitor con su propio PID, consumiendo su propia memoria. Un daemon `aine-system_server` y `aine-binder-daemon` corren siempre en segundo plano como los servicios centrales.

## Requisitos

- **macOS 13+** en Apple Silicon (M1/M2/M3/M4) — obligatorio
- Xcode 15+ con Command Line Tools
- CMake 3.22+
- Ninja
- Python 3.10+
- Git con soporte de submódulos

## Setup rápido

```bash
git clone https://github.com/tu-usuario/aine.git
cd aine
./scripts/init.sh
./scripts/build.sh
```

## Estructura del proyecto

```
aine/
├── README.md                  # Este fichero
├── ROADMAP.md                 # Milestones y plan de desarrollo
├── ARCHITECTURE.md            # Diseño técnico detallado
├── CONTRIBUTING.md            # Cómo contribuir
│
├── docs/
│   ├── context/               # Contexto completo del proyecto para AI
│   ├── ai-agents/             # Guías e instrucciones para agentes AI
│   ├── milestones/            # Detalle de cada milestone
│   └── blockers.md            # Bloqueantes conocidos y soluciones
│
├── scripts/
│   ├── init.sh                # Inicializar entorno y dependencias
│   ├── build.sh               # Compilar AINE
│   ├── sync-atl.sh            # Sincronizar cambios desde ATL upstream
│   ├── run-app.sh             # Ejecutar un APK con AINE
│   └── setup-deps.sh          # Instalar dependencias del sistema
│
├── src/
│   ├── aine-shim/             # Syscall shim Linux → XNU (C)
│   ├── aine-binder/           # Binder IPC sobre Mach (C++)
│   ├── aine-hals/             # HAL bridges (Obj-C/C++)
│   │   ├── audio/             # CoreAudio
│   │   ├── graphics/          # ANGLE + CAMetalLayer
│   │   ├── input/             # NSEvent → Android input
│   │   └── camera/            # AVFoundation
│   └── aine-launcher/         # App macOS (SwiftUI)
│
└── vendor/
    └── atl/                   # Submódulo ATL (upstream)
```

## Licencia

AINE hereda GPL v3 de ATL. Ver [docs/context/04-legal-licenses.md](docs/context/04-legal-licenses.md) para las implicaciones completas.

## Links clave

- [Android Translation Layer (ATL)](https://gitlab.com/android_translation_layer/android_translation_layer)
- [AOSP Source](https://source.android.com/)
- [Darling (referencia para syscalls macOS)](https://github.com/darlinghq/darling)
- [ANGLE (OpenGL ES → Metal)](https://github.com/google/angle)
- [MoltenVK (Vulkan → Metal)](https://github.com/KhronosGroup/MoltenVK)
- [XNU Source](https://github.com/apple-oss-distributions/xnu)
