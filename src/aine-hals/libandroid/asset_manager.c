/*
 * aine-hals/libandroid/asset_manager.c — AAssetManager stubs
 *
 * Reads assets from the extracted APK directory under
 * /tmp/aine/<pkg>/assets/ (extracted by aine-pm).
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ---- Types -------------------------------------------------------------- */

typedef struct AAssetManager {
    char assets_dir[1024];  /* /tmp/aine/<pkg>/assets */
} AAssetManager;

typedef struct AAsset {
    FILE  *fp;
    long   length;
} AAsset;

#define AASSET_MODE_UNKNOWN    0
#define AASSET_MODE_RANDOM     1
#define AASSET_MODE_STREAMING  2
#define AASSET_MODE_BUFFER     3

/* ---- AAssetManager ------------------------------------------------------ */

__attribute__((visibility("default")))
AAssetManager *AAssetManager_fromJava(void *env, void *assetManager)
{
    (void)env; (void)assetManager;
    fprintf(stderr, "[aine-android] AAssetManager_fromJava (stub): "
            "use aine_asset_manager_create() instead\n");
    return NULL;
}

/* AINE extension: create manager pointing to extracted assets directory */
__attribute__((visibility("default")))
AAssetManager *aine_asset_manager_create(const char *assets_dir)
{
    AAssetManager *mgr = calloc(1, sizeof(AAssetManager));
    if (!mgr) return NULL;
    strncpy(mgr->assets_dir, assets_dir ? assets_dir : "/tmp/aine/assets",
            sizeof(mgr->assets_dir) - 1);
    return mgr;
}

__attribute__((visibility("default")))
void aine_asset_manager_free(AAssetManager *mgr)
{
    free(mgr);
}

/* ---- AAsset ------------------------------------------------------------- */

__attribute__((visibility("default")))
AAsset *AAssetManager_open(AAssetManager *mgr, const char *filename, int mode)
{
    (void)mode;
    if (!mgr || !filename) return NULL;

    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", mgr->assets_dir, filename);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "[aine-android] AAssetManager_open: %s not found\n", path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    AAsset *asset = calloc(1, sizeof(AAsset));
    if (!asset) { fclose(fp); return NULL; }
    asset->fp     = fp;
    asset->length = len;
    return asset;
}

__attribute__((visibility("default")))
int AAsset_read(AAsset *asset, void *buf, size_t count)
{
    if (!asset || !buf) return -1;
    return (int)fread(buf, 1, count, asset->fp);
}

__attribute__((visibility("default")))
off_t AAsset_seek(AAsset *asset, off_t offset, int whence)
{
    if (!asset) return -1;
    if (fseek(asset->fp, (long)offset, whence) != 0) return -1;
    return (off_t)ftell(asset->fp);
}

__attribute__((visibility("default")))
void AAsset_close(AAsset *asset)
{
    if (!asset) return;
    if (asset->fp) fclose(asset->fp);
    free(asset);
}

__attribute__((visibility("default")))
off_t AAsset_getLength(AAsset *asset)
{
    return asset ? (off_t)asset->length : 0;
}

__attribute__((visibility("default")))
off_t AAsset_getRemainingLength(AAsset *asset)
{
    if (!asset) return 0;
    long pos = ftell(asset->fp);
    return (off_t)(asset->length - pos);
}

__attribute__((visibility("default")))
const void *AAsset_getBuffer(AAsset *asset)
{
    /* Minimal: read entire asset into a malloc'd buffer */
    if (!asset) return NULL;
    fseek(asset->fp, 0, SEEK_SET);
    void *buf = malloc((size_t)asset->length + 1);
    if (!buf) return NULL;
    fread(buf, 1, (size_t)asset->length, asset->fp);
    return buf;  /* caller must not free (leak acceptable for stub) */
}

__attribute__((visibility("default")))
int AAsset_isAllocated(AAsset *asset)
{
    (void)asset;
    return 0;
}
