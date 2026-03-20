# Estrategia de fork: AINE como fork de ATL

## El problema del fork

Un fork naïve (clonar ATL y modificarlo directamente) tiene un problema clásico: **divergencia**. Con el tiempo ATL sigue avanzando, AINE acumula cambios, y hacer cherry-pick se vuelve doloroso porque los diffs son enormes y llenos de conflictos.

La estrategia de AINE evita esto con una arquitectura en capas.

## Arquitectura: capas de abstracción

```
┌─────────────────────────────────────────────────────┐
│                   AINE (tu repo)                    │
│                                                     │
│  src/aine-shim/     ← 100% nuevo, no está en ATL   │
│  src/aine-binder/   ← Adaptación de ATL             │
│  src/aine-hals/     ← 100% nuevo (HAL bridges)      │
│  src/aine-launcher/ ← 100% nuevo (SwiftUI/GTK)      │
│                                                     │
│  ┌─────────────────────────────────────────────┐   │
│  │          vendor/atl/ (submódulo)            │   │
│  │                                             │   │
│  │  art/        ← Usamos directamente          │   │
│  │  libcore/    ← Usamos directamente          │   │
│  │  frameworks/ ← Usamos directamente          │   │
│  │  binder/     ← Base para aine-binder        │   │
│  │  bionic_tr/  ← Fork interno en AINE         │   │
│  └─────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

**La regla de oro:** Nunca modificar `vendor/atl/`. Todo el código específico de AINE vive en `src/`. Los componentes de ATL se usan como biblioteca.

## Estructura del repositorio AINE

```
aine/                           ← Tu repositorio en GitHub
│
├── vendor/
│   └── atl/                   ← git submodule (ATL upstream, solo lectura)
│       └── [código ATL sin modificar]
│
├── src/
│   ├── aine-shim/             ← Solo existe en AINE (no en ATL)
│   │   ├── macos/             ← Implementaciones XNU
│   │   ├── linux/             ← Pass-throughs para Linux
│   │   └── common/            ← Código compartido
│   │
│   ├── aine-binder/           ← Fork del binder de ATL con adaptaciones
│   │   ├── macos/             ← Primitivas Mach
│   │   ├── linux/             ← Primitivas Linux (compatible ATL)
│   │   └── common/            ← Protocolo Binder (idéntico a ATL)
│   │
│   ├── aine-hals/             ← Solo existe en AINE
│   │   ├── macos/             ← Metal, CoreAudio, AVFoundation
│   │   └── linux/             ← Mesa, PipeWire, evdev
│   │
│   └── aine-launcher/         ← Solo existe en AINE
│       ├── macos/             ← SwiftUI
│       └── linux/             ← GTK4 (planificado M6)
│
├── cmake/
│   ├── platform.cmake         ← Detección de plataforma
│   ├── atl-integration.cmake  ← Cómo linkear con vendor/atl/
│   └── toolchain-macos.cmake  ← Toolchain para macOS ARM64
│
└── scripts/
    ├── fork-setup.sh          ← Setup del fork en GitHub
    └── sync-atl.sh            ← Sincronización con ATL upstream
```

## Setup inicial del fork en GitHub

```bash
# 1. Crear el fork en GitHub
#    (desde la UI de GitLab: fork de ATL → tu cuenta de GitHub)
#    O crear repo nuevo y añadir ATL como remote:

git init aine
cd aine
git remote add origin https://github.com/tu-usuario/aine.git

# 2. Añadir ATL como upstream reference (NO como base del repo)
git submodule add \
  https://gitlab.com/android_translation_layer/android_translation_layer.git \
  vendor/atl

# 3. Crear branch de tracking para ATL
git checkout -b atl-upstream
git checkout main

# 4. Primer commit con la estructura AINE
git add .
git commit -m "chore: initialize AINE fork structure over ATL"
git push -u origin main
```

## Estrategia de branches

```
main                     ← Estable. Solo merges desde develop.
  │
  └── develop            ← Integración. CI completo (Linux + macOS).
        │
        ├── feature/m0-toolchain      ← Trabajo activo del milestone
        ├── feature/m1-syscall-shim
        ├── feature/m1-art-port
        │
        └── atl-upstream              ← Sigue ATL sin modificaciones
              │                          Se actualiza con sync-atl.sh --fetch
              └── [commits de ATL tal cual]
