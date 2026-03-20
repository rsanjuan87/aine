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
