# AINE — Quick Reference

## Comandos más usados

```bash
# Setup inicial (solo una vez)
./scripts/init.sh

# Compilar todo
./scripts/build.sh

# Compilar solo un componente
./scripts/build.sh aine-shim
./scripts/build.sh aine-binder

# Compilar clean
./scripts/build.sh --clean

# Testear ART arranca
./scripts/run-app.sh --test-art

# Testear Binder básico
./scripts/run-app.sh --test-binder

# Instalar y ejecutar APK
./scripts/run-app.sh app.apk
./scripts/run-app.sh app.apk --debug

# Ver APKs instalados
./scripts/run-app.sh --list

# Sincronizar desde ATL
./scripts/sync-atl.sh --fetch
./scripts/sync-atl.sh --check
./scripts/sync-atl.sh --analyze 20
./scripts/sync-atl.sh --cherry 20
```

---

## Syscall shim — tabla de estado

| Syscall Linux | macOS Equiv | Archivo | Estado |
|---|---|---|---|
| `epoll_*` | `kqueue/kevent` | `src/aine-shim/epoll.c` | M1 |
| `/proc/self/maps` | `mach_vm_region` | `src/aine-shim/proc.c` | M1 |
| `futex` | `pthread_cond` | `src/aine-shim/futex.c` | M1 |
| `ashmem` | `shm_open` | `src/aine-shim/ashmem.c` | M1 |
| `eventfd` | `pipe+atomic` | `src/aine-shim/eventfd.c` | M1 |
| `timerfd` | `dispatch_source` | `src/aine-shim/timerfd.c` | M1 |
| `inotify` | `FSEvents+kqueue` | `src/aine-shim/inotify.c` | M1 |
| `/dev/binder` | Mach port | `src/aine-shim/binder-dev.c` | M2 |
| `signalfd` | `kqueue EVFILT_SIGNAL` | `src/aine-shim/signalfd.c` | M1 |
| `prctl(PR_SET_NAME)` | `pthread_setname_np` | `src/aine-shim/prctl.c` | M1 |

---

## Bloqueantes conocidos — resumen

| ID | Descripción | Severidad | Milestone |
|---|---|---|---|
| B1 | Page size 16KB (ART AOT roto) | CRÍTICO | M1 |
| B2 | Syscalls Linux-only | CRÍTICO | M1 |
| B3 | /proc filesystem | CRÍTICO | M1 |
| B4 | bionic_translation vs libSystem | ALTO | M1 |
| B5 | ATL no compila en macOS | ALTO | M0 |
| B6 | Binder usa eventfd/ashmem | ALTO | M2 |
| B7 | Mesa → ANGLE | MEDIO | M4 |
| B8 | ld-android vs dyld | MEDIO | M3 |

Ver detalles completos: `docs/blockers.md`

---

## Rutas importantes

| Qué | Dónde |
|---|---|
| ATL upstream (lectura) | `vendor/atl/` |
| Syscall shim | `src/aine-shim/` |
| Binder IPC | `src/aine-binder/` |
| HAL bridges | `src/aine-hals/` |
| Launcher SwiftUI | `src/aine-launcher/` |
| Tests | `tests/` |
| Apps instaladas | `~/Library/AINE/packages/` |
| Logs de apps | `~/Library/AINE/logs/` |
| Documentación | `docs/` |
| Milestones | `docs/milestones/` |
| Prompts AI | `docs/ai-agents/prompts/` |

---

## Referencias externas clave

| Recurso | URL | Para qué |
|---|---|---|
| ATL | gitlab.com/android_translation_layer | Upstream base |
| AOSP | source.android.com | ART, framework, HALs |
| Darling | github.com/darlinghq/darling | Referencia syscalls macOS |
| ANGLE | github.com/google/angle | OpenGL ES → Metal |
| MoltenVK | github.com/KhronosGroup/MoltenVK | Vulkan → Metal |
| XNU Source | github.com/apple-oss-distributions/xnu | Internals de macOS |
| Binder protocol | kernel.org/doc/html/latest/driver-api/android/binder.html | Protocolo Binder |

---

## Variables de entorno útiles para debug

```bash
# Habilitar logs verbosos del shim
export AINE_LOG_LEVEL=debug

# Ver qué dylibs se cargan
export DYLD_PRINT_LIBRARIES=1

# Ver interposiciones activas
export DYLD_PRINT_INTERPOSING=1

# Deshabilitar workaround JIT cuando page size esté resuelto
# (comentar estas flags en build cuando B1 esté resuelto)
# -Xnoimage-dex2oat -Xusejit:false
```
