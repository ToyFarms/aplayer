#ifndef __METAGEN_H
#define __METAGEN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *mime_type;
    char *description;
    char *picture_type;
    unsigned char *data;
    size_t size;
} picture_t;

typedef struct {
    char *text;
} lyric_line_t;

typedef struct {
    char *text;
    int start_ms;
} synced_lyric_line_t;

typedef struct {
    double value;
    int has_value;
} opt_double_t;

typedef struct {
    unsigned int value;
    int has_value;
} opt_uint_t;

typedef struct {
    char *key;
    char *value;
} kv_t;

typedef struct {
    char *title;
    char **artists;
    size_t n_artists;
    char *album;
    char *album_artist;
    char *genre;
    char *comment;

    char *recording_time;
    char *track_number;
    char *disc_number;
    char *isrc;

    opt_uint_t popularity;

    opt_double_t track_gain;
    opt_double_t track_peak;
    opt_double_t album_gain;
    opt_double_t album_peak;

    lyric_line_t *lyrics;
    size_t n_lyrics;

    synced_lyric_line_t *lyrics_synced;
    size_t n_lyrics_synced;

    picture_t *pictures;
    size_t n_pictures;

    int duration_seconds;
    int bitrate_kbps;
    int sample_rate_hz;
    int channels;

    kv_t *extra_properties;
    size_t n_extra_properties;
} metadata_t;

metadata_t *metadata_read(const char *path);
void metadata_free(metadata_t *meta);

#ifdef __cplusplus
}
#endif

#endif /* __METAGEN_H */
