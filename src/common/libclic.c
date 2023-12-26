#include "libcli.h"

CLIState *cli_state_init()
{
    av_log(NULL, AV_LOG_DEBUG, "Initializing CLIState.\n");

    CLIState *cst = (CLIState *)malloc(sizeof(CLIState));
    if (!cst)
    {
        av_log(NULL, AV_LOG_DEBUG, "Could not allocate CLIState.\n");
        return NULL;
    }

    memset(cst, 0, sizeof(CLIState));

    pthread_mutex_init(&cst->mutex, NULL);

    cst->entries = NULL;
    cst->entry_size = 0;
    cst->entry_offset = 0;

    cst->hovered_idx = -1;
    cst->playing_idx = -1;
    cst->selected_idx = -1;

    cst->media_duration = 0;
    cst->media_timestamp = 0;
    cst->media_volume = 0.0f;
    cst->media_paused = false;
    cst->media_is_muted = false;

    cst->force_redraw = false;

    cst->out.handle = NULL;
    cst->width = 0;
    cst->width = 0;
    cst->cursor_x = 0;
    cst->cursor_y = 0;

    cst->mouse_x = 0;
    cst->mouse_y = 0;

    cst->icon = (UnicodeSymbol){
        .volume_mute = wchar2mbs(L"🔇"),
        .volume_off = wchar2mbs(L"🔈"),
        .volume_low = wchar2mbs(L"🔉"),
        .volume_medium = wchar2mbs(L"🔊"),
        .volume_high = wchar2mbs(L"🔊"),

        .media_play = wchar2mbs(L"⏵"),
        .media_pause = wchar2mbs(L"⏸"),
        .media_next_track = wchar2mbs(L"⏭"),
        .media_prev_track = wchar2mbs(L"⏮"),
    };

    cst->icon_nerdfont = (UnicodeSymbol){
        .volume_mute = wchar2mbs(L"󰝟"),
        .volume_off = wchar2mbs(L"󰖁"),
        .volume_low = wchar2mbs(L"󰕿"),
        .volume_medium = wchar2mbs(L"󰖀"),
        .volume_high = wchar2mbs(L"󰕾"),

        .media_play = wchar2mbs(L""),
        .media_pause = wchar2mbs(L""),
        .media_next_track = wchar2mbs(L"󰒭"),
        .media_prev_track = wchar2mbs(L"󰒮"),
    };

    cst->prev_button_hovered = false;
    cst->playback_button_hovered = false;
    cst->next_button_hovered = false;

    return cst;
}

void cli_state_free(CLIState **cst)
{
    av_log(NULL, AV_LOG_DEBUG, "Free CLIState.\n");

    if (!cst)
        return;

    if (!(*cst))
        return;

    if ((*cst)->entries)
    {
        av_log(NULL, AV_LOG_DEBUG, "Free CLIState entries.\n");
        for (int i = 0; i < (*cst)->entry_size; i++)
        {
            free((*cst)->entries[i].path);
            free((*cst)->entries[i].filename);
        }

        free((*cst)->entries);
    }

    free(*cst);
    *cst = NULL;
}

static inline void cli_cursor_to(HANDLE out, int x, int y)
{
    char buf[128];
    snprintf(buf, 128, "\x1b[%d;%dH", y, x);
    printf("%s", buf);
}

void cli_clear_screen(HANDLE out)
{
    printf("\x1b[2J");
}

static StringBuilder *list_sb;
static const Color overlay_fg_color = {230, 200, 150};

