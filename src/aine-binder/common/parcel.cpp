// AINE: src/aine-binder/common/parcel.cpp
// Serialización Binder Parcel — formato compatible con Android
// Solo el subconjunto necesario para el Service Manager (M2)

#include "parcel.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define PAD4(n) (((n) + 3) & ~3)

// =============================================================================
// Write helpers
// =============================================================================

static int parcel_ensure(Parcel *p, size_t needed) {
    size_t want = p->pos + needed;
    if (want > p->capacity) {
        size_t newcap = p->capacity ? p->capacity * 2 : 256;
        while (newcap < want) newcap *= 2;
        uint8_t *buf = (uint8_t *)realloc(p->data, newcap);
        if (!buf) return -1;
        p->data     = buf;
        p->capacity = newcap;
    }
    return 0;
}

void parcel_init(Parcel *p) {
    memset(p, 0, sizeof(*p));
}

void parcel_init_from(Parcel *p, const void *data, size_t size) {
    memset(p, 0, sizeof(*p));
    if (size) {
        p->data = (uint8_t *)malloc(size);
        if (p->data) {
            memcpy(p->data, data, size);
            p->size     = size;
            p->capacity = size;
        }
    }
    p->owned = 1;
}

void parcel_free(Parcel *p) {
    if (p->owned && p->data) free(p->data);
    memset(p, 0, sizeof(*p));
}

void parcel_rewind(Parcel *p) {
    p->pos = 0;
}

size_t parcel_size(const Parcel *p) {
    return p->size;
}

const void *parcel_data(const Parcel *p) {
    return p->data;
}

// Escribir int32 (4 bytes alineados)
int parcel_write_int32(Parcel *p, int32_t v) {
    if (parcel_ensure(p, 4)) return -1;
    memcpy(p->data + p->pos, &v, 4);
    p->pos  += 4;
    if (p->pos > p->size) p->size = p->pos;
    return 0;
}

// Escribir string16 Android: int32 len + UTF-16LE + null terminator
int parcel_write_string16(Parcel *p, const char *utf8) {
    if (!utf8) {
        return parcel_write_int32(p, -1);
    }
    // Convertir ASCII/UTF-8 a UTF-16LE (solo Basic Multilingual Plane)
    size_t len = strlen(utf8);
    // tamaño: int32 length + chars*2 + null*2, padded to 4
    size_t byte_len  = len * 2 + 2;
    size_t padded    = PAD4(byte_len);
    if (parcel_write_int32(p, (int32_t)len)) return -1;
    if (parcel_ensure(p, padded)) return -1;
    uint8_t *dst = p->data + p->pos;
    memset(dst, 0, padded);
    for (size_t i = 0; i < len; i++) {
        dst[i * 2]     = (uint8_t)utf8[i];
        dst[i * 2 + 1] = 0;
    }
    // null terminator already set by memset
    p->pos += padded;
    if (p->pos > p->size) p->size = p->pos;
    return 0;
}

// Escribir un bloque de bytes raw (con int32 tamaño)
int parcel_write_bytes(Parcel *p, const void *data, size_t size) {
    if (parcel_write_int32(p, (int32_t)size)) return -1;
    if (size == 0) return 0;
    size_t padded = PAD4(size);
    if (parcel_ensure(p, padded)) return -1;
    memcpy(p->data + p->pos, data, size);
    if (padded > size)
        memset(p->data + p->pos + size, 0, padded - size);
    p->pos += padded;
    if (p->pos > p->size) p->size = p->pos;
    return 0;
}

// Escribir la cabecera de interface ("android.os.IServiceManager")
int parcel_write_interface_token(Parcel *p, const char *token) {
    // strict mode policy (int32) + interface descriptor (string16)
    if (parcel_write_int32(p, 0x100)) return -1;  // STRICT_MODE_PENALTY_GATHER
    if (parcel_write_int32(p, 0)) return -1;       // work source UID
    return parcel_write_string16(p, token);
}

// =============================================================================
// Read helpers
// =============================================================================

int parcel_read_int32(Parcel *p, int32_t *v) {
    if (p->pos + 4 > p->size) return -1;
    memcpy(v, p->data + p->pos, 4);
    p->pos += 4;
    return 0;
}

// Lee string16 (int32 len + UTF-16LE) y lo convierte a UTF-8 (ASCII subset)
int parcel_read_string16(Parcel *p, char *out, size_t out_size) {
    int32_t len;
    if (parcel_read_int32(p, &len)) return -1;
    if (len < 0) { if (out_size) out[0] = '\0'; return 0; }
    size_t byte_len = (size_t)len * 2 + 2;
    size_t padded   = PAD4(byte_len);
    if (p->pos + padded > p->size) return -1;
    // Convert UTF-16LE (BMP only) to ASCII/UTF-8
    size_t copy = (size_t)len < out_size - 1 ? (size_t)len : out_size - 1;
    for (size_t i = 0; i < copy; i++) {
        out[i] = (char)p->data[p->pos + i * 2];
    }
    if (out_size) out[copy] = '\0';
    p->pos += padded;
    return 0;
}

int parcel_read_bytes(Parcel *p, void *out, size_t *size) {
    int32_t len;
    if (parcel_read_int32(p, &len)) return -1;
    if (len < 0) { *size = 0; return 0; }
    size_t padded = PAD4((size_t)len);
    if (p->pos + padded > p->size) return -1;
    size_t copy = (size_t)len < *size ? (size_t)len : *size;
    memcpy(out, p->data + p->pos, copy);
    *size   = copy;
    p->pos += padded;
    return 0;
}

int parcel_skip_interface_token(Parcel *p) {
    int32_t dummy;
    if (parcel_read_int32(p, &dummy)) return -1; // strict mode
    if (parcel_read_int32(p, &dummy)) return -1; // work source
    char token[128];
    return parcel_read_string16(p, token, sizeof(token));
}
