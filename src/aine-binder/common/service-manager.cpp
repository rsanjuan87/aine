// AINE: src/aine-binder/common/service-manager.cpp
// Cliente del Service Manager de Android (handle=0)
// Implementa addService / getService / listServices

#include "../include/aine-binder.h"
#include "parcel.h"
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SVC_MGR_INTERFACE "android.os.IServiceManager"

// Construir un BC_TRANSACTION para el service manager
static int build_svc_transaction(uint8_t *buf, uint32_t *size,
                                   uint32_t code,
                                   const void *payload, uint32_t payload_size) {
    uint32_t pos = 0;

    uint32_t cmd = BC_TRANSACTION;
    memcpy(buf + pos, &cmd, 4); pos += 4;

    struct binder_transaction_data txn;
    memset(&txn, 0, sizeof(txn));
    txn.target.handle = 0;             // service manager = handle 0
    txn.code          = code;
    txn.flags         = 0x10;          // TF_ACCEPT_FDS
    txn.data_size     = payload_size;
    txn.offsets_size  = 0;
    txn.cookie        = 0xA1E0A1E0;   // inline data signal
    memcpy(buf + pos, &txn, sizeof(txn)); pos += sizeof(txn);

    if (payload && payload_size > 0) {
        memcpy(buf + pos, payload, payload_size);
        pos += payload_size;
    }
    *size = pos;
    return 0;
}

// Llamada genérica al service manager via binder fd
static int svc_call(int binder_fd, uint32_t code,
                     const void *pay, uint32_t pay_size,
                     uint8_t *reply_payload, uint32_t *reply_payload_size) {
    static uint8_t wbuf[AINE_BINDER_MSG_MAX];
    static uint8_t rbuf[AINE_BINDER_MSG_MAX];

    uint32_t wsize;
    build_svc_transaction(wbuf, &wsize, code, pay, pay_size);

    struct binder_write_read bwr;
    memset(&bwr, 0, sizeof(bwr));
    bwr.write_buffer = (binder_uintptr_t)wbuf;
    bwr.write_size   = wsize;
    bwr.read_buffer  = (binder_uintptr_t)rbuf;
    bwr.read_size    = sizeof(rbuf);

    if (aine_binder_ioctl(binder_fd, BINDER_WRITE_READ, &bwr) < 0)
        return -1;

    if (bwr.read_consumed < 4) return -1;

    // Parsear BR_REPLY
    uint32_t pos = 0;
    uint32_t cmd;
    memcpy(&cmd, rbuf + pos, 4); pos += 4;
    if (cmd != BR_REPLY) return -1;

    if (pos + sizeof(struct binder_transaction_data) > bwr.read_consumed)
        return -1;

    struct binder_transaction_data txn;
    memcpy(&txn, rbuf + pos, sizeof(txn)); pos += sizeof(txn);

    if (txn.data_size > 0 && reply_payload && reply_payload_size) {
        uint32_t copy = txn.data_size < *reply_payload_size
                      ? txn.data_size : *reply_payload_size;
        memcpy(reply_payload, rbuf + pos, copy);
        *reply_payload_size = copy;
    } else if (reply_payload_size) {
        *reply_payload_size = 0;
    }
    return 0;
}

// =============================================================================
// API pública del service manager
// =============================================================================

extern "C" {

// Listar servicio en posición `index` (0-based). Escribe nombre en `out`.
// Devuelve 0 si hay servicio, -1 si no.
int aine_svc_list(int binder_fd, int index, char *out, size_t out_size) {
    Parcel req;
    parcel_init(&req);
    parcel_write_interface_token(&req, SVC_MGR_INTERFACE);
    parcel_write_int32(&req, (int32_t)index);

    uint8_t  rbuf[AINE_BINDER_MSG_MAX];
    uint32_t rsize = sizeof(rbuf);
    if (svc_call(binder_fd, SVC_MGR_LIST_SERVICES,
                  parcel_data(&req), (uint32_t)parcel_size(&req),
                  rbuf, &rsize) < 0) {
        parcel_free(&req);
        return -1;
    }
    parcel_free(&req);

    Parcel resp;
    parcel_init_from(&resp, rbuf, rsize);
    int32_t status;
    if (parcel_read_int32(&resp, &status) || status < 0) {
        parcel_free(&resp);
        return -1;
    }
    // If positive result precedes string (implementation quirk), read string
    char name[256];
    if (parcel_read_string16(&resp, name, sizeof(name)) == 0) {
        strncpy(out, name, out_size - 1);
        out[out_size - 1] = '\0';
        parcel_free(&resp);
        return 0;
    }
    parcel_free(&resp);
    return -1;
}

// Obtener handle de un servicio por nombre. Devuelve 0 si encontrado.
int aine_svc_get(int binder_fd, const char *name, uint32_t *handle_out) {
    Parcel req;
    parcel_init(&req);
    parcel_write_interface_token(&req, SVC_MGR_INTERFACE);
    parcel_write_string16(&req, name);

    uint8_t  rbuf[AINE_BINDER_MSG_MAX];
    uint32_t rsize = sizeof(rbuf);
    if (svc_call(binder_fd, SVC_MGR_GET_SERVICE,
                  parcel_data(&req), (uint32_t)parcel_size(&req),
                  rbuf, &rsize) < 0) {
        parcel_free(&req);
        return -1;
    }
    parcel_free(&req);

    // reply: flat_binder_object + status
    if (rsize >= sizeof(struct flat_binder_object)) {
        struct flat_binder_object fbo;
        memcpy(&fbo, rbuf, sizeof(fbo));
        if (fbo.type == BINDER_TYPE_HANDLE) {
            if (handle_out) *handle_out = fbo.handle;
            return 0;
        }
    }
    return -1;
}

// Registrar un servicio en el daemon
int aine_svc_add(int binder_fd, const char *name) {
    Parcel req;
    parcel_init(&req);
    parcel_write_interface_token(&req, SVC_MGR_INTERFACE);
    parcel_write_string16(&req, name);
    // En M2 no enviamos un Binder real, solo el nombre
    parcel_write_int32(&req, 0); // flags: ALLOW_ISOLATED=0

    uint8_t  rbuf[AINE_BINDER_MSG_MAX];
    uint32_t rsize = sizeof(rbuf);
    int r = svc_call(binder_fd, SVC_MGR_ADD_SERVICE,
                      parcel_data(&req), (uint32_t)parcel_size(&req),
                      rbuf, &rsize);
    parcel_free(&req);
    return r;
}

} // extern "C"
