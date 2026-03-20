# Contexto del proyecto: visión general

## ¿Qué es AINE?

AINE (Aine Is No Emulator) es una capa de compatibilidad para ejecutar apps Android basadas en AOSP en macOS, sin emulación de CPU ni máquina virtual. Es el equivalente conceptual de Wine (que corre apps Windows en Linux/macOS) pero para apps Android en macOS.

## Motivación técnica

Apple Silicon (M1/M2/M3/M4) usa ARM64 — exactamente la misma arquitectura de CPU que la gran mayoría de dispositivos Android modernos. Esto significa que el código nativo de una app Android puede ejecutarse directamente en un Mac sin ninguna traducción de instrucciones. El problema no es la CPU sino las APIs: Android habla Linux, macOS habla Darwin/XNU.

## Analogía con Wine

Wine funciona porque:
1. Traduce llamadas a la API Win32 → llamadas POSIX
2. El código nativo de las apps Windows corre directamente (sin emulación) porque la CPU es la misma (x86/x86_64)

AINE funciona con el mismo principio:
1. Traduce llamadas a las APIs de Android → llamadas macOS
2. El código nativo de las apps Android corre directamente en Apple Silicon (misma arquitectura ARM64)

## Diferencia con soluciones existentes

| Solución | Mecanismo | Problema |
|---|---|---|
| BlueStacks | VM + emulación | CPU emulada, lento |
| Android Studio Emulator | QEMU | CPU emulada |
| Apple M1 con apps iOS | Nativo | Solo apps iOS, no Android |
| Waydroid (Linux) | Contenedor LXC + kernel Linux real | No funciona en macOS |
| AINE | Traducción de API, sin VM ni emulación | El objetivo |

## Scope: solo AOSP y apps FOSS

AINE no intenta correr Google Play Store ni apps que dependan de Google Play Services (GMS). Eso es código propietario de Google que no puede reimplementarse legalmente. El foco es:

- Apps que usan únicamente el Android Open Source Project (AOSP)
- Apps FOSS (Free and Open Source Software) que no dependen de GMS
- Apps con dependencias opcionales de GMS (que funcionen en modo degradado)

Esto cubre una amplia gama de apps útiles: clientes de mensajería (Session, Element/Matrix), lectores RSS, apps de productividad, juegos indie, utilidades, etc.

## Relación con ATL

AINE parte del proyecto Android Translation Layer (ATL), que hace lo mismo pero para Linux. ATL ya resolvió los problemas más difíciles:
- Port de ART (Android Runtime) a sistemas no-Android
- Reimplementación de Binder en userspace (sin módulo de kernel)
- Traducción del framework Android básico

AINE añade la capa específica de macOS:
- Traducción de syscalls Linux-only → XNU equivalents
- Gráficos: OpenGL ES → Metal via ANGLE
- Audio: AudioFlinger → CoreAudio
- Input: NSEvent → Android input_event
- UI: NSWindow + CAMetalLayer como superficie nativa

## Modelo de proceso

Cada app Android en AINE es un proceso macOS independiente:
- Visible en Activity Monitor con su propio PID
- Crash-aislado (si una app muere no afecta a otras)
- Con su propia NSWindow
- Comunicándose con los servicios centrales via IPC (Binder reimplementado sobre Mach)

## Licencia

GPL v3 (heredada de ATL + AOSP Apache 2.0 compatible). Ver docs/context/04-legal-licenses.md.
