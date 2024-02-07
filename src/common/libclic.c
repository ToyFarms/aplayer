#include "libcli.h"

#define CLI_CHECK_INITIALIZED(fn_name, ret)                                                                  \
    do                                                                                                       \
    {                                                                                                        \
        if (!cst)                                                                                            \
        {                                                                                                    \
            av_log(NULL, AV_LOG_WARNING, "%s %s:%d CLI is not initialized.\n", fn_name, __FILE__, __LINE__); \
            ret;                                                                                             \
        };                                                                                                   \
    } while (0)

CLIState *cst;

static void cli_state_init()
{
    av_log(NULL, AV_LOG_DEBUG, "Initializing CLIState.\n");

    cst = (CLIState *)malloc(sizeof(CLIState));
    if (!cst)
    {
        av_log(NULL, AV_LOG_DEBUG, "Could not allocate CLIState.\n");
        return;
    }

    memset(cst, 0, sizeof(cst));

    pthread_mutex_init(&cst->mutex, NULL);

    cst->entry_offset = 0;

    cst->hovered_idx = -1;
    cst->selected_idx = -1;

    cst->pl = NULL;

    cst->force_redraw = false;

    cst->is_in_input_mode = false;
    cst->input_buffer_capacity = 4096;
    cst->input_buffer = (char *)malloc(cst->input_buffer_capacity * sizeof(char));
    memset(cst->input_buffer, '\0', sizeof(cst->input_buffer));
    cst->input_buffer_size = 0;

    cst->out = cli_get_handle();
    cst->width = 0;
    cst->width = 0;
    cst->cursor_x = 0;
    cst->cursor_y = 0;
    cst->mouse_clicked = false;

    cst->mouse_x = 0;
    cst->mouse_y = 0;

    cst->icon = (UnicodeSymbol){
        .volume_mute = wchar2mbs(L"🔇"),
        .volume_off = wchar2mbs(L"🔈"),
        .volume_low = wchar2mbs(L"🔉"),
        .volume_medium = wchar2mbs(L"🔊"),
        .volume_high = wchar2mbs(L"🔊"),

        .media_play = wchar2mbs(L"▶"),
        .media_pause = wchar2mbs(L"="),
        .media_next_track = wchar2mbs(L">"),
        .media_prev_track = wchar2mbs(L"<"),
    };

    cst->icon_nerdfont = (UnicodeSymbol){
        // .volume_mute = wchar2mbs(L"󰝟"),
        // .volume_off = wchar2mbs(L"󰖁"),
        // .volume_low = wchar2mbs(L"󰕿"),
        // .volume_medium = wchar2mbs(L"󰖀"),
        // .volume_high = wchar2mbs(L"󰕾"),
        .volume_mute = wchar2mbs(L"🔇"),
        .volume_off = wchar2mbs(L"🔈"),
        .volume_low = wchar2mbs(L"🔉"),
        .volume_medium = wchar2mbs(L"🔊"),
        .volume_high = wchar2mbs(L"🔊"),

        .media_play = wchar2mbs(L""),
        .media_pause = wchar2mbs(L""),
        .media_next_track = wchar2mbs(L"󰒭"),
        .media_prev_track = wchar2mbs(L"󰒮"),
    };
    cst->current_icon = &cst->icon_nerdfont;

    cst->prev_button_hovered = false;
    cst->playback_button_hovered = false;
    cst->next_button_hovered = false;
    cst->volume_control_hovered = false;
}

static void cli_state_free(CLIState **cst)
{
    av_log(NULL, AV_LOG_DEBUG, "Free CLIState.\n");

    if (!cst)
        return;

    if (!(*cst))
        return;

    if ((*cst)->mutex)
    {
        av_log(NULL, AV_LOG_DEBUG, "Destroy CLIState mutex.\n");
        pthread_mutex_destroy(&(*cst)->mutex);
    }

    free(*cst);
    *cst = NULL;
}

void cli_free()
{
    cli_state_free(&cst);
    cli_buffer_switch(BUF_MAIN);
}

static void cli_restore_session(const char *file);

int cli_init(Playlist *pl)
{
    if (!cst)
        cli_state_init();

    if (!cst)
        return -1;

    cli_get_console_size(cst);

    cst->pl = pl;
    cli_buffer_switch(BUF_ALTERNATE);

    if (!pl->entries)
    {
        if (file_exists("session.auto.json") && !file_empty("session.auto.json"))
            cli_restore_session("session.auto.json");
    }

    return 1;
}

void cli_cursor_to(Handle out, int x, int y)
{
    char buf[128];
    snprintf(buf, 128, "\x1b[%d;%dH", y + 1, x + 1);
    cli_write(out, buf, strlen(buf));
}

void cli_clear_screen(Handle out)
{
    cli_write(out, "\x1b[2J", 5);
}

static void cli_compute_offset()
{
    CLI_CHECK_INITIALIZED("cli_compute_offset", return);

    cst->force_redraw = false;
    int height_adjusted = cst->height - 3; // overlay area

    if (cst->selected_idx < 0)
    {
        cst->selected_idx = cst->pl->entry_size - 1;
        cst->entry_offset = cst->pl->entry_size - height_adjusted;
        cst->force_redraw = true;
    }
    else if (cst->selected_idx > cst->pl->entry_size - 1)
    {
        cst->selected_idx = 0;
        cst->entry_offset = 0;
        cst->force_redraw = true;
    }

    int scroll_margin = height_adjusted > 12 ? 5 : 1;

    if (cst->selected_idx - cst->entry_offset > height_adjusted - scroll_margin)
    {
        cst->entry_offset = FFMAX(FFMIN(cst->entry_offset + ((cst->selected_idx - cst->entry_offset) - (height_adjusted - scroll_margin)), cst->pl->entry_size - height_adjusted), 0);
        cst->force_redraw = true;
    }
    else if (cst->selected_idx - cst->entry_offset < scroll_margin)
    {
        cst->entry_offset = FFMAX(cst->entry_offset - (scroll_margin - (cst->selected_idx - cst->entry_offset)), 0);
        cst->force_redraw = true;
    }
}

