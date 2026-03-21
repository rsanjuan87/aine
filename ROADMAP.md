# ROADMAP — AINE

> **AINE = Aine Is No Emulator** — capa de compatibilidad para apps Android en macOS nativo,
> sin emulación de CPU, sin VM, sin contenedores. Igual que Wine pero para Android en Apple Silicon.

---

## Estado actual

| CTest | Nombre | Milestone | Estado |
|---|---|---|---|
| #1 | `binder-protocol` | F5/M2 | ✅ Pasa |
| #2 | `shim-epoll` | F1/M1 | ✅ Pasa |
| #3 | `shim-futex` | F1/M1 | ✅ Pasa |
| #4 | `shim-eventfd` | F1/M1 | ✅ Pasa |
| #5 | `shim-prctl` | F1/M1 | ✅ Pasa |
| #6 | `binder-roundtrip` | F5/M2 | ✅ Pasa |
| #7 | `pm-install` | F4/M3 | ✅ Pasa |
| #8 | `loader-path-map` | F5/M3 | ✅ Pasa |
| #9 | `dalvik-f6-opcodes` | F6/M3 | ✅ Pasa |
| #10 | `surface-egl-headless` | F7/M4 | ✅ Pasa |
| #11 | `input-hal` | F8/M4 | ✅ Pasa |
| #12 | `audio-hal` | F9/M5 | ✅ Pasa |
| #13 | `launcher-run` | F10/M5 | ✅ Pasa |
| #14 | `activity-lifecycle` | F11/M3 | ✅ Pasa |
| #15 | `hals-f12` | F12/M4 | ✅ Pasa |
| #16 | `handler-loop` | G1 | ✅ Pasa |
| #17 | `g2-stdlib` | G2 | ✅ Pasa |

**17/17 CTests — 0 fallos** (rama `feature/m0-toolchain`, macOS ARM64 nativo)

---

## Milestones

### M0 — Toolchain y entorno funcional ✅ COMPLETADO
**Criterio de éxito:** ATL compila en macOS ARM64, aunque crashee en runtime.

- [x] Repo creado con estructura multicomponente (`aine-shim`, `aine-binder`, `aine-hals`, `aine-dalvik`)
- [x] ATL clonado como submódulo en `vendor/atl`
- [x] CMake + Ninja configurado para `arm64-apple-macos13`
- [x] Primera compilación de `aine-shim` con stubs de syscalls Linux
- [x] `scripts/setup-deps.sh` + `scripts/build.sh` funcionales
- [x] Documentación de bloqueantes en `docs/blockers.md`

**Resultado:** Toolchain ARM64 nativo operativo. Sin emulador, sin contenedor.

---

### M1 — Shims de syscalls Linux operativos ✅ COMPLETADO
**Criterio de éxito:** Las syscalls Linux más críticas tienen equivalentes macOS/XNU funcionales.

- [x] `epoll_*` → `kqueue` (F1: `shim-epoll` ✅)
- [x] `futex` → `pthread_mutex`/`pthread_cond` por dirección (F1: `shim-futex` ✅)
- [x] `eventfd` → pipe + contador atómico (F1: `shim-eventfd` ✅)
- [x] `prctl(PR_SET_NAME)` → `pthread_setname_np` (F1: `shim-prctl` ✅)
- [x] `/proc/self/maps` → `mach_vm_region()` iterativo (F2, en `aine-shim`)
- [x] `ashmem` → `shm_open` + `mmap` (F3, en `aine-shim`)
- [x] `HelloWorld.dex` ejecuta y devuelve output correcto (commit `78789625`)

**Notas de implementación:**
- Sin emulación de CPU: ARM64 nativo en Apple Silicon
- Sin ninguna capa de virtualización del kernel de Linux
- `bionic_translation` no necesaria: se traduce sólo la syscall, no el ABI completo

---

### M2 — Binder funcional sobre Unix Socket ✅ COMPLETADO
**Criterio de éxito:** `aine-binder` permite pasar mensajes Binder entre procesos macOS.

