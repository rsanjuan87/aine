# ROADMAP — AINE

## Visión

AINE v1.0: cualquier desarrollador puede arrastrar un APK de una app AOSP/FOSS sobre AINE.app y verla ejecutarse en una ventana nativa de macOS, como si fuera una app Mac más.

---

## Milestones

### M0 — Toolchain y entorno funcional
**Target:** Semanas 1–3
**Criterio de éxito:** ATL compila en macOS ARM64, aunque crashee en runtime.

- [ ] Repo creado con estructura de submódulos
- [ ] ATL clonado como submódulo en `vendor/atl`
- [ ] CMake configurado para arm64-apple-macos13
- [ ] Primera compilación de ATL en macOS (documentar todos los errores)
- [ ] CI con GitHub Actions en runner macOS ARM64
- [ ] Primer PR: stubs de headers Linux-only (epoll.h, memfd.h, etc.)

**Bloqueante principal:** ATL usa headers Linux que no existen en macOS.
**Estimación de trabajo:** 2–3 semanas para 1 desarrollador.

---

### M1 — ART arranca en macOS
**Target:** Meses 1–3
**Criterio de éxito:** `dalvikvm` ejecuta un `.dex` de "hola mundo" y devuelve la cadena en stdout.

- [ ] `aine-shim` v0.1: stubs de las 20 syscalls más críticas
  - [ ] `epoll_*` → `kqueue`
  - [ ] `eventfd` → `pipe` + contador atómico
  - [ ] `timerfd_*` → `dispatch_source_t`
  - [ ] `/proc/self/maps` → `mach_vm_region()` iterativo
  - [ ] `ashmem` → `shm_open` + `mmap`
  - [ ] `futex` → `pthread_cond` + hashmap por dirección
- [ ] ART compilado con soporte de páginas 16KB (`PRODUCT_MAX_PAGE_SIZE_SUPPORTED=16384`)
- [ ] Workaround activo: `-Xnoimage-dex2oat -Xusejit:false` mientras se resuelve AOT
- [ ] `bionic_translation` adaptado para libSystem (pthread_*, errno, dlopen)
- [ ] Test: `HelloWorld.dex` ejecuta y devuelve output correcto

**Bloqueante principal:** page size 16KB + syscalls Linux-only.
**Estimación de trabajo:** 6–8 semanas adicionales.

---

### M2 — Binder funcional sobre Mach IPC
**Target:** Meses 3–5
**Criterio de éxito:** `service list` via `aine-binder` devuelve los servicios del system_server.

- [ ] `aine-binder-daemon` v0.1: router central vía Mach messages
- [ ] `/dev/binder` interceptado en `aine-shim`
- [ ] `eventfd` y `ashmem` en el Binder de ATL sustituidos por equivalentes macOS
- [ ] `servicemanager` compilado y arrancando contra `aine-binder`
- [ ] `system_server` arrancando con servicios básicos stubados
- [ ] Test: `getService("package")` resuelve correctamente

**Bloqueante principal:** eventfd + ashmem en implementación Binder de ATL.
**Estimación de trabajo:** 4–6 semanas.

---

### M3 — Primera app sin gráficos ejecuta
**Target:** Meses 5–7
**Criterio de éxito:** Una app Android de consola/utilidad (sin UI) completa su ciclo de vida sin crash.

- [ ] `PackageManager` mínimo: instala APK desde path local
- [ ] `ActivityManagerService` mínimo: lanza Activity, gestiona ciclo de vida
- [ ] Cargador de `.so` nativas: dlopen macOS con path mapping de Android
- [ ] `aine-shim` ampliado con syscalls que falten en el path de arranque real
- [ ] Test: app de terminal/utilidad AOSP completa onCreate → onResume → onPause → onDestroy

**Estimación de trabajo:** 4–6 semanas.

---

### M4 — Gráficos: primera app con UI renderiza
**Target:** Meses 7–10
**Criterio de éxito:** Una app simple (calculadora, reloj) renderiza su UI en una NSWindow.

- [ ] ANGLE integrado: OpenGL ES → Metal (backend CAMetalLayer)
- [ ] `EGLNativeWindowType` mapeado a `CAMetalLayer`
- [ ] `SurfaceFlinger` mínimo: presenta el buffer de la app en `NSWindow`
- [ ] `Choreographer` + VSYNC sincronizado con `CADisplayLink` de macOS
- [ ] `InputFlinger`: `NSEvent` (teclado, ratón) → Android `input_event`
- [ ] Test: app calculadora AOSP renderiza UI y responde a clicks

**Estimación de trabajo:** 6–8 semanas.

---

### M5 — Audio y primera app real funcional
**Target:** Meses 10–13
**Criterio de éxito:** Una app de mensajería o media AOSP (FOSS) lanza y es usable.

- [ ] Audio HAL → CoreAudio: PCM buffers de `AudioFlinger` → `AudioUnit`
- [ ] Notificaciones → `UNUserNotificationCenter`
- [ ] Portapapeles: `ClipboardManager` → `NSPasteboard`
- [ ] `aine-launcher` v0.1 (SwiftUI): instala y lanza APKs desde la GUI
- [ ] Ventanas nativas: chrome macOS (minimizar, maximizar, cerrar) mapeado al ciclo de vida Android
- [ ] Test: Session (messenger FOSS) o AntennaPod arrancan y son usables

**Estimación de trabajo:** 8–10 semanas.

---

### M6 — Beta pública
**Target:** Meses 14–18
**Criterio de éxito:** Release público en GitHub con >10 apps FOSS funcionales documentadas.

- [ ] Rendimiento AOT resuelto (páginas 16KB nativas, sin workaround JIT)
- [ ] Gestión de memoria: LMK (Low Memory Killer) adaptado a presión de memoria macOS
- [ ] Vulkan vía MoltenVK como backend alternativo
- [ ] Cámara HAL: `AVCaptureSession` → `camera3_device_t`
- [ ] Website del proyecto + documentación de usuario
- [ ] Lista de compatibilidad de apps verificada

---

## Estimación global

| Escenario | Tiempo | Equipo |
|---|---|---|
| 1 desarrollador dedicado | 18–24 meses | Solo |
| 2 desarrolladores | 12–16 meses | Tú + 1 colaborador C/C++ |
| Equipo pequeño (3–4) | 8–12 meses | Con financiación |

## Financiación

- **NLnet / NGI Zero** — financiaron ATL, AINE es candidato natural
- **GitHub Sponsors** — desde M1 con demo funcionando
- **Open Collective** — para transparencia de gastos
- Dual-licensing comercial a partir de M6

---

## Métricas de progreso

Cada milestone se considera completado cuando:
1. El criterio de éxito definido pasa en CI
2. Existe al menos un test automatizado que lo verifica
3. Está documentado en `docs/milestones/MX-nombre.md`