static StringBuilder *list_sb;
static const Color overlay_fg_color = {230, 200, 150};
static const Color overlay_bg_color = {10, 10, 10};

static wchar_t *cli_line_routine(CLIState *cst, int idx, LineState line_state, int *out_strwlen)
{
    sb_appendf(list_sb, "\x1b[38;2;%d;%d;%d;48;2;41;41;41m%3d \x1b[39m", overlay_fg_color.r, overlay_fg_color.g, overlay_fg_color.b, idx);
    switch (line_state)
    {
    case LINE_PLAYING:
        sb_appendf(list_sb, "\x1b[38;2;0;0;0;48;2;91;201;77m%s", cst->pl->entries[idx].filename);
        break;
    case LINE_SELECTED:
        sb_appendf(list_sb, "\x1b[38;2;0;0;0;48;2;255;255;255m%s", cst->pl->entries[idx].filename);
        break;
    case LINE_HOVERED:
        sb_appendf(list_sb, "\x1b[38;2;0;0;0;48;2;150;150;150m%s", cst->pl->entries[idx].filename);
        break;
    case LINE_NORMAL:
        sb_appendf(list_sb, "%s", cst->pl->entries[idx].filename);
        break;
    default:
        break;
    }

    char *str = sb_concat(list_sb);

    wchar_t *strw = mbs2wchar(str, 2048, out_strwlen);

    free(str);
    sb_reset(list_sb);

    return strw;
}

static LineState cli_get_line_state(CLIState *cst, int idx)
{
    if (idx == cst->pl->playing_idx)
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
        sb_appendf(pad_sb,
                   "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm\x1b[%dX\x1b[0m",
                   fg->r, fg->g, fg->b,
                   bg->r, bg->g, bg->b,
                   length);
    else if (fg)
        sb_appendf(pad_sb,
                   "\x1b[38;2;%d;%d;%dm\x1b[%dX\x1b[0m",
                   fg->r, fg->g, fg->b,
                   length);
    else if (bg)
        sb_appendf(pad_sb,
                   "\x1b[48;2;%d;%d;%dm\x1b[%dX\x1b[0m",
                   bg->r, bg->g, bg->b,
                   length);
    else
        sb_appendf(pad_sb,
                   "\x1b[%dX", length);

    char *str = sb_concat(pad_sb);

    if (pos)
        cli_cursor_to(cst->out, pos->x, pos->y);

    cli_write(cst->out, str, strlen(str));

    free(str);
    sb_reset(pad_sb);
}

static void cli_draw_vline(CLIState *cst,
                           Vec2 pos,
                           int length,
                           int line_width,
                           const Color *fg,
                           const Color *bg)
{
    if (!pad_sb)
        pad_sb = sb_create();

    if (bg && fg)
        sb_appendf(pad_sb,
                   "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm\x1b[%dX\x1b[0m",
                   fg->r, fg->g, fg->b,
                   bg->r, bg->g, bg->b,
                   line_width);
    else if (fg)
        sb_appendf(pad_sb,
                   "\x1b[38;2;%d;%d;%dm\x1b[%dX\x1b[0m",
                   fg->r, fg->g, fg->b,
                   line_width);
    else if (bg)
        sb_appendf(pad_sb,
                   "\x1b[48;2;%d;%d;%dm\x1b[%dX\x1b[0m",
                   bg->r, bg->g, bg->b,
                   line_width);

    char *str = sb_concat(pad_sb);

    for (int y = pos.y; y < pos.y + length; y++)
    {
        cli_cursor_to(cst->out, pos.x, y);
        cli_write(cst->out, str, strlen(str));
    }

    free(str);
    sb_reset(pad_sb);
}

static LineState *lines_state_cache = NULL;
static int lines_state_cache_size = 0;

static void cli_draw_list(CLIState *cst)
{
    int strw_len;

    for (int viewport_offset = 0; //   - 3 : bottom overlay
         viewport_offset < cst->height - 3 && cst->entry_offset + viewport_offset < cst->pl->entry_size;
         viewport_offset++)
    {
        int abs_entry_idx = cst->entry_offset + viewport_offset;
        LineState line_state = cli_get_line_state(cst, abs_entry_idx);

        if (!cst->force_redraw && lines_state_cache[abs_entry_idx] && line_state == lines_state_cache[abs_entry_idx])
            continue;

        lines_state_cache[abs_entry_idx] = line_state;

        cli_cursor_to(cst->out, 0, viewport_offset);

        wchar_t *strw = cli_line_routine(cst, abs_entry_idx, line_state, &strw_len);
        cli_writew(cst->out, strw, strw_len);
        free(strw);

        cli_get_cursor_pos(cst);

        int pad = cst->width - cst->cursor_x - 6; // -6 : right overlay
        if (pad <= 0)
        {
            cli_draw_padding(cst, &(Vec2){cst->width - 6, viewport_offset}, 6, NULL, &overlay_bg_color);
            continue;
        }

        cli_draw_padding(cst, NULL, pad, NULL, NULL);
    }
}

static StringBuilder *overlay_sb;

static inline void cli_draw_rect(CLIState *cst, Rect rect, Color color)
{
    if (rect.x < 0 || rect.y < 0 || rect.w <= 0 || rect.h <= 0)
        return;

    cli_draw_vline(cst, (Vec2){rect.x, rect.y}, rect.h, rect.w, &color, &color);
}

