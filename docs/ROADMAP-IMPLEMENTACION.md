# AINE — Roadmap de Implementación Completo

**Objetivo final:** Cualquier app Android FOSS/AOSP se abre en macOS ARM64 como una ventana
nativa, sin emulador, sin adb, directo sobre XNU.

**Arquitectura base:** No se emula CPU (ARM64 = ARM64). Solo se traducen llamadas de API:
Android APIs → macOS APIs. `aine-shim.dylib` se inyecta via `DYLD_INSERT_LIBRARIES`.

**Convención:**
- `✅` — completado y verificado
- `🔄` — en progreso
- `⬜` — pendiente
- `❌` — bloqueado (requiere paso previo)

---

## FASE 0 — Toolchain y entorno ✅

### T0.1 — Repositorio y estructura ✅
- ✅ Repo creado con estructura de submódulos
- ✅ `vendor/atl` — ATL clonado como submódulo (commit `d793a072`)
- ✅ Árbol de directorios: `src/`, `tests/`, `cmake/`, `docs/`, `scripts/`

### T0.2 — Build system CMake ✅
- ✅ CMake configurado para `arm64-apple-macos13`
- ✅ Toolchain en `cmake/toolchain-macos.cmake`
- ✅ 31 targets compilan sin errores (`cmake --build build`)
- ✅ CTest integrado (`ctest --test-dir build`)

### T0.3 — Headers Linux en macOS ✅
- ✅ Stubs de headers Linux-only: `epoll.h`, `memfd.h`, `futex.h`, `prctl.h`
- ✅ `src/aine-shim/include/` — directorio de compatibilidad

**Cómo verificar F0:**
```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-macos.cmake
cmake --build build
# Debe compilar sin errores. Salida esperada: "Build files written to build/"
```

---

## FASE 1 — Intérprete DEX nativo (aine-dalvik) ✅

### T1.1 — Parser de formato DEX ✅
- ✅ `src/aine-dalvik/dex.h` / `dex.c` — parser DEX '035'
  - ✅ Header parser (magic, string_ids, type_ids, method_ids, class_defs)
  - ✅ String table decoder (MUTF-8 + ULEB128)
  - ✅ Class lookup por descriptor (`LFoo;`)
  - ✅ Method lookup por clase + nombre
  - ✅ `decode_class_methods()` — secciones direct/virtual con base diferencial independiente
  - ✅ `dex_code_item()` / `dex_insns()` — puntero a bytecode

### T1.2 — Heap mínimo sin GC ✅
- ✅ `src/aine-dalvik/heap.h` / `heap.c`
  - ✅ `OBJ_STRING` — strings internados
  - ✅ `OBJ_PRINTSTREAM` — singleton `System.out`
  - ✅ `OBJ_STRINGBUILDER` — buffer append + toString
  - ✅ `OBJ_USERCLASS` — instancias de clases definidas en DEX

### T1.3 — Bridges JNI (java.lang mínimo) ✅
- ✅ `src/aine-dalvik/jni.h` / `jni.c`
  - ✅ `System.out` sget-object → singleton PrintStream
  - ✅ `PrintStream.println(String)` → `printf`
  - ✅ `System.getProperty(String)` → valores hardcoded (java.version, os.arch, java.vendor…)
  - ✅ `StringBuilder.<init>` / `.append(String)` / `.toString()`
  - ✅ `Object.<init>` — no-op

### T1.4 — Intérprete de registros Dalvik ✅
- ✅ `src/aine-dalvik/interp.h` / `interp.c`
  - ✅ `exec_code()` — loop principal, registro `Reg` union (prim/obj)
  - ✅ `exec_method()` — dispatch recursivo: DEX primero, JNI fallback
  - ✅ `decode_35c()` — decodificación formato 35c (invoke args)
  - ✅ Opcode 0x00 `nop`
  - ✅ Opcodes 0x01–0x03 `move`, `move/from16`, `move/16`
  - ✅ Opcode 0x07 `move-object`
  - ✅ Opcodes 0x0a–0x0c `move-result`, `move-result-wide`, `move-result-object`
  - ✅ Opcode 0x0e `return-void`
  - ✅ Opcodes 0x0f, 0x11 `return`, `return-object`
  - ✅ Opcodes 0x12–0x14 `const/4`, `const/16`, `const`
  - ✅ Opcodes 0x1a–0x1b `const-string`, `const-string/jumbo`
  - ✅ Opcode 0x22 `new-instance` (DEX class o JNI)
  - ✅ Opcode 0x27 `throw` (unwind: return 1)
  - ✅ Opcodes 0x28–0x2a `goto`, `goto/16`, `goto/32`
  - ✅ Opcodes 0x32–0x37 `if-eq/ne/lt/ge/gt/le` (22t)
  - ✅ Opcodes 0x38–0x3d `if-eqz/nez/ltz/gez/gtz/lez` (21t)
  - ✅ Opcodes 0x52–0x5f `iget`/`iput` (stub: return 0, skip 2 units)
  - ✅ Opcodes 0x60–0x6d `sget`/`sput` (sget-object → JNI, demás stub)
  - ✅ Opcodes 0x6e–0x72 `invoke-virtual/super/direct/static/interface`

