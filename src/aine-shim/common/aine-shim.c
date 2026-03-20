// AINE: src/aine-shim/common/aine-shim.c
// Inicialización portable del shim — sin código platform-específico
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const char* aine_shim_os(void);   // implementado en macos/ o linux/

__attribute__((constructor))
static void aine_shim_init(void) {
    const char *log_level = getenv("AINE_LOG_LEVEL");
    if (log_level && strcmp(log_level, "debug") == 0) {
        fprintf(stderr, "[AINE-shim] initialized on %s\n", aine_shim_os());
#ifdef AINE_ATL_COMMIT
        fprintf(stderr, "[AINE-shim] ATL base commit: %s\n", AINE_ATL_COMMIT);
#endif
    }
}
