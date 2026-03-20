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
} ObjType;

typedef struct AineObj {
    ObjType type;
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
