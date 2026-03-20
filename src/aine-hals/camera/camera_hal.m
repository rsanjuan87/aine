/*
 * src/aine-hals/camera/camera_hal.m — AINE Camera HAL (AVCaptureSession backend)
 *
 * Maps android.hardware.Camera2 semantics to AVFoundation on macOS ARM64.
 * Works headlessly (no crash) when no camera is present.
 *
 * Thread model:
 *   - open/start/stop/close may be called from any thread.
 *   - frame_cb is called on a high-priority dispatch queue.
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#include "camera_hal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Internal device struct ─────────────────────────────────────────── */
@interface AineCaptureDelegateObjC : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate,
                                               AVCapturePhotoCaptureDelegate>
@property (nonatomic) AineCameraFrameCb frameCb;
@property (nonatomic) void             *userdata;
@property (nonatomic) int               pixFmt;
@end

@implementation AineCaptureDelegateObjC

- (void)captureOutput:(AVCaptureOutput *)output
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection
{
    if (!self.frameCb) return;
    CVImageBufferRef imgBuf = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!imgBuf) return;
    CVPixelBufferLockBaseAddress(imgBuf, kCVPixelBufferLock_ReadOnly);
    const uint8_t *base = (const uint8_t *)CVPixelBufferGetBaseAddress(imgBuf);
    size_t size = CVPixelBufferGetDataSize(imgBuf);
    if (base && size > 0) self.frameCb(base, size, self.userdata);
    CVPixelBufferUnlockBaseAddress(imgBuf, kCVPixelBufferLock_ReadOnly);
}

/* AVCapturePhotoCaptureDelegate — for still capture */
- (void)captureOutput:(AVCapturePhotoOutput *)output
didFinishProcessingPhoto:(AVCapturePhoto *)photo
                error:(NSError *)error API_AVAILABLE(macos(10.15))
{
    if (!self.frameCb || error) return;
    NSData *data = [photo fileDataRepresentation];
    if (data) self.frameCb((const uint8_t *)data.bytes, data.length, self.userdata);
}

@end

/* ── AineCameraDevice (C wrapper) ───────────────────────────────────── */
struct AineCameraDevice {
    int                        camera_id;
    int                        width, height, pixel_format;
    AVCaptureSession          *session;
    AVCaptureDeviceInput      *input;
    AVCaptureVideoDataOutput  *video_output;
    AVCapturePhotoOutput      *photo_output;
    AineCaptureDelegateObjC   *delegate;
    dispatch_queue_t           queue;
};

/* ── enumerate ──────────────────────────────────────────────────────── */
int aine_cam_enumerate(int *ids_out, int max_ids)
{
    if (!ids_out || max_ids <= 0) return 0;

#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500
    AVCaptureDeviceDiscoverySession *ds =
        [AVCaptureDeviceDiscoverySession
            discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInWideAngleCamera,
                                              AVCaptureDeviceTypeExternalUnknown]
                                  mediaType:AVMediaTypeVideo
                                   position:AVCaptureDevicePositionUnspecified];
    NSArray<AVCaptureDevice *> *devs = ds.devices;
#else
    NSArray<AVCaptureDevice *> *devs =
        [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
#endif

    int count = 0;
    for (AVCaptureDevice *d in devs) {
        (void)d;
        if (count >= max_ids) break;
        ids_out[count] = count;
        count++;
    }
    return count;
}

/* ── open ───────────────────────────────────────────────────────────── */
AineCameraDevice *aine_cam_open(int camera_id, int width, int height,
                                int pixel_format,
                                AineCameraFrameCb frame_cb, void *userdata)
{
#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500
    AVCaptureDeviceDiscoverySession *ds =
        [AVCaptureDeviceDiscoverySession
            discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInWideAngleCamera,
                                              AVCaptureDeviceTypeExternalUnknown]
                                  mediaType:AVMediaTypeVideo
                                   position:AVCaptureDevicePositionUnspecified];
    NSArray<AVCaptureDevice *> *devs = ds.devices;
