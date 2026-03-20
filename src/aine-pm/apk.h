// aine-pm/apk.h — APK file handler
// Combines ZIP extraction + AXML parsing into a single install operation.
#pragma once
#include "axml.h"

// Result of installing an APK
typedef struct {
    AxmlManifest manifest;
    char         install_dir[512]; // e.g. /tmp/aine/com.example.myapp/
    char         dex_path[512];    // path to extracted classes.dex
    // lib_dir: install_dir + "lib/" (arm64-v8a .so files)
    char         lib_dir[512];
} ApkInfo;

// Install APK: extract classes.dex, lib/arm64-v8a/*.so, parse manifest.
// install_base: parent directory (e.g. /tmp/aine). Created if not present.
// Returns 0 on success, -1 on error.
int apk_install(const char *apk_path, const char *install_base, ApkInfo *out);

// Print a summary of an installed APK to stdout.
void apk_print(const ApkInfo *info);
