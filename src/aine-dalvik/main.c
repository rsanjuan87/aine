// aine-dalvik/main.c — dalvikvm entry point
//
// Usage (compatible with real dalvikvm):
//   dalvikvm [-cp <dexfile>] <ClassName> [args...]
//   dalvikvm -Xnoimage-dex2oat -Xusejit:false -cp foo.dex ClassName
//   dalvikvm --window -cp foo.dex ActivityClassName   (opens NSWindow)
//
// Flags starting with -X are accepted and silently ignored (ART compat).
//
// This binary is a native macOS ARM64 Mach-O — no Android emulator required.
// DYLD_INSERT_LIBRARIES=libaine-shim.dylib provides Linux syscall translation
// for any dependency that needs it.

#include "dex.h"
#include "interp.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

static void usage(const char *argv0) {
    fprintf(stderr,
        "Usage: %s [-cp <dexfile>] [--window] [-X...] <ClassName> [args...]\n"
        "\n"
        "AINE native Dalvik interpreter — runs DEX bytecode on macOS ARM64.\n"
        "No Android emulator required.\n"
        "\n"
        "  --window   Open a native NSWindow for Activity classes.\n",
        argv0);
}

int main(int argc, char *argv[]) {
    const char *dex_path    = NULL;
    const char *class_name  = NULL;
    int         window_mode = 0;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-cp") == 0 || strcmp(argv[i], "-classpath") == 0) {
            if (++i >= argc) { usage(argv[0]); return 1; }
            dex_path = argv[i];
        } else if (strcmp(argv[i], "--window") == 0 ||
                   strcmp(argv[i], "-window")  == 0) {
            window_mode = 1;
        } else if (strncmp(argv[i], "-X", 2) == 0) {
            // ART flags — ignore silently
        } else if (strncmp(argv[i], "-D", 2) == 0) {
            // System properties — ignore for now
        } else if (argv[i][0] != '-') {
            if (!class_name) {
                class_name = argv[i];
            }
            // remaining are app args — ignore for now
        }
    }

    if (!dex_path || !class_name) {
        usage(argv[0]);
        return 1;
    }

    // Load DEX file via mmap
    int fd = open(dex_path, O_RDONLY);
    if (fd < 0) {
        perror(dex_path);
        return 1;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) { perror("fstat"); close(fd); return 1; }
    size_t sz = (size_t)st.st_size;
    void *mem = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mem == MAP_FAILED) { perror("mmap"); return 1; }

    // Parse DEX
    DexFile df;
    if (dex_open(&df, (const uint8_t *)mem, sz) != 0) {
        fprintf(stderr, "[aine-dalvik] %s: not a valid DEX file\n", dex_path);
        munmap(mem, sz);
        return 1;
    }

    // Convert class name to DEX descriptor: "HelloWorld" → "LHelloWorld;"
    char descriptor[512];
    if (class_name[0] == 'L') {
        // Already a descriptor
        snprintf(descriptor, sizeof(descriptor), "%s", class_name);
    } else {
        snprintf(descriptor, sizeof(descriptor), "L%s;", class_name);
        // Replace dots with slashes: "com.foo.Bar" → "Lcom/foo/Bar;"
        for (char *p = descriptor + 1; *p && *p != ';'; p++) {
            if (*p == '.') *p = '/';
        }
    }

    // Run
    AineInterp *interp = interp_new(&df);
    int ret;
    if (window_mode) {
        /* Open NSWindow + run Activity lifecycle (headless-safe) */
        ret = aine_window_run(interp, descriptor);
    } else {
        ret = interp_run_main(interp, descriptor);
    }
    interp_free(interp);
    munmap(mem, sz);
    return ret;
}
