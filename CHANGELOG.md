# CHANGELOG — AINE (Android Translation Layer for macOS ARM64)

## [0.1.0-beta] — 2026-03-20

First public beta. All milestones M0–M3 and roadmap phases F0–F12 complete.

### Added (F0–F6 — Core Runtime)
- **F0**: CMake + Ninja toolchain for macOS ARM64 (`ccache`, `clang`, linker flags)
- **F1**: `aine-dalvik` — Dalvik/DEX bytecode interpreter (HelloWorld, M1)
- **F2**: `aine-shim` — Linux syscall translation layer (epoll, futex, eventfd, prctl, ashmem)
- **F3**: `aine-binder` — Binder IPC (transaction, Service registration, round-trip)
- **F4**: `aine-pm` — Package Manager (APK ZIP extraction, AXML manifest parser, package DB)
- **F5**: `aine-loader` — native `.so` ARM64 loader + `liblog` + `libandroid` stubs
- **F6**: `aine-dalvik` extended — full extended opcode set + JNI dispatch table (9/9 CTest)

### Added (F7–F9 — Platform HALs)
- **F7**: Graphics stack
  - `libegl` — EGL 1.4 backed by Metal (`MTLDevice`, `CAMetalLayer`, `IOSurface`)
  - `libgles2` — GL ES 2.0 stub dylib (all 100+ functions, non-crashing)
  - `surface` — `aine_surface_create_offscreen()` + `aine_surface_create_window()`
  - `surfaceflinger` — CPU double-buffer compositor
  - `vsync` — `CVDisplayLink`-backed VSYNC (macOS 10.4+, not CADisplayLink)
- **F8**: Input HAL
  - Carbon VK → Android `AKEYCODE_*` translation table
  - Thread-safe circular event queue (256 capacity, POSIX mutex/cond)
  - NSEvent keyboard + pointer monitors (`aine-input` dylib, `aine-input-core` static)
- **F9**: Audio HAL
  - `AudioUnit` render callback with ring buffer (single-producer/consumer)
  - `kAudioUnitSubType_GenericOutput` fallback for headless CI
  - `AudioFlinger` stub with 8-track registry

### Added (F10–F12 — Launcher + Integration)
- **F10**: `aine-run` — AINE-native APK launcher
  - `pm_install()` → `apk_install()` → `posix_spawn(dalvikvm)`
  - `--list`, `--query`, `--dry-run` commands
  - `AINE_DALVIKVM` env var override for dalvikvm path
- **F11**: Activity lifecycle in `dalvikvm`
  - Fallback from `main(String[])` to `onCreate/onStart/onResume/onPause/onStop/onDestroy`
  - M3TestApp (com.aine.testapp) runs full Activity lifecycle end-to-end
- **F12**: Beta HALs
  - `vulkan` — MoltenVK runtime detection (`dlopen`), graceful fallback if absent
  - `camera` — `AVCaptureSession` → `camera3_device_t` bridge (headless-safe)
  - `clipboard` — `NSPasteboard` ↔ `ClipboardManager` bridge

### Test Suite  
| ID  | Name                  | Description                               |
|-----|-----------------------|-------------------------------------------|
|  1  | binder-protocol       | Binder transaction wire format            |
|  2  | shim-epoll            | epoll_create/ctl/wait shim                |
|  3  | shim-futex            | futex WAIT/WAKE shim                      |
|  4  | shim-eventfd          | eventfd read/write shim                   |
|  5  | shim-prctl            | prctl PR_SET_NAME shim                    |
|  6  | binder-roundtrip      | Service add + query IPC round-trip        |
|  7  | pm-install            | APK install → package DB                  |
|  8  | loader-path-map       | .so → .dylib path remapping               |
|  9  | dalvik-f6-opcodes     | Extended DEX opcode interpreter           |
| 10  | surface-egl-headless  | EGL + Metal + IOSurface (no display)      |
| 11  | input-hal             | Keymap + event queue (no NSEvent)         |
| 12  | audio-hal             | AudioUnit GenericOutput PCM (no speaker)  |
| 13  | launcher-run          | aine-run --list (no APKs needed)          |
| 14  | activity-lifecycle    | Full M3TestApp Activity lifecycle via JVM |
| 15  | hals-f12              | Vulkan detect + Camera stub + Clipboard   |

**All 15/15 CTests pass on macOS ARM64 (Apple Silicon, headless).**

### Architecture
- Platform: **macOS ARM64** (Apple Silicon) — no emulation, no containers
- Build: `cmake -S . -B build && cmake --build build`
- Test: `ctest --test-dir build --output-on-failure`
- Branch: `feature/m0-toolchain`

### Known Limitations (post-beta)
- GLES2 functions are stubs (no actual GPU rendering yet — requires Metal shader pipeline)
- Vulkan requires user to install MoltenVK separately (`brew install molten-vk`)
- Camera requires macOS camera permission (TCC) to access real hardware
- Handler/Looper are no-ops (postDelayed does not fire after a real delay)
- No AOT compilation (dex2oat for macOS ARM64 is pending)
