// aine-dalvik/jni.h — Native method bridge
// Maps Android/Java method signatures to native macOS implementations
#pragma once
#include "heap.h"

// Result union for jni_dispatch
typedef struct {
    int      is_void;
    AineObj *obj;
    int64_t  prim;
} JniResult;

// Dispatch a virtual/static/direct method call
// class_desc:  "Ljava/io/PrintStream;"
// method_name: "println"
// shorty:      "VL"  (V = void return, L = object arg)
// this_obj:    receiver object (NULL for static)
// args:        array of object args (parallel to arg portion of shorty)
// Returns a JniResult
JniResult jni_dispatch(const char *class_desc,
                       const char *method_name,
                       AineObj    *this_obj,
                       AineObj   **args,
                       int         nargs,
                       int         is_static);

// Get a static object field
// class_desc:  "Ljava/lang/System;"
// field_name:  "out"
AineObj *jni_sget_object(const char *class_desc, const char *field_name);

// Get a static primitive field (Integer.MAX_VALUE etc.)
int64_t  jni_sget_prim(const char *class_desc, const char *field_name);

/* Content view for onDraw dispatch (set by Activity.setContentView(View)) */
struct AineObj;
struct AineObj *jni_get_content_view(void);
int             jni_pop_invalidated(void);
