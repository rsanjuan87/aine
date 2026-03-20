/*
 * aine-loader/path_map.c — Android .so path → macOS dylib path mapping
 */
#include "path_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

/* Maximum path length for constructed dylib paths */
#define PATH_MAX_LEN 1024

static char s_lib_dir[PATH_MAX_LEN];
static char s_resolved[PATH_MAX_LEN];  /* scratch buffer for resolved paths */

/* Table: Android basename → AINE stub dylib name */
static const struct {
    const char *android_name;   /* e.g. "liblog.so" */
    const char *aine_dylib;     /* e.g. "libaine-log.dylib" */
} k_map[] = {
    { "liblog.so",             "libaine-log.dylib"     },
    { "libandroid.so",         "libaine-android.dylib" },
    /* EGL/GLES: map to ANGLE when available, or system frameworks */
    { "libEGL.so",             "libaine-egl.dylib"     },
    { "libGLESv1_CM.so",       "libaine-gles1.dylib"   },
    { "libGLESv2.so",          "libaine-gles2.dylib"   },
    { "libGLESv3.so",          "libaine-gles2.dylib"   },
    /* Android standard libs that map to macOS system libs */
    { "libc.so",               "/usr/lib/libSystem.B.dylib" },
    { "libm.so",               "/usr/lib/libSystem.B.dylib" },
    { "libdl.so",              "/usr/lib/libSystem.B.dylib" },
    { "libpthread.so",         "/usr/lib/libSystem.B.dylib" },
    { "libz.so",               "/usr/lib/libz.dylib"        },
    { NULL, NULL }
};

/* Return just the basename of a path (last '/' component) */
static const char *basename_of(const char *path)
{
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

void path_map_init(const char *dir)
{
    if (dir) {
        strncpy(s_lib_dir, dir, sizeof(s_lib_dir) - 1);
        s_lib_dir[sizeof(s_lib_dir) - 1] = '\0';
        return;
    }

    /* Check AINE_LIB_DIR env var first */
    const char *env = getenv("AINE_LIB_DIR");
    if (env && env[0]) {
        strncpy(s_lib_dir, env, sizeof(s_lib_dir) - 1);
        s_lib_dir[sizeof(s_lib_dir) - 1] = '\0';
        return;
    }

#ifdef __APPLE__
    /* Default: same directory as the running executable */
    uint32_t sz = sizeof(s_lib_dir);
    if (_NSGetExecutablePath(s_lib_dir, &sz) == 0) {
        char *slash = strrchr(s_lib_dir, '/');
        if (slash) *slash = '\0';
        return;
    }
#endif

    /* Fallback: current directory */
    strncpy(s_lib_dir, ".", sizeof(s_lib_dir) - 1);
}

const char *path_map_resolve(const char *android_path)
{
    if (!android_path) return NULL;

    const char *base = basename_of(android_path);

    for (int i = 0; k_map[i].android_name != NULL; i++) {
        if (strcmp(base, k_map[i].android_name) != 0) continue;

        const char *dylib = k_map[i].aine_dylib;

        /* Absolute paths are returned as-is (e.g. /usr/lib/libSystem.B.dylib) */
        if (dylib[0] == '/') {
            return dylib;
        }

        /* Relative: prepend AINE lib dir */
        snprintf(s_resolved, sizeof(s_resolved), "%s/%s", s_lib_dir, dylib);
        return s_resolved;
    }

    return NULL;  /* not mapped — caller uses original path */
}
