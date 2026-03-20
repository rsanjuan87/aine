// AINE: src/aine-launcher/macos/launcher.cpp
// Orquestador de apps Android para macOS.
// Coordina con el emulador Android via adb para:
//   - Parsear metadatos del APK (package, activity principal)
//   - Instalar el APK en el dispositivo/emulador
//   - Lanzar la Activity y monitorear su ciclo de vida via logcat
//   - Verificar el ciclo completo: onCreate → onResume → onPause → onDestroy

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <time.h>

// popen con PID para poder matar el proceso hijo (adb logcat no termina solo)
static FILE *popen_pid(const char *cmd, pid_t *pid_out) {
    int pipefd[2];
    if (pipe(pipefd) < 0) return NULL;
    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return NULL; }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    close(pipefd[1]);
    if (pid_out) *pid_out = pid;
    return fdopen(pipefd[0], "r");
}

static void pclose_pid(FILE *f, pid_t pid) {
    if (f) fclose(f);
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Configuración y detección de entorno
// ─────────────────────────────────────────────────────────────────────────────

#define MAX_PATH   512
#define MAX_PKG    256
#define MAX_LINE   1024

static char g_adb_path[MAX_PATH]   = "adb";
static char g_aapt_path[MAX_PATH]  = "aapt";
static char g_adb_serial[MAX_PATH] = "";  // vacío = cualquier dispositivo

// Detecta rutas a adb y aapt desde ANDROID_SDK o PATH
static void detect_tools(void) {
    const char *sdk = getenv("ANDROID_SDK");
    char candidate[MAX_PATH];

    // adb
    if (sdk) {
        snprintf(candidate, sizeof(candidate), "%s/platform-tools/adb", sdk);
        if (access(candidate, X_OK) == 0) {
            strncpy(g_adb_path, candidate, sizeof(g_adb_path) - 1);
        }
    }
    // Fallback: buscar adb en HOME/Library/Android/sdk
    if (strcmp(g_adb_path, "adb") == 0) {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(candidate, sizeof(candidate),
                     "%s/Library/Android/sdk/platform-tools/adb", home);
            if (access(candidate, X_OK) == 0)
                strncpy(g_adb_path, candidate, sizeof(g_adb_path) - 1);
        }
    }

    // aapt — buscar en build-tools más reciente
    const char *bases[] = { sdk, NULL };
    char home_sdk[MAX_PATH];
    const char *home2 = getenv("HOME");
    if (home2) {
        snprintf(home_sdk, sizeof(home_sdk),
                 "%s/Library/Android/sdk", home2);
        bases[0] = sdk ? sdk : home_sdk;
        if (!sdk) bases[0] = home_sdk;
    }

    // Prueba con aapt2 y aapt al mismo tiempo
    for (int i = 0; i < 2; i++) {
        const char *base = bases[i];
        if (!base) break;
        // Llamar a ls para encontrar la versión más reciente
        char cmd[MAX_PATH * 2];
        snprintf(cmd, sizeof(cmd),
            "ls -d \"%s/build-tools/\"*/ 2>/dev/null | sort -V | tail -1", base);
        FILE *f = popen(cmd, "r");
        if (!f) continue;
        char bt[MAX_PATH] = "";
        if (fgets(bt, sizeof(bt), f)) {
            // trim newline
            size_t l = strlen(bt);
            if (l > 0 && bt[l-1] == '\n') bt[l-1] = '\0';
            if (l > 0 && bt[l-2] == '/') bt[l-2] = '\0';
            snprintf(candidate, sizeof(candidate), "%s/aapt", bt);
            if (access(candidate, X_OK) == 0)
                strncpy(g_aapt_path, candidate, sizeof(g_aapt_path) - 1);
        }
        pclose(f);
        if (strcmp(g_aapt_path, "aapt") != 0) break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Utilidades de subprocess
// ─────────────────────────────────────────────────────────────────────────────

// Ejecuta cmd, captura stdout+stderr en buf. Devuelve exit code.
static int run_capture(const char *cmd, char *buf, size_t buf_size) {
    FILE *f = popen(cmd, "r");
    if (!f) return -1;
    size_t total = 0;
    size_t n;
    while (total < buf_size - 1 &&
           (n = fread(buf + total, 1, buf_size - 1 - total, f)) > 0)
        total += n;
    buf[total] = '\0';
    int st = pclose(f);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

// Ejecuta cmd sin capturar salida. Devuelve exit code.
static int run_silent(const char *cmd) {
    char full[MAX_PATH * 4];
    snprintf(full, sizeof(full), "%s > /dev/null 2>&1", cmd);
    return system(full);
}

// Construye prefijo adb con serial opcional
static void adb_cmd(char *out, size_t size, const char *subcmd) {
    if (g_adb_serial[0])
        snprintf(out, size, "\"%s\" -s %s %s", g_adb_path, g_adb_serial, subcmd);
    else
        snprintf(out, size, "\"%s\" %s", g_adb_path, subcmd);
}

// ─────────────────────────────────────────────────────────────────────────────
// Información del APK
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    char package[MAX_PKG];    // e.g. "com.aine.testapp"
    char activity[MAX_PKG];   // e.g. "com.aine.testapp.MainActivity"
    char label[128];          // e.g. "AINE M3 TestApp"
    int  version_code;
    char version_name[32];
    int  sdk_min;
    int  sdk_target;
} ApkInfo;

// Parsea la salida de `aapt dump badging <apk>`
static int parse_badging(const char *output, ApkInfo *info) {
    memset(info, 0, sizeof(*info));
    const char *p = output;
    char line[MAX_LINE];

    while (*p) {
        // Copiar línea
        int li = 0;
        while (*p && *p != '\n' && li < MAX_LINE - 1)
            line[li++] = *p++;
        line[li] = '\0';
        if (*p == '\n') p++;

        // package: name='...' versionCode='...' versionName='...'
        if (strncmp(line, "package:", 8) == 0) {
            const char *n = strstr(line, "name='");
            if (n) {
                n += 6;
                int i = 0;
                while (*n && *n != '\'' && i < MAX_PKG - 1)
                    info->package[i++] = *n++;
                info->package[i] = '\0';
            }
            const char *vc = strstr(line, "versionCode='");
            if (vc) info->version_code = atoi(vc + 13);
            const char *vn = strstr(line, "versionName='");
            if (vn) {
                vn += 13;
                int i = 0;
                while (*vn && *vn != '\'' && i < 31)
                    info->version_name[i++] = *vn++;
                info->version_name[i] = '\0';
            }
        }
        // launchable-activity: name='...'
        else if (strncmp(line, "launchable-activity:", 20) == 0) {
            const char *n = strstr(line, "name='");
            if (n) {
                n += 6;
                int i = 0;
                while (*n && *n != '\'' && i < MAX_PKG - 1)
                    info->activity[i++] = *n++;
                info->activity[i] = '\0';
            }
        }
        // application-label:'...'
        else if (strncmp(line, "application-label:", 18) == 0) {
            const char *n = line + 18;
            if (*n == '\'') n++;
            int i = 0;
            while (*n && *n != '\'' && i < 127)
                info->label[i++] = *n++;
            info->label[i] = '\0';
        }
        // sdkVersion:'X'
        else if (strncmp(line, "sdkVersion:'", 12) == 0)
            info->sdk_min = atoi(line + 12);
        // targetSdkVersion:'X'
        else if (strncmp(line, "targetSdkVersion:'", 18) == 0)
            info->sdk_target = atoi(line + 18);
    }

    return (info->package[0] && info->activity[0]) ? 0 : -1;
}

static int apk_info(const char *apk_path, ApkInfo *info) {
    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "\"%s\" dump badging \"%s\" 2>&1", g_aapt_path, apk_path);
    char buf[1024 * 32];
    int rc = run_capture(cmd, buf, sizeof(buf));
    if (rc != 0) {
        fprintf(stderr, "[aine-launcher] aapt falló (rc=%d): %s\n", rc, buf);
        return -1;
    }
    return parse_badging(buf, info);
}

// ─────────────────────────────────────────────────────────────────────────────
// Gestión del emulador / dispositivo
// ─────────────────────────────────────────────────────────────────────────────

// Detecta si hay un dispositivo conectado. Devuelve serial en `serial_out` o "".
static int find_device(char *serial_out, size_t size) {
    char cmd[MAX_PATH * 2];
    adb_cmd(cmd, sizeof(cmd), "devices -l 2>/dev/null");
    char buf[4096];
    run_capture(cmd, buf, sizeof(buf));
    // Buscar línea con "device" (no "offline")
    const char *p = buf;
    char line[256];
    while (*p) {
        int li = 0;
        while (*p && *p != '\n' && li < 255) line[li++] = *p++;
        line[li] = '\0';
        if (*p == '\n') p++;
        if (strstr(line, "\tdevice") || strstr(line, " device")) {
            // Extrae el serial (primera palabra)
            int i = 0;
            while (line[i] && line[i] != ' ' && line[i] != '\t' && i < (int)size - 1)
                serial_out[i] = line[i++];
            serial_out[i] = '\0';
            if (serial_out[0] && strcmp(serial_out, "List") != 0)
                return 1;
        }
    }
    serial_out[0] = '\0';
    return 0;
}

// Espera hasta que haya un dispositivo conectado (timeout en segundos)
static int wait_for_device(int timeout_sec) {
    char cmd[MAX_PATH * 2];
    char wait_cmd[MAX_PATH * 3];
    adb_cmd(cmd, sizeof(cmd), "wait-for-device");
    snprintf(wait_cmd, sizeof(wait_cmd),
        "timeout %d %s 2>/dev/null", timeout_sec, cmd);
    return system(wait_cmd) == 0 ? 0 : -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Operaciones de instalación y lanzamiento
// ─────────────────────────────────────────────────────────────────────────────

static int install_apk(const char *apk_path) {
    char subcmd[MAX_PATH * 2];
    snprintf(subcmd, sizeof(subcmd), "install -r \"%s\" 2>&1", apk_path);
    char cmd[MAX_PATH * 3];
    adb_cmd(cmd, sizeof(cmd), subcmd);
    char buf[4096];
    int rc = run_capture(cmd, buf, sizeof(buf));
    if (rc != 0 || strstr(buf, "Failure") || strstr(buf, "error:")) {
        fprintf(stderr, "[aine-launcher] adb install falló:\n%s\n", buf);
        return -1;
    }
    printf("[aine-launcher] APK instalado: %s\n", apk_path);
    return 0;
}

static int start_activity(const char *package, const char *activity) {
    char subcmd[MAX_PKG * 3 + 64];
    snprintf(subcmd, sizeof(subcmd),
        "shell am start -n \"%s/%s\" -a android.intent.action.MAIN 2>&1",
        package, activity);
    char cmd[MAX_PATH * 2];
    adb_cmd(cmd, sizeof(cmd), subcmd);
    char buf[2048];
    int rc = run_capture(cmd, buf, sizeof(buf));
    if (rc != 0 || strstr(buf, "Error")) {
        fprintf(stderr, "[aine-launcher] am start falló:\n%s\n", buf);
        return -1;
    }
    printf("[aine-launcher] Activity lanzada: %s/%s\n", package, activity);
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Monitor de ciclo de vida via logcat
// ─────────────────────────────────────────────────────────────────────────────

// Bitmask de eventos de ciclo de vida detectados
#define LC_ONCREATE   (1 << 0)
#define LC_ONSTART    (1 << 1)
#define LC_ONRESUME   (1 << 2)
#define LC_ONPAUSE    (1 << 3)
#define LC_ONSTOP     (1 << 4)
#define LC_ONDESTROY  (1 << 5)
#define LC_FULL       (LC_ONCREATE | LC_ONSTART | LC_ONRESUME | \
                       LC_ONPAUSE  | LC_ONSTOP  | LC_ONDESTROY)

// Monitorea logcat filtrando el tag AINE-M3. Devuelve bitmask de eventos.
// Si logcat_f y logcat_pid ya son válidos (abiertos previamente), los usa.
// Si son NULL/-1, abre su propio logcat (comportamiento previo).
static int monitor_lifecycle(const char *tag, int timeout_sec,
                              FILE *logcat_f, pid_t logcat_pid_arg) {
    pid_t logcat_pid = logcat_pid_arg;
    FILE *f = logcat_f;
    int owned = 0;

    if (!f) {
        // Limpiar buffer de logcat previo
        char clearcmd[MAX_PATH * 2];
        adb_cmd(clearcmd, sizeof(clearcmd), "logcat -c 2>/dev/null");
        run_silent(clearcmd);

        // Abrir logcat como stream
        char logcat_cmd[MAX_PATH * 2 + 64];
        char subcmd[128];
        snprintf(subcmd, sizeof(subcmd), "logcat -s %s:I 2>&1", tag);
        char base[MAX_PATH * 2];
        adb_cmd(base, sizeof(base), subcmd);
        strncpy(logcat_cmd, base, sizeof(logcat_cmd) - 1);

        f = popen_pid(logcat_cmd, &logcat_pid);
        if (!f) {
            fprintf(stderr, "[aine-launcher] No se puede abrir logcat\n");
            return -1;
        }
        owned = 1;
    }

    // Hacer el fd no-bloqueante para poder timeout
    int fd = fileno(f);
    int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);

    int lifecycle = 0;
    time_t deadline = time(NULL) + timeout_sec;
    char linebuf[MAX_LINE];
    int  pos = 0;

    printf("[aine-launcher] Monitoreando logcat tag=%s (timeout=%ds)...\n",
           tag, timeout_sec);

    while (time(NULL) < deadline) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv = {1, 0};
        int sel = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (sel <= 0) {
            // Verificar si ya terminó el ciclo
            if (lifecycle & LC_ONDESTROY) break;
            continue;
        }

        // Leer caracteres disponibles
        char c;
        while (read(fd, &c, 1) == 1) {
            if (c == '\n' || pos >= MAX_LINE - 1) {
                linebuf[pos] = '\0';
                pos = 0;

                // Imprimir la línea
                if (strstr(linebuf, tag))
                    printf("  logcat: %s\n", linebuf);

                // Detectar eventos de ciclo de vida
                if (strstr(linebuf, "onCreate"))  lifecycle |= LC_ONCREATE;
                if (strstr(linebuf, "onStart"))   lifecycle |= LC_ONSTART;
                if (strstr(linebuf, "onResume"))  lifecycle |= LC_ONRESUME;
                if (strstr(linebuf, "onPause"))   lifecycle |= LC_ONPAUSE;
                if (strstr(linebuf, "onStop"))    lifecycle |= LC_ONSTOP;
                if (strstr(linebuf, "onDestroy")) lifecycle |= LC_ONDESTROY;

                // Ciclo completo
                if ((lifecycle & LC_FULL) == LC_FULL) goto done;
            } else {
                linebuf[pos++] = c;
            }
        }
    }
done:
    if (owned) pclose_pid(f, logcat_pid);
    return lifecycle;
}

