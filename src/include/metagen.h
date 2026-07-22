/*
 * metadata.h
 *
 * A generalized, read-only audio metadata structure built on top of
 * TagLib's C bindings (taglib/tag_c.h). The goal is one struct shape that
 * looks the same whether the underlying file is MP3/ID3, MP4/M4A atoms,
 * FLAC/Ogg Vorbis comments, etc. -- mirroring the field set of a
 * `MetadataProtocol` (title, artists, album, dates, track/disc numbers,
 * ISRC, popularity, pictures, lyrics, ReplayGain).
 *
 * Design notes:
 *  - Read-only. No setters are exposed; nothing here calls taglib_file_save()
 *    or any taglib_*_set_* function. Adding write support later would mean
 *    adding a mirrored metadata_write.c that consumes the same struct,
 *    without touching this header.
 *  - Fields are populated primarily from TagLib's PropertyMap
 *    (taglib_property_get), which normalizes tag names across formats.
 *    The classic taglib_tag_* basic API is used only as a fallback when a
 *    property is missing, since some files (esp. MP4) don't map every
 *    field into PropertyMap consistently across TagLib versions.
 *  - Anything not explicitly modeled (e.g. MOOD, BPM, COMPOSER, custom
 *    vendor tags) is preserved verbatim in `extra_properties`, so consumers
 *    can grow what they display without needing a struct change.
 *  - Synced lyrics (SYLT / LRC-style / TTML timestamped lines) ARE
 *    extracted, per-format, via a small C++ shim (synced_lyrics.cpp)
 *    that talks to TagLib's real class API directly, since none of this
 *    is exposed through TagLib's plain C bindings:
 *      MP3/ID3v2   -> native SYLT frame(s), millisecond-accurate
 *      Ogg/FLAC    -> LRC-formatted text in a LYRICS/SYNCEDLYRICS/
 *                     UNSYNCEDLYRICS Xiph comment field, if present
 *      MP4/M4A     -> TTML in "----:com.apple.iTunes:lyrics", or LRC
 *                     text in "\xa9lyr" as a fallback
 *    Formats without a modeled source simply come back empty.
 */


#ifndef __METAGEN_H
#define __METAGEN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A single embedded picture (cover art, etc). */
typedef struct {
    char *mime_type;   /* e.g. "image/jpeg", may be NULL if unknown */
    char *description; /* free-form description, may be NULL */
    char *picture_type; /* TagLib's string form, e.g. "Front Cover" */
    unsigned char *data;
    size_t size;
} picture_t;

/* One line of plain (unsynced) lyrics. */
typedef struct {
    char *text;
} lyric_line_t;

/* Placeholder for a future synced-lyrics implementation (see header notes). */
typedef struct {
    char *text;
    int start_ms;
} synced_lyric_line_t;

/* An optional double, since 0.0 is a legitimate ReplayGain value. */
typedef struct {
    double value;
    int has_value;
} opt_double_t;

/* An optional unsigned int, since 0 is ambiguous with "not set" for things
 * like popularity/track/disc numbers on some formats. */
typedef struct {
    unsigned int value;
    int has_value;
} opt_uint_t;

/* Raw, unmapped key/value pair -- everything PropertyMap exposed that we
 * didn't fold into a named field above. Values are joined with "; " when
 * a property has multiple values. */
typedef struct {
    char *key;
    char *value;
} kv_t;

typedef struct {
    /* Core tags */
    char *title;
    char **artists;
    size_t n_artists;
    char *album;
    char *album_artist;    /* NULL if not present */
    char *genre;
    char *comment;

    /* Dates / numbering */
    char *recording_time;  /* raw DATE / year string, e.g. "2021-05-14" or "2021" */
    char *track_number;    /* e.g. "3" or "3/12" depending on what the format gave us */
    char *disc_number;     /* e.g. "1" or "1/2" */
    char *isrc;

    /* Popularity, 0-255 style rating if present (POPM-like semantics) */
    opt_uint_t popularity;

    /* ReplayGain */
    opt_double_t track_gain;
    opt_double_t track_peak;
    opt_double_t album_gain;
    opt_double_t album_peak;

    /* Lyrics */
    lyric_line_t *lyrics;
    size_t n_lyrics;

    synced_lyric_line_t *lyrics_synced; /* see synced_lyrics.cpp for per-format sourcing */
    size_t n_lyrics_synced;

    /* Embedded pictures (cover art, etc) */
    picture_t *pictures;
    size_t n_pictures;

    /* Audio properties, read directly from TagLib, not PropertyMap */
    int duration_seconds;
    int bitrate_kbps;
    int sample_rate_hz;
    int channels;

    /* Anything else PropertyMap gave us that we didn't map above */
    kv_t *extra_properties;
    size_t n_extra_properties;
} metadata_t;

/*
 * Reads metadata from `path`. Returns NULL on failure (file doesn't exist,
 * unsupported/unrecognized format, or TagLib couldn't parse it).
 * The returned pointer must be freed with metadata_free().
 */
metadata_t *metadata_read(const char *path);

/*
 * Format-specific synced-lyrics extraction, implemented in
 * synced_lyrics.cpp against TagLib's C++ classes directly (ID3v2
 * SYLT frames for MP3, LRC-in-comment-field for Ogg/FLAC, TTML/LRC in
 * MP4 atoms). Called automatically by metadata_read(); you normally
 * don't need to call this yourself -- it's exposed so you can invoke it
 * standalone, or add new formats without touching metadata_read().
 *
 * Returns 0 on success (including "found nothing", which yields
 * *out_count == 0), or -1 on a hard failure (file couldn't be opened).
 */
int extract_synced_lyrics(const char *path,
                                synced_lyric_line_t **out_lines,
                                size_t *out_count);
void free_synced_lyric_lines(synced_lyric_line_t *lines, size_t count);

/* Frees a struct returned by metadata_read(), including all owned
 * strings, arrays, and picture buffers. Safe to call with NULL. */
void metadata_free(metadata_t *meta);

#ifdef __cplusplus
}
#endif

#endif /* __METAGEN_H */
