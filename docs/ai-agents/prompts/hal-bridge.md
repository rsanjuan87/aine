# Prompt: implementar HAL bridge

Usa este prompt para implementar puentes entre las HAL de Android y los frameworks de macOS.

---

## Prompt base

```
Contexto del proyecto AINE:
- Capa de compatibilidad Android → macOS (tipo Wine para Android en Apple Silicon)
- Los HAL bridges conectan las interfaces HAL de Android con los frameworks nativos de macOS
- Lenguaje: Objective-C++ (.mm) para acceder tanto a C++ de Android como a frameworks Apple
- Target: macOS 13+ ARM64

Necesito implementar el HAL bridge para [COMPONENTE].

Interfaz Android HAL (de AOSP):
[pegar interfaz de hardware/interfaces/[componente]/ o el header HAL clásico]

Framework macOS equivalente:
[CoreAudio / AVFoundation / CoreMotion / NSEvent / etc.]

Flujo de datos esperado:
Android Framework → [ruta] → HAL AINE → [framework macOS] → Hardware

Casos de uso que debe cubrir:
[lista de casos]

Por favor genera:
1. src/aine-hals/[componente]/[Componente]HAL.mm con la implementación
2. src/aine-hals/[componente]/CMakeLists.txt
3. Una función de registro del HAL que el system_server pueda usar
4. Tests básicos en tests/hals/test_[componente].mm

Restricciones:
- Usar ARC (Automatic Reference Counting) para objetos ObjC
- Manejar errores Android-style (return -EINVAL, etc.)
- Comentar las diferencias semánticas entre Android y macOS
- Documentar los casos donde la semántica no es idéntica (ej: sample rates disponibles)
```

---

## HALs por implementar (por milestone)

### M5 — Audio HAL (CoreAudio)

```
Interfaz Android: hardware/libhardware/include/hardware/audio.h
Framework macOS: CoreAudio + AudioToolbox (AudioUnit)

Flujo:
AudioFlinger (system_server)
  │  PCM buffers + metadata (sample_rate, channels, format)
  ▼
aine-audio-hal
  │  Crea AudioUnit con kAudioUnitSubType_DefaultOutput
  │  Configura formato: match sample_rate y channels
  │  render callback → recibe PCM de Android, lo pasa a CoreAudio
  ▼
CoreAudio → Altavoces/Auriculares

Casos edge importantes:
- Android usa AUDIO_FORMAT_PCM_16_BIT principalmente
- Puede pedir sample rates que macOS no soporta (resample si necesario)
- Latency requirements: Android mide en frames, CoreAudio en segundos
```

### M5 — Input HAL (NSEvent)

```
Interfaz Android: frameworks/native/include/input/Input.h
Framework macOS: NSApplication + NSEvent

Flujo:
NSApplication main run loop
  │  NSEvent (keyDown, mouseDown, mouseMoved, scrollWheel)
  ▼
aine-input-bridge
  │  Convierte NSEvent → struct input_event (Linux input subsystem)
  │  Mapeo:
  │    NSLeftMouseDown → BTN_LEFT
  │    NSKeyDown → KEY_[código]
  │    NSScrollWheel → REL_WHEEL
  │    NSMouseMoved → REL_X, REL_Y
  ▼
InputFlinger (via Binder) → App Android onTouchEvent/onKeyDown
```

### M5 — Camera HAL (AVFoundation)

```
Interfaz Android: hardware/interfaces/camera/device/3.x/
Framework macOS: AVCaptureSession + AVCaptureDevice

Flujo:
Android CameraService (via Binder)
  │  open(camera_id)
  ▼
aine-camera-hal
  │  AVCaptureDevice.default(.video)
  │  AVCaptureSession con preset 1080p
  │  AVCaptureVideoDataOutputSampleBufferDelegate
  │  Convierte CMSampleBuffer → YUV_420_888 (formato Android)
  ▼
Android app → CameraX / Camera2 API

Casos edge:
- En Mac no hay cámara trasera — reportar solo "0" (frontal/webcam)
- Formatos: Android quiere YUV_420_888, AVFoundation da kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
  → Conversión necesaria con vImage o libyuv
```
