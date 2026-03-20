// AINE: src/aine-binder/linux/binder-daemon.cpp
// Binder daemon para Linux — basado en ATL, con logging AINE
// En Linux podemos usar el Binder userspace de ATL directamente.
// AINE añade el sistema de arranque unificado y logging consistente.
// TODO M2: integrar con vendor/atl/binder/

#include <stdio.h>
#include <stdlib.h>

extern "C" int aine_binder_linux_start(void) {
    // Detectar backend (kernel module vs userspace ATL)
    // Ver: src/aine-shim/linux/binder-dev.c
    fprintf(stderr, "[AINE-binder/linux] starting — TODO: integrate ATL binder\n");
    return 0;
}
