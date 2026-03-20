/*
 * test-apps/native-stub/native_stub.c
 *
 * Minimal native library used by F5 loader tests.
 * Built as libaine-native-stub.dylib (macOS ARM64 Mach-O).
 *
 * In a real APK the equivalent would be a JNI .so built for android-arm64.
 */
#include <stdio.h>

/* aine_native_test — returns 42, printed by aine-loader-test */
__attribute__((visibility("default")))
int aine_native_test(void)
{
    fprintf(stderr, "[native-stub] aine_native_test() called\n");
    return 42;
}

/* Simulate a JNI_OnLoad stub */
__attribute__((visibility("default")))
int JNI_OnLoad(void *vm, void *reserved)
{
    (void)vm; (void)reserved;
    fprintf(stderr, "[native-stub] JNI_OnLoad() called\n");
    return 65536; /* JNI_VERSION_1_6 */
}
