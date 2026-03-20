/*
 * tests/macos/test_f12_hals.c — CTest for F12 AINE HALs
 *
 * Tests:
 *   1. Vulkan HAL: aine_vk_init() does not crash (MoltenVK optional)
 *   2. Camera HAL: aine_cam_enumerate() does not crash (0 cameras OK)
 *   3. Camera HAL: aine_cam_open(999, …) returns non-NULL stub (headless OK)
 *   4. Camera HAL: aine_cam_start/stop/close stub is safe
 *   5. Clipboard: aine_clip_set_text roundtrip
 *   6. Clipboard: aine_clip_has_text() returns 1 after set
 *   7. Clipboard: aine_clip_get_text() retrieves value
 *   8. Clipboard: aine_clip_clear() does not crash
 *
 * All tests must pass on a headless CI Mac with no camera and no MoltenVK.
 */

#include <stdio.h>
#include <string.h>

#include "vulkan_hal.h"
#include "camera_hal.h"
#include "clipboard_hal.h"

static int s_pass = 0, s_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [ok]   %s\n", msg); s_pass++; } \
    else      { printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); s_fail++; } \
} while (0)

int main(void)
{
    printf("=== F12 HALs test ===\n");

    /* ── T12.2 Vulkan ─────────────────────────────────────── */
    printf("-- Vulkan HAL --\n");
    AineVkResult vk = aine_vk_init();
    /* Either SUCCESS (MoltenVK present) or NOT_AVAILABLE (not installed) */
    CHECK(vk == AINE_VK_SUCCESS || vk == AINE_VK_NOT_AVAILABLE,
          "vk_init: does not crash");
    const char *dn = aine_vk_device_name();
    CHECK(dn != NULL, "vk_device_name: not NULL");
    aine_vk_shutdown();
    CHECK(1, "vk_shutdown: does not crash");

    /* ── T12.3 Camera ─────────────────────────────────────── */
    printf("-- Camera HAL --\n");
    int ids[8] = {0};
    int ncam = aine_cam_enumerate(ids, 8);
    CHECK(ncam >= 0, "cam_enumerate: no crash");
    printf("  [info] cameras found: %d\n", ncam);

    /* Open a non-existent camera ID → headless stub */
    AineCameraDevice *dev = aine_cam_open(999, 640, 480,
                                          AINE_CAM_FMT_NV21, NULL, NULL);
    CHECK(dev != NULL, "cam_open(stub): returns non-NULL");
    int started = aine_cam_start(dev);
    CHECK(started == 0, "cam_start(stub): returns 0");
    aine_cam_stop(dev);
    CHECK(1, "cam_stop(stub): does not crash");
    aine_cam_close(dev);
    CHECK(1, "cam_close(stub): does not crash");

    /* ── T12.4 Clipboard ──────────────────────────────────── */
    printf("-- Clipboard HAL --\n");
    int set_ok = aine_clip_set_text("AINE-beta-test");
    CHECK(set_ok == 0, "clip_set_text: returns 0");
    CHECK(aine_clip_has_text() == 1, "clip_has_text: 1 after set");

    char buf[64] = {0};
    int got = aine_clip_get_text(buf, sizeof(buf));
    CHECK(got > 0 && strcmp(buf, "AINE-beta-test") == 0,
          "clip_get_text: roundtrip");

    aine_clip_clear();
    CHECK(1, "clip_clear: does not crash");

    /* ── Result ────────────────────────────────────────────── */
    printf("RESULT: %d ok, %d failed\n", s_pass, s_fail);
    return s_fail == 0 ? 0 : 1;
}
