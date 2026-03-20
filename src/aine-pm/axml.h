// aine-pm/axml.h — Android binary XML (AXML) manifest parser
// Extracts package name, version info, and main activity from
// the AndroidManifest.xml stored inside an APK.
#pragma once
#include <stdint.h>
#include <stddef.h>

// Information extracted from AndroidManifest.xml
typedef struct {
    char package[256];          // e.g. "com.example.myapp"
    char version_name[64];      // e.g. "1.0"
    int  version_code;          // e.g. 1
    char main_activity[256];    // fully-qualified, e.g. "com.example.myapp.MainActivity"
    char permissions[32][256];  // required permissions
    int  permission_count;
    int  min_sdk;
    int  target_sdk;
    char label[128];            // app label (raw string, not resolved from res)
} AxmlManifest;

// Parse a raw AXML buffer. Returns 0 on success, -1 on error.
int axml_parse(const uint8_t *data, size_t size, AxmlManifest *out);