static const wchar_t *blocks_horizontal = L" ▎▎▍▌▋▊▉█";
static const wchar_t *blocks_vertical = L" ▂▂▄▄▅▆▇█";
static const int block_len = 9;
static const float block_increment = 1.0f / (float)block_len;

static void cli_draw_hblock(CLIState *cst, Vec2 pos, float x, Color fg, Color bg)
{
    x = FFMIN(FFMAX(x, 0.0f), 1.0f);

    sb_appendf(overlay_sb,
               "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm",
               fg.r, fg.g, fg.b,
               bg.r, bg.g, bg.b);

    char *str = sb_concat(overlay_sb);

    cli_cursor_to(cst->out, pos.x, pos.y);
    cli_write(cst->out, str, strlen(str));

    int block_index = (int)(x * (block_len - 1));
    wchar_t block = blocks_horizontal[block_index];
    cli_writew(cst->out, &block, 1);

    free(str);
    sb_reset(overlay_sb);
}

static void cli_draw_hlinef(CLIState *cst,
                            Vec2 pos,
                            float length,
                            Color fg,
                            Color bg,
                            bool reset_bg_color)
{
    int int_part = (int)length;
    float float_part = length - (float)int_part;

    cli_cursor_to(cst->out, pos.x, pos.y);

    if (int_part > 0)
    {
        cli_draw_padding(cst, NULL, int_part, NULL, &fg);
        cli_cursor_to(cst->out, pos.x + int_part, pos.y);
    }

    cli_get_cursor_pos(cst);
    cli_draw_hblock(cst, (Vec2){cst->cursor_x, cst->cursor_y}, float_part, fg, bg);

    if (reset_bg_color)
        cli_write(cst->out, "\x1b[0m", 5);
    else
        cli_write(cst->out, "\x1b[39m", 6);
}

static void cli_draw_vblock(CLIState *cst, Vec2 pos, float x, int width, Color fg, Color bg)
{
    x = FFMIN(FFMAX(x, 0.0f), 1.0f);

    sb_appendf(overlay_sb,
               "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm",
               fg.r, fg.g, fg.b,
               bg.r, bg.g, bg.b);

    char *str = sb_concat(overlay_sb);

    cli_cursor_to(cst->out, pos.x, pos.y);
    cli_write(cst->out, str, strlen(str));

    int block_index = (int)(x * (block_len - 1));
    wchar_t block = blocks_vertical[block_index];

    for (int i = 0; i < width; i++)
        cli_writew(cst->out, &block, 1);

    free(str);
    sb_reset(overlay_sb);
}

static void cli_draw_vlinef(CLIState *cst,
                            Vec2 pos,
                            float length,
                            int line_width,
                            Color fg,
                            Color bg,
                            bool reset_bg_color)
{
    int int_part = (int)length;
    float float_part = length - (float)int_part;

    if (int_part > 0)
        cli_draw_vline(cst, (Vec2){pos.x, pos.y - int_part}, int_part, line_width, NULL, &fg);

    cli_cursor_to(cst->out, pos.x, pos.y - (int_part + 1));

    sb_appendf(overlay_sb, "\x1b[48;2;%d;%d;%d;38;2;%d;%d;%dm", bg.r, bg.g, bg.b, fg.r, fg.g, fg.b);

    char *str = sb_concat(overlay_sb);
    cli_write(cst->out, str, strlen(str));

    free(str);
    sb_reset(overlay_sb);

    int block_index = (int)(float_part * (block_len - 1));
    wchar_t final_block = blocks_vertical[block_index];
    for (int i = 0; i < line_width; i++)
        cli_writew(cst->out, &final_block, 1);

    if (reset_bg_color)
        cli_write(cst->out, "\x1b[0m", 5);
    else
        cli_write(cst->out, "\x1b[39m", 6);
}

static void cli_draw_progress(CLIState *cst,
                              Vec2 pos,
                              int length,
                              float current,
                              float max,
                              Color fg,
                              Color bg)
{
    // TODO: Add visualization of 'hovered' progress bar
    if (audio_is_initialized() &&
        cst->mouse_clicked &&
        cst->mouse_y == pos.y &&
        cst->mouse_x >= pos.x &&
        cst->mouse_x < pos.x + length)
    {
        int64_t new_timestamp = (int64_t)map((double)cst->mouse_x,
                                             (double)pos.x,
                                             (double)(pos.x + length),
                                             0.0f,
                                             (double)cst->pl->pst->duration);

        audio_seek_to(new_timestamp);
    }

    float mapped_length = mapf(current, 0.0f, max, 0.0f, (float)length);

    cli_draw_hlinef(cst, pos, mapped_length, fg, bg, false);

    cli_get_cursor_pos(cst);

    if ((pos.x + length) - cst->cursor_x > 0)
        cli_draw_padding(cst,
                         NULL,
                         (pos.x + length) - cst->cursor_x,
                         NULL,
                         NULL);

    cli_write(cst->out, "\x1b[0m", 5);
}

static void cli_draw_timestamp(CLIState *cst, Vec2 pos, Color fg, Color bg)
{
    cli_cursor_to(cst->out, pos.x, pos.y);

    Time ts = time_from_us((double)cst->pl->pst->timestamp);
    Time d = time_from_us((double)cst->pl->pst->duration);

    sb_appendf(overlay_sb,
               "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm",
               fg.r, fg.g, fg.b,
               bg.r, bg.g, bg.b);

    char *str = sb_concat(overlay_sb);

    cli_write(cst->out, str, strlen(str));

    free(str);
    sb_reset(overlay_sb);

    if (ts.h >= 1.0)
        sb_appendf(overlay_sb, "%01lld:", (int64_t)ts.h);

    sb_appendf(overlay_sb,
               ts.h >= 1.0 ? "%02lld:%02lld / " : "%01lld:%02lld / ",
               (int64_t)ts.m,
               (int64_t)ts.s);

    if (d.h >= 1.0)
        sb_appendf(overlay_sb, "%01lld:", (int64_t)d.h);

    sb_appendf(overlay_sb,
               d.h >= 1.0 ? "%02lld:%02lld\x1b[0m" : "%01lld:%02lld\x1b[0m",
               (int64_t)d.m,
               (int64_t)d.s);

    str = sb_concat(overlay_sb);

    cli_write(cst->out, str, strlen(str));

    free(str);
    sb_reset(overlay_sb);
}

