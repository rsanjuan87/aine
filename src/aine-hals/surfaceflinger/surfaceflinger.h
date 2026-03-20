/* surfaceflinger.h — AINE minimal SurfaceFlinger C API */
#ifndef AINE_SURFACEFLINGER_H
#define AINE_SURFACEFLINGER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AineSFSurface {
    int    width, height;
    char   name[64];
    void  *back_buf;
    void  *front_buf;
    size_t buf_size;
    int    frame_count;
} AineSFSurface;

AineSFSurface *aine_sf_create_surface(int width, int height, const char *name);
int  aine_sf_lock(AineSFSurface *sf, void **out_buf, int *stride_bytes);
int  aine_sf_unlock_and_post(AineSFSurface *sf);
AineSFSurface *aine_sf_get_active(void);
void aine_sf_destroy_surface(AineSFSurface *sf);

#ifdef __cplusplus
}
#endif

#endif /* AINE_SURFACEFLINGER_H */