static wchar_t *cli_line_routine(CLIState *cst, int idx, LineState line_state, int *out_strlen)
{
    sb_appendf(list_sb, "\x1b[38;2;%d;%d;%d;48;2;41;41;41m%3d \x1b[39m", overlay_fg_color.r, overlay_fg_color.g, overlay_fg_color.b, idx);
    switch (line_state)
    {
    case LINE_PLAYING:
        sb_appendf(list_sb, "\x1b[38;2;0;0;0;48;2;91;201;77m%s", cst->entries[idx].filename);
        break;
    case LINE_SELECTED:
        sb_appendf(list_sb, "\x1b[38;2;0;0;0;48;2;255;255;255m%s", cst->entries[idx].filename);
        break;
    case LINE_HOVERED:
        sb_appendf(list_sb, "\x1b[38;2;0;0;0;48;2;150;150;150m%s", cst->entries[idx].filename);
        break;
    case LINE_NORMAL:
        sb_appendf(list_sb, "%s", cst->entries[idx].filename);
        break;
    default:
        break;
    }

    char *str = sb_concat(list_sb);

    wchar_t *strw = mbs2wchar(str, 2048, out_strlen);

    free(str);
    sb_reset(list_sb);

    return strw;
}

static LineState cli_get_line_state(CLIState *cst, int idx)
{
    if (idx == cst->playing_idx)
        return LINE_PLAYING;
    else if (idx == cst->selected_idx)
        return LINE_SELECTED;
    else if (idx == cst->hovered_idx)
        return LINE_HOVERED;
    else
        return LINE_NORMAL;
}

static StringBuilder *pad_sb;

static void cli_draw_padding(CLIState *cst,
                             const Vec2 *pos,
                             int length,
                             const Color *fg,
                             const Color *bg)
{
    if (!pad_sb)
        pad_sb = sb_create();

    if (bg && fg)
        sb_appendf(pad_sb, "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm", fg->r, fg->g, fg->b, bg->r, bg->g, bg->b);
    else if (fg)
        sb_appendf(pad_sb, "\x1b[38;2;%d;%d;%dm", fg->r, fg->g, fg->b);
    else if (bg)
        sb_appendf(pad_sb, "\x1b[48;2;%d;%d;%dm", bg->r, bg->g, bg->b);

    sb_appendf(pad_sb, "\x1b[%dX", length);

    if (bg || fg)
        sb_append(pad_sb, "\x1b[0m");

    char *str = sb_concat(pad_sb);

    if (pos)
        cli_cursor_to(cst->out.handle, pos->x, pos->y);

    WriteConsole(cst->out.handle, str, strlen(str), NULL, NULL);

    free(str);
    sb_reset(pad_sb);
}

static LineState *lines_state_cache;

static void cli_draw_list(CLIState *cst)
{
    int strw_len;

    if (cst->force_redraw)
        cli_clear_screen(cst->out.handle);

    for (int viewport_offset = 0; //   - 3 : bottom overlay
         viewport_offset < cst->height - 3 && cst->entry_offset + viewport_offset < cst->entry_size;
         viewport_offset++)
    {
        int abs_entry_idx = cst->entry_offset + viewport_offset;
        LineState line_state = cli_get_line_state(cst, abs_entry_idx);

        if (!cst->force_redraw && lines_state_cache[abs_entry_idx] && line_state == lines_state_cache[abs_entry_idx])
            continue;

        lines_state_cache[abs_entry_idx] = line_state;

        cli_cursor_to(cst->out.handle, 0, viewport_offset);

        wchar_t *strw = cli_line_routine(cst, abs_entry_idx, line_state, &strw_len);
        WriteConsoleW(cst->out.handle, strw, strw_len, NULL, NULL);
        free(strw);

        cli_get_cursor_pos(cst);

        int pad = cst->width - cst->cursor_x;
        if (pad <= 0)
            continue;

        cli_draw_padding(cst, NULL, pad, NULL, NULL);
    }
}

void cli_draw(CLIState *cst)
{
    pthread_mutex_lock(&cst->mutex);

    if (!list_sb)
        list_sb = sb_create();

    cli_get_console_size(cst);

    if (!lines_state_cache)
        lines_state_cache = (LineState *)malloc(cst->entry_size * sizeof(LineState));
    else if (sizeof(lines_state_cache) / sizeof(LineState) != cst->entry_size)
        realloc(lines_state_cache, cst->entry_size * sizeof(LineState));

    cli_draw_list(cst);
    if (cst->force_redraw)
        cli_draw_overlay(cst);

    pthread_mutex_unlock(&cst->mutex);
}

