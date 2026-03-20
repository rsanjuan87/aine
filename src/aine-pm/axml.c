// aine-pm/axml.c — Android binary XML (AXML) parser
//
// Format reference: AOSP frameworks/base/include/androidfw/ResourceTypes.h
//
// AXML structure:
//   FileHeader  (type=0x0003, hdrSize=8, fileSize)
//   StringPool  (type=0x0001) — all strings in the document
//   ResourceMap (type=0x0180) — attribute → resource ID map (optional/skipped)
//   XML events: START_NS(0x0100), END_NS(0x0101),
//               START_ELEMENT(0x0102), END_ELEMENT(0x0103)
//
// We care about extracting from JavaManifest.xml:
//   <manifest package="..." android:versionCode="..." android:versionName="...">
//   <activity android:name="...">
//     <intent-filter>
//       <action android:name="android.intent.action.MAIN"/>
//       <category android:name="android.intent.category.LAUNCHER"/>
//     </intent-filter>
//   </activity>
//   <uses-permission android:name="..."/>
//
#include "axml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Chunk type constants ───────────────────────────────────────────────────
#define AXML_STRING_POOL    0x0001u
#define AXML_RESOURCE_MAP   0x0180u
#define AXML_START_NS       0x0100u
#define AXML_END_NS         0x0101u
#define AXML_START_ELEMENT  0x0102u
#define AXML_END_ELEMENT    0x0103u

// Typed value data types (Res_value::dataType)
#define TYPE_NULL       0x00u
#define TYPE_REFERENCE  0x01u
#define TYPE_STRING     0x03u
#define TYPE_INT_DEC    0x10u
#define TYPE_INT_HEX    0x11u
#define TYPE_INT_BOOL   0x12u

#define UTF8_FLAG 0x00000100u   // flag in string pool header

// ── Little-endian read helpers ─────────────────────────────────────────────
static inline uint16_t r16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t r32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

// ── String pool ────────────────────────────────────────────────────────────
typedef struct {
    char   **strs;    // array of UTF-8 strings (heap-allocated)
    int      count;
    int      is_utf8;
} StringPool;

// Decode UTF-16LE string (length-prefixed) into dst (UTF-8 via ASCII path).
// Returns number of chars decoded.
static int utf16le_decode(const uint8_t *src, int char_count,
                          char *dst, int dst_max) {
    int i;
    for (i = 0; i < char_count && i < dst_max - 1; i++) {
        uint32_t c = r16(src + i * 2);
        // Encode to UTF-8 (handles BMP codepoints)
        if (c < 0x80) {
            dst[i] = (char)c;
        } else if (c < 0x800) {
            if (i + 1 >= dst_max - 1) break;
            dst[i++] = (char)(0xC0 | (c >> 6));
            dst[i]   = (char)(0x80 | (c & 0x3F));
        } else {
            if (i + 2 >= dst_max - 1) break;
            dst[i++] = (char)(0xE0 | (c >> 12));
            dst[i++] = (char)(0x80 | ((c >> 6) & 0x3F));
            dst[i]   = (char)(0x80 | (c & 0x3F));
        }
    }
    dst[i] = '\0';
    return i;
}

// Decode ULEB128 (for UTF-8 mode string lengths)
static uint32_t uleb128(const uint8_t **p) {
    uint32_t val = 0; int shift = 0;
    do { val |= (uint32_t)(**p & 0x7f) << shift; shift += 7; } while (*(*p)++ & 0x80);
    return val;
}

static int parse_string_pool(const uint8_t *chunk, size_t chunk_size,
                              StringPool *sp) {
    if (chunk_size < 28) return -1;
    // chunk: type(2)+hdrSize(2)+chunkSize(4)+strCount(4)+styleCount(4)+
    //        flags(4)+stringsStart(4)+stylesStart(4)
    uint32_t str_count    = r32(chunk + 8);
    uint32_t flags        = r32(chunk + 16);
    uint32_t strings_off  = r32(chunk + 20);  // relative to chunk start

    sp->is_utf8 = (flags & UTF8_FLAG) != 0;
    sp->count   = (int)str_count;
    sp->strs    = calloc(str_count, sizeof(char *));
    if (!sp->strs) return -1;

    // Offsets array starts at chunk+28
    // Each offset is relative to the start of the strings data region
    const uint8_t *offsets = chunk + 28;
    const uint8_t *strs    = chunk + strings_off;  // absolute in chunk

    for (uint32_t i = 0; i < str_count; i++) {
        uint32_t rel = r32(offsets + i * 4);
        const uint8_t *p = strs + rel;
        char buf[1024];

        if (sp->is_utf8) {
            // UTF-8 mode: UTF-16 length (1-2 ULEB128 bytes) + UTF-8 length + bytes
            uleb128(&p);            // UTF-16 char count (skip)
            uint32_t byte_len = uleb128(&p);
            int n = (int)(byte_len < sizeof(buf)-1 ? byte_len : sizeof(buf)-1);
            memcpy(buf, p, (size_t)n);
            buf[n] = '\0';
        } else {
            // UTF-16LE mode: 2-byte char count + chars + 2-byte null
            uint16_t char_count = r16(p);
            utf16le_decode(p + 2, (int)char_count, buf, sizeof(buf));
        }
        sp->strs[i] = strdup(buf);
    }
    return 0;
}

