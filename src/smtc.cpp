#define SMTC_BUILD_DLL
#include "smtc.h"

#include <systemmediatransportcontrolsinterop.h>
#include <unknwn.h>
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <winrt/base.h>

#include <process.h>

#include <mutex>
#include <string>

using namespace winrt;
using namespace winrt::Windows::Media;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Foundation;

struct smtc_ctx
{
    SystemMediaTransportControls smtc{nullptr};
    winrt::event_token button_token{};
    std::mutex cb_mutex;
    smtc_button_callback_t callback = nullptr;
    void *user_data = nullptr;
    bool com_initialized_here = false;

    bool owns_window = false;
    HWND owned_hwnd = nullptr;
    HANDLE worker_thread = nullptr;

    void on_button_pressed(
        SystemMediaTransportControls const &,
        SystemMediaTransportControlsButtonPressedEventArgs const &args);
};

namespace
{

enum smtc_button map_button(SystemMediaTransportControlsButton b)
{
    switch (b)
    {
    case SystemMediaTransportControlsButton::Play:
        return SMTC_BUTTON_PLAY;
    case SystemMediaTransportControlsButton::Pause:
        return SMTC_BUTTON_PAUSE;
    case SystemMediaTransportControlsButton::Stop:
        return SMTC_BUTTON_STOP;
    case SystemMediaTransportControlsButton::Record:
        return SMTC_BUTTON_RECORD;
    case SystemMediaTransportControlsButton::FastForward:
        return SMTC_BUTTON_FAST_FORWARD;
    case SystemMediaTransportControlsButton::Rewind:
        return SMTC_BUTTON_REWIND;
    case SystemMediaTransportControlsButton::Next:
        return SMTC_BUTTON_NEXT;
    case SystemMediaTransportControlsButton::Previous:
        return SMTC_BUTTON_PREVIOUS;
    case SystemMediaTransportControlsButton::ChannelUp:
        return SMTC_BUTTON_CHANNEL_UP;
    case SystemMediaTransportControlsButton::ChannelDown:
        return SMTC_BUTTON_CHANNEL_DOWN;
    default:
        return SMTC_BUTTON_PLAY;
    }
}

MediaPlaybackStatus map_status(smtc_playback_status_t s)
{
    switch (s)
    {
    case SMTC_PLAYBACK_STATUS_CLOSED:
        return MediaPlaybackStatus::Closed;
    case SMTC_PLAYBACK_STATUS_CHANGING:
        return MediaPlaybackStatus::Changing;
    case SMTC_PLAYBACK_STATUS_STOPPED:
        return MediaPlaybackStatus::Stopped;
    case SMTC_PLAYBACK_STATUS_PLAYING:
        return MediaPlaybackStatus::Playing;
    case SMTC_PLAYBACK_STATUS_PAUSED:
        return MediaPlaybackStatus::Paused;
    default:
        return MediaPlaybackStatus::Closed;
    }
}

MediaPlaybackType map_media_type(enum smtc_media_type t)
{
    switch (t)
    {
    case SMTC_MEDIA_TYPE_MUSIC:
        return MediaPlaybackType::Music;
    case SMTC_MEDIA_TYPE_VIDEO:
        return MediaPlaybackType::Video;
    case SMTC_MEDIA_TYPE_IMAGE:
        return MediaPlaybackType::Image;
    default:
        return MediaPlaybackType::Unknown;
    }
}

bool init_smtc_for_hwnd(smtc_ctx *ctx, HWND hwnd)
{
    try
    {
        auto interop = winrt::get_activation_factory<
            SystemMediaTransportControls,
            ISystemMediaTransportControlsInterop>();

        winrt::com_ptr<::ABI::Windows::Media::ISystemMediaTransportControls>
            abi_smtc;
        winrt::check_hresult(interop->GetForWindow(
            hwnd, winrt::guid_of<SystemMediaTransportControls>(),
            abi_smtc.put_void()));

        SystemMediaTransportControls smtc{nullptr};
        winrt::copy_from_abi(smtc, abi_smtc.get());

        ctx->smtc = smtc;
        ctx->button_token = ctx->smtc.ButtonPressed(
            [ctx](SystemMediaTransportControls const &sender,
                  SystemMediaTransportControlsButtonPressedEventArgs const
                      &args) { ctx->on_button_pressed(sender, args); });
        return true;
    }
    catch (...)
    {
        return false;
    }
}

const wchar_t *kHiddenWndClassName = L"smtc_c_hidden_window";

LRESULT CALLBACK hidden_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ensure_hidden_wndclass_registered()
{
    static std::once_flag once;
    std::call_once(once, [] {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = hidden_wnd_proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kHiddenWndClassName;
        RegisterClassExW(&wc);
    });
}

struct AutoThreadStartup
{
    smtc_ctx *ctx = nullptr;
    HANDLE ready_event = nullptr;
    bool success = false;
};

unsigned __stdcall auto_thread_proc(void *param)
{
    auto *startup = static_cast<AutoThreadStartup *>(param);
    smtc_ctx *ctx = startup->ctx;

    bool com_inited_here = false;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (hr == S_OK)
    {
        com_inited_here = true;
    }
    ctx->com_initialized_here = com_inited_here;

    ensure_hidden_wndclass_registered();

    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kHiddenWndClassName, L"",
                                WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                                GetModuleHandleW(nullptr), nullptr);

