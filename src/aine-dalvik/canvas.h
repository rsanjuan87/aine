/* aine-dalvik/canvas.h — Software 2-D canvas backed by CGBitmapContext.
 *
 * Used by jni.c Canvas drawing calls → aine_canvas_*() → CGContext.
 * The AineCanvasView in window.m reads aine_canvas_copy_cgimage() to blit
 * the bitmap to the screen every time the view is redrawn.
 *
 * Thread model:
 *   Interpreter (background thread) writes via aine_canvas_clear / fill_rect
 *   / draw_text / draw_circle.  Lock is held for each call.
 *   Main thread reads via aine_canvas_copy_cgimage() inside drawRect:.
 */
#pragma once
#include <stdint.h>

#ifdef __APPLE__
/* Init / shutdown — must be called from main thread */
void aine_canvas_init(int w, int h);
void aine_canvas_destroy(void);

/* Drawing — thread-safe; all coordinates Android-style (origin top-left) */
void aine_canvas_clear(uint32_t argb);
void aine_canvas_fill_rect(float x, float y, float w, float h, uint32_t argb);
void aine_canvas_draw_text(float x, float y, const char *text,
                           float size, uint32_t argb);
void aine_canvas_draw_circle(float cx, float cy, float r, uint32_t argb);
void aine_canvas_draw_arc(float left, float top, float right, float bottom,
                          float start_deg, float sweep_deg,
                          int use_center, uint32_t argb);

/* Dimensions */
int aine_canvas_width(void);
int aine_canvas_height(void);

/* Frame batching: suppress per-call dirty marks during onDraw; mark once on end */
void aine_canvas_begin_frame(void);
void aine_canvas_end_frame(void);

/* Stroke variants (respect Paint.Style.STROKE) */
void aine_canvas_stroke_rect(float x, float y, float w, float h,
                             float sw, uint32_t argb);
void aine_canvas_stroke_arc(float left, float top, float right, float bottom,
                            float start_deg, float sweep_deg,
                            int use_center, float sw, uint32_t argb);

/* Dirty flag: set by drawing, cleared after each blit in the run-loop */
void aine_canvas_mark_dirty(void);
int  aine_canvas_is_dirty(void);
void aine_canvas_clear_dirty(void);

/* Returns a retained CGImageRef — caller must CGImageRelease it */
void *aine_canvas_copy_cgimage(void);
#endif /* __APPLE__ */