- [x] `aine-binder-daemon` v0.1: router central vía Unix socket (F5: `binder-roundtrip` ✅)
- [x] Protocolo Binder básico: `BC_TRANSACTION` / `BR_REPLY` sobre socket local
- [x] `binder-client.cpp` + `binder-daemon.cpp` compilando en macOS sin dependencias Linux
- [x] Test: parcela ida-vuelta con payload arbitrario funciona (commit `e464d4ad`)
- [x] `binder-protocol` valida el formato de wire (CTest #1 ✅)

**Notas:** Sin `/dev/binder` real — se usa Unix socket con el mismo protocolo de wire. ATL/Binder no compilado directamente; AINE implementa un subconjunto del protocolo necesario para M3.

---

### M3 — Primera app Android completa ciclo de vida sin crash ✅ COMPLETADO
**Criterio de éxito:** `onCreate → onStart → onResume → onPause → onStop → onDestroy` ejecuta en macOS nativo.

- [x] `aine-pm` v0.1: PackageManager nativo — parsea APK (ZIP), extrae AndroidManifest + DEX (F4: `pm-install` ✅)
- [x] `aine-loader`: dlopen macOS con path mapping Android→macOS (F5: `loader-path-map` ✅)
- [x] `aine-dalvik` v1.0: intérprete Dalvik/DEX nativo ARM64 sin JIT, sin ART (F6: `dalvik-f6-opcodes` ✅)
  - Opcodes cubiertos: `0x00`–`0x22`, `0x27`–`0x9a`, `0xa0`–`0xe2`, `invoke-*`, `iget/iput`, `sget/sput`
- [x] `aine-run` v1.0: lanzador de APK — extrae DEX, lanza `dalvikvm` (F10: `launcher-run` ✅)
- [x] Ciclo de vida Activity completo: `onCreate → onResume → onPause → onDestroy` (F11: `activity-lifecycle` ✅)
- [x] `M3TestApp.apk` ejecuta en macOS ARM64 sin crash

**Commits clave:** `d04ead80`, `4d1d9d9e`, `974d41b2`, `d7d2a714`, `829a8547`, `2dc94c3a`

---

### M4 — HALs principales operativos ✅ COMPLETADO (headless)
**Criterio de éxito:** EGL headless, Input y Vulkan/Camera/Clipboard funcionan en macOS sin pantalla.

- [x] **EGL/Metal** (F7): EGL headless vía Metal + IOSurface, sin display físico (`surface-egl-headless` ✅)
  - `CAMetalLayer` como `EGLNativeWindowType` — sin ANGLE, sin capa de traducción GL
- [x] **Input HAL** (F8): `NSEvent` → Android `KeyEvent`/`MotionEvent`, cola lock-free (`input-hal` ✅)
- [x] **Vulkan HAL** (F12): detección MoltenVK, `vkEnumerateInstanceExtensionProperties` operativo (`hals-f12` ✅)
- [x] **Camera HAL** (F12): `AVCaptureSession` → `camera3_device_t` stub (`hals-f12` ✅)
- [x] **Clipboard HAL** (F12): `NSPasteboard` ↔ `ClipboardManager` (`hals-f12` ✅)

**Commit clave:** `45bb3fa1` (F7), `2fbb5865` (F8), `20f03217` (F12)

---

### M5 — Audio HAL + Launcher nativo ✅ COMPLETADO
**Criterio de éxito:** AudioUnit CoreAudio funciona; aine-run lanza APKs desde CLI.

- [x] **Audio HAL** (F9): CoreAudio `AudioUnit` → `AudioFlinger` ring buffer PCM, 48kHz/16-bit (`audio-hal` ✅)
  - Sin emulador de audio: PCM nativo → `kAudioUnitSubType_GenericOutput`
- [x] **`aine-run`** (F10): lanzador CLI — `--list`, `--install`, lanza Activity vía `AINE_DALVIKVM` (`launcher-run` ✅)
- [x] Sin notificaciones ni portapapeles avanzado (pendiente G3/G4)

**Commits:** `cab64d84` (F9), `829a8547` (F10)

---

### G1 — Framework stubs: Handler/Looper + campos de instancia/estáticos ✅ COMPLETADO
**Criterio de éxito:** `Handler.postDelayed()` dispara el `Runnable` en el bucle de actividad.

- [x] `handler.h/c`: cola de prioridad de 128 entradas, timer `nanosleep`/`CLOCK_MONOTONIC`
- [x] `heap.h/c`: tabla dinámica de campos por objeto (`iget`/`iput` reales); tabla global 512 entradas para campos estáticos (`sget`/`sput` reales)
- [x] `interp.c`: `iget-object` sólo opcode `0x54`; `iput-object` sólo `0x5b`; `class_desc` en `new-instance`; `handler_drain()` tras `onResume`
- [x] `jni.c`: `Handler.postDelayed()` y `Handler.post()` conectados a `handler_post_delayed()`
- [x] `interp_run_runnable()`: despacha `run()` sobre el objeto Runnable por nombre de clase
- [x] `HandlerTest.apk`: Activity que publica callback 100ms → log `"handler-fired"` (`handler-loop` ✅ #16)

**Commit:** `282c65bd`

---

### G2 — Stdlib Java esencial: colecciones, Math, String.format ✅ COMPLETADO
**Criterio de éxito:** `ArrayList`, `HashMap`, `Math`, y `String.format` funcionan en Activity mode.

- [x] `heap.h/c`: `OBJ_ARRAYLIST` — add/get/set/size/remove/clear/contains/toArray; `OBJ_HASHMAP` — put/get/containsKey/remove/size/keySet/values
- [x] `jni.c`: `java.util.ArrayList` / `LinkedList` / `Vector` dispatch completo; `java.util.HashMap` / `LinkedHashMap` / `TreeMap` dispatch completo; `java.lang.Math` (abs/max/min/sqrt/pow/floor/ceil/round/random); `System.currentTimeMillis()` / `nanoTime()` reales (no stub 0); `System.arraycopy()` implementado; `String.format()` con `%d/%s/%f/%b/%c/%x/%e/%g` y ancho de campo
- [x] `interp.c`: `filled-new-array` 0x24/0x25 rellena `arr_obj[]` para registros `REG_OBJ` (corrige varargs `Object[]`); `instance-of` 0x20 comprueba `class_desc` + tipos `OBJ_ARRAYLIST`/`OBJ_HASHMAP`; `fill-array-data` 0x26 implementado (elem_width 1/2/4/8); boxing primitivo usa `heap_string()` para seguridad cross-call
- [x] `new-instance` 0x22 crea `OBJ_ARRAYLIST`/`OBJ_HASHMAP` para `java.util.*`
- [x] `G2Test.apk`: Activity que ejercita time, ArrayList, HashMap, Math, String.format (`g2-stdlib` ✅ #17)

**Commit:** `9c4aaa9b`

---

### G3 — Threads + SharedPreferences + Intent extras 🔲 PRÓXIMO
**Criterio de éxito:** `new Thread(runnable).start()` ejecuta el Runnable; `SharedPreferences` persiste entre llamadas; `Intent.putExtra`/`getStringExtra` funciona.

- [ ] `handler.c`: `Thread.sleep()` via `nanosleep`; Thread stub que despacha Runnable sincrónicamente en el bucle
- [ ] `jni.c`: `SharedPreferences` vía fichero JSON en `/tmp/aine-prefs/`; `Intent.putExtra`/`getExtra` en tabla de campos; `Bundle.getString`/`putString`
- [ ] `interp.c`: `move-exception` (0x0d) conectado a estado de excepción; try/catch básico sobre excepciones stubadas
- [ ] `G3Test.apk`: verifica Thread, SharedPreferences, Intent extras

---

### G4 — Resources + View stub + TextView ✅ (parcial — sin UI real)
**Criterio de éxito:** `getResources().getString(R.string.*)` devuelve strings del APK; `setContentView` no crashea con layouts.

- [ ] `aine-pm`: extraer `resources.arsc` de APK y parsear tabla de strings básica
- [ ] `jni.c`: `Resources.getString(int)` busca en tabla extraída; `setContentView(int)` es no-op con log
- [ ] `TextView.setText` → `Log.d` (sin UI real — UI real en M4-full)
- [ ] `G4Test.apk`: Activity con R.string, setContentView, TextView.setText

---

### M4-full — Ventana nativa: primera UI Android visible en NSWindow 🔲
**Criterio de éxito:** Una app con `TextView` + `Button` renderiza en una `NSWindow` real.

> **Nota:** Sin emulador, sin contenedor. El renderer es Metal/CAMetalLayer directamente.
> El frame GLES se presenta vía MoltenVK → Metal → NSWindow. La ruta de gráficos
> ya está parcialmente lista (F7 EGL headless ✅, F12 Vulkan ✅).

- [ ] `SurfaceFlinger` mínimo: composita el buffer de la app en `CAMetalLayer` en `NSWindow`
- [ ] `Choreographer` + VSYNC sincronizado con `CADisplayLink` de macOS
- [ ] `View` básico: `setContentView` → crear `NSView` + `CAMetalLayer`
- [ ] `Canvas` / `Paint` básico: dibujar texto en buffer Metal
- [ ] `InputFlinger` conectado: clicks `NSEvent` → `MotionEvent` → `View.onTouchEvent`
- [ ] Test: app calculadora AOSP renderiza UI y responde a clicks

---

### M5-full — Audio en apps reales, Notificaciones, Portapapeles 🔲
**Criterio de éxito:** Una app de media FOSS arranca y produce sonido.

> Audio HAL CoreAudio ya funciona (F9 ✅). Falta conectarlo al ciclo de vida de la app.

- [ ] `AudioTrack.write()` → ring buffer → `AudioUnit` play loop
- [ ] Notificaciones → `UNUserNotificationCenter` macOS
- [ ] Portapapeles bidireccional: `ClipboardManager` ↔ `NSPasteboard` (F12 partial ✅)
- [ ] `aine-launcher` GUI (SwiftUI): drag & drop de APK, lista de apps instaladas
- [ ] Ventana nativa: botones minimize/maximize/close macOS → `onPause`/`onResume`/`onDestroy`
- [ ] Test: AntennaPod o Session arrancan y producen output audible

---

### M6 — Beta pública 🔲
**Criterio de éxito:** Release en GitHub con >10 apps FOSS verificadas.

- [ ] Rendimiento: sin workarounds JIT, bytecode optimizado o compilación AOT ligera
- [ ] Gestión de memoria: presión de memoria macOS → `onTrimMemory` en apps
- [ ] > 10 apps FOSS documentadas como compatibles
- [ ] Website del proyecto + documentación de usuario
- [ ] Lista de compatibilidad publicada
- [ ] Release firmado con notarization de Apple

---

## Implementación — detalle de Features

Cada Feature (Fx) corresponde a un commit atómico con sus CTests:

| Feature | Descripción | CTest | Commit |
|---|---|---|---|
| F1 | Shims: epoll, futex, eventfd, prctl | #2–#5 | `9f4794f3` |
| F2 | Shims: /proc, ashmem, timerfd | — | `c0a9e03f` |
| F3 | aine-shim completo + tests reales | — | `c0a9e03f` |
| F4 | aine-pm: APK parser + registry | #7 | `974d41b2` |
| F5 | aine-loader + binder roundtrip | #1,#6,#8 | `9933c998` |
| F6 | aine-dalvik opcodes + JNI expandido | #9 | `d7d2a714` |
| F7 | EGL headless Metal + IOSurface | #10 | `45bb3fa1` |
| F8 | Input HAL NSEvent→KeyEvent/MotionEvent | #11 | `2fbb5865` |
| F9 | Audio HAL CoreAudio AudioUnit | #12 | `cab64d84` |
| F10 | aine-run lanzador CLI | #13 | `829a8547` |
| F11 | Activity lifecycle dalvikvm+aine-run | #14 | `2dc94c3a` |
| F12 | Vulkan/MoltenVK + Camera + Clipboard HAL | #15 | `20f03217` |
| G1 | Handler.postDelayed + iget/iput/sget/sput reales | #16 | `282c65bd` |
| G2 | ArrayList+HashMap+Math+String.format+fill-array-data | #17 | `9c4aaa9b` |

---

## Estimación global

| Escenario | Tiempo desde M0 | Equipo |
|---|---|---|
| 1 desarrollador dedicado | 18–24 meses | Solo |
| 2 desarrolladores | 12–16 meses | Tú + 1 colaborador C/C++ |
| Equipo pequeño (3–4) | 8–12 meses | Con financiación |

> **Principio de diseño:** AINE nunca usará emulación de CPU, VM, emulador Android
> ni contenedor de cualquier tipo. La arquitectura ARM64 compartida entre Apple Silicon
> y Android es la razón por la que esto es posible — igual que Wine con x86.

## Financiación

- **NLnet / NGI Zero** — financiaron ATL, AINE es candidato natural
- **GitHub Sponsors** — desde M1 con demo funcionando
- **Open Collective** — para transparencia de gastos
- Dual-licensing comercial a partir de M6

---

## Métricas de progreso

Cada milestone se considera completado cuando:
1. El criterio de éxito definido pasa en CI (`ctest --test-dir build`)
2. Existe al menos un CTest automatizado que lo verifica
3. Commit atómico con descripción en `git log`

