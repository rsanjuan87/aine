// aine-pm/apk.c — APK install: ZIP extraction + AXML manifest parsing
#include "apk.h"
#include "zip.h"
#include "axml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define INSTALL_BASE "/tmp/aine"

// Context for lib extraction callback
typedef struct {
    ZipFile    *zf;
    const char *lib_dir;
    int         count;
} LibCtx;

static void extract_lib_cb(const char *entry_name, void *ud) {
    LibCtx *ctx = ud;
    // entry_name is like "lib/arm64-v8a/libfoo.so"
    // We want just the filename
    const char *fname = strrchr(entry_name, '/');
    fname = fname ? fname + 1 : entry_name;
    if (!fname[0] || fname[0] == '.') return;  // skip empty / hidden

    char dest[1024];
    snprintf(dest, sizeof(dest), "%s/%s", ctx->lib_dir, fname);
    if (zip_extract(ctx->zf, entry_name, dest) == 0) ctx->count++;
}

int apk_install(const char *apk_path, const char *install_base, ApkInfo *out) {
    memset(out, 0, sizeof(*out));

    const char *base = install_base ? install_base : INSTALL_BASE;

    ZipFile *zf = zip_open(apk_path);
    if (!zf) {
        fprintf(stderr, "[aine-pm] cannot open APK: %s\n", apk_path);
        return -1;
    }

    // ── 1. Extract and parse AndroidManifest.xml ──────────────────────────
    uint8_t *manifest_buf = NULL;
    size_t   manifest_sz  = 0;
    if (zip_extract_mem(zf, "AndroidManifest.xml",
                        &manifest_buf, &manifest_sz) < 0) {
        fprintf(stderr, "[aine-pm] AndroidManifest.xml not found\n");
        zip_close(zf);
        return -1;
    }

    if (axml_parse(manifest_buf, manifest_sz, &out->manifest) < 0) {
        fprintf(stderr, "[aine-pm] failed to parse AndroidManifest.xml\n");
        free(manifest_buf);
        zip_close(zf);
        return -1;
    }
    free(manifest_buf);

    // ── 2. Set up install directory ───────────────────────────────────────
    snprintf(out->install_dir, sizeof(out->install_dir),
             "%s/%s", base, out->manifest.package);
    mkdir(base, 0755);
    mkdir(out->install_dir, 0755);

    // ── 3. Extract classes.dex ────────────────────────────────────────────
    snprintf(out->dex_path, sizeof(out->dex_path),
             "%s/classes.dex", out->install_dir);
    if (zip_extract(zf, "classes.dex", out->dex_path) < 0) {
        // Try classes2.dex as fallback (multi-dex APKs)
        snprintf(out->dex_path, sizeof(out->dex_path),
                 "%s/classes.dex", out->install_dir);
        fprintf(stderr, "[aine-pm] warning: classes.dex not found\n");
    }

    // ── 4. Extract lib/arm64-v8a/*.so ─────────────────────────────────────
    snprintf(out->lib_dir, sizeof(out->lib_dir), "%s/lib", out->install_dir);
    mkdir(out->lib_dir, 0755);

    LibCtx ctx = { .zf = zf, .lib_dir = out->lib_dir, .count = 0 };
    zip_foreach(zf, "lib/arm64-v8a/", extract_lib_cb, &ctx);
    if (ctx.count == 0) {
        // No arm64 libs — that's fine for pure-Java apps
        fprintf(stderr, "[aine-pm] info: no arm64-v8a native libs\n");
    }

    zip_close(zf);
    return 0;
}

void apk_print(const ApkInfo *info) {
    const AxmlManifest *m = &info->manifest;
    printf("[aine-pm] Package:        %s\n",  m->package);
    printf("[aine-pm] Version:        %s (code %d)\n",
           m->version_name, m->version_code);
    printf("[aine-pm] Main activity:  %s\n",
           m->main_activity[0] ? m->main_activity : "(none)");
    printf("[aine-pm] SDK:            min=%d target=%d\n",
           m->min_sdk, m->target_sdk);
    printf("[aine-pm] Install dir:    %s\n",  info->install_dir);
    printf("[aine-pm] DEX:            %s\n",  info->dex_path);
    if (m->permission_count > 0) {
        printf("[aine-pm] Permissions:\n");
        for (int i = 0; i < m->permission_count; i++)
            printf("[aine-pm]   %s\n", m->permissions[i]);
    }
}