static void cli_draw_volume(CLIState *cst, Vec2 pos, Color fg, Color bg)
{
    char *volume_icon;

    if (cst->pl->pst->muted)
        volume_icon = cst->current_icon->volume_mute;
    else if (cst->pl->pst->volume - 1e-3f < 0.0f)
        volume_icon = cst->current_icon->volume_off;
    else if (cst->pl->pst->volume < 0.35f)
        volume_icon = cst->current_icon->volume_low;
    else if (cst->pl->pst->volume < 0.75f)
        volume_icon = cst->current_icon->volume_medium;
    else
        volume_icon = cst->current_icon->volume_high;

    sb_appendf(overlay_sb,
               "\x1b[48;2;%d;%d;%dm%s\x1b[38;2;%d;%d;%dm%.0f\x1b[0m",
               bg.r, bg.g, bg.b,
               volume_icon,
               fg.r, fg.g, fg.b,
               cst->pl->pst->volume * 100.0f);

    char *str = sb_concat(overlay_sb);

    int strw_len;
    wchar_t *strw = mbs2wchar(str, 128, &strw_len);

    cli_cursor_to(cst->out, pos.x, pos.y);
    cli_writew(cst->out, strw, strw_len);

    cli_get_cursor_pos(cst);

    if (cst->mouse_y == pos.y && cst->mouse_x >= pos.x && cst->mouse_x < cst->cursor_x)
        cst->volume_control_hovered = true;
    else
        cst->volume_control_hovered = false;

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

    char *playback_icon = cst->pl->pst->paused ? cst->current_icon->media_play : cst->current_icon->media_pause;
    const char *hovered_color = "\x1b[38;2;0;0;0;48;2;255;255;255m";

    sb_appendf(overlay_sb,
               "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm",
               fg.r, fg.g, fg.b,
               bg.r, bg.g, bg.b);

    char *normal_color = sb_concat(overlay_sb);

    sb_reset(overlay_sb);

    cli_cursor_to(cst->out, center.x - 5, center.y);

    sb_appendf(overlay_sb,
               "%s %s %s %s %s %s %s %s %s",
               prev_collision ? hovered_color : normal_color,
               cst->current_icon->media_prev_track,
               prev_collision ? normal_color : "",

               playback_collision ? hovered_color : normal_color,
               playback_icon,
               playback_collision ? normal_color : "",

               next_collision ? hovered_color : normal_color,
               cst->current_icon->media_next_track,
               next_collision ? normal_color : "");

    char *str = sb_concat(overlay_sb);

    int strw_len;
    wchar_t *strw = mbs2wchar(str, 1024, &strw_len);

    cli_writew(cst->out, strw, strw_len);

    free(normal_color);
    free(strw);
    free(str);
    sb_reset(overlay_sb);
}

static void cli_draw_now_playing(CLIState *cst, Vec2 pos, Color fg, Color bg)
{
    static int64_t last_shifted = 0;
    static int shift_interval = 200;
    static int offset = 0;
    static int direction = 1;
    static int text_len = 0;

    cli_cursor_to(cst->out, pos.x, pos.y);

    sb_appendf(overlay_sb,
               "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm",
               fg.r, fg.g, fg.b,
               bg.r, bg.g, bg.b);

    char *str = sb_concat(overlay_sb);
    cli_write(cst->out, str, strlen(str));

    const char *playing = "Playing: ";
    cli_write(cst->out, playing, strlen(playing));

    if (cst->pl->playing_idx < 0)
        goto exit;

    int max_len = ((cst->width / 2) - 5) - (pos.x + strlen(playing));

    int strw_len;
    wchar_t *strw = mbs2wchar(cst->pl->entries[cst->pl->playing_idx].filename, 1024, &strw_len);

    if ((text_len + offset) > max_len)
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

    cli_writew(cst->out, strw + offset, strw_len - offset);

    cli_get_cursor_pos(cst);
    text_len = cst->cursor_x - prev_x;

    cli_write(cst->out, "\x1b[0m", 5);

    free(strw);
exit:
    free(str);
    sb_reset(overlay_sb);
}

typedef struct _LoudnessBarState
{
    float prev;
    float cap;
    int64_t last_cap_set;
} _LoudnessBarState;

