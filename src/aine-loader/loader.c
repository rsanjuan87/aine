/*
 * aine-loader/loader.c — dlopen wrapper with Android path mapping
 */
#include "loader.h"
#include "path_map.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#define PATH_MAX_LEN 1024

static char s_app_lib_dir[PATH_MAX_LEN];
static int  s_app_lib_dir_set = 0;

void loader_set_app_lib_dir(const char *dir)
{
    if (!dir) {
        s_app_lib_dir_set = 0;
        s_app_lib_dir[0] = '\0';
        return;
    }
    strncpy(s_app_lib_dir, dir, sizeof(s_app_lib_dir) - 1);
    s_app_lib_dir[sizeof(s_app_lib_dir) - 1] = '\0';
    s_app_lib_dir_set = 1;
}

/* Return just the basename */
static const char *basename_of(const char *p)
{
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

void *aine_dlopen(const char *android_path)
{
    if (!android_path) return NULL;

    char candidate[PATH_MAX_LEN];
    void *handle = NULL;

    /* 1. App lib dir: look for extracted .so from APK */
    if (s_app_lib_dir_set) {
        const char *base = basename_of(android_path);
        snprintf(candidate, sizeof(candidate), "%s/%s", s_app_lib_dir, base);
        handle = dlopen(candidate, RTLD_LAZY | RTLD_LOCAL);
        if (handle) {
            fprintf(stderr, "[aine-loader] dlopen: %s -> %s\n", android_path, candidate);
            return handle;
        }
    }

    /* 2. AINE stub dylib via path_map */
    const char *mapped = path_map_resolve(android_path);
    if (mapped) {
        handle = dlopen(mapped, RTLD_LAZY | RTLD_LOCAL);
        if (handle) {
            fprintf(stderr, "[aine-loader] dlopen: %s -> %s (mapped)\n",
                    android_path, mapped);
            return handle;
        }
        fprintf(stderr, "[aine-loader] warn: mapped path %s failed: %s\n",
                mapped, dlerror());
    }

    /* 3. Original path as-is */
    handle = dlopen(android_path, RTLD_LAZY | RTLD_LOCAL);
    if (handle) {
        fprintf(stderr, "[aine-loader] dlopen: %s (direct)\n", android_path);
    } else {
        fprintf(stderr, "[aine-loader] error: dlopen %s: %s\n",
                android_path, dlerror());
    }
    return handle;
}

void *aine_dlsym(void *handle, const char *symbol)
{
    return dlsym(handle, symbol);
}

int aine_dlclose(void *handle)
{
    return dlclose(handle);
}
