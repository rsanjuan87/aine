// aine-dalvik/jni.c — Native method bridges for core Android/Java APIs
#include "jni.h"
#include "handler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <float.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef __APPLE__
#include "canvas.h"
#endif

// ── Safe primitive arg reader ───────────────────────────────────────────
// Primitive args from the interpreter are boxed as heap_string objects.
// Real array elements come as OBJ_ARRAY with arr_prim set.
// This helper handles both cases safely.
static inline int64_t arg_prim(const AineObj *a) {
    if (!a) return 0;
    if (a->arr_prim) return a->arr_prim[0];  /* actual int64 array */
    if (a->str)      return atoll(a->str);    /* boxed primitive from interp */
    return 0;
}

/* Extract IEEE 754 float stored as raw bits in a primitive arg */
static inline float arg_float(const AineObj *a) {
    union { int64_t i; float f; } v;
    v.i = arg_prim(a);
    return v.f;
}

/* ── Content view globals for onDraw dispatch ───────────────────────── */
static AineObj *g_content_view = NULL;
static int      g_view_dirty   = 0;

AineObj *jni_get_content_view(void) { return g_content_view; }
int      jni_pop_invalidated(void)  { int v = g_view_dirty; g_view_dirty = 0; return v; }

/* ── XML layout support ─────────────────────────────────────────────── */
static AineResMap   *g_res_map    = NULL;
static AineViewNode *g_layout_root = NULL;

int jni_set_res_dir(const char *dir) {
    if (g_res_map) { aine_res_free(g_res_map); g_res_map = NULL; }
    g_res_map = aine_res_load(dir);
    return g_res_map ? 1 : 0;
}

AineViewNode *jni_get_layout_root(void) { return g_layout_root; }

/* ── View-stub registry: AineObj* stubs returned by Activity.findViewById() */
#define VIEW_STUB_MAX 64
static struct { int res_id; AineObj *stub; } g_view_stubs[VIEW_STUB_MAX];
static int g_view_stub_count = 0;

static void register_view_stub(int res_id, AineObj *stub) {
    if (!stub || !res_id) return;
    for (int i = 0; i < g_view_stub_count; i++) {
        if (g_view_stubs[i].res_id == res_id) { g_view_stubs[i].stub = stub; return; }
    }
    if (g_view_stub_count < VIEW_STUB_MAX) {
        g_view_stubs[g_view_stub_count].res_id = res_id;
        g_view_stubs[g_view_stub_count].stub   = stub;
        g_view_stub_count++;
    }
}

AineObj *jni_get_view_stub(int res_id) {
    for (int i = 0; i < g_view_stub_count; i++)
        if (g_view_stubs[i].res_id == res_id) return g_view_stubs[i].stub;
    return NULL;
}

// ── Static field singletons ──────────────────────────────────────────────
static AineObj g_system_out = { .type = OBJ_PRINTSTREAM };
static AineObj g_system_err = { .type = OBJ_PRINTSTREAM };

// ── jni_sget_prim — known static primitive constants ─────────────────────
int64_t jni_sget_prim(const char *class_desc, const char *field_name) {
    if (!class_desc || !field_name) return 0;
    if (strcmp(class_desc, "Ljava/lang/Integer;") == 0) {
        if (strcmp(field_name, "MAX_VALUE") == 0) return 2147483647LL;
        if (strcmp(field_name, "MIN_VALUE") == 0) return -2147483648LL;
        if (strcmp(field_name, "SIZE") == 0) return 32;
        if (strcmp(field_name, "BYTES") == 0) return 4;
    }
    if (strcmp(class_desc, "Ljava/lang/Long;") == 0) {
        if (strcmp(field_name, "MAX_VALUE") == 0) return INT64_MAX;
        if (strcmp(field_name, "MIN_VALUE") == 0) return INT64_MIN;
    }
    if (strcmp(class_desc, "Ljava/lang/Short;") == 0) {
        if (strcmp(field_name, "MAX_VALUE") == 0) return 32767;
        if (strcmp(field_name, "MIN_VALUE") == 0) return -32768;
    }
    if (strcmp(class_desc, "Ljava/lang/Byte;") == 0) {
        if (strcmp(field_name, "MAX_VALUE") == 0) return 127;
        if (strcmp(field_name, "MIN_VALUE") == 0) return -128;
    }
    if (strcmp(class_desc, "Ljava/lang/Character;") == 0) {
        if (strcmp(field_name, "MAX_VALUE") == 0) return 65535;
    }
    if (strcmp(class_desc, "Ljava/lang/Boolean;") == 0) {
        if (strcmp(field_name, "TRUE") == 0)  return 1;
        if (strcmp(field_name, "FALSE") == 0) return 0;
    }
    if (strcmp(class_desc, "Ljava/lang/Float;") == 0) {
        if (strcmp(field_name, "MAX_VALUE") == 0) return (int64_t)(double)FLT_MAX;
    }
    if (strcmp(class_desc, "Ljava/lang/Double;") == 0) {
        if (strcmp(field_name, "MAX_VALUE") == 0) return (int64_t)DBL_MAX;
    }
    return 0;
}

// ── String helpers ────────────────────────────────────────────────────────

/* Split str by delim → OBJ_ARRAY of OBJ_STRING */
static AineObj *string_split(const char *str, const char *delim) {
    if (!str) str = "";
    if (!delim || !*delim) {
        AineObj *a = heap_array_new(1);
        a->arr_obj[0] = heap_string(str);
        return a;
    }
    /* Count parts */
    size_t dlen = strlen(delim), count = 1;
    const char *p = str;
    while ((p = strstr(p, delim))) { count++; p += dlen; }
    AineObj *arr = heap_array_new((int)count);
    const char *s = str; int i = 0;
    while ((p = strstr(s, delim))) {
        char *part = strndup(s, (size_t)(p - s));
        arr->arr_obj[i++] = heap_string(part); free(part);
        s = p + dlen;
    }
    arr->arr_obj[i] = heap_string(s);
    return arr;
}

/* Replace first/all occurrences of from → to */
static char *str_replace_all(const char *str, const char *from, const char *to) {
    if (!str) return strdup("");
    if (!from || !*from) return strdup(str);
    if (!to) to = "";
    size_t flen = strlen(from), tlen = strlen(to);
    size_t cap = strlen(str) * 2 + 64;
    char *out = malloc(cap); int pos = 0;
    const char *s = str;
    while (*s) {
        const char *found = strstr(s, from);
        if (!found) {
            size_t rest = strlen(s);
            while (pos + rest + 1 > cap) { cap *= 2; out = realloc(out, cap); }
            memcpy(out + pos, s, rest); pos += (int)rest; break;
        }
        size_t before = (size_t)(found - s);
        while (pos + before + tlen + 1 > cap) { cap *= 2; out = realloc(out, cap); }
        memcpy(out + pos, s, before); pos += (int)before;
        memcpy(out + pos, to, tlen); pos += (int)tlen;
        s = found + flen;
    }
    out[pos] = 0;
    return out;
}

// ── SharedPreferences ─────────────────────────────────────────────────────
#define PREFS_DIR "/tmp/aine-prefs"

/* Load a prefs file → OBJ_HASHMAP */
static AineObj *prefs_load(const char *name) {
    AineObj *map = heap_hashmap_new();
    if (!name) return map;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.prefs", PREFS_DIR, name);
    FILE *f = fopen(path, "r");
    if (!f) return map;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        heap_hashmap_put(map, line, heap_string(eq + 1));
    }
    fclose(f);
    return map;
}

/* Save a prefs OBJ_HASHMAP to file */
static void prefs_save(const char *name, const AineObj *map) {
    if (!name || !map) return;
#ifdef __APPLE__
    mkdir(PREFS_DIR, 0755);
#else
    mkdir(PREFS_DIR, 0755);
#endif
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.prefs", PREFS_DIR, name);
    FILE *f = fopen(path, "w");
    if (!f) return;
    /* arr_obj has pairs: [key_str, val_str, key_str, val_str, ...] */
    for (int i = 0; i + 1 < map->arr_len; i += 2) {
        AineObj *k = map->arr_obj[i];
        AineObj *v = map->arr_obj[i + 1];
        if (k && k->str && v && v->str &&
            strcmp(k->str, "__prefs_name__") != 0)
            fprintf(f, "%s=%s\n", k->str, v->str);
    }
    fclose(f);
}

AineObj *jni_sget_object(const char *class_desc, const char *field_name) {
    if (strcmp(class_desc, "Ljava/lang/System;") == 0) {
        if (strcmp(field_name, "out") == 0) return &g_system_out;
        if (strcmp(field_name, "err") == 0) return &g_system_err;
    }
    /* Paint.Style enum constants — return a stub; setStyle() is a no-op renderer side */
    if (strstr(class_desc, "Paint$Style") || strstr(class_desc, "Paint.Style")) {
        AineObj *s = calloc(1, sizeof(AineObj));
        s->type = OBJ_USERCLASS; s->class_desc = class_desc;
        heap_iput_prim(s, "value",
            strcmp(field_name, "FILL")         == 0 ? 0LL :
            strcmp(field_name, "STROKE")        == 0 ? 1LL : 2LL);
        return s;
    }
    /* Suppress noisy warnings for common static-object fields we don't need */
    if (strstr(class_desc, "android/") || strstr(class_desc, "java/") ||
        strstr(class_desc, "kotlin/") || strstr(class_desc, "androidx/") ||
        strstr(class_desc, "com/google/") || strstr(class_desc, "com/facebook/")) {
        AineObj *stub = calloc(1, sizeof(AineObj));
        stub->type = OBJ_USERCLASS; stub->class_desc = class_desc;
        return stub;
    }
    fprintf(stderr, "[aine-dalvik] sget-object: unknown field %s->%s\n",
            class_desc, field_name);
    return NULL;
}

// ── System.getProperty ───────────────────────────────────────────────────
static AineObj *system_get_property(const char *key) {
    if (!key) return heap_string("null");
    if (strcmp(key, "java.version")           == 0) return heap_string("0");
    if (strcmp(key, "os.arch")                == 0) return heap_string("aarch64");
    if (strcmp(key, "os.name")                == 0) return heap_string("Linux");
    if (strcmp(key, "java.vendor")            == 0) return heap_string("The Android Project");
    if (strcmp(key, "java.home")              == 0) return heap_string("/system");
    if (strcmp(key, "java.class.path")        == 0) return heap_string(".");
    if (strcmp(key, "user.home")              == 0) return heap_string("/data");
    if (strcmp(key, "user.name")              == 0) return heap_string("android");
    if (strcmp(key, "file.separator")         == 0) return heap_string("/");
    if (strcmp(key, "path.separator")         == 0) return heap_string(":");
    if (strcmp(key, "line.separator")         == 0) return heap_string("\n");
    return heap_string("null");
}

// ── android.util.Log ────────────────────────────────────────────────────
static const char *log_level_char(int prio) {
    if (prio == 2) return "V";
    if (prio == 3) return "D";
    if (prio == 4) return "I";
    if (prio == 5) return "W";
    if (prio == 6) return "E";
    if (prio == 7) return "A";
    return "?";
}

static JniResult dispatch_log(const char *method_name,
                              AineObj **args, int nargs) {
    JniResult res = { .is_void = 0, .obj = NULL, .prim = 0 };
    int prio = 4; // INFO default
    if      (strcmp(method_name, "v") == 0) prio = 2;
    else if (strcmp(method_name, "d") == 0) prio = 3;
    else if (strcmp(method_name, "i") == 0) prio = 4;
    else if (strcmp(method_name, "w") == 0) prio = 5;
    else if (strcmp(method_name, "e") == 0) prio = 6;
    const char *tag = (nargs >= 1 && args[0] && args[0]->str) ? args[0]->str : "?";
    const char *msg = (nargs >= 2 && args[1] && args[1]->str) ? args[1]->str : "(null)";
    int n = fprintf(stderr, "[%s/%s] %s\n", log_level_char(prio), tag, msg);
    res.prim = n;
    return res;
}

