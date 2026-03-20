/* audio_track.h — AINE AudioTrack → CoreAudio bridge C API */
#ifndef AINE_AUDIO_TRACK_H
#define AINE_AUDIO_TRACK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Supported sample formats */
typedef enum {
    AINE_AUDIO_FORMAT_PCM16 = 1,   /* Signed 16-bit PCM (most common) */
    AINE_AUDIO_FORMAT_PCM8  = 2,   /* Unsigned 8-bit PCM */
    AINE_AUDIO_FORMAT_FLOAT = 3,   /* 32-bit float PCM */
} AineAudioFormat;

typedef struct AineAudioTrack AineAudioTrack;

/*
 * aine_audio_track_create — create an AudioTrack backed by CoreAudio.
 *
 * sample_rate     : e.g. 44100, 48000
 * channels        : 1 (mono) or 2 (stereo)
 * format          : AINE_AUDIO_FORMAT_PCM16 etc.
 * buffer_frames   : internal ring-buffer size in sample frames
 *
 * Returns NULL on failure (no audio device, bad params).
 */
AineAudioTrack *aine_audio_track_create(int sample_rate, int channels,
                                        AineAudioFormat format,
                                        int buffer_frames);

/*
 * aine_audio_track_write — push PCM data to the output buffer.
 * Returns number of bytes consumed, or -1 on error.
 * Blocks if the internal buffer is full (back-pressure).
 */
int  aine_audio_track_write(AineAudioTrack *track,
                            const void *data, int byte_count);

/*
 * aine_audio_track_start / stop — control CoreAudio output unit.
 */
void aine_audio_track_start(AineAudioTrack *track);
void aine_audio_track_stop(AineAudioTrack *track);
void aine_audio_track_flush(AineAudioTrack *track);
void aine_audio_track_destroy(AineAudioTrack *track);

/* Query */
int  aine_audio_track_get_sample_rate(AineAudioTrack *track);
int  aine_audio_track_get_channels(AineAudioTrack *track);
int  aine_audio_track_get_buffer_size_bytes(AineAudioTrack *track);

#ifdef __cplusplus
}
#endif

#endif /* AINE_AUDIO_TRACK_H */
