// AINE: prctl → Darwin equivalents (B4 blocker)
// pthread_setname_np en Linux toma (thread, name), en macOS solo (name)
// Android usa prctl(PR_SET_NAME) para nombrar threads
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>

// Opciones de prctl relevantes para ART/Android
#define PR_SET_NAME      15
#define PR_GET_NAME      16
#define PR_SET_DUMPABLE   4
#define PR_GET_DUMPABLE   3
#define PR_SET_NO_NEW_PRIVS 38
#define PR_SET_PDEATHSIG  1

// AINE: implementación de prctl para macOS
// Solo las opciones que Android/ART realmente usa
int aine_prctl(int option, unsigned long arg2, unsigned long arg3,
               unsigned long arg4, unsigned long arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    switch (option) {
        case PR_SET_NAME: {
            // AINE (B4): Linux pasa (thread, name) pero macOS solo acepta name
            // para el hilo actual. ART siempre llama desde el hilo a nombrar.
            const char *name = (const char *)(uintptr_t)arg2;
            // macOS: nombre máximo 63 chars (MAXTHREADNAMESIZE)
            char truncated[64];
            strncpy(truncated, name, 63);
            truncated[63] = '\0';
            pthread_setname_np(truncated);
            return 0;
        }
        case PR_GET_NAME: {
            char *buf = (char *)(uintptr_t)arg2;
            pthread_getname_np(pthread_self(), buf, 16);
            return 0;
        }
        case PR_SET_DUMPABLE:
        case PR_GET_DUMPABLE:
            // AINE: stub — devolver 1 (dumpable), Android lo lee pero no es crítico
            return (option == PR_GET_DUMPABLE) ? 1 : 0;
        case PR_SET_NO_NEW_PRIVS:
            return 0; // silencio — no aplicable en macOS userspace
        case PR_SET_PDEATHSIG:
            return 0; // AINE: ignorado, sin equivalente directo en macOS
        default:
            errno = EINVAL;
            return -1;
    }
}

// AINE (B4): shim de pthread_setname_np con firma Linux
// Linux: int pthread_setname_np(pthread_t thread, const char *name)
// macOS: int pthread_setname_np(const char *name)  ← solo hilo actual
// Android siempre llama desde el propio hilo, así que el shim es correcto.
int aine_pthread_setname_np(pthread_t thread, const char *name) {
    if (thread == pthread_self()) {
        char buf[64];
        strncpy(buf, name, 63);
        buf[63] = '\0';
        return pthread_setname_np(buf);
    }
    // AINE: no hay API para renombrar otro hilo en macOS — ignorar silenciosamente
    return 0;
}
