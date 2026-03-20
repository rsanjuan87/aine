// aine-pm/zip.c — Minimal ZIP reader using libz for DEFLATE
// Supports STORED (method 0) and DEFLATE (method 8).
// Parses Central Directory to enumerate entries, then jumps to Local File
// Headers to read data. Does NOT support ZIP64 or encrypted archives.
#include "zip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <zlib.h>

// ── ZIP signatures ─────────────────────────────────────────────────────────
#define ZIP_LFH_SIG   0x04034b50u   // Local File Header
#define ZIP_CDH_SIG   0x02014b50u   // Central Directory Header
#define ZIP_EOCD_SIG  0x06054b50u   // End of Central Directory

// ── In-memory representation of a central-directory entry ─────────────────
typedef struct {
    char     name[512];
    uint16_t compress_method;   // 0 = STORED, 8 = DEFLATE
    uint32_t crc32;
    uint32_t compressed_sz;
    uint32_t uncompressed_sz;
    uint32_t local_hdr_off;     // offset of Local File Header in file
} ZipEntry;

struct ZipFile {
    const uint8_t *data;
    size_t         size;
    int            fd;
    ZipEntry      *entries;
    int            entry_count;
};

// ── Little-endian helpers ──────────────────────────────────────────────────
static inline uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

// ── Find End-of-Central-Directory record ──────────────────────────────────
// Searches backwards from end of file (comment can be up to 65535 bytes).
static const uint8_t *find_eocd(const uint8_t *data, size_t size) {
    if (size < 22) return NULL;
    size_t max_scan = size < (22u + 65535u) ? size : 22u + 65535u;
    for (size_t i = 22; i <= max_scan; i++) {
        const uint8_t *p = data + size - i;
        if (le32(p) == ZIP_EOCD_SIG) return p;
    }
    return NULL;
}

// ── Parse all central directory entries ───────────────────────────────────
static int parse_central_dir(ZipFile *zf) {
    const uint8_t *eocd = find_eocd(zf->data, zf->size);
    if (!eocd) { fprintf(stderr, "[aine-pm] zip: EOCD not found\n"); return -1; }

    // EOCD layout: sig(4) disk(2) disk_start(2) entries_here(2) total_entries(2)
    //              cd_size(4) cd_off(4) comment_len(2)
    uint16_t total  = le16(eocd + 10);
    uint32_t cd_off = le32(eocd + 16);

    if (cd_off >= zf->size) { fprintf(stderr, "[aine-pm] zip: bad cd_off\n"); return -1; }

    zf->entries = calloc((size_t)total, sizeof(ZipEntry));
    if (!zf->entries) return -1;

    const uint8_t *p = zf->data + cd_off;
    int count = 0;
    for (int i = 0; i < total; i++) {
        if ((size_t)(p - zf->data) + 46 > zf->size) break;
        if (le32(p) != ZIP_CDH_SIG) break;

        // CDH: sig(4) ver_made(2) ver_needed(2) flags(2) compress(2)
        //      mtime(2) mdate(2) crc(4) comp_sz(4) uncomp_sz(4)
        //      fname_len(2) extra_len(2) comment_len(2) disk_start(2)
        //      int_attr(2) ext_attr(4) local_hdr_off(4) [fname] [extra] [comment]
        uint16_t compress   = le16(p + 10);
        uint32_t crc        = le32(p + 16);
        uint32_t comp_sz    = le32(p + 20);
        uint32_t uncomp_sz  = le32(p + 24);
        uint16_t fname_len  = le16(p + 28);
        uint16_t extra_len  = le16(p + 30);
        uint16_t comment_len= le16(p + 32);
        uint32_t lhdr_off   = le32(p + 42);

        if (fname_len >= sizeof(zf->entries[0].name)) {
            p += 46 + fname_len + extra_len + comment_len;
            continue;
        }
        ZipEntry *e = &zf->entries[count++];
        memcpy(e->name, p + 46, fname_len);
        e->name[fname_len]  = '\0';
        e->compress_method  = compress;
        e->crc32            = crc;
        e->compressed_sz    = comp_sz;
        e->uncompressed_sz  = uncomp_sz;
        e->local_hdr_off    = lhdr_off;

        p += 46 + fname_len + extra_len + comment_len;
    }
    zf->entry_count = count;
    return 0;
}

// ── Public: open ──────────────────────────────────────────────────────────
ZipFile *zip_open(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror(path); return NULL; }

    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return NULL; }

    void *data = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) { close(fd); return NULL; }

    ZipFile *zf = calloc(1, sizeof(ZipFile));
    zf->data = data;
    zf->size = (size_t)st.st_size;
    zf->fd   = fd;

    if (parse_central_dir(zf) < 0) { zip_close(zf); return NULL; }
    return zf;
}

