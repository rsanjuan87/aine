#pragma once
/*
 * aine-loader/path_map.h — Android .so path → macOS dylib path mapping
 *
 * When an APK calls dlopen("/system/lib64/liblog.so"), AINE redirects
 * to its own stub dylib (libaine-log.dylib) from AINE_LIB_DIR.
 */

/**
 * path_map_init - set the directory where AINE stub dylibs live.
 * @dir: path to directory containing libaine-log.dylib etc.
 *       If NULL, uses the directory of the current executable.
 */
void path_map_init(const char *dir);

/**
 * path_map_resolve - translate an Android library path to a macOS path.
 * @android_path: Android path, e.g. "/system/lib64/liblog.so"
 *                or a simple name, e.g. "liblog.so"
 *
 * Returns: a macOS-compatible path ready for dlopen(), or NULL if the
 *          path has no registered mapping (caller should use as-is).
 *
 * The returned pointer is valid until the next call to path_map_init().
 */
const char *path_map_resolve(const char *android_path);
