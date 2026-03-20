// aine-dalvik/dex.h — DEX file format structures (minimal subset)
// Reference: https://source.android.com/docs/core/runtime/dex-format
#pragma once
#include <stdint.h>
#include <stddef.h>

// DEX magic: "dex\n035\0" or "dex\n039\0"
#define DEX_MAGIC     "dex\n"
#define DEX_ENDIAN_CONSTANT 0x12345678u

typedef struct {
    uint8_t  magic[8];
    uint32_t checksum;
    uint8_t  signature[20];
    uint32_t file_size;
    uint32_t header_size;       // always 112
    uint32_t endian_tag;
    uint32_t link_size;
    uint32_t link_off;
    uint32_t map_off;
    uint32_t string_ids_size;
    uint32_t string_ids_off;
    uint32_t type_ids_size;
    uint32_t type_ids_off;
    uint32_t proto_ids_size;
    uint32_t proto_ids_off;
    uint32_t field_ids_size;
    uint32_t field_ids_off;
    uint32_t method_ids_size;
    uint32_t method_ids_off;
    uint32_t class_defs_size;
    uint32_t class_defs_off;
    uint32_t data_size;
    uint32_t data_off;
} DexHeader;

typedef struct {
    uint16_t class_idx;
    uint16_t type_idx;
    uint32_t name_idx;
} DexFieldId;

typedef struct {
    uint16_t class_idx;
    uint16_t proto_idx;
    uint32_t name_idx;
} DexMethodId;

typedef struct {
    uint32_t class_idx;
    uint32_t access_flags;
    uint32_t superclass_idx;
    uint32_t interfaces_off;
    uint32_t source_file_idx;
    uint32_t annotations_off;
    uint32_t class_data_off;
    uint32_t static_values_off;
} DexClassDef;

typedef struct {
    uint16_t registers_size;
    uint16_t ins_size;
    uint16_t outs_size;
    uint16_t tries_size;
    uint32_t debug_info_off;
    uint32_t insns_size;    // count of 16-bit code units
    // followed by insns_size * uint16_t insns[]
} DexCodeItem;

// Decoded view of an encoded method from class_data
typedef struct {
    uint32_t method_idx;
    uint32_t access_flags;
    uint32_t code_off;
} DexEncodedMethod;

// Loaded DEX file
typedef struct {
    const uint8_t *data;
    size_t         size;
    const DexHeader *hdr;
} DexFile;

// Open a DEX file from a memory buffer (does not take ownership)
int  dex_open(DexFile *df, const uint8_t *data, size_t size);

// String accessors
uint32_t    dex_string_count(const DexFile *df);
const char *dex_string(const DexFile *df, uint32_t idx);     // returns UTF-8 string
const char *dex_type_name(const DexFile *df, uint32_t type_idx);  // type descriptor
const char *dex_field_name(const DexFile *df, uint32_t field_idx);
const char *dex_field_class(const DexFile *df, uint32_t field_idx);
const char *dex_method_name(const DexFile *df, uint32_t method_idx);
const char *dex_method_class(const DexFile *df, uint32_t method_idx);
const char *dex_method_proto(const DexFile *df, uint32_t method_idx); // proto shorty

// Class lookup
int dex_find_class(const DexFile *df, const char *descriptor); // returns class_def index
// Method lookup within a class_def
int dex_find_method(const DexFile *df, int class_def_idx, const char *name, const char *descriptor);

// Get code item for a method (returns NULL if abstract/native)
const DexCodeItem *dex_code_item(const DexFile *df, int class_def_idx, int method_local_idx);

// Return pointer to instruction array for a code item
const uint16_t *dex_insns(const DexCodeItem *ci);

// ULEB128 decode helper
uint32_t uleb128_decode(const uint8_t **ptr);
