/* aine-dalvik/canvas.m — CGBitmapContext software canvas for Android Canvas ops
 *
 * Implements a 32-bit BGRA bitmap as the "screen" for the Android app.
 * Coordinates follow Android conventions: origin at top-left.
 * CoreGraphics uses bottom-left origin, so all y coordinates are flipped.
 *
 * Thread safety: NSLock guards all CGContext writes.  Reads (copy_cgimage)
 * are also locked.
 */

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#include <stdatomic.h>
#include "canvas.h"

static CGContextRef g_ctx      = NULL;
static int          g_cw       = 0;
static int          g_ch       = 0;
static NSLock      *g_lock     = nil;
static atomic_int   g_dirty    = ATOMIC_VAR_INIT(0);
static int          g_in_frame = 0;  /* suppresses per-call dirty marking */

/* Mark dirty only outside a frame; end_frame marks once after full onDraw */
#define MARK_DIRTY() do { if (!g_in_frame) atomic_store(&g_dirty, 1); } while(0)

/* ── Extract normalised RGBA channels from 0xAARRGGBB ───────────────── */
#define CH_A(c) (((c) >> 24 & 0xFFu) / 255.0f)
#define CH_R(c) (((c) >> 16 & 0xFFu) / 255.0f)
#define CH_G(c) (((c) >>  8 & 0xFFu) / 255.0f)
#define CH_B(c) (( (c)      & 0xFFu) / 255.0f)
#define ALPHA(c) ({ float _a = CH_A(c); _a > 0.0f ? _a : 1.0f; })

/* ── Init / shutdown ─────────────────────────────────────────────────── */

void aine_canvas_init(int w, int h)
{
    g_lock = [[NSLock alloc] init];
    g_cw = w; g_ch = h;
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    g_ctx = CGBitmapContextCreate(NULL,
                                  (size_t)w, (size_t)h,
                                  8, (size_t)w * 4,
                                  cs,
                                  kCGImageAlphaPremultipliedFirst |
                                  kCGBitmapByteOrder32Host);
    CGColorSpaceRelease(cs);
    /* Default: dark blue background */
    CGContextSetRGBFillColor(g_ctx, 0.08f, 0.08f, 0.15f, 1.0f);
    CGContextFillRect(g_ctx, CGRectMake(0, 0, w, h));
    atomic_store(&g_dirty, 1);
}

void aine_canvas_destroy(void)
{
    if (g_ctx) { CGContextRelease(g_ctx); g_ctx = NULL; }
    g_lock = nil;
}

/* ── Frame batching ─────────────────────────────────────────────────── */
void aine_canvas_begin_frame(void) { g_in_frame = 1; }
void aine_canvas_end_frame(void)   { g_in_frame = 0; atomic_store(&g_dirty, 1); }

/* ── Drawing calls ───────────────────────────────────────────────────── */

void aine_canvas_clear(uint32_t argb)
{
    if (!g_ctx) return;
    [g_lock lock];
    CGContextSetRGBFillColor(g_ctx, CH_R(argb), CH_G(argb), CH_B(argb), ALPHA(argb));
    CGContextFillRect(g_ctx, CGRectMake(0, 0, g_cw, g_ch));
    [g_lock unlock];
    MARK_DIRTY();
}

void aine_canvas_fill_rect(float x, float y, float w, float h, uint32_t argb)
{
    if (!g_ctx || w <= 0 || h <= 0) return;
    [g_lock lock];
    CGContextSetRGBFillColor(g_ctx, CH_R(argb), CH_G(argb), CH_B(argb), ALPHA(argb));
    /* Flip y: Android top-left → CG bottom-left */
    CGContextFillRect(g_ctx, CGRectMake((CGFloat)x,
                                        (CGFloat)(g_ch - y - h),
                                        (CGFloat)w, (CGFloat)h));
    [g_lock unlock];
    MARK_DIRTY();
}

void aine_canvas_stroke_rect(float x, float y, float w, float h,
                             float sw, uint32_t argb)
{
    if (!g_ctx || w <= 0 || h <= 0) return;
    [g_lock lock];
    CGContextSetRGBStrokeColor(g_ctx,
        CH_R(argb), CH_G(argb), CH_B(argb), ALPHA(argb));
    CGContextSetLineWidth(g_ctx, sw <= 0.0f ? 1.0f : (CGFloat)sw);
    /* Flip y */
    CGContextStrokeRect(g_ctx,
        CGRectMake((CGFloat)x, (CGFloat)(g_ch - y - h),
                   (CGFloat)w, (CGFloat)h));
    [g_lock unlock];
    MARK_DIRTY();
}

void aine_canvas_draw_text(float x, float y, const char *text,
                           float size, uint32_t argb)
{
    if (!g_ctx || !text || !*text) return;
    if (size <= 0.5f) size = 16.0f;
    NSString *ns = [NSString stringWithUTF8String:text];
    if (!ns) ns = @"?";

    [g_lock lock];
    CTFontRef font = CTFontCreateWithName(CFSTR("Helvetica"), (CGFloat)size, NULL);
    CGColorRef color = CGColorCreateGenericRGB(CH_R(argb), CH_G(argb),
                                               CH_B(argb), ALPHA(argb));
    NSDictionary *attrs = @{
        (__bridge NSString *)kCTFontAttributeName:            (__bridge id)font,
        (__bridge NSString *)kCTForegroundColorAttributeName: (__bridge id)color,
    };
    NSAttributedString *astr = [[NSAttributedString alloc]
                                 initWithString:ns attributes:attrs];
    CTLineRef line = CTLineCreateWithAttributedString(
                         (__bridge CFAttributedStringRef)astr);

    CGContextSaveGState(g_ctx);
    /* Map Android top-down y to CG bottom-up y for the text baseline.
     * Do NOT flip the CTM: CTLineDraw needs an unflipped context to render
     * glyphs correctly — any ScaleCTM(1,-1) in the CTM causes CoreText to
     * mirror glyphs horizontally as a side-effect of its glyph path rendering. */
    CGContextSetTextPosition(g_ctx, (CGFloat)x, (CGFloat)(g_ch - y));
    CTLineDraw(line, g_ctx);
    CGContextRestoreGState(g_ctx);

    CFRelease(line);
    CFRelease(font);
    CGColorRelease(color);
    [g_lock unlock];
    MARK_DIRTY();
}