void zip_close(ZipFile *zf) {
    if (!zf) return;
    if (zf->data) munmap((void *)zf->data, zf->size);
    if (zf->fd >= 0) close(zf->fd);
    free(zf->entries);
    free(zf);
}

int zip_entry_count(ZipFile *zf) { return zf ? zf->entry_count : -1; }

// ── Read compressed data from a Local File Header ────────────────────────
static const uint8_t *entry_data(const ZipFile *zf, const ZipEntry *e,
                                  uint32_t *out_comp_sz) {
    const uint8_t *lhdr = zf->data + e->local_hdr_off;
    if ((size_t)(lhdr - zf->data) + 30 > zf->size) return NULL;
    if (le32(lhdr) != ZIP_LFH_SIG) return NULL;

    uint16_t fname_len = le16(lhdr + 26);
    uint16_t extra_len = le16(lhdr + 28);
    const uint8_t *data_start = lhdr + 30 + fname_len + extra_len;
    if ((size_t)(data_start - zf->data) > zf->size) return NULL;

    // If central dir says 0 sizes, try local header (data descriptor case)
    *out_comp_sz = e->compressed_sz ? e->compressed_sz : le32(lhdr + 18);
    return data_start;
}

// ── Decompress or copy to heap buffer ─────────────────────────────────────
static int decompress_entry(const ZipEntry *e, const uint8_t *src,
                             uint32_t comp_sz, uint8_t **out, size_t *out_size) {
    if (e->compress_method == 0) {     // STORED
        *out = malloc(comp_sz);
        if (!*out) return -1;
        memcpy(*out, src, comp_sz);
        *out_size = comp_sz;
        return 0;
    }
    if (e->compress_method == 8) {     // DEFLATE
        size_t alloc_sz = e->uncompressed_sz ? e->uncompressed_sz : comp_sz * 4;
        *out = malloc(alloc_sz + 1);
        if (!*out) return -1;

        z_stream strm = {0};
        strm.next_in  = (Bytef *)src;
        strm.avail_in = comp_sz;
        strm.next_out = *out;
        strm.avail_out= (uInt)alloc_sz;

        if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) { free(*out); return -1; }
        int ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);

        if (ret != Z_STREAM_END) { free(*out); return -1; }
        *out_size = strm.total_out;
        return 0;
    }
    fprintf(stderr, "[aine-pm] zip: unsupported compression method %d\n",
            e->compress_method);
    return -1;
}

// ── Find entry by name ─────────────────────────────────────────────────────
static const ZipEntry *zip_find(const ZipFile *zf, const char *name) {
    for (int i = 0; i < zf->entry_count; i++) {
        if (strcmp(zf->entries[i].name, name) == 0) return &zf->entries[i];
    }
    return NULL;
}

// ── Public: extract to memory ─────────────────────────────────────────────
int zip_extract_mem(ZipFile *zf, const char *name,
                    uint8_t **out, size_t *out_size) {
    const ZipEntry *e = zip_find(zf, name);
    if (!e) { fprintf(stderr, "[aine-pm] zip: entry '%s' not found\n", name); return -1; }

    uint32_t comp_sz;
    const uint8_t *src = entry_data(zf, e, &comp_sz);
    if (!src) return -1;

    return decompress_entry(e, src, comp_sz, out, out_size);
}

// ── mkdir -p helper ───────────────────────────────────────────────────────
static int mkdirs(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return 0;
}

// ── Public: extract to file ───────────────────────────────────────────────
int zip_extract(ZipFile *zf, const char *entry_name, const char *dest_path) {
    uint8_t *buf = NULL;
    size_t   sz  = 0;
    if (zip_extract_mem(zf, entry_name, &buf, &sz) < 0) return -1;

    // Ensure parent directories exist
    char parent[1024];
    snprintf(parent, sizeof(parent), "%s", dest_path);
    char *slash = strrchr(parent, '/');
    if (slash) { *slash = '\0'; mkdirs(parent); }

    FILE *f = fopen(dest_path, "wb");
    if (!f) { perror(dest_path); free(buf); return -1; }
    fwrite(buf, 1, sz, f);
    fclose(f);
    free(buf);
    fprintf(stderr, "[aine-pm] extracted: %s\n", dest_path);
    return 0;
}

// ── Public: iterate entries with prefix ───────────────────────────────────
void zip_foreach(ZipFile *zf, const char *prefix,
                 void (*cb)(const char *name, void *ud), void *ud) {
    size_t plen = strlen(prefix);
    for (int i = 0; i < zf->entry_count; i++) {
        if (strncmp(zf->entries[i].name, prefix, plen) == 0)
            cb(zf->entries[i].name, ud);
    }
}
