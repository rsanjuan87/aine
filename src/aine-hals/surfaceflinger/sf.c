/*
 * aine-hals/surfaceflinger/sf.c — SurfaceFlinger minimal compositor stub
 *
 * In Android, SurfaceFlinger is the system compositor. In AINE we run a
 * single app at a time, so the "compositor" is just a thin passthrough to
 * aine-surface (NSWindow + CAMetalLayer).
 *
 * Android apps call SurfaceComposerClient and Surface APIs to get a buffer.
 * We redirect those to aine_surface_* calls.
 */

#include "surfaceflinger.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* One active surface per process (single-app compositor) */
static AineSFSurface *s_active = NULL;

AineSFSurface *aine_sf_create_surface(int width, int height, const char *name)
{
    AineSFSurface *sf = calloc(1, sizeof(*sf));
    sf->width  = width;
    sf->height = height;
    strncpy(sf->name, name ? name : "aine-sf", sizeof(sf->name) - 1);

    /* Allocate a double-buffer */
    size_t sz = (size_t)width * (size_t)height * 4;
    sf->back_buf  = calloc(1, sz);
    sf->front_buf = calloc(1, sz);
    sf->buf_size  = sz;

    if (s_active) aine_sf_destroy_surface(s_active);
    s_active = sf;
    fprintf(stderr, "[aine-sf] surface '%s' created %dx%d\n", sf->name, width, height);
    return sf;
}

/* Lock the back buffer for CPU drawing */
int aine_sf_lock(AineSFSurface *sf, void **out_buf, int *stride_bytes)
{
    if (!sf || !sf->back_buf) return -1;
    if (out_buf)      *out_buf      = sf->back_buf;
    if (stride_bytes) *stride_bytes = sf->width * 4;
    return 0;
}

/* Post back buffer to front (swap) */
int aine_sf_unlock_and_post(AineSFSurface *sf)
{
    if (!sf) return -1;
    void *tmp    = sf->front_buf;
    sf->front_buf = sf->back_buf;
    sf->back_buf  = tmp;
    sf->frame_count++;
    return 0;
}

AineSFSurface *aine_sf_get_active(void) { return s_active; }

void aine_sf_destroy_surface(AineSFSurface *sf)
{
    if (!sf) return;
    if (s_active == sf) s_active = NULL;
    free(sf->back_buf);
    free(sf->front_buf);
    free(sf);
}
