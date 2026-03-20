/* vsync.h — AINE VSYNC source API (CADisplayLink-backed) */
#ifndef AINE_VSYNC_H
#define AINE_VSYNC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AineVsync AineVsync;

/* Called with timestamp_ns = nanoseconds since boot, matching Choreographer */
typedef void (*AineVsyncCallback)(uint64_t timestamp_ns, void *userdata);

AineVsync *aine_vsync_create(AineVsyncCallback cb, void *userdata);
void       aine_vsync_start(AineVsync *v);
void       aine_vsync_stop(AineVsync *v);
void       aine_vsync_destroy(AineVsync *v);

/* Portable one-shot: block the calling thread for ~1 frame (60fps) */
void       aine_vsync_wait_once(void);

#ifdef __cplusplus
}
#endif

#endif /* AINE_VSYNC_H */
