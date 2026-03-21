// aine-dalvik/heap.c — Minimal object heap
#include "heap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

AineObj *heap_string(const char *s) {
    AineObj *o = calloc(1, sizeof(AineObj));
    o->type = OBJ_STRING;
    o->str  = s ? strdup(s) : strdup("");
    return o;
}

AineObj *heap_sb_new(void) {
    AineObj *o = calloc(1, sizeof(AineObj));
    o->type   = OBJ_STRINGBUILDER;
    o->sb.cap = 64;
    o->sb.buf = calloc(1, o->sb.cap);
    o->sb.len = 0;
    return o;
}

AineObj *heap_sb_append(AineObj *sb, const char *s) {
    if (!sb || sb->type != OBJ_STRINGBUILDER) return sb;
    if (!s) return sb;
    size_t slen = strlen(s);
    while (sb->sb.len + slen + 1 > sb->sb.cap) {
        sb->sb.cap *= 2;
        sb->sb.buf  = realloc(sb->sb.buf, sb->sb.cap);
    }
    memcpy(sb->sb.buf + sb->sb.len, s, slen + 1);
    sb->sb.len += slen;
    return sb;
}

AineObj *heap_sb_tostring(AineObj *sb) {
    if (!sb || sb->type != OBJ_STRINGBUILDER) return heap_string("");
    return heap_string(sb->sb.buf);
}

AineObj *heap_userclass(int class_def_idx) {
    AineObj *o = calloc(1, sizeof(AineObj));
    o->type = OBJ_USERCLASS;
    o->uc.class_def_idx = class_def_idx;
    return o;
}

AineObj *heap_array_new(int len) {
    if (len < 0) len = 0;
    AineObj *o   = calloc(1, sizeof(AineObj));
    o->type      = OBJ_ARRAY;
    o->arr_len   = len;
    o->arr_prim  = (int64_t *)calloc((size_t)(len > 0 ? len : 1), sizeof(int64_t));
    o->arr_obj   = (AineObj **)calloc((size_t)(len > 0 ? len : 1), sizeof(AineObj *));
    return o;
}

/* ── Instance fields ──────────────────────────────────────────────────── */

static AineFieldSlot *find_field(const AineObj *obj, const char *name) {
    for (int i = 0; i < obj->n_fields; i++) {
        if (strcmp(obj->fields[i].name, name) == 0)
            return &obj->fields[i];
    }
    return NULL;
}

static AineFieldSlot *alloc_field(AineObj *obj, const char *name) {
    if (obj->n_fields >= obj->fields_cap) {
        int nc = obj->fields_cap ? obj->fields_cap * 2 : 8;
        obj->fields = realloc(obj->fields, (size_t)nc * sizeof(AineFieldSlot));
        memset(obj->fields + obj->fields_cap, 0,
               (size_t)(nc - obj->fields_cap) * sizeof(AineFieldSlot));
        obj->fields_cap = nc;
    }
    AineFieldSlot *s = &obj->fields[obj->n_fields++];
    strncpy(s->name, name, sizeof(s->name) - 1);
    return s;
}

void heap_iput_prim(AineObj *obj, const char *name, int64_t value) {
    if (!obj || !name) return;
    AineFieldSlot *s = find_field(obj, name);
    if (!s) s = alloc_field(obj, name);
    s->is_obj = 0;
    s->prim   = value;
}

void heap_iput_obj(AineObj *obj, const char *name, AineObj *value) {
    if (!obj || !name) return;
    AineFieldSlot *s = find_field(obj, name);
    if (!s) s = alloc_field(obj, name);
    s->is_obj = 1;
    s->obj    = value;
}

int64_t heap_iget_prim(const AineObj *obj, const char *name) {
    if (!obj || !name) return 0;
    const AineFieldSlot *s = find_field(obj, name);
    return (s && !s->is_obj) ? s->prim : 0;
}

AineObj *heap_iget_obj(const AineObj *obj, const char *name) {
    if (!obj || !name) return NULL;
    const AineFieldSlot *s = find_field(obj, name);
    return (s && s->is_obj) ? s->obj : NULL;
}

/* ── Static fields — global table ────────────────────────────────────── */
#define MAX_STATIC 512

typedef struct {
    char    key[256];   /* "class/Name::fieldName" */
    int     is_obj;
    union { int64_t prim; AineObj *obj; };
} StaticSlot;

static StaticSlot g_static[MAX_STATIC];
static int        g_static_count = 0;

static StaticSlot *find_static(const char *cls, const char *field) {
    char key[256];
    snprintf(key, sizeof(key), "%s::%s", cls ? cls : "", field ? field : "");
    for (int i = 0; i < g_static_count; i++) {
        if (strcmp(g_static[i].key, key) == 0) return &g_static[i];
    }
    return NULL;
}

static StaticSlot *alloc_static(const char *cls, const char *field) {
    if (g_static_count >= MAX_STATIC) return NULL;
    StaticSlot *s = &g_static[g_static_count++];
    snprintf(s->key, sizeof(s->key), "%s::%s", cls ? cls : "", field ? field : "");
    return s;
}

void heap_sput_prim(const char *cls, const char *field, int64_t value) {
    StaticSlot *s = find_static(cls, field);
    if (!s) s = alloc_static(cls, field);
    if (!s) return;
    s->is_obj = 0; s->prim = value;
}

void heap_sput_obj(const char *cls, const char *field, AineObj *value) {
    StaticSlot *s = find_static(cls, field);
    if (!s) s = alloc_static(cls, field);
    if (!s) return;
    s->is_obj = 1; s->obj = value;
}

int64_t heap_sget_prim(const char *cls, const char *field) {
    const StaticSlot *s = find_static(cls, field);
    return (s && !s->is_obj) ? s->prim : 0;
}

AineObj *heap_sget_obj(const char *cls, const char *field) {
    const StaticSlot *s = find_static(cls, field);
    return (s && s->is_obj) ? s->obj : NULL;
}

