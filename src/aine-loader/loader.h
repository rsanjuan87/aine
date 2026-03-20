#pragma once
/*
 * aine-loader/loader.h — dlopen wrapper with Android path mapping
 *
 * Usage:
 *   loader_set_app_lib_dir("/tmp/aine/com.example.app/lib");
 *   void *h = aine_dlopen("libfoo.so");
 *   void (*fn)() = aine_dlsym(h, "JNI_OnLoad");
 */

/**
 * loader_set_app_lib_dir - directory where APK's extracted native libs live.
 * @dir: e.g. "/tmp/aine/com.example.app/lib"
 *        Pass NULL to clear.
 */
void loader_set_app_lib_dir(const char *dir);

/**
 * aine_dlopen - dlopen with AINE path mapping.
 *   Resolution order:
 *   1. app lib dir: <app_lib_dir>/<basename>
 *   2. AINE stub dylib via path_map_resolve()
 *   3. Original path as-is
 *
 * Returns: handle or NULL (check dlerror()).
 */
void *aine_dlopen(const char *android_path);

/**
 * aine_dlsym - dlsym wrapper (pass-through, for future symbol hooking).
 */
void *aine_dlsym(void *handle, const char *symbol);

/**
 * aine_dlclose - dlclose wrapper.
 */
int aine_dlclose(void *handle);