    bool ok = false;
    if (hwnd)
    {
        ctx->owned_hwnd = hwnd;
        ok = init_smtc_for_hwnd(ctx, hwnd);
    }

    startup->success = ok;
    SetEvent(startup->ready_event);

    if (!ok)
    {
        if (hwnd)
        {
            DestroyWindow(hwnd);
        }
        if (com_inited_here)
        {
            CoUninitialize();
        }
        return 0;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    try
    {
        if (ctx->smtc)
        {
            ctx->smtc.ButtonPressed(ctx->button_token);
            ctx->smtc.IsEnabled(false);
        }
    }
    catch (...)
    {
    }
    ctx->smtc = nullptr;

    if (com_inited_here)
    {
        CoUninitialize();
    }
    return 0;
}

}

void smtc_ctx::on_button_pressed(
    SystemMediaTransportControls const &,
    SystemMediaTransportControlsButtonPressedEventArgs const &args)
{
    smtc_button_callback_t cb;
    void *ud;
    {
        std::lock_guard<std::mutex> lock(cb_mutex);
        cb = callback;
        ud = user_data;
    }
    if (cb)
    {
        cb(map_button(args.Button()), ud);
    }
}

extern "C"
{

    SMTC_API smtc_handle_t smtc_create(void *hwnd)
    {
        if (!hwnd)
        {
            return nullptr;
        }

        auto ctx = std::make_unique<smtc_ctx>();

        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (hr == S_OK)
        {
            ctx->com_initialized_here = true;
        }

        if (!init_smtc_for_hwnd(ctx.get(), reinterpret_cast<HWND>(hwnd)))
        {
            if (ctx->com_initialized_here)
            {
                CoUninitialize();
            }
            return nullptr;
        }

        return ctx.release();
    }

    SMTC_API smtc_handle_t smtc_create_auto(void)
    {
        auto ctx = std::make_unique<smtc_ctx>();
        ctx->owns_window = true;

        HANDLE ready_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!ready_event)
        {
            return nullptr;
        }

        AutoThreadStartup startup;
        startup.ctx = ctx.get();
        startup.ready_event = ready_event;

        HANDLE thread = reinterpret_cast<HANDLE>(
            _beginthreadex(nullptr, 0, auto_thread_proc, &startup, 0, nullptr));

        if (!thread)
        {
            CloseHandle(ready_event);
            return nullptr;
        }

        WaitForSingleObject(ready_event, INFINITE);
        CloseHandle(ready_event);

        if (!startup.success)
        {
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
            return nullptr;
        }

        ctx->worker_thread = thread;
        return ctx.release();
    }

    SMTC_API void smtc_destroy(smtc_handle_t handle)
    {
        if (!handle)
        {
            return;
        }
        std::unique_ptr<smtc_ctx> ctx(handle);

        if (ctx->owns_window)
        {
            if (ctx->owned_hwnd)
            {
                PostMessageW(ctx->owned_hwnd, WM_CLOSE, 0, 0);
            }
            if (ctx->worker_thread)
            {
                WaitForSingleObject(ctx->worker_thread, INFINITE);
                CloseHandle(ctx->worker_thread);
            }
            return;
        }

        try
        {
            if (ctx->smtc)
            {
                ctx->smtc.ButtonPressed(ctx->button_token);
                ctx->smtc.IsEnabled(false);
            }
        }
        catch (...)
        {
        }

        ctx->smtc = nullptr;

        if (ctx->com_initialized_here)
        {
            CoUninitialize();
        }
    }

    SMTC_API bool smtc_set_enabled(smtc_handle_t handle, bool enabled)
    {
        if (!handle || !handle->smtc)
            return false;
        try
        {
            handle->smtc.IsEnabled(enabled);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    SMTC_API bool smtc_set_button_enabled(smtc_handle_t handle,
                                          smtc_button_t button, bool enabled)
    {
        if (!handle || !handle->smtc)
            return false;
        try
        {
            auto const &s = handle->smtc;
            switch (button)
            {
            case SMTC_BUTTON_PLAY:
                s.IsPlayEnabled(enabled);
                break;
            case SMTC_BUTTON_PAUSE:
                s.IsPauseEnabled(enabled);
                break;
            case SMTC_BUTTON_STOP:
                s.IsStopEnabled(enabled);
                break;
            case SMTC_BUTTON_RECORD:
                s.IsRecordEnabled(enabled);
                break;
            case SMTC_BUTTON_FAST_FORWARD:
                s.IsFastForwardEnabled(enabled);
                break;
            case SMTC_BUTTON_REWIND:
                s.IsRewindEnabled(enabled);
                break;
            case SMTC_BUTTON_NEXT:
                s.IsNextEnabled(enabled);
                break;
            case SMTC_BUTTON_PREVIOUS:
                s.IsPreviousEnabled(enabled);
                break;
            case SMTC_BUTTON_CHANNEL_UP:
                s.IsChannelUpEnabled(enabled);
                break;
            case SMTC_BUTTON_CHANNEL_DOWN:
                s.IsChannelDownEnabled(enabled);
                break;
            default:
                return false;
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    SMTC_API bool smtc_set_playback_status(smtc_handle_t handle,
                                           smtc_playback_status_t status)
    {
        if (!handle || !handle->smtc)
            return false;
        try
        {
            handle->smtc.PlaybackStatus(map_status(status));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    SMTC_API bool smtc_update_metadata(smtc_handle_t handle,
                                       const smtc_metadata_t *metadata)
    {
        if (!handle || !handle->smtc || !metadata)
            return false;
        try
        {
            auto updater = handle->smtc.DisplayUpdater();
            updater.Type(map_media_type(metadata->media_type));

            auto props = updater.MusicProperties();
            if (metadata->title)
                props.Title(metadata->title);
            if (metadata->artist)
                props.Artist(metadata->artist);
            if (metadata->album_title)
                props.AlbumTitle(metadata->album_title);
            if (metadata->album_artist)
                props.AlbumArtist(metadata->album_artist);
            if (metadata->track_number)
                props.TrackNumber(metadata->track_number);

            if (metadata->subtitle)
            {
                auto vprops = updater.VideoProperties();
                vprops.Subtitle(metadata->subtitle);
            }

            if (metadata->thumbnail_path)
            {
                std::wstring path = metadata->thumbnail_path;
                std::wstring uri = L"file:///" + path;
                for (auto &c : uri)
                {
                    if (c == L'\\')
                        c = L'/';
                }
                Uri thumb_uri{uri};
                updater.Thumbnail(
                    RandomAccessStreamReference::CreateFromUri(thumb_uri));
            }

            updater.Update();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    SMTC_API bool smtc_update_timeline(smtc_handle_t handle,
                                       const smtc_timeline_t *timeline)
    {
        if (!handle || !handle->smtc || !timeline)
            return false;
        try
        {
            SystemMediaTransportControlsTimelineProperties props;
            props.StartTime(TimeSpan{timeline->start_time_100ns});
            props.EndTime(TimeSpan{timeline->end_time_100ns});
            props.Position(TimeSpan{timeline->position_100ns});
            props.MinSeekTime(TimeSpan{timeline->min_seek_time_100ns});
            props.MaxSeekTime(TimeSpan{timeline->max_seek_time_100ns});
            handle->smtc.UpdateTimelineProperties(props);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    SMTC_API void smtc_set_button_callback(smtc_handle_t handle,
                                           smtc_button_callback_t callback,
                                           void *user_data)
    {
        if (!handle)
            return;
        std::lock_guard<std::mutex> lock(handle->cb_mutex);
        handle->callback = callback;
        handle->user_data = user_data;
    }
}
