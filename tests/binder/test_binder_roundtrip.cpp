// AINE: tests/binder/test_binder_roundtrip.cpp
// Test M2: round-trip cliente → daemon → reply en < 5ms
// Verifica que el daemon responde a listServices con al menos 1 servicio.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// API pública de aine-binder
#ifdef __cplusplus
extern "C" {
#endif
int aine_binder_open(void);
int aine_binder_close(int fd);
int aine_svc_list(int binder_fd, int index, char *out, size_t out_size);
int aine_svc_add(int binder_fd, const char *name);
#ifdef __cplusplus
}
#endif

static double elapsed_ms(struct timespec *start, struct timespec *end) {
    return (end->tv_sec  - start->tv_sec)  * 1000.0
         + (end->tv_nsec - start->tv_nsec) / 1.0e6;
}

static int check(const char *label, int cond) {
    if (cond) {
        printf("[PASS] %s\n", label);
        return 0;
    } else {
        printf("[FAIL] %s\n", label);
        return 1;
    }
}

int main(void) {
    int failures = 0;

    // ──────────────────────────────────────────────────────────────
    // 1. Abrir fd binder (arranca daemon si es necesario)
    // ──────────────────────────────────────────────────────────────
    int fd = aine_binder_open();
    failures += check("aine_binder_open devuelve fd válido", fd >= 0);
    if (fd < 0) {
        printf("[ABORT] No se puede continuar sin fd binder\n");
        return 1;
    }

    // ──────────────────────────────────────────────────────────────
    // 2. El daemon pre-registra IServiceManager — índice 0 existe
    // ──────────────────────────────────────────────────────────────
    char name[256];
    int r = aine_svc_list(fd, 0, name, sizeof(name));
    failures += check("SVC_MGR_LIST_SERVICES[0] devuelve servicio", r == 0);
    if (r == 0) {
        failures += check("Servicio 0 es android.os.IServiceManager",
                          strcmp(name, "android.os.IServiceManager") == 0);
        printf("       nombre: %s\n", name);
    }

    // ──────────────────────────────────────────────────────────────
    // 3. Latencia del round-trip < 5ms
    // ──────────────────────────────────────────────────────────────
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    aine_svc_list(fd, 0, name, sizeof(name));
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = elapsed_ms(&t0, &t1);
    printf("       latencia round-trip: %.3f ms\n", ms);
    failures += check("Latencia round-trip < 5ms", ms < 5.0);

    // ──────────────────────────────────────────────────────────────
    // 4. Registrar un servicio y volver a listarlo
    // ──────────────────────────────────────────────────────────────
    const char *test_svc = "com.aine.test.ITestService";
    r = aine_svc_add(fd, test_svc);
    failures += check("aine_svc_add registra servicio", r == 0);

    // Buscar el servicio recién añadido en la lista
    int found = 0;
    for (int i = 0; i < 32; i++) {
        char tmp[256];
        if (aine_svc_list(fd, i, tmp, sizeof(tmp)) < 0) break;
        if (strcmp(tmp, test_svc) == 0) { found = 1; break; }
    }
    failures += check("Servicio añadido aparece en la lista", found);

    // ──────────────────────────────────────────────────────────────
    // 5. Índice fuera de rango devuelve -1 (fin de lista)
    // ──────────────────────────────────────────────────────────────
    r = aine_svc_list(fd, 9999, name, sizeof(name));
    failures += check("Índice fuera de rango devuelve -1", r < 0);

    aine_binder_close(fd);

    printf("\n%s — %d fallo(s)\n",
           failures == 0 ? "TODOS LOS TESTS PASARON" : "ALGUNOS TESTS FALLARON",
           failures);
    return failures == 0 ? 0 : 1;
}
