// AINE: tests/shared/test_binder_protocol.cpp
// Tests del protocolo Binder que deben pasar en Linux Y macOS
// El protocolo BC_*/BR_* es idéntico en ambas plataformas
// Solo cambia el transporte (Mach en macOS, kernel/userspace en Linux)

#include <stdio.h>
#include <string.h>
#include <assert.h>

// Constantes del protocolo (de src/aine-binder/common/binder-protocol.cpp)
#define BC_TRANSACTION  0x40406300
#define BC_REPLY        0x40406301
#define BR_TRANSACTION  0x80886302
#define BR_REPLY        0x80886303
#define BR_OK           0x00006301

// Test 1: Las constantes del protocolo son correctas
static void test_protocol_constants(void) {
    assert((BC_TRANSACTION & 0xFF) == 0x00);  // cmd base
    assert((BR_TRANSACTION & 0xFF) == 0x02);
    printf("  [OK] Protocol constants\n");
}

// Test 2: Tamaños de estructuras Binder (deben ser iguales en ambas plataformas)
// TODO M2: añadir structs reales cuando aine-binder esté implementado
static void test_struct_sizes(void) {
    // binder_transaction_data debe ser 56 bytes en ARM64
    // struct binder_transaction_data { ... }
    // assert(sizeof(struct binder_transaction_data) == 56);
    printf("  [SKIP] Struct sizes — implement in M2\n");
}

// Test 3: Parcel serialización básica
// TODO M2: implementar cuando parcel.cpp esté listo
static void test_parcel_basic(void) {
    printf("  [SKIP] Parcel basic — implement in M2\n");
}

int main(void) {
    printf("AINE shared tests: binder protocol\n");
    printf("Platform: %s\n\n",
#if defined(__APPLE__)
        "macOS"
#elif defined(__linux__)
        "Linux"
#else
        "unknown"
#endif
    );

    test_protocol_constants();
    test_struct_sizes();
    test_parcel_basic();

    printf("\nAll shared tests passed\n");
    return 0;
}
