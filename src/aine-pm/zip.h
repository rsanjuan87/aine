// aine-pm/zip.h — Minimal ZIP/APK reader (no external deps beyond libz)
// Used by aine-pm to extract classes.dex, libs and AndroidManifest.xml
// from Android APK files (which are standard ZIP archives).
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct ZipFile ZipFile;

// Open an APK/ZIP file. Returns NULL on error.
ZipFile *zip_open(const char *path);
void     zip_close(ZipFile *zf);

// Extract a named entry to a file path. Directories in dest_path are created.
// Returns 0 on success, -1 on error.
int zip_extract(ZipFile *zf, const char *entry_name, const char *dest_path);

// Extract a named entry to a heap-allocated buffer. Caller must free *out.
// Returns 0 on success, -1 if entry not found.
int zip_extract_mem(ZipFile *zf, const char *entry_name,
                    uint8_t **out, size_t *out_size);

// Call cb(entry_name, user_data) for every entry whose name starts with prefix.
// Pass prefix="" to iterate all entries.
void zip_foreach(ZipFile *zf, const char *prefix,
                 void (*cb)(const char *name, void *ud), void *ud);

// Return number of entries, or -1 on error.
int zip_entry_count(ZipFile *zf);