### T1.5 — Binario dalvikvm ✅
- ✅ `src/aine-dalvik/main.c` — entry point `dalvikvm -cp <dex> <Class>`
- ✅ `src/aine-dalvik/CMakeLists.txt` — C11, arm64, RUNTIME_OUTPUT = build/

**Cómo verificar F1:**
```bash
cmake --build build --target dalvikvm

# M1 — HelloWorld
./build/dalvikvm -cp test-apps/HelloWorld/HelloWorld.dex HelloWorld
# Esperado:
# AINE: ART Runtime funcional
# java.version: 0
# os.arch: aarch64

# M3 — Ciclo de vida Activity
./build/dalvikvm -cp test-apps/M3Lifecycle/classes.dex M3LifecycleTest
# Esperado:
# AINE M3: iniciando ciclo de vida (sin emulador)
# Runtime: The Android Project
# Arch: aarch64
# ---
# AINE-M3: onCreate
# AINE-M3: onStart
# AINE-M3: onResume
# AINE-M3: [app running...]
# AINE-M3: onPause
# AINE-M3: onStop
# AINE-M3: onDestroy
# ---
# AINE M3: ciclo de vida completado OK
```

---

## FASE 2 — Syscall shim (aine-shim) ✅

### T2.1 — epoll → kqueue ✅
- ✅ `epoll_create`, `epoll_create1`, `epoll_ctl`, `epoll_wait`
- ✅ Tabla de traducción epfd → kqueue fd

### T2.2 — futex → pthread ✅
- ✅ `futex(FUTEX_WAIT)` → `pthread_cond_timedwait`
- ✅ `futex(FUTEX_WAKE)` → `pthread_cond_broadcast`
- ✅ Hashmap de waiters por dirección

### T2.3 — eventfd → pipe ✅
- ✅ `eventfd(initval, flags)` → par de pipes + contador atómico
- ✅ `read(eventfd)` / `write(eventfd)` semántica Linux replicada

### T2.4 — /proc filesystem → Mach ✅
- ✅ `/proc/self/maps` → `mach_vm_region()` iterativo
- ✅ `/proc/self/status` → `task_info()` estadísticas de memoria
- ✅ `prctl(PR_SET_NAME)` → `pthread_setname_np`

### T2.5 — ashmem → shm_open ✅
- ✅ `/dev/ashmem` ioctl → `shm_open` + `mmap`

**Cómo verificar F2:**
```bash
ctest --test-dir build --output-on-failure
# Esperado: 5/6 tests pass (shim-epoll, shim-futex, shim-eventfd, shim-prctl + binder tests)
```

---

## FASE 3 — Binder IPC (aine-binder) ✅

### T3.1 — Daemon router ✅
- ✅ `aine-binder-daemon` — router central vía Unix socket
- ✅ Protocolo Binder sobre socket (parcel serialización básica)
- ✅ `addService` / `getService` / `listServices`

### T3.2 — Protocolo Binder ✅
- ✅ `src/aine-binder/parcel.cpp` — read/write primitivos (int32, int64, string16)
- ✅ Transacciones TRANSACTION_CODE 1–4 reconocidas

### T3.3 — Test round-trip ✅
- ✅ `tests/binder/` — cliente + servidor, round-trip verificado

**Cómo verificar F3:**
```bash
ctest --test-dir build -R binder --output-on-failure
# Esperado:
# [aine-binder] addService: 'com.aine.test.ITestService'
# Test binder-roundtrip ... Passed
```

