/* EGL/egl.h — Khronos EGL 1.4 header for AINE (macOS ARM64 / Metal backend)
 * Subset sufficient for OpenGL ES 2.0 Android apps.
 */
#ifndef AINE_EGL_EGL_H
#define AINE_EGL_EGL_H

#include "eglplatform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * EGL base types
 * ---------------------------------------------------------------------- */
typedef unsigned int  EGLenum;
typedef unsigned int  EGLBoolean;
typedef int           EGLint;
typedef void*         EGLDisplay;
typedef void*         EGLConfig;
typedef void*         EGLSurface;
typedef void*         EGLContext;
typedef void          (*__eglMustCastToProperFunctionPointerType)(void);

/* -----------------------------------------------------------------------
 * EGL constants
 * ---------------------------------------------------------------------- */
#define EGL_FALSE                           0
#define EGL_TRUE                            1

#define EGL_DEFAULT_DISPLAY                 ((EGLNativeDisplayType)0)
#define EGL_NO_CONTEXT                      ((EGLContext)0)
#define EGL_NO_DISPLAY                      ((EGLDisplay)0)
#define EGL_NO_SURFACE                      ((EGLSurface)0)

/* EGL error codes */
#define EGL_SUCCESS                         0x3000
#define EGL_NOT_INITIALIZED                 0x3001
#define EGL_BAD_ACCESS                      0x3002
#define EGL_BAD_ALLOC                       0x3003
#define EGL_BAD_ATTRIBUTE                   0x3004
#define EGL_BAD_CONFIG                      0x3005
#define EGL_BAD_CONTEXT                     0x3006
#define EGL_BAD_CURRENT_SURFACE             0x3007
#define EGL_BAD_DISPLAY                     0x3008
#define EGL_BAD_MATCH                       0x3009
#define EGL_BAD_NATIVE_PIXMAP               0x300A
#define EGL_BAD_NATIVE_WINDOW               0x300B
#define EGL_BAD_PARAMETER                   0x300C
#define EGL_BAD_SURFACE                     0x300D
#define EGL_CONTEXT_LOST                    0x300E

/* Config attributes */
#define EGL_BUFFER_SIZE                     0x3020
#define EGL_ALPHA_SIZE                      0x3021
#define EGL_BLUE_SIZE                       0x3022
#define EGL_GREEN_SIZE                      0x3023
#define EGL_RED_SIZE                        0x3024
#define EGL_DEPTH_SIZE                      0x3025
#define EGL_STENCIL_SIZE                    0x3026
#define EGL_CONFIG_CAVEAT                   0x3027
#define EGL_CONFIG_ID                       0x3028
#define EGL_LEVEL                           0x3029
#define EGL_MAX_PBUFFER_HEIGHT              0x302A
#define EGL_MAX_PBUFFER_PIXELS              0x302B
#define EGL_MAX_PBUFFER_WIDTH               0x302C
#define EGL_NATIVE_RENDERABLE               0x302D
#define EGL_NATIVE_VISUAL_ID                0x302E
#define EGL_NATIVE_VISUAL_TYPE              0x302F
#define EGL_SAMPLES                         0x3031
#define EGL_SAMPLE_BUFFERS                  0x3032
#define EGL_SURFACE_TYPE                    0x3033
#define EGL_TRANSPARENT_TYPE                0x3034
#define EGL_TRANSPARENT_BLUE_VALUE          0x3035
#define EGL_TRANSPARENT_GREEN_VALUE         0x3036
#define EGL_TRANSPARENT_RED_VALUE           0x3037
#define EGL_NONE                            0x3038
#define EGL_BIND_TO_TEXTURE_RGB             0x3039
#define EGL_BIND_TO_TEXTURE_RGBA            0x303A
#define EGL_MIN_SWAP_INTERVAL               0x303B
#define EGL_MAX_SWAP_INTERVAL               0x303C
#define EGL_LUMINANCE_SIZE                  0x303D
#define EGL_ALPHA_MASK_SIZE                 0x303E
#define EGL_COLOR_BUFFER_TYPE               0x303F
#define EGL_RENDERABLE_TYPE                 0x3040
#define EGL_MATCH_NATIVE_PIXMAP             0x3041
#define EGL_CONFORMANT                      0x3042

/* Surface types (EGL_SURFACE_TYPE bitmask) */
#define EGL_PBUFFER_BIT                     0x0001
#define EGL_PIXMAP_BIT                      0x0002
#define EGL_WINDOW_BIT                      0x0004
#define EGL_VG_COLORSPACE_LINEAR_BIT        0x0020
#define EGL_VG_ALPHA_FORMAT_PRE_BIT         0x0040
#define EGL_MULTISAMPLE_RESOLVE_BOX_BIT     0x0200
#define EGL_SWAP_BEHAVIOR_PRESERVED_BIT     0x0400

