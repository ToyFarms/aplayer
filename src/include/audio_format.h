#ifndef __AUDIO_FORMAT_H
#define __AUDIO_FORMAT_H

#include "libavutil/samplefmt.h"

#include <stdbool.h>

#define AUDIO_FORMAT_LIST(DO)                                                  \
    DO(AUDIO_U8, 0)                                                            \
    DO(AUDIO_S16, 1)                                                           \
    DO(AUDIO_S32, 2)                                                           \
    DO(AUDIO_FLT, 3)                                                           \
    DO(AUDIO_DBL, 4)                                                           \
    DO(AUDIO_S64, 5)
#define AUDIO_PLANAR (1 << 8)

#define DO_DEFINE_ENUM(name, value) name = value,
enum audio_format
{
    AUDIO_FORMAT_LIST(DO_DEFINE_ENUM)
};

#define AUDIO_IS_PLANAR(fmt) ((fmt) & AUDIO_PLANAR != 0)

#define DO_STRINGIFY(name, value)                                              \
case name:                                                                     \
    return #name;
static inline const char *audio_format_str(enum audio_format fmt)
{
    switch (fmt)
    {
        AUDIO_FORMAT_LIST(DO_STRINGIFY)
    default:
        return "AUDIO_UNKNOWN";
    }
}

#define CASE_PLANAR(pattern, ret)                                              \
case pattern:                                                                  \
    return ret;                                                                \
case pattern | AUDIO_PLANAR:                                                   \
    return ret##P

static inline enum AVSampleFormat audio_format_to_av_variant(
    enum audio_format fmt)
{
    switch ((int)fmt)
    {
        CASE_PLANAR(AUDIO_U8, AV_SAMPLE_FMT_U8);
        CASE_PLANAR(AUDIO_S16, AV_SAMPLE_FMT_S16);
        CASE_PLANAR(AUDIO_S32, AV_SAMPLE_FMT_S32);
        CASE_PLANAR(AUDIO_FLT, AV_SAMPLE_FMT_FLT);
        CASE_PLANAR(AUDIO_DBL, AV_SAMPLE_FMT_DBL);
        CASE_PLANAR(AUDIO_S64, AV_SAMPLE_FMT_S64);
    default:
        return -1;
    }
}

#undef CASE_PLANAR
#define CASE_PLANAR(pattern, ret)                                              \
case pattern:                                                                  \
    return ret;                                                                \
case pattern##P:                                                               \
    return ret | AUDIO_PLANAR
static inline enum audio_format audio_format_from_av_variant(
    enum AVSampleFormat fmt)
{
    switch (fmt)
    {
        CASE_PLANAR(AV_SAMPLE_FMT_U8, AUDIO_U8);
        CASE_PLANAR(AV_SAMPLE_FMT_S16, AUDIO_S16);
        CASE_PLANAR(AV_SAMPLE_FMT_S32, AUDIO_S32);
        CASE_PLANAR(AV_SAMPLE_FMT_FLT, AUDIO_FLT);
        CASE_PLANAR(AV_SAMPLE_FMT_DBL, AUDIO_DBL);
        CASE_PLANAR(AV_SAMPLE_FMT_S64, AUDIO_S64);
    default:
        return -1;
    }
}

#endif /* __AUDIO_FORMAT_H */
