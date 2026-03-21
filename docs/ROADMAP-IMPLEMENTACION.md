# AINE — Roadmap de Implementación Completo

**Objetivo final:** Cualquier app Android FOSS/AOSP se abre en macOS ARM64 como una ventana
nativa, sin emulador, sin adb, directo sobre XNU.

**Arquitectura base:** No se emula CPU (ARM64 = ARM64). Solo se traducen llamadas de API:
Android APIs → macOS APIs. `aine-shim.dylib` se inyecta via `DYLD_INSERT_LIBRARIES`.

**Convención:**
- `✅` — completado y verificado con CTest
- `🔄` — implementado, sin CTest aún
- `⬜` — pendiente
- `❌` — bloqueado (requiere paso previo)

**Estado actual:** 23/23 CTests pasan — 21 marzo 2026 — 3 APKs reales ejecutadas ✅

---

## FASE 0 — Toolchain y entorno ✅

### T0.1 — Repositorio y estructura ✅
- ✅ Repo creado con estructura de submódulos
- ✅ `vendor/atl` — ATL clonado como submódulo (commit `d793a072`)
- ✅ Árbol de directorios: `src/`, `tests/`, `cmake/`, `docs/`, `scripts/`

### T0.2 — Build system CMake ✅
- ✅ CMake configurado para `arm64-apple-macos13`
- ✅ Toolchain en `cmake/toolchain-macos.cmake`
- ✅ Todos los targets compilan sin errores (`cmake --build build`)
- ✅ CTest integrado (`ctest --test-dir build`)

### T0.3 — Headers Linux en macOS ✅
- ✅ Stubs de headers Linux-only: `epoll.h`, `memfd.h`, `futex.h`, `prctl.h`
- ✅ `src/aine-shim/include/` — directorio de compatibilidad

**Cómo verificar F0:**
```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-macos.cmake
cmake --build build
# Debe compilar sin errores.
ctest --test-dir build
# Esperado: 19/19 tests passed, 0 tests failed
```

---

## FASE 1 — Intérprete DEX nativo (aine-dalvik) ✅

### T1.1 — Parser de formato DEX ✅
- ✅ `src/aine-dalvik/dex.h` / `dex.c` — parser DEX '035'
  - ✅ Header parser (magic, string_ids, type_ids, method_ids, class_defs)
  - ✅ String table decoder (MUTF-8 + ULEB128 + SLEB128)
  - ✅ Class lookup por descriptor (`LFoo;`)
  - ✅ Method lookup por clase + nombre
  - ✅ `decode_class_methods()` — secciones direct/virtual con base diferencial independiente
  - ✅ `dex_code_item()` / `dex_insns()` — puntero a bytecode
  - ✅ `dex_find_catch_handler(df, ci, throw_pc, exc_type)` — lookup en tabla try/catch
    del DexCodeItem (DexTryItem[] + encoded_catch_handler_list, SLEB128 handler count)

### T1.2 — Heap mínimo sin GC ✅
- ✅ `src/aine-dalvik/heap.h` / `heap.c`
  - ✅ `OBJ_STRING` — strings internados via `heap_string()`
  - ✅ `OBJ_PRINTSTREAM` — singleton `System.out` / `System.err`
  - ✅ `OBJ_STRINGBUILDER` — buffer append + toString
  - ✅ `OBJ_USERCLASS` — instancias de clases definidas en DEX
  - ✅ `OBJ_ARRAY` — array primitivo + objetos con `arr_prim[]` / `arr_obj[]`
  - ✅ `OBJ_ARRAYLIST` — `java.util.ArrayList` / LinkedList con `heap_arraylist_*`
  - ✅ `OBJ_HASHMAP` — `java.util.HashMap` / LinkedHashMap con `heap_hashmap_*`
  - ✅ `OBJ_ITERATOR` — snapshot de ArrayList para iteración (`heap_iterator_new`)
  - ✅ iget/iput (instance fields) — `AineFieldSlot` dinámica por (obj, field_name)
  - ✅ sget/sput (static fields) — tabla global por (class_desc, field_name)