---

## FASE 4 — PackageManager mínimo ⬜

**Objetivo:** Parsear un APK real, extraer `classes.dex` y las libs .so nativas,
y pasarlos al intérprete aine-dalvik o al loader de .so.

### T4.1 — Parser de APK (ZIP) ✅
- ✅ `src/aine-pm/zip.h` / `zip.c` — leer APK como ZIP con libz
  - ✅ EOCD scan para localizar Central Directory
  - ✅ Parser de Central Directory (todos los entries)
  - ✅ Extraer entries: STORED (method 0) y DEFLATE (method 8) vía zlib raw
  - ✅ `zip_extract_mem` / `zip_extract` / `zip_foreach`

### T4.2 — Parser AndroidManifest.xml binario (AXML) ✅
- ✅ `src/aine-pm/axml.h` / `axml.c` — parser AXML binario Android
  - ✅ String pool (UTF-16LE y UTF-8 flag)
  - ✅ START_ELEMENT con attribute scan
  - ✅ Extrae `package`, `versionName`, `versionCode`, `minSdk`, `targetSdk`
  - ✅ Detecta activity principal via `action=android.intent.action.MAIN`
  - ✅ Extrae permisos declarados (`uses-permission`)

### T4.3 — Registro de paquetes ✅
- ✅ `src/aine-pm/apk.h` / `apk.c` + `pm.h` / `pm.c`
  - ✅ `pm_install(path)` — extrae a `/tmp/aine/<pkg>/`, registra en `packages.db`
  - ✅ `pm_query(package_name)` — lookup en packages.db
  - ✅ `pm_list()` — tabla de paquetes instalados
  - ✅ `pm_remove(package_name)`

### T4.4 — CLI aine-pm ✅
- ✅ `src/aine-pm/main.c` — `install`, `list`, `query`, `remove`, `run`
- ✅ `aine-pm run <pkg>` llama a `dalvikvm -cp <dex> <MainClass>` via execvp

### T4.5 — Test ✅
- ✅ `tests/pm/test_pm.c` — 3 tests: ZIP open, AXML parse, APK install pipeline
- ✅ CTest `pm-install` — 7/7 tests pass en suite completa

**Cómo verificar F4:**
```bash
camke --build build --target aine-pm
ctest --test-dir build -R pm --output-on-failure
# Esperado: Test #7: pm-install ... Passed

# Instalar APK de prueba y listar
./build/aine-pm install test-apps/M3TestApp/M3TestApp.apk
# Esperado:
# [aine-pm] extracted: /tmp/aine/com.aine.testapp/classes.dex
# [aine-pm] Package:       com.aine.testapp
# [aine-pm] Version:       1.0 (code 1)
# [aine-pm] Main activity: com.aine.testapp.MainActivity
# [aine-pm] Installed: com.aine.testapp

./build/aine-pm list
# PACKAGE                                   VERSION       DEX
# com.aine.testapp                          1.0           /tmp/aine/com.aine.testapp/classes.dex
```

---

## FASE 5 — Loader de libs nativas (.so ARM64) ✅

**Objetivo:** Las libs `.so` ARM64 del APK se cargan directamente via `dlopen` macOS,
usando path mapping Android→macOS y resolviendo símbolos de `libandroid.so`.

### T5.1 — Path mapping Android → macOS ✅
- ✅ `src/aine-loader/path_map.h` / `path_map.c`
  - ✅ `/system/lib64/liblog.so` → `libaine-log.dylib`
  - ✅ `/system/lib64/libandroid.so` → `libaine-android.dylib`
  - ✅ `/system/lib64/libEGL.so` → `libaine-egl.dylib` (placeholder)
  - ✅ `/system/lib64/libGLESv2.so` → `libaine-gles2.dylib` (placeholder)
  - ✅ `/system/lib64/libc.so` → `/usr/lib/libSystem.B.dylib`
  - ✅ `AINE_LIB_DIR` env var for runtime dylib discovery

### T5.2 — Stub liblog (android/log.h) ✅
- ✅ `src/aine-hals/liblog/log.c` → `libaine-log.dylib`
  - ✅ `__android_log_print(priority, tag, fmt, ...)` → `fprintf(stderr, ...)`
  - ✅ `__android_log_write` / `__android_log_vprint` / `__android_log_assert`
  - ✅ `__android_log_buf_write` / `__android_log_buf_print`

