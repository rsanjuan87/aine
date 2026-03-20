/*
 * aine-loader CLI tool — manual test for path mapping and dlopen
 * Usage: aine-loader-test <android_path_or_lib> [symbol_name]
 */
#include "loader.h"
#include "path_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <lib_path> [symbol]\n", argv[0]);
        fprintf(stderr, "  lib_path: Android path or simple name, e.g.:\n");
        fprintf(stderr, "    /system/lib64/liblog.so\n");
        fprintf(stderr, "    /data/app/com.example/lib/arm64/libnative.so\n");
        return 1;
    }

    /* Init path map using executable directory */
    path_map_init(NULL);

    const char *lib_path = argv[1];
    const char *symbol   = argc >= 3 ? argv[2] : NULL;

    /* Show mapping */
    const char *mapped = path_map_resolve(lib_path);
    if (mapped) {
        printf("[aine-loader] path map: %s -> %s\n", lib_path, mapped);
    } else {
        printf("[aine-loader] path map: %s -> (no mapping, direct)\n", lib_path);
    }

    /* Try to open */
    void *handle = aine_dlopen(lib_path);
    if (!handle) {
        fprintf(stderr, "[aine-loader] FAIL: could not open library\n");
        return 2;
    }
    printf("[aine-loader] dlopen: ok (handle=%p)\n", handle);

    /* Resolve symbol if requested */
    if (symbol) {
        void *sym = aine_dlsym(handle, symbol);
        if (!sym) {
            fprintf(stderr, "[aine-loader] symbol '%s': NOT FOUND\n", symbol);
            aine_dlclose(handle);
            return 3;
        }
        printf("[aine-loader] symbol '%s' found at %p\n", symbol, sym);

        /* If the symbol is named aine_native_test, call it */
        if (strcmp(symbol, "aine_native_test") == 0) {
            typedef int (*test_fn_t)(void);
            test_fn_t fn = (test_fn_t)(uintptr_t)sym;
            int result = fn();
            printf("[aine-loader] aine_native_test() -> %d%s\n",
                   result, result == 42 ? " -> ok" : " -> unexpected");
        }
    }

    aine_dlclose(handle);
    return 0;
}
