// AINE: src/aine-shim/linux/platform.c
const char* aine_shim_os(void) { return "Linux"; }
int aine_shim_platform_is_macos(void) { return 0; }
int aine_shim_platform_is_linux(void) { return 1; }
