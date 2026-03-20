// aine-pm/pm.c — Package manager: install, list, query, remove
//
// Registry format (/tmp/aine/packages.db — one line per package):
//   package_name|version_name|version_code|main_activity|install_dir|dex_path
//
#include "pm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define AINE_BASE     "/tmp/aine"
#define PACKAGES_DB   AINE_BASE "/packages.db"
#define LINE_MAX_LEN  2048

// ── Registry helpers ───────────────────────────────────────────────────────

static void db_ensure_base(void) { mkdir(AINE_BASE, 0755); }

// Write or update a package entry in packages.db.
static int db_write(const ApkInfo *info) {
    db_ensure_base();
    const AxmlManifest *m = &info->manifest;

    // Read existing db, skip line for this package if present
    char *existing_lines[1024];
    int   n = 0;
    FILE *f = fopen(PACKAGES_DB, "r");
    if (f) {
        char line[LINE_MAX_LEN];
        while (fgets(line, sizeof(line), f)) {
            // Check if first field matches package
            char pkg[256] = {0};
            sscanf(line, "%255[^|]", pkg);
            if (strcmp(pkg, m->package) != 0) {
                existing_lines[n++] = strdup(line);
                if (n >= 1024) break;
            }
        }
        fclose(f);
    }

    // Append the new/updated entry
    f = fopen(PACKAGES_DB, "w");
    if (!f) { perror(PACKAGES_DB); goto cleanup; }

    for (int i = 0; i < n; i++) { fputs(existing_lines[i], f); }
    fprintf(f, "%s|%s|%d|%s|%s|%s\n",
            m->package, m->version_name, m->version_code,
            m->main_activity, info->install_dir, info->dex_path);
    fclose(f);

cleanup:
    for (int i = 0; i < n; i++) free(existing_lines[i]);
    return 0;
}

// Parse one db line into an ApkInfo struct.
static int db_parse_line(const char *line, ApkInfo *out) {
    memset(out, 0, sizeof(*out));
    int vc = 0;
    int scanned = sscanf(line,
        "%255[^|]|%63[^|]|%d|%255[^|]|%511[^|]|%511[^\n]",
        out->manifest.package,
        out->manifest.version_name,
        &vc,
        out->manifest.main_activity,
        out->install_dir,
        out->dex_path);
    out->manifest.version_code = vc;
    return (scanned >= 4) ? 0 : -1;
}

// ── Public API ────────────────────────────────────────────────────────────

int pm_install(const char *apk_path) {
    ApkInfo info;
    if (apk_install(apk_path, AINE_BASE, &info) < 0) return -1;
    apk_print(&info);
    db_write(&info);
    printf("[aine-pm] Installed: %s\n", info.manifest.package);
    return 0;
}

void pm_list(void) {
    FILE *f = fopen(PACKAGES_DB, "r");
    if (!f) { printf("[aine-pm] No packages installed.\n"); return; }

    printf("%-40s  %-12s  %s\n", "PACKAGE", "VERSION", "DEX");
    printf("%-40s  %-12s  %s\n", "-------", "-------", "---");

    char line[LINE_MAX_LEN];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        ApkInfo info;
        if (db_parse_line(line, &info) == 0) {
            printf("%-40s  %-12s  %s\n",
                   info.manifest.package,
                   info.manifest.version_name,
                   info.dex_path);
            count++;
        }
    }
    fclose(f);
    if (count == 0) printf("[aine-pm] No packages installed.\n");
}

int pm_query(const char *package_name, ApkInfo *out) {
    FILE *f = fopen(PACKAGES_DB, "r");
    if (!f) return -1;

    char line[LINE_MAX_LEN];
    int found = -1;
    while (fgets(line, sizeof(line), f)) {
        ApkInfo tmp;
        if (db_parse_line(line, &tmp) == 0 &&
            strcmp(tmp.manifest.package, package_name) == 0) {
            *out  = tmp;
            found = 0;
            break;
        }
    }
    fclose(f);
    return found;
}

int pm_remove(const char *package_name) {
    char *lines[1024];
    int   n = 0;
    FILE *f = fopen(PACKAGES_DB, "r");
    if (!f) return -1;

    char line[LINE_MAX_LEN];
    while (fgets(line, sizeof(line), f)) {
        char pkg[256] = {0};
        sscanf(line, "%255[^|]", pkg);
        if (strcmp(pkg, package_name) != 0) {
            lines[n++] = strdup(line);
            if (n >= 1024) break;
        }
    }
    fclose(f);

    f = fopen(PACKAGES_DB, "w");
    if (!f) { for (int i=0;i<n;i++) free(lines[i]); return -1; }
    for (int i = 0; i < n; i++) { fputs(lines[i], f); free(lines[i]); }
    fclose(f);

    printf("[aine-pm] Removed: %s\n", package_name);
    return 0;
}