/* Context attributes */
#define EGL_CONTEXT_CLIENT_VERSION          0x3098

/* Renderable type */
#define EGL_OPENGL_ES_BIT                   0x0001
#define EGL_OPENVG_BIT                      0x0002
#define EGL_OPENGL_ES2_BIT                  0x0004
#define EGL_OPENGL_ES3_BIT                  0x0040
#define EGL_OPENGL_BIT                      0x0008

/* APIs */
#define EGL_OPENGL_ES_API                   0x30A0
#define EGL_OPENVG_API                      0x30A1
#define EGL_OPENGL_API                      0x30A2

/* Surface attributes */
#define EGL_HEIGHT                          0x3056
#define EGL_WIDTH                           0x3057
#define EGL_LARGEST_PBUFFER                 0x3058
#define EGL_TEXTURE_FORMAT                  0x3080
#define EGL_TEXTURE_TARGET                  0x3081
#define EGL_MIPMAP_TEXTURE                  0x3082
#define EGL_MIPMAP_LEVEL                    0x3083
#define EGL_RENDER_BUFFER                   0x3086
#define EGL_VG_COLORSPACE                   0x3087
#define EGL_VG_ALPHA_FORMAT                 0x3088
#define EGL_HORIZONTAL_RESOLUTION          0x3090
#define EGL_VERTICAL_RESOLUTION            0x3091
#define EGL_PIXEL_ASPECT_RATIO             0x3092
#define EGL_SWAP_BEHAVIOR                   0x3093
#define EGL_MULTISAMPLE_RESOLVE             0x3099

#define EGL_BACK_BUFFER                     0x3084
#define EGL_SINGLE_BUFFER                   0x3085

#define EGL_NO_TEXTURE                      0x305C
#define EGL_TEXTURE_RGB                     0x305D
#define EGL_TEXTURE_RGBA                    0x305E
#define EGL_TEXTURE_2D                      0x305F

/* Display attributes */
#define EGL_VENDOR                          0x3053
#define EGL_VERSION                         0x3054
#define EGL_EXTENSIONS                      0x3055
#define EGL_CLIENT_APIS                     0x308D

#define EGL_DONT_CARE                       ((EGLint)-1)

#define EGL_READ                            0x305A
#define EGL_DRAW                            0x3059

/* -----------------------------------------------------------------------
 * EGL 1.4 function declarations
 * ---------------------------------------------------------------------- */
EGLDisplay  eglGetDisplay(EGLNativeDisplayType display_id);
EGLBoolean  eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor);
EGLBoolean  eglTerminate(EGLDisplay dpy);

EGLBoolean  eglBindAPI(EGLenum api);
EGLenum     eglQueryAPI(void);

EGLBoolean  eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
                            EGLConfig *configs, EGLint config_size,
                            EGLint *num_config);
EGLBoolean  eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config,
                               EGLint attribute, EGLint *value);

EGLSurface  eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                   EGLNativeWindowType win,
                                   const EGLint *attrib_list);
EGLSurface  eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
                                    const EGLint *attrib_list);
EGLBoolean  eglDestroySurface(EGLDisplay dpy, EGLSurface surface);
EGLBoolean  eglQuerySurface(EGLDisplay dpy, EGLSurface surface,
                            EGLint attribute, EGLint *value);

EGLContext  eglCreateContext(EGLDisplay dpy, EGLConfig config,
                             EGLContext share_context,
                             const EGLint *attrib_list);
EGLBoolean  eglDestroyContext(EGLDisplay dpy, EGLContext ctx);
EGLBoolean  eglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
                           EGLSurface read, EGLContext ctx);

EGLContext  eglGetCurrentContext(void);
EGLSurface  eglGetCurrentSurface(EGLint which);
EGLDisplay  eglGetCurrentDisplay(void);

EGLBoolean  eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);
EGLBoolean  eglSwapInterval(EGLDisplay dpy, EGLint interval);

EGLint      eglGetError(void);

const char* eglQueryString(EGLDisplay dpy, EGLint name);
__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *procname);

EGLBoolean  eglReleaseThread(void);
EGLBoolean  eglWaitClient(void);
EGLBoolean  eglWaitNative(EGLint engine);
EGLBoolean  eglWaitGL(void);

#ifdef __cplusplus
}
#endif

#endif /* AINE_EGL_EGL_H */
