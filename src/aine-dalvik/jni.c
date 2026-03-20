// aine-dalvik/jni.c — Native method bridges for core Android/Java APIs
// Implements just enough of java.lang.* and java.io.* for M1 HelloWorld
#include "jni.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ── Static field singletons ──────────────────────────────────────────────
static AineObj g_system_out = { .type = OBJ_PRINTSTREAM };

AineObj *jni_sget_object(const char *class_desc, const char *field_name) {
    if (strcmp(class_desc, "Ljava/lang/System;") == 0 &&
        strcmp(field_name, "out") == 0) {
        return &g_system_out;
    }
    fprintf(stderr, "[aine-dalvik] sget-object: unknown field %s->%s\n",
            class_desc, field_name);
    return NULL;
}

// ── System.getProperty ───────────────────────────────────────────────────
static AineObj *system_get_property(const char *key) {
    if (!key) return heap_string("null");

    // Match ART's actual return values
    if (strcmp(key, "java.version")           == 0) return heap_string("0");
    if (strcmp(key, "os.arch")                == 0) return heap_string("aarch64");
    if (strcmp(key, "os.name")                == 0) return heap_string("Linux");  // as seen by apps
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

// ── Method dispatch ──────────────────────────────────────────────────────
JniResult jni_dispatch(const char *class_desc,
                       const char *method_name,
                       AineObj    *this_obj,
                       AineObj   **args,
                       int         nargs,
                       int         is_static) {
    JniResult res = { .is_void = 1, .obj = NULL, .prim = 0 };

    // ── PrintStream.println(String) ──────────────────────────────────────
    if (strcmp(class_desc, "Ljava/io/PrintStream;") == 0 &&
        strcmp(method_name, "println") == 0 && nargs >= 1) {
        const char *s = (args[0] && args[0]->str) ? args[0]->str : "null";
        printf("%s\n", s);
        fflush(stdout);
        res.is_void = 1;
        return res;
    }

    // ── System.getProperty(String) ──────────────────────────────────────
    if (strcmp(class_desc, "Ljava/lang/System;") == 0 &&
        strcmp(method_name, "getProperty") == 0 && nargs >= 1 && is_static) {
        const char *key = (args[0] && args[0]->str) ? args[0]->str : "";
        res.is_void = 0;
        res.obj     = system_get_property(key);
        return res;
    }

    // ── StringBuilder.<init>() ───────────────────────────────────────────
    if (strcmp(class_desc, "Ljava/lang/StringBuilder;") == 0 &&
        strcmp(method_name, "<init>") == 0) {
        // this_obj was already new-instance'd; initialise its SB state
        if (this_obj && this_obj->type == OBJ_STRINGBUILDER) {
            // Already initialised by heap_sb_new in new-instance handler
        }
        res.is_void = 1;
        return res;
    }

    // ── StringBuilder.append(String) ────────────────────────────────────
    if (strcmp(class_desc, "Ljava/lang/StringBuilder;") == 0 &&
        strcmp(method_name, "append") == 0 && nargs >= 1) {
        const char *s = (args[0] && args[0]->str) ? args[0]->str : "null";
        if (this_obj) heap_sb_append(this_obj, s);
        res.is_void = 0;
        res.obj     = this_obj;
        return res;
    }

    // ── StringBuilder.toString() ─────────────────────────────────────────
    if (strcmp(class_desc, "Ljava/lang/StringBuilder;") == 0 &&
        strcmp(method_name, "toString") == 0) {
        res.is_void = 0;
        res.obj     = heap_sb_tostring(this_obj);
        return res;
    }

    // ── Object.<init>() — no-op ──────────────────────────────────────────
    if (strcmp(method_name, "<init>") == 0) {
        res.is_void = 1;
        return res;
    }

    fprintf(stderr, "[aine-dalvik] unimplemented: %s->%s (nargs=%d)\n",
            class_desc, method_name, nargs);
    res.is_void = 0;
    res.obj     = NULL;
    return res;
}
