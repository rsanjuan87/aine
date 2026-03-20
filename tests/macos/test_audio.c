/*
 * tests/macos/test_audio.c — CTest for F9 AINE audio HAL
 *
 * Tests AudioTrack creation using GenericOutput (headless — no speaker needed).
 * Verifies:
 *   1. AudioTrack creation with PCM16 stereo 44100Hz (GenericOutput fallback)
 *   2. AudioTrack attributes match requested params
 *   3. write() returns byte count (buffered, no underflow)
 *   4. start() / stop() don't crash
 *   5. AudioFlinger openOutput / write / close round-trip
 *
 * If AudioComponentFindNext fails entirely (no audio subsystem — very rare)
 * the test skips gracefully.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "audio_track.h"
#include "audioflinger.h"

static int s_pass = 0, s_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [ok]   %s\n", msg); s_pass++; } \
    else      { printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); s_fail++; } \
} while (0)

/* Generate 440 Hz sine-wave tone as PCM16 */
#include <math.h>
static void gen_sine_pcm16(int16_t *buf, int frames, int channels, int sample_rate)
{
    double freq = 440.0;
    for (int i = 0; i < frames; i++) {
        int16_t s = (int16_t)(32767.0 * sin(2.0 * 3.14159265 * freq * i / sample_rate));
        for (int c = 0; c < channels; c++)
            buf[i * channels + c] = s;
    }
}

int main(void)
{
    printf("=== F9 audio HAL test ===\n");

    /* Try to create an AudioTrack (GenericOutput fallback for headless) */
    AineAudioTrack *track = aine_audio_track_create(44100, 2,
                                                    AINE_AUDIO_FORMAT_PCM16,
                                                    4096);
    if (!track) {
        printf("  [skip] No audio component available — headless environment\n");
        printf("=== RESULT: 0 ok, 0 failed (skipped) ===\n");
        return 0;
    }

    CHECK(track != NULL, "aine_audio_track_create(44100, 2, PCM16, 4096)");
    CHECK(aine_audio_track_get_sample_rate(track) == 44100, "sample_rate == 44100");
    CHECK(aine_audio_track_get_channels(track)    == 2,     "channels == 2");
    CHECK(aine_audio_track_get_buffer_size_bytes(track) > 0, "buffer_size > 0");

    /* Generate 1024 frames of 440Hz sine tone */
    int frames   = 1024;
    int channels = 2;
    int16_t *pcm = malloc((size_t)(frames * channels * 2));
    gen_sine_pcm16(pcm, frames, channels, 44100);

    int bytes_written = aine_audio_track_write(track, pcm, frames * channels * 2);
    free(pcm);
    CHECK(bytes_written == frames * channels * 2, "write() returns full byte count");

    /* Start / stop (GenericOutput doesn't produce sound but shouldn't crash) */
    aine_audio_track_start(track);
    CHECK(1, "aine_audio_track_start did not crash");
    aine_audio_track_stop(track);
    CHECK(1, "aine_audio_track_stop did not crash");
    aine_audio_track_flush(track);
    CHECK(1, "aine_audio_track_flush did not crash");
    aine_audio_track_destroy(track);
    CHECK(1, "aine_audio_track_destroy did not crash");

    /* AudioFlinger round-trip */
    int tid = aine_af_open_output(44100, 2, AINE_AUDIO_FORMAT_PCM16, 2048);
    CHECK(tid > 0, "aine_af_open_output returns valid track_id");

    uint8_t small_buf[128] = {0};
    int wr = aine_af_write(tid, small_buf, 128);
    CHECK(wr == 128, "aine_af_write returns 128");

    aine_af_start(tid);
    CHECK(1, "aine_af_start did not crash");
    aine_af_stop(tid);
    CHECK(1, "aine_af_stop did not crash");
    aine_af_close(tid);
    CHECK(1, "aine_af_close did not crash");

    printf("=== RESULT: %d ok, %d failed ===\n", s_pass, s_fail);
    return s_fail == 0 ? 0 : 1;
}