static StringBuilder *overlay_sb;

static void cli_draw_rect(CLIState *cst, Rect rect, Color color)
{
    if (rect.x < 0 || rect.y < 0 || rect.w <= 0 || rect.h <= 0)
        return;

    for (int current_y = rect.y; current_y < rect.y + rect.h; current_y++)
        cli_draw_padding(cst, &(Vec2){rect.x, current_y}, rect.w, &color, &color);
}

static const wchar_t blocks[] = {L'█', L'▉', L'▊', L'▋', L'▌', L'▍', L'▎', L'▎', L' '};
static const int block_len = 9;
static const float block_increment = 1.0f / (float)block_len;

static void cli_draw_hlinef(CLIState *cst,
                            Vec2 pos,
                            float length,
                            Color fg,
                            Color bg,
                            bool reset_bg_color)
{
    int int_part = (int)length;
    float float_part = length - (float)int_part;

    cli_cursor_to(cst->out.handle, pos.x, pos.y);

    char *str;
    if (int_part > 0)
        cli_draw_padding(cst, NULL, int_part, NULL, &fg);
    
    cli_cursor_to(cst->out.handle, pos.x + int_part, pos.y);

    sb_appendf(overlay_sb, "\x1b[48;2;%d;%d;%d;38;2;%d;%d;%dm", bg.r, bg.g, bg.b, fg.r, fg.g, fg.b);
    str = sb_concat(overlay_sb);
    WriteConsole(cst->out.handle, str, strlen(str), NULL, NULL);
    free(str);
    sb_reset(overlay_sb);

    int block_index = block_len - (int)(roundf(float_part / block_increment) * block_increment * block_len);
    wchar_t final_block = blocks[block_index];
    WriteConsoleW(cst->out.handle, &final_block, 1, NULL, NULL);

    if (reset_bg_color)
        WriteConsole(cst->out.handle, "\x1b[0m", 5, NULL, NULL);
    else
        WriteConsole(cst->out.handle, "\x1b[39m", 6, NULL, NULL);
}

static void cli_draw_progress(CLIState *cst,
                              Vec2 pos,
                              int length,
                              float current,
                              float max,
                              Color fg,
                              Color bg)
{
    float mapped_length = mapf(current, 0.0f, max, 0.0f, (float)length);

    cli_draw_hlinef(cst, pos, mapped_length, fg, bg, false);

    cli_get_cursor_pos(cst);
    cli_draw_padding(cst,
                     NULL,
                     (pos.x + length) - cst->cursor_x,
                     NULL,
                     NULL);

    WriteConsole(cst->out.handle, "\x1b[0m", 5, NULL, NULL);
}

static void cli_draw_timestamp(CLIState *cst, Vec2 pos, Color fg, Color bg)
{
    cli_cursor_to(cst->out.handle, pos.x, pos.y);

    Time ts = time_from_us((double)cst->media_timestamp);
    Time d = time_from_us((double)cst->media_duration);

    sb_appendf(overlay_sb,
               "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm",
               fg.r, fg.g, fg.b,
               bg.r, bg.g, bg.b);

    char *str = sb_concat(overlay_sb);

    WriteConsole(cst->out.handle, str, strlen(str), NULL, NULL);

    free(str);
    sb_reset(overlay_sb);

    if (d.h >= 1.0)
        sb_appendf(overlay_sb, "%01lld:", (int64_t)ts.h);

    sb_appendf(overlay_sb,
               d.h >= 1.0 ? "%02lld:%02lld / " : "%01lld:%02lld / ",
               (int64_t)ts.m,
               (int64_t)ts.s);

    if (d.h >= 1.0)
        sb_appendf(overlay_sb, "%01lld:", (int64_t)d.h);

    sb_appendf(overlay_sb,
               d.h >= 1.0 ? "%02lld:%02lld\x1b[0m" : "%01lld:%02lld\x1b[0m",
               (int64_t)d.m,
               (int64_t)d.s);

    str = sb_concat(overlay_sb);

    WriteConsole(cst->out.handle, str, strlen(str), NULL, NULL);

    free(str);
    sb_reset(overlay_sb);
}