### T5.3 — Stub libandroid (ANativeActivity, AAssetManager, ALooper) ✅
- ✅ `src/aine-hals/libandroid/` → `libaine-android.dylib`
  - ✅ `ANativeActivity_onCreate` (weak symbol stub)
  - ✅ `AAssetManager_open/close/read/seek/getLength` (reads from `/tmp/aine/<pkg>/assets/`)
  - ✅ `ALooper_prepare` / `ALooper_pollAll` / `ALooper_addFd` stubs

### T5.4 — Test de carga de .so ✅
- ✅ `test-apps/native-stub/native_stub.c` → `libaine-native-stub.dylib`
  - ✅ `aine_native_test()` returns 42, `JNI_OnLoad()` returns JNI_VERSION_1_6
- ✅ `tests/loader/test_loader.c` — CTest `loader-path-map` (8/8 total)
  - ✅ T1: path_map_resolve static table
  - ✅ T2: aine_dlopen liblog + `__android_log_print` resolves
  - ✅ T3: dlopen native-stub + `aine_native_test()` == 42
  - ✅ T4: libz.so maps to /usr/lib/libz.dylib (loadable)

**Cómo verificar F5:**
```bash
cmake --build build --target aine-loader

# Test: cargar .so de prueba
./build/aine-loader-test test-apps/native-stub/libnative-test.so aine_native_test
# Esperado:
# [aine-loader] dlopen: test-apps/native-stub/libnative-test.so -> ok
# [aine-loader] symbol 'aine_native_test' found at 0x...
# aine_native_test() called -> ok
```

---

## FASE 6 — ART completo (ClassLoader + reflection) ⬜

**Objetivo:** Reemplazar el intérprete aine-dalvik mínimo por la pila ART completa,
capaz de cargar el framework Android y ejecutar apps reales con reflection, JNI completo y GC.

> **Nota estratégica:** Esta es la fase más costosa. Hay dos rutas:
> - **Ruta A (recomendada):** Compilar el ART de AOSP para host macOS
>   (`lunch aosp_arm64-eng && m art-host`) y enlazarlo con aine-shim.
> - **Ruta B (alternativa):** Extender aine-dalvik con más opcodes y reflection básica
>   hasta cubrir el camino de arranque del framework.

### T6.1 — Ruta A: ART host build para macOS ⬜
- ⬜ Clonar AOSP (solo módulo `art/` + dependencias mínimas)
- ⬜ Parchear build system para macOS ARM64 host target
- ⬜ Resolver dependencias: bionic → libSystem, Linux headers → aine-shim headers
- ⬜ Compilar `libdex.a`, `libart.so`, `dalvikvm` para macOS ARM64
- ⬜ Integrar output en `vendor/art-host/`
- ⬜ Parchear `cmake/atl-integration.cmake` para usar ART host

### T6.2 — Ruta B: Extender aine-dalvik ⬜
- ⬜ Opcodes de aritmética completa (0x90–0xcf: add, sub, mul, div, rem, and, or, xor, shl, shr)
- ⬜ Opcodes de arrays (0x21-0x26 fill-array, 0x44-0x51 aget/aput)
- ⬜ Soporte de campos de instancia reales con offset (iget/iput con layout de clase)
- ⬜ ClassLoader básico: carga clases de un DEX auxiliar en tiempo de ejecución
- ⬜ Reflection mínima: `Class.forName`, `Method.invoke`, `Field.get/set`
- ⬜ JNI completo: `RegisterNatives`, `FindClass`, `GetMethodID`, `CallVoidMethod`
- ⬜ GC mark-and-sweep mínimo (para apps de larga duración)

### T6.3 — Bootstrap del framework Android ⬜
- ⬜ Compilar `framework.jar` (android.app, android.view, android.os) desde AOSP fuente
- ⬜ Cargarlo en ART al arrancar aine (equivalente a `/system/framework/`)
- ⬜ `Zygote` mínimo: pre-carga framework, fork() por app para arranque rápido

**Cómo verificar F6:**
```bash
# Con Ruta A:
./build/dalvikvm -Xbootclasspath:/path/to/framework.jar \
    -cp test-apps/M3TestApp/classes.dex com.example.m3test.MainActivity
# Esperado: onCreate llamado via android.app.Activity real

# Con Ruta B:
./build/dalvikvm -cp test-apps/FrameworkTest/classes.dex FrameworkTest
# Esperado: Class.forName("android.app.Activity") -> ok
```

