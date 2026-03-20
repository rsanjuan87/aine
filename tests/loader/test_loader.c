/*
 * tests/loader/test_loader.c — CTest suite for aine-loader + liblog + libandroid
 *
 * Tests:
 *   T1: path_map_resolve — static mapping table
 *   T2: aine_dlopen libaine-log.dylib + call __android_log_print
 *   T3: aine_dlopen libaine-native-stub.dylib + call aine_native_test()
 *   T4: path_map_resolve for system libc maps to /usr/lib/libSystem.B.dylib
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "path_map.h"
#include "loader.h"

/* AINE_BUILD_DIR passed via -D from CMake (see CMakeLists.txt) */
#ifndef AINE_BUILD_DIR
#define AINE_BUILD_DIR "."
#endif

static int failures = 0;

#define PASS(msg)  do { printf("  [PASS] %s\n", msg); } while(0)
#define FAIL(msg)  do { printf("  [FAIL] %s\n", msg); failures++; } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(msg); else FAIL(msg); } while(0)

/* ------------------------------------------------------------------ */
static void t1_path_map_static(void)
{
    printf("T1: path_map_resolve — static table\n");

    /* Known Android paths should map to AINE stubs */
    const char *r;

    r = path_map_resolve("/system/lib64/liblog.so");
    CHECK(r != NULL, "liblog.so has a mapping");
    if (r) CHECK(strstr(r, "aine-log") != NULL, "liblog maps to aine-log");

    r = path_map_resolve("/system/lib64/libandroid.so");
    CHECK(r != NULL, "libandroid.so has a mapping");
    if (r) CHECK(strstr(r, "aine-android") != NULL, "libandroid maps to aine-android");

    r = path_map_resolve("liblog.so");  /* simple name */
    CHECK(r != NULL, "liblog.so (simple name) has a mapping");

    r = path_map_resolve("/system/lib64/libc.so");
    CHECK(r != NULL, "libc.so has a mapping");
    if (r) CHECK(strstr(r, "libSystem") != NULL, "libc maps to libSystem");

    r = path_map_resolve("/data/app/com.example/lib/arm64/libunknown.so");
    CHECK(r == NULL, "unknown .so returns NULL (no mapping)");
}

/* ------------------------------------------------------------------ */
static void t2_dlopen_liblog(void)
{
    printf("T2: aine_dlopen libaine-log.dylib + __android_log_print\n");

    /* Tell the loader where AINE stub dylibs live */
    path_map_init(AINE_BUILD_DIR);

    void *handle = aine_dlopen("/system/lib64/liblog.so");
    CHECK(handle != NULL, "aine_dlopen /system/lib64/liblog.so succeeded");
    if (!handle) return;

    /* Resolve __android_log_print */
    typedef int (*log_print_t)(int, const char *, const char *, ...);
    log_print_t fn = (log_print_t)(uintptr_t)aine_dlsym(handle, "__android_log_print");
    CHECK(fn != NULL, "__android_log_print symbol found");

    if (fn) {
        int n = fn(4 /*INFO*/, "test_loader", "hello from aine-log stub");
        CHECK(n > 0, "__android_log_print returned positive bytes");
    }

    aine_dlclose(handle);
}

/* ------------------------------------------------------------------ */
static void t3_dlopen_native_stub(void)
{
    printf("T3: aine_dlopen libaine-native-stub.dylib + aine_native_test\n");

    /* Direct path to the test stub (no path-map, explicit path) */
    char stub_path[1024];
    snprintf(stub_path, sizeof(stub_path),
             "%s/libaine-native-stub.dylib", AINE_BUILD_DIR);

    void *handle = dlopen(stub_path, RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        printf("  [SKIP] libaine-native-stub.dylib not found at %s: %s\n",
               stub_path, dlerror());
        return;
    }
    PASS("dlopen libaine-native-stub.dylib");

    typedef int (*test_fn_t)(void);
    test_fn_t fn = (test_fn_t)(uintptr_t)dlsym(handle, "aine_native_test");
    CHECK(fn != NULL, "aine_native_test symbol found");

    if (fn) {
        int result = fn();
        CHECK(result == 42, "aine_native_test() == 42");
    }

    dlclose(handle);
}

/* ------------------------------------------------------------------ */
static void t4_path_map_libz(void)
{
    printf("T4: path_map_resolve /system/lib64/libz.so -> /usr/lib/libz.dylib\n");

    const char *r = path_map_resolve("/system/lib64/libz.so");
    CHECK(r != NULL, "libz.so has a mapping");
    if (r) {
        CHECK(strstr(r, "libz") != NULL, "libz maps to something with 'libz'");
        /* Verify the mapped path is actually loadable */
        void *h = dlopen(r, RTLD_LAZY | RTLD_NOLOAD);
        if (!h) h = dlopen(r, RTLD_LAZY | RTLD_LOCAL);
        CHECK(h != NULL, "mapped libz path is loadable");
        if (h) dlclose(h);
    }
}

/* ------------------------------------------------------------------ */
int main(void)
{
    printf("=== aine-loader tests ===\n");
    printf("Build dir: %s\n\n", AINE_BUILD_DIR);

    /* Initialise with build dir so libaine-*.dylib are found */
    path_map_init(AINE_BUILD_DIR);

    t1_path_map_static();
    t2_dlopen_liblog();
    t3_dlopen_native_stub();
    t4_path_map_libz();

    printf("\n=== Result: %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
