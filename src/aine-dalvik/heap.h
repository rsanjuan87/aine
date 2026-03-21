// aine-dalvik/heap.h — Minimal object heap for aine-dalvik interpreter
// No GC: objects live until process exit (sufficient for M1 HelloWorld)
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef enum {
    OBJ_NULL         = 0,
    OBJ_STRING       = 1,
    OBJ_PRINTSTREAM  = 2,
    OBJ_STRINGBUILDER = 3,
    OBJ_USERCLASS    = 4,   // user-defined class from DEX
    OBJ_ARRAY        = 5,   // primitive / object array
} ObjType;

/* Instance field slot — stored inline in AineObj for user-defined classes */
typedef struct AineFieldSlot {
    char    name[64];    /* field name (e.g. "f$0", "mValue") */
    int     is_obj;      /* 1 = obj, 0 = primitive */
    union {
        int64_t         prim;
        struct AineObj *obj;
    };
} AineFieldSlot;

typedef struct AineObj {
    ObjType type;
    int     arr_len;         // OBJ_ARRAY: element count
    int64_t *arr_prim;       // OBJ_ARRAY: primitive element storage
    struct AineObj **arr_obj;// OBJ_ARRAY: object element storage (separate)
    union {
        char *str;               // OBJ_STRING: null-terminated UTF-8 (owned)
        struct {
            char   *buf;
            size_t  len;
            size_t  cap;
        } sb;                     // OBJ_STRINGBUILDER
        struct {
            int class_def_idx;   // OBJ_USERCLASS: index into DexFile class_defs
        } uc;
    };
    /* Instance fields — used by OBJ_USERCLASS and generic instances */
    AineFieldSlot *fields;
    int            n_fields;
    int            fields_cap;
    /* Runtime type info — set for OBJ_USERCLASS by new-instance */
    const char    *class_desc;   /* e.g. "Lcom/example/Foo;" (not owned) */
} AineObj;

// Allocate a new string object (copies s)
AineObj *heap_string(const char *s);
// Allocate a new empty StringBuilder
AineObj *heap_sb_new(void);
// Append a string to a StringBuilder (returns the same SB for chaining)
AineObj *heap_sb_append(AineObj *sb, const char *s);
// Materialise a StringBuilder to a String object
AineObj *heap_sb_tostring(AineObj *sb);
// Allocate a user-defined class instance
AineObj *heap_userclass(int class_def_idx);
// Allocate an array of given length (zero-initialised)
AineObj *heap_array_new(int len);

/* Instance field access (for iget/iput opcodes) */
void     heap_iput_prim(AineObj *obj, const char *name, int64_t value);
void     heap_iput_obj (AineObj *obj, const char *name, AineObj *value);
int64_t  heap_iget_prim(const AineObj *obj, const char *name);
AineObj *heap_iget_obj (const AineObj *obj, const char *name);

/* Static field store (sget/sput opcodes) — global per-process table */
void     heap_sput_prim(const char *cls, const char *field, int64_t value);
void     heap_sput_obj (const char *cls, const char *field, AineObj *value);
int64_t  heap_sget_prim(const char *cls, const char *field);
AineObj *heap_sget_obj (const char *cls, const char *field);