void aine_canvas_draw_circle(float cx, float cy, float r, uint32_t argb)
{
    if (!g_ctx || r <= 0) return;
    [g_lock lock];
    CGContextSetRGBFillColor(g_ctx, CH_R(argb), CH_G(argb), CH_B(argb), ALPHA(argb));
    /* Flip y */
    CGContextFillEllipseInRect(g_ctx,
        CGRectMake((CGFloat)(cx - r), (CGFloat)(g_ch - cy - r),
                   (CGFloat)(r * 2), (CGFloat)(r * 2)));
    [g_lock unlock];
    MARK_DIRTY();
}

/* ── Arc (Android semantics: degrees CW from 3-o'clock) ─────────────── */
void aine_canvas_draw_arc(float l, float t, float r, float b,
                          float start_deg, float sweep_deg,
                          int use_center, uint32_t argb)
{
    if (!g_ctx || r <= l || b <= t) return;
    [g_lock lock];
    CGContextSetRGBFillColor(g_ctx, CH_R(argb), CH_G(argb), CH_B(argb), ALPHA(argb));

    /* y-flip: CG origin is bottom-left */
    float cg_t = (float)g_ch - b;
    float w    = r - l;
    float h    = b - t;
    float cx   = l + w * 0.5f;
    float cy   = cg_t + h * 0.5f;
    float rx   = w * 0.5f;
    float ry   = h * 0.5f;

    /* Android: CW from 3-o'clock → CG: after y-flip, CCW negated = we pass clockwise=1 */
    float cg_start = (float)(-(double)start_deg * M_PI / 180.0);
    float cg_end   = (float)(-(double)(start_deg + sweep_deg) * M_PI / 180.0);

    /* Build elliptic arc path using a unit-circle + scale transform */
    CGAffineTransform xf = CGAffineTransformMakeTranslation(cx, cy);
    xf = CGAffineTransformScale(xf, rx, ry);

    CGMutablePathRef path = CGPathCreateMutable();
    if (use_center)
        CGPathMoveToPoint(path, &xf, 0.0f, 0.0f);
    CGPathAddArc(path, &xf, 0.0f, 0.0f, 1.0f, cg_start, cg_end, 1 /*clockwise*/);
    CGPathCloseSubpath(path);

    CGContextAddPath(g_ctx, path);
    CGContextFillPath(g_ctx);
    CGPathRelease(path);

    [g_lock unlock];
    MARK_DIRTY();
}

void aine_canvas_stroke_arc(float l, float t, float r, float b,
                            float start_deg, float sweep_deg,
                            int use_center, float sw, uint32_t argb)
{
    if (!g_ctx || r <= l || b <= t) return;
    [g_lock lock];
    CGContextSetRGBStrokeColor(g_ctx,
        CH_R(argb), CH_G(argb), CH_B(argb), ALPHA(argb));
    CGContextSetLineWidth(g_ctx, sw <= 0.0f ? 1.0f : (CGFloat)sw);

    float cg_t = (float)g_ch - b;
    float w    = r - l;
    float h    = b - t;
    float cx   = l + w * 0.5f;
    float cy   = cg_t + h * 0.5f;
    float rx   = w * 0.5f;
    float ry   = h * 0.5f;

    float cg_start = (float)(-(double)start_deg * M_PI / 180.0);
    float cg_end   = (float)(-(double)(start_deg + sweep_deg) * M_PI / 180.0);

    CGAffineTransform xf = CGAffineTransformMakeTranslation(cx, cy);
    xf = CGAffineTransformScale(xf, rx, ry);

    CGMutablePathRef path = CGPathCreateMutable();
    if (use_center)
        CGPathMoveToPoint(path, &xf, 0.0f, 0.0f);
    CGPathAddArc(path, &xf, 0.0f, 0.0f, 1.0f, cg_start, cg_end, 1 /*clockwise*/);
    if (use_center)
        CGPathCloseSubpath(path);  /* wedge outline: arc + two lines to center */

    CGContextAddPath(g_ctx, path);
    CGContextStrokePath(g_ctx);
    CGPathRelease(path);

    [g_lock unlock];
    MARK_DIRTY();
}

/* ── Dimensions ──────────────────────────────────────────────────────── */
int aine_canvas_width(void)  { return g_cw; }
int aine_canvas_height(void) { return g_ch; }

/* ── Dirty flag ──────────────────────────────────────────────────────── */
void aine_canvas_mark_dirty(void)  { atomic_store(&g_dirty, 1); }
int  aine_canvas_is_dirty(void)    { return atomic_load(&g_dirty); }
void aine_canvas_clear_dirty(void) { atomic_store(&g_dirty, 0); }

/* ── Image snapshot ──────────────────────────────────────────────────── */
void *aine_canvas_copy_cgimage(void)
{
    if (!g_ctx || !g_lock) return NULL;
    [g_lock lock];
    CGImageRef img = CGBitmapContextCreateImage(g_ctx);
    [g_lock unlock];
    return (void *)img;
}
