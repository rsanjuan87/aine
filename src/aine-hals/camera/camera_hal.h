// aine-hals/camera/camera_hal.h — AINE Camera HAL
// Maps android.hardware.Camera2 / CameraX to AVCaptureSession (macOS ARM64).
// Headless-safe: preview/capture is a no-op when no camera is available.
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pixel formats (Android subset) */
#define AINE_CAM_FMT_JPEG   0x100
#define AINE_CAM_FMT_NV21   0x11
#define AINE_CAM_FMT_YUV420 0x23

/* Opaque camera device handle */
typedef struct AineCameraDevice AineCameraDevice;

/* Called when a new frame arrives.
 * data     — raw pixel data in the format requested at open()
 * size     — byte count
 * userdata — the pointer passed to aine_cam_open() */
typedef void (*AineCameraFrameCb)(const uint8_t *data, size_t size,
                                  void *userdata);

/* List available camera IDs (0 = rear / built-in, 1 = front FaceTime HD).
 * Returns number of cameras found (may be 0 on headless systems). */
int aine_cam_enumerate(int *ids_out, int max_ids);

/* Open camera by id.  frame_cb may be NULL (preview only).
 * Returns handle or NULL on error. */
AineCameraDevice *aine_cam_open(int camera_id, int width, int height,
                                int pixel_format,
                                AineCameraFrameCb frame_cb, void *userdata);

/* Start streaming. */
int aine_cam_start(AineCameraDevice *dev);

/* Stop streaming. */
void aine_cam_stop(AineCameraDevice *dev);

/* Trigger a still capture.  Result delivered via frame_cb with JPEG data. */
int aine_cam_capture(AineCameraDevice *dev);

/* Release and free all resources. */
void aine_cam_close(AineCameraDevice *dev);

#ifdef __cplusplus
}
#endif
