// AINE: src/aine-shim/macos/platform.c
const char* aine_shim_os(void) { return "macOS"; }
int aine_shim_platform_is_macos(void) { return 1; }
int aine_shim_platform_is_linux(void) { return 0; }
