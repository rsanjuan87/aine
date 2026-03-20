# Workflow de desarrollo

## Ciclo diario recomendado

```bash
# 1. Actualizar rama y submódulos
git pull origin develop
git submodule update --init

# 2. Comprobar si hay novedades en ATL (una vez por semana)
./scripts/sync-atl.sh --check

# 3. Trabajar en el componente del milestone actual
# (ver ROADMAP.md para el milestone activo)

# 4. Compilar y testear frecuentemente
./scripts/build.sh aine-shim          # Compilación rápida del componente
cmake --build build --target test      # Tests

# 5. Antes de commit
./scripts/build.sh                    # Build completo
git add -p                            # Staging interactivo
git commit -m "tipo(scope): descripción"
```

## Convención de commits

```
tipo(scope): descripción corta en presente

[cuerpo opcional explicando el por qué, no el qué]

[referencias: Fixes #N, Closes #N]
```

Tipos:
- `feat` — nueva funcionalidad
- `fix` — corrección de bug
- `shim` — implementación de syscall en aine-shim
- `sync(atl)` — cambio sincronizado desde ATL upstream
- `chore` — build, CI, dependencias
- `docs` — documentación
- `test` — tests

Ejemplos:
```
shim(epoll): implement epoll_wait over kqueue

Uses kevent() with proper timeout conversion.
EPOLLHUP behavior on kqueue EV_EOF differs from Linux —
see comment in epoll.c for the workaround.

feat(binder): replace eventfd with pipe+atomic

Linux eventfd not available on macOS XNU.
Replaces with pipe(2) pair + atomic counter.
Semantics preserved: read blocks until value>0, resets on read.

sync(atl): fix ART GC with 16KB page alignment

Cherry-picked from ATL a3f91bc.
Adapted: removed Linux-specific madvise flags,
using macOS madvise(MADV_FREE) equivalent.
```

## Debugging

### Habilitar logs del shim
```bash
AINE_LOG_LEVEL=debug ./scripts/run-app.sh app.apk 2>&1 | tee debug.log
```

### Inspeccionar llamadas interceptadas
```bash
# Ver qué está interceptando el shim en tiempo real
AINE_LOG_LEVEL=debug \
AINE_LOG_SHIM_CALLS=1 \
DYLD_INSERT_LIBRARIES=build/src/aine-shim/libaine-shim.dylib \
./build/dalvikvm -cp HelloWorld.dex HelloWorld
```

### LLDB para crashes
```bash
# Adjuntar LLDB a un proceso de app AINE
lldb -- build/dalvikvm -cp app.dex com.example.Main
(lldb) env DYLD_INSERT_LIBRARIES=build/src/aine-shim/libaine-shim.dylib
(lldb) run
```

### Inspeccionar syscalls que llegan al shim
```c
// En cualquier función del shim, añadir:
AINE_LOG_DEBUG("epoll_wait: epfd=%d maxevents=%d timeout=%d", epfd, maxevents, timeout);
```

## Herramientas de diagnóstico útiles

```bash
# Ver qué dylibs carga un proceso
DYLD_PRINT_LIBRARIES=1 ./build/dalvikvm -cp test.dex Main

# Ver si el shim se inyecta correctamente
DYLD_PRINT_APIS=1 \
DYLD_INSERT_LIBRARIES=build/src/aine-shim/libaine-shim.dylib \
./build/dalvikvm -cp test.dex Main 2>&1 | head -30

# Listar símbolos exportados por el shim
nm -gU build/src/aine-shim/libaine-shim.dylib | grep " T "

# Ver secciones de interpose
otool -s __DATA __interpose build/src/aine-shim/libaine-shim.dylib

# Trazar llamadas al sistema (equivalente a strace en macOS)
sudo dtruss -p <PID>
# o desde el inicio:
sudo dtruss ./build/dalvikvm -cp test.dex Main
```

## Gestión de milestones

Cada milestone tiene su propio documento en `docs/milestones/`.
El milestone activo es el más bajo que no tiene su "Definition of Done" completo.

Para marcar una tarea como completada:
1. Edita el `.md` del milestone y cambia `- [ ]` por `- [x]`
2. Añade una nota con la fecha y el commit que lo resolvió
3. Si es el último ítem del milestone, abre un issue de celebración en GitHub

## Trabajar con agentes AI

Ver `docs/ai-agents/AGENTS.md` para el flujo completo.

Regla de oro: **siempre compila y testea en el Mac real** antes de dar un trabajo como terminado. Los agentes generan código plausible pero no ejecutan en Apple Silicon — solo tú puedes verificar que funciona de verdad.
