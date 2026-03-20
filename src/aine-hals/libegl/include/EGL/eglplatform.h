/* EGL/eglplatform.h — platform-specific EGL types for AINE (macOS ARM64)
 * Based on Khronos EGL 1.4 specification.
 */
#ifndef AINE_EGL_EGLPLATFORM_H
#define AINE_EGL_EGLPLATFORM_H

#include <stdint.h>

/* Native types for macOS */
typedef void*              EGLNativeDisplayType;    /* Not used on macOS */
typedef void*              EGLNativeWindowType;     /* CAMetalLayer* */
typedef void*              EGLNativePixmapType;     /* IOSurface* */

typedef intptr_t           EGLAttrib;

#endif /* AINE_EGL_EGLPLATFORM_H */
