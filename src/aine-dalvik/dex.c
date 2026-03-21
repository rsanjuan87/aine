// aine-dalvik/dex.c — DEX file parser
// Reference: https://source.android.com/docs/core/runtime/dex-format
#include "dex.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ── ULEB128 ────────────────────────────────────────────────────────────────
uint32_t uleb128_decode(const uint8_t **ptr) {
    uint32_t result = 0;
    int shift = 0;
    const uint8_t *p = *ptr;
    do {
        result |= (uint32_t)(*p & 0x7f) << shift;
        shift += 7;
    } while (*p++ & 0x80);
    *ptr = p;
    return result;
}

// ── SLEB128 ────────────────────────────────────────────────────────────────
int32_t sleb128_decode(const uint8_t **ptr) {
    int32_t result = 0;
    int shift = 0;
    const uint8_t *p = *ptr;
    uint8_t b;
    do {
        b = *p++;
        result |= (int32_t)(b & 0x7f) << shift;
        shift += 7;
    } while (b & 0x80);
    if (shift < 32 && (b & 0x40)) result |= ~0 << shift; /* sign extend */
    *ptr = p;
    return result;
}

// ── Exception handler lookup ───────────────────────────────────────────────
int32_t dex_find_catch_handler(const DexFile *df, const DexCodeItem *ci,
                                uint32_t throw_pc, const char *exc_type)
{
    if (!ci || ci->tries_size == 0) return -1;

    /* Tries array follows insns[], aligned to 4 bytes */
    const uint16_t *insns = dex_insns(ci);
    const uint8_t *after_insns = (const uint8_t *)insns + ci->insns_size * 2;
    if ((uintptr_t)after_insns & 2) after_insns += 2;  /* 4-byte align */

    /* encoded_catch_handler_list starts right after the tries array */
    const uint8_t *handlers_base = after_insns + (size_t)ci->tries_size * 8;

    for (uint16_t t = 0; t < ci->tries_size; t++) {
        const uint8_t *tp = after_insns + (size_t)t * 8;
        uint32_t start = (uint32_t)tp[0] | ((uint32_t)tp[1]<<8)
                       | ((uint32_t)tp[2]<<16) | ((uint32_t)tp[3]<<24);
        uint16_t count = (uint16_t)tp[4] | ((uint16_t)tp[5]<<8);
        uint16_t hoff  = (uint16_t)tp[6] | ((uint16_t)tp[7]<<8);

        if (throw_pc < start || throw_pc >= start + count) continue;

        const uint8_t *p = handlers_base + hoff;
        int32_t nhdr = sleb128_decode(&p);  /* negative → has catch-all */
        int abs_nhdr = nhdr < 0 ? -nhdr : nhdr;

        for (int h = 0; h < abs_nhdr; h++) {
            uint32_t type_idx = uleb128_decode(&p);
            uint32_t addr     = uleb128_decode(&p);
            const char *tn = dex_type_name(df, type_idx);
            if (tn) {
                if (exc_type && strcmp(tn, exc_type) == 0) return (int32_t)addr;
                /* Accept any Exception/Throwable/RuntimeException handler */
                if (strcmp(tn, "Ljava/lang/Throwable;")        == 0) return (int32_t)addr;
                if (strcmp(tn, "Ljava/lang/Exception;")        == 0) return (int32_t)addr;
                if (strcmp(tn, "Ljava/lang/RuntimeException;") == 0) return (int32_t)addr;
            }
        }
        if (nhdr <= 0) {
            uint32_t catch_all = uleb128_decode(&p);
            return (int32_t)catch_all;
        }
    }
    return -1;
}

// ── Open ───────────────────────────────────────────────────────────────────
int dex_open(DexFile *df, const uint8_t *data, size_t size) {
    if (size < sizeof(DexHeader)) return -1;
    const DexHeader *h = (const DexHeader *)data;
    if (memcmp(h->magic, "dex\n", 4) != 0) return -1;
    if (h->endian_tag != DEX_ENDIAN_CONSTANT) return -1;   // big-endian not supported
    df->data = data;
    df->size = size;
    df->hdr  = h;
    return 0;
}

// ── String table ──────────────────────────────────────────────────────────
uint32_t dex_string_count(const DexFile *df) {
    return df->hdr->string_ids_size;
}

const char *dex_string(const DexFile *df, uint32_t idx) {
    if (idx >= df->hdr->string_ids_size) return NULL;
    const uint32_t *string_ids = (const uint32_t *)(df->data + df->hdr->string_ids_off);
    uint32_t off = string_ids[idx];
    const uint8_t *p = df->data + off;
    uleb128_decode(&p);          // skip UTF-16 length
    return (const char *)p;      // null-terminated MUTF-8
}

// ── Type names ────────────────────────────────────────────────────────────
const char *dex_type_name(const DexFile *df, uint32_t type_idx) {
    if (type_idx >= df->hdr->type_ids_size) return NULL;
    const uint32_t *type_ids = (const uint32_t *)(df->data + df->hdr->type_ids_off);
    return dex_string(df, type_ids[type_idx]);
}

