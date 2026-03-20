/*
 * aine-hals/audio/audio_track.c — AudioTrack → CoreAudio AudioUnit bridge
 *
 * Maps Android AudioTrack semantics to a CoreAudio output AudioUnit.
 * Uses kAudioUnitSubType_DefaultOutput for real hardware output or
 * kAudioUnitSubType_GenericOutput for headless / testing.
 *
 * Internal ring buffer absorbs write() calls and feeds the AudioUnit
 * render callback on Apple's real-time audio thread.
 */

#include "audio_track.h"

#include <AudioToolbox/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * Ring buffer — lock-free single-producer/single-consumer
 * ---------------------------------------------------------------------- */
typedef struct {
    uint8_t        *buf;
    size_t          capacity;     /* must be power-of-2 */
    volatile size_t write_pos;
    volatile size_t read_pos;
    pthread_mutex_t lock;
    pthread_cond_t  cond_space;   /* signalled when space available */
} RingBuf;

static RingBuf *ringbuf_create(size_t cap)
{
    /* Round up to next power-of-2 */
    size_t c = 1;
    while (c < cap) c <<= 1;

    RingBuf *rb = calloc(1, sizeof(*rb));
    rb->buf      = calloc(1, c);
    rb->capacity = c;
    pthread_mutex_init(&rb->lock, NULL);
    pthread_cond_init(&rb->cond_space, NULL);
    return rb;
}

static void ringbuf_destroy(RingBuf *rb)
{
    free(rb->buf);
    pthread_mutex_destroy(&rb->lock);
    pthread_cond_destroy(&rb->cond_space);
    free(rb);
}

static size_t ringbuf_used(RingBuf *rb)
{
    return rb->write_pos - rb->read_pos;
}

/* Write bytes (blocks until all are written) */
static int ringbuf_write(RingBuf *rb, const uint8_t *data, size_t n)
{
    size_t written = 0;
    while (written < n) {
        pthread_mutex_lock(&rb->lock);
        while (rb->capacity - ringbuf_used(rb) == 0)
            pthread_cond_wait(&rb->cond_space, &rb->lock);

        size_t avail = rb->capacity - ringbuf_used(rb);
        size_t chunk = (n - written) < avail ? (n - written) : avail;
        size_t pos   = rb->write_pos & (rb->capacity - 1);
        size_t tail  = rb->capacity - pos;
        if (chunk <= tail) {
            memcpy(rb->buf + pos, data + written, chunk);
        } else {
            memcpy(rb->buf + pos, data + written, tail);
            memcpy(rb->buf,       data + written + tail, chunk - tail);
        }
        rb->write_pos += chunk;
        written += chunk;
        pthread_mutex_unlock(&rb->lock);
    }
    return (int)n;
}

/* Read bytes (returns number actually read, may be < n if underrun) */
static size_t ringbuf_read(RingBuf *rb, uint8_t *dst, size_t n)
{
    size_t avail = ringbuf_used(rb);
    if (avail == 0) return 0;
    size_t chunk = n < avail ? n : avail;
    size_t pos   = rb->read_pos & (rb->capacity - 1);
    size_t tail  = rb->capacity - pos;
    if (chunk <= tail) {
        memcpy(dst, rb->buf + pos, chunk);
    } else {
        memcpy(dst,        rb->buf + pos, tail);
        memcpy(dst + tail, rb->buf,       chunk - tail);
    }
    rb->read_pos += chunk;
    pthread_mutex_lock(&rb->lock);
    pthread_cond_signal(&rb->cond_space);
    pthread_mutex_unlock(&rb->lock);
    return chunk;
}

static void ringbuf_flush(RingBuf *rb)
{
    pthread_mutex_lock(&rb->lock);
    rb->read_pos = rb->write_pos;
    pthread_cond_broadcast(&rb->cond_space);
    pthread_mutex_unlock(&rb->lock);
}

/* -------------------------------------------------------------------------
 * AudioTrack
 * ---------------------------------------------------------------------- */
struct AineAudioTrack {
    AudioUnit        unit;
    AudioComponentDescription desc;
    AudioStreamBasicDescription asbd;
    RingBuf         *ring;
    int              sample_rate;
    int              channels;
    AineAudioFormat  format;
    int              bytes_per_frame;
    int              running;
    int              use_generic;  /* 1 = GenericOutput (headless) */
};

/* CoreAudio render callback — called on real-time thread */
static OSStatus render_cb(void                       *inRefCon,
                          AudioUnitRenderActionFlags *ioActionFlags,
                          const AudioTimeStamp        *inTimeStamp,
                          UInt32                      inBusNumber,
                          UInt32                      inNumberFrames,
                          AudioBufferList            *ioData)
{
    (void)inTimeStamp; (void)inBusNumber; (void)ioActionFlags;
    AineAudioTrack *t = (AineAudioTrack *)inRefCon;

    for (UInt32 b = 0; b < ioData->mNumberBuffers; b++) {
        AudioBuffer *ab  = &ioData->mBuffers[b];
        size_t   needed  = ab->mDataByteSize;
        size_t   got     = ringbuf_read(t->ring, (uint8_t *)ab->mData, needed);
        if (got < needed)
            memset((uint8_t *)ab->mData + got, 0, needed - got); /* silence on underrun */
    }
    return noErr;
}

