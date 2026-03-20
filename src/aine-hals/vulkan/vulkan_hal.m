/*
 * src/aine-hals/vulkan/vulkan_hal.m — AINE Vulkan HAL (MoltenVK backend)
 *
 * Strategy:
 *   1. Try dlopen("libMoltenVK.dylib") — present when MoltenVK is installed
 *      via Vulkan SDK (brew install molten-vk or vulkan-sdk).
 *   2. If found, resolve vkCreateInstance / vkEnumeratePhysicalDevices, etc.
 *      via dlsym and use them for the real Vulkan path.
 *   3. If not found, return AINE_VK_NOT_AVAILABLE so callers fall back to
 *      the EGL/Metal pipeline that is always available.
 *
 * We do NOT embed MoltenVK source — we dynamically detect it at runtime.
 */

#import <Foundation/Foundation.h>
#include "vulkan_hal.h"

#include <dlfcn.h>
#include <string.h>
#include <stdio.h>

/* ── Internal state ───────────────────────────────────────────────────── */
static void        *g_mvk_handle   = NULL;
static int          g_available    = 0;
static char         g_device_name[256] = "unknown";

/* MoltenVK search paths */
static const char * const k_mvk_paths[] = {
    "/usr/local/lib/libMoltenVK.dylib",
    "/opt/homebrew/lib/libMoltenVK.dylib",
    "/usr/lib/libMoltenVK.dylib",
    /* Vulkan SDK default install */
    "/usr/local/share/vulkan/icd.d/../../../lib/libMoltenVK.dylib",
    NULL
};

/* Minimal Vulkan types needed just for device enumeration */
typedef void* VkInstance_T;
typedef void* VkPhysicalDevice_T;
typedef uint32_t VkResult_vk;  /* 0 = VK_SUCCESS */

typedef struct {
    uint32_t sType;   /* 1 = VK_STRUCTURE_TYPE_APPLICATION_INFO */
    const void *pNext;
    const char *pApplicationName;
    uint32_t    applicationVersion;
    const char *pEngineName;
    uint32_t    engineVersion;
    uint32_t    apiVersion;
} VkApplicationInfo;

typedef struct {
    uint32_t sType;   /* 32 = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO */
    const void *pNext;
    uint32_t    flags;
    const VkApplicationInfo *pApplicationInfo;
    uint32_t    enabledLayerCount;
    const char * const *ppEnabledLayerNames;
    uint32_t    enabledExtensionCount;
    const char * const *ppEnabledExtensionNames;
} VkInstanceCreateInfo;

typedef struct {
    char deviceName[256];
    /* We only read deviceName, skip the rest */
} VkPhysicalDeviceProperties_stub;

typedef VkResult_vk (*PFN_vkCreateInstance)(
    const VkInstanceCreateInfo *, const void *, VkInstance_T **);
typedef VkResult_vk (*PFN_vkEnumeratePhysicalDevices)(
    VkInstance_T *, uint32_t *, VkPhysicalDevice_T **);
typedef void        (*PFN_vkGetPhysicalDeviceProperties)(
    VkPhysicalDevice_T *, VkPhysicalDeviceProperties_stub *);
typedef void        (*PFN_vkDestroyInstance)(VkInstance_T *, const void *);

/* ── Public API ─────────────────────────────────────────────────────── */
int aine_vk_available(void) { return g_available; }

AineVkResult aine_vk_init(void)
{
    if (g_mvk_handle) return AINE_VK_SUCCESS; /* already init */

    /* Try each search path */
    for (int i = 0; k_mvk_paths[i]; i++) {
        g_mvk_handle = dlopen(k_mvk_paths[i], RTLD_LAZY | RTLD_LOCAL);
        if (g_mvk_handle) break;
    }

    if (!g_mvk_handle) {
        fprintf(stderr, "[aine-vulkan] MoltenVK not found — Vulkan unavailable\n");
        return AINE_VK_NOT_AVAILABLE;
    }

    PFN_vkCreateInstance create =
        (PFN_vkCreateInstance)dlsym(g_mvk_handle, "vkCreateInstance");
    PFN_vkEnumeratePhysicalDevices enumerate =
        (PFN_vkEnumeratePhysicalDevices)dlsym(g_mvk_handle,
                                              "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceProperties get_props =
        (PFN_vkGetPhysicalDeviceProperties)dlsym(g_mvk_handle,
                                                  "vkGetPhysicalDeviceProperties");
    PFN_vkDestroyInstance destroy =
        (PFN_vkDestroyInstance)dlsym(g_mvk_handle, "vkDestroyInstance");

    if (!create || !enumerate || !get_props) {
        fprintf(stderr, "[aine-vulkan] MoltenVK symbols not found\n");
        dlclose(g_mvk_handle); g_mvk_handle = NULL;
        return AINE_VK_ERROR;
    }

    /* Probe device name via a throw-away instance */
    VkApplicationInfo app_info = {
        .sType = 1, /* VK_STRUCTURE_TYPE_APPLICATION_INFO */
        .pApplicationName = "AINE",
        .applicationVersion = 1,
        .pEngineName = "AINE-HAL",
        .engineVersion = 1,
        .apiVersion = (1 << 22) | (0 << 12) | 0  /* VK 1.0.0 */
    };
    VkInstanceCreateInfo ci = {
        .sType = 32, /* VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO */
        .pApplicationInfo = &app_info,
    };

    VkInstance_T *inst = NULL;
    if (create(&ci, NULL, &inst) == 0 && inst) {
        uint32_t count = 1;
        VkPhysicalDevice_T *gpu = NULL;
        if (enumerate(inst, &count, &gpu) == 0 && gpu) {
            VkPhysicalDeviceProperties_stub props;
            get_props(gpu, &props);
            strncpy(g_device_name, props.deviceName,
                    sizeof(g_device_name) - 1);
        }
        if (destroy) destroy(inst, NULL);
    }

    g_available = 1;
    fprintf(stderr, "[aine-vulkan] MoltenVK ready — device: %s\n", g_device_name);
    return AINE_VK_SUCCESS;
}

void aine_vk_shutdown(void)
{
    if (g_mvk_handle) { dlclose(g_mvk_handle); g_mvk_handle = NULL; }
    g_available = 0;
}

const char *aine_vk_device_name(void) { return g_device_name; }