static void _cli_draw_loudness_bar(CLIState *cst,
                                   Vec2 pos,
                                   int length,
                                   int width,
                                   Color bg,
                                   float loudness,
                                   Color cap_color,
                                   _LoudnessBarState *s)
{
    float y = map3f(loudness,
                    -70.0f, -14.0f, 0.0f,
                    0.0f, (float)length * 0.65f, (float)length);

    float lerp_y = 0.0f;

    if (cst->pl->pst->paused || cst->pl->pst->volume - 1e-3f < 0.0f)
    {
        lerp_y = lerpf(s->prev, 0, 0.5f);
        s->cap = lerpf(s->cap, 0, 0.5f);
    }
    else
    {
        lerp_y = lerpf(s->prev, y, 0.5f);

        // ease in interpolation t = 0 -> 2 in 1 seconds
        float t = FFMIN((av_gettime() - s->last_cap_set) / (float)ms2us(1000), 2.0f);
        s->cap -= ((float)cst->height * 0.01f) * t;
    }

    int color = (int)map3f(lerp_y,
                           0.0f, (float)length * 0.70f, (float)length,
                           0.0f, 100.0f, 255.0f);

    cli_draw_vlinef(cst,
                    (Vec2){pos.x, length},
                    lerp_y,
                    width,
                    (Color){color, 255 - color, 0},
                    lerp_y > s->cap ? cap_color : bg,
                    true);

    if (lerp_y > s->cap)
    {
        s->last_cap_set = av_gettime();
        float displayed_block = 1.0f - (lerp_y - (int)lerp_y);
        s->cap = lerp_y;

        if (lerp_y - (int)lerp_y < block_increment)
        {
            displayed_block = 0.0f;
            if (!(lerp_y - (int)lerp_y - 1e-5f < 0.0f))
                s->cap = (int)s->cap;
        }

        cli_draw_vblock(cst,
                        (Vec2){pos.x, length - (s->cap + 1.0f)},
                        1.0f - displayed_block,
                        width,
                        cap_color,
                        bg);
    }
    else
    {
        if (s->cap - (int)s->cap > block_increment)
        {
            cli_draw_vblock(cst,
                            (Vec2){pos.x, length - s->cap},
                            s->cap - (int)s->cap,
                            width,
                            bg,
                            cap_color);

            cli_draw_vblock(cst,
                            (Vec2){pos.x, length - s->cap - 1},
                            s->cap - (int)s->cap,
                            width,
                            cap_color,
                            bg);
        }
        else
        {
            cli_draw_vblock(cst,
                            (Vec2){pos.x, length - s->cap},
                            1.0f,
                            width,
                            cap_color,
                            bg);
        }
    }

    s->prev = lerp_y;
}

static void cli_draw_loudness(CLIState *cst, Vec2 pos, int length, Color bg)
{
    static _LoudnessBarState state_l = {0};
    static _LoudnessBarState state_r = {0};
    static const Color cap_color = {250, 250, 250};
    static int prev_height = 0;

    if (cst->pl->playing_idx < 0)
        return;

    if (prev_height != cst->height)
    {
        state_l.cap = mapf(state_l.cap, 0, prev_height, 0, cst->height);
        state_r.cap = mapf(state_r.cap, 0, prev_height, 0, cst->height);

        prev_height = cst->height;
    }

    cli_draw_rect(cst, (Rect){pos.x, pos.y, 6, length}, bg);

    _cli_draw_loudness_bar(cst,
                           (Vec2){pos.x + 1, pos.y},
                           length,
                           2,
                           bg,
                           cst->pl->pst->LUFS_current_l,
                           cap_color,
                           &state_l);

    _cli_draw_loudness_bar(cst,
                           (Vec2){(pos.x + 2) + 1, pos.y},
                           length,
                           2,
                           bg,
                           cst->pl->pst->LUFS_current_r,
                           cap_color,
                           &state_r);
}

static void cli_draw_media_info(CLIState *cst, Vec2 pos, Color fg, Color bg)
{
    StreamState *sst = _audio_get_stream();
    if (!sst)
        return;

    if (!sst->audiodec)
        return;

    AVCodecContext *avctx = sst->audiodec->avctx;
    if (!avctx)
        return;

    cli_cursor_to(cst->out, pos.x, pos.y);

    sb_appendf(overlay_sb, "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm", fg.r, fg.g, fg.b, bg.r, bg.g, bg.b);
    sb_appendf(overlay_sb,
               "ch=%d sr=%dk ld=%.1f g=%.1f f=%.2f",
               avctx->ch_layout.nb_channels,
               avctx->sample_rate / 1000,
               cst->pl->pst->LUFS_avg,
               cst->pl->pst->LUFS_target - cst->pl->pst->LUFS_avg,
               powf(10.0f, (cst->pl->pst->LUFS_target - cst->pl->pst->LUFS_avg) / 20.0f));

    sb_append(overlay_sb, "\x1b[0m");

    char *str = sb_concat(overlay_sb);

    cli_write(cst->out, str, strlen(str));

    free(str);
    sb_reset(overlay_sb);
}

static void cli_draw_input(CLIState *cst, Vec2 pos, Color fg, Color bg)
{
    if (!cst->is_in_input_mode)
        return;

    cli_cursor_to(cst->out, pos.x, pos.y);
    char *input = (char *)malloc((cst->input_buffer_size + 1) * sizeof(char));
    memcpy_s(input, cst->input_buffer_size, cst->input_buffer, cst->input_buffer_size * sizeof(char));
    input[cst->input_buffer_size] = '\0';

    sb_appendf(overlay_sb,
               "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm%s%s\x1b[0m",
               fg.r, fg.g, fg.b,
               bg.r, bg.g, bg.b,
               cst->is_in_input_mode ? ":" : "",
               input);

    char *str = sb_concat(overlay_sb);
    cli_write(cst->out, str, strlen(str));

    free(str);
    free(input);
    sb_reset(overlay_sb);
}