// ── Method dispatch ──────────────────────────────────────────────────────
JniResult jni_dispatch(const char *class_desc,
                       const char *method_name,
                       AineObj    *this_obj,
                       AineObj   **args,
                       int         nargs,
                       int         is_static) {
    JniResult res = { .is_void = 1, .obj = NULL, .prim = 0 };


    // ── PrintStream.println / print ──────────────────────────────────────
    if (strcmp(class_desc, "Ljava/io/PrintStream;") == 0) {
        if (strcmp(method_name, "println") == 0) {
            if (nargs >= 1 && args[0]) {
                if (args[0]->type == OBJ_STRING || args[0]->type == OBJ_STRINGBUILDER)
                    printf("%s\n", args[0]->str ? args[0]->str : "");
                else
                    printf("(obj)\n");
            } else printf("\n");
            fflush(stdout); res.is_void = 1; return res;
        }
        if (strcmp(method_name, "print") == 0) {
            if (nargs >= 1 && args[0])
                printf("%s", args[0]->str ? args[0]->str : "");
            fflush(stdout); res.is_void = 1; return res;
        }
        if (strcmp(method_name, "printf") == 0 || strcmp(method_name, "format") == 0) {
            if (nargs >= 1 && args[0] && args[0]->str) printf("%s", args[0]->str);
            fflush(stdout); res.is_void = 0; res.obj = this_obj; return res;
        }
        if (strcmp(method_name, "flush") == 0) { fflush(stdout); return res; }
    }

    // ── System.getProperty / System.exit / System.currentTimeMillis ─────
    if (strcmp(class_desc, "Ljava/lang/System;") == 0) {
        if (strcmp(method_name, "getProperty") == 0 && nargs >= 1 && is_static) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str : "";
            res.is_void = 0; res.obj = system_get_property(key); return res;
        }
        if (strcmp(method_name, "exit") == 0) {
            int code = 0;
            if (nargs >= 1 && args[0]) code = (int)arg_prim(args[0]);
            exit(code);
        }
        if (strcmp(method_name, "currentTimeMillis") == 0) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            res.is_void = 0;
            res.prim = (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
            return res;
        }
        if (strcmp(method_name, "nanoTime") == 0) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            res.is_void = 0;
            res.prim = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
            return res;
        }
        if (strcmp(method_name, "gc") == 0) { return res; }
        if (strcmp(method_name, "arraycopy") == 0 && nargs >= 5) {
            /* arraycopy(src, srcPos, dst, dstPos, len) */
            AineObj *src = args[0];
            int srcPos   = (args[1] && args[1]->str) ? atoi(args[1]->str) : 0;
            AineObj *dst = args[2];
            int dstPos   = (args[3] && args[3]->str) ? atoi(args[3]->str) : 0;
            int len      = (args[4] && args[4]->str) ? atoi(args[4]->str) : 0;
            if (src && dst && src->type == OBJ_ARRAY && dst->type == OBJ_ARRAY) {
                for (int ci = 0; ci < len; ci++) {
                    int si = srcPos + ci, di = dstPos + ci;
                    if (si < src->arr_len && di < dst->arr_len) {
                        dst->arr_prim[di] = src->arr_prim[si];
                        dst->arr_obj[di]  = src->arr_obj[si];
                    }
                }
            }
            return res;  /* void */
        }
    }

    // ── android.util.Log ──────────────────────────────────────────────────────
    if (strcmp(class_desc, "Landroid/util/Log;") == 0) {
        if (strcmp(method_name, "v") == 0 || strcmp(method_name, "d") == 0 ||
            strcmp(method_name, "i") == 0 || strcmp(method_name, "w") == 0 ||
            strcmp(method_name, "e") == 0) {
            return dispatch_log(method_name, args, nargs);
        }
        // Log.wtf / Log.println stubs
        res.is_void = 0; res.prim = 0; return res;
    }

    // ── Integer / Number ─────────────────────────────────────────────────
    if (strcmp(class_desc, "Ljava/lang/Integer;") == 0 ||
        strcmp(class_desc, "Ljava/lang/Number;") == 0) {
        if (strcmp(method_name, "parseInt") == 0 && nargs >= 1) {
            const char *s = (args[0] && args[0]->str) ? args[0]->str : "0";
            res.is_void = 0; res.prim = (int32_t)atoi(s); return res;
        }
        if (strcmp(method_name, "valueOf") == 0 && nargs >= 1) {
            // valueOf(int) or valueOf(String)
            if (args[0] && args[0]->type == OBJ_STRING) {
                res.is_void = 0; res.obj = args[0]; return res;
            }
            res.is_void = 0; res.obj = args[0]; return res;
        }
        if (strcmp(method_name, "toString") == 0) {
            if (nargs >= 1 && args[0]) {
                res.is_void = 0; res.obj = args[0]; return res;
            }
            if (this_obj) { res.is_void = 0; res.obj = this_obj; return res; }
        }
        if (strcmp(method_name, "intValue") == 0 && this_obj && this_obj->str) {
            res.is_void = 0; res.prim = atoi(this_obj->str); return res;
        }
        if (strcmp(method_name, "compareTo") == 0 && nargs >= 1) {
            res.is_void = 0; res.prim = 0; return res;
        }
    }

    // ── Long ──────────────────────────────────────────────────────────────
    if (strcmp(class_desc, "Ljava/lang/Long;") == 0) {
        if (strcmp(method_name, "parseLong") == 0 && nargs >= 1) {
            const char *s = (args[0] && args[0]->str) ? args[0]->str : "0";
            res.is_void = 0; res.prim = (int64_t)atoll(s); return res;
        }
        if (strcmp(method_name, "valueOf") == 0 || strcmp(method_name, "toString") == 0) {
            res.is_void = 0; res.obj = nargs >= 1 ? args[0] : NULL; return res;
        }
    }

    // ── String ───────────────────────────────────────────────────────────
    if (strcmp(class_desc, "Ljava/lang/String;") == 0) {
        const char *str_val = (this_obj && this_obj->str) ? this_obj->str : "";
        if (strcmp(method_name, "length") == 0) {
            res.is_void = 0; res.prim = (int32_t)strlen(str_val); return res;
        }
        if (strcmp(method_name, "isEmpty") == 0) {
            res.is_void = 0; res.prim = (strlen(str_val) == 0) ? 1 : 0; return res;
        }
        if (strcmp(method_name, "equals") == 0 && nargs >= 1) {
            const char *other = (args[0] && args[0]->str) ? args[0]->str : "";
            res.is_void = 0; res.prim = strcmp(str_val, other) == 0 ? 1 : 0; return res;
        }
        if (strcmp(method_name, "equalsIgnoreCase") == 0 && nargs >= 1) {
            const char *other = (args[0] && args[0]->str) ? args[0]->str : "";
            res.is_void = 0; res.prim = strcasecmp(str_val, other) == 0 ? 1 : 0; return res;
        }
        if (strcmp(method_name, "contains") == 0 && nargs >= 1) {
            const char *needle = (args[0] && args[0]->str) ? args[0]->str : "";
            res.is_void = 0; res.prim = strstr(str_val, needle) ? 1 : 0; return res;
        }
        if (strcmp(method_name, "startsWith") == 0 && nargs >= 1) {
            const char *prefix = (args[0] && args[0]->str) ? args[0]->str : "";
            res.is_void = 0; res.prim = strncmp(str_val, prefix, strlen(prefix)) == 0 ? 1 : 0; return res;
        }
        if (strcmp(method_name, "endsWith") == 0 && nargs >= 1) {
            const char *suf = (args[0] && args[0]->str) ? args[0]->str : "";
            size_t slen = strlen(str_val), suflen = strlen(suf);
            res.is_void = 0; res.prim = (slen >= suflen && strcmp(str_val + slen - suflen, suf) == 0) ? 1 : 0; return res;
        }
        if (strcmp(method_name, "indexOf") == 0 && nargs >= 1) {
            const char *needle = (args[0] && args[0]->str) ? args[0]->str : "";
            const char *found = strstr(str_val, needle);
            res.is_void = 0; res.prim = found ? (int32_t)(found - str_val) : -1; return res;
        }
        if (strcmp(method_name, "trim") == 0) {
            size_t start = 0, len = strlen(str_val);
            while (start < len && (str_val[start] == ' ' || str_val[start] == '\t' || str_val[start] == '\n')) start++;
            while (len > start && (str_val[len-1] == ' ' || str_val[len-1] == '\t' || str_val[len-1] == '\n')) len--;
            char *t = strndup(str_val + start, len - start);
            res.is_void = 0; res.obj = heap_string(t); free(t); return res;
        }
        if (strcmp(method_name, "toLowerCase") == 0) {
            char *t = strdup(str_val);
            for (char *p = t; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
            res.is_void = 0; res.obj = heap_string(t); free(t); return res;
        }
        if (strcmp(method_name, "toUpperCase") == 0) {
            char *t = strdup(str_val);
            for (char *p = t; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
            res.is_void = 0; res.obj = heap_string(t); free(t); return res;
        }
        if (strcmp(method_name, "toString") == 0 || strcmp(method_name, "intern") == 0) {
            res.is_void = 0; res.obj = this_obj; return res;
        }
        if (strcmp(method_name, "valueOf") == 0 && nargs >= 1) {
            if (args[0] && args[0]->type == OBJ_STRING) { res.is_void = 0; res.obj = args[0]; return res; }
            char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)arg_prim(args[0]));
            res.is_void = 0; res.obj = heap_string(buf); return res;
        }
        if (strcmp(method_name, "format") == 0 && nargs >= 1 && args[0] && args[0]->str) {
            const char *fmt = args[0]->str;
            /* args[1] may be an Object[] varargs array or individual args */
            AineObj **fargs = NULL;
            int nfargs = 0;
            if (nargs >= 2 && args[1] && args[1]->type == OBJ_ARRAY) {
                fargs  = args[1]->arr_obj;
                nfargs = args[1]->arr_len;
            } else {
                fargs  = args + 1;
                nfargs = nargs - 1;
            }
            char out[2048] = {0};
            int  out_pos   = 0;
            int  arg_idx   = 0;
            for (const char *p = fmt; *p && out_pos < 2040; p++) {
                if (*p != '%') { out[out_pos++] = *p; continue; }
                p++;
                if (!*p) break;
                if (*p == '%') { out[out_pos++] = '%'; continue; }
                char spec[64]; int sp = 0;
                spec[sp++] = '%';
                while ((*p == '-' || *p == '+' || *p == '0' || *p == ' ' || *p == '#') && sp < 58)
                    spec[sp++] = *p++;
                while (isdigit((unsigned char)*p) && sp < 58) spec[sp++] = *p++;
                if (*p == '.') { spec[sp++] = *p++;
                    while (isdigit((unsigned char)*p) && sp < 58) spec[sp++] = *p++; }
                if (*p == 'l') { p++; if (*p == 'l') p++; }
                char sc = *p;
                char tmp[512] = {0};
                if (arg_idx < nfargs) {
                    AineObj *fa = fargs[arg_idx];
                    const char *sv = (fa && fa->str) ? fa->str :
                                     (fa && fa->class_desc) ? fa->class_desc : "null";
                    long long iv = fa ? atoll(sv) : 0;
                    double    dv = fa ? atof(sv)  : 0.0;
                    char      fmt2[64];
                    switch (sc) {
                        case 'd': case 'i':
                            memcpy(fmt2, spec, sp); memcpy(fmt2+sp, "lld", 4);
                            snprintf(tmp, sizeof(tmp), fmt2, iv); break;
                        case 'u':
                            memcpy(fmt2, spec, sp); memcpy(fmt2+sp, "llu", 4);
                            snprintf(tmp, sizeof(tmp), fmt2, (unsigned long long)iv); break;
                        case 'f': case 'e': case 'g':
                            spec[sp++] = sc; spec[sp] = 0;
                            snprintf(tmp, sizeof(tmp), spec, dv); break;
                        case 's':
                            spec[sp++] = 's'; spec[sp] = 0;
                            snprintf(tmp, sizeof(tmp), spec, sv); break;
                        case 'b':
                            snprintf(tmp, sizeof(tmp), "%s", iv ? "true" : "false"); break;
                        case 'c': tmp[0] = (char)iv; tmp[1] = 0; break;
                        case 'x': case 'X':
                            memcpy(fmt2, spec, sp); fmt2[sp] = sc; fmt2[sp+1] = 0;
                            snprintf(tmp, sizeof(tmp), fmt2, (unsigned long long)iv); break;
                        default: tmp[0] = sc; tmp[1] = 0; break;
                    }
                    arg_idx++;
                }
                int tl = (int)strlen(tmp);
                if (out_pos + tl < 2040) { memcpy(out + out_pos, tmp, tl); out_pos += tl; }
            }
            out[out_pos] = 0;
            res.is_void = 0; res.obj = heap_string(out); return res;
        }
        if (strcmp(method_name, "concat") == 0 && nargs >= 1) {
            const char *other = (args[0] && args[0]->str) ? args[0]->str : "";
            size_t len1 = strlen(str_val), len2 = strlen(other);
            char *t = malloc(len1 + len2 + 1);
            memcpy(t, str_val, len1); memcpy(t + len1, other, len2 + 1);
            res.is_void = 0; res.obj = heap_string(t); free(t); return res;
        }
        if (strcmp(method_name, "charAt") == 0 && nargs >= 1) {
            int idx_ch = (int)arg_prim(args[0]);
            res.is_void = 0; res.prim = (idx_ch >= 0 && idx_ch < (int)strlen(str_val)) ? (uint8_t)str_val[idx_ch] : 0;
            return res;
        }
        if (strcmp(method_name, "substring") == 0 && nargs >= 1) {
            int from = (int)arg_prim(args[0]);
            const char *sub = (from >= 0 && from < (int)strlen(str_val)) ? str_val + from : "";
            res.is_void = 0; res.obj = heap_string(sub); return res;
        }
        if (strcmp(method_name, "toCharArray") == 0) {
            res.is_void = 0; res.obj = heap_array_new((int)strlen(str_val)); return res;
        }
        if (strcmp(method_name, "split") == 0 && nargs >= 1) {
            const char *delim = (args[0] && args[0]->str) ? args[0]->str : "";
            /* Strip regex anchors for simple delimiters */
            char simple_delim[64]; strncpy(simple_delim, delim, 63); simple_delim[63]=0;
            if (strlen(simple_delim) > 1 &&
                (simple_delim[0] == '\\' || simple_delim[0] == '^'))
                memmove(simple_delim, simple_delim + 1, strlen(simple_delim));
            res.is_void = 0; res.obj = string_split(str_val, simple_delim); return res;
        }
        if (strcmp(method_name, "replace") == 0 && nargs >= 2) {
            const char *from = (args[0] && args[0]->str) ? args[0]->str : "";
            const char *to   = (args[1] && args[1]->str) ? args[1]->str : "";
            char *r = str_replace_all(str_val, from, to);
            res.is_void = 0; res.obj = heap_string(r); free(r); return res;
        }
        if (strcmp(method_name, "replaceAll") == 0 && nargs >= 2) {
            const char *pat = (args[0] && args[0]->str) ? args[0]->str : "";
            const char *rep = (args[1] && args[1]->str) ? args[1]->str : "";
            /* Treat pattern as literal for common cases */
            char *r = str_replace_all(str_val, pat, rep);
            res.is_void = 0; res.obj = heap_string(r); free(r); return res;
        }
        if (strcmp(method_name, "matches") == 0) {
            res.is_void = 0; res.prim = 0; return res;  /* stub */
        }
        if (strcmp(method_name, "getBytes") == 0) {
            int len = (int)strlen(str_val);
            AineObj *arr = heap_array_new(len);
            for (int i = 0; i < len; i++) arr->arr_prim[i] = (uint8_t)str_val[i];
            res.is_void = 0; res.obj = arr; return res;
        }
        if (strcmp(method_name, "compareTo") == 0 && nargs >= 1) {
            const char *other = (args[0] && args[0]->str) ? args[0]->str : "";
            res.is_void = 0; res.prim = strcmp(str_val, other); return res;
        }
        if (strcmp(method_name, "<init>") == 0) { res.is_void = 1; return res; }
    }

    // ── StringBuilder ────────────────────────────────────────────────────
    if (strcmp(class_desc, "Ljava/lang/StringBuilder;") == 0) {
        if (strcmp(method_name, "<init>") == 0) { res.is_void = 1; return res; }
        if (strcmp(method_name, "append") == 0 && nargs >= 1) {
            const char *s = NULL;
            char buf[64] = {0};
            if (args[0]) {
                if (args[0]->type == OBJ_STRING || args[0]->type == OBJ_STRINGBUILDER)
                    s = args[0]->str;
                else { snprintf(buf, sizeof(buf), "%lld", (long long)arg_prim(args[0])); s = buf; }
            } else s = "null";
            if (this_obj) heap_sb_append(this_obj, s);
            res.is_void = 0; res.obj = this_obj; return res;
        }
        if (strcmp(method_name, "toString") == 0) {
            res.is_void = 0; res.obj = heap_sb_tostring(this_obj); return res;
        }
        if (strcmp(method_name, "length") == 0) {
            res.is_void = 0; res.prim = this_obj ? (int32_t)this_obj->sb.len : 0; return res;
        }
        if (strcmp(method_name, "delete") == 0 || strcmp(method_name, "insert") == 0) {
            res.is_void = 0; res.obj = this_obj; return res;
        }
    }

    // ── Object base methods ──────────────────────────────────────────────
    // NOTE: <init> is NOT intercepted here — specialized class sections below
    // (ArrayList, HashMap, Thread, File, Exception, etc.) have their own <init>.
    if (strcmp(method_name, "toString") == 0) {
        res.is_void = 0;
        if (this_obj && this_obj->type == OBJ_STRING) res.obj = this_obj;
        else if (this_obj && (this_obj->type == OBJ_STRINGBUILDER))
            res.obj = heap_sb_tostring(this_obj);
        else res.obj = heap_string("[object]");
        return res;
    }
    if (strcmp(method_name, "hashCode") == 0) { res.is_void = 0; res.prim = 0; return res; }
    if (strcmp(method_name, "equals") == 0) { res.is_void = 0; res.prim = 0; return res; }
    if (strcmp(method_name, "getClass") == 0) {
        res.is_void = 0; res.obj = heap_string(class_desc); return res;
    }
    if (strcmp(method_name, "getName") == 0) {
        res.is_void = 0; res.obj = heap_string(class_desc); return res;
    }
    // ── java.lang.Math ──────────────────────────────────────────────────────
    if (strcmp(class_desc, "Ljava/lang/Math;") == 0) {
        double a = (nargs >= 1 && args[0] && args[0]->str) ? atof(args[0]->str) : 0.0;
        double b = (nargs >= 2 && args[1] && args[1]->str) ? atof(args[1]->str) : 0.0;
        res.is_void = 0;
        if      (!strcmp(method_name, "abs"))   res.prim = (int64_t)fabs(a);
        else if (!strcmp(method_name, "max"))   res.prim = ((int64_t)a > (int64_t)b) ? (int64_t)a : (int64_t)b;
        else if (!strcmp(method_name, "min"))   res.prim = ((int64_t)a < (int64_t)b) ? (int64_t)a : (int64_t)b;
        else if (!strcmp(method_name, "sqrt"))  res.prim = (int64_t)sqrt(a);
        else if (!strcmp(method_name, "pow"))   res.prim = (int64_t)pow(a, b);
        else if (!strcmp(method_name, "floor")) res.prim = (int64_t)floor(a);
        else if (!strcmp(method_name, "ceil"))  res.prim = (int64_t)ceil(a);
        else if (!strcmp(method_name, "round")) res.prim = (int64_t)round(a);
        else if (!strcmp(method_name, "log"))   res.prim = (int64_t)(log(a) * 1000000);
        else if (!strcmp(method_name, "random")) res.prim = 0;
        else                                    res.prim = (int64_t)a;
        return res;
    }

    // ── java.util.ArrayList / LinkedList ─────────────────────────────────
    if (strstr(class_desc, "java/util/ArrayList") ||
        strstr(class_desc, "java/util/LinkedList") ||
        strstr(class_desc, "java/util/Vector")) {
        if (!strcmp(method_name, "<init>")) {
            /* ArrayList(Collection) constructor — populate from source */
            if (nargs >= 1 && args[0] && this_obj &&
                args[0]->type == OBJ_ARRAYLIST) {
                for (int i = 0; i < args[0]->arr_len; i++)
                    heap_arraylist_add(this_obj, args[0]->arr_obj[i]);
            }
            return res;
        }
        if (!this_obj || this_obj->type != OBJ_ARRAYLIST) return res;
        if (!strcmp(method_name, "add")) {
            /* add(E) — single arg; add(int, E) — index insert (stub: append) */
            AineObj *item = (nargs >= 2) ? args[1] : (nargs >= 1 ? args[0] : NULL);
            heap_arraylist_add(this_obj, item);
            res.is_void = 0; res.prim = 1; return res;
        }
        if (!strcmp(method_name, "get") && nargs >= 1) {
            int idx = (args[0] && args[0]->str) ? atoi(args[0]->str) : 0;
            res.is_void = 0; res.obj = heap_arraylist_get(this_obj, idx); return res;
        }
        if (!strcmp(method_name, "set") && nargs >= 2) {
            int idx = (args[0] && args[0]->str) ? atoi(args[0]->str) : 0;
            AineObj *old = heap_arraylist_get(this_obj, idx);
            heap_arraylist_set(this_obj, idx, args[1]);
            res.is_void = 0; res.obj = old; return res;
        }
        if (!strcmp(method_name, "size")) {
            res.is_void = 0; res.prim = heap_arraylist_size(this_obj); return res;
        }
        if (!strcmp(method_name, "isEmpty")) {
            res.is_void = 0; res.prim = heap_arraylist_size(this_obj) == 0 ? 1 : 0; return res;
        }
        if (!strcmp(method_name, "remove") && nargs >= 1) {
            int idx = (args[0] && args[0]->str) ? atoi(args[0]->str) : 0;
            res.is_void = 0; res.prim = heap_arraylist_remove_idx(this_obj, idx); return res;
        }
        if (!strcmp(method_name, "clear")) {
            heap_arraylist_clear(this_obj); return res;
        }
        if (!strcmp(method_name, "contains") && nargs >= 1) {
            const char *needle = (args[0] && args[0]->str) ? args[0]->str : "";
            int found = 0;
            for (int i = 0; i < this_obj->arr_len && !found; i++) {
                AineObj *it = this_obj->arr_obj[i];
                if (it && it->str && strcmp(it->str, needle) == 0) found = 1;
            }
            res.is_void = 0; res.prim = found; return res;
        }
        if (!strcmp(method_name, "toArray")) {
            AineObj *arr = heap_array_new(this_obj->arr_len);
            for (int i = 0; i < this_obj->arr_len; i++) arr->arr_obj[i] = this_obj->arr_obj[i];
            res.is_void = 0; res.obj = arr; return res;
        }
        if (!strcmp(method_name, "iterator") || !strcmp(method_name, "listIterator")) {
            res.is_void = 0; res.obj = heap_iterator_new(this_obj); return res;
        }
        if (!strcmp(method_name, "subList") && nargs >= 2) {
            int from_idx = (args[0] && args[0]->str) ? atoi(args[0]->str) : 0;
            int to_idx   = (args[1] && args[1]->str) ? atoi(args[1]->str) : 0;
            AineObj *sub = heap_arraylist_new();
            for (int i = from_idx; i < to_idx && i < this_obj->arr_len; i++)
                heap_arraylist_add(sub, this_obj->arr_obj[i]);
            res.is_void = 0; res.obj = sub; return res;
        }
        return res;
    }

    // ── java.util.HashMap / LinkedHashMap / TreeMap ──────────────────────
    if (strstr(class_desc, "java/util/HashMap") ||
        strstr(class_desc, "java/util/LinkedHashMap") ||
        strstr(class_desc, "java/util/TreeMap") ||
        strstr(class_desc, "java/util/Hashtable")) {
        if (!strcmp(method_name, "<init>")) { return res; }
        if (!this_obj || this_obj->type != OBJ_HASHMAP) return res;
        if (!strcmp(method_name, "put") && nargs >= 2) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str :
                              (args[0] && args[0]->class_desc) ? args[0]->class_desc : "";
            heap_hashmap_put(this_obj, key, args[1]);
            res.is_void = 0; res.obj = NULL; return res;
        }
        if (!strcmp(method_name, "get") && nargs >= 1) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str :
                              (args[0] && args[0]->class_desc) ? args[0]->class_desc : "";
            res.is_void = 0; res.obj = heap_hashmap_get(this_obj, key); return res;
        }
        if (!strcmp(method_name, "containsKey") && nargs >= 1) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str :
                              (args[0] && args[0]->class_desc) ? args[0]->class_desc : "";
            res.is_void = 0; res.prim = heap_hashmap_contains_key(this_obj, key); return res;
        }
        if (!strcmp(method_name, "containsValue") && nargs >= 1) {
            res.is_void = 0; res.prim = 0; return res; /* stub */
        }
        if (!strcmp(method_name, "remove") && nargs >= 1) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str : "";
            AineObj *old = heap_hashmap_get(this_obj, key);
            heap_hashmap_remove(this_obj, key);
            res.is_void = 0; res.obj = old; return res;
        }
        if (!strcmp(method_name, "size")) {
            res.is_void = 0; res.prim = heap_hashmap_size(this_obj); return res;
        }
        if (!strcmp(method_name, "isEmpty")) {
            res.is_void = 0; res.prim = heap_hashmap_size(this_obj) == 0 ? 1 : 0; return res;
        }
        if (!strcmp(method_name, "clear")) {
            this_obj->arr_len = 0; return res;
        }
        if (!strcmp(method_name, "keySet") || !strcmp(method_name, "entrySet")) {
            res.is_void = 0; res.obj = heap_hashmap_keyset(this_obj); return res;
        }
        if (!strcmp(method_name, "values")) {
            AineObj *list = heap_arraylist_new();
            for (int i = 0; i < this_obj->arr_len; i++)
                heap_arraylist_add(list, this_obj->arr_obj[2 * i + 1]);
            res.is_void = 0; res.obj = list; return res;
        }
        if (!strcmp(method_name, "getOrDefault") && nargs >= 2) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str : "";
            AineObj *v = heap_hashmap_get(this_obj, key);
            res.is_void = 0; res.obj = v ? v : args[1]; return res;
        }
        if (!strcmp(method_name, "putAll")) { return res; }
        return res;
    }
    // ── android.app.Activity / Context lifecycle ─────────────────────────
    if (strstr(class_desc, "android/app/Activity") ||
        strstr(class_desc, "android/app/Application") ||
        strstr(class_desc, "android/content/Context")) {
        // All lifecycle methods are no-ops unless handled in DEX
        if (strcmp(method_name, "finish") == 0 ||
            strcmp(method_name, "startActivity") == 0 ||
            strcmp(method_name, "getSystemService") == 0 ||
            strcmp(method_name, "runOnUiThread") == 0 ||
            strcmp(method_name, "onCreate") == 0 ||
            strcmp(method_name, "onStart") == 0 ||
            strcmp(method_name, "onResume") == 0 ||
            strcmp(method_name, "onPause") == 0 ||
            strcmp(method_name, "onStop") == 0 ||
            strcmp(method_name, "onDestroy") == 0 ||
            strcmp(method_name, "<init>") == 0 ||
            strcmp(method_name, "requestWindowFeature") == 0) {
            return res;  // is_void = 1
        }
        /* setContentView(View|int): store the view for onDraw dispatch */
        if (strcmp(method_name, "setContentView") == 0) {
            if (nargs >= 1 && args[0]) {
                if (args[0]->type == OBJ_USERCLASS) {
                    g_content_view = args[0];
                    g_view_dirty   = 1;   /* trigger initial onDraw */
                } else {
                    /* int layout resource ID — inflate from XML resource map */
                    uint32_t res_id = (uint32_t)arg_prim(args[0]);
                    if (g_res_map) {
                        AineViewNode *root = aine_layout_inflate(g_res_map, res_id);
                        if (root) {
                            if (g_layout_root) aine_layout_free(g_layout_root);
                            g_layout_root = root;
                            /* Synthetic content-view stub so the event loop draws */
                            static AineObj s_layout_cv = {
                                .type = OBJ_USERCLASS,
                                .class_desc = "Laine/Layout;"
                            };
                            g_content_view = &s_layout_cv;
                            g_view_dirty   = 1;
                            fprintf(stderr, "[aine-ui] setContentView(0x%x) inflated\n",
                                    res_id);
                        } else {
                            fprintf(stderr, "[aine-ui] setContentView(0x%x) no XML\n",
                                    res_id);
                        }
                    } else {
                        fprintf(stderr, "[aine-ui] setContentView(0x%x) no res map\n",
                                res_id);
                    }
                }
            }
            return res;
        }
        /* invalidate() called on Activity itself — trigger redraw */
        if (strcmp(method_name, "invalidate") == 0) {
            g_view_dirty = 1;
            return res;
        }
        if (strcmp(method_name, "getResources") == 0) {
            static AineObj g_resources = { .type = OBJ_NULL, .class_desc = "Landroid/content/res/Resources;" };
            res.is_void = 0; res.obj = &g_resources; return res;
        }
        if (strcmp(method_name, "getString") == 0) {
            res.is_void = 0; res.obj = heap_string(""); return res;
        }
        if (strcmp(method_name, "getSharedPreferences") == 0 && nargs >= 1) {
            const char *pname = (args[0] && args[0]->str) ? args[0]->str : "default";
            AineObj *map = prefs_load(pname);
            map->class_desc = "Landroid/content/SharedPreferences;";
            heap_hashmap_put(map, "__prefs_name__", heap_string(pname));
            res.is_void = 0; res.obj = map; return res;
        }
        if (strcmp(method_name, "getPackageName") == 0) {
            res.is_void = 0; res.obj = heap_string("com.aine.app"); return res;
        }
        if (strcmp(method_name, "getFilesDir") == 0) {
            res.is_void = 0; res.obj = heap_string("/tmp/aine-files"); return res;
        }
        if (strcmp(method_name, "getCacheDir") == 0) {
            res.is_void = 0; res.obj = heap_string("/tmp/aine-cache"); return res;
        }
    }

    // ── android.os.Handler / Looper ──────────────────────────────────────
    if (strstr(class_desc, "android/os/Handler")) {
        if (strcmp(method_name, "<init>") == 0) { res.is_void = 1; return res; }
        if (strcmp(method_name, "postDelayed") == 0 && nargs >= 2) {
            /* args[0] = Runnable, args[1] = delay (prim, boxed as string) */
            AineObj *runnable = args[0];
            int64_t delay_ms = 0;
            if (args[1]) {
                if (args[1]->type == OBJ_STRING && args[1]->str)
                    delay_ms = atoll(args[1]->str);
                else
                    delay_ms = args[1]->arr_prim[0];
            }
            handler_post_delayed(runnable, delay_ms);
            res.is_void = 0; res.prim = 1; return res; /* returns boolean true */
        }
        if (strcmp(method_name, "post") == 0 && nargs >= 1) {
            handler_post_delayed(args[0], 0);
            res.is_void = 0; res.prim = 1; return res;
        }
        if (strcmp(method_name, "removeCallbacks") == 0) { return res; }
    }
    if (strstr(class_desc, "android/os/Looper")) {
        if (strcmp(method_name, "getMainLooper") == 0 || strcmp(method_name, "myLooper") == 0) {
            static AineObj fake_looper = { .type = OBJ_NULL };
            res.is_void = 0; res.obj = &fake_looper; return res;
        }
        if (strcmp(method_name, "loop") == 0 || strcmp(method_name, "prepare") == 0) {
            return res;
        }
    }

    // ── Runnable / Thread ────────────────────────────────────────────────
    if (strstr(class_desc, "java/lang/Thread")) {
        if (strcmp(method_name, "<init>") == 0) {
            /* Thread(Runnable) or Thread(String, Runnable) etc. */
            if (nargs >= 1 && args[0] && args[0]->class_desc)
                heap_iput_obj(this_obj, "target", args[0]);
            if (nargs >= 2 && args[1] && args[1]->class_desc)
                heap_iput_obj(this_obj, "target", args[1]);
            return res;
        }
        if (strcmp(method_name, "start") == 0) {
            AineObj *target = heap_iget_obj(this_obj, "target");
            if (target) handler_post_delayed(target, 0);
            return res;
        }
        if (strcmp(method_name, "sleep") == 0 && nargs >= 1) {
            int64_t ms = 0;
            if (args[0] && args[0]->str) ms = atoll(args[0]->str);
            if (ms > 0) {
                struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
                nanosleep(&ts, NULL);
            }
            return res;
        }
        if (strcmp(method_name, "currentThread") == 0) {
            static AineObj g_main_thread = { .type = OBJ_NULL,
                                             .class_desc = "Ljava/lang/Thread;" };
            res.is_void = 0; res.obj = &g_main_thread; return res;
        }
        if (strcmp(method_name, "getName") == 0) {
            AineObj *name = heap_iget_obj(this_obj, "name");
            res.is_void = 0; res.obj = name ? name : heap_string("main"); return res;
        }
        if (strcmp(method_name, "setName") == 0 && nargs >= 1) {
            heap_iput_obj(this_obj, "name", args[0]);
            return res;
        }
        if (strcmp(method_name, "join") == 0 ||
            strcmp(method_name, "interrupt") == 0 ||
            strcmp(method_name, "setDaemon") == 0 ||
            strcmp(method_name, "setPriority") == 0 ||
            strcmp(method_name, "getThreadGroup") == 0) {
            return res;  /* no-op in cooperative single-thread mode */
        }
        if (strcmp(method_name, "isAlive") == 0 ||
            strcmp(method_name, "isInterrupted") == 0) {
            res.is_void = 0; res.prim = 0; return res;
        }
        return res;
    }
    if (strstr(class_desc, "java/lang/Runnable")) {
        return res;  /* caller should dispatch run() directly */
    }

    // ── Class ───────────────────────────────────────────────────────────
    if (strcmp(class_desc, "Ljava/lang/Class;") == 0) {
        if (strcmp(method_name, "getName") == 0 || strcmp(method_name, "getSimpleName") == 0) {
            res.is_void = 0; res.obj = heap_string(class_desc); return res;
        }
        if (strcmp(method_name, "forName") == 0 && nargs >= 1) {
            res.is_void = 0; res.obj = args[0]; return res;
        }
    }

    // ── java.util.Iterator ───────────────────────────────────────────────
    if (this_obj && this_obj->type == OBJ_ITERATOR) {
        if (!strcmp(method_name, "hasNext")) {
            res.is_void = 0; res.prim = (this_obj->arr_cap < this_obj->arr_len) ? 1 : 0;
            return res;
        }
        if (!strcmp(method_name, "next")) {
            if (this_obj->arr_cap < this_obj->arr_len) {
                res.is_void = 0; res.obj = this_obj->arr_obj[this_obj->arr_cap++];
            } else { res.is_void = 0; res.obj = NULL; }
            return res;
        }
        if (!strcmp(method_name, "remove")) { return res; }
    }
    if (strstr(class_desc, "java/util/Iterator") ||
        strstr(class_desc, "java/util/ListIterator")) {
        if (!strcmp(method_name, "hasNext") && this_obj) {
            res.is_void = 0; res.prim = (this_obj->arr_cap < this_obj->arr_len) ? 1 : 0;
            return res;
        }
        if (!strcmp(method_name, "next") && this_obj) {
            if (this_obj->arr_cap < this_obj->arr_len) {
                res.is_void = 0; res.obj = this_obj->arr_obj[this_obj->arr_cap++];
            } else { res.is_void = 0; res.obj = NULL; }
            return res;
        }
        return res;
    }

    // ── android.content.SharedPreferences ────────────────────────────────
    if (strstr(class_desc, "SharedPreferences") ||
        (this_obj && this_obj->class_desc &&
         strstr(this_obj->class_desc, "SharedPreferences"))) {
        if (!this_obj || this_obj->type != OBJ_HASHMAP) { return res; }
        AineObj *pname_obj = heap_hashmap_get(this_obj, "__prefs_name__");
        const char *pname = pname_obj && pname_obj->str ? pname_obj->str : "default";

        if (!strcmp(method_name, "getString") && nargs >= 1) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str : "";
            AineObj *v = heap_hashmap_get(this_obj, key);
            if (!v && nargs >= 2) v = args[1];
            res.is_void = 0; res.obj = v ? v : heap_string(""); return res;
        }
        if (!strcmp(method_name, "getInt") && nargs >= 1) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str : "";
            AineObj *v = heap_hashmap_get(this_obj, key);
            int64_t def = (nargs >= 2 && args[1] && args[1]->str) ? atoll(args[1]->str) : 0;
            res.is_void = 0; res.prim = v && v->str ? atoll(v->str) : def; return res;
        }
        if (!strcmp(method_name, "getLong") && nargs >= 1) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str : "";
            AineObj *v = heap_hashmap_get(this_obj, key);
            int64_t def = (nargs >= 2 && args[1] && args[1]->str) ? atoll(args[1]->str) : 0;
            res.is_void = 0; res.prim = v && v->str ? atoll(v->str) : def; return res;
        }
        if (!strcmp(method_name, "getBoolean") && nargs >= 1) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str : "";
            AineObj *v = heap_hashmap_get(this_obj, key);
            int64_t def = (nargs >= 2 && args[1] && args[1]->str) ? atoll(args[1]->str) : 0;
            res.is_void = 0;
            res.prim = v && v->str ? (strcmp(v->str,"true")==0||strcmp(v->str,"1")==0?1:0) : def;
            return res;
        }
        if (!strcmp(method_name, "contains") && nargs >= 1) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str : "";
            res.is_void = 0; res.prim = heap_hashmap_contains_key(this_obj, key); return res;
        }
        if (!strcmp(method_name, "getAll")) {
            res.is_void = 0; res.obj = this_obj; return res;
        }
        if (!strcmp(method_name, "edit")) {
            /* Return a new editor map linked to same prefs */
            AineObj *editor = heap_hashmap_new();
            editor->class_desc = "Landroid/content/SharedPreferences$Editor;";
            heap_hashmap_put(editor, "__prefs_name__", heap_string(pname));
            /* Copy existing values into editor */
            for (int i = 0; i + 1 < this_obj->arr_len; i += 2) {
                AineObj *k = this_obj->arr_obj[i];
                AineObj *v = this_obj->arr_obj[i + 1];
                if (k && k->str && v)
                    heap_hashmap_put(editor, k->str, v);
            }
            res.is_void = 0; res.obj = editor; return res;
        }
        return res;
    }
    if (strstr(class_desc, "SharedPreferences$Editor") ||
        (this_obj && this_obj->class_desc &&
         strstr(this_obj->class_desc, "SharedPreferences$Editor"))) {
        if (!this_obj || this_obj->type != OBJ_HASHMAP) { return res; }
        AineObj *pname_obj = heap_hashmap_get(this_obj, "__prefs_name__");
        const char *pname = pname_obj && pname_obj->str ? pname_obj->str : "default";

        if (!strncmp(method_name, "put", 3) && nargs >= 2) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str : "";
            if (!strcmp(method_name, "putBoolean")) {
                const char *sv = (args[1] && args[1]->str) ? args[1]->str : "0";
                int bv = atoi(sv);
                heap_hashmap_put(this_obj, key, heap_string(bv ? "true" : "false"));
            } else {
                heap_hashmap_put(this_obj, key, args[1] ? args[1] : heap_string(""));
            }
            res.is_void = 0; res.obj = this_obj; return res;
        }
        if (!strcmp(method_name, "remove") && nargs >= 1) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str : "";
            heap_hashmap_remove(this_obj, key);
            res.is_void = 0; res.obj = this_obj; return res;
        }
        if (!strcmp(method_name, "clear")) {
            this_obj->arr_len = 0;
            res.is_void = 0; res.obj = this_obj; return res;
        }
        if (!strcmp(method_name, "commit") || !strcmp(method_name, "apply")) {
            prefs_save(pname, this_obj);
            res.is_void = 0; res.prim = 1; return res;
        }
        return res;
    }

    // ── android.content.Intent / Bundle ──────────────────────────────────
    if (strstr(class_desc, "android/content/Intent") ||
        strstr(class_desc, "android/os/Bundle")) {
        if (!strcmp(method_name, "<init>")) { return res; }
        if (!strcmp(method_name, "putExtra") && nargs >= 2) {
            if (this_obj) {
                const char *key = (args[0] && args[0]->str) ? args[0]->str : "";
                heap_iput_obj(this_obj, key, args[1]);
            }
            res.is_void = 0; res.obj = this_obj; return res;
        }
        if ((!strncmp(method_name, "get", 3) &&
             strstr(method_name, "Extra")) && nargs >= 1) {
            const char *key = (args[0] && args[0]->str) ? args[0]->str : "";
            AineObj *v = this_obj ? heap_iget_obj(this_obj, key) : NULL;
            if (!v && nargs >= 2) v = args[1];
            res.is_void = 0; res.obj = v; return res;
        }
        if (!strcmp(method_name, "setAction") || !strcmp(method_name, "addFlags") ||
            !strcmp(method_name, "setClass")  || !strcmp(method_name, "setComponent")) {
            res.is_void = 0; res.obj = this_obj; return res;
        }
        if (!strcmp(method_name, "getAction") || !strcmp(method_name, "getType")) {
            res.is_void = 0; res.obj = heap_string(""); return res;
        }
        return res;
    }

    // ── java.util.Collections ─────────────────────────────────────────────
    if (strcmp(class_desc, "Ljava/util/Collections;") == 0) {
        if (!strcmp(method_name, "sort") && nargs >= 1 && args[0] &&
            args[0]->type == OBJ_ARRAYLIST) {
            /* Simple string-compare sort (insertion sort for correctness) */
            AineObj *list = args[0];
            for (int i = 1; i < list->arr_len; i++) {
                AineObj *key_obj = list->arr_obj[i];
                const char *ks = key_obj && key_obj->str ? key_obj->str : "";
                int j = i - 1;
                while (j >= 0) {
                    const char *js = list->arr_obj[j] && list->arr_obj[j]->str
                                     ? list->arr_obj[j]->str : "";
                    if (strcmp(js, ks) <= 0) break;
                    list->arr_obj[j + 1] = list->arr_obj[j];
                    j--;
                }
                list->arr_obj[j + 1] = key_obj;
            }
            return res;
        }
        if (!strcmp(method_name, "reverse") && nargs >= 1 && args[0] &&
            args[0]->type == OBJ_ARRAYLIST) {
            AineObj *list = args[0];
            for (int i = 0, j = list->arr_len - 1; i < j; i++, j--) {
                AineObj *tmp = list->arr_obj[i];
                list->arr_obj[i] = list->arr_obj[j];
                list->arr_obj[j] = tmp;
            }
            return res;
        }
        if (!strcmp(method_name, "shuffle") && nargs >= 1) { return res; }
        if (!strcmp(method_name, "unmodifiableList") ||
            !strcmp(method_name, "synchronizedList") ||
            !strcmp(method_name, "unmodifiableMap")) {
            res.is_void = 0; res.obj = nargs >= 1 ? args[0] : NULL; return res;
        }
        if (!strcmp(method_name, "singletonList") && nargs >= 1) {
            AineObj *list = heap_arraylist_new();
            heap_arraylist_add(list, args[0]);
            res.is_void = 0; res.obj = list; return res;
        }
        if (!strcmp(method_name, "emptyList") || !strcmp(method_name, "EMPTY_LIST")) {
            res.is_void = 0; res.obj = heap_arraylist_new(); return res;
        }
        if (!strcmp(method_name, "min") || !strcmp(method_name, "max")) {
            if (nargs >= 1 && args[0] && args[0]->type == OBJ_ARRAYLIST &&
                args[0]->arr_len > 0) {
                AineObj *best = args[0]->arr_obj[0];
                for (int i = 1; i < args[0]->arr_len; i++) {
                    AineObj *it = args[0]->arr_obj[i];
                    const char *a = best && best->str ? best->str : "";
                    const char *b = it && it->str ? it->str : "";
                    int cmp = strcmp(a, b);
                    if ((!strcmp(method_name,"max") && cmp < 0) ||
                        (!strcmp(method_name,"min") && cmp > 0)) best = it;
                }
                res.is_void = 0; res.obj = best; return res;
            }
        }
        if (!strcmp(method_name, "frequency") || !strcmp(method_name, "nCopies")) {
            res.is_void = 0; res.prim = 0; return res;
        }
        return res;
    }

    // ── java.util.Arrays ─────────────────────────────────────────────────
    if (strcmp(class_desc, "Ljava/util/Arrays;") == 0) {
        if (!strcmp(method_name, "asList") && nargs >= 1) {
            AineObj *list = heap_arraylist_new();
            if (args[0] && args[0]->type == OBJ_ARRAY) {
                for (int i = 0; i < args[0]->arr_len; i++)
                    heap_arraylist_add(list, args[0]->arr_obj[i]);
            } else {
                for (int i = 0; i < nargs; i++)
                    heap_arraylist_add(list, args[i]);
            }
            res.is_void = 0; res.obj = list; return res;
        }
        if (!strcmp(method_name, "sort") && nargs >= 1 && args[0] &&
            args[0]->type == OBJ_ARRAY) {
            /* Simple insertion sort */
            AineObj *arr = args[0];
            for (int i = 1; i < arr->arr_len; i++) {
                int64_t kp = arr->arr_prim[i];
                AineObj *ko = arr->arr_obj[i];
                const char *ks = ko && ko->str ? ko->str : "";
                int j = i - 1;
                while (j >= 0) {
                    const char *js = arr->arr_obj[j] && arr->arr_obj[j]->str
                                     ? arr->arr_obj[j]->str : "";
                    if (arr->arr_obj[j] ? strcmp(js,ks) <= 0
                                        : arr->arr_prim[j] <= kp) break;
                    arr->arr_prim[j+1] = arr->arr_prim[j];
                    arr->arr_obj[j+1]  = arr->arr_obj[j];
                    j--;
                }
                arr->arr_prim[j+1] = kp;
                arr->arr_obj[j+1]  = ko;
            }
            return res;
        }
        if (!strcmp(method_name, "fill") && nargs >= 2) {
            if (args[0] && args[0]->type == OBJ_ARRAY) {
                for (int i = 0; i < args[0]->arr_len; i++) {
                    args[0]->arr_obj[i]  = args[1];
                    if (args[1] && args[1]->str)
                        args[0]->arr_prim[i] = atoll(args[1]->str);
                }
            }
            return res;
        }
        if ((!strcmp(method_name, "copyOf") || !strcmp(method_name, "copyOfRange"))
            && nargs >= 2) {
            AineObj *src = args[0];
            int newlen = (args[1] && args[1]->str) ? atoi(args[1]->str) : 0;
            AineObj *dst = heap_array_new(newlen);
            if (src && src->type == OBJ_ARRAY) {
                int cp = newlen < src->arr_len ? newlen : src->arr_len;
                memcpy(dst->arr_prim, src->arr_prim, (size_t)cp * sizeof(int64_t));
                memcpy(dst->arr_obj,  src->arr_obj,  (size_t)cp * sizeof(AineObj *));
            }
            res.is_void = 0; res.obj = dst; return res;
        }
        if (!strcmp(method_name, "toString") && nargs >= 1 && args[0] &&
            args[0]->type == OBJ_ARRAY) {
            char buf[512] = "["; int pos = 1;
            for (int i = 0; i < args[0]->arr_len && pos < 500; i++) {
                if (i > 0) buf[pos++] = ',';
                AineObj *it = args[0]->arr_obj[i];
                const char *sv = it && it->str ? it->str : "null";
                int sl = (int)strlen(sv);
                if (pos + sl < 500) { memcpy(buf+pos, sv, sl); pos += sl; }
            }
            buf[pos++] = ']'; buf[pos] = 0;
            res.is_void = 0; res.obj = heap_string(buf); return res;
        }
        return res;
    }

    // ── java.lang.Boolean / Double / Float / Character ───────────────────
    if (strcmp(class_desc, "Ljava/lang/Boolean;") == 0) {
        if (!strcmp(method_name, "valueOf") && nargs >= 1) {
            int bv = (args[0] && args[0]->str) ?
                     (strcmp(args[0]->str,"true")==0 || strcmp(args[0]->str,"1")==0 ? 1 : atoi(args[0]->str))
                     : 0;
            res.is_void = 0; res.obj = heap_string(bv ? "true" : "false"); return res;
        }
        if (!strcmp(method_name, "booleanValue") || !strcmp(method_name, "parseBoolean")) {
            const char *sv = this_obj && this_obj->str ? this_obj->str
                           : (nargs>=1 && args[0] && args[0]->str ? args[0]->str : "false");
            res.is_void = 0; res.prim = strcmp(sv,"true")==0||strcmp(sv,"1")==0?1:0;
            return res;
        }
        if (!strcmp(method_name, "toString")) {
            const char *sv = this_obj && this_obj->str ? this_obj->str : "false";
            res.is_void = 0; res.obj = heap_string(sv); return res;
        }
    }
    if (strcmp(class_desc, "Ljava/lang/Double;") == 0 ||
        strcmp(class_desc, "Ljava/lang/Float;")  == 0) {
        if (!strcmp(method_name, "parseDouble") || !strcmp(method_name, "parseFloat")) {
            double v = (nargs>=1 && args[0] && args[0]->str) ? atof(args[0]->str) : 0.0;
            res.is_void = 0; res.prim = (int64_t)v; return res;
        }
        if (!strcmp(method_name, "toString") || !strcmp(method_name, "valueOf")) {
            res.is_void = 0; res.obj = nargs>=1 ? args[0] : this_obj; return res;
        }
        if (!strcmp(method_name, "doubleValue") || !strcmp(method_name, "floatValue")) {
            const char *sv = this_obj && this_obj->str ? this_obj->str : "0";
            res.is_void = 0; res.prim = (int64_t)atof(sv); return res;
        }
        if (!strcmp(method_name, "isNaN") || !strcmp(method_name, "isInfinite")) {
            res.is_void = 0; res.prim = 0; return res;
        }
    }
    if (strcmp(class_desc, "Ljava/lang/Character;") == 0) {
        if (!strcmp(method_name, "isDigit") && nargs >= 1) {
            int c = (args[0] && args[0]->str) ? (unsigned char)args[0]->str[0] : 0;
            res.is_void = 0; res.prim = isdigit(c) ? 1 : 0; return res;
        }
        if (!strcmp(method_name, "isLetter") && nargs >= 1) {
            int c = (args[0] && args[0]->str) ? (unsigned char)args[0]->str[0] : 0;
            res.is_void = 0; res.prim = isalpha(c) ? 1 : 0; return res;
        }
        if (!strcmp(method_name, "isUpperCase") || !strcmp(method_name, "isLowerCase")) {
            int c = (args[0] && args[0]->str) ? (unsigned char)args[0]->str[0] : 0;
            res.is_void = 0;
            res.prim = !strcmp(method_name,"isUpperCase") ? (isupper(c)?1:0) : (islower(c)?1:0);
            return res;
        }
        if (!strcmp(method_name, "toUpperCase") || !strcmp(method_name, "toLowerCase")) {
            int c = (args[0] && args[0]->str) ? (unsigned char)args[0]->str[0] : 0;
            char buf[2] = { (char)(!strcmp(method_name,"toUpperCase") ? toupper(c) : tolower(c)), 0 };
            res.is_void = 0; res.prim = buf[0]; return res;
        }
        if (!strcmp(method_name, "valueOf") && nargs >= 1) {
            res.is_void = 0; res.obj = args[0]; return res;
        }
    }

    // ── java.io.File ─────────────────────────────────────────────────────
    if (strcmp(class_desc, "Ljava/io/File;") == 0) {
        if (!strcmp(method_name, "<init>") && nargs >= 1) {
            if (this_obj) {
                const char *p = (args[0] && args[0]->str) ? args[0]->str : "/tmp";
                heap_iput_obj(this_obj, "path", heap_string(p));
            }
            return res;
        }
        const char *path = "";
        if (this_obj) {
            AineObj *pv = heap_iget_obj(this_obj, "path");
            if (pv && pv->str) path = pv->str;
        }
        if (!strcmp(method_name, "getPath") || !strcmp(method_name, "getAbsolutePath") ||
            !strcmp(method_name, "toString")) {
            res.is_void = 0; res.obj = heap_string(path); return res;
        }
        if (!strcmp(method_name, "getName")) {
            const char *slash = strrchr(path, '/');
            res.is_void = 0; res.obj = heap_string(slash ? slash+1 : path); return res;
        }
        if (!strcmp(method_name, "exists") || !strcmp(method_name, "isFile") ||
            !strcmp(method_name, "isDirectory")) {
            struct stat st;
            int exists = stat(path, &st) == 0;
            if (!strcmp(method_name, "isFile")) exists = exists && S_ISREG(st.st_mode);
            if (!strcmp(method_name, "isDirectory")) exists = exists && S_ISDIR(st.st_mode);
            res.is_void = 0; res.prim = exists ? 1 : 0; return res;
        }
        if (!strcmp(method_name, "mkdirs") || !strcmp(method_name, "mkdir")) {
            res.is_void = 0; res.prim = mkdir(path, 0755) == 0 ? 1 : 0; return res;
        }
        if (!strcmp(method_name, "length")) {
            struct stat st;
            res.is_void = 0; res.prim = (stat(path,&st)==0) ? (int64_t)st.st_size : 0;
            return res;
        }
        if (!strcmp(method_name, "delete")) {
            res.is_void = 0; res.prim = remove(path) == 0 ? 1 : 0; return res;
        }
        if (!strcmp(method_name, "canRead") || !strcmp(method_name, "canWrite")) {
            res.is_void = 0; res.prim = 1; return res;
        }
        return res;
    }

    // ── android.content.res.Resources ────────────────────────────────────
    if (strstr(class_desc, "android/content/res/Resources")) {
        if (!strcmp(method_name, "getString") || !strcmp(method_name, "getText")) {
            res.is_void = 0; res.obj = heap_string(""); return res;
        }
        if (!strcmp(method_name, "getInteger")) { res.is_void = 0; res.prim = 0; return res; }
        if (!strcmp(method_name, "getBoolean")) { res.is_void = 0; res.prim = 0; return res; }
        if (!strcmp(method_name, "getIdentifier")) { res.is_void = 0; res.prim = 0; return res; }
        return res;
    }

    // ── android.os.SystemClock ────────────────────────────────────────────
    if (strstr(class_desc, "android/os/SystemClock")) {
        if (!strcmp(method_name, "elapsedRealtime") ||
            !strcmp(method_name, "uptimeMillis") ||
            !strcmp(method_name, "currentThreadTimeMillis")) {
            struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
            res.is_void = 0;
            res.prim = (int64_t)ts.tv_sec*1000 + ts.tv_nsec/1000000;
            return res;
        }
        if (!strcmp(method_name, "sleep") && nargs >= 1) {
            int64_t ms = (args[0] && args[0]->str) ? atoll(args[0]->str) : 0;
            if (ms > 0) {
                struct timespec ts = { ms/1000, (ms%1000)*1000000L };
                nanosleep(&ts, NULL);
            }
            return res;
        }
    }

    // ── Activity extras: finish, getWindow, findViewById, LayoutInflater ──
    if (strstr(class_desc, "android/app/Activity") ||
        strstr(class_desc, "android/app/Application")) {
        if (!strcmp(method_name, "finish")) {
            /* Signal the interactive event loop to exit */
            fprintf(stderr, "[aine-dalvik] Activity.finish() called\n");
#ifdef __APPLE__
            extern void aine_activity_request_finish(void);
            aine_activity_request_finish();
#endif
            return res;
        }
        if (!strcmp(method_name, "getWindow")) {
            static AineObj g_window_stub = { .type = OBJ_NULL };
            g_window_stub.class_desc = "Landroid/view/Window;";
            res.is_void = 0; res.obj = &g_window_stub; return res;
        }
        if (!strcmp(method_name, "getWindowManager")) {
            static AineObj g_wm_stub = { .type = OBJ_NULL };
            g_wm_stub.class_desc = "Landroid/view/WindowManager;";
            res.is_void = 0; res.obj = &g_wm_stub; return res;
        }
        if (!strcmp(method_name, "getLayoutInflater") ||
            !strcmp(method_name, "getMenuInflater")) {
            static AineObj g_inflater = { .type = OBJ_NULL };
            g_inflater.class_desc = "Landroid/view/LayoutInflater;";
            res.is_void = 0; res.obj = &g_inflater; return res;
        }
        if (!strcmp(method_name, "findViewById") ||
            !strcmp(method_name, "requireViewById")) {
            /* Return a generic View stub with res_id stored for setText/onClick */
            AineObj *view = calloc(1, sizeof(AineObj));
            view->type = OBJ_USERCLASS;
            view->class_desc = "Landroid/view/View;";
            if (nargs >= 1 && args[0]) {
                heap_iput_obj(view, "__resid__", args[0]);
                int rid = (int)arg_prim(args[0]);
                /* Register so click dispatch can find this stub by res_id */
                register_view_stub(rid, view);
            }
            res.is_void = 0; res.obj = view; return res;
        }
        if (!strcmp(method_name, "getActionBar") ||
            !strcmp(method_name, "getSupportActionBar")) {
            res.is_void = 0; res.obj = NULL; return res;
        }
        if (!strcmp(method_name, "getIntent")) {
            static AineObj g_intent = { .type = OBJ_HASHMAP };
            g_intent.class_desc = "Landroid/content/Intent;";
            res.is_void = 0; res.obj = &g_intent; return res;
        }
        if (!strcmp(method_name, "getApplication") ||
            !strcmp(method_name, "getApplicationContext")) {
            res.is_void = 0; res.obj = this_obj; return res;
        }
        if (!strcmp(method_name, "setTitle") ||
            !strcmp(method_name, "setRequestedOrientation") ||
            !strcmp(method_name, "setVolumeControlStream") ||
            !strcmp(method_name, "overridePendingTransition") ||
            !strcmp(method_name, "invalidateOptionsMenu") ||
            !strcmp(method_name, "supportInvalidateOptionsMenu")) {
            return res;
        }
    }

    // ── ViewBinding / DataBinding stubs ─────────────────────────────────
    if (strstr(class_desc, "/databinding/") ||
        strstr(class_desc, "ViewBinding")) {
        if (!strcmp(method_name, "inflate") || !strcmp(method_name, "bind")) {
            /* Create a stub binding object with pre-populated stub views */
            AineObj *binding = calloc(1, sizeof(AineObj));
            binding->type = OBJ_USERCLASS; binding->class_desc = class_desc;
            /* Root view */
            AineObj *root = calloc(1, sizeof(AineObj));
            root->type = OBJ_USERCLASS; root->class_desc = "Landroid/view/View;";
            heap_iput_obj(binding, "root", root);
            /* Stub TextViews */
            static const char *tv_names[] = { "textDisplay", "textExpression",
                "display", "expression", "tvResult", "tvExpression", NULL };
            for (int i = 0; tv_names[i]; i++) {
                AineObj *tv = calloc(1, sizeof(AineObj));
                tv->type = OBJ_USERCLASS; tv->class_desc = "Landroid/widget/TextView;";
                heap_iput_obj(binding, tv_names[i], tv);
            }
            /* Stub Buttons */
            static const char *btn_names[] = {
                "button0","button1","button2","button3","button4",
                "button5","button6","button7","button8","button9",
                "buttonDecimal","buttonClear","buttonBackspace","buttonSign",
                "buttonPercent","buttonAdd","buttonSubtract","buttonMultiply",
                "buttonDivide","buttonEquals","buttonAC","buttonBack",
                "btnEquals","btnAdd","btnSub","btnMul","btnDiv",
                "btnClear","btnDecimal",NULL
            };
            for (int i = 0; btn_names[i]; i++) {
                AineObj *btn = calloc(1, sizeof(AineObj));
                btn->type = OBJ_USERCLASS; btn->class_desc = "Landroid/widget/Button;";
                heap_iput_obj(binding, btn_names[i], btn);
            }
            fprintf(stderr, "[aine-ui] ViewBinding.inflate -> stub binding created\n");
            res.is_void = 0; res.obj = binding; return res;
        }
        if (!strcmp(method_name, "getRoot")) {
            AineObj *r = this_obj ? heap_iget_obj(this_obj, "root") : NULL;
            if (!r) { r = calloc(1, sizeof(AineObj)); r->type = OBJ_USERCLASS; r->class_desc = "Landroid/view/View;"; }
            res.is_void = 0; res.obj = r; return res;
        }
        /* Other binding field getters → stub view */
        res.is_void = 0; res.obj = this_obj; return res;
    }

    // ── android.view.KeyEvent ─────────────────────────────────────────────
    if (strstr(class_desc, "android/view/KeyEvent")) {
        if (!strcmp(method_name, "<init>") && nargs >= 2) {
            /* KeyEvent(int action, int keyCode) */
            if (this_obj) {
                this_obj->class_desc = "Landroid/view/KeyEvent;";
                heap_iput_prim(this_obj, "action",  arg_prim(args[0]));
                heap_iput_prim(this_obj, "keycode", arg_prim(args[1]));
            }
            return res;
        }
        if (!strcmp(method_name, "getAction")) {
            res.is_void = 0;
            res.prim = this_obj ? heap_iget_prim(this_obj, "action") : 0;
            return res;
        }
        if (!strcmp(method_name, "getKeyCode")) {
            res.is_void = 0;
            res.prim = this_obj ? heap_iget_prim(this_obj, "keycode") : 0;
            return res;
        }
        if (!strcmp(method_name, "getMetaState")) {
            res.is_void = 0;
            res.prim = this_obj ? heap_iget_prim(this_obj, "meta") : 0;
            return res;
        }
        if (!strcmp(method_name, "getRepeatCount") || !strcmp(method_name, "getFlags") ||
            !strcmp(method_name, "getScanCode") ||
            !strcmp(method_name, "isCtrlPressed") || !strcmp(method_name, "isShiftPressed") ||
            !strcmp(method_name, "isAltPressed") || !strcmp(method_name, "isCanceled")) {
            res.is_void = 0; res.prim = 0; return res;
        }
        return res;
    }

    // ── android.view.MotionEvent ──────────────────────────────────────────
    if (strstr(class_desc, "android/view/MotionEvent")) {
        if (!strcmp(method_name, "<init>")) {
            if (this_obj) this_obj->class_desc = "Landroid/view/MotionEvent;";
            return res;
        }
        if (!strcmp(method_name, "getAction") || !strcmp(method_name, "getActionMasked")) {
            res.is_void = 0;
            res.prim = this_obj ? heap_iget_prim(this_obj, "action") : 0;
            return res;
        }
        if (!strcmp(method_name, "getX") || !strcmp(method_name, "getRawX")) {
            union { int64_t i; float f; } v;
            v.i = this_obj ? heap_iget_prim(this_obj, "x") : 0;
            res.is_void = 0; res.prim = (int64_t)(v.f); return res;
        }
        if (!strcmp(method_name, "getY") || !strcmp(method_name, "getRawY")) {
            union { int64_t i; float f; } v;
            v.i = this_obj ? heap_iget_prim(this_obj, "y") : 0;
            res.is_void = 0; res.prim = (int64_t)(v.f); return res;
        }
        if (!strcmp(method_name, "getPointerCount")) { res.is_void = 0; res.prim = 1; return res; }
        if (!strcmp(method_name, "getPointerId"))    { res.is_void = 0; res.prim = 0; return res; }
        if (!strcmp(method_name, "getPressure"))     { res.is_void = 0; res.prim = 1; return res; }
        if (!strcmp(method_name, "getSize"))         { res.is_void = 0; res.prim = 0; return res; }
        if (!strcmp(method_name, "obtain") || !strcmp(method_name, "recycle")) { return res; }
        return res;
    }

    // ── android.view.Window ──────────────────────────────────────────────
    if (strstr(class_desc, "android/view/Window")) {
        if (!strcmp(method_name, "setFlags") ||
            !strcmp(method_name, "addFlags") ||
            !strcmp(method_name, "clearFlags") ||
            !strcmp(method_name, "setSoftInputMode") ||
            !strcmp(method_name, "requestFeature") ||
            !strcmp(method_name, "setStatusBarColor") ||
            !strcmp(method_name, "setNavigationBarColor")) {
            return res;
        }
        if (!strcmp(method_name, "getDecorView") ||
            !strcmp(method_name, "getContext")) {
            res.is_void = 0; res.obj = this_obj; return res;
        }
        return res;
    }

    // ── android.view.View / ViewGroup / FrameLayout / LinearLayout etc ───
    if (strstr(class_desc, "android/view/View") ||
        strstr(class_desc, "android/view/ViewGroup") ||
        strstr(class_desc, "android/view/SurfaceView") ||
        strstr(class_desc, "android/widget/FrameLayout") ||
        strstr(class_desc, "android/widget/LinearLayout") ||
        strstr(class_desc, "android/widget/RelativeLayout") ||
        strstr(class_desc, "android/widget/ScrollView") ||
        strstr(class_desc, "android/widget/ListView") ||
        strstr(class_desc, "android/widget/RecyclerView") ||
        strstr(class_desc, "android/support/") ||
        strstr(class_desc, "androidx/")) {
        if (!strcmp(method_name, "<init>")) {
            /* Only set class_desc from framework <init> if the object doesn't
             * already have a more specific (user-defined) class_desc. */
            if (this_obj && !this_obj->class_desc)
                this_obj->class_desc = class_desc;
            return res;
        }
        if (!strcmp(method_name, "setVisibility") ||
            !strcmp(method_name, "setEnabled") ||
            !strcmp(method_name, "setClickable") ||
            !strcmp(method_name, "setFocusable") ||
            !strcmp(method_name, "setFocusableInTouchMode") ||
            !strcmp(method_name, "setSelected") ||
            !strcmp(method_name, "setActivated") ||
            !strcmp(method_name, "setAlpha") ||
            !strcmp(method_name, "setScaleX") || !strcmp(method_name, "setScaleY") ||
            !strcmp(method_name, "setTranslationX") || !strcmp(method_name, "setTranslationY") ||
            !strcmp(method_name, "setPadding") || !strcmp(method_name, "setMargins") ||
            !strcmp(method_name, "setBackgroundColor") ||
            !strcmp(method_name, "setBackgroundResource") ||
            !strcmp(method_name, "setBackground") ||
            !strcmp(method_name, "setLayoutParams") ||
            !strcmp(method_name, "requestLayout") ||
            !strcmp(method_name, "bringToFront") ||
            !strcmp(method_name, "addView") ||
            !strcmp(method_name, "removeView") ||
            !strcmp(method_name, "removeAllViews") ||
            !strcmp(method_name, "measure") ||
            !strcmp(method_name, "layout") ||
            !strcmp(method_name, "draw")) {
            return res;
        }
        if (!strcmp(method_name, "setOnClickListener") && nargs >= 1) {
            if (this_obj) heap_iput_obj(this_obj, "onClick", args[0]);
            return res;
        }
        /* View.invalidate() — mark content view dirty for onDraw dispatch */
        if (!strcmp(method_name, "invalidate")) {
            if (this_obj == g_content_view || g_content_view == NULL)
                g_view_dirty = 1;
            return res;
        }
        if (!strcmp(method_name, "setOnLongClickListener") ||
            !strcmp(method_name, "setOnTouchListener") ||
            !strcmp(method_name, "setOnKeyListener") ||
            !strcmp(method_name, "setOnFocusChangeListener") ||
            !strcmp(method_name, "setOnCheckedChangeListener") ||
            !strcmp(method_name, "setOnItemClickListener") ||
            !strcmp(method_name, "setOnItemSelectedListener")) {
            if (this_obj && nargs >= 1) heap_iput_obj(this_obj, "listener", args[0]);
            return res;
        }
        if (!strcmp(method_name, "getContext")) {
            res.is_void = 0; res.obj = this_obj; return res;
        }
        if (!strcmp(method_name, "getId")) {
            AineObj *rid = this_obj ? heap_iget_obj(this_obj, "__resid__") : NULL;
            res.is_void = 0; res.prim = rid && rid->str ? atoi(rid->str) : 0; return res;
        }
        if (!strcmp(method_name, "getTag")) {
            res.is_void = 0; res.obj = this_obj ? heap_iget_obj(this_obj, "__tag__") : NULL; return res;
        }
        if (!strcmp(method_name, "setTag") && nargs >= 1) {
            if (this_obj) heap_iput_obj(this_obj, "__tag__", args[0]);
            return res;
        }
        if (!strcmp(method_name, "getWidth")) { res.is_void = 0; res.prim = 800; return res; }
        if (!strcmp(method_name, "getHeight")) { res.is_void = 0; res.prim = 600; return res; }
        if (!strcmp(method_name, "getVisibility")) { res.is_void = 0; res.prim = 0; return res; }  /* VISIBLE */
        if (!strcmp(method_name, "isEnabled")) { res.is_void = 0; res.prim = 1; return res; }
        if (!strcmp(method_name, "isShown")) { res.is_void = 0; res.prim = 1; return res; }
        if (!strcmp(method_name, "post") || !strcmp(method_name, "postDelayed")) {
            if (nargs >= 1) handler_post_delayed(args[0], 0);
            res.is_void = 0; res.prim = 1; return res;
        }
        if (!strcmp(method_name, "getChildCount")) { res.is_void = 0; res.prim = 0; return res; }
        if (!strcmp(method_name, "getChildAt")) { res.is_void = 0; res.obj = NULL; return res; }
        if (!strcmp(method_name, "findViewById")) {
            res.is_void = 0; res.obj = this_obj; return res;
        }
        /* Compose setContent stubs — caught here due to androidx/ wildcard above */
        if (!strcmp(method_name, "setContent") ||
            !strcmp(method_name, "setContent$default")) {
            fprintf(stderr, "[aine-ui] ComponentActivity.setContent called (Compose UI)\n");
            return res;
        }
        if (!strcmp(method_name, "mutableStateOf$default") ||
            !strcmp(method_name, "mutableStateOf")) {
            AineObj *sv = calloc(1, sizeof(AineObj));
            sv->type = OBJ_USERCLASS; sv->class_desc = class_desc;
            if (nargs >= 1 && args[0]) {
                heap_iput_obj(sv, "value", args[0]);
                AineObj *disp = heap_iget_obj(args[0], "display");
                AineObj *expr = heap_iget_obj(args[0], "expression");
                if (disp || expr) {
                    fprintf(stderr, "[aine-ui] Compose initial state: display=\"%s\" expression=\"%s\"\n",
                            disp && disp->str ? disp->str : "",
                            expr && expr->str ? expr->str : "");
                }
            }
            res.is_void = 0; res.obj = sv; return res;
        }
        /* Fallthrough: return void stub */
        return res;
    }

    // ── android.widget.TextView / Button / EditText / CheckBox / Material Components / etc ─────
    if (strstr(class_desc, "android/widget/") ||
        strstr(class_desc, "android/app/AlertDialog") ||
        strstr(class_desc, "com/google/android/material/")) {
        if (!strcmp(method_name, "<init>")) {
            if (this_obj && !this_obj->class_desc)
                this_obj->class_desc = class_desc;
            return res;
        }
        if (!strcmp(method_name, "setText") && nargs >= 1) {
            if (this_obj && args[0]) {
                const char *t = args[0]->str ? args[0]->str : "";
                fprintf(stderr, "[aine-ui] setText: \"%s\"\n", t);
                heap_iput_obj(this_obj, "text", args[0]);
                /* Propagate to layout node if this is a view stub with __resid__ */
                if (g_layout_root) {
                    AineObj *rid_obj = heap_iget_obj(this_obj, "__resid__");
                    if (rid_obj) {
                        int rid = (int)arg_prim(rid_obj);
                        AineViewNode *node = aine_layout_find_by_id(g_layout_root, rid);
                        if (node) {
                            aine_layout_set_text(node, t);
                            g_view_dirty = 1;   /* trigger redraw */
                        }
                    }
                }
            }
            return res;
        }
        if (!strcmp(method_name, "getText") ||
            !strcmp(method_name, "getHint") ||
            !strcmp(method_name, "getError")) {
            AineObj *t = this_obj ? heap_iget_obj(this_obj, "text") : NULL;
            res.is_void = 0; res.obj = t ? t : heap_string(""); return res;
        }
        if (!strcmp(method_name, "append") && nargs >= 1) {
            AineObj *old = this_obj ? heap_iget_obj(this_obj, "text") : NULL;
            const char *os = old && old->str ? old->str : "";
            const char *ns = args[0] && args[0]->str ? args[0]->str : "";
            char buf[1024]; snprintf(buf, sizeof(buf), "%s%s", os, ns);
            if (this_obj) heap_iput_obj(this_obj, "text", heap_string(buf));
            return res;
        }
        if (!strcmp(method_name, "setTextColor") ||
            !strcmp(method_name, "setTextSize") ||
            !strcmp(method_name, "setTypeface") ||
            !strcmp(method_name, "setHint") ||
            !strcmp(method_name, "setHintTextColor") ||
            !strcmp(method_name, "setInputType") ||
            !strcmp(method_name, "setImeOptions") ||
            !strcmp(method_name, "setGravity") ||
            !strcmp(method_name, "setLines") ||
            !strcmp(method_name, "setMaxLines") ||
            !strcmp(method_name, "setSingleLine") ||
            !strcmp(method_name, "setEllipsize") ||
            !strcmp(method_name, "setCompoundDrawables") ||
            !strcmp(method_name, "setCompoundDrawablePadding") ||
            !strcmp(method_name, "setMovementMethod") ||
            !strcmp(method_name, "setAdapter") ||
            !strcmp(method_name, "setSelection") ||
            !strcmp(method_name, "setEnabled") ||
            !strcmp(method_name, "setOnClickListener") ||
            !strcmp(method_name, "setOnCheckedChangeListener") ||
            !strcmp(method_name, "setOnEditorActionListener") ||
            !strcmp(method_name, "setContentDescription")) {
            if (!strcmp(method_name, "setOnClickListener") && nargs >= 1 && this_obj)
                heap_iput_obj(this_obj, "onClick", args[0]);
            return res;
        }
        if (!strcmp(method_name, "setChecked") && nargs >= 1) {
            if (this_obj) {
                AineObj *v = calloc(1, sizeof(AineObj));
                v->type = OBJ_STRING; v->str = args[0] ? "true" : "false";
                heap_iput_obj(this_obj, "checked", v);
            }
            return res;
        }
        if (!strcmp(method_name, "isChecked")) {
            AineObj *c = this_obj ? heap_iget_obj(this_obj, "checked") : NULL;
            res.is_void = 0; res.prim = (c && c->str && !strcmp(c->str, "true")) ? 1 : 0;
            return res;
        }
        if (!strcmp(method_name, "length")) {
            AineObj *t = this_obj ? heap_iget_obj(this_obj, "text") : NULL;
            res.is_void = 0; res.prim = t && t->str ? (int64_t)strlen(t->str) : 0;
            return res;
        }
        if (!strcmp(method_name, "show") || !strcmp(method_name, "dismiss") ||
            !strcmp(method_name, "create") || !strcmp(method_name, "setPositiveButton") ||
            !strcmp(method_name, "setNegativeButton") || !strcmp(method_name, "setMessage") ||
            !strcmp(method_name, "setTitle") || !strcmp(method_name, "setCancelable") ||
            !strcmp(method_name, "setView") || !strcmp(method_name, "setItems")) {
            if (!strcmp(method_name, "show") && strstr(class_desc, "Toast")) {
                AineObj *t = this_obj ? heap_iget_obj(this_obj, "text") : NULL;
                fprintf(stderr, "[aine-toast] %s\n", t && t->str ? t->str : "");
            }
            return res;
        }
        /* Generic view methods (visibility, background, etc.) */
        return res;
    }

    // ── android.widget.Toast (static makeText) ───────────────────────────
    if (strstr(class_desc, "android/widget/Toast")) {
        if (!strcmp(method_name, "makeText") && nargs >= 2) {
            AineObj *toast = calloc(1, sizeof(AineObj));
            toast->type = OBJ_USERCLASS;
            toast->class_desc = "Landroid/widget/Toast;";
            heap_iput_obj(toast, "text", args[1] ? args[1] : heap_string(""));
            res.is_void = 0; res.obj = toast; return res;
        }
        if (!strcmp(method_name, "show")) {
            AineObj *t = this_obj ? heap_iget_obj(this_obj, "text") : NULL;
            fprintf(stderr, "[aine-toast] %s\n", t && t->str ? t->str : "");
            return res;
        }
        if (!strcmp(method_name, "cancel") || !strcmp(method_name, "setDuration") ||
            !strcmp(method_name, "setGravity") || !strcmp(method_name, "setView")) {
            return res;
        }
        return res;
    }

    // ── android.view.LayoutInflater ──────────────────────────────────────
    if (strstr(class_desc, "android/view/LayoutInflater") ||
        strstr(class_desc, "android/view/MenuInflater")) {
        if (!strcmp(method_name, "inflate") || !strcmp(method_name, "inflateMenu")) {
            AineObj *view = calloc(1, sizeof(AineObj));
            view->type = OBJ_USERCLASS;
            view->class_desc = "Landroid/view/View;";
            res.is_void = 0; res.obj = view; return res;
        }
        if (!strcmp(method_name, "from") || !strcmp(method_name, "<init>")) {
            AineObj *inf = calloc(1, sizeof(AineObj));
            inf->type = OBJ_USERCLASS;
            inf->class_desc = "Landroid/view/LayoutInflater;";
            res.is_void = 0; res.obj = inf; return res;
        }
        return res;
    }

    // ── android.graphics.* ──────────────────────────────────────────────
    if (strstr(class_desc, "android/graphics/")) {
        if (!strcmp(method_name, "<init>")) {
            if (this_obj && strstr(class_desc, "RectF")) {
                /* RectF(left, top, right, bottom) */
                this_obj->class_desc = "Landroid/graphics/RectF;";
                if (nargs >= 4) {
                    heap_iput_prim(this_obj, "left",   arg_prim(args[0]));
                    heap_iput_prim(this_obj, "top",    arg_prim(args[1]));
                    heap_iput_prim(this_obj, "right",  arg_prim(args[2]));
                    heap_iput_prim(this_obj, "bottom", arg_prim(args[3]));
                }
                return res;
            }
            if (this_obj && strstr(class_desc, "Paint")) {
                /* Copy constructor: new Paint(src) */
                if (nargs >= 1 && args[0] && args[0]->class_desc &&
                    strstr(args[0]->class_desc, "Paint")) {
                    heap_iput_prim(this_obj, "color",       heap_iget_prim(args[0], "color"));
                    heap_iput_prim(this_obj, "textsize",    heap_iget_prim(args[0], "textsize"));
                    heap_iput_prim(this_obj, "style",       heap_iget_prim(args[0], "style"));
                    heap_iput_prim(this_obj, "strokewidth", heap_iget_prim(args[0], "strokewidth"));
                } else {
                    /* Default constructor: FILL style, width 0, black */
                    heap_iput_prim(this_obj, "color",       0xFF000000LL);
                    heap_iput_prim(this_obj, "textsize",    0x41800000LL); /* 16.0f bits */
                    heap_iput_prim(this_obj, "style",       0LL);          /* FILL */
                    heap_iput_prim(this_obj, "strokewidth", 0LL);          /* 0.0f */
                }
            }
            return res;
        }
        /* ── Paint setters ──────────────────────────────────────────── */
        if (!strcmp(method_name, "setColor") && nargs >= 1) {
            if (this_obj) heap_iput_prim(this_obj, "color", arg_prim(args[0]));
            return res;
        }
        if (!strcmp(method_name, "setTextSize") && nargs >= 1) {
            if (this_obj) heap_iput_prim(this_obj, "textsize", arg_prim(args[0]));
            return res;
        }
        if (!strcmp(method_name, "getColor") && this_obj) {
            res.is_void = 0;
            res.prim = heap_iget_prim(this_obj, "color");
            return res;
        }
        if (!strcmp(method_name, "measureText") && nargs >= 1) {
            const char *s = args[0] && args[0]->str ? args[0]->str : "";
            res.is_void = 0; res.prim = (int64_t)(strlen(s) * 8); return res;
        }
        if (!strcmp(method_name, "setStrokeWidth") && nargs >= 1 && this_obj) {
            heap_iput_prim(this_obj, "strokewidth", arg_prim(args[0]));
            return res;
        }
        if (!strcmp(method_name, "setStyle") && nargs >= 1 && this_obj) {
            /* args[0] is Paint.Style enum object; read its "value" field */
            int64_t sv = args[0] ? heap_iget_prim(args[0], "value") : 0LL;
            heap_iput_prim(this_obj, "style", sv);
            return res;
        }
        if (!strcmp(method_name, "setAntiAlias") ||
            !strcmp(method_name, "setTypeface") || !strcmp(method_name, "setAlpha") ||
            !strcmp(method_name, "setFlags") || !strcmp(method_name, "reset") ||
            !strcmp(method_name, "setShadowLayer") || !strcmp(method_name, "setFakeBoldText") ||
            !strcmp(method_name, "setLetterSpacing") || !strcmp(method_name, "setXfermode")) {
            return res;
        }
        /* ── Canvas drawing — wire to software canvas ────────────────── */
        if (!strcmp(method_name, "drawColor") && nargs >= 1) {
#ifdef __APPLE__
            uint32_t c = (uint32_t)arg_prim(args[0]);
            if ((c & 0xFF000000u) == 0) c |= 0xFF000000u;
            aine_canvas_clear(c);
#endif
            return res;
        }
        if (!strcmp(method_name, "drawRect") && nargs == 2 && args[0] &&
            args[0]->class_desc && strstr(args[0]->class_desc, "RectF")) {
#ifdef __APPLE__
            /* drawRect(RectF oval, Paint paint) */
            union { int64_t i; float f; } lv, tv, rv, bv;
            lv.i = heap_iget_prim(args[0], "left");
            tv.i = heap_iget_prim(args[0], "top");
            rv.i = heap_iget_prim(args[0], "right");
            bv.i = heap_iget_prim(args[0], "bottom");
            AineObj *paint = args[1];
            uint32_t color = paint ? (uint32_t)heap_iget_prim(paint, "color") : 0xFF888888u;
            if ((color & 0xFF000000u) == 0) color |= 0xFF000000u;
            int64_t style = paint ? heap_iget_prim(paint, "style") : 0LL;
            if (style == 1 /* STROKE */) {
                union { int64_t i; float f; } sw; sw.i = paint ? heap_iget_prim(paint, "strokewidth") : 0LL;
                aine_canvas_stroke_rect(lv.f, tv.f, rv.f - lv.f, bv.f - tv.f, sw.f, color);
            } else {
                aine_canvas_fill_rect(lv.f, tv.f, rv.f - lv.f, bv.f - tv.f, color);
            }
#endif
            return res;
        }
        if (!strcmp(method_name, "drawRect") && nargs >= 5) {
#ifdef __APPLE__
            float l = arg_float(args[0]);
            float t = arg_float(args[1]);
            float r = arg_float(args[2]);
            float b = arg_float(args[3]);
            AineObj *paint = args[4];
            uint32_t color = paint ? (uint32_t)heap_iget_prim(paint, "color") : 0xFF888888u;
            if ((color & 0xFF000000u) == 0) color |= 0xFF000000u;
            int64_t style = paint ? heap_iget_prim(paint, "style") : 0LL;
            if (style == 1 /* STROKE */) {
                union { int64_t i; float f; } sw; sw.i = paint ? heap_iget_prim(paint, "strokewidth") : 0LL;
                aine_canvas_stroke_rect(l, t, r - l, b - t, sw.f, color);
            } else {
                aine_canvas_fill_rect(l, t, r - l, b - t, color);
            }
#endif
            return res;
        }
        if (!strcmp(method_name, "drawText") && nargs >= 4) {
#ifdef __APPLE__
            /* drawText(String text, float x, float y, Paint paint) */
            const char *text = args[0] && args[0]->str ? args[0]->str : "";
            float x        = arg_float(args[1]);
            float y        = arg_float(args[2]);
            AineObj *paint = args[3];
            uint32_t color = 0xFFFFFFFFu;
            float    tsize = 16.0f;
            if (paint) {
                color = (uint32_t)heap_iget_prim(paint, "color");
                if ((color & 0xFF000000u) == 0) color |= 0xFF000000u;
                union { int64_t i; float f; } tv;
                tv.i = heap_iget_prim(paint, "textsize");
                if (tv.f > 0.5f) tsize = tv.f;
            }
            aine_canvas_draw_text(x, y, text, tsize, color);
#endif
            return res;
        }
        if (!strcmp(method_name, "drawCircle") && nargs >= 4) {
#ifdef __APPLE__
            float cx    = arg_float(args[0]);
            float cy    = arg_float(args[1]);
            float rad   = arg_float(args[2]);
            AineObj *paint = args[3];
            uint32_t color = paint ? (uint32_t)heap_iget_prim(paint, "color") : 0xFFFF0000u;
            if ((color & 0xFF000000u) == 0) color |= 0xFF000000u;
            aine_canvas_draw_circle(cx, cy, rad, color);
#endif
            return res;
        }
        if (!strcmp(method_name, "drawLine") ||
            !strcmp(method_name, "drawBitmap") || !strcmp(method_name, "drawPath") ||
            !strcmp(method_name, "drawOval") ||
            !strcmp(method_name, "drawRoundRect") || !strcmp(method_name, "clipRect") ||
            !strcmp(method_name, "save") || !strcmp(method_name, "restore") ||
            !strcmp(method_name, "translate") || !strcmp(method_name, "scale") ||
            !strcmp(method_name, "rotate") || !strcmp(method_name, "concat") ||
            !strcmp(method_name, "setMatrix")) {
            return res;
        }
        if (!strcmp(method_name, "drawArc") && nargs >= 5) {
#ifdef __APPLE__
            /* drawArc(RectF oval, float startAngle, float sweepAngle, boolean useCenter, Paint) */
            AineObj *rectf  = args[0];
            float start_deg = arg_float(args[1]);
            float sweep_deg = arg_float(args[2]);
            int   use_center = (int)arg_prim(args[3]);
            AineObj *paint  = args[4];
            uint32_t color  = paint ? (uint32_t)heap_iget_prim(paint, "color") : 0xFF888888u;
            if ((color & 0xFF000000u) == 0) color |= 0xFF000000u;
            if (rectf) {
                union { int64_t i; float f; } lv, tv, rv, bv;
                lv.i = heap_iget_prim(rectf, "left");
                tv.i = heap_iget_prim(rectf, "top");
                rv.i = heap_iget_prim(rectf, "right");
                bv.i = heap_iget_prim(rectf, "bottom");
                int64_t style = paint ? heap_iget_prim(paint, "style") : 0LL;
                if (style == 1 /* STROKE */) {
                    union { int64_t i; float f; } sw; sw.i = paint ? heap_iget_prim(paint, "strokewidth") : 0LL;
                    aine_canvas_stroke_arc(lv.f, tv.f, rv.f, bv.f,
                                          start_deg, sweep_deg, use_center, sw.f, color);
                } else {
                    aine_canvas_draw_arc(lv.f, tv.f, rv.f, bv.f,
                                        start_deg, sweep_deg, use_center, color);
                }
            }
#endif
            return res;
        }
        if (!strcmp(method_name, "getWidth"))  { res.is_void = 0; res.prim = 800; return res; }
        if (!strcmp(method_name, "getHeight")) { res.is_void = 0; res.prim = 600; return res; }
        /* Color static */
        if (!strcmp(method_name, "rgb") && nargs >= 3) {
            int64_t r = arg_prim(args[0]);
            int64_t g = arg_prim(args[1]);
            int64_t b = arg_prim(args[2]);
            res.is_void = 0; res.prim = 0xFF000000 | (r<<16) | (g<<8) | b; return res;
        }
        if (!strcmp(method_name, "argb") && nargs >= 4) {
            int64_t a = arg_prim(args[0]);
            int64_t r = arg_prim(args[1]);
            int64_t g = arg_prim(args[2]);
            int64_t b = arg_prim(args[3]);
            res.is_void = 0; res.prim = (a<<24) | (r<<16) | (g<<8) | b; return res;
        }
        if (!strcmp(method_name, "parseColor") && nargs >= 1) {
            res.is_void = 0; res.prim = 0xFF000000; return res;
        }
        /* Bitmap */
        if (!strcmp(method_name, "createBitmap") || !strcmp(method_name, "decodeResource") ||
            !strcmp(method_name, "decodeFile") || !strcmp(method_name, "decodeStream") ||
            !strcmp(method_name, "createScaledBitmap")) {
            AineObj *bmp = calloc(1, sizeof(AineObj));
            bmp->type = OBJ_USERCLASS;
            bmp->class_desc = "Landroid/graphics/Bitmap;";
            res.is_void = 0; res.obj = bmp; return res;
        }
        if (!strcmp(method_name, "recycle") || !strcmp(method_name, "isRecycled") ||
            !strcmp(method_name, "compress")) {
            return res;
        }
        /* Path */
        if (!strcmp(method_name, "moveTo") || !strcmp(method_name, "lineTo") ||
            !strcmp(method_name, "quadTo") || !strcmp(method_name, "cubicTo") ||
            !strcmp(method_name, "arcTo") || !strcmp(method_name, "close") ||
            !strcmp(method_name, "reset") || !strcmp(method_name, "addRect") ||
            !strcmp(method_name, "addCircle") || !strcmp(method_name, "addArc") ||
            !strcmp(method_name, "setFillType")) {
            return res;
        }
        /* RectF/Rect */
        if (!strcmp(method_name, "set") || !strcmp(method_name, "offset") ||
            !strcmp(method_name, "inset") || !strcmp(method_name, "union") ||
            !strcmp(method_name, "intersect") || !strcmp(method_name, "contains") ||
            !strcmp(method_name, "isEmpty")) {
            return res;
        }
        return res;
    }

    // ── android.content.res.Resources (improved) ─────────────────────────
    if (strstr(class_desc, "android/content/res/Resources") ||
        strstr(class_desc, "android/content/res/AssetManager")) {
        if (!strcmp(method_name, "getString") || !strcmp(method_name, "getText") ||
            !strcmp(method_name, "getQuantityString")) {
            res.is_void = 0; res.obj = heap_string(""); return res;
        }
        if (!strcmp(method_name, "getInteger") || !strcmp(method_name, "getDimensionPixelSize") ||
            !strcmp(method_name, "getDimensionPixelOffset") || !strcmp(method_name, "getColor")) {
            res.is_void = 0; res.prim = 0; return res;
        }
        if (!strcmp(method_name, "getDimension")) {
            res.is_void = 0; res.prim = 0; return res; /* float as int bits */
        }
        if (!strcmp(method_name, "getDrawable") || !strcmp(method_name, "getDrawableForDensity")) {
            res.is_void = 0; res.obj = NULL; return res;
        }
        if (!strcmp(method_name, "getStringArray") || !strcmp(method_name, "getTextArray")) {
            AineObj *arr = calloc(1, sizeof(AineObj));
            arr->type = OBJ_ARRAY; arr->arr_len = 0;
            res.is_void = 0; res.obj = arr; return res;
        }
        if (!strcmp(method_name, "open") || !strcmp(method_name, "openFd")) {
            res.is_void = 0; res.obj = NULL; return res;
        }
        return res;
    }

    // ── android.content.res.Configuration ───────────────────────────────
    if (strstr(class_desc, "android/content/res/Configuration")) {
        return res; /* all no-op */
    }

    // ── android.os.Build / Build.VERSION ────────────────────────────────
    if (strstr(class_desc, "android/os/Build")) {
        if (!strcmp(method_name, "SDK_INT") || !strcmp(method_name, "getSDK_INT")) {
            res.is_void = 0; res.prim = 35; return res;
        }
        return res;
    }

    // ── android.util.DisplayMetrics / TypedValue ──────────────────────────
    if (strstr(class_desc, "android/util/DisplayMetrics") ||
        strstr(class_desc, "android/util/TypedValue")) {
        if (!strcmp(method_name, "<init>")) return res;
        if (!strcmp(method_name, "getDensity") || !strcmp(method_name, "density")) {
            res.is_void = 0; res.prim = 2; return res; /* xhdpi */
        }
        return res;
    }

    // ── android.app.NotificationManager / NotificationChannel ───────────
    if (strstr(class_desc, "android/app/Notification")) {
        return res; /* all no-op stubs */
    }

    // ── java.util.concurrent.* ───────────────────────────────────────────
    if (strstr(class_desc, "java/util/concurrent/") ||
        strstr(class_desc, "java/util/concurrent/atomic/")) {
        if (!strcmp(method_name, "<init>")) return res;
        if (!strcmp(method_name, "get")) {
            AineObj *v = this_obj ? heap_iget_obj(this_obj, "__val__") : NULL;
            res.is_void = 0; res.obj = v; return res;
        }
        if (!strcmp(method_name, "set") && nargs >= 1) {
            if (this_obj) heap_iput_obj(this_obj, "__val__", args[0]);
            return res;
        }
        if (!strcmp(method_name, "getAndIncrement") ||
            !strcmp(method_name, "incrementAndGet") ||
            !strcmp(method_name, "decrementAndGet")) {
            res.is_void = 0; res.prim = 0; return res;
        }
        if (!strcmp(method_name, "submit") || !strcmp(method_name, "execute")) {
            if (nargs >= 1) handler_post_delayed(args[0], 0);
            return res;
        }
        if (!strcmp(method_name, "shutdown") || !strcmp(method_name, "shutdownNow")) {
            return res;
        }
        return res;
    }

    // ── java.util.regex.* ────────────────────────────────────────────────
    if (strstr(class_desc, "java/util/regex/")) {
        if (!strcmp(method_name, "compile") && nargs >= 1) {
            AineObj *pat = calloc(1, sizeof(AineObj));
            pat->type = OBJ_USERCLASS;
            pat->class_desc = "Ljava/util/regex/Pattern;";
            heap_iput_obj(pat, "pattern", args[0] ? args[0] : heap_string(""));
            res.is_void = 0; res.obj = pat; return res;
        }
        if (!strcmp(method_name, "matcher") && nargs >= 1) {
            AineObj *m = calloc(1, sizeof(AineObj));
            m->type = OBJ_USERCLASS;
            m->class_desc = "Ljava/util/regex/Matcher;";
            heap_iput_obj(m, "input", args[0] ? args[0] : heap_string(""));
            res.is_void = 0; res.obj = m; return res;
        }
        if (!strcmp(method_name, "matches") || !strcmp(method_name, "find")) {
            res.is_void = 0; res.prim = 0; return res;
        }
        if (!strcmp(method_name, "group")) {
            res.is_void = 0; res.obj = heap_string(""); return res;
        }
        if (!strcmp(method_name, "replaceAll") && nargs >= 1) {
            AineObj *in = this_obj ? heap_iget_obj(this_obj, "input") : NULL;
            res.is_void = 0; res.obj = in ? in : heap_string(""); return res;
        }
        return res;
    }

    // ── java.util.Scanner ────────────────────────────────────────────────
    if (strstr(class_desc, "java/util/Scanner")) {
        if (!strcmp(method_name, "<init>")) return res;
        if (!strcmp(method_name, "nextLine") || !strcmp(method_name, "next")) {
            res.is_void = 0; res.obj = heap_string(""); return res;
        }
        if (!strcmp(method_name, "hasNextLine") || !strcmp(method_name, "hasNext")) {
            res.is_void = 0; res.prim = 0; return res;
        }
        if (!strcmp(method_name, "close")) return res;
        return res;
    }

    // ── java.io.* streams ────────────────────────────────────────────────
    if (strstr(class_desc, "java/io/") &&
        (strstr(class_desc, "Stream") || strstr(class_desc, "Reader") ||
         strstr(class_desc, "Writer") || strstr(class_desc, "Buffer"))) {
        if (!strcmp(method_name, "<init>")) return res;
        if (!strcmp(method_name, "close") || !strcmp(method_name, "flush")) return res;
        if (!strcmp(method_name, "write") || !strcmp(method_name, "print") ||
            !strcmp(method_name, "println")) {
            if (nargs >= 1 && args[0] && args[0]->str)
                fprintf(stdout, "%s", args[0]->str);
            return res;
        }
        if (!strcmp(method_name, "read")) { res.is_void = 0; res.prim = -1; return res; }
        if (!strcmp(method_name, "readLine")) { res.is_void = 0; res.obj = NULL; return res; }
        return res;
    }
    if (strstr(class_desc, "java/lang/") &&
        (strstr(class_desc, "Exception") || strstr(class_desc, "Error") ||
         strstr(class_desc, "Throwable"))) {
        if (!strcmp(method_name, "<init>")) {
            if (this_obj && nargs >= 1 && args[0])
                heap_iput_obj(this_obj, "message", args[0]);
            return res;
        }
        if (!strcmp(method_name, "getMessage") || !strcmp(method_name, "toString")) {
            AineObj *msg = this_obj ? heap_iget_obj(this_obj, "message") : NULL;
            res.is_void = 0; res.obj = msg ? msg : heap_string(class_desc); return res;
        }
        if (!strcmp(method_name, "printStackTrace")) {
            AineObj *msg = this_obj ? heap_iget_obj(this_obj, "message") : NULL;
            fprintf(stderr, "[aine-dalvik] exception %s: %s\n", class_desc,
                    msg && msg->str ? msg->str : "(no message)");
            return res;
        }
        if (!strcmp(method_name, "getClass")) {
            res.is_void = 0; res.obj = heap_string(class_desc); return res;
        }
        return res;
    }

    // ── Generic <init> fallback for all other unrecognized classes ───────
    if (strcmp(method_name, "<init>") == 0) { res.is_void = 1; return res; }

    // ── Class ───────────────────────────────────────────────────────────
    if (strcmp(class_desc, "Ljava/lang/Class;") == 0) {
        if (strcmp(method_name, "getName") == 0 || strcmp(method_name, "getSimpleName") == 0) {
            res.is_void = 0; res.obj = heap_string(class_desc); return res;
        }
        if (strcmp(method_name, "forName") == 0 && nargs >= 1) {
            res.is_void = 0; res.obj = args[0]; return res;
        }
    }

    /* Generic <init> fallback — silently no-op for any unrecognized class */
    if (strcmp(method_name, "<init>") == 0) { res.is_void = 1; return res; }

    /* ── Generic inherited View/Activity methods on user-defined subclasses ──
     * When a user class (SampleView, etc.) inherits View/Activity methods that
     * are not in DEX, they fall here. Handle key ones by name. */
    if (!strcmp(method_name, "invalidate")) {
        /* Any View.invalidate() call → mark content view dirty for redraw */
        g_view_dirty = 1;
        return res;
    }
    if (!strcmp(method_name, "getWidth"))    { res.is_void = 0; res.prim = 800; return res; }
    if (!strcmp(method_name, "getHeight"))   { res.is_void = 0; res.prim = 600; return res; }
    if (!strcmp(method_name, "requestFocus") || !strcmp(method_name, "setFocusable") ||
        !strcmp(method_name, "setFocusableInTouchMode")) {
        res.is_void = 0; res.prim = 1; return res;
    }
    if (!strcmp(method_name, "onStart")  || !strcmp(method_name, "onResume") ||
        !strcmp(method_name, "onPause")  || !strcmp(method_name, "onStop")   ||
        !strcmp(method_name, "onDestroy")|| !strcmp(method_name, "onSaveInstanceState")) {
        return res;  /* lifecycle no-ops for user Activity subclasses */
    }

    /* Generic Activity-inherited methods for user subclasses (e.g. MyActivity extends Activity) */
    if (!strcmp(method_name, "setContentView")) {
        if (nargs >= 1 && args[0]) {
            if (args[0]->type == OBJ_USERCLASS) {
                g_content_view = args[0];
                g_view_dirty   = 1;
            } else {
                uint32_t res_id2 = (uint32_t)arg_prim(args[0]);
                if (g_res_map) {
                    AineViewNode *root2 = aine_layout_inflate(g_res_map, res_id2);
                    if (root2) {
                        if (g_layout_root) aine_layout_free(g_layout_root);
                        g_layout_root = root2;
                        static AineObj s_layout_cv2 = {
                            .type = OBJ_USERCLASS, .class_desc = "Laine/Layout;" };
                        g_content_view = &s_layout_cv2;
                        g_view_dirty   = 1;
                    }
                }
            }
        }
        return res;
    }
    if (!strcmp(method_name, "finish")) {
        extern void aine_activity_request_finish(void);
        aine_activity_request_finish(); return res;
    }
    if (!strcmp(method_name, "getWindow") || !strcmp(method_name, "getWindowManager") ||
        !strcmp(method_name, "getSystemService")) {
        res.is_void = 0; res.obj = this_obj; return res;
    }
    if (!strcmp(method_name, "requireViewById") || !strcmp(method_name, "findViewById")) {
        AineObj *v = calloc(1, sizeof(AineObj)); v->type = OBJ_USERCLASS;
        v->class_desc = "Landroid/view/View;";
        if (nargs >= 1 && args[0]) {
            heap_iput_obj(v, "__resid__", args[0]);
            register_view_stub((int)arg_prim(args[0]), v);
        }
        res.is_void = 0; res.obj = v; return res;
    }
    if (!strcmp(method_name, "getLayoutInflater") || !strcmp(method_name, "getMenuInflater")) {
        static AineObj g_inflater2 = { .type = OBJ_NULL };
        g_inflater2.class_desc = "Landroid/view/LayoutInflater;";
        res.is_void = 0; res.obj = &g_inflater2; return res;
    }

    // ── Kotlin jvm.internal.Intrinsics (all assertion/throw → no-op) ──────
    if (strstr(class_desc, "kotlin/jvm/internal/") ||
        strstr(class_desc, "kotlin/internal/")) {
        return res;  /* no-op: checkNotNull, throwUninitializedProperty, etc. */
    }

    // ── Kotlin stdlib: TuplesKt, CollectionsKt ───────────────────────────
    if (strstr(class_desc, "kotlin/TuplesKt") ||
        strstr(class_desc, "kotlin/collections/") ||
        strstr(class_desc, "kotlin/")) {
        if (!strcmp(method_name, "to") && nargs >= 2) {
            /* a.to(b) → Pair(first=a, second=b) */
            AineObj *pair = calloc(1, sizeof(AineObj));
            pair->type = OBJ_USERCLASS; pair->class_desc = "Lkotlin/Pair;";
            heap_iput_obj(pair, "first",  args[0]);
            heap_iput_obj(pair, "second", args[1]);
            res.is_void = 0; res.obj = pair; return res;
        }
        if (!strcmp(method_name, "listOf")) {
            AineObj *list = heap_arraylist_new();
            if (nargs == 1 && args[0] && args[0]->type == OBJ_ARRAY) {
                for (int i = 0; i < args[0]->arr_len; i++)
                    heap_arraylist_add(list, args[0]->arr_obj[i]);
            } else {
                for (int i = 0; i < nargs && i < 8; i++)
                    if (args[i]) heap_arraylist_add(list, args[i]);
            }
            res.is_void = 0; res.obj = list; return res;
        }
        if (!strcmp(method_name, "mapOf") || !strcmp(method_name, "setOf") ||
            !strcmp(method_name, "emptyList") || !strcmp(method_name, "emptyMap")) {
            res.is_void = 0; res.obj = heap_arraylist_new(); return res;
        }
        if (!strcmp(method_name, "mutableStateOf") || !strcmp(method_name, "remember")) {
            /* Compose/state stubs — return arg or stub */
            AineObj *sv = calloc(1, sizeof(AineObj));
            sv->type = OBJ_USERCLASS; sv->class_desc = class_desc;
            if (nargs >= 1 && args[0]) heap_iput_obj(sv, "value", args[0]);
            res.is_void = 0; res.obj = sv; return res;
        }
        /* Pair decomposition */
        if (!strcmp(method_name, "component1") || !strcmp(method_name, "getFirst")) {
            res.is_void = 0; res.obj = this_obj ? heap_iget_obj(this_obj, "first") : NULL; return res;
        }
        if (!strcmp(method_name, "component2") || !strcmp(method_name, "getSecond")) {
            res.is_void = 0; res.obj = this_obj ? heap_iget_obj(this_obj, "second") : NULL; return res;
        }
        return res;  /* other kotlin stdlib stubs → no-op */
    }

    // ── java.lang.Iterable / java.util.List iterator ─────────────────────
    if (!strcmp(class_desc, "Ljava/lang/Iterable;") ||
        !strcmp(class_desc, "Ljava/util/List;") ||
        !strcmp(class_desc, "Ljava/util/Collection;")) {
        if (!strcmp(method_name, "iterator") && this_obj) {
            res.is_void = 0; res.obj = heap_iterator_new(this_obj); return res;
        }
        return res;
    }

    // ── androidx (generic stub for remaining unhandled classes) ──────────
    // Note: most androidx/ calls are already handled by the android/view/View+androidx section above.
    // This section handles any com/google/ classes not already handled.
    if (strstr(class_desc, "androidx/") ||
        strstr(class_desc, "com/google/")) {
        if (!strcmp(method_name, "setContent") ||
            !strcmp(method_name, "setContent$default")) {
            fprintf(stderr, "[aine-ui] ComponentActivity.setContent called (Compose UI)\n");
        }
        return res;  /* all other stubs → no-op */
    }

    fprintf(stderr, "[aine-dalvik] unimplemented: %s->%s (nargs=%d)\n",
            class_desc, method_name, nargs);
    res.is_void = 0;
    res.obj     = NULL;
    return res;
}
