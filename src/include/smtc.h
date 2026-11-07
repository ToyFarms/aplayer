#ifndef __SMTC_H
#define __SMTC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _WIN32
#  ifdef SMTC_BUILD_DLL
#    define SMTC_API __declspec(dllexport)
#  elif defined(SMTC_STATIC)
#    define SMTC_API
#  else
#    define SMTC_API __declspec(dllimport)
#  endif
#else
#  define SMTC_API
#endif

    typedef struct smtc_ctx *smtc_handle_t;

    enum smtc_button
    {
        SMTC_BUTTON_PLAY = 0,
        SMTC_BUTTON_PAUSE,
        SMTC_BUTTON_STOP,
        SMTC_BUTTON_RECORD,
        SMTC_BUTTON_FAST_FORWARD,
        SMTC_BUTTON_REWIND,
        SMTC_BUTTON_NEXT,
        SMTC_BUTTON_PREVIOUS,
        SMTC_BUTTON_CHANNEL_UP,
        SMTC_BUTTON_CHANNEL_DOWN
    };

    enum smtc_playback_status
    {
        SMTC_PLAYBACK_STATUS_CLOSED = 0,
        SMTC_PLAYBACK_STATUS_CHANGING,
        SMTC_PLAYBACK_STATUS_STOPPED,
        SMTC_PLAYBACK_STATUS_PLAYING,
        SMTC_PLAYBACK_STATUS_PAUSED
    };

    enum smtc_media_type
    {
        SMTC_MEDIA_TYPE_UNKNOWN = 0,
        SMTC_MEDIA_TYPE_MUSIC,
        SMTC_MEDIA_TYPE_VIDEO,
        SMTC_MEDIA_TYPE_IMAGE
    };

    typedef void (*smtc_button_callback_t)(enum smtc_button button,
                                           void *user_data);

    typedef struct smtc_metadata_s
    {
        const wchar_t *title;
        const wchar_t *artist;
        const wchar_t *album_title;
        const wchar_t *album_artist;
        const wchar_t *subtitle;
        const wchar_t *thumbnail_path;
        uint32_t track_number;
        enum smtc_media_type media_type;
    } smtc_metadata_t;

    // times are in 100-nanosecond units
    typedef struct smtc_timeline_s
    {
        int64_t start_time_100ns;
        int64_t end_time_100ns;
        int64_t position_100ns;
        int64_t min_seek_time_100ns;
        int64_t max_seek_time_100ns;
    } smtc_timeline_t;

    SMTC_API smtc_handle_t smtc_create(void *hwnd);
    SMTC_API smtc_handle_t smtc_create_auto(void);

    SMTC_API void smtc_destroy(smtc_handle_t handle);

    SMTC_API bool smtc_set_enabled(smtc_handle_t handle, bool enabled);
    SMTC_API bool smtc_set_button_enabled(smtc_handle_t handle,
                                          enum smtc_button button, bool enabled);
    SMTC_API bool smtc_set_playback_status(smtc_handle_t handle,
                                           enum smtc_playback_status status);
    SMTC_API bool smtc_update_metadata(smtc_handle_t handle,
                                       const smtc_metadata_t *metadata);
    SMTC_API bool smtc_update_timeline(smtc_handle_t handle,
                                       const smtc_timeline_t *timeline);
    SMTC_API void smtc_set_button_callback(smtc_handle_t handle,
                                           smtc_button_callback_t callback,
                                           void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* __SMTC_H */
