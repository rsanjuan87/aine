// aine-dalvik/jni.c — Native method bridges for core Android/Java APIs
#include "jni.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
            res.is_void = 0; res.prim = 0; return res; // stub: 0ms
        }
        if (strcmp(method_name, "nanoTime") == 0) {
            res.is_void = 0; res.prim = 0; return res;
        }
        if (strcmp(method_name, "gc") == 0) { return res; }
        if (strcmp(method_name, "arraycopy") == 0) { return res; } // stub no-op
    }

    // ── android.util.Log ────────────────────────────────────────────────
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
            /* Super-minimal format: just return the first arg (format string) */
            res.is_void = 0; res.obj = args[0]; return res;
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
        if (strcmp(method_name, "<init>") == 0 || strcmp(method_name, "postDelayed") == 0 ||
            strcmp(method_name, "post") == 0 || strcmp(method_name, "removeCallbacks") == 0) {
            return res;  // is_void = 1, no-op
        }
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