// ── Field accessors ───────────────────────────────────────────────────────
static const DexFieldId *field_id(const DexFile *df, uint32_t idx) {
    return (const DexFieldId *)(df->data + df->hdr->field_ids_off) + idx;
}

const char *dex_field_name(const DexFile *df, uint32_t fidx) {
    return dex_string(df, field_id(df, fidx)->name_idx);
}

const char *dex_field_class(const DexFile *df, uint32_t fidx) {
    return dex_type_name(df, field_id(df, fidx)->class_idx);
}

// ── Method accessors ──────────────────────────────────────────────────────
static const DexMethodId *method_id(const DexFile *df, uint32_t idx) {
    return (const DexMethodId *)(df->data + df->hdr->method_ids_off) + idx;
}

const char *dex_method_name(const DexFile *df, uint32_t midx) {
    return dex_string(df, method_id(df, midx)->name_idx);
}

const char *dex_method_class(const DexFile *df, uint32_t midx) {
    return dex_type_name(df, method_id(df, midx)->class_idx);
}

// ── Class lookup ──────────────────────────────────────────────────────────
int dex_find_class(const DexFile *df, const char *descriptor) {
    const DexClassDef *defs = (const DexClassDef *)(df->data + df->hdr->class_defs_off);
    for (uint32_t i = 0; i < df->hdr->class_defs_size; i++) {
        const char *name = dex_type_name(df, defs[i].class_idx);
        if (name && strcmp(name, descriptor) == 0) return (int)i;
    }
    return -1;
}

const char *dex_class_super(const DexFile *df, int class_def_idx) {
    const DexClassDef *defs = (const DexClassDef *)(df->data + df->hdr->class_defs_off);
    if (class_def_idx < 0 || (uint32_t)class_def_idx >= df->hdr->class_defs_size) return NULL;
    uint32_t super_idx = defs[class_def_idx].superclass_idx;
    if (super_idx == 0xFFFFFFFFu) return NULL;  /* java/lang/Object */
    return dex_type_name(df, super_idx);
}

// ── Class data: iterate encoded methods ───────────────────────────────────
// Fills 'out_methods' (max_methods entries), returns count of direct+virtual methods
static int decode_class_methods(const DexFile *df, const DexClassDef *cdef,
                                DexEncodedMethod *out, int max) {
    if (cdef->class_data_off == 0) return 0;
    const uint8_t *p = df->data + cdef->class_data_off;

    uint32_t static_fields_size  = uleb128_decode(&p);
    uint32_t instance_fields_size = uleb128_decode(&p);
    uint32_t direct_methods_size  = uleb128_decode(&p);
    uint32_t virtual_methods_size = uleb128_decode(&p);

    // Skip fields
    for (uint32_t i = 0; i < static_fields_size + instance_fields_size; i++) {
        uleb128_decode(&p); // field_idx_diff
        uleb128_decode(&p); // access_flags
    }

    // Decode methods — each section (direct / virtual) restarts idx from 0
    uint32_t method_idx = 0;
    int count = 0;

    for (uint32_t i = 0; i < direct_methods_size && count < max; i++) {
        uint32_t diff  = uleb128_decode(&p);
        uint32_t flags = uleb128_decode(&p);
        uint32_t code  = uleb128_decode(&p);
        method_idx += diff;
        out[count].method_idx   = method_idx;
        out[count].access_flags = flags;
        out[count].code_off     = code;
        count++;
    }

    method_idx = 0;  // virtual methods section uses its own differential base
    for (uint32_t i = 0; i < virtual_methods_size && count < max; i++) {
        uint32_t diff  = uleb128_decode(&p);
        uint32_t flags = uleb128_decode(&p);
        uint32_t code  = uleb128_decode(&p);
        method_idx += diff;
        out[count].method_idx   = method_idx;
        out[count].access_flags = flags;
        out[count].code_off     = code;
        count++;
    }

    return count;
}

int dex_find_method(const DexFile *df, int class_def_idx,
                    const char *name, const char *descriptor) {
    const DexClassDef *cdef =
        (const DexClassDef *)(df->data + df->hdr->class_defs_off) + class_def_idx;

    DexEncodedMethod methods[256];
    int n = decode_class_methods(df, cdef, methods, 256);
    for (int i = 0; i < n; i++) {
        if (strcmp(dex_method_name(df, methods[i].method_idx), name) == 0)
            return i;   // local method index
    }
    (void)descriptor;
    return -1;
}

const DexCodeItem *dex_code_item(const DexFile *df, int class_def_idx, int method_local_idx) {
    const DexClassDef *cdef =
        (const DexClassDef *)(df->data + df->hdr->class_defs_off) + class_def_idx;

    DexEncodedMethod methods[256];
    int n = decode_class_methods(df, cdef, methods, 256);
    if (method_local_idx < 0 || method_local_idx >= n) return NULL;
    uint32_t off = methods[method_local_idx].code_off;
    if (off == 0) return NULL;
    return (const DexCodeItem *)(df->data + off);
}

const uint16_t *dex_insns(const DexCodeItem *ci) {
    return (const uint16_t *)((const uint8_t *)ci + sizeof(DexCodeItem));
}
