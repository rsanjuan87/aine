// tests/pm/test_pm.c — aine-pm unit tests
// Tests: ZIP extraction, AXML parsing, APK install pipeline
#include "zip.h"
#include "axml.h"
#include "apk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Path to the test APK (set by CMake via compile definition)
#ifndef TEST_APK_PATH
#define TEST_APK_PATH "test-apps/M3TestApp/M3TestApp.apk"
#endif

#ifndef TEST_INSTALL_BASE
#define TEST_INSTALL_BASE "/tmp/aine-test"
#endif

static int failures = 0;

#define EXPECT_STR(got, expected, label) do { \
    if (strcmp((got), (expected)) != 0) { \
        fprintf(stderr, "FAIL [%s]: expected '%s', got '%s'\n", \
                (label), (expected), (got)); \
        failures++; \
    } else { \
        printf("PASS [%s]: '%s'\n", (label), (got)); \
    } \
} while(0)

#define EXPECT_INT(got, expected, label) do { \
    if ((got) != (expected)) { \
        fprintf(stderr, "FAIL [%s]: expected %d, got %d\n", \
                (label), (expected), (got)); \
        failures++; \
    } else { \
        printf("PASS [%s]: %d\n", (label), (got)); \
    } \
} while(0)

#define EXPECT_NZ(got, label) do { \
    if (!(got)) { \
        fprintf(stderr, "FAIL [%s]: expected non-empty/non-null\n", (label)); \
        failures++; \
    } else { \
        printf("PASS [%s]\n", (label)); \
    } \
} while(0)

// ── T1: ZIP opens and has expected entries ────────────────────────────────
static void test_zip_open(void) {
    printf("\n=== T1: ZIP open ===\n");
    ZipFile *zf = zip_open(TEST_APK_PATH);
    if (!zf) {
        fprintf(stderr, "FAIL: zip_open('%s') returned NULL\n", TEST_APK_PATH);
        failures++;
        return;
    }
    EXPECT_NZ(zip_entry_count(zf) > 0, "entry-count > 0");

    // Verify required entries exist
    uint8_t *buf = NULL; size_t sz = 0;
    int ok = zip_extract_mem(zf, "AndroidManifest.xml", &buf, &sz);
    EXPECT_INT(ok, 0, "AndroidManifest.xml extracted");
    EXPECT_NZ(sz > 0, "AndroidManifest.xml size > 0");
    free(buf);

    buf = NULL; sz = 0;
    ok = zip_extract_mem(zf, "classes.dex", &buf, &sz);
    EXPECT_INT(ok, 0, "classes.dex extracted");
    EXPECT_NZ(sz > 0, "classes.dex size > 0");
    // Verify DEX magic
    if (buf && sz >= 4) {
        EXPECT_INT(memcmp(buf, "dex\n", 4), 0, "classes.dex magic 'dex\\n'");
    }
    free(buf);

    zip_close(zf);
}

// ── T2: AXML parse extracts correct manifest info ─────────────────────────
static void test_axml_parse(void) {
    printf("\n=== T2: AXML parse ===\n");
    ZipFile *zf = zip_open(TEST_APK_PATH);
    if (!zf) { fprintf(stderr, "SKIP: cannot open APK\n"); return; }

    uint8_t *buf = NULL; size_t sz = 0;
    if (zip_extract_mem(zf, "AndroidManifest.xml", &buf, &sz) < 0) {
        fprintf(stderr, "SKIP: no manifest\n");
        zip_close(zf); return;
    }
    zip_close(zf);

    AxmlManifest m;
    int rc = axml_parse(buf, sz, &m);
    free(buf);

    EXPECT_INT(rc, 0, "axml_parse returns 0");
    EXPECT_STR(m.package, "com.aine.testapp", "package name");
    EXPECT_NZ(m.version_name[0] != '\0', "versionName non-empty");
    EXPECT_NZ(m.main_activity[0] != '\0', "main_activity non-empty");

    // Main activity must include the package prefix
    EXPECT_NZ(strstr(m.main_activity, "com.aine.testapp") != NULL,
              "main_activity has package prefix");
    EXPECT_NZ(strstr(m.main_activity, "MainActivity") != NULL ||
              strstr(m.main_activity, "Activity") != NULL,
              "main_activity has Activity suffix");

    printf("PASS [full main_activity]: %s\n", m.main_activity);
}

// ── T3: APK install pipeline ──────────────────────────────────────────────
static void test_apk_install(void) {
    printf("\n=== T3: APK install ===\n");
    ApkInfo info;
    int rc = apk_install(TEST_APK_PATH, TEST_INSTALL_BASE, &info);
    EXPECT_INT(rc, 0, "apk_install returns 0");
    if (rc != 0) return;

    EXPECT_STR(info.manifest.package, "com.aine.testapp", "installed package name");
    EXPECT_NZ(info.dex_path[0] != '\0', "dex_path non-empty");
    EXPECT_NZ(info.install_dir[0] != '\0', "install_dir non-empty");

    // Verify classes.dex actually exists on disk
    FILE *f = fopen(info.dex_path, "rb");
    EXPECT_NZ(f != NULL, "classes.dex exists on disk");
    if (f) {
        char magic[4];
        fread(magic, 1, 4, f);
        fclose(f);
        EXPECT_INT(memcmp(magic, "dex\n", 4), 0, "on-disk DEX magic");
    }

    printf("PASS [install_dir]: %s\n", info.install_dir);
    printf("PASS [dex_path]:    %s\n", info.dex_path);
}

// ── Main ───────────────────────────────────────────────────────────────────
int main(void) {
    printf("aine-pm tests: APK=%s\n", TEST_APK_PATH);

    test_zip_open();
    test_axml_parse();
    test_apk_install();

    printf("\n=== Results: %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
