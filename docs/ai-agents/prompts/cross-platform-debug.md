# Prompt: debugging cross-platform (Linux vs macOS)

Usa este prompt cuando algo funciona en Linux pero falla en macOS (o viceversa).

---

## Prompt base

```
Contexto AINE cross-platform:

AINE corre sobre dos plataformas:
- Linux: syscalls Android disponibles nativamente (epoll, eventfd, /dev/ashmem, /proc...)
- macOS: aine-shim traduce esas syscalls → XNU equivalentes

Tengo un comportamiento diferente entre plataformas:

En Linux: [comportamiento correcto / output esperado]
En macOS: [comportamiento incorrecto / error / crash]

Stack trace en macOS:
[pegar stack trace]

Código relevante (src/aine-shim/macos/[archivo].c):
[pegar código]

Por favor:
1. Identifica qué syscall o primitiva de Linux está causando la diferencia
2. Verifica si la implementación en macos/ cubre este caso edge
3. Sugiere el fix en la implementación macOS
4. Indica si hay un test en tests/shared/ que debería cubrir este caso

Restricciones:
- La solución va en src/aine-shim/macos/ o src/aine-binder/macos/
- NO modificar common/ con código platform-específico
- NO modificar vendor/atl/ (es upstream, solo lectura)
```

---

## Checklist de debugging cross-platform

Antes de abrir un issue o usar el agente, verifica:

```bash
# 1. El test shared falla en macOS pero no en Linux?
cmake --build build --target test
# Si el test ni existe: añadirlo es parte del fix

# 2. El shim está cargado?
DYLD_PRINT_INTERPOSING=1 DYLD_INSERT_LIBRARIES=build/src/aine-shim/libaine-shim.dylib \
  ./build/dalvikvm -cp test.dex Main 2>&1 | grep "aine_"

# 3. La syscall está siendo interceptada?
AINE_LOG_LEVEL=debug AINE_LOG_SHIM_CALLS=1 ./tu-comando 2>&1 | grep "\[AINE-shim\]"

# 4. Comparar strace (Linux) vs dtruss (macOS)
# Linux:  strace -e trace=epoll_create,epoll_ctl,epoll_wait ./comando
# macOS:  sudo dtruss -t kevent ./comando

# 5. El comportamiento difiere en el caso edge?
# Ej: epoll con EPOLLHUP — macOS kqueue maneja EV_EOF diferente
# Buscar en: docs/blockers.md, src/aine-shim/macos/[syscall].c
```

---

## Casos conocidos de comportamiento diferente

| Syscall | Linux | macOS (AINE) | Documentado en |
|---|---|---|---|
| `epoll EPOLLHUP` | automático al cerrar fd | EV_EOF en kqueue, timing diferente | src/aine-shim/macos/epoll.c |
| `futex timeout` | CLOCK_REALTIME o MONOTONIC | solo MONOTONIC en pthread_cond | src/aine-shim/macos/futex.c |
| `/proc/self/maps` | actualiza en tiempo real | generado en snapshot al `open()` | src/aine-shim/macos/proc.c |
| `fork()` | funciona | no recomendado en macOS — usar posix_spawn | docs/ARCHITECTURE.md |
| page size | 4KB | 16KB Apple Silicon — bloqueante B1 | docs/blockers.md#B1 |
