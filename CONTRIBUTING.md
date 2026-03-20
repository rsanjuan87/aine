# Contribuir a AINE

## Antes de empezar

AINE es un proyecto en fase muy temprana (pre-alpha). Las contribuciones más valiosas ahora mismo son:

1. Fixes del build system para que ATL compile en macOS
2. Implementaciones del syscall shim (aine-shim)
3. Documentación de errores y soluciones
4. Tests

## Requisitos

- Mac con Apple Silicon (M1/M2/M3/M4) — imprescindible
- macOS 13+
- Conocimiento de C/C++ y macOS a nivel de sistema
- Familiaridad con AOSP es útil pero no imprescindible

## Setup

```bash
git clone https://github.com/tu-usuario/aine.git
cd aine
./scripts/init.sh
./scripts/build.sh
```

## Flujo de trabajo

1. Crea un branch desde `develop`: `git checkout -b feature/descripcion`
2. Implementa, compila, testea
3. Abre un PR contra `develop` con:
   - Descripción de qué resuelve (referencia al bloqueante en `docs/blockers.md` si aplica)
   - Cómo testearlo
   - Si es una adaptación de ATL o Darling, indicarlo en el código con `// AINE:`

## Convenciones de código

- C11 para el shim, C++17 para binder y HALs
- Nombres de funciones: `aine_nombre_funcion` para las implementaciones del shim
- Añadir `// AINE: adaptado de [origen]` si el código viene de ATL o Darling
- Un test por cada syscall implementada en `tests/shim/`

## Licencia

Todo el código contribuido a AINE es GPL v3. Al hacer un PR aceptas esta licencia.

## Comunicación

Abre issues en GitHub para:
- Bugs de compilación con el mensaje de error completo
- Propuestas de nuevas implementaciones del shim
- Preguntas sobre la arquitectura

Para discusiones más largas sobre diseño, usa las GitHub Discussions.
