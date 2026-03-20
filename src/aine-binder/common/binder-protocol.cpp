// AINE: src/aine-binder/common/binder-protocol.cpp
// Protocolo Binder BC_*/BR_* — idéntico en Linux y macOS
// Basado en ATL (vendor/atl/binder/) — solo el transporte cambia
// TODO M2: copiar e integrar desde vendor/atl/binder/

// Comandos cliente → driver
#define BC_TRANSACTION       0x40406300
#define BC_REPLY             0x40406301
#define BC_ACQUIRE_RESULT    0x40046302
#define BC_FREE_BUFFER       0x40086303
#define BC_INCREFS           0x40046304
#define BC_ACQUIRE           0x40046305
#define BC_RELEASE           0x40046306
#define BC_DECREFS           0x40046307
#define BC_REGISTER_LOOPER   0x00006308
#define BC_ENTER_LOOPER      0x00006309
#define BC_EXIT_LOOPER       0x0000630a

// Respuestas driver → cliente
#define BR_ERROR             0x80046300
#define BR_OK                0x00006301
#define BR_TRANSACTION       0x80886302
#define BR_REPLY             0x80886303
#define BR_ACQUIRE_RESULT    0x80046304
#define BR_DEAD_REPLY        0x00006305
#define BR_TRANSACTION_COMPLETE 0x00006306
#define BR_INCREFS           0x80086307
#define BR_ACQUIRE           0x80086308
#define BR_RELEASE           0x80086309
#define BR_DECREFS           0x8008630a
#define BR_NOOP              0x0000630c
#define BR_SPAWN_LOOPER      0x0000630d
#define BR_DEAD_BINDER       0x80086315
#define BR_CLEAR_DEATH_NOTIFICATION_DONE 0x80086316
#define BR_FAILED_REPLY      0x00006317