#else
    NSArray<AVCaptureDevice *> *devs =
        [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
#endif

    AVCaptureDevice *device = nil;
    int idx = 0;
    for (AVCaptureDevice *d in devs) {
        if (idx == camera_id) { device = d; break; }
        idx++;
    }

    AineCameraDevice *cam = calloc(1, sizeof(AineCameraDevice));
    cam->camera_id    = camera_id;
    cam->width        = width;
    cam->height       = height;
    cam->pixel_format = pixel_format;

    if (!device) {
        fprintf(stderr, "[aine-camera] camera %d not found (headless mode)\n", camera_id);
        return cam; /* return stub that does nothing */
    }

    NSError *err = nil;
    cam->input    = [AVCaptureDeviceInput deviceInputWithDevice:device error:&err];
    if (err || !cam->input) {
        fprintf(stderr, "[aine-camera] deviceInput error: %s\n",
                err ? err.localizedDescription.UTF8String : "nil");
        return cam;
    }

    cam->session = [[AVCaptureSession alloc] init];
    [cam->session beginConfiguration];

    if ([cam->session canAddInput:cam->input])
        [cam->session addInput:cam->input];

    cam->delegate = [[AineCaptureDelegateObjC alloc] init];
    cam->delegate.frameCb  = frame_cb;
    cam->delegate.userdata = userdata;
    cam->delegate.pixFmt   = pixel_format;

    cam->queue = dispatch_queue_create("aine.camera", DISPATCH_QUEUE_SERIAL);

    cam->video_output = [[AVCaptureVideoDataOutput alloc] init];
    [cam->video_output setSampleBufferDelegate:cam->delegate queue:cam->queue];
    /* Request BGRA if NV21/YUV requested (macOS doesn't natively output NV21) */
    cam->video_output.videoSettings = @{
        (NSString *)kCVPixelBufferPixelFormatTypeKey :
            @(kCVPixelFormatType_32BGRA)
    };
    if ([cam->session canAddOutput:cam->video_output])
        [cam->session addOutput:cam->video_output];

    cam->photo_output = [[AVCapturePhotoOutput alloc] init];
    if ([cam->session canAddOutput:cam->photo_output])
        [cam->session addOutput:cam->photo_output];

    [cam->session commitConfiguration];
    fprintf(stderr, "[aine-camera] opened: %s  %dx%d\n",
            device.localizedName.UTF8String, width, height);
    return cam;
}

/* ── start ──────────────────────────────────────────────────────────── */
int aine_cam_start(AineCameraDevice *dev)
{
    if (!dev || !dev->session) return 0; /* headless stub — OK */
    [dev->session startRunning];
    return dev->session.isRunning ? 0 : -1;
}

/* ── stop ───────────────────────────────────────────────────────────── */
void aine_cam_stop(AineCameraDevice *dev)
{
    if (!dev || !dev->session) return;
    [dev->session stopRunning];
}

/* ── capture ────────────────────────────────────────────────────────── */
int aine_cam_capture(AineCameraDevice *dev)
{
    if (!dev || !dev->photo_output) return -1;
    if (@available(macOS 10.15, *)) {
        AVCapturePhotoSettings *settings =
            [AVCapturePhotoSettings photoSettingsWithFormat:
                @{AVVideoCodecKey : AVVideoCodecTypeJPEG}];
        [dev->photo_output capturePhotoWithSettings:settings
                                           delegate:dev->delegate];
    }
    return 0;
}

/* ── close ──────────────────────────────────────────────────────────── */
void aine_cam_close(AineCameraDevice *dev)
{
    if (!dev) return;
    if (dev->session) { [dev->session stopRunning]; dev->session = nil; }
    if (dev->queue)   { dev->queue = NULL; }
    dev->delegate = nil;
    free(dev);
}