static void cli_draw_volume(CLIState *cst, Vec2 pos, Color fg, Color bg)
{
    char *volume_icon;

    if (cst->media_is_muted)
        volume_icon = cst->icon.volume_mute;
    else if (cst->media_volume - 1e-3 < 0.0f)
        volume_icon = cst->icon.volume_off;
    else if (cst->media_volume < 0.5f)
        volume_icon = cst->icon.volume_low;
    else if (cst->media_volume < 0.75f)
        volume_icon = cst->icon.volume_medium;
    else
        volume_icon = cst->icon.volume_high;

    sb_appendf(overlay_sb,
               "\x1b[48;2;%d;%d;%dm%s\x1b[38;2;%d;%d;%dm%.0f\x1b[0m",
               bg.r, bg.g, bg.b,
               volume_icon,
               fg.r, fg.g, fg.b,
               cst->media_volume * 100.0f);

    char *str = sb_concat(overlay_sb);

    int strw_len;
    wchar_t *strw = mbs2wchar(str, 128, &strw_len);

    cli_cursor_to(cst->out.handle, pos.x, pos.y);
    WriteConsoleW(cst->out.handle, strw, strw_len, NULL, NULL);

    free(strw);
    free(str);
    sb_reset(overlay_sb);
}

static void cli_draw_media_control(CLIState *cst, Vec2 center, Color fg, Color bg)
{
    bool prev_collision = false;
    bool playback_collision = false;
    bool next_collision = false;

    if (cst->mouse_y == center.y)
    {
        if (FFABS(cst->mouse_x - (center.x - 4)) <= 1)
            prev_collision = true;
        else if (FFABS(cst->mouse_x - center.x) <= 1)
            playback_collision = true;
        else if (FFABS(cst->mouse_x - (center.x + 4)) <= 1)
            next_collision = true;
    }

    cst->prev_button_hovered = prev_collision;
    cst->playback_button_hovered = playback_collision;
    cst->next_button_hovered = next_collision;

    char *playback_icon = cst->media_paused ? cst->icon_nerdfont.media_play : cst->icon_nerdfont.media_pause;
    const char *hovered_color = "\x1b[38;2;0;0;0;48;2;255;255;255m";

    sb_appendf(overlay_sb,
               "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm",
               fg.r, fg.g, fg.b,
               bg.r, bg.g, bg.b);

    char *normal_color = sb_concat(overlay_sb);

    sb_reset(overlay_sb);

    cli_cursor_to(cst->out.handle, center.x - 5, center.y);

    sb_appendf(overlay_sb,
               "%s %s %s %s %s %s %s %s %s",
               prev_collision ? hovered_color : normal_color,
               cst->icon_nerdfont.media_prev_track,
               prev_collision ? normal_color : "",

               playback_collision ? hovered_color : normal_color,
               playback_icon,
               playback_collision ? normal_color : "",

               next_collision ? hovered_color : normal_color,
               cst->icon_nerdfont.media_next_track,
               next_collision ? normal_color : "");

    char *str = sb_concat(overlay_sb);

    int strw_len;
    wchar_t *strw = mbs2wchar(str, 1024, &strw_len);

    WriteConsoleW(cst->out.handle, strw, strw_len, NULL, NULL);

    free(normal_color);
    free(strw);
    free(str);
    sb_reset(overlay_sb);
}

