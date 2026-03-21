// aine-pm/main.c — aine-pm CLI
//
// Usage:
//   aine-pm install <apk>          install APK and register package
//   aine-pm list                   list installed packages
//   aine-pm query  <package>       show info for installed package
//   aine-pm remove <package>       remove installed package
//   aine-pm run    <package>       launch via aine-dalvik (native, no emulator)
//
#include "pm.h"
#include "apk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void) {
    fprintf(stderr,
        "Usage: aine-pm <command> [args]\n"
        "  install <apk>     — install APK\n"
        "  list              — list installed packages\n"
        "  query  <package>  — show package info\n"
        "  remove <package>  — remove package\n"
        "  run    <package>  — run via aine-dalvik\n");
}

// Find dalvikvm binary relative to our own executable path
#ifdef __APPLE__
#  include <mach-o/dyld.h>
#endif
static int find_dalvikvm(char *out, size_t outsz) {
    char self[1024] = {0};
#ifdef __APPLE__
    uint32_t sz = (uint32_t)sizeof(self);
    if (_NSGetExecutablePath(self, &sz) != 0) self[0] = '\0';
#else
    readlink("/proc/self/exe", self, sizeof(self)-1);
#endif
    char *slash = strrchr(self, '/');
    if (slash) {
        *slash = '\0';
        snprintf(out, outsz, "%s/dalvikvm", self);
        if (access(out, X_OK) == 0) return 0;
    }
    snprintf(out, outsz, "dalvikvm");
    return 0;
}

static int cmd_run(const char *package_name) {
    ApkInfo info;
    if (pm_query(package_name, &info) < 0) {
        fprintf(stderr, "[aine-pm] package not found: %s\n", package_name);
        fprintf(stderr, "[aine-pm] hint: run 'aine-pm install <apk>' first\n");
        return 1;
    }

    if (info.dex_path[0] == '\0') {
        fprintf(stderr, "[aine-pm] no DEX for package %s\n", package_name);
        return 1;
    }

    if (info.manifest.main_activity[0] == '\0') {
        fprintf(stderr, "[aine-pm] no main activity for %s\n", package_name);
        fprintf(stderr, "[aine-pm] (app has no android.intent.action.MAIN intent-filter)\n");
        return 1;
    }

    char dalvikvm[1024];
    find_dalvikvm(dalvikvm, sizeof(dalvikvm));

    printf("[aine-pm] Launching: %s\n", package_name);
    printf("[aine-pm]   Activity: %s\n", info.manifest.main_activity);
    printf("[aine-pm]   DEX:      %s\n", info.dex_path);
    printf("[aine-pm]   Runtime:  %s (native, no emulator)\n", dalvikvm);
    printf("---\n");

    // Build argv for execvp
    // dalvikvm --window -cp <dex> <MainClass>
    // For now we pass the class name and let dalvikvm attempt execution.
    char *argv[] = {
        dalvikvm,
        (char *)"--window",
        (char *)"-cp",
        info.dex_path,
        info.manifest.main_activity,
        NULL
    };

    execvp(dalvikvm, argv);
    // execvp only returns on error
    perror(dalvikvm);
    fprintf(stderr,
        "[aine-pm] hint: build dalvikvm first with: cmake --build build --target dalvikvm\n");
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { usage(); return 1; }

    const char *cmd = argv[1];

    if (strcmp(cmd, "install") == 0) {
        if (argc < 3) { fprintf(stderr, "usage: aine-pm install <apk>\n"); return 1; }
        return pm_install(argv[2]) == 0 ? 0 : 1;
    }
    if (strcmp(cmd, "list") == 0) {
        pm_list();
        return 0;
    }
    if (strcmp(cmd, "query") == 0) {
        if (argc < 3) { fprintf(stderr, "usage: aine-pm query <package>\n"); return 1; }
        ApkInfo info;
        if (pm_query(argv[2], &info) < 0) {
            fprintf(stderr, "[aine-pm] not installed: %s\n", argv[2]);
            return 1;
        }
        apk_print(&info);
        return 0;
    }
    if (strcmp(cmd, "remove") == 0) {
        if (argc < 3) { fprintf(stderr, "usage: aine-pm remove <package>\n"); return 1; }
        return pm_remove(argv[2]) == 0 ? 0 : 1;
    }
    if (strcmp(cmd, "run") == 0) {
        if (argc < 3) { fprintf(stderr, "usage: aine-pm run <package>\n"); return 1; }
        return cmd_run(argv[2]);
    }

    fprintf(stderr, "[aine-pm] unknown command: %s\n", cmd);
    usage();
    return 1;
}
