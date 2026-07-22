#include "metagen.h"

#include <taglib/tag_c.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#  include <strings.h>
#endif

/* ------------------------------------------------------------------------
 * Small internal helpers
 * ---------------------------------------------------------------------- */

static char *xstrdup(const char *s)
{
    if (!s)
        return NULL;
    size_t n = strlen(s) + 1;
    char *out = (char *)malloc(n);
    if (out)
        memcpy(out, s, n);
    return out;
}

/* Join a NULL-terminated array of C strings with "; ", or NULL if empty. */
static char *join_values(char **values)
{
    if (!values || !values[0])
        return NULL;

    size_t total = 0;
    size_t count = 0;
    for (char **v = values; *v; ++v)
    {
        total += strlen(*v);
        count++;
    }
    if (count == 0)
        return NULL;

    total += (count - 1) * 2; /* "; " separators */
    total += 1;               /* NUL */

    char *out = (char *)malloc(total);
    if (!out)
        return NULL;
    out[0] = '\0';

    for (size_t i = 0; values[i]; ++i)
    {
        strcat(out, values[i]);
        if (values[i + 1])
            strcat(out, "; ");
    }
    return out;
}

/* Case-insensitive key compare, PropertyMap keys are uppercase but be safe. */
static int key_is(const char *key, const char *name)
{
#if defined(_WIN32)
    return _stricmp(key, name) == 0;
#else
    return strcasecmp(key, name) == 0;
#endif
}

static double parse_double_or(const char *s, int *ok)
{
    if (!s || !*s)
    {
        *ok = 0;
        return 0.0;
    }
    char *end = NULL;
    double v = strtod(s, &end);
    *ok = (end != s);
    return v;
}

typedef struct
{
    char **keys; /* from taglib_property_keys, owned */
    size_t n_keys;
} prop_keys_t;

/* ------------------------------------------------------------------------
 * Picture extraction (complex "PICTURE" property)
 * ---------------------------------------------------------------------- */

static void extract_pictures(TagLib_File *file, metadata_t *meta)
{
    TagLib_Complex_Property_Attribute ***pics =
        taglib_complex_property_get(file, "PICTURE");
    if (!pics)
        return;

    size_t n = 0;
    while (pics[n])
        n++;
    if (n == 0)
    {
        taglib_complex_property_free(pics);
        return;
    }

    picture_t *out = (picture_t *)calloc(n, sizeof(picture_t));
    if (!out)
    {
        taglib_complex_property_free(pics);
        return;
    }

    for (size_t i = 0; i < n; ++i)
    {
        TagLib_Complex_Property_Attribute **attrs = pics[i];
        for (size_t j = 0; attrs[j]; ++j)
        {
            const char *key = attrs[j]->key;
            TagLib_Variant *v = &attrs[j]->value;

            if (key_is(key, "data") && v->type == TagLib_Variant_ByteVector)
            {
                out[i].size = v->size;
                out[i].data = (unsigned char *)malloc(v->size);
                if (out[i].data)
                    memcpy(out[i].data, v->value.byteVectorValue, v->size);
            }
            else if (key_is(key, "mimeType") &&
                     v->type == TagLib_Variant_String)
            {
                out[i].mime_type = xstrdup(v->value.stringValue);
            }
            else if (key_is(key, "description") &&
                     v->type == TagLib_Variant_String)
            {
                out[i].description = xstrdup(v->value.stringValue);
            }
            else if (key_is(key, "pictureType") &&
                     v->type == TagLib_Variant_String)
            {
                out[i].picture_type = xstrdup(v->value.stringValue);
            }
            /* Other attributes some formats add (e.g. FLAC's "width",
             * "height", "numColors", "colorDepth") are intentionally
             * skipped here -- add fields to picture_t and handle
             * them here if you need them later. */
        }
    }

    taglib_complex_property_free(pics);

    meta->pictures = out;
    meta->n_pictures = n;
}

/* ------------------------------------------------------------------------
 * Main entry point
 * ---------------------------------------------------------------------- */

