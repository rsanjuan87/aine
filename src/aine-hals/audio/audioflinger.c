/*
 * aine-hals/audio/audioflinger.c — AudioFlinger stub (AINE)
 *
 * In Android, AudioFlinger is the central audio server that manages
 * tracks and routing.  In AINE each app gets its own AudioTrack backed
 * directly by CoreAudio — there is no separate audio server process.
 *
 * This file provides the C-facing stubs that map Android Audio* calls
 * to aine_audio_track_* and keeps a simple registry of open tracks.
 */

#include "audioflinger.h"
#include "audio_track.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_TRACKS 8

static struct {
    AineAudioTrack *track;
    int             id;
} s_tracks[MAX_TRACKS];

static int s_next_id = 1;

int aine_af_open_output(int sample_rate, int channels,
                        AineAudioFormat format, int buffer_frames)
{
    for (int i = 0; i < MAX_TRACKS; i++) {
        if (!s_tracks[i].track) {
            AineAudioTrack *t = aine_audio_track_create(
                sample_rate, channels, format, buffer_frames);
            if (!t) return -1;
            s_tracks[i].track = t;
            s_tracks[i].id    = s_next_id++;
            fprintf(stderr, "[aine-af] openOutput -> track_id=%d\n", s_tracks[i].id);
            return s_tracks[i].id;
        }
    }
    fprintf(stderr, "[aine-af] openOutput: too many tracks\n");
    return -1;
}

int aine_af_write(int track_id, const void *data, int byte_count)
{
    for (int i = 0; i < MAX_TRACKS; i++) {
        if (s_tracks[i].id == track_id)
            return aine_audio_track_write(s_tracks[i].track, data, byte_count);
    }
    return -1;
}

void aine_af_start(int track_id)
{
    for (int i = 0; i < MAX_TRACKS; i++) {
        if (s_tracks[i].id == track_id) {
            aine_audio_track_start(s_tracks[i].track);
            return;
        }
    }
}

void aine_af_stop(int track_id)
{
    for (int i = 0; i < MAX_TRACKS; i++) {
        if (s_tracks[i].id == track_id) {
            aine_audio_track_stop(s_tracks[i].track);
            return;
        }
    }
}

void aine_af_close(int track_id)
{
    for (int i = 0; i < MAX_TRACKS; i++) {
        if (s_tracks[i].id == track_id) {
            aine_audio_track_destroy(s_tracks[i].track);
            s_tracks[i].track = NULL;
            s_tracks[i].id    = 0;
            return;
        }
    }
}
