/*
 * src/aine-launcher/aine-run.c — aine-run: AINE native APK launcher (macOS ARM64)
 *
 * The AINE-native replacement for the adb-based launcher.
 * Does not use emulation, containers, or adb.
 *
 * Usage:
 *   aine-run --list                  List installed packages
 *   aine-run --query <package>       Show package info
 *   aine-run --dry-run <apk>         Install + print run command, don't exec
 *   aine-run <apk>                   Install APK and launch via dalvikvm
 *
 * Flow:
 *   1. pm_install(apk)           → /tmp/aine/<pkg>/{classes.dex, lib/}
 *   2. pm_query(package_name)    → resolve main_class, dex_path, lib_dir
 *   3. posix_spawn(dalvikvm)     → AINE_LIB_DIR=<lib> dalvikvm -cp <dex> <main>
 *   4. waitpid() + forward SIGTERM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <spawn.h>

#include "pm.h"
#include "apk.h"

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */
static char g_dalvikvm_path[1024] = "dalvikvm";

static void find_dalvikvm(const char *argv0)
{
    const char *slash = strrchr(argv0, '/');
    if (slash) {
        size_t dir_len = (size_t)(slash - argv0);
        snprintf(g_dalvikvm_path, sizeof(g_dalvikvm_path),
                 "%.*s/dalvikvm", (int)dir_len, argv0);
        if (access(g_dalvikvm_path, X_OK) == 0) return;
    }
    strncpy(g_dalvikvm_path, "dalvikvm", sizeof(g_dalvikvm_path) - 1);
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s --list                  List installed packages\n"
        "  %s --query <package>       Show package info\n"
        "  %s --dry-run <apk>         Install + print command (no exec)\n"
        "  %s <apk>                   Install and run APK\n",
        prog, prog, prog, prog);
}

/* -------------------------------------------------------------------------
 * Commands
 * ---------------------------------------------------------------------- */
static int cmd_list(void)
{
    printf("[aine-run] Installed packages:\n");
    pm_list();
    return 0;
}

static int cmd_query(const char *pkg_name)
{
    ApkInfo info;
    if (pm_query(pkg_name, &info) != 0) {
        fprintf(stderr, "[aine-run] Package '%s' not found\n", pkg_name);
        return 1;
    }
    apk_print(&info);
    return 0;
}

static int do_launch(const char *apk_path, int dry_run)
{
    /* Step 1: Install APK */
    printf("[aine-run] Installing: %s\n", apk_path);
    if (pm_install(apk_path) != 0) {
        fprintf(stderr, "[aine-run] Install failed\n");
        return 1;
    }

    /* Step 2: Get package info (re-parse manifest to avoid pm db round-trip) */
    ApkInfo info;
    memset(&info, 0, sizeof(info));
    if (apk_install(apk_path, "/tmp/aine", &info) != 0) {
        fprintf(stderr, "[aine-run] Failed to read APK info\n");
        return 1;
    }

    const char *pkg  = info.manifest.package;
    const char *main = info.manifest.main_activity;
    const char *dex  = info.dex_path;
    const char *libs = info.lib_dir;

    if (!pkg || !pkg[0]) {
        fprintf(stderr, "[aine-run] Could not determine package name\n");
        return 1;
    }
    if (!main || !main[0]) main = "Main";
    if (!dex  || !dex[0]) {
        snprintf(info.dex_path, sizeof(info.dex_path),
                 "/tmp/aine/%s/classes.dex", pkg);
        dex = info.dex_path;
    }

    /* Step 3: Report */
    printf("[aine-run] Package:  %s\n", pkg);
    printf("[aine-run] Activity: %s\n", main);
    printf("[aine-run] DEX:      %s\n", dex);
    printf("[aine-run] Libs:     %s\n", libs[0] ? libs : "(none)");
    printf("[aine-run] Command:  %s -cp %s %s\n", g_dalvikvm_path, dex, main);

    if (dry_run) {
        printf("[aine-run] (dry-run: not executing)\n");
        return 0;
    }

    /* Step 4: posix_spawn dalvikvm */
    char lib_env[1024];
    snprintf(lib_env, sizeof(lib_env), "AINE_LIB_DIR=%s",
             libs[0] ? libs : "/tmp/aine");

    /* Build child argv */
    char *child_argv[5];
    child_argv[0] = g_dalvikvm_path;
    child_argv[1] = "-cp";
    child_argv[2] = (char *)dex;
    static char main_buf[256];
    strncpy(main_buf, main, sizeof(main_buf) - 1);
    child_argv[3] = main_buf;
    child_argv[4] = NULL;

    /* Build child environment: prepend AINE_LIB_DIR */
    extern char **environ;
    int env_count = 0;
    for (char **e = environ; *e; e++) env_count++;
    char **new_env = malloc(((size_t)env_count + 2) * sizeof(char *));
    if (!new_env) { perror("malloc"); return 1; }
    new_env[0] = lib_env;
    for (int i = 0; i < env_count; i++) new_env[i + 1] = environ[i];
    new_env[env_count + 1] = NULL;

    pid_t pid = -1;
    int err = posix_spawn(&pid, g_dalvikvm_path, NULL, NULL, child_argv, new_env);
    free(new_env);

    if (err != 0) {
        fprintf(stderr, "[aine-run] posix_spawn failed: %s\n", strerror(err));
        return 1;
    }

    printf("[aine-run] Launched (pid %d)\n", (int)pid);

    int status = 0;
    waitpid(pid, &status, 0);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    printf("[aine-run] Exited with code %d\n", exit_code);
    return exit_code;
}

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    find_dalvikvm(argv[0]);

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--list") == 0)
        return cmd_list();

    if (strcmp(argv[1], "--query") == 0) {
        if (argc < 3) { usage(argv[0]); return 1; }
        return cmd_query(argv[2]);
    }

    if (strcmp(argv[1], "--dry-run") == 0) {
        if (argc < 3) { usage(argv[0]); return 1; }
        return do_launch(argv[2], 1);
    }

    return do_launch(argv[1], 0);
}