static void cli_draw_now_playing(CLIState *cst, Vec2 pos, Color fg, Color bg)
{
    static bool text_overflow = false;
    static int64_t last_shifted = 0;
    static int shift_interval = 200;
    static int offset = 0;
    static int direction = 1;
    static int text_len = 0;

    cli_cursor_to(cst->out.handle, pos.x, pos.y);

    sb_appendf(overlay_sb,
               "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm",
               fg.r, fg.g, fg.b,
               bg.r, bg.g, bg.b);

    char *str = sb_concat(overlay_sb);
    WriteConsole(cst->out.handle, str, strlen(str), NULL, NULL);

    const char *playing = "Playing: ";
    WriteConsole(cst->out.handle, playing, strlen(playing), NULL, NULL);

    int max_len = ((cst->width / 2) - 5) - (pos.x + strlen(playing));

    int strw_len;
    wchar_t *strw = mbs2wchar(cst->entries[cst->playing_idx].filename, 1024, &strw_len);

    if (text_overflow)
    {
        if (offset > 0 && text_len <= max_len)
        {
            direction = -1;
            shift_interval = 1000;
        }
        else if (offset == 0)
        {
            direction = 1;
            shift_interval = 1000;
        }

        if (us2ms(av_gettime()) - last_shifted > shift_interval)
        {
            offset += direction;
            last_shifted = us2ms(av_gettime());
            shift_interval = 200;
        }
    }
    else
        offset = 0;

    cli_get_cursor_pos(cst);
    int prev_x = cst->cursor_x;

    WriteConsoleW(cst->out.handle, strw + offset, strw_len - offset, NULL, NULL);

    cli_get_cursor_pos(cst);
    text_len = cst->cursor_x - prev_x;

    text_overflow = text_len >= max_len - 1;

    WriteConsole(cst->out.handle, "\x1b[0m", 5, NULL, NULL);

    free(str);
    free(strw);
    sb_reset(overlay_sb);
}

void cli_draw_overlay(CLIState *cst)
{
    if (!overlay_sb)
        overlay_sb = sb_create();

    if (cst->width <= 0 || cst->height <= 0)
        return;

    static const int timestamp_left_pad = 1;
    static const int timestamp_right_pad = 2;
    static const int timestamp_bottom_pad = 2;

    static const int volume_left_pad = 2;
    static const int volume_right_pad = 6;
    static const int volume_bottom_pad = 2;

    static const int progress_bottom_pad = 2;

    static const Color overlay_bg_color = {10, 10, 10};

    cli_draw_rect(cst, (Rect){0, cst->height - 3, cst->width + 10, 3}, overlay_bg_color);
    cli_draw_timestamp(cst,
                       (Vec2){timestamp_left_pad,
                              cst->height - timestamp_bottom_pad},
                       overlay_fg_color,
                       overlay_bg_color);

    cli_get_cursor_pos(cst);

    cli_draw_volume(cst,
                    (Vec2){cst->width - volume_right_pad,
                           cst->height - volume_bottom_pad},
                    overlay_fg_color,
                    overlay_bg_color);

    cli_draw_progress(cst,
                      (Vec2){cst->cursor_x + timestamp_right_pad, cst->height - progress_bottom_pad},
                      cst->width - cst->cursor_x - (volume_right_pad + timestamp_right_pad + volume_left_pad),
                      (float)cst->media_timestamp,
                      (float)cst->media_duration,
                      (Color){255, 0, 0},
                      (Color){150, 150, 150});

    cli_draw_now_playing(cst,
                         (Vec2){1,
                                cst->height - 1},
                         overlay_fg_color,
                         overlay_bg_color);

    cli_draw_media_control(cst,
                           (Vec2){cst->width / 2, cst->height - 1},
                           (Color){255, 255, 255},
                           overlay_bg_color);

    cli_get_cursor_pos(cst);

    cli_draw_padding(cst,
                     NULL,
                     cst->width - cst->cursor_x,
                     NULL,
                     &overlay_bg_color);
}