```

### Por qué `atl-upstream` es un branch local

Mantener `atl-upstream` como branch de AINE (en lugar de solo el submódulo) permite:

```bash
# Ver exactamente qué cambió en ATL desde la última sync
git diff atl-upstream..develop -- vendor/atl/

# Ver el historial de cherry-picks aplicados
git log develop --grep="sync(atl):" --oneline

# Revertir un cherry-pick problemático de ATL
git revert <commit-hash>
```

## Mantener compatibilidad con Linux de ATL

### El principio de compatibilidad inversa

Todo lo que funciona en **ATL + Linux** debe seguir funcionando en **AINE + Linux**. Si alguien tiene un script que usa ATL en Linux, debería poder cambiar `atl` por `aine` y que funcione igual o mejor.

Para garantizar esto:

```cmake
# cmake/atl-compatibility.cmake
# En Linux, AINE puede opcionalmente usar los mismos paths de instalación que ATL

option(AINE_ATL_COMPAT "Install to ATL-compatible paths on Linux" OFF)

if(AINE_PLATFORM_LINUX AND AINE_ATL_COMPAT)
    # Mismo directorio de instalación que ATL
    set(CMAKE_INSTALL_PREFIX "/usr/lib/android-translation-layer")
    # Mismos nombres de binarios que ATL
    set_target_properties(dalvikvm PROPERTIES OUTPUT_NAME "dalvikvm")
    # Mismo formato de packages que ATL
    set(AINE_PACKAGES_DIR "$ENV{HOME}/.local/share/android-translation-layer")
endif()
```

### Tests de compatibilidad ATL

```bash
# tests/linux/test_atl_compat.sh
# Verifica que AINE Linux se comporta igual que ATL para los casos básicos

#!/bin/bash
# Ejecutar un APK de prueba con ATL
ATL_OUTPUT=$(atl-run test.apk 2>&1)
# Ejecutar el mismo APK con AINE Linux
AINE_OUTPUT=$(aine-run test.apk 2>&1)
# Los outputs deben ser idénticos (excepto el prefijo de log)
diff <(echo "$ATL_OUTPUT" | grep -v "\[ATL\]") \
     <(echo "$AINE_OUTPUT" | grep -v "\[AINE\]")
```

## Proceso de contribución cross-platform

Para cualquier cambio en AINE, el PR debe:

1. Compilar en Linux (`cmake -DAINE_PLATFORM=linux`)
2. Compilar en macOS ARM64 (`cmake -DAINE_PLATFORM=macos-arm64`)
3. Tests `tests/shared/` pasando en ambas plataformas
4. Si el cambio es en `src/aine-binder/common/` o `src/aine-shim/common/`, tests específicos en ambas plataformas

```yaml
# .github/workflows/ci.yml — matriz completa
strategy:
  matrix:
    include:
      - os: ubuntu-24.04        # Linux (gratis)
        platform: linux
      - os: macos-14            # macOS ARM64 ($$$)
        platform: macos-arm64
```

## FAQ sobre el fork

**¿Por qué no hacer un fork directo en GitLab y modificar ATL?**
Porque los cambios de macOS contaminarían el código Linux y haría muy difícil el cherry-pick desde ATL. La arquitectura en capas (`vendor/atl/` + `src/aine-*`) mantiene la separación limpia.

**¿Qué pasa si ATL cambia su build system (de CMake a otra cosa)?**
Solo afecta a `cmake/atl-integration.cmake`. El código de AINE no depende del build system de ATL, solo de sus headers y binarios compilados.

**¿Podría AINE contribuir de vuelta a ATL?**
Sí, y es deseable. La implementación de Binder sin módulo kernel, el APK loader, y algunas utilidades del framework son mejoras que ATL podría adoptar. Se puede abrir MRs en GitLab de ATL para los cambios que sean portátiles.

**¿AINE reemplaza a ATL en Linux?**
No. ATL es más maduro en Linux. AINE en Linux es útil para desarrollo, CI y para usuarios que quieran la experiencia unificada macOS+Linux. No hay razón para recomendar AINE sobre ATL en producción Linux hasta que AINE tenga más superficie testeada.