// ─────────────────────────────────────────────────────────────────────────────
// Subcomandos
// ─────────────────────────────────────────────────────────────────────────────

static void usage(const char *prog) {
    fprintf(stderr,
        "Uso: %s [opciones] <subcomando> [args...]\n"
        "\n"
        "Opciones:\n"
        "  --serial <serial>   Serial del dispositivo adb\n"
        "  --sdk    <path>     Ruta al Android SDK\n"
        "\n"
        "Subcomandos:\n"
        "  info     <apk>      Mostrar información del APK\n"
        "  install  <apk>      Instalar APK en el dispositivo\n"
        "  launch   <apk>      Instalar + lanzar Activity\n"
        "  lifecycle <apk>     Prueba completa de ciclo de vida (install+launch+logcat)\n"
        "\n"
        "Ejemplos:\n"
        "  %s info test-apps/M3TestApp/M3TestApp.apk\n"
        "  %s lifecycle test-apps/M3TestApp/M3TestApp.apk\n",
        prog, prog, prog);
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    // Parsear opciones globales
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-') {
        if (strcmp(argv[argi], "--serial") == 0 && argi + 1 < argc) {
            strncpy(g_adb_serial, argv[++argi], sizeof(g_adb_serial) - 1);
        } else if (strcmp(argv[argi], "--sdk") == 0 && argi + 1 < argc) {
            setenv("ANDROID_SDK", argv[++argi], 1);
        } else {
            fprintf(stderr, "Opción desconocida: %s\n", argv[argi]);
            return 1;
        }
        argi++;
    }

    if (argi >= argc) { usage(argv[0]); return 1; }
    const char *subcmd = argv[argi++];

    // Auto-detectar herramientas
    detect_tools();

    // ── info ──────────────────────────────────────────────────────────────────
    if (strcmp(subcmd, "info") == 0) {
        if (argi >= argc) { fprintf(stderr, "info requiere <apk>\n"); return 1; }
        const char *apk = argv[argi];
        ApkInfo info;
        if (apk_info(apk, &info) < 0) {
            fprintf(stderr, "[FAIL] No se pudo parsear %s\n", apk);
            return 1;
        }
        printf("APK:          %s\n", apk);
        printf("Package:      %s\n", info.package);
        printf("Activity:     %s\n", info.activity);
        printf("Label:        %s\n", info.label);
        printf("Version:      %s (%d)\n", info.version_name, info.version_code);
        printf("SDK:          min=%d  target=%d\n", info.sdk_min, info.sdk_target);
        return 0;
    }

    // ── install ───────────────────────────────────────────────────────────────
    if (strcmp(subcmd, "install") == 0) {
        if (argi >= argc) { fprintf(stderr, "install requiere <apk>\n"); return 1; }
        const char *apk = argv[argi];

        // Esperar dispositivo
        char serial[MAX_PATH];
        if (!find_device(serial, sizeof(serial))) {
            printf("[aine-launcher] Esperando dispositivo...\n");
            if (wait_for_device(60) < 0) {
                fprintf(stderr, "[FAIL] No hay dispositivo conectado\n");
                return 1;
            }
            find_device(serial, sizeof(serial));
        }
        if (serial[0] && !g_adb_serial[0])
            strncpy(g_adb_serial, serial, sizeof(g_adb_serial) - 1);
        printf("[aine-launcher] Dispositivo: %s\n",
               g_adb_serial[0] ? g_adb_serial : "(default)");

        return install_apk(apk) == 0 ? 0 : 1;
    }

    // ── launch ────────────────────────────────────────────────────────────────
    if (strcmp(subcmd, "launch") == 0) {
        if (argi >= argc) { fprintf(stderr, "launch requiere <apk>\n"); return 1; }
        const char *apk = argv[argi];

        ApkInfo info;
        if (apk_info(apk, &info) < 0) {
            fprintf(stderr, "[FAIL] No se pudo parsear %s\n", apk);
            return 1;
        }
        printf("[aine-launcher] Package:  %s\n", info.package);
        printf("[aine-launcher] Activity: %s\n", info.activity);

        // Dispositivo
        char serial[MAX_PATH];
        if (!find_device(serial, sizeof(serial))) {
            printf("[aine-launcher] Esperando dispositivo...\n");
            wait_for_device(60);
            find_device(serial, sizeof(serial));
        }
        if (serial[0] && !g_adb_serial[0])
            strncpy(g_adb_serial, serial, sizeof(g_adb_serial) - 1);

        if (install_apk(apk) < 0) return 1;
        if (start_activity(info.package, info.activity) < 0) return 1;
        return 0;
    }

    // ── lifecycle ─────────────────────────────────────────────────────────────
    if (strcmp(subcmd, "lifecycle") == 0) {
        if (argi >= argc) { fprintf(stderr, "lifecycle requiere <apk>\n"); return 1; }
        const char *apk = argv[argi];

        ApkInfo info;
        if (apk_info(apk, &info) < 0) {
            fprintf(stderr, "[FAIL] No se pudo parsear %s\n", apk);
            return 1;
        }
        printf("[aine-launcher] === Test de ciclo de vida M3 ===\n");
        printf("[aine-launcher] APK:      %s\n", apk);
        printf("[aine-launcher] Package:  %s\n", info.package);
        printf("[aine-launcher] Activity: %s\n", info.activity);

        // Dispositivo
        char serial[MAX_PATH];
        if (!find_device(serial, sizeof(serial))) {
            printf("[aine-launcher] Esperando dispositivo...\n");
            if (wait_for_device(60) < 0) {
                fprintf(stderr, "[FAIL] No hay dispositivo\n");
                return 1;
            }
            find_device(serial, sizeof(serial));
        }
        if (serial[0] && !g_adb_serial[0])
            strncpy(g_adb_serial, serial, sizeof(g_adb_serial) - 1);
        printf("[aine-launcher] Dispositivo: %s\n",
               g_adb_serial[0] ? g_adb_serial : "(default)");

        // Instalar
        if (install_apk(apk) < 0) return 1;

        // 1. Limpiar buffer de logcat
        {
            char clearcmd[MAX_PATH * 2];
            adb_cmd(clearcmd, sizeof(clearcmd), "logcat -c 2>/dev/null");
            run_silent(clearcmd);
        }

        // 2. Abrir logcat ANTES de lanzar la app para no perder onCreate/onStart
        char logcat_cmd[MAX_PATH * 2 + 64];
        {
            char subcmd[128];
            snprintf(subcmd, sizeof(subcmd), "logcat -s AINE-M3:I 2>&1");
            char base[MAX_PATH * 2];
            adb_cmd(base, sizeof(base), subcmd);
            strncpy(logcat_cmd, base, sizeof(logcat_cmd) - 1);
        }
        pid_t logcat_pid = -1;
        FILE *logcat_f = popen_pid(logcat_cmd, &logcat_pid);
        if (!logcat_f) {
            fprintf(stderr, "[FAIL] No se puede abrir logcat\n");
            return 1;
        }
        // Pequeña pausa para que el proceso logcat arranque
        usleep(200 * 1000);  // 200ms

        // 3. Lanzar activity
        if (start_activity(info.package, info.activity) < 0) {
            pclose_pid(logcat_f, logcat_pid);
            return 1;
        }

        // 4. Monitorear ciclo de vida (hasta 15 segundos)
        int lc = monitor_lifecycle("AINE-M3", 15, logcat_f, logcat_pid);
        pclose_pid(logcat_f, logcat_pid);
        if (lc < 0) return 1;

        // Mostrar resumen
        printf("\n[aine-launcher] === Resultado del ciclo de vida ===\n");
        printf("  onCreate  : %s\n", (lc & LC_ONCREATE)  ? "✓" : "✗");
        printf("  onStart   : %s\n", (lc & LC_ONSTART)   ? "✓" : "✗");
        printf("  onResume  : %s\n", (lc & LC_ONRESUME)  ? "✓" : "✗");
        printf("  onPause   : %s\n", (lc & LC_ONPAUSE)   ? "✓" : "✗");
        printf("  onStop    : %s\n", (lc & LC_ONSTOP)    ? "✓" : "✗");
        printf("  onDestroy : %s\n", (lc & LC_ONDESTROY) ? "✓" : "✗");

        int pass = (lc & LC_FULL) == LC_FULL;
        if (pass) {
            printf("\n[PASS] Ciclo de vida M3 completo ✓\n");
            return 0;
        } else {
            int missing = LC_FULL & ~lc;
            printf("\n[FAIL] Ciclo incompleto — falta:");
            if (missing & LC_ONCREATE)  printf(" onCreate");
            if (missing & LC_ONSTART)   printf(" onStart");
            if (missing & LC_ONRESUME)  printf(" onResume");
            if (missing & LC_ONPAUSE)   printf(" onPause");
            if (missing & LC_ONSTOP)    printf(" onStop");
            if (missing & LC_ONDESTROY) printf(" onDestroy");
            printf("\n");
            return 1;
        }
    }

    fprintf(stderr, "Subcomando desconocido: %s\n", subcmd);
    usage(argv[0]);
    return 1;
}