static void free_string_pool(StringPool *sp) {
    for (int i = 0; i < sp->count; i++) free(sp->strs[i]);
    free(sp->strs);
    memset(sp, 0, sizeof(*sp));
}

static const char *pool_str(const StringPool *sp, uint32_t idx) {
    if (idx == 0xFFFFFFFFu || (int)idx >= sp->count) return NULL;
    return sp->strs[idx];
}

// ── Attribute scan helper ──────────────────────────────────────────────────
// Attributes in a START_ELEMENT chunk: each is 20 bytes.
// Layout: ns(4) + name(4) + rawValue(4) + typedSize(2) + res0(1) + dataType(1) + data(4)

typedef struct {
    const char *ns_str;
    const char *name_str;
    const char *raw_str;   // NULL if rawValue=-1
    uint8_t     data_type;
    uint32_t    data;
} Attr;

// Parse all attributes of a START_ELEMENT into attrs[].
// chunk points to start of START_ELEMENT chunk.
// Returns attribute count.
static int parse_attrs(const uint8_t *chunk, const StringPool *sp,
                        Attr *attrs, int max_attrs) {
    // Extended header: after basic 8-byte header and 8-byte (lineNum+comment)
    // the attrExt starts at offset 16 from chunk:
    //   ns(4) + name(4) + attrStart(2) + attrSize(2) + attrCount(2) + ...
    const uint8_t *ext = chunk + 16;
    uint16_t attr_start = r16(ext + 8);   // offset from ext to first attr
    uint16_t attr_size  = r16(ext + 10);
    uint16_t attr_count = r16(ext + 12);
    if (attr_size < 20) attr_size = 20;

    const uint8_t *p = ext + attr_start;
    int n = attr_count < (uint16_t)max_attrs ? attr_count : max_attrs;
    for (int i = 0; i < n; i++, p += attr_size) {
        attrs[i].ns_str   = pool_str(sp, r32(p));
        attrs[i].name_str = pool_str(sp, r32(p + 4));
        attrs[i].raw_str  = pool_str(sp, r32(p + 8));
        attrs[i].data_type= p[15];
        attrs[i].data     = r32(p + 16);
    }
    return n;
}

// Find an attribute by local name, return its string or integer value.
static const char *attr_string(const Attr *attrs, int n,
                                const char *name, const StringPool *sp) {
    for (int i = 0; i < n; i++) {
        if (!attrs[i].name_str) continue;
        if (strcmp(attrs[i].name_str, name) != 0) continue;
        if (attrs[i].data_type == TYPE_STRING)
            return pool_str(sp, attrs[i].data);
        if (attrs[i].raw_str) return attrs[i].raw_str;
    }
    return NULL;
}

static int attr_int(const Attr *attrs, int n, const char *name,
                    int default_val) {
    for (int i = 0; i < n; i++) {
        if (!attrs[i].name_str) continue;
        if (strcmp(attrs[i].name_str, name) != 0) continue;
        if (attrs[i].data_type == TYPE_INT_DEC ||
            attrs[i].data_type == TYPE_INT_HEX ||
            attrs[i].data_type == TYPE_INT_BOOL)
            return (int)attrs[i].data;
        // Fall back to raw string
        if (attrs[i].raw_str) return atoi(attrs[i].raw_str);
    }
    return default_val;
}

// Resolve activity name: if it starts with '.' prepend package;
// if it has no '.' at all, prepend package + '.'.
static void resolve_activity(const char *raw, const char *pkg,
                              char *out, size_t outsz) {
    if (raw[0] == '.') {
        snprintf(out, outsz, "%s%s", pkg, raw);
    } else if (!strchr(raw, '.')) {
        snprintf(out, outsz, "%s.%s", pkg, raw);
    } else {
        snprintf(out, outsz, "%s", raw);
    }
}