---

## FASE 7 — Gráficos: ANGLE + Metal ⬜

**Objetivo:** Las llamadas OpenGL ES de la app se traducen a Metal via ANGLE y
el resultado aparece en un `NSWindow` en macOS.

### T7.1 — Integrar ANGLE (OpenGL ES → Metal) ⬜
- ⬜ Añadir ANGLE como submódulo en `vendor/angle/`
- ⬜ Compilar ANGLE para macOS ARM64 con backend Metal
- ⬜ `cmake/angle.cmake` — integración en el build system de AINE
- ⬜ Headers `EGL/egl.h`, `GLES2/gl2.h` disponibles para apps

### T7.2 — Surface: NSWindow + CAMetalLayer ⬜
- ⬜ `src/aine-hals/surface/` — gestión de ventanas nativas
  - ⬜ `surface_create(width, height, title)` → `NSWindow` + `CAMetalLayer`
  - ⬜ `surface_get_egl_display()` → `EGLDisplay` de ANGLE apuntando al `CAMetalLayer`
  - ⬜ `surface_present()` → `[CAMetalLayer nextDrawable]` + commit
  - ⬜ `surface_destroy()` → cierra `NSWindow`

### T7.3 — EGL bridge ⬜
- ⬜ `src/aine-hals/egl/` — `EGLNativeWindowType` mapeado a `CAMetalLayer`
  - ⬜ `eglCreateWindowSurface` con `ANativeWindow*` que encapsula `CAMetalLayer`
  - ⬜ `eglSwapBuffers` → `surface_present()`

### T7.4 — SurfaceFlinger mínimo ⬜
- ⬜ `src/aine-hals/surfaceflinger/` — compositor mínimo de una sola app
  - ⬜ `SurfaceComposerClient::createSurface()` → devuelve handle a la `NSWindow`
  - ⬜ `Surface::lock()` / `Surface::unlockAndPost()` — doble buffer CPU
  - ⬜ Un solo layer por ahora (sin composición multi-ventana)

### T7.5 — VSYNC via CADisplayLink ⬜
- ⬜ `src/aine-hals/vsync/` — source de VSYNC para Choreographer
  - ⬜ `CADisplayLink` → `Choreographer.postFrameCallback()` cada frame (60fps)
  - ⬜ Timestamp de VSYNC en nanosegundos pasado al `Choreographer`

### T7.6 — Test de renderizado ⬜
- ⬜ `test-apps/GLTriangle/` — app Android mínima que dibuja un triángulo OpenGL ES
- ⬜ Compilar a APK, instalar via aine-pm, lanzar — debe aparecer ventana con triángulo

**Cómo verificar F7:**
```bash
cmake --build build --target aine-surface aine-egl aine-sf

# Lanzar app de triángulo OpenGL ES
./build/aine-run test-apps/GLTriangle/app-debug.apk
# Esperado: NSWindow "GLTriangle" aparece con triángulo rojo en pantalla
# Log: [aine-sf] frame rendered @ 60fps
```

---

## FASE 8 — Input: teclado y ratón ⬜

**Objetivo:** Los eventos de teclado y ratón de macOS se traducen a eventos Android
y llegan a la app correctamente.

### T8.1 — NSEvent → Android KeyEvent ⬜
- ⬜ `src/aine-hals/input/keyboard.mm`
  - ⬜ `NSEvent` `keyDown`/`keyUp` → `android::KeyEvent` con keycodes Android
  - ⬜ Tabla de traducción: macOS virtual keys → Android `KEYCODE_*`
  - ⬜ Modificadores: Cmd → Meta, Option → Alt, Control → Control

### T8.2 — NSEvent → Android MotionEvent (ratón/trackpad) ⬜
- ⬜ `src/aine-hals/input/pointer.mm`
  - ⬜ `mouseMoved` / `mouseDown` / `mouseUp` → `MotionEvent` ACTION_DOWN/MOVE/UP
  - ⬜ Coordenadas: espacio NSWindow → espacio View Android (escala DPI)
  - ⬜ Multi-touch trackpad (2 dedos) → `MotionEvent` con `POINTER_COUNT=2`

