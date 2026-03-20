/* audioflinger.h — AINE AudioFlinger C API */
#ifndef AINE_AUDIOFLINGER_H
#define AINE_AUDIOFLINGER_H

#include "audio_track.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Open an output stream; returns track_id (> 0) or -1 on error */
int  aine_af_open_output(int sample_rate, int channels,
                         AineAudioFormat format, int buffer_frames);

/* Write PCM data to a track */
int  aine_af_write(int track_id, const void *data, int byte_count);

void aine_af_start(int track_id);
void aine_af_stop(int track_id);
void aine_af_close(int track_id);

#ifdef __cplusplus
}
#endif

#endif /* AINE_AUDIOFLINGER_H */
