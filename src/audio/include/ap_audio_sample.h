#ifndef __AP_AUDIO_SAMPLE_H
#define __AP_AUDIO_SAMPLE_H

#include "libavutil/samplefmt.h"
#include "portaudio.h"
#include <stdbool.h>

#define AP_AUDIO_SAMPLEFMT_LIST                                                \
    __X(AP_AUDIO_SAMPLEFMT_U8, 1 << 0)  /* Unsigned 8 bits */                  \
    __X(AP_AUDIO_SAMPLEFMT_S16, 1 << 1) /* Signed 16 bits  */                  \
    __X(AP_AUDIO_SAMPLEFMT_S32, 1 << 2) /* Signed 32 bits  */                  \
    __X(AP_AUDIO_SAMPLEFMT_FLT, 1 << 3) /* Float 32 bits   */                  \
    __X(AP_AUDIO_SAMPLEFMT_DBL, 1 << 4) /* Float 64 bits   */                  \
    __X(AP_AUDIO_SAMPLEFMT_S64, 1 << 5) /* Signed 64 bits  */
#define AP_AUDIO_SAMPLEFMT_PLANAR                                              \
    (1 << 20) /* Planar: each channel get their own buffer */

#define __X(name, value) name = value,
typedef enum APAudioSampleFmt
{
    AP_AUDIO_SAMPLEFMT_LIST
} APAudioSampleFmt;
#undef __X

#define __X(name, value)                                                       \
case name:                                                                     \
    return #name;
static inline const char *ap_audio_samplefmt_str(APAudioSampleFmt sample_fmt)
{
    switch (sample_fmt)
    {
        AP_AUDIO_SAMPLEFMT_LIST
    default:
        return "AP_AUDIO_SAMPLEFMT_UNKNOWN";
    }
}
#undef __X

#define CASE_PLANAR(pattern, ret)                                              \
case pattern:                                                                  \
    return ret;                                                                \
case pattern | AP_AUDIO_SAMPLEFMT_PLANAR:                                      \
    return ret##P
static inline enum AVSampleFormat ap_samplefmt_to_av_samplefmt(
    APAudioSampleFmt sample_fmt)
{
    switch ((int)sample_fmt)
    {
        CASE_PLANAR(AP_AUDIO_SAMPLEFMT_U8, AV_SAMPLE_FMT_U8);
        CASE_PLANAR(AP_AUDIO_SAMPLEFMT_S16, AV_SAMPLE_FMT_S16);
        CASE_PLANAR(AP_AUDIO_SAMPLEFMT_S32, AV_SAMPLE_FMT_S32);
        CASE_PLANAR(AP_AUDIO_SAMPLEFMT_FLT, AV_SAMPLE_FMT_FLT);
        CASE_PLANAR(AP_AUDIO_SAMPLEFMT_DBL, AV_SAMPLE_FMT_DBL);
        CASE_PLANAR(AP_AUDIO_SAMPLEFMT_S64, AV_SAMPLE_FMT_S64);
    default:
        return -1;
    }
}
#undef CASE_PLANAR

#define CASE_PLANAR(pattern, ret)                                              \
case pattern:                                                                  \
    return ret;                                                                \
case pattern | AP_AUDIO_SAMPLEFMT_PLANAR:                                      \
    return ret | paNonInterleaved
static inline PaSampleFormat ap_samplefmt_to_pa_samplefmt(
    APAudioSampleFmt sample_fmt)
{
    switch ((int)sample_fmt)
    {
        CASE_PLANAR(AP_AUDIO_SAMPLEFMT_U8, paUInt8);
        CASE_PLANAR(AP_AUDIO_SAMPLEFMT_S16, paInt16);
        CASE_PLANAR(AP_AUDIO_SAMPLEFMT_S32, paInt32);
        CASE_PLANAR(AP_AUDIO_SAMPLEFMT_FLT, paFloat32);
        // CASE_PLANAR(AP_AUDIO_SAMPLEFMT_DBL, paFloat32);
        // CASE_PLANAR(AP_AUDIO_SAMPLEFMT_S64, paInt64);
    default:
        return -1;
    }
}

static inline bool ap_samplefmt_is_planar(APAudioSampleFmt sample_fmt)
{
    return (sample_fmt & AP_AUDIO_SAMPLEFMT_PLANAR) != 0;
}

#endif /* __AP_AUDIO_SAMPLE_H */
