# M2 — Binder IPC funcional sobre Mach

**Target:** Meses 3–5
**Criterio de éxito:** `service list` via `aine-binder` devuelve los servicios del `system_server`.
**Prerequisito:** M1 completado (ART arranca)

---

## Por qué Binder es el componente más crítico

Binder es el sistema nervioso de Android. Cada llamada entre procesos pasa por él:
- `getSystemService("audio")` → Binder → `AudioFlinger`
- `startActivity(intent)` → Binder → `ActivityManagerService`
- `getPackageInfo(pkg)` → Binder → `PackageManagerService`

Sin Binder, el framework Android no puede arrancar. Con Binder funcionando, el resto del sistema se construye sobre una base sólida.

## Arquitectura de aine-binder

```
Proceso App                aine-binder-daemon           system_server
    │                           │                            │
    │  open("/dev/binder")      │                            │
    │  [interceptado por shim]  │                            │
    │                           │                            │
    │  Mach message ──────────▶ │                            │
    │  { BC_TRANSACTION,        │  lookup destino            │
    │    target: "SvcManager",  │  en tabla                  │
    │    data: [intent] }       │ ──────────────────────────▶│
    │                           │                            │
    │ ◀────────────────────────────────────────────────────  │
    │  Mach message             │                            │
    │  { BR_REPLY, data: [...] }│                            │
```

## Sustitución de primitivas Linux en el Binder de ATL

ATL usa `eventfd` y `/dev/ashmem`. Hay que reemplazarlos:

```c
// aine-binder/compat-macos.h
#pragma once

// En ATL: int fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
// En AINE: un pipe donde write=notify, read=wait
typedef struct {
    int read_fd;
    int write_fd;
    _Atomic int counter;
} aine_eventfd_t;

aine_eventfd_t* aine_eventfd_create(void);
int  aine_eventfd_read(aine_eventfd_t *efd, uint64_t *value);
int  aine_eventfd_write(aine_eventfd_t *efd, uint64_t value);
void aine_eventfd_destroy(aine_eventfd_t *efd);

// En ATL: shm_fd = open("/dev/ashmem", O_RDWR) + ioctl(ASHMEM_SET_SIZE)
// En AINE: shm_open + ftruncate
int  aine_ashmem_create(const char *name, size_t size);
void aine_ashmem_destroy(int fd, const char *name);
```

## aine-binder-daemon

El proceso daemon que actúa como router central:

```cpp
// src/aine-binder/binder-daemon.cpp
// Arranca antes que cualquier app.
// Se registra en bootstrap server de Mach como "com.aine.binder"

#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <unordered_map>
#include <string>

class BinderDaemon {
    mach_port_t server_port_;
    // Tabla: nombre de servicio → mach_port_t del servidor
    std::unordered_map<std::string, mach_port_t> services_;

public:
    void run() {
        // Registrar en bootstrap
        bootstrap_register(bootstrap_port, "com.aine.binder", &server_port_);

        // Loop de mensajes
        while (true) {
            aine_binder_message_t msg;
            mach_msg(&msg.header, MACH_RCV_MSG, 0, sizeof(msg),
                     server_port_, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
            dispatch(msg);
        }
    }

    void dispatch(const aine_binder_message_t& msg) {
        switch (msg.command) {
            case BC_TRANSACTION:  handle_transaction(msg); break;
            case BC_REGISTER_SVC: handle_register(msg);   break;
            case BC_LOOKUP_SVC:   handle_lookup(msg);     break;
        }
    }
};
```

## Interceptor de /dev/binder en aine-shim

```c
// src/aine-shim/binder-dev.c
#include <mach/mach.h>
#include <servers/bootstrap.h>

// fd falso → Mach port al aine-binder-daemon
static mach_port_t binder_daemon_port = MACH_PORT_NULL;

static void ensure_binder_connected(void) {
    if (binder_daemon_port != MACH_PORT_NULL) return;
    kern_return_t kr = bootstrap_look_up(
        bootstrap_port, "com.aine.binder", &binder_daemon_port);
    if (kr != KERN_SUCCESS) {
        // aine-binder-daemon no está corriendo — intentar arrancarlo
        aine_start_binder_daemon();
        // retry
        bootstrap_look_up(bootstrap_port, "com.aine.binder", &binder_daemon_port);
    }
}

int aine_open(const char *path, int flags, ...) {
    if (path && strcmp(path, "/dev/binder") == 0) {
        ensure_binder_connected();
        // Devolver fd especial que aine_ioctl reconocerá
        return AINE_BINDER_FAKE_FD;
    }
    // ... resto de open
}

int aine_ioctl(int fd, unsigned long request, void *arg) {
    if (fd == AINE_BINDER_FAKE_FD) {
        return aine_binder_ioctl(request, arg);
    }
    return ioctl(fd, request, arg);
}
```

## service manager

El primer servicio que debe arrancar:

```bash
# Probar que service manager funciona:
DYLD_INSERT_LIBRARIES=build/src/aine-shim/libaine-shim.dylib \
./build/servicemanager &

# En otro terminal:
DYLD_INSERT_LIBRARIES=build/src/aine-shim/libaine-shim.dylib \
./build/service list
# Salida esperada: lista de servicios (aunque vacía al principio)
```

## Definition of Done — M2

- [ ] `aine-binder-daemon` arranca sin crash
- [ ] `servicemanager` se registra correctamente
- [ ] `service list` devuelve respuesta (aunque esté vacía)
- [ ] Test: client → daemon → server → reply round-trip en < 5ms
- [ ] No hay `eventfd` ni `/dev/ashmem` en el código Binder (sustituidos)
- [ ] `aine-binder-daemon` se lanza automáticamente si no está corriendo

## Siguiente: M3

Con Binder funcionando, M3 arranca el framework completo y ejecuta la primera app sin UI.
Ver: `docs/milestones/M3-first-app.md`
