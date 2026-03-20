// aine-hals/vulkan/vulkan_hal.h — AINE Vulkan HAL (MoltenVK backend)
// Provides Vulkan instance/device creation bridged to Metal via MoltenVK.
// Falls back to a no-op stub when MoltenVK is not installed.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AINE_VK_SUCCESS         =  0,
    AINE_VK_NOT_AVAILABLE   = -1,  // MoltenVK not installed
    AINE_VK_ERROR           = -2,
} AineVkResult;

// Returns 1 if a Vulkan capable driver (MoltenVK) is present, 0 otherwise.
int aine_vk_available(void);

// Initialize the Vulkan HAL.  Must be called before any other vk function.
// Returns AINE_VK_SUCCESS or AINE_VK_NOT_AVAILABLE (caller should fall back
// to EGL/Metal).
AineVkResult aine_vk_init(void);

// Teardown.
void aine_vk_shutdown(void);

// Get vendor/device string for logging.
const char *aine_vk_device_name(void);

#ifdef __cplusplus
}
#endif