AineAudioTrack *aine_audio_track_create(int sample_rate, int channels,
                                        AineAudioFormat format,
                                        int buffer_frames)
{
    AineAudioTrack *t = calloc(1, sizeof(*t));
    t->sample_rate   = sample_rate;
    t->channels      = channels;
    t->format        = format;

    int bytes_per_sample = (format == AINE_AUDIO_FORMAT_FLOAT) ? 4 :
                           (format == AINE_AUDIO_FORMAT_PCM16)  ? 2 : 1;
    t->bytes_per_frame = bytes_per_sample * channels;

    /* Try default output first; fall back to GenericOutput for headless */
    AudioComponentDescription desc = {0};
    desc.componentType         = kAudioUnitType_Output;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    /* Attempt DefaultOutput */
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    AudioComponent comp = AudioComponentFindNext(NULL, &desc);

    if (!comp) {
        /* Headless fallback */
        desc.componentSubType = kAudioUnitSubType_GenericOutput;
        comp = AudioComponentFindNext(NULL, &desc);
        t->use_generic = 1;
    }

    if (!comp) {
        fprintf(stderr, "[aine-audio] No AudioComponent found\n");
        free(t);
        return NULL;
    }

    OSStatus err = AudioComponentInstanceNew(comp, &t->unit);
    if (err != noErr) {
        fprintf(stderr, "[aine-audio] AudioComponentInstanceNew error: %d\n", (int)err);
        free(t);
        return NULL;
    }

    /* Configure stream format: PCM native float or int16 */
    AudioStreamBasicDescription asbd = {0};
    asbd.mSampleRate       = (Float64)sample_rate;
    asbd.mFormatID         = kAudioFormatLinearPCM;
    asbd.mChannelsPerFrame = (UInt32)channels;
    asbd.mBitsPerChannel   = (UInt32)(bytes_per_sample * 8);
    asbd.mBytesPerFrame    = (UInt32)t->bytes_per_frame;
    asbd.mFramesPerPacket  = 1;
    asbd.mBytesPerPacket   = asbd.mBytesPerFrame;

    if (format == AINE_AUDIO_FORMAT_FLOAT) {
        asbd.mFormatFlags = kAudioFormatFlagIsFloat
                          | kAudioFormatFlagIsPacked
                          | kAudioFormatFlagIsNonInterleaved;
    } else {
        asbd.mFormatFlags = kAudioFormatFlagIsSignedInteger
                          | kAudioFormatFlagIsPacked;
    }
    t->asbd = asbd;

    err = AudioUnitSetProperty(t->unit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0,
                               &asbd, sizeof(asbd));
    if (err != noErr) {
        fprintf(stderr, "[aine-audio] StreamFormat error: %d\n", (int)err);
        AudioComponentInstanceDispose(t->unit);
        free(t);
        return NULL;
    }

    /* Set render callback */
    AURenderCallbackStruct rcs = { .inputProc = render_cb, .inputProcRefCon = t };
    err = AudioUnitSetProperty(t->unit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input, 0,
                               &rcs, sizeof(rcs));
    if (err != noErr) {
        fprintf(stderr, "[aine-audio] SetRenderCallback error: %d\n", (int)err);
        AudioComponentInstanceDispose(t->unit);
        free(t);
        return NULL;
    }

    err = AudioUnitInitialize(t->unit);
    if (err != noErr) {
        fprintf(stderr, "[aine-audio] AudioUnitInitialize error: %d\n", (int)err);
        AudioComponentInstanceDispose(t->unit);
        free(t);
        return NULL;
    }

    size_t ring_cap = (size_t)buffer_frames * (size_t)t->bytes_per_frame * 2;
    t->ring = ringbuf_create(ring_cap);

    fprintf(stderr, "[aine-audio] AudioTrack created: %dHz %dch fmt=%d%s\n",
            sample_rate, channels, (int)format, t->use_generic ? " (generic output)" : "");
    return t;
}

int aine_audio_track_write(AineAudioTrack *track, const void *data, int byte_count)
{
    if (!track || !data || byte_count <= 0) return -1;
    return ringbuf_write(track->ring, (const uint8_t *)data, (size_t)byte_count);
}

void aine_audio_track_start(AineAudioTrack *track)
{
    if (!track || track->running) return;
    OSStatus err = AudioOutputUnitStart(track->unit);
    if (err == noErr) {
        track->running = 1;
        fprintf(stderr, "[aine-audio] started\n");
    } else {
        fprintf(stderr, "[aine-audio] start error: %d\n", (int)err);
    }
}

void aine_audio_track_stop(AineAudioTrack *track)
{
    if (!track || !track->running) return;
    AudioOutputUnitStop(track->unit);
    track->running = 0;
    fprintf(stderr, "[aine-audio] stopped\n");
}

void aine_audio_track_flush(AineAudioTrack *track)
{
    if (!track) return;
    ringbuf_flush(track->ring);
}

void aine_audio_track_destroy(AineAudioTrack *track)
{
    if (!track) return;
    if (track->running) aine_audio_track_stop(track);
    AudioUnitUninitialize(track->unit);
    AudioComponentInstanceDispose(track->unit);
    ringbuf_destroy(track->ring);
    free(track);
}

int aine_audio_track_get_sample_rate(AineAudioTrack *track) {
    return track ? track->sample_rate : 0; }
int aine_audio_track_get_channels(AineAudioTrack *track) {
    return track ? track->channels : 0; }
int aine_audio_track_get_buffer_size_bytes(AineAudioTrack *track) {
    return track ? (int)(track->ring ? track->ring->capacity : 0) : 0; }
