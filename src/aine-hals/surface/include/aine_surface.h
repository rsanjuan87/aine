/* aine_surface.h — C interface to the AINE surface manager (F7)
 *
 * Wraps NSWindow + CAMetalLayer creation for Android apps.
 * Implementation is Objective-C (surface.m); this header is C-compatible.
 */
#ifndef AINE_SURFACE_H
#define AINE_SURFACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AineSurfaceHandle AineSurfaceHandle;

/*
 * aine_surface_create_window — open an NSWindow with a CAMetalLayer backing.
 *   Returns NULL on failure (e.g. no NSApplication / no display).
 *   Intended for interactive use.
 */
AineSurfaceHandle *aine_surface_create_window(int width, int height,
                                              const char *title);

/*
 * aine_surface_create_offscreen — create an IOSurface-backed layer suitable
 *   for headless / test rendering (no NSWindow, no display required).
 */
AineSurfaceHandle *aine_surface_create_offscreen(int width, int height);

/*
 * aine_surface_get_layer — return the CAMetalLayer* (as void*) for EGL.
 */
void *aine_surface_get_layer(AineSurfaceHandle *h);

/*
 * aine_surface_get_size — fill *w and *h with the current drawable size.
 */
void  aine_surface_get_size(AineSurfaceHandle *h, int *w, int *h_out);

/*
 * aine_surface_present — commit the next Metal drawable to screen.
 *   (For window surfaces only; offscreen surfaces are a no-op.)
 */
void  aine_surface_present(AineSurfaceHandle *h);

/*
 * aine_surface_destroy — release all resources, close NSWindow if open.
 */
void  aine_surface_destroy(AineSurfaceHandle *h);

#ifdef __cplusplus
}
#endif

#endif /* AINE_SURFACE_H */
