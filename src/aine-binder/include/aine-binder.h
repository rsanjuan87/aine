// AINE: src/aine-binder/include/aine-binder.h
// Protocolo Binder para macOS via Mach IPC
// Compatible con el protocolo Linux BC_*/BR_* de AOSP

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Visibilidad pública
// =============================================================================

#ifndef AINE_PUBLIC
#define AINE_PUBLIC __attribute__((visibility("default")))
#endif

// =============================================================================
// Tipos básicos de Binder (ARM64)
// =============================================================================

typedef uint64_t binder_size_t;
typedef uint64_t binder_uintptr_t;

// =============================================================================
// Comandos cliente → driver (BC_*)
// =============================================================================

#define BC_TRANSACTION       0x40406300
#define BC_REPLY             0x40406301
#define BC_ACQUIRE_RESULT    0x40046302
#define BC_FREE_BUFFER       0x40086303
#define BC_INCREFS           0x40046304
#define BC_ACQUIRE           0x40046305
#define BC_RELEASE           0x40046306
#define BC_DECREFS           0x40046307
#define BC_REGISTER_LOOPER   0x00006308
#define BC_ENTER_LOOPER      0x00006309
#define BC_EXIT_LOOPER       0x0000630a

// =============================================================================
// Respuestas driver → cliente (BR_*)
// =============================================================================

#define BR_ERROR             0x80046300
#define BR_OK                0x00006301
#define BR_TRANSACTION       0x80886302
#define BR_REPLY             0x80886303
#define BR_ACQUIRE_RESULT    0x80046304
#define BR_DEAD_REPLY        0x00006305
#define BR_TRANSACTION_COMPLETE 0x00006306
#define BR_INCREFS           0x80086307
#define BR_ACQUIRE           0x80086308
#define BR_RELEASE           0x80086309
#define BR_DECREFS           0x8008630a
#define BR_NOOP              0x0000630c
#define BR_SPAWN_LOOPER      0x0000630d
#define BR_DEAD_BINDER       0x80086315
#define BR_FAILED_REPLY      0x00006317

// =============================================================================
// Códigos de transacción del Service Manager (handle 0)
// =============================================================================

#define SVC_MGR_GET_SERVICE     1
#define SVC_MGR_CHECK_SERVICE   2
#define SVC_MGR_ADD_SERVICE     3
#define SVC_MGR_LIST_SERVICES   4
#define SVC_MGR_GET_SERVICE_DEBUG_INFO 5

// =============================================================================
// ioctl BINDER_WRITE_READ
// =============================================================================

#define BINDER_WRITE_READ       0xc0306201
#define BINDER_SET_MAX_THREADS  0x40046205
#define BINDER_SET_CONTEXT_MGR  0x40046207
#define BINDER_THREAD_EXIT      0x40046208
#define BINDER_VERSION          0xc0046209

#define BINDER_CURRENT_PROTOCOL_VERSION 8

struct binder_write_read {
    binder_size_t    write_size;
    binder_size_t    write_consumed;
    binder_uintptr_t write_buffer;
    binder_size_t    read_size;
    binder_size_t    read_consumed;
    binder_uintptr_t read_buffer;
};

// =============================================================================
// binder_transaction_data (56 bytes en ARM64)
// =============================================================================

struct flat_binder_object {
    uint32_t type;       // BINDER_TYPE_BINDER, BINDER_TYPE_HANDLE, etc.
    uint32_t flags;
    union {
        binder_uintptr_t binder;
        uint32_t         handle;
    };
    binder_uintptr_t cookie;
};

#define BINDER_TYPE_BINDER  0x73622a85 // 'sB*\x85'
#define BINDER_TYPE_HANDLE  0x73682a85 // 'sH*\x85'

struct binder_transaction_data {
    union {
        uint32_t         handle;
        binder_uintptr_t ptr;
    } target;
    binder_uintptr_t cookie;
    uint32_t         code;
    uint32_t         flags;
    int32_t          sender_pid;
    int32_t          sender_euid;
    binder_size_t    data_size;
    binder_size_t    offsets_size;
    union {
        struct {
            binder_uintptr_t buffer;
            binder_uintptr_t offsets;
        } ptr;
        uint8_t buf[8];
    } data;
};

// =============================================================================
// Nombre del daemon en Mach bootstrap
// =============================================================================

#define AINE_BINDER_SERVICE_NAME  "com.aine.binder"
#define AINE_BINDER_MSG_MAX       (64 * 1024)
#define AINE_BINDER_FAKE_FD_BASE  0x42494e44  // 'BIND'

// =============================================================================
// API pública de aine-binder (llamada desde aine-shim/binder-dev.c)
// =============================================================================

AINE_PUBLIC int  aine_binder_open(void);
AINE_PUBLIC int  aine_binder_ioctl(int fake_fd, unsigned long request, void *arg);
AINE_PUBLIC int  aine_binder_close(int fake_fd);

// =============================================================================
// Daemon: arrancar / verificar si corre
// =============================================================================

AINE_PUBLIC int  aine_binder_daemon_start(void);
AINE_PUBLIC int  aine_binder_daemon_run(void);     // bloquea hasta señal
AINE_PUBLIC int  aine_binder_daemon_is_running(void);

// =============================================================================
// Service Manager: listado / consulta / registro de servicios
// =============================================================================

AINE_PUBLIC int  aine_svc_list(int binder_fd, int index, char *out, size_t out_size);
AINE_PUBLIC int  aine_svc_get(int binder_fd, const char *name, uint32_t *handle_out);
AINE_PUBLIC int  aine_svc_add(int binder_fd, const char *name);

#ifdef __cplusplus
}
#endif
