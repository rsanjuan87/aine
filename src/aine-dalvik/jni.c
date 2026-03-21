// aine-dalvik/jni.c — Native method bridges for core Android/Java APIs
#include "jni.h"
#include "handler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

// ── Static field singletons ──────────────────────────────────────────────
static AineObj g_system_out = { .type = OBJ_PRINTSTREAM };
static AineObj g_system_err = { .type = OBJ_PRINTSTREAM };

AineObj *jni_sget_object(const char *class_desc, const char *field_name) {
    if (strcmp(class_desc, "Ljava/lang/System;") == 0) {
        if (strcmp(field_name, "out") == 0) return &g_system_out;
        if (strcmp(field_name, "err") == 0) return &g_system_err;
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
            if (nargs >= 1 && args[0]) code = (int)args[0]->arr_prim[0];
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

    // ── Integer ──────────────────────────────────────────────────────────
    if (strcmp(class_desc, "Ljava/lang/Integer;") == 0) {
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
            char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)(args[0] ? args[0]->arr_prim[0] : 0));
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
            int idx_ch = (int)(args[0] ? args[0]->arr_prim[0] : 0);
            res.is_void = 0; res.prim = (idx_ch >= 0 && idx_ch < (int)strlen(str_val)) ? (uint8_t)str_val[idx_ch] : 0;
            return res;
        }
        if (strcmp(method_name, "substring") == 0 && nargs >= 1) {
            int from = (int)(args[0] ? args[0]->arr_prim[0] : 0);
            const char *sub = (from >= 0 && from < (int)strlen(str_val)) ? str_val + from : "";
            res.is_void = 0; res.obj = heap_string(sub); return res;
        }
        if (strcmp(method_name, "toCharArray") == 0) {
            res.is_void = 0; res.obj = heap_array_new((int)strlen(str_val)); return res;
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
                else { snprintf(buf, sizeof(buf), "%lld", (long long)args[0]->arr_prim); s = buf; }
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
    if (strcmp(method_name, "<init>") == 0) { res.is_void = 1; return res; }
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
        if (!strcmp(method_name, "<init>")) { return res; }
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
            res.is_void = 0; res.obj = this_obj; return res; /* stub: self-iteration */
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
            strcmp(method_name, "setContentView") == 0 ||
            strcmp(method_name, "getSystemService") == 0 ||
            strcmp(method_name, "getResources") == 0 ||
            strcmp(method_name, "runOnUiThread") == 0) {
            return res;  // is_void = 1
        }
        if (strcmp(method_name, "getString") == 0) {
            res.is_void = 0; res.obj = heap_string(""); return res;
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
    if (strstr(class_desc, "java/lang/Thread") ||
        strstr(class_desc, "java/lang/Runnable")) {
        return res;  // stub all thread ops
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

    fprintf(stderr, "[aine-dalvik] unimplemented: %s->%s (nargs=%d)\n",
            class_desc, method_name, nargs);
    res.is_void = 0;
    res.obj     = NULL;
    return res;
}
