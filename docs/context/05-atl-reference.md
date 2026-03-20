# ATL: referencia y estrategia de sincronización

## ¿Qué es ATL?

Android Translation Layer es el proyecto del que parte AINE. Hace exactamente lo mismo pero para Linux en lugar de macOS:
- Corre apps AOSP como procesos Linux nativos sin VM ni contenedor
- Reimplementa Binder en userspace (sin módulo de kernel)
- Porta ART y el framework Android a un sistema no-Android
- Recibió financiación de NLnet/NGI Mobifree en 2024

**Repositorio:** https://gitlab.com/android_translation_layer/android_translation_layer
**Licencia:** GPL v3
**Lenguaje principal:** C/C++

## Relación AINE ↔ ATL

```
ATL (upstream, Linux)
    │
    │  fork inicial
    ▼
AINE (macOS)
    │
    ├── Partes de ATL que AINE usa directamente:
    │   ├── ART port (con adaptaciones de page size)
    │   ├── Binder userspace (con sustitución de primitivas)
    │   ├── Android Framework Java (sin cambios)
    │   └── APK loader / PackageManager base
    │
    └── Partes que AINE reemplaza completamente:
        ├── bionic_translation → aine-bionic-translation (libSystem vs glibc)
        ├── Mesa/OpenGL ES → ANGLE + Metal
        ├── Linux syscalls → aine-shim (XNU equivalents)
        └── Wayland/X11 surface → NSWindow + CAMetalLayer
```

## Qué vale la pena sincronizar desde ATL

### Alta prioridad (sincronizar siempre)
- Fixes de bugs en ART port
- Mejoras en el Binder userspace (protocolo, threading)
- Nuevas APIs del framework Android que ATL implemente
- Fixes de compatibilidad con versiones nuevas de apps

### Media prioridad (evaluar caso por caso)
- Cambios en bionic_translation que afecten APIs también en libSystem
- Optimizaciones de rendimiento en el runtime
- Nuevos servicios de system_server

### No sincronizar
- Cambios específicos de Mesa/Wayland/X11
- Cambios de build system que asuman Linux (soong, etc.)
- Optimizaciones específicas de glibc
- Cualquier cosa que referencie `/dev/` devices Linux-específicos

---

## Estrategia de rama

```
main (AINE estable)
    │
    ├── develop (integración)
    │       │
    │       ├── feature/m0-toolchain
    │       ├── feature/m1-syscall-shim
    │       └── feature/m1-art-port
    │
    └── atl-upstream (tracking de ATL)
            │
            └── Se actualiza con sync-atl.sh
                Commits cherry-picked manualmente a develop
```

El branch `atl-upstream` sigue ATL sin modificaciones de AINE. Esto facilita ver exactamente qué ha cambiado en ATL y cherry-pickear lo que aplica.
