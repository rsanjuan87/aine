# Binarios compilados de AINE

> **AINE = Aine Is No Emulator** — capa de compatibilidad para ejecutar apps Android en macOS ARM64 de forma nativa, sin emulación de CPU.

Esta página documenta todos los binarios y bibliotecas que genera el sistema de build de AINE, para qué sirve cada uno y cómo se usan.

---

## Tabla de contenidos

- [Compilar el proyecto](#compilar-el-proyecto)
- [Binarios ejecutables](#binarios-ejecutables)
  - [dalvikvm](#dalvikvm)
  - [aine-pm](#aine-pm)
  - [aine-run](#aine-run)
  - [aine-loader-test](#aine-loader-test)
- [Bibliotecas compartidas](#bibliotecas-compartidas)
  - [libaine-shim.dylib](#libaine-shimdylib)
  - [libaine-binder.dylib](#libaine-binderdylib)
- [Flujo completo: de APK a ejecución](#flujo-completo-de-apk-a-ejecución)
- [Referencia rápida](#referencia-rápida)
- [Apps de prueba incluidas](#apps-de-prueba-incluidas)

---

## Compilar el proyecto

Todos los binarios se generan en el directorio `build/` con:

```bash
./scripts/build.sh
```

O para compilar un binario concreto:

```bash
cmake --build build --target dalvikvm
cmake --build build --target aine-pm
cmake --build build --target aine-run
```

**Requisitos mínimos:**
- macOS 13+ en Apple Silicon (ARM64) — obligatorio
- Xcode 15+ con Command Line Tools
- CMake 3.22+, Ninja, Python 3.10+

Tras el build, los ejecutables quedan en:

```
build/
├── dalvikvm
├── aine-pm
├── aine-run
├── aine-loader-test       ← sólo tests
├── libaine-shim.dylib
└── libaine-binder.dylib
```

---

## Binarios ejecutables

### `dalvikvm`

**Fuente:** `src/aine-dalvik/`  
**Target CMake:** `dalvikvm`

El intérprete Dalvik/DEX nativo de AINE. Ejecuta bytecode DEX directamente sobre macOS ARM64 sin emulador ni máquina virtual. Es el componente más bajo nivel: recibe un `.dex` y el nombre de la clase a ejecutar.

Compatible con la interfaz de línea de comandos del `dalvikvm` real de Android, incluyendo flags `-X` que se aceptan y descartan silenciosamente.

**Sintaxis:**

```
dalvikvm [-cp <dexfile>] [--window] [--max-frames N] [-X...] <ClassName> [args...]
```

| Flag | Descripción |
|---|---|
| `-cp <dexfile>` | Ruta al archivo `.dex` a ejecutar |
| `-classpath <dexfile>` | Alias de `-cp` |
| `<ClassName>` | Nombre de la clase (sin `.class`), p.ej. `HelloWorld` o `com.example.MainActivity` |
| `--window` | Abre una `NSWindow` nativa para clases tipo Activity (UI) |
| `--max-frames N` | Salir tras renderizar N frames (útil para tests automáticos) |
| `-X<flag>` | Flags de ART/Dalvik ignorados silenciosamente (compatibilidad) |
| `-D<prop>=<val>` | System properties ignoradas (compatibilidad futura) |

**Ejemplos:**

```bash
# Línea de comandos simple (sin ventana)
./build/dalvikvm -cp test-apps/HelloWorld/HelloWorld.dex HelloWorld

# App con ventana nativa macOS
./build/dalvikvm --window -cp test-apps/G5WindowTest/classes.dex G5WindowActivity

# App Android con package name completo
./build/dalvikvm --window -cp test-apps/M3TestApp/classes.dex com.aine.testapp.MainActivity

# App Jetpack/Compose (múltiples dex)
./build/dalvikvm --window -cp test-apps/Calc_Jetpack/dex-debug/classes3.dex org.santech.calc.MainActivity

# Ciclo de vida Activity completo
./build/dalvikvm -cp test-apps/M3Lifecycle/classes.dex M3LifecycleTest
```

**Cómo funciona internamente:**
1. Lee el `.dex` con `mmap()` — sin copia en memoria
2. Parsea el formato DEX (cabecera, pool de strings, tipos, métodos, código)
3. Ejecuta el método estático `main()` de la clase objetivo con el intérprete de bytecode
4. Si `--window`, crea una `NSWindow` + `CAMetalLayer` y entra en el bucle de eventos de AppKit

**Variables de entorno:**

| Variable | Efecto |
|---|---|
| `DYLD_INSERT_LIBRARIES=build/libaine-shim.dylib` | Activa la traducción de syscalls Linux |
| `AINE_LIB_DIR=<ruta>` | Directorio de `.so` nativas a inyectar |
| `AINE_DEBUG=1` | Verbose logging adicional (requiere build con `--debug-shim`) |

---

### `aine-pm`

**Fuente:** `src/aine-pm/`  
**Target CMake:** `aine-pm`

El gestor de paquetes APK de AINE. Instala APKs, mantiene un registro local en `/tmp/aine/packages.db` y puede lanzar apps directamente delegando en `dalvikvm`.

**Sintaxis:**

```
aine-pm <comando> [args]
```

| Comando | Descripción |
|---|---|
| `install <apk>` | Extrae el APK (ZIP), parsea `AndroidManifest.xml`, copia DEX y `.so` a `/tmp/aine/<package>/` y registra el paquete |
| `list` | Lista todos los paquetes instalados |
| `query <package>` | Muestra información detallada de un paquete (nombre, clase principal, rutas de DEX y libs) |
| `remove <package>` | Elimina el paquete del registro y sus archivos de `/tmp/aine/` |
| `run <package>` | Lanza la app registrada invocando `dalvikvm` con los parámetros correctos |

**Ejemplos:**

```bash
# Instalar un APK
./build/aine-pm install /path/to/MyApp.apk

# Listar paquetes instalados
./build/aine-pm list

# Ver info de un paquete
./build/aine-pm query com.example.myapp

# Lanzar un paquete instalado
./build/aine-pm run com.example.myapp

# Eliminar un paquete
./build/aine-pm remove com.example.myapp
```

**Registro de paquetes:**

`aine-pm` almacena el registro en `/tmp/aine/packages.db` y los archivos extraídos en:

```
/tmp/aine/
└── com.example.myapp/
    ├── classes.dex
    ├── classes2.dex   (si los hay)
    └── lib/
        └── arm64-v8a/
            └── libnative.so
```

**Componentes internos:**
- `zip.c` — lector ZIP mínimo con DEFLATE vía `libz`
- `axml.c` — parser de `AndroidManifest.xml` en formato binario Android
- `apk.c` — extracción de APK (ZIP + AXML)
- `pm.c` — registro de paquetes

---

### `aine-run`

**Fuente:** `src/aine-launcher/aine-run.c`  
**Target CMake:** `aine-run`

El lanzador de alto nivel de AINE. Combina `aine-pm install` + `dalvikvm` en un solo comando: dado un APK, lo instala y ejecuta en un paso. Es el equivalente nativo a `adb install && adb shell am start`.

**Sintaxis:**

```
aine-run [--list | --query <package> | --dry-run <apk> | <apk>]
```

| Opción | Descripción |
|---|---|
| `<apk>` | Instala el APK y lo lanza inmediatamente |
| `--list` | Lista los paquetes actualmente instalados |
| `--query <package>` | Muestra información del paquete |
| `--dry-run <apk>` | Instala el APK e imprime el comando `dalvikvm` que se ejecutaría, sin lanzarlo |

**Ejemplos:**

```bash
# Flujo completo: instalar y ejecutar
./build/aine-run /path/to/MyApp.apk

# Ver qué comando se generaría sin ejecutar
./build/aine-run --dry-run /path/to/MyApp.apk

# Listar apps instaladas
./build/aine-run --list

# Info de un paquete
./build/aine-run --query com.example.myapp
```

**Flujo interno:**

```
aine-run <apk>
    │
    ├─ 1. pm_install(apk)         → /tmp/aine/<pkg>/{classes.dex, lib/}
    ├─ 2. pm_query(package_name)  → main_class, dex_path, lib_dir
    └─ 3. posix_spawn(dalvikvm)   → AINE_LIB_DIR=<lib> dalvikvm -cp <dex> <main>
                                     waitpid() + forward SIGTERM
```

**Variable de entorno:**

| Variable | Efecto |
|---|---|
| `AINE_DALVIKVM=<ruta>` | Fuerza la ruta al binario `dalvikvm` a usar |

---

### `aine-loader-test`

**Fuente:** `src/aine-loader/main.c`  
**Target CMake:** `aine-loader-test`

Herramienta de test/diagnóstico para el subsistema de carga de bibliotecas. Verifica el mapeo de rutas Android → macOS y que `dlopen`/`dlsym` funciona correctamente para `.so` nativas.

**Sintaxis:**

```
aine-loader-test <lib_path> [symbol_name]
```

**Ejemplos:**

```bash
# Comprobar mapeo de ruta
./build/aine-loader-test /system/lib64/liblog.so

# Abrir biblioteca y resolver símbolo
./build/aine-loader-test /data/app/com.example/lib/arm64/libnative.so aine_native_test

# Probar biblioteca local
./build/aine-loader-test ./test-apps/native-stub/libnative-stub.so aine_native_test
```

**Ejemplo de salida:**

```
[aine-loader] path map: /system/lib64/liblog.so -> /usr/local/lib/android/liblog.so
[aine-loader] dlopen: ok (handle=0x600003a0)
[aine-loader] symbol 'aine_native_test' found at 0x10045f320
[aine-loader] aine_native_test() -> 42 -> ok
```

> **Nota:** Este binario es una herramienta de desarrollo/diagnóstico, no parte del flujo de usuario final.

---

## Bibliotecas compartidas

### `libaine-shim.dylib`

**Fuente:** `src/aine-shim/`  
**Target CMake:** `aine-shim`

La pieza central de AINE. Es una `dylib` macOS inyectada en los procesos de app mediante `DYLD_INSERT_LIBRARIES`. Intercepta las llamadas a syscalls Linux que haría el código Android y las traduce a equivalentes macOS/XNU, sin modificar el binario de la app.

**Tabla de traducciones principales:**

| Syscall Android/Linux | Equivalente macOS/XNU | Notas |
|---|---|---|
| `epoll_create/ctl/wait` | `kqueue` / `kevent` | Multiplexado de I/O |
| `futex(WAIT/WAKE)` | `pthread_mutex` + `pthread_cond` | Indexado por dirección de memoria |
| `eventfd` | `pipe` + contador atómico | Semántica simplificada |
| `timerfd_create` | `dispatch_source_t` (GCD) | Timers por proceso |
| `inotify_*` | `FSEvents` + `kqueue` | File watching |
| `open("/dev/binder")` | Socket Mach → `aine-binder-daemon` | fd falso devuelto |
| `open("/proc/self/maps")` | `mach_vm_region()` iterativo | Generado dinámicamente |
| `ashmem ioctl` | `shm_open` + `mmap` | Memoria compartida IPC |
| `prctl(PR_SET_NAME)` | `pthread_setname_np()` | Nombre de thread |
| `clone()` con flags | `posix_spawn` | Procesos ligeros |

**Uso manual:**

```bash
DYLD_INSERT_LIBRARIES=./build/libaine-shim.dylib ./build/dalvikvm -cp my.dex MyClass
```

En la práctica, `aine-run` y `aine-pm run` la inyectan automáticamente cuando es necesario.

**No necesaria para DEX puro:** El intérprete `dalvikvm` ejecuta bytecode Dalvik directamente en ARM64. El shim solo es necesario si el código Java usa JNI con bibliotecas nativas que hagan syscalls Linux directamente.

---

### `libaine-binder.dylib`

**Fuente:** `src/aine-binder/`  
**Target CMake:** `aine-binder`

Reimplementación del protocolo Binder de Android en userspace sobre Mach IPC (macOS) o sockets Unix (Linux). Permite comunicación entre procesos Android que usen el mecanismo de IPC de Binder sin necesidad del driver `/dev/binder` del kernel Linux.

**Componentes:**
- `common/parcel.cpp` — serialización/deserialización de `Parcel`
- `common/binder-protocol.cpp` — cabeceras del protocolo wire (`BC_TRANSACTION`, `BR_REPLY`, etc.)
- `common/service-manager.cpp` — registro y lookup de servicios
- `macos/mach-transport.cpp` — capa de transporte sobre Mach IPC
- `macos/binder-daemon.cpp` — daemon central de routing

**Daemon:**

El sistema Binder de AINE requiere que `aine-binder-daemon` esté corriendo como proceso de sistema. En el flujo final, `aine-run` lo inicia automáticamente si no está activo.

---

## Flujo completo: de APK a ejecución

```
Usuario
   │
   │  ./build/aine-run MyApp.apk
   ▼
aine-run
   │
   ├─ aine-pm install MyApp.apk
   │       │
   │       ├─ Descomprime ZIP (zip.c)
   │       ├─ Parsea AndroidManifest.xml binario (axml.c)
   │       ├─ Extrae classes.dex + lib/arm64-v8a/*.so
   │       └─ Registra en /tmp/aine/packages.db
   │
   ├─ pm_query("com.example.myapp")
   │       └─ Resuelve: dex_path, main_class, lib_dir
   │
   └─ posix_spawn(dalvikvm)
           │
           │  AINE_LIB_DIR=<lib> dalvikvm --window -cp classes.dex com.example.MainActivity
           ▼
       dalvikvm
           │
           ├─ mmap(classes.dex)
           ├─ dex_load() — parsea formato DEX
           ├─ interp_exec() — intérprete de bytecode Dalvik
           │       ├─ Opcodes 0x00–0xe2
           │       ├─ invoke-virtual/direct/static/interface
           │       ├─ JNI stubs (System.out, StringBuilder, Object, ...)
           │       └─ iget/iput, sget/sput, new-instance, throw, goto, if-*
           │
           └─ [--window] AppKit NSWindow + CAMetalLayer
                   └─ Rendering nativo macOS ARM64
```

---

## Referencia rápida

```bash
# ── Build ──────────────────────────────────────────────────────────────
./scripts/build.sh                          # Todo el proyecto
cmake --build build --target dalvikvm       # Solo dalvikvm
cmake --build build --target aine-pm        # Solo aine-pm
cmake --build build --target aine-run       # Solo aine-run

# ── dalvikvm ───────────────────────────────────────────────────────────
./build/dalvikvm -cp <dex> <ClassName>              # Sin ventana
./build/dalvikvm --window -cp <dex> <ClassName>     # Con NSWindow
./build/dalvikvm --window --max-frames 60 -cp <dex> <ClassName>

# ── aine-pm ────────────────────────────────────────────────────────────
./build/aine-pm install <apk>
./build/aine-pm list
./build/aine-pm query  <package>
./build/aine-pm run    <package>
./build/aine-pm remove <package>

# ── aine-run ───────────────────────────────────────────────────────────
./build/aine-run <apk>                    # Instalar + ejecutar
./build/aine-run --dry-run <apk>          # Solo mostrar comando
./build/aine-run --list
./build/aine-run --query <package>

# ── Tests ──────────────────────────────────────────────────────────────
cd build && ctest --output-on-failure     # 17 tests
```

---

## Apps de prueba incluidas

Las apps en `test-apps/` son proyectos Android compilados a DEX para probar el intérprete en diferentes escenarios. No requieren instalación, Solo se pasa el `.dex` directamente a `dalvikvm`.

| App | DEX | Clase principal | Descripción |
|---|---|---|---|
| `HelloWorld` | `HelloWorld/HelloWorld.dex` | `HelloWorld` | Hello World básico, stdout |
| `DalvikTest` | `DalvikTest/classes.dex` | `DalvikTest` | Tests unitarios de opcodes |
| `HandlerTest` | `HandlerTest/classes.dex` | `HandlerTest` | Test del bucle Handler/Looper |
| `G2Test` | `G2Test/classes.dex` | `G2Test` | stdlib Java (G2: stdlib) |
| `G3Test` | `G3Test/classes.dex` | `G3Test` | Collections, iteradores |
| `G4Test` | `G4Test/classes.dex` | `G4Test` | Aritmética y tipos |
| `G5WindowTest` | `G5WindowTest/classes.dex` | `G5WindowActivity` | Primera ventana NSWindow |
| `G6RealActivity` | `G6RealActivity/classes.dex` | `G6Activity` | Activity con layout |
| `G7DrawApp` | `G7DrawApp/classes.dex` | `G7DrawActivity` | Canvas 2D nativo |
| `M3Lifecycle` | `M3Lifecycle/classes.dex` | `M3LifecycleTest` | Ciclo de vida Activity completo |
| `M3TestApp` | `M3TestApp/classes.dex` | `com.aine.testapp.MainActivity` | App M3 de referencia |
| `Calc` | `Calc/classes.dex` | — | Calculadora simple |
| `Calc_Java` | `Calc_Java/classes.dex` | — | Calc en Java puro |
| `Calc_Jetpack` | `Calc_Jetpack/dex-debug/` | `org.santech.calc.MainActivity` | Calc con Jetpack Compose |
| `CalcApp` | `CalcApp/classes.dex` | — | Calc (variante APK) |
| `ArcsApp` | `ArcsApp/classes.dex` | — | Test de arcos y paths |

**Ejemplo con HelloWorld:**
```bash
./build/dalvikvm -cp test-apps/HelloWorld/HelloWorld.dex HelloWorld
# Salida: Hello, World!
```

**Ejemplo con ventana:**
```bash
./build/dalvikvm --window -cp test-apps/G5WindowTest/classes.dex G5WindowActivity
# Abre una ventana macOS nativa con el contenido Android renderizado
```

---

## Ver también

- [ARCHITECTURE.md](../../ARCHITECTURE.md) — Diseño técnico de AINE
- [ROADMAP.md](../../ROADMAP.md) — Milestones y estado de implementación
- [docs/milestones/](../milestones/) — Detalle de cada milestone
- [docs/blockers.md](../blockers.md) — Bloqueantes conocidos y soluciones
- [CONTRIBUTING.md](../../CONTRIBUTING.md) — Cómo contribuir
