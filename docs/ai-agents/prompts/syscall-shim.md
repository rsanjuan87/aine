# Prompt: implementar syscall shim

Usa este prompt con tu agente AI para implementar nuevas traducciones de syscalls en aine-shim.

---

## Prompt base

```
Contexto del proyecto AINE:
- Capa de compatibilidad Android → macOS (tipo Wine pero para Android en Apple Silicon)
- src/aine-shim/ es una dylib inyectada via DYLD_INSERT_LIBRARIES
- Intercepta syscalls Linux que no existen en macOS/XNU
- C11, target macOS 13+ ARM64, compilado con clang de Apple

Necesito implementar la traducción de [SYSCALL_LINUX] para macOS.

Comportamiento Linux esperado:
[descripción exacta de la semántica que Android necesita]

Equivalente en macOS:
[kqueue / dispatch_source_t / shm_open / etc.]

Casos edge que debo manejar:
[lista de casos específicos]

Por favor genera:
1. src/aine-shim/[nombre].c con la implementación completa
2. Declaraciones en src/aine-shim/include/sys/[nombre].h
3. tests/shim/test_[nombre].c con tests de la semántica básica
4. Las líneas DYLD_INTERPOSE() necesarias

Restricciones:
- No usar APIs privadas de Apple
- Manejar errores via errno (no excepciones)
- Comentar con // AINE: las decisiones de diseño no obvias
- Si hay código de referencia en Darling (darlinghq/darling), indicarlo

Formato de DYLD_INTERPOSE a usar:
#define DYLD_INTERPOSE(_new, _old) \
  __attribute__((used)) static struct { const void *replacement; const void *replacee; } \
  _interpose_##_old __attribute__((section("__DATA,__interpose"))) = \
  { (const void*)_new, (const void*)_old };
```

---

## Syscalls pendientes por implementar (M1)

Copia el prompt base y sustituye [SYSCALL_LINUX] por cada una:

### eventfd
- Linux: crea un fd que actúa como contador/notificador entre procesos/threads
- macOS: implementar con `pipe(2)` + `atomic_int` como contador
- Semántica: `read` bloquea hasta valor > 0, devuelve y resetea; `write` añade al contador

### timerfd_create / timerfd_settime / timerfd_gettime
- Linux: timer como file descriptor, legible con read()
- macOS: implementar con `dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER)` + pipe
- Casos edge: CLOCK_REALTIME vs CLOCK_MONOTONIC, TFD_NONBLOCK

### inotify_init / inotify_add_watch / inotify_rm_watch
- Linux: notificaciones de cambios en filesystem
- macOS: FSEvents + kqueue con EVFILT_VNODE
- Solo necesitamos: IN_MODIFY, IN_CREATE, IN_DELETE, IN_CLOSE_WRITE

### signalfd
- Linux: recibir señales como datos en un fd
- macOS: implementar con `kqueue + EVFILT_SIGNAL`
- Casos edge: sigmask, SFD_NONBLOCK, SFD_CLOEXEC

### prctl (opciones específicas Android)
- PR_SET_NAME: renombrar thread — macOS: `pthread_setname_np(name)` (sin arg thread)
- PR_GET_NAME: obtener nombre de thread
- PR_SET_DUMPABLE, PR_GET_DUMPABLE: stub devolviendo 1
- Otras opciones: errno EINVAL (no crash)
