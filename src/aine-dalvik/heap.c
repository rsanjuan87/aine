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

/* ── ArrayList ────────────────────────────────────────────────────────── */

AineObj *heap_arraylist_new(void) {
    AineObj *o  = calloc(1, sizeof(AineObj));
    o->type     = OBJ_ARRAYLIST;
    o->arr_cap  = 8;
    o->arr_len  = 0;
    o->arr_obj  = calloc(8, sizeof(AineObj *));
    return o;
}

static void arraylist_grow(AineObj *list) {
    int nc = list->arr_cap ? list->arr_cap * 2 : 8;
    list->arr_obj = realloc(list->arr_obj, (size_t)nc * sizeof(AineObj *));
    memset(list->arr_obj + list->arr_cap, 0,
           (size_t)(nc - list->arr_cap) * sizeof(AineObj *));
    list->arr_cap = nc;
}

void heap_arraylist_add(AineObj *list, AineObj *item) {
    if (!list || list->type != OBJ_ARRAYLIST) return;
    if (list->arr_len >= list->arr_cap) arraylist_grow(list);
    list->arr_obj[list->arr_len++] = item;
}

AineObj *heap_arraylist_get(const AineObj *list, int idx) {
    if (!list || list->type != OBJ_ARRAYLIST) return NULL;
    if (idx < 0 || idx >= list->arr_len) return NULL;
    return list->arr_obj[idx];
}

void heap_arraylist_set(AineObj *list, int idx, AineObj *item) {
    if (!list || list->type != OBJ_ARRAYLIST) return;
    if (idx < 0 || idx >= list->arr_len) return;
    list->arr_obj[idx] = item;
}

int heap_arraylist_remove_idx(AineObj *list, int idx) {
    if (!list || list->type != OBJ_ARRAYLIST) return 0;
    if (idx < 0 || idx >= list->arr_len) return 0;
    memmove(list->arr_obj + idx, list->arr_obj + idx + 1,
            (size_t)(list->arr_len - idx - 1) * sizeof(AineObj *));
    list->arr_len--;
    return 1;
}

int heap_arraylist_size(const AineObj *list) {
    if (!list || list->type != OBJ_ARRAYLIST) return 0;
    return list->arr_len;
}

void heap_arraylist_clear(AineObj *list) {
    if (!list || list->type != OBJ_ARRAYLIST) return;
    memset(list->arr_obj, 0, (size_t)list->arr_len * sizeof(AineObj *));
    list->arr_len = 0;
}

/* ── HashMap ─────────────────────────────────────────────────────────── */
/* Layout: arr_obj[2*i] = key (OBJ_STRING), arr_obj[2*i+1] = value      */

AineObj *heap_hashmap_new(void) {
    AineObj *o  = calloc(1, sizeof(AineObj));
    o->type     = OBJ_HASHMAP;
    o->arr_cap  = 8;   /* pair capacity */
    o->arr_len  = 0;
    o->arr_obj  = calloc(8 * 2, sizeof(AineObj *));
    return o;
}

static void hashmap_grow(AineObj *map) {
    int nc = map->arr_cap ? map->arr_cap * 2 : 8;
    map->arr_obj = realloc(map->arr_obj, (size_t)(nc * 2) * sizeof(AineObj *));
    memset(map->arr_obj + map->arr_cap * 2, 0,
           (size_t)((nc - map->arr_cap) * 2) * sizeof(AineObj *));
    map->arr_cap = nc;
}

void heap_hashmap_put(AineObj *map, const char *key, AineObj *val) {
    if (!map || map->type != OBJ_HASHMAP || !key) return;
    for (int i = 0; i < map->arr_len; i++) {
        AineObj *k = map->arr_obj[2 * i];
        if (k && k->str && strcmp(k->str, key) == 0) {
            map->arr_obj[2 * i + 1] = val;
            return;
        }
    }
    if (map->arr_len >= map->arr_cap) hashmap_grow(map);
    map->arr_obj[2 * map->arr_len]     = heap_string(key);
    map->arr_obj[2 * map->arr_len + 1] = val;
    map->arr_len++;
}

AineObj *heap_hashmap_get(const AineObj *map, const char *key) {
    if (!map || map->type != OBJ_HASHMAP || !key) return NULL;
    for (int i = 0; i < map->arr_len; i++) {
        AineObj *k = map->arr_obj[2 * i];
        if (k && k->str && strcmp(k->str, key) == 0)
            return map->arr_obj[2 * i + 1];
    }
    return NULL;
}

int heap_hashmap_contains_key(const AineObj *map, const char *key) {
    if (!map || map->type != OBJ_HASHMAP || !key) return 0;
    for (int i = 0; i < map->arr_len; i++) {
        AineObj *k = map->arr_obj[2 * i];
        if (k && k->str && strcmp(k->str, key) == 0) return 1;
    }
    return 0;
}

void heap_hashmap_remove(AineObj *map, const char *key) {
    if (!map || map->type != OBJ_HASHMAP || !key) return;
    for (int i = 0; i < map->arr_len; i++) {
        AineObj *k = map->arr_obj[2 * i];
        if (k && k->str && strcmp(k->str, key) == 0) {
            memmove(map->arr_obj + 2 * i, map->arr_obj + 2 * (i + 1),
                    (size_t)(map->arr_len - i - 1) * 2 * sizeof(AineObj *));
            map->arr_len--;
            return;
        }
    }
}

int heap_hashmap_size(const AineObj *map) {
    if (!map || map->type != OBJ_HASHMAP) return 0;
    return map->arr_len;
}

AineObj *heap_hashmap_keyset(const AineObj *map) {
    AineObj *list = heap_arraylist_new();
    if (!map || map->type != OBJ_HASHMAP) return list;
    for (int i = 0; i < map->arr_len; i++)
        heap_arraylist_add(list, map->arr_obj[2 * i]);
    return list;
}