metadata_t *metadata_read(const char *path)
{
    TagLib_File *file = taglib_file_new(path);
    if (!file)
        return NULL;

    if (!taglib_file_is_valid(file))
    {
        taglib_file_free(file);
        return NULL;
    }

    metadata_t *meta =
        (metadata_t *)calloc(1, sizeof(metadata_t));
    if (!meta)
    {
        taglib_file_free(file);
        return NULL;
    }

    /* --- Audio properties (not part of PropertyMap) --- */
    const TagLib_AudioProperties *ap = taglib_file_audioproperties(file);
    if (ap)
    {
        meta->duration_seconds = taglib_audioproperties_length(ap);
        meta->bitrate_kbps = taglib_audioproperties_bitrate(ap);
        meta->sample_rate_hz = taglib_audioproperties_samplerate(ap);
        meta->channels = taglib_audioproperties_channels(ap);
    }

    /* --- Basic tag, used as fallback for anything PropertyMap misses --- */
    TagLib_Tag *basic = taglib_file_tag(file);

    /* --- Walk the PropertyMap --- */
    char **keys = taglib_property_keys(file);

    /* Count non-consumed extras up front isn't trivial since we don't know
     * count ahead of time; collect into a growable array instead. */
    size_t extra_cap = 8, extra_n = 0;
    kv_t *extras = (kv_t *)malloc(extra_cap * sizeof(kv_t));

    if (keys)
    {
        for (size_t i = 0; keys[i]; ++i)
        {
            const char *key = keys[i];
            char **values = taglib_property_get(file, key);
            char *joined = join_values(values);

            int consumed = 1;

            if (key_is(key, "TITLE"))
            {
                meta->title = joined ? joined : NULL;
                joined = NULL;
            }
            else if (key_is(key, "ARTIST"))
            {
                if (values)
                {
                    size_t n = 0;
                    while (values[n])
                        n++;
                    meta->artists = (char **)calloc(n, sizeof(char *));
                    if (meta->artists)
                    {
                        for (size_t a = 0; a < n; ++a)
                            meta->artists[a] = xstrdup(values[a]);
                        meta->n_artists = n;
                    }
                }
            }
            else if (key_is(key, "ALBUM"))
            {
                meta->album = joined;
                joined = NULL;
            }
            else if (key_is(key, "ALBUMARTIST"))
            {
                meta->album_artist = joined;
                joined = NULL;
            }
            else if (key_is(key, "GENRE"))
            {
                meta->genre = joined;
                joined = NULL;
            }
            else if (key_is(key, "COMMENT"))
            {
                meta->comment = joined;
                joined = NULL;
            }
            else if (key_is(key, "DATE") || key_is(key, "ORIGINALDATE") ||
                     key_is(key, "YEAR"))
            {
                if (!meta->recording_time)
                {
                    meta->recording_time = joined;
                    joined = NULL;
                }
            }
            else if (key_is(key, "TRACKNUMBER"))
            {
                meta->track_number = joined;
                joined = NULL;
            }
            else if (key_is(key, "DISCNUMBER"))
            {
                meta->disc_number = joined;
                joined = NULL;
            }
            else if (key_is(key, "ISRC"))
            {
                meta->isrc = joined;
                joined = NULL;
            }
            else if (key_is(key, "LYRICS"))
            {
                if (values)
                {
                    size_t n = 0;
                    while (values[n])
                        n++;
                    meta->lyrics = (lyric_line_t *)calloc(
                        n, sizeof(lyric_line_t));
                    if (meta->lyrics)
                    {
                        for (size_t l = 0; l < n; ++l)
                            meta->lyrics[l].text = xstrdup(values[l]);
                        meta->n_lyrics = n;
                    }
                }
            }
            else if (key_is(key, "REPLAYGAIN_TRACK_GAIN"))
            {
                int ok;
                double v = parse_double_or(joined, &ok);
                meta->track_gain.value = v;
                meta->track_gain.has_value = ok;
            }
            else if (key_is(key, "REPLAYGAIN_TRACK_PEAK"))
            {
                int ok;
                double v = parse_double_or(joined, &ok);
                meta->track_peak.value = v;
                meta->track_peak.has_value = ok;
            }
            else if (key_is(key, "REPLAYGAIN_ALBUM_GAIN"))
            {
                int ok;
                double v = parse_double_or(joined, &ok);
                meta->album_gain.value = v;
                meta->album_gain.has_value = ok;
            }
            else if (key_is(key, "REPLAYGAIN_ALBUM_PEAK"))
            {
                int ok;
                double v = parse_double_or(joined, &ok);
                meta->album_peak.value = v;
                meta->album_peak.has_value = ok;
            }
            else if (key_is(key, "POPULARIMETER"))
            {
                /* Typical form: "email|rating|playcount" */
                if (joined)
                {
                    char *copy = xstrdup(joined);
                    char *first_bar = strchr(copy, '|');
                    if (first_bar)
                    {
                        char *rating_start = first_bar + 1;
                        char *second_bar = strchr(rating_start, '|');
                        if (second_bar)
                            *second_bar = '\0';
                        if (*rating_start)
                        {
                            meta->popularity.value =
                                (unsigned int)strtoul(rating_start, NULL, 10);
                            meta->popularity.has_value = 1;
                        }
                    }
                    free(copy);
                }
            }
            else
            {
                consumed = 0; /* fall through to extras below */
            }

            if (!consumed)
            {
                if (extra_n == extra_cap)
                {
                    extra_cap *= 2;
                    kv_t *grown = (kv_t *)realloc(
                        extras, extra_cap * sizeof(kv_t));
                    if (grown)
                        extras = grown;
                }
                if (extra_n < extra_cap)
                {
                    extras[extra_n].key = xstrdup(key);
                    extras[extra_n].value = joined;
                    joined = NULL;
                    extra_n++;
                }
            }

            free(joined);
            if (values)
                taglib_property_free(values);
        }
        taglib_property_free(keys);
    }

    meta->extra_properties = extras;
    meta->n_extra_properties = extra_n;

    /* --- Fallback to basic tag API for anything PropertyMap left empty --- */
    if (basic)
    {
        if (!meta->title)
            meta->title = xstrdup(taglib_tag_title(basic));
        if (!meta->album)
            meta->album = xstrdup(taglib_tag_album(basic));
        if (!meta->genre)
            meta->genre = xstrdup(taglib_tag_genre(basic));
        if (!meta->comment)
            meta->comment = xstrdup(taglib_tag_comment(basic));
        if (meta->n_artists == 0)
        {
            char *a = taglib_tag_artist(basic);
            if (a && *a)
            {
                meta->artists = (char **)calloc(1, sizeof(char *));
                if (meta->artists)
                {
                    meta->artists[0] = xstrdup(a);
                    meta->n_artists = 1;
                }
            }
        }
        if (!meta->recording_time)
        {
            unsigned int year = taglib_tag_year(basic);
            if (year > 0)
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "%u", year);
                meta->recording_time = xstrdup(buf);
            }
        }
        if (!meta->track_number)
        {
            unsigned int track = taglib_tag_track(basic);
            if (track > 0)
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "%u", track);
                meta->track_number = xstrdup(buf);
            }
        }
    }

    /* --- Pictures --- */
    extract_pictures(file, meta);

    /* --- Synced lyrics (per-format, see synced_lyrics.cpp) --- */
    {
        synced_lyric_line_t *synced = NULL;
        size_t n_synced = 0;
        if (extract_synced_lyrics(path, &synced, &n_synced) == 0)
        {
            meta->lyrics_synced = synced;
            meta->n_lyrics_synced = n_synced;
        }
    }

    taglib_tag_free_strings(); /* frees strings returned by taglib_tag_* calls
                                  above */
    taglib_file_free(file);

    return meta;
}

void metadata_free(metadata_t *meta)
{
    if (!meta)
        return;

    free(meta->title);
    for (size_t i = 0; i < meta->n_artists; ++i)
        free(meta->artists[i]);
    free(meta->artists);
    free(meta->album);
    free(meta->album_artist);
    free(meta->genre);
    free(meta->comment);
    free(meta->recording_time);
    free(meta->track_number);
    free(meta->disc_number);
    free(meta->isrc);

    for (size_t i = 0; i < meta->n_lyrics; ++i)
        free(meta->lyrics[i].text);
    free(meta->lyrics);

    for (size_t i = 0; i < meta->n_lyrics_synced; ++i)
        free(meta->lyrics_synced[i].text);
    free(meta->lyrics_synced);

    for (size_t i = 0; i < meta->n_pictures; ++i)
    {
        free(meta->pictures[i].mime_type);
        free(meta->pictures[i].description);
        free(meta->pictures[i].picture_type);
        free(meta->pictures[i].data);
    }
    free(meta->pictures);

    for (size_t i = 0; i < meta->n_extra_properties; ++i)
    {
        free(meta->extra_properties[i].key);
        free(meta->extra_properties[i].value);
    }
    free(meta->extra_properties);

    free(meta);
}