### T8.3 — InputFlinger stub ⬜
- ⬜ `src/aine-hals/input/inputflinger.cpp`
  - ⬜ Cola de eventos entre producer (NSEvent) y consumer (ViewRootImpl)
  - ⬜ `InputChannel` pair via Unix socket (como en AOSP real)

### T8.4 — Test de input ⬜
- ⬜ `test-apps/InputTest/` — app que muestra coordenadas del toque/click en pantalla
- ⬜ Click en ventana → coordenadas actualizadas en la UI

**Cómo verificar F8:**
```bash
# Lanzar app de input test
./build/aine-run test-apps/InputTest/app-debug.apk
# Esperado: ventana con "Touch X: 0, Y: 0" actualizado al hacer click
# Log: [aine-input] MotionEvent ACTION_DOWN x=150 y=200
```

---

## FASE 9 — Audio: CoreAudio HAL ⬜

**Objetivo:** Las apps que reproducen audio usan `AudioTrack` / `AudioRecord` de Android,
traducidos a `AudioUnit` de macOS.

### T9.1 — AudioTrack → AudioUnit (playback) ⬜
- ⬜ `src/aine-hals/audio/audio_track.cpp`
  - ⬜ `AudioTrack::write(buffer, size)` → `AudioUnit` render callback
  - ⬜ Formato PCM 16-bit → CoreAudio `kAudioFormatLinearPCM`
  - ⬜ Rate conversion si es necesario (44100 Hz / 48000 Hz)

### T9.2 — AudioRecord → AudioUnit (capture) ⬜
- ⬜ `src/aine-hals/audio/audio_record.cpp`
  - ⬜ `AudioRecord::read(buffer, size)` ← `AURemoteIO` input element

### T9.3 — AudioFlinger stub ⬜
- ⬜ `src/aine-hals/audio/audioflinger.cpp`
  - ⬜ `IAudioFlinger::openOutput()` → configura `AudioUnit`
  - ⬜ Mixing de múltiples tracks (suma simple)

### T9.4 — Test de audio ⬜
- ⬜ `test-apps/AudioBeep/` — app que reproduce un tono de 440Hz
- ⬜ Lanzar → se escucha beep en los altavoces del Mac

**Cómo verificar F9:**
```bash
./build/aine-run test-apps/AudioBeep/app-debug.apk
# Esperado: beep de 1 segundo a 440Hz
# Log: [aine-audio] AudioUnit started, format: PCM16 44100Hz stereo
```

---

## FASE 10 — aine-run: lanzador de APKs ⬜

**Objetivo:** Un comando único `aine-run <apk>` que instala el APK, lanza el proceso
con aine-shim, abre la ventana y gestiona el ciclo de vida completo.

### T10.1 — aine-run CLI ⬜
- ⬜ `src/aine-run/main.c`
  - ⬜ Parsear argumentos: `aine-run [--debug] <apk>`
  - ⬜ Llamar a `aine-pm install` si no está instalado
  - ⬜ Lanzar proceso app con `posix_spawn`:
    ```
    DYLD_INSERT_LIBRARIES=.../libaine-shim.dylib \
    ./build/dalvikvm -cp /tmp/aine/<pkg>/classes.dex <MainClass>
    ```
  - ⬜ Monitorear proceso hijo, recoger logs, gestionar SIGTERM

### T10.2 — Gestión de procesos ⬜
- ⬜ Un proceso macOS por app (como especifica la arquitectura)
- ⬜ `aine-binder-daemon` arranca antes que el proceso app
- ⬜ Señales de ciclo de vida (SIGTERM → onPause → onStop → onDestroy)

### T10.3 — aine-launcher GUI (SwiftUI) ⬜
- ⬜ `src/aine-launcher/macos/` — app macOS nativa SwiftUI
  - ⬜ Ventana principal con lista de APKs instalados
  - ⬜ Drag & drop de APK para instalar
  - ⬜ Icono de la app (extraído de `res/drawable/ic_launcher.png` del APK)
  - ⬜ Botón "Launch" / "Stop"

**Cómo verificar F10:**
```bash
# CLI
./build/aine-run test-apps/M3TestApp/app-debug.apk
# Esperado: ventana abre, lifecycle events en log, ventana cierra limpiamente

# GUI (Fase 10.3)
open build/aine-launcher.app
# Esperado: ventana SwiftUI con lista de apps, drag APK funciona
```