static void cli_draw_overlay()
{
    CLI_CHECK_INITIALIZED("cli_draw_overlay", return);

    static int64_t last_overlay_draw = 0;

    if (av_gettime() - last_overlay_draw < ms2us(50))
        return;

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

    cli_draw_loudness(cst, (Vec2){cst->width - 6, 0}, cst->height - 2, overlay_bg_color);

    cli_draw_rect(cst, (Rect){0, cst->height - 3, cst->width, 3}, overlay_bg_color);

    cli_draw_timestamp(cst,
                       (Vec2){timestamp_left_pad,
                              cst->height - timestamp_bottom_pad},
                       overlay_fg_color,
                       overlay_bg_color);

    cli_get_cursor_pos(cst);
    Vec2 pre_volume = (Vec2){cst->cursor_x, cst->cursor_y};

    cli_draw_volume(cst,
                    (Vec2){cst->width - volume_right_pad,
                           cst->height - volume_bottom_pad},
                    overlay_fg_color,
                    overlay_bg_color);

    cli_draw_progress(cst,
                      (Vec2){pre_volume.x + timestamp_right_pad, cst->height - progress_bottom_pad},
                      cst->width - pre_volume.x - (volume_right_pad + timestamp_right_pad + volume_left_pad),
                      (float)cst->pl->pst->timestamp,
                      (float)cst->pl->pst->duration,
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

    if (!cst->is_in_input_mode)
        cli_draw_media_info(cst,
                            (Vec2){(cst->width / 2) + 7,
                                   cst->height - 1},
                            overlay_fg_color,
                            overlay_bg_color);
    cli_draw_input(cst, (Vec2){(cst->width / 2) + 7, cst->height - 1}, overlay_fg_color, overlay_bg_color);

    last_overlay_draw = av_gettime();
}

static void cli_draw()
{
    CLI_CHECK_INITIALIZED("cli_draw", return);

    pthread_mutex_lock(&cst->mutex);

    if (!list_sb)
        list_sb = sb_create();

    cli_get_console_size(cst);

    if (!lines_state_cache)
    {
        lines_state_cache = (LineState *)malloc(cst->pl->entry_size * sizeof(LineState));
        lines_state_cache_size = cst->pl->entry_size;
    }
    else if (lines_state_cache_size != cst->pl->entry_size)
    {
        lines_state_cache = (LineState *)realloc(lines_state_cache, cst->pl->entry_size * sizeof(LineState));
        lines_state_cache_size = cst->pl->entry_size;
    }

    cli_draw_list(cst);
    if (cst->force_redraw)
        cli_draw_overlay();

    pthread_mutex_unlock(&cst->mutex);
}

void cli_shuffle_entry()
{
    CLI_CHECK_INITIALIZED("cli_shuffle_entry", return);

    File prev_selected = {.path = NULL};
    File prev_playing = {.path = NULL};

    if (cst->selected_idx >= 0)
        prev_selected = cst->pl->entries[cst->selected_idx];

    if (cst->pl->playing_idx >= 0)
        prev_playing = cst->pl->entries[cst->pl->playing_idx];

    shuffle_array(cst->pl->entries, cst->pl->entry_size, sizeof(cst->pl->entries[0]));

    int new_idx = 0;
    if (prev_playing.path && array_find(cst->pl->entries,
                                        cst->pl->entry_size,
                                        sizeof(cst->pl->entries[0]),
                                        &prev_playing,
                                        file_compare_function,
                                        &new_idx))
    {
        cst->pl->playing_idx = new_idx;
        cst->selected_idx = new_idx;
        cli_compute_offset();
        cst->selected_idx = -1;
    }
    else if (prev_selected.path && array_find(cst->pl->entries,
                                              cst->pl->entry_size,
                                              sizeof(cst->pl->entries[0]),
                                              &prev_selected,
                                              file_compare_function,
                                              &new_idx))
    {
        cst->selected_idx = new_idx;
        cli_compute_offset();
    }

    cst->force_redraw = true;
    cli_draw();
}

static int sort_method_alphabetically(const void *a, const void *b)
{
    File *af = (File *)a;
    File *bf = (File *)b;

    wchar_t *strw_af = mbs2wchar(af->filename, 1024, NULL);
    wchar_t *strw_bf = mbs2wchar(bf->filename, 1024, NULL);

    int ret = wcscoll(strw_af, strw_bf);

    free(strw_af);
    free(strw_bf);

    return ret;
}

static int sort_method_ctime_desc(const void *a, const void *b)
{
    File *af = (File *)a;
    File *bf = (File *)b;

    return af->stat.st_ctime - bf->stat.st_ctime;
}

static int sort_method_ctime_asc(const void *a, const void *b)
{
    File *af = (File *)a;
    File *bf = (File *)b;

    return bf->stat.st_ctime - af->stat.st_ctime;
}

void cli_sort_entry(SortMethod sort, SortFlag flag)
{
    CLI_CHECK_INITIALIZED("cli_shuffle_entry", return);

    File prev_selected = {.path = NULL};
    File prev_playing = {.path = NULL};

    if (cst->selected_idx >= 0)
        prev_selected = cst->pl->entries[cst->selected_idx];

    if (cst->pl->playing_idx >= 0)
        prev_playing = cst->pl->entries[cst->pl->playing_idx];

    int (*sort_method)(const void *a, const void *b) = NULL;
    bool reverse = false;

    int sort_type = (sort << 16) | flag;
    switch (sort_type)
    {
    case (SORT_CTIME << 16) | SORT_FLAG_ASC:
        sort_method = sort_method_ctime_asc;
        break;
    case (SORT_CTIME << 16) | SORT_FLAG_DESC:
        sort_method = sort_method_ctime_desc;
        break;
    case (SORT_ALPHABETICALLY << 16) | SORT_FLAG_ASC:
        sort_method = sort_method_alphabetically;
        break;
    case (SORT_ALPHABETICALLY << 16) | SORT_FLAG_DESC:
        sort_method = sort_method_alphabetically;
        reverse = true;
        break;
    default:
        break;
    }

    if (!sort_method)
        return;

    qsort(cst->pl->entries, cst->pl->entry_size, sizeof(cst->pl->entries[0]), sort_method);
    if (reverse)
        reverse_array(cst->pl->entries, cst->pl->entry_size, sizeof(cst->pl->entries[0]));

    int new_idx = 0;
    if (prev_playing.path && array_find(cst->pl->entries,
                                        cst->pl->entry_size,
                                        sizeof(cst->pl->entries[0]),
                                        &prev_playing,
                                        file_compare_function,
                                        &new_idx))
    {
        cst->pl->playing_idx = new_idx;
        cst->selected_idx = new_idx;
        cli_compute_offset();
        cst->selected_idx = -1;
    }
    else if (prev_selected.path && array_find(cst->pl->entries,
                                              cst->pl->entry_size,
                                              sizeof(cst->pl->entries[0]),
                                              &prev_selected,
                                              file_compare_function,
                                              &new_idx))
    {
        cst->selected_idx = new_idx;
        cli_compute_offset();
    }

    cst->force_redraw = true;
    cli_draw();
}

static void cli_selected_go_down(int *last)
{
    if (cst->selected_idx < 0 && cst->pl->playing_idx >= 0)
        cst->selected_idx = cst->pl->playing_idx;
    else if (cst->selected_idx < 0)
    {
        cst->selected_idx = -1;
        *last = 0;
    }

    cst->selected_idx++;
}

static void cli_selected_go_up(int *last)
{
    if (cst->selected_idx < 0 && cst->pl->playing_idx >= 0)
        cst->selected_idx = cst->pl->playing_idx;
    else if (cst->selected_idx < 0)
    {
        cst->selected_idx = *last;
        *last = 0;
    }

    cst->selected_idx--;
}

static void cli_restore_session(const char *file)
{
    int success;
    SessionState st = session_read(file, &success);

    // TODO: Add message toast for info and error messages
    if (success < 0)
        return;

    // Temporary solution
    const char *loading = "Loading Last Session...";
    cli_cursor_to(cst->out, (cst->width / 2) - (strlen(loading) / 2), cst->height / 2);
    cli_write(cst->out, loading, strlen(loading));

    playlist_update_entry(st.entries, st.entry_size);

    cli_playlist_play(st.playing);

    audio_seek_to(st.timestamp);
    audio_set_volume(st.volume);

    if (st.paused)
        audio_pause();

    cst->selected_idx = FFMAX(cst->pl->playing_idx, 0);
    cli_compute_offset();
    cli_draw();
}

static void cli_save_session(const char *file)
{
    SessionState st = {
        .entries = cst->pl->entries,
        .entry_size = cst->pl->entry_size,
        .paused = audio_is_paused(),
        .playing = cst->pl->playing_idx,
        .timestamp = audio_get_timestamp(),
        .volume = audio_get_volume()};

    session_write(file, st);
}

static bool should_close = false;
static bool fast_scroll = false;

static void cli_handle_event_key(KeyEvent ev)
{
    if (!ev.key_down)
    {
        if (ev.vk_key == VK_MENU)
            fast_scroll = false;
        return;
    }

    if (cst->is_in_input_mode)
    {
        if (ev.vk_key == VIRT_ESCAPE)
        {
            cli_write(cst->out, "\x1b[?25l", 7);
            cst->is_in_input_mode = false;
            goto clear_buffer;
        }
        else if (ev.ascii_key == 'q')
            should_close = true;
        else if (ev.vk_key == VIRT_RETURN)
        {
            cli_write(cst->out, "\x1b[?25l", 7);
            cst->is_in_input_mode = false;
            int index = atoi(cst->input_buffer);
            cst->selected_idx = FFMAX(FFMIN(index, cst->pl->entry_size - 1), 0);

            cli_compute_offset();
            cli_draw();

            goto clear_buffer;
        }
        else if (ev.vk_key == VIRT_BACK)
        {
            cst->input_buffer_size = FFMAX(cst->input_buffer_size - 1, 0);
        }
        else if (is_numeric(&ev.ascii_key))
        {
            cst->input_buffer[cst->input_buffer_size] = ev.ascii_key;
            cst->input_buffer_size++;
        }

        return;
    clear_buffer:
        memset(cst->input_buffer, '\0', sizeof(cst->input_buffer));
        cst->input_buffer_size = 0;
        return;
    }

    bool need_redraw = false;
    char key = ev.ascii_key;
    static int selected_before_escaped = 0;

    if (key == 'q')
    {
        should_close = true;
        return;
    }
    else if (key == 'j')
    {
        cli_selected_go_down(&selected_before_escaped);
        need_redraw = true;
    }
    else if (key == 'k')
    {
        cli_selected_go_up(&selected_before_escaped);
        need_redraw = true;
    }
    else if (ev.vk_key == VIRT_DOWN && ev.modifier_key & CTRL_KEY_PRESSED)
        audio_set_volume(FFMAX(audio_get_volume() - 0.05f, 0));
    else if (ev.vk_key == VIRT_UP && ev.modifier_key & CTRL_KEY_PRESSED)
    {
        audio_set_volume(FFMIN(audio_get_volume() + 0.05f, 1.25f));
    }
    else if (ev.vk_key == VIRT_UP)
    {
        cli_selected_go_up(&selected_before_escaped);
        need_redraw = true;
    }
    else if (ev.vk_key == VIRT_DOWN)
    {
        cli_selected_go_down(&selected_before_escaped);
        need_redraw = true;
    }
    else if (ev.vk_key == VIRT_LEFT)
        audio_seek(-2.5 * (double)AV_TIME_BASE);
    else if (ev.vk_key == VIRT_RIGHT)
        audio_seek(2.5 * (double)AV_TIME_BASE);
    else if (ev.vk_key == VIRT_SPACE)
        audio_toggle_play();
    else if (ev.ascii_key == 'N')
        cli_playlist_next();
    else if (ev.ascii_key == 'P')
        cli_playlist_prev();
    else if (ev.ascii_key == 'S')
        cli_sort_entry(SORT_CTIME, SORT_FLAG_ASC);
    else if (ev.ascii_key == 's')
        cli_shuffle_entry();
    else if (ev.ascii_key == 'm')
    {
        if (audio_is_muted())
            audio_unmute();
        else
            audio_mute();
    }
    else if (ev.ascii_key == 'g')
    {
        cst->selected_idx = 0;
        need_redraw = true;
    }
    else if (ev.ascii_key == 'G')
    {
        cst->selected_idx = cst->pl->entry_size - 1;
        need_redraw = true;
    }
    else if (ev.ascii_key == ':')
    {
        cst->is_in_input_mode = true;
        cli_write(cst->out, "\x1b[?25h", 7);
    }
    else if (ev.ascii_key == 'r')
        cli_restore_session("session.json");
    else if (ev.ascii_key == 'w')
        cli_save_session("session.json");

    if (fast_scroll && ev.ascii_key == 'j')
    {
        cst->entry_offset = FFMIN(cst->entry_offset + 1, cst->pl->entry_size - cst->height);
        cst->force_redraw = true;
        cli_draw();
    }
    else if (fast_scroll && ev.ascii_key == 'k')
    {
        cst->entry_offset = FFMAX(cst->entry_offset - 1, 0);
        cst->force_redraw = true;
        cli_draw();
    }

    if (ev.vk_key == VIRT_MENU)
        fast_scroll = true;

    if (ev.vk_key == VIRT_RETURN)
        cli_playlist_play(cst->selected_idx);
    else if (ev.vk_key == VIRT_ESCAPE)
    {
        selected_before_escaped = cst->selected_idx;
        cst->selected_idx = -1;
        cli_draw();
    }

    if (need_redraw)
    {
        cli_compute_offset();
        cli_draw();
    }
}

static void cli_handle_event_mouse(MouseEvent ev)
{
    cst->mouse_x = ev.x;
    cst->mouse_y = ev.y;
    // TODO: Move overlay event handling in this function in the related widget
    cst->mouse_clicked = ev.state & MOUSE_LEFT_CLICKED;

    // overlay area
    if (ev.y > cst->height - 4)
    {
        if (ev.state & MOUSE_LEFT_CLICKED)
        {
            if (cst->prev_button_hovered)
                cli_playlist_prev();
            else if (cst->playback_button_hovered)
                audio_toggle_play();
            else if (cst->next_button_hovered)
                cli_playlist_next();
        }

        if (ev.scrolled && cst->volume_control_hovered)
            ev.scroll_delta > 0
                ? audio_set_volume(FFMIN(audio_get_volume() + 0.05f, 1.25f))
                : audio_set_volume(FFMAX(audio_get_volume() - 0.05f, 0));

        return;
    }

    if (ev.scrolled)
    {
        int delta = fast_scroll ? 5 : 1;
        if (ev.scroll_delta > 0)
            delta *= -1;

        cst->entry_offset += delta;

        cst->force_redraw = true;
        if (cst->entry_offset < 0 || cst->entry_offset > cst->pl->entry_size - cst->height)
            cst->force_redraw = false;

        cst->entry_offset = FFMIN(FFMAX(cst->entry_offset, 0), cst->pl->entry_size - cst->height);

        cli_draw();
    }
    else if (ev.moved)
    {
        cst->hovered_idx = cst->entry_offset + ev.y;
        cst->force_redraw = false;
        cli_draw();
    }

    if (ev.state & MOUSE_LEFT_CLICKED)
        cst->selected_idx = cst->hovered_idx;

    if (ev.state & MOUSE_LEFT_CLICKED && ev.double_clicked)
        if (cst->selected_idx >= 0)
        {
            cst->pl->playing_idx = cst->selected_idx;
            cli_draw();
            cli_playlist_play(cst->selected_idx);
        }

    cst->force_redraw = false;
    cli_draw();
}

static int cli_handle_event_buffer_changed(BufferChangedEvent ev)
{
    cli_get_console_size(cst);

    cst->force_redraw = true;
    cli_draw();
}

static void cli_handle_event(Event ev)
{
    switch (ev.type)
    {
    case KEY_EVENT_TYPE:
        cli_handle_event_key(ev.key_event);
        break;
    case MOUSE_EVENT_TYPE:
        cli_handle_event_mouse(ev.mouse_event);
        break;
    case BUFFER_CHANGED_EVENT_TYPE:
        cli_handle_event_buffer_changed(ev.buf_event);
        break;
    default:
        break;
    }
}

static void *update_thread(void *arg)
{
    static int64_t last_update = 0;

    while (cst)
    {
        if (av_gettime() - last_update > ms2us(1000))
        {
            cli_save_session("session.auto.json");
            last_update = av_gettime();
        }

        pthread_mutex_lock(&cst->mutex);
        cli_draw_overlay();
        pthread_mutex_unlock(&cst->mutex);

        av_usleep(ms2us(50));
    }
}

void cli_event_loop()
{
    CLI_CHECK_INITIALIZED("cli_event_loop", return);

    pthread_t update_thread_id;
    pthread_create(&update_thread_id, NULL, update_thread, NULL);

    while (!should_close)
    {
        Events events = cli_read_in();
        if (!events.event)
        {
            should_close = true;
            break;
        }
        for (int i = 0; i < events.event_size; i++)
        {
            cli_handle_event(events.event[i]);
        }
    }
}

void cli_playlist_next()
{
    CLI_CHECK_INITIALIZED("cli_playlist_next", return);

    playlist_next(cli_playlist_next);
    cst->selected_idx = cst->pl->playing_idx;

    cli_compute_offset();
    cli_draw();
}

void cli_playlist_prev()
{
    CLI_CHECK_INITIALIZED("cli_playlist_prev", return);

    playlist_prev(cli_playlist_next);
    cst->selected_idx = cst->pl->playing_idx;

    cli_compute_offset();
    cli_draw();
}

void cli_playlist_play(int index)
{
    CLI_CHECK_INITIALIZED("cli_playlist_play", return);

    if (index < 0 || index > cst->pl->entry_size)
        return;

    playlist_play_idx(index, cli_playlist_next);
    cst->selected_idx = cst->pl->playing_idx;

    cli_compute_offset();
    cli_draw();
}