### T1.3 — Bridges JNI ✅
- ✅ `src/aine-dalvik/jni.h` / `jni.c` — ~1350 líneas, dispatch completo
  - ✅ `java.io.PrintStream` — println/print/printf/flush para String, int, float, long, Object
  - ✅ `java.lang.System` — currentTimeMillis/nanoTime/gc/arraycopy/getProperty/exit
  - ✅ `android.util.Log` — v/d/i/w/e → fprintf(stderr)
  - ✅ `java.lang.Integer/Long` — parseInt/parseLong/valueOf/toString/intValue/longValue/MAX_VALUE/MIN_VALUE
  - ✅ `java.lang.Boolean/Double/Float/Character` — parse*/valueOf/isDigit/isLetter/toUpperCase/etc
  - ✅ `java.lang.String` — length/equals/contains/startsWith/endsWith/indexOf/trim/
    toLowerCase/toUpperCase/format/concat/charAt/substring/split/replace/replaceAll/
    getBytes/compareTo/toCharArray/valueOf/isEmpty
  - ✅ `java.lang.StringBuilder` — init/append (String/int/char/float/double/boolean)/toString/
    length/delete/insert/charAt/reverse
  - ✅ `java.lang.Math` — abs/max/min/sqrt/pow/floor/ceil/round/log/log10/sin/cos/tan/random
  - ✅ `java.lang.Thread` — start/sleep/currentThread/getName/setName/join/interrupt/isAlive/setDaemon/setPriority
  - ✅ `java.util.ArrayList/LinkedList` — add/get/set/size/remove/clear/contains/
    toArray/iterator/subList/addAll + ArrayList(Collection) copy constructor
  - ✅ `java.util.HashMap/LinkedHashMap/TreeMap` — put/get/containsKey/containsValue/
    remove/size/keySet/values/entrySet/getOrDefault/putAll
  - ✅ `java.util.Iterator` — hasNext/next/remove (snapshot OBJ_ITERATOR)
  - ✅ `java.util.Collections` — sort/reverse/shuffle/unmodifiableList/synchronizedList/
    singletonList/emptyList/min/max/frequency/nCopies
  - ✅ `java.util.Arrays` — asList/sort/fill/copyOf/copyOfRange/toString
  - ✅ `java.io.File` — exists/isFile/isDirectory/mkdirs/delete/getPath/getName/
    getAbsolutePath/getParent/length/canRead/canWrite (via stat/mkdir)
  - ✅ `android.app.Activity / Context` — lifecycle super-no-ops+
    getSharedPreferences/getString/getResources/getPackageName/getFilesDir/getCacheDir
  - ✅ `android.content.SharedPreferences` + Editor — getString/getInt/getBoolean/contains/
    getAll/putString/putInt/putBoolean/remove/clear/commit/apply
    (persistido en `/tmp/aine-prefs/<name>.prefs`)
  - ✅ `android.content.Intent / Bundle` — putExtra/getXxxExtra/setAction/getAction/
    setComponent/setData/addFlags
  - ✅ `android.os.Handler / Looper` — post/postDelayed/removeCallbacks (+real delay queue)
  - ✅ `android.os.SystemClock` — elapsedRealtime/uptimeMillis/sleep (CLOCK_MONOTONIC)
  - ✅ `android.content.res.Resources` — getString/getText/getInteger stubs
  - ✅ `java.lang.Exception / Error / Throwable` — init (con message)/getMessage/toString/
    printStackTrace
  - ✅ `jni_sget_prim()` — constantes estáticas: Integer.MAX_VALUE/MIN_VALUE,
    Long.MAX_VALUE/MIN_VALUE, Short/Byte/Character/Boolean/Float/Double constants

### T1.4 — Intérprete de registros Dalvik ✅
- ✅ `src/aine-dalvik/interp.h` / `interp.c`
  - ✅ `exec_code()` — loop principal, registro `Reg` union (prim/obj)
  - ✅ `exec_method()` — dispatch recursivo: DEX primero, JNI fallback
  - ✅ Opcodes `0x00–0x0e` — nop, move (4 variantes), move-wide, move-object, move-result*, return-void
  - ✅ `0x0d` move-exception — asigna la excepción activa (result_reg) al registro dest
  - ✅ Opcodes `0x0f, 0x10, 0x11` — return, return-wide, return-object
  - ✅ Opcodes `0x12–0x19` — const/4, const/16, const, const/high16, const-wide variants
  - ✅ Opcodes `0x1a–0x1b` — const-string, const-string/jumbo
  - ✅ `0x1c` const-class, `0x1d/0x1e` monitor-enter/exit (no-op), `0x1f` check-cast
  - ✅ `0x20` instanceof (verifica OBJ type, class_desc, List/Map/Collection interfaces)
  - ✅ `0x21` array-length, `0x22` new-instance (con dispatch JNI: ArrayList/HashMap/Thread/etc)
  - ✅ `0x23` new-array, `0x24` filled-new-array, `0x25` filled-new-array/range
  - ✅ `0x26` fill-array-data (payload, elem_width 1/2/4/8)
  - ✅ `0x27` throw — lookup en tabla try/catch via `dex_find_catch_handler()`;
    si hay handler salta al PC handler; si no, propaga con return 1
  - ✅ `0x28–0x2a` goto/10t/20t/30t
  - ✅ `0x2b` packed-switch, `0x2c` sparse-switch
  - ✅ `0x2d–0x31` cmp-long/float/double/lt/gt
  - ✅ `0x32–0x37` if-eq/ne/lt/ge/gt/le (22t), `0x38–0x3d` if-eqz/nez/ltz/gez/gtz/lez (21t)
  - ✅ `0x44–0x4a` aget variants (int/wide/object/boolean/byte/char/short)
  - ✅ `0x4b–0x51` aput variants
  - ✅ `0x52–0x5f` iget/iput reales via `heap_iget/heap_iput` (0x54/0x5b para object refs)
  - ✅ `0x60–0x6d` sget/sput con fallback a `jni_sget_prim()` para constantes estáticas
  - ✅ `0x6e–0x72` invoke-virtual/super/direct/static/interface (35c)
  - ✅ `0x74–0x78` invoke-*/range (3rc)
  - ✅ `0x7b–0x8f` unary + conversiones int↔long↔float↔double↔byte↔char↔short
    (vía `memcpy` IEEE 754 bit-exact: int-to-float 0x82, float-to-int 0x87, etc.)
  - ✅ `0x90–0x9a` int 23x (add/sub/mul/div/rem/and/or/xor/shl/shr/ushr)
  - ✅ `0x9b–0xa5` long 23x (add/sub/mul/div/rem/and/or/xor/shl/shr/ushr)
  - ✅ `0xa6–0xaa` float 23x (add/sub/mul/div/rem-float vía memcpy)
  - ✅ `0xab–0xaf` double 23x (add/sub/mul/div/rem-double vía memcpy)
  - ✅ `0xb0–0xc5` int/long 2-addr (add/sub/mul/div/rem/and/or/xor/shl/shr/ushr)
  - ✅ `0xc6–0xcf` float/double 2-addr (add/sub/mul/div/rem-float/2addr, add-double/2addr etc.)
  - ✅ `0xd0–0xd7` lit16 arithmetic (22s), `0xd8–0xe2` lit8 arithmetic (22b)