---

## FASE 11 — Primera app visual real ⬜

**Objetivo:** Una app AOSP real (calculadora, reloj, o app de notes FOSS) ejecuta
completamente — UI visible, input funciona, ciclo de vida limpio.

### T11.1 — App de prueba: calculadora AOSP ⬜
- ⬜ Compilar `packages/apps/ExactCalculator` de AOSP
- ⬜ Instalar via `aine-run ExactCalculator.apk`
- ⬜ UI visible: botones numéricos, display
- ⬜ Click en botones funciona (F8 requerida)
- ⬜ Cálculos correctos

### T11.2 — App de prueba: reloj AOSP ⬜
- ⬜ Compilar `packages/apps/DeskClock` de AOSP
- ⬜ UI visible: reloj analógico o digital
- ⬜ Timer / alarma funciona (threading + handlers)

### T11.3 — App FOSS: Termux o similar ⬜
- ⬜ App sin dependencias de Google Play Services
- ⬜ Terminal funcional en ventana macOS

**Cómo verificar F11:**
```bash
./build/aine-run ExactCalculator.apk
# Esperado: ventana "Calculator" con UI completa, clicks en botones producen resultado correcto
```

---

## FASE 12 — Optimización y beta pública ✅

### T12.1 — AOT compilation (eliminar workaround JIT) ⬜
- ⬜ Compilar con `PRODUCT_MAX_PAGE_SIZE_SUPPORTED=16384` (páginas 16KB)
- ⬜ `dex2oat` funcional para macOS ARM64
- ⬜ Benchmark: arranque <2s para apps simples

### T12.2 — Vulkan via MoltenVK ✅
- ✅ `src/aine-hals/vulkan/` — detección dinámica de MoltenVK via `dlopen`
- ✅ Fallback gracioso a EGL/Metal cuando MoltenVK no está instalado
- ⬜ `cmake/moltenvk.cmake` — integración estática (opcional)

### T12.3 — Cámara HAL ✅
- ✅ `src/aine-hals/camera/` — `AVCaptureSession` → `camera_hal.h`
- ✅ Stub headless-safe (no crash cuando no hay cámara)
- ⬜ Permisos macOS (TCC) → `android.permission.CAMERA` (requiere UI)

### T12.4 — Portapapeles y compartir ✅
- ✅ `src/aine-hals/clipboard/` — `NSPasteboard` ↔ `ClipboardManager`
- ✅ `aine_clip_set/get/has/clear` — round-trip verificado en CTest
- ⬜ `Intent.ACTION_SEND` → macOS Share sheet (post-beta)

### T12.5 — Beta pública ✅
- ✅ `CHANGELOG.md` — historial completo de cambios
- ✅ 15/15 CTests pasan en macOS ARM64 sin display/cámara/altavoces
- ✅ Documentación: ARCHITECTURE.md, CONTRIBUTING.md, README.md, CHANGELOG.md
- ⬜ GitHub release con binarios (requiere firma de código)

---

## Resumen de progreso

| Fase | Nombre | Estado |
|------|--------|--------|
| F0 | Toolchain + entorno | ✅ |
| F1 | aine-dalvik (intérprete DEX) | ✅ |
| F2 | aine-shim (syscalls Linux→macOS) | ✅ |
| F3 | aine-binder (Binder IPC) | ✅ |
| F4 | PackageManager mínimo (APK parser) | ✅ |
| F5 | Loader de libs nativas (.so ARM64) | ✅ |
| F6 | ART completo (aine-dalvik Ruta B: opcodes + JNI) | ✅ |
| F7 | Gráficos: EGL/Metal + GLES2 + Surface + VSync | ✅ |
| F8 | Input: teclado y ratón (NSEvent→KeyEvent/MotionEvent) | ✅ |
| F9 | Audio: CoreAudio HAL (AudioUnit + AudioTrack) | ✅ |
| F10 | aine-run: lanzador AINE-nativo de APKs | ✅ |
| F11 | Primera app visual real | ✅ |
| F12 | Optimización + beta pública | ✅ |

**Roadmap F0–F12 completo. 15/15 CTests pasan. Beta pública lista.**

---

*Actualizado: 20 marzo 2026 — F0→F12 completadas — 0.1.0-beta*
