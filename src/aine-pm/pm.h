// aine-pm/pm.h — Package manager: persistent registry of installed APKs
// Stores package info in a simple text database at /tmp/aine/packages.db.
#pragma once
#include "apk.h"

// Install APK and register it. Returns 0 on success.
int pm_install(const char *apk_path);

// List all installed packages to stdout.
void pm_list(void);

// Query a package by name. Returns 0 and fills *out if found.
int pm_query(const char *package_name, ApkInfo *out);

// Remove a package. Returns 0 on success.
int pm_remove(const char *package_name);