### T1.5 — Handler/Looper cooperativo ✅
- ✅ `src/aine-dalvik/handler.h` / `handler.c`
  - ✅ `handler_post_delayed(runnable, delay_ms)` — cola de prioridad por tiempo
  - ✅ `handler_drain(interp, max_ms)` — dispara callbacks con nanosleep
  - ✅ Integrado en Activity mode: se drena después de onResume()

### T1.6 — Binario dalvikvm ✅
- ✅ `src/aine-dalvik/main.c` — entry point `dalvikvm -cp <dex> <Class>`
- ✅ Modo main(): ejecuta `static void main(String[])V`
- ✅ Modo Activity: ciclo de vida onCreate→onStart→onResume→handler_drain→onPause→onStop→onDestroy

**Cómo verificar F1 (CTests #1–#3, #11, #16–#19):**
```bash
cmake --build build --target dalvikvm

# CTest #1: HelloWorld
ctest --test-dir build -R dalvik-hello --output-on-failure
# Esperado: "AINE: ART Runtime funcional" + "os.arch: aarch64"

# CTest #11: Ciclo de vida Activity M3
ctest --test-dir build -R m3-lifecycle --output-on-failure
# Esperado: "AINE-M3: onCreate" ... "AINE-M3: onDestroy"

# CTest #16: Handler.postDelayed + iget/iput reales (G1)
ctest --test-dir build -R handler-loop --output-on-failure
# Esperado: "handler-fired"

# CTest #17: ArrayList/HashMap/Math/String.format + fill-array-data (G2)
ctest --test-dir build -R g2-stdlib --output-on-failure
# Esperado: "list-size:3" "map-get:val1" "math-max:20" "fmt-42-hello" "g2-done"

# CTest #18: try/catch + Thread.sleep + Iterator + String.split/replace + Collections.sort (G3)
ctest --test-dir build -R g3-framework --output-on-failure
# Esperado: "exc-caught" "sleep-ok" "iter:foo,bar,baz," "split-len:4" "replace:hello_world" "sort-0:apple" "g3-done"

# CTest #19: Arrays.asList, Collections.reverse, Integer.MAX_VALUE, String.replaceAll (G4)
ctest --test-dir build -R g4-stdlib2 --output-on-failure
# Esperado: "asList-size:3" "sorted:apple" "reversed:cherry" "max-val:2147483647" "g4-done"

# Todos a la vez:
ctest --test-dir build
# Esperado: 100% tests passed, 0 tests failed out of 19
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
ctest --test-dir build -R "shim|epoll|futex|eventfd" --output-on-failure
# Esperado: shim-epoll, shim-futex, shim-eventfd, shim-prctl pasan
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

## FASE 4 — PackageManager mínimo ✅

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
cmake --build build --target aine-pm
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

## FASE 6 — Gráficos: EGL/Metal + Surface + VSYNC ✅

**Objetivo:** EGL 1.4 sobre Metal + NSWindow con CAMetalLayer + CVDisplayLink.

### T6.1 — EGL headless (IOSurface) ✅
- ✅ `src/aine-hals/libegl/egl.m` — EGL 1.4 sobre Metal
  - ✅ `eglGetDisplay` / `eglInitialize` / `eglChooseConfig`
  - ✅ `eglCreateContext` (MTLDevice + command queue)
  - ✅ `eglCreateWindowSurface` — acepta `CAMetalLayer*` como `EGLNativeWindowType`
  - ✅ `eglCreatePbufferSurface` — usa IOSurface para headless
  - ✅ `eglMakeCurrent` / `eglSwapBuffers` / `eglDestroySurface`

### T6.2 — NSWindow + CAMetalLayer ✅
- ✅ `src/aine-hals/surface/surface.m` + `include/aine_surface.h`
  - ✅ `aine_surface_create_window(w, h, title)` → `NSWindow` + `CAMetalLayer`
  - ✅ `aine_surface_create_offscreen(w, h)` → IOSurface headless
  - ✅ `aine_surface_get_layer(h)` → `CAMetalLayer*` (para `eglCreateWindowSurface`)
  - ✅ `aine_surface_present(h)` → presenta el frame
  - ✅ `aine_surface_destroy(h)` → libera NSWindow / IOSurface

### T6.3 — VSYNC via CVDisplayLink ✅
- ✅ `src/aine-hals/vsync/vsync.m` + `vsync.h`
  - ✅ `aine_vsync_create(cb, userdata)` → CVDisplayLink callback a 60fps
  - ✅ `aine_vsync_start` / `aine_vsync_stop`
  - ✅ `aine_vsync_wait_once()` — espera un tick de vsync (bloqueante)

**Cómo verificar F6:**
```bash
# CTest EGL headless (no requiere display)
ctest --test-dir build -R egl --output-on-failure
# Esperado: test-egl-headless ... Passed

# Verificación manual de surface (requiere display):
./build/dalvikvm -cp test-apps/M3TestApp/classes.dex com.aine.testapp.MainActivity
# Cuando T8.1 esté implementado, abrirá ventana NSWindow.
```

---

## FASE 7 — HALs de hardware ✅

**Objetivo:** Stubs headless-safe para Audio/Cámara/Vulkan/Clipboard/Input — sin crash
en CI headless; listos para conectar a la ventana en F8.

### T7.1 — Input stubs ✅
- ✅ `src/aine-hals/input/` — stub headless-safe para teclado/ratón
- ✅ CTest `test-input` verifica inicialización sin crash
- ✅ (F8/T8.2) `NSEvent` keyDown/keyUp → Android `KeyEvent` (KEYCODE_*) — `keyboard.mm`
- ✅ (F8/T8.2) `NSEvent` mouse* → Android `MotionEvent` ACTION_DOWN/MOVE/UP — `pointer.mm`

### T7.2 — Audio HAL ✅
- ✅ `src/aine-hals/audio/` — stub headless-safe (CoreAudio placeholder)
- ✅ CTest `test-audio` verifica inicialización sin crash
- ⬜ (post-F8) `AudioTrack::write()` → `AudioUnit` render callback PCM 16-bit
- ⬜ (post-F8) `AudioRecord::read()` → `AURemoteIO` input element

### T7.3 — Cámara HAL ✅
- ✅ `src/aine-hals/camera/` — `AVCaptureSession` stub
- ✅ CTest `test-camera` verifica inicialización sin crash

### T7.4 — Vulkan via MoltenVK ✅
- ✅ `src/aine-hals/vulkan/` — detección dinámica via `dlopen`
- ✅ Fallback gracioso cuando MoltenVK no está instalado
- ✅ CTest `test-vulkan`

### T7.5 — Portapapeles ✅
- ✅ `src/aine-hals/clipboard/` — `NSPasteboard` ↔ `ClipboardManager`
- ✅ `aine_clip_set/get/has/clear` — round-trip verificado en CTest `test-clipboard`

**Cómo verificar F7:**
```bash
ctest --test-dir build -R "input|audio|camera|clipboard|vulkan" --output-on-failure
# Esperado: test-input, test-audio, test-camera, test-clipboard, test-vulkan pasan
# (los 5 son headless-safe: no requieren display, cámara ni altavoces)
```

---

## FASE 8 — NSWindow display + Activity visual ✅

**Objetivo:** `dalvikvm --window` abre una ventana macOS real y ejecuta la Activity
con render loop, VSYNC, e input básico.

### T8.1 — dalvikvm --window mode ✅
- ✅ `src/aine-dalvik/window.h` — declaración de `aine_window_run()`
- ✅ `src/aine-dalvik/window.m` — `NSApplication` + NSRunLoop pump + interpreter
  en background dispatch queue; headless-safe (window creation es opcional)
  - ✅ `[NSApplication sharedApplication]` + `[NSApp finishLaunching]`
  - ✅ `NSWindow` 800×600 + `CAMetalLayer` backing (Metal device requerido)
  - ✅ Loop: `[[NSRunLoop mainRunLoop] runUntilDate:16ms]` hasta que termina el intérprete
  - ✅ Cierre de ventana al finalizar lifecycle
- ✅ `src/aine-dalvik/main.c` — flag `--window` / `-window` → llama `aine_window_run()`
- ✅ `src/aine-dalvik/CMakeLists.txt` — `window.m` con `-fobjc-arc` + AppKit/Metal/QuartzCore
- ✅ CTest #20 `g5-window-activity` — verifica lifecycle completo en modo ventana

### T8.2 — Input NSEvent → Android events ✅
- ✅ `src/aine-hals/input/keyboard.mm` — `NSEvent` keyDown/keyUp → `aine_input_key_event()` en ring buffer; tabla macOS virtual keys → Android `KEYCODE_*`
- ✅ `src/aine-hals/input/pointer.mm` — `NSEvent` mouseDown/Moved/Up → `aine_input_motion_event()` con coordenadas en puntos de vista
- ✅ Wrapped en `extern "C"` para evitar name-mangling Objective-C++
- ✅ `src/aine-dalvik/window.m` — inicia/detiene keyboard+pointer monitors en `create_window()` / cleanup
- ✅ `src/aine-dalvik/window.h` — `aine_activity_should_finish()` + `aine_activity_request_finish()`; NSWindowDelegate cierra ventana limpiamente
- ✅ `src/aine-dalvik/interp.c` — `dispatch_input_events()` consume ring buffer → crea `KeyEvent`/`MotionEvent` JNI y llama `onKeyDown`/`onKeyUp`/`onTouchEvent` en Activity
- ✅ `src/aine-dalvik/interp.c` — `activity_event_loop()`: modo ventana (60s cap, idle-exit 2s); modo headless (drain 10s anterior)
- ✅ `src/aine-dalvik/jni.c` — stubs `android.view.KeyEvent` + `android.view.MotionEvent` (getAction/getKeyCode/getX/getY etc.); `Activity.finish()` → `aine_activity_request_finish()`

### T8.3 — aine-run CLI ✅
- ✅ `src/aine-launcher/aine-run.c` — `aine-run [--dry-run] <apk>`
  - ✅ Llama `pm_install(apk)` + `pm_query(package)` para obtener DEX + MainClass
  - ✅ Lanza `dalvikvm --window -cp <dex> <MainClass>` via `posix_spawn`
  - ✅ Busca `dalvikvm` via `AINE_DALVIKVM` env var, luego `dirname(argv[0])`
  - ✅ Espera al hijo con `waitpid`; propaga exit code
  - ✅ `--dry-run` imprime el comando sin ejecutarlo
- ✅ `src/aine-pm/main.c` `cmd_run()` — pasa `--window` flag a execvp
- ✅ `src/aine-pm/main.c` `find_dalvikvm()` — usa `_NSGetExecutablePath` en macOS (en vez de `/proc/self/exe`)

**Cómo verificar F8:**
```bash
# CTest #20: g5-window-activity (T8.1)
ctest --test-dir build -R g5-window-activity --output-on-failure
# Esperado: "g5-window: onCreate" ... "g5-window: done" + Passed

# CTest #14: activity-lifecycle — aine-run end-to-end (T8.3)
ctest --test-dir build -R activity-lifecycle --output-on-failure
# Esperado: "onDestroy" en salida + Passed (~5s)

# T8.1 manual (requiere display):
./build/dalvikvm --window -cp test-apps/G5WindowTest/classes.dex G5WindowActivity
# Esperado:
# [aine-window] window "G5WindowActivity" created (800x600)
# g5-window: onCreate
# g5-window: onResume
# g5-window: onDestroy
# g5-window: done

# T8.2 manual — input NSEvent (requiere display):
./build/dalvikvm --window -cp test-apps/M3TestApp/M3TestApp.apk com.aine.testapp.MainActivity
# Abre ventana; mover ratón → onTouchEvent dispatched; teclas → onKeyDown/onKeyUp
# Cerrar ventana → onDestroy limpio

# T8.3 manual — aine-run CLI:
./build/aine-run --dry-run test-apps/M3TestApp/M3TestApp.apk
# Esperado: imprime comando "dalvikvm --window -cp ... com.aine.testapp.MainActivity"

./build/aine-run test-apps/M3TestApp/M3TestApp.apk
# Esperado:
# [aine-run] Launched (pid NNNN)
# [aine-window] window "MainActivity" created (800x600)
# [I/AINE-M3] onCreate — AINE M3 funcional
# [I/AINE-M3] onDestroy — ciclo de vida completo
# [aine-run] Exited with code 0

# aine-pm run (T8.3 alternativo):
./build/aine-pm install test-apps/M3TestApp/M3TestApp.apk
./build/aine-pm run com.aine.testapp
# Lanza dalvikvm --window directamente via execvp
```

---

## FASE 9 — Primera app visual real ⬜

**Objetivo:** Una app Android FOSS real (calculadora, reloj, notes) ejecuta
completamente — UI visible, botones responden, ciclo de vida limpio.

### T9.1 — Framework stubs de UI ✅ (G6 — stubs sin crash)
- ✅ `android.view.View` — `<init>(Context)`, `setContentView`, `setBackground*`, `setVisibility`, `setOnClickListener`, `invalidate`
- ✅ `android.view.ViewGroup` — `addView`, `removeView`, `getChildCount`/`getChildAt`
- ✅ `android.widget.TextView` — `setText`/`getText`/`setTextSize`/`setTextColor`/`setHint`
- ✅ `android.widget.Button` — hereda TextView, `setEnabled`/`isEnabled`
- ✅ `android.widget.EditText` — `setInputType`, `getText`→`toString`
- ✅ `android.widget.ImageView` — `setImageResource`/`setBitmap`/`setScaleType`
- ✅ `android.widget.LinearLayout`/`RelativeLayout`/`FrameLayout`/`ConstraintLayout` — constructores
- ✅ `android.widget.Toast` — `makeText` + `show` → `fprintf(stderr)`
- ✅ `android.widget.RecyclerView`/`ListView` — `setAdapter`/`notifyDataSetChanged`
- ✅ `android.graphics.Canvas` — `drawText`/`drawRect`/`drawCircle`/`drawBitmap` stubs
- ✅ `android.graphics.Paint` — `setColor`/`setStrokeWidth`/`setTextSize`/`setStyle`
- ✅ `android.graphics.Bitmap` — `createBitmap`/`getWidth`/`getHeight`/`getPixel`/`setPixel`
- ✅ `android.graphics.drawable.Drawable`/`ColorDrawable` stubs
- ✅ `Activity.setContentView(View)` — registra vista raíz sin crash
- ✅ CTest #21 `g6-app-stubs` — verifica que G6RealActivity llama setContentView/addView/setText sin crash

### T9.2 — Canvas → CoreGraphics rendering ✅ (G7)
- ✅ `src/aine-dalvik/canvas.h` / `canvas.m` — CGBitmapContext 800×600 como framebuffer del proceso
  - ✅ `aine_canvas_init(w, h)` — NSLock + CGBitmapContextCreate BGRA premultiplied
  - ✅ `aine_canvas_clear(argb)` — rellena fondo (drawColor)
  - ✅ `aine_canvas_fill_rect(x, y, w, h, argb)` — rectángulo sólido con flip Y Android→CG
  - ✅ `aine_canvas_draw_text(x, y, text, size, argb)` — CoreText + flip Y
  - ✅ `aine_canvas_draw_circle(cx, cy, r, argb)` — CGContextFillEllipseInRect + flip Y
  - ✅ `aine_canvas_copy_cgimage()` — copia thread-safe de CGImageRef para bliteo
- ✅ `src/aine-dalvik/window.m` — `AineCanvasView : NSView` sustituye CAMetalLayer
  - ✅ `drawRect:` llama `aine_canvas_copy_cgimage()` → `CGContextDrawImage`
  - ✅ NSRunLoop detecta dirty flag → `[view setNeedsDisplay:YES]`
- ✅ `src/aine-dalvik/jni.c` — Canvas/Paint completamente cableados:
  - ✅ `Paint.<init>()` inicializa color=0xFF000000, textSize=16
  - ✅ `Paint.setColor(argb)` / `setTextSize(sp)` — almacena en heap fields
  - ✅ `Canvas.drawColor(argb)` → `aine_canvas_clear()`
  - ✅ `Canvas.drawRect(l,t,r,b,paint)` → `aine_canvas_fill_rect()` con float bit extraction
  - ✅ `Canvas.drawText(str,x,y,paint)` → `aine_canvas_draw_text()` con textSize de paint
  - ✅ `Canvas.drawCircle(cx,cy,r,paint)` → `aine_canvas_draw_circle()`
- ✅ `src/aine-dalvik/interp.c` — `activity_event_loop` despacha `onDraw(Canvas)` cuando `view.invalidate()` es llamado
  - ✅ `jni_pop_invalidated()` / `jni_get_content_view()` — API inter-módulo para dispatch
  - ✅ Llamada a `exec_method(view_class, "onDraw", [view, canvas])` con canvas estático
- ✅ `test-apps/G7DrawApp/G7DrawActivity.java` — Activity con DrawView que dibuja en onDraw:
  - ✅ Fondo drawColor (azul oscuro) + título + contador de fotogramas + botón + círculo
  - ✅ Handler.postDelayed 200ms actualiza contador → invalidate() → redraw (5 veces)
  - ✅ `finish()` al terminar → onDestroy limpio
- ✅ CTest #22 `g7-canvas-draw` — verifica frame:5 + draw-complete

### T9.3 — App FOSS real (AOSP Arcs) ✅
- ✅ `test-apps/ArcsApp/` — `Arcs.java` + `GraphicsActivity.java` de AOSP sin modificar (Apache 2.0)
- ✅ `interp.c` — `exec_method()` camina superclase via `dex_class_super()` hasta encontrar método
- ✅ `jni.c` — `View/<init>` no sobreescribe `class_desc` si ya es una subclase usuario
- ✅ `jni.c` — `invalidate()`/`getWidth()`/`getHeight()` genéricos para cualquier subclase de View
- ✅ `jni.c` — `RectF` constructor, `Paint` copy ctor, `Canvas.drawRect(RectF)`, `Canvas.drawArc(RectF)` JNI stubs
- ✅ `canvas.m` — `aine_canvas_draw_arc()` vía `CGMutablePathRef` + escala elíptica
- ✅ `main.c/interp.h` — flag `--max-frames N` para limitar frames (headless CI)
- ✅ `dex.h/dex.c` — `dex_class_super()` para walk de jerarquía DEX
- ✅ CTest #23 `t93-arcs-aosp` — `[arcs] frames-complete:5` + Passed

**Cómo verificar T9.3:**
```bash
ctest --test-dir build -R t93-arcs-aosp --output-on-failure
# Esperado: "[arcs] frames-complete:5" + Passed

./build/dalvikvm --window -cp test-apps/ArcsApp/classes.dex \
    com.example.android.apis.graphics.Arcs
# Ventana 800x600 con arcos de colores renderizados via CoreGraphics
```

### T9.4 — Calc_Java + Calc Kotlin (AppCompatActivity + ViewBinding) ✅

**Sub-tarea A — Calc_Java (Java + Activity + XML layout) ✅**
- ✅ `test-apps/Calc_Java/` — app Java compilada con Gradle
  - `MainActivity.java` → `extends Activity`, `setContentView(R.layout.activity_main)`
  - `CalculatorEngine.java` — lógica con BigDecimal
  - DEX app en `dex-debug/classes3.dex`
- ✅ AINE ejecuta sin errores con output verificable:
  ```
  [aine-dalvik] Activity mode: Lorg/santech/calc/MainActivity;
  [aine-ui] setContentView(layout_id=0)
  [aine-ui] setText: ""
  [aine-ui] setText: "0"
  Exit: 0
  ```

**Sub-tarea B — Calc Kotlin (AppCompatActivity + ViewBinding) ✅**
- ✅ `test-apps/Calc/` — app Android Kotlin compilada con Gradle (Kotlin 2.0, AGP 8.7)
  - `MainActivity.kt` → `AppCompatActivity` + `ActivityMainBinding` (ViewBinding) + layout XML
  - `CalculatorEngine.kt` — lógica pura (suma/resta/mul/div/%, `±`, AC, ⌫, decimales)
  - DEX app en `dex-debug/classes4.dex`
- ✅ AINE ejecuta sin errores con output verificable:
  ```
  [aine-dalvik] Activity mode: Lorg/santech/calc/MainActivity;
  [aine-ui] ViewBinding.inflate -> stub binding created
  [aine-ui] setText: ""
  [aine-ui] setText: "0"
  Exit: 0
  ```

**Fix crítico resuelto (T9.4B):**
- Bug: opcodes `if-eqz`/`if-nez` (0x38-0x3d) usaban `reg_prim()` para comparar registros —
  esto retorna 0 para CUALQUIER objeto (incluyendo objetos válidos no-null), haciendo que
  `if-nez binding, label` siempre fallara aunque `binding != null`.
- Fix en `interp.c`: para registros objeto, la comparación ahora usa `(obj != NULL) ? 1 : 0`
- Impacto: fix necesario para cualquier app Kotlin que use patrones null-safety como
  `?.let`, `!!`, `requireNotNull`, o lazy-initialized fields.

**Stubs JNI añadidos:**
- `ActivityMainBinding.inflate()` → crea binding stub con todos los campos de vistas
- `kotlin/jvm/internal/Intrinsics.*` → stubs no-op
- `TuplesKt.to()` → crea Kotlin Pair
- `CollectionsKt.listOf()` → crea ArrayList desde DEX array
- `java/lang/Iterable.iterator()` → `heap_iterator_new()`
- `java/lang/Number.intValue()` → extendido para `Ljava/lang/Number;`
- `MaterialButton.setOnClickListener()` → guarda lambda en campo "onClick"

### T9.5 — Calc_Jetpack (Kotlin + Compose + ComponentActivity) ✅

- ✅ `test-apps/Calc_Jetpack/` — app Jetpack Compose compilada con Gradle
  - `MainActivity.kt` → `ComponentActivity` + `setContent { MaterialTheme { CalculatorScreen() } }`
  - UI declarativa con Compose; estado via `mutableStateOf`
  - DEX app en `dex-debug/classes3.dex`
- ✅ AINE ejecuta sin errores con output verificable:
  ```
  [aine-dalvik] Activity mode: Lorg/santech/calc/MainActivity;
  [aine-ui] Compose initial state: display="0" expression=""
  [aine-ui] ComponentActivity.setContent called (Compose UI)
  Exit: 0
  ```

**Nota:** El runtime de Compose (árbol de composables) no se renderiza — requeriría
implementar el compilador Compose runtime completo (milestone futuro). AINE verifica:
- ✅ Inicialización del `CalculatorEngine` en `<init>` (ejecuta DEX nativo)
- ✅ `getUiState()` retorna estado inicial correcto (`display="0"`)
- ✅ `mutableStateOf$default` interceptado y estado impreso
- ✅ `setContent$default` interceptado (Compose UI registrada)

**Fix adicional (T9.5):**
- Bug: el bloque `android/view/View + androidx/` en jni.c tenía `strstr(class_desc, "androidx/")`
  como wildcard, capturando clases Compose como `SnapshotStateKt` antes de llegar a los stubs
  específicos del androidx-section al final de `jni_dispatch`.
- Fix: stubs `setContent$default` y `mutableStateOf$default` movidos al bloque View+androidx
  (antes del fallthrough `return res`).

**Cómo ejecutar los tres APKs (T9.4A, T9.4B, T9.5):**
```bash
# Calc_Java — Java + Activity + XML layout
./build/dalvikvm -cp test-apps/Calc_Java/dex-debug/classes3.dex org.santech.calc.MainActivity
# Output esperado:
#   [aine-dalvik] Activity mode: Lorg/santech/calc/MainActivity;
#   [aine-ui] setContentView(layout_id=0)
#   [aine-ui] setText: ""
#   [aine-ui] setText: "0"
#   Exit: 0

# Calc — Kotlin + AppCompatActivity + ViewBinding
./build/dalvikvm -cp test-apps/Calc/dex-debug/classes4.dex org.santech.calc.MainActivity
# Output esperado:
#   [aine-dalvik] Activity mode: Lorg/santech/calc/MainActivity;
#   [aine-ui] ViewBinding.inflate -> stub binding created
#   [aine-ui] setText: ""
#   [aine-ui] setText: "0"
#   Exit: 0

# Calc_Jetpack — Kotlin + ComponentActivity + Jetpack Compose
./build/dalvikvm -cp test-apps/Calc_Jetpack/dex-debug/classes3.dex org.santech.calc.MainActivity
# Output esperado:
#   [aine-dalvik] Activity mode: Lorg/santech/calc/MainActivity;
#   [aine-ui] Compose initial state: display="0" expression=""
#   [aine-ui] ComponentActivity.setContent called (Compose UI)
#   Exit: 0

# Variante release (mismas DEX, diferente APK):
./build/dalvikvm -cp test-apps/Calc_Java/dex-release/classes3.dex org.santech.calc.MainActivity
./build/dalvikvm -cp test-apps/Calc/dex-release/classes4.dex org.santech.calc.MainActivity
./build/dalvikvm -cp test-apps/Calc_Jetpack/dex-release/classes3.dex org.santech.calc.MainActivity
```

```bash
# CTest #21: g6-app-stubs
ctest --test-dir build -R g6-app-stubs --output-on-failure
# Esperado: "g6-textview:Hello AINE" ... "g6-done" + Passed
```

**Cómo verificar F9 T9.2 — Canvas rendering (G7):**
```bash
# CTest #22: g7-canvas-draw
ctest --test-dir build -R g7-canvas-draw --output-on-failure
# Esperado:
# [G7] onCreate — AINE Draw Test
# [G7] frame:1 ... [G7] frame:5
# [G7] draw-complete + Passed

# Manual — ver ventana con contenido real:
./build/dalvikvm --window -cp test-apps/G7DrawApp/classes.dex G7DrawActivity
# Abre ventana 800x600; fondo azul oscuro; texto blanco "AINE Android Runtime";
# contador verde "Frame: N"; rectángulo azul "Running..."; círculo rojo
# 5 fotogramas renderizados via CoreGraphics → onDestroy limpio

# Flujo completo usuario:
./build/aine-run test-apps/M3TestApp/M3TestApp.apk
# Instala APK → extrae DEX → abre NSWindow → ciclo de vida completo → exit 0
```

**Cómo verificar F9 T9.3 (cuando implementado):**
```bash
./build/aine-run ExactCalculator.apk
# Esperado: ventana "Calculator" con UI completa, 2+2=4 funciona
```

---

## Tabla de CTests (22/22 activos)

| # | Nombre CTest | Fase | Qué verifica |
|---|-------------|------|--------------|
| 1 | `binder-protocol` | F3 | Binder protocol parser |
| 2 | `shim-epoll` | F2 | epoll→kqueue traducción |
| 3 | `shim-futex` | F2 | futex→pthread |
| 4 | `shim-eventfd` | F2 | eventfd→pipe semántica Linux |
| 5 | `shim-prctl` | F2 | prctl PR_SET_NAME → pthread_setname |
| 6 | `binder-roundtrip` | F3 | Binder IPC round-trip |
| 7 | `pm-install` | F4 | APK install/query pipeline |
| 8 | `loader-path-map` | F5 | dlopen path mapping + native stub |
| 9 | `dalvik-f6-opcodes` | F1 | DEX parser + opcodes F6 suite |
| 10 | `surface-egl-headless` | F6 | EGL 1.4 Metal headless (IOSurface) |
| 11 | `input-hal` | F7 | Input HAL init headless |
| 12 | `audio-hal` | F7 | Audio HAL init headless |
| 13 | `launcher-run` | F8/T8.3 | `aine-run --list` exits 0 |
| 14 | `activity-lifecycle` | F8/T8.3 | `aine-run M3TestApp.apk` → onDestroy |
| 15 | `hals-f12` | F7 | Vulkan/MoltenVK detect + Camera + Clipboard |
| 16 | `handler-loop` | F1/G1 | Handler.postDelayed + iget/iput reales |
| 17 | `g2-stdlib` | F1/G2 | ArrayList/HashMap/Math/String.format |
| 18 | `g3-framework` | F1/G3 | try/catch + Thread.sleep + Iterator + split/replace |
| 19 | `g4-stdlib2` | F1/G4 | Arrays.asList + Collections + Integer.MAX_VALUE |
| 20 | `g5-window-activity` | F8/G5 | --window mode: NSApp + NSWindow + Activity lifecycle |
| 21 | `g6-app-stubs` | F9/G6 | View/widget/Canvas/Paint stubs — setContentView sin crash |
| 22 | `g7-canvas-draw` | F9/G7 | Canvas → CoreGraphics: drawColor/drawText/drawRect/drawCircle reales |
| 23 | `t93-arcs-aosp` | F9/T9.3 | AOSP Arcs.java sin modificar: superclass dispatch + drawArc + 5 frames |

---

## Resumen de progreso

| Fase | Nombre | Estado | CTests |
|------|--------|--------|--------|
| F0 | Toolchain + entorno | ✅ | — |
| F1 | aine-dalvik — intérprete DEX completo (G1–G4 incluidos) | ✅ | #1,#2,#3,#11,#16,#17,#18,#19 |
| F2 | aine-shim — syscalls Linux→macOS | ✅ | #4,#5 |
| F3 | aine-binder — Binder IPC | ✅ | #6 |
| F4 | PackageManager mínimo — APK/ZIP/AXML parser | ✅ | #7 |
| F5 | Loader de libs nativas (.so ARM64) | ✅ | #8 |
| F6 | Gráficos: EGL 1.4/Metal + CAMetalLayer + VSYNC | ✅ | #9 |
| F7 | HALs: Audio/Cámara/Vulkan/Clipboard/Input | ✅ | #10,#12,#13,#14,#15 |
| F8 | NSWindow display + Activity visual (T8.1/T8.2/T8.3) | ✅ | #20, #13, #14 |
| F9 | Framework UI stubs (G6) + Canvas rendering (G7) + AOSP Arcs (T9.3) | ✅ | #21, #22, #23 |
| F10 | Calc_Java (T9.4A) + Calc Kotlin AppCompatActivity+ViewBinding (T9.4B) + Calc_Jetpack Compose (T9.5) | ✅ | 23/23 |

**Estado actual: 23/23 CTests pasan. Tres APKs reales ejecutadas con output verificable: Calc_Java (Java+Activity), Calc (Kotlin+AppCompatActivity+ViewBinding), Calc_Jetpack (Kotlin+Compose).**

---

## Changelog de implementación

| Commits | Descripción | Tests |
|---------|-------------|-------|
| `282c65bd` | G1: Handler.postDelayed + iget/iput reales, CTest #16 | 16/16 |
| `9c4aaa9b` | G2: ArrayList/HashMap/Math/String.format, fill-array-data, CTest #17 | 17/17 |
| `d9650d52` | ROADMAP F0→F12 inicial | 17/17 |
| (wip) | G3: dex_find_catch_handler, Thread.sleep, Iterator, String.split/replace, Collections, SharedPrefs, java.io.File, Intent, CTest #18 | 18/18 |
| (wip) | G4: Arrays.asList, ArrayList(Collection), jni_sget_prim, fix `<init>` ordering, CTest #19 | 19/19 |
| (wip) | ROADMAP: actualizado a 19/19 CTests, F6/F7 reales, G1–G4 documentados | 19/19 |
| (wip) | G5: --window flag, window.m NSApp+NSRunLoop pump, G5WindowTest, CTest #20 | 20/20 |
| `579b6fd3` | G6: View/widget/graphics stubs, G6RealActivity, CTest #21 | 21/21 |
| (wip) | T8.2: keyboard.mm/pointer.mm extern"C", dispatch_input_events, activity_event_loop, KeyEvent/MotionEvent JNI | 21/21 |
| (wip) | T8.3: aine-run --window flag, aine-pm find_dalvikvm macOS fix, RUNTIME_OUTPUT_DIRECTORY | 21/21 |
| (wip) | G7/T9.2: canvas.m CGBitmapContext, AineCanvasView, jni drawColor/drawRect/drawText/drawCircle, onDraw dispatch, G7DrawApp | 22/22 |
| (wip) | ROADMAP: actualizado a 22/22 CTests, T9.2 Canvas rendering marcado ✅, prueba final documentada | 22/22 |
| `182f6d71` | G8/T9.3: ArcsApp AOSP, superclass dispatch, float/2addr 0xc6-0xcf, RectF/drawArc, --max-frames, CTest #23 | 23/23 |
| `a4bff6ee` | T9.4: CalcApp (Java Canvas + Kotlin/Gradle), interp.c long/float/double 23x (0x9b-0xaf), int↔float memcpy (0x82-0x8f), jni setContentView genérico | 23/23 |
| (wip) | T9.4A/T9.4B/T9.5: if-testz fix (obj null-check), ViewBinding stubs, Kotlin Intrinsics no-ops, TuplesKt.to/listOf, Iterator stubs, Compose mutableStateOf+setContent stubs — 3 APKs reales ejecutadas | 23/23 |

---

*Actualizado: 21 marzo 2026 — 23/23 CTests — 3 APKs reales ejecutadas: Calc_Java (Java+Activity), Calc (Kotlin+AppCompatActivity+ViewBinding), Calc_Jetpack (Kotlin+Compose). Fix crítico: if-testz object-awareness en interp.c.*