// ── Public: parse ─────────────────────────────────────────────────────────
int axml_parse(const uint8_t *data, size_t size, AxmlManifest *out) {
    memset(out, 0, sizeof(*out));

    if (size < 8) return -1;
    // File header: type(2)+hdrSize(2)+fileSize(4)
    if (r16(data) != 0x0003) {
        fprintf(stderr, "[aine-pm] axml: bad magic 0x%04x\n", r16(data));
        return -1;
    }

    StringPool sp = {0};
    int sp_ready = 0;

    // State for tracking nested elements
    int  in_application   = 0;
    int  in_activity      = 0;
    int  in_intent_filter = 0;
    int  is_main_action   = 0;
    int  is_launcher_cat  = 0;
    char cur_activity[256]= {0};

    const uint8_t *p   = data + 8;  // skip file header
    const uint8_t *end = data + size;

    while (p + 8 <= end) {
        uint16_t chunk_type = r16(p);
        // uint16_t hdr_size   = r16(p + 2);
        uint32_t chunk_size = r32(p + 4);
        if (chunk_size < 8 || (size_t)(p - data) + chunk_size > size) break;

        switch (chunk_type) {

        case AXML_STRING_POOL:
            if (!sp_ready) {
                parse_string_pool(p, chunk_size, &sp);
                sp_ready = 1;
            }
            break;

        case AXML_RESOURCE_MAP:
            // Skip — resource ID map not needed for manifest parsing
            break;

        case AXML_START_NS:
        case AXML_END_NS:
            // Namespace declarations — not needed for our purposes
            break;

        case AXML_START_ELEMENT: {
            if (!sp_ready) break;
            // Element name index is at offset 28 from chunk start (ext+12)
            const uint8_t *ext = p + 16;
            const char *elem = pool_str(&sp, r32(ext + 4));

            Attr attrs[32];
            int  nattr = parse_attrs(p, &sp, attrs, 32);

            if (elem && strcmp(elem, "manifest") == 0) {
                const char *pkg = attr_string(attrs, nattr, "package", &sp);
                if (pkg) snprintf(out->package, sizeof(out->package), "%s", pkg);

                const char *vn = attr_string(attrs, nattr, "versionName", &sp);
                if (vn) snprintf(out->version_name, sizeof(out->version_name), "%s", vn);

                out->version_code = attr_int(attrs, nattr, "versionCode", 0);
            }
            else if (elem && strcmp(elem, "uses-sdk") == 0) {
                out->min_sdk    = attr_int(attrs, nattr, "minSdkVersion", 0);
                out->target_sdk = attr_int(attrs, nattr, "targetSdkVersion", 0);
            }
            else if (elem && strcmp(elem, "uses-permission") == 0) {
                const char *perm = attr_string(attrs, nattr, "name", &sp);
                if (perm && out->permission_count < 32) {
                    snprintf(out->permissions[out->permission_count++],
                             sizeof(out->permissions[0]), "%s", perm);
                }
            }
            else if (elem && strcmp(elem, "application") == 0) {
                in_application = 1;
                const char *lbl = attr_string(attrs, nattr, "label", &sp);
                if (lbl) snprintf(out->label, sizeof(out->label), "%s", lbl);
            }
            else if (elem && strcmp(elem, "activity") == 0 && in_application) {
                const char *aname = attr_string(attrs, nattr, "name", &sp);
                if (aname) {
                    resolve_activity(aname, out->package,
                                     cur_activity, sizeof(cur_activity));
                }
                in_activity      = 1;
                is_main_action   = 0;
                is_launcher_cat  = 0;
            }
            else if (elem && strcmp(elem, "intent-filter") == 0 && in_activity) {
                in_intent_filter = 1;
            }
            else if (elem && strcmp(elem, "action") == 0 && in_intent_filter) {
                const char *aname = attr_string(attrs, nattr, "name", &sp);
                if (aname && strcmp(aname, "android.intent.action.MAIN") == 0)
                    is_main_action = 1;
            }
            else if (elem && strcmp(elem, "category") == 0 && in_intent_filter) {
                const char *cname = attr_string(attrs, nattr, "name", &sp);
                if (cname && strcmp(cname, "android.intent.category.LAUNCHER") == 0)
                    is_launcher_cat = 1;
            }
            break;
        }

        case AXML_END_ELEMENT: {
            if (!sp_ready) { break; }
            const uint8_t *ext = p + 16;
            const char *elem = pool_str(&sp, r32(ext + 4));

            if (elem && strcmp(elem, "intent-filter") == 0) {
                if (is_main_action && is_launcher_cat && out->main_activity[0] == '\0') {
                    snprintf(out->main_activity, sizeof(out->main_activity),
                             "%s", cur_activity);
                }
                in_intent_filter = 0;
            }
            else if (elem && strcmp(elem, "activity") == 0) {
                in_activity = 0; cur_activity[0] = '\0';
            }
            else if (elem && strcmp(elem, "application") == 0) {
                in_application = 0;
            }
            break;
        }

        default:
            break;
        }

        p += chunk_size;
    }

    free_string_pool(&sp);
    return (out->package[0] != '\0') ? 0 : -1;
}
