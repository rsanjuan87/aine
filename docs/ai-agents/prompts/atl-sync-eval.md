# Prompt: evaluar commit de ATL para AINE

Usa este prompt para que un agente AI evalúe si un commit de ATL es aplicable a AINE y cómo adaptarlo.

---

## Prompt base

```
Soy desarrollador de AINE, una capa de compatibilidad Android → macOS basada en ATL (Android Translation Layer).

ATL corre sobre Linux. AINE corre sobre macOS/XNU. Las diferencias clave:

AINE reemplaza en ATL:
- Mesa/OpenGL ES → ANGLE + Metal (CAMetalLayer)
- Syscalls Linux → aine-shim (epoll→kqueue, /proc→Mach, ashmem→shm_open, etc.)
- bionic_translation para glibc → adaptado para libSystem de macOS
- Wayland/X11 → NSWindow + CAMetalLayer
- eventfd → pipe + atomic counter
- /dev/ashmem → shm_open + mmap

El siguiente commit de ATL upstream necesita evaluación:

---
Commit: [HASH]
Mensaje: [MENSAJE DEL COMMIT]

Diff:
[PEGAR EL DIFF COMPLETO AQUÍ]
---

Por favor:

1. RELEVANCIA: ¿Este cambio es relevante para AINE?
   - HIGH: afecta ART, libcore, framework Java, Binder protocol
   - MEDIUM: afecta libs compartidas, build system, servicios genéricos
   - SKIP: es específico de Mesa, Wayland, X11, glibc, o drivers Linux

2. APLICABILIDAD: Si es relevante, ¿puede aplicarse directamente a AINE?
   - SÍ DIRECTO: el código es portátil (Java, lógica de protocolo, etc.)
   - NECESITA ADAPTACIÓN: tiene partes Linux-específicas que hay que adaptar
   - NO APLICABLE: es 100% Linux-específico

3. Si NECESITA ADAPTACIÓN, identifica:
   - Qué líneas son Linux-específicas y por qué
   - El equivalente en macOS para cada parte
   - Código sugerido para la versión macOS

4. RIESGO: ¿Hay riesgo de regresión al aplicarlo?
   - ¿Cambia interfaces públicas?
   - ¿Afecta el protocolo Binder?
   - ¿Cambia el comportamiento del GC de ART?

5. RECOMENDACIÓN final: aplicar / adaptar / ignorar
```

---

## Ejemplos de uso

### Commit de fix de ART (HIGH, aplicar directo)
```
Commit: a3f91bc
Mensaje: fix: ART GC crash with concurrent mark-sweep on large heaps

[pegar diff de art/runtime/gc/]
```
→ Esperar: HIGH relevancia, probablemente directo si no usa primitivas Linux

### Commit de Wayland (SKIP)
```
Commit: b7c2d1e
Mensaje: feat: improve Wayland surface creation performance

[pegar diff de wayland-specific code]
```
→ Esperar: SKIP — 100% específico de Linux/Wayland

### Commit de Binder fix (HIGH, adaptación)
```
Commit: c4e8f2a
Mensaje: fix: Binder thread pool deadlock with eventfd

[pegar diff que usa eventfd]
```
→ Esperar: HIGH, necesita adaptación (eventfd → pipe+atomic en AINE)
