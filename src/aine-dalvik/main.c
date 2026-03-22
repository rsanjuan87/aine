// aine-dalvik/main.c — dalvikvm entry point
//
// Usage (compatible with real dalvikvm):
//   dalvikvm [-cp <dexfile>] <ClassName> [args...]
//   dalvikvm -Xnoimage-dex2oat -Xusejit:false -cp foo.dex ClassName
//   dalvikvm --window -cp foo.dex ActivityClassName         (opens NSWindow)
//   dalvikvm --window --max-frames N -cp foo.dex ClassName  (exit after N frames)
//
// Flags starting with -X are accepted and silently ignored (ART compat).
//
// This binary is a native macOS ARM64 Mach-O — no Android emulator required.
// DYLD_INSERT_LIBRARIES=libaine-shim.dylib provides Linux syscall translation
// for any dependency that needs it.

#include "dex.h"
#include "interp.h"
#include "jni.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <dirent.h>

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
    int         max_frames  = 0;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-cp") == 0 || strcmp(argv[i], "-classpath") == 0) {
            if (++i >= argc) { usage(argv[0]); return 1; }
            dex_path = argv[i];
        } else if (strcmp(argv[i], "--window") == 0 ||
                   strcmp(argv[i], "-window")  == 0) {
            window_mode = 1;
        } else if (strcmp(argv[i], "--max-frames") == 0) {
            if (++i >= argc) { usage(argv[0]); return 1; }
            max_frames = atoi(argv[i]);
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

    // Parse primary DEX
    DexFile df;
    if (dex_open(&df, (const uint8_t *)mem, sz) != 0) {
        fprintf(stderr, "[aine-dalvik] %s: not a valid DEX file\n", dex_path);
        munmap(mem, sz);
        return 1;
    }

    // Derive the directory containing the primary DEX for companion-file lookup
    char dex_dir[512] = ".";
    {
        const char *last_slash = strrchr(dex_path, '/');
        if (last_slash) {
            size_t dir_len = (size_t)(last_slash - dex_path);
            if (dir_len < sizeof(dex_dir) - 1) {
                memcpy(dex_dir, dex_path, dir_len);
                dex_dir[dir_len] = 0;
            }
        }
    }

    // Load resource companion file (aine-res.txt) from DEX directory
    jni_set_res_dir(dex_dir);

    // Load additional classesN.dex files from the same directory (multi-DEX)
    // Track extra allocations for cleanup
#define MAX_EXTRA_DEX 8
    static DexFile  extra_dfs[MAX_EXTRA_DEX];
    static void    *extra_mems[MAX_EXTRA_DEX];
    static size_t   extra_szs[MAX_EXTRA_DEX];
    int n_extra = 0;

    // Convert primary DEX basename: if it is "classes.dex", also load classes2..8.dex;
    // if it's "classes3.dex", load all others as extras.
    {
        const char *pbase = strrchr(dex_path, '/');
        pbase = pbase ? pbase + 1 : dex_path;
        // Only do multi-DEX if primary is named classesN.dex
        if (strncmp(pbase, "classes", 7) == 0) {
            for (int idx = 1; idx <= 8 && n_extra < MAX_EXTRA_DEX; idx++) {
                char extra_path[640];
                if (idx == 1)
                    snprintf(extra_path, sizeof(extra_path), "%s/classes.dex", dex_dir);
                else
                    snprintf(extra_path, sizeof(extra_path), "%s/classes%d.dex",
                             dex_dir, idx);

                // Skip the primary DEX
                if (strcmp(extra_path, dex_path) == 0) continue;

                int xfd = open(extra_path, O_RDONLY);
                if (xfd < 0) continue;          /* file doesn't exist — skip */
                struct stat xst;
                if (fstat(xfd, &xst) < 0) { close(xfd); continue; }
                size_t xsz = (size_t)xst.st_size;
                void *xmem = mmap(NULL, xsz, PROT_READ, MAP_PRIVATE, xfd, 0);
                close(xfd);
                if (xmem == MAP_FAILED) continue;
                if (dex_open(&extra_dfs[n_extra],
                             (const uint8_t *)xmem, xsz) != 0) {
                    munmap(xmem, xsz);
                    continue;
                }
                extra_mems[n_extra] = xmem;
                extra_szs[n_extra]  = xsz;
                fprintf(stderr, "[aine-dalvik] loaded extra DEX: %s\n", extra_path);
                n_extra++;
            }
        }
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
    for (int i = 0; i < n_extra; i++)
        interp_add_dex(interp, &extra_dfs[i]);
    if (max_frames > 0)
        interp_set_max_frames(max_frames);
    int ret;
    if (window_mode) {
        /* Open NSWindow + run Activity lifecycle (headless-safe) */
        ret = aine_window_run(interp, descriptor);
    } else {
        ret = interp_run_main(interp, descriptor);
    }
    interp_free(interp);
    munmap(mem, sz);
    for (int i = 0; i < n_extra; i++) munmap(extra_mems[i], extra_szs[i]);
    return ret;
}
