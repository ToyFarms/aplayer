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

    cst->force_redraw = false;

    cst->out.handle = NULL;
    cst->width = 0;
    cst->width = 0;
    cst->cursor_x = 0;
    cst->cursor_y = 0;
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
            free((*cst)->entries[i]);

        free((*cst)->entries);
    }

    free(*cst);
    *cst = NULL;
}

static inline void cli_cursor_to(HANDLE out, int x, int y)
{
    SetConsoleCursorPosition(out, (COORD){x, y});
}

static void _cli_get_console_size(HANDLE out, int *out_width, int *out_height)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(out, &csbi);

    *out_width = csbi.dwSize.X;
    *out_height = csbi.dwSize.Y;
}

void cli_get_console_size(CLIState *cst)
{
    _cli_get_console_size(cst->out.handle, &cst->width, &cst->height);
}

static void _cli_get_cursor_pos(HANDLE out, int *out_x, int *out_y)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(out, &csbi);

    *out_x = csbi.dwCursorPosition.X;
    *out_y = csbi.dwCursorPosition.Y;
}

void cli_get_cursor_pos(CLIState *cst)
{
    _cli_get_cursor_pos(cst->out.handle, &cst->cursor_x, &cst->cursor_y);
}

void cli_clear_screen(HANDLE out)
{
    int w, h;
    _cli_get_console_size(out, &w, &h);

    unsigned long written;

    FillConsoleOutputCharacter(out, ' ', w * h, (COORD){0, 0}, &written);
    FillConsoleOutputAttribute(out, 0, w * h, (COORD){0, 0}, &written);

    cli_cursor_to(out, 0, 0);
}

typedef enum LineState
{
    LINE_HOVERED,
    LINE_PLAYING,
    LINE_SELECTED,
    LINE_NORMAL,
} LineState;

static StringBuilder *list_sb;
static wchar_t strw[2048];

static wchar_t *cli_line_routine(CLIState *cst, int idx, LineState line_state, int *out_strlen)
{
    sb_appendf(list_sb, "\x1b[39;48;2;41;41;41m%d ", idx);
    switch (line_state)
    {
    case LINE_PLAYING:
        sb_appendf(list_sb, "\x1b[38;2;0;0;0;48;2;91;201;77m%s", cst->entries[idx]);
        break;
    case LINE_SELECTED:
        sb_appendf(list_sb, "\x1b[38;2;0;0;0;48;2;255;255;255m%s", cst->entries[idx]);
        break;
    case LINE_HOVERED:
        sb_appendf(list_sb, "\x1b[38;2;0;0;0;48;2;150;150;150m%s", cst->entries[idx]);
        break;
    case LINE_NORMAL:
        sb_appendf(list_sb, "%s", cst->entries[idx]);
        break;
    default:
        break;
    }

    char *str = sb_concat(list_sb);
    *out_strlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, strw, sizeof(strw) / sizeof(wchar_t));
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
                             int x, int y,
                             int length,
                             int fr, int fg, int fb,
                             int br, int bg, int bb)
{
    pad_sb = sb_create();

    char padding[length + 1];
    memset(padding, ' ', sizeof(padding));
    padding[length + 1] = '\0';

    bool foreground_color = fr >= 0 && fg >= 0 && fb >= 0;
    bool background_color = br >= 0 && bg >= 0 && bb >= 0;

    if (foreground_color && background_color)
        sb_appendf(pad_sb, "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm%s", fr, fg, fb, br, bg, bb, padding);
    else if (foreground_color)
        sb_appendf(pad_sb, "\x1b[38;2;%d;%d;%dm%s", fr, fg, fb, padding);
    else if (background_color)
        sb_appendf(pad_sb, "\x1b[48;2;%d;%d;%dm%s", br, bg, bb, padding);
    else
        sb_appendf(pad_sb, "%s", padding);

    if (foreground_color || background_color)
        sb_append(pad_sb, "\x1b[0m");

    char *str = sb_concat(pad_sb);

    if (x >= 0 && y >= 0)
        cli_cursor_to(cst->out.handle, x, y);

    WriteConsole(cst->out.handle, str, strlen(str), NULL, NULL);

    free(str);
    sb_reset(pad_sb);
}

static LineState *lines_state_cache;

static void cli_draw_list(CLIState *cst)
{
    int str_len;
    wchar_t *str;
    LineState line_state;

    if (cst->force_redraw)
        cli_clear_screen(cst->out.handle);

    for (int viewport_offset = 0;
         viewport_offset < cst->height && cst->entry_offset + viewport_offset < cst->entry_size;
         viewport_offset++)
    {
        int abs_entry_idx = cst->entry_offset + viewport_offset;
        line_state = cli_get_line_state(cst, abs_entry_idx);

        if (!cst->force_redraw && lines_state_cache[abs_entry_idx] && line_state == lines_state_cache[abs_entry_idx])
            continue;

        lines_state_cache[abs_entry_idx] = line_state;

        cli_cursor_to(cst->out.handle, 0, viewport_offset);

        str = cli_line_routine(cst, abs_entry_idx, line_state, &str_len);
        WriteConsoleW(cst->out.handle, str, str_len, NULL, NULL);

        cli_get_cursor_pos(cst);

        int pad = cst->width - cst->cursor_x;
        if (pad <= 0)
            continue;

        cli_draw_padding(cst, -1, -1, pad, -1, -1, -1, -1, -1, -1);
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
    cli_draw_overlay(cst);

    pthread_mutex_unlock(&cst->mutex);
}

static StringBuilder *overlay_sb;

static void cli_draw_rect(CLIState *cst, int x, int y, int w, int h, int r, int g, int b)
{
    if (x < 0 || y < 0 || w <= 0 || h <= 0)
        return;

    for (int current_y = y; current_y < y + h; current_y++)
        cli_draw_padding(cst, x, current_y, w, -1, -1, -1, r, g, b);
}

static const wchar_t blocks[] = {L'█', L'▉', L'▊', L'▋', L'▌', L'▍', L'▎', L'▎', L' '};
static const int block_len = 9;
static const float block_increment = 1.0f / (float)block_len;

static void cli_draw_hlinef(CLIState *cst,
                            int x, int y,
                            float length,
                            int fr, int fg, int fb,
                            int br, int bg, int bb,
                            bool reset_bg_color)
{
    int int_part = (int)length;
    float float_part = length - (float)int_part;

    cli_cursor_to(cst->out.handle, x, y);

    char *str;

    if (int_part > 0)
        cli_draw_padding(cst, -1, -1, int_part, -1, -1, -1, fr, fg, fb);

    sb_appendf(overlay_sb, "\x1b[48;2;%d;%d;%d;38;2;%d;%d;%dm", br, bg, bg, fr, fg, fb);
    str = sb_concat(overlay_sb);
    WriteConsole(cst->out.handle, str, strlen(str), NULL, NULL);
    free(str);
    sb_reset(overlay_sb);

    int block_index = block_len - (int)(roundf(float_part / block_increment) * block_increment * block_len);
    wchar_t final_block = blocks[block_index];
    WriteConsoleW(cst->out.handle, &final_block, 1, NULL, NULL);

    if (reset_bg_color)
        WriteConsole(cst->out.handle, "\x1b[0m", 4, NULL, NULL);
    else
        WriteConsole(cst->out.handle, "\x1b[39m", 5, NULL, NULL);
}

static void cli_draw_progress(CLIState *cst,
                              int x, int y,
                              int length,
                              float current,
                              float max,
                              int fr, int fg, int fb,
                              int br, int bg, int bb)
{
    float mapped_length = mapf(current, 0.0f, max, 0.0f, (float)length);

    cli_draw_hlinef(cst, x, y, mapped_length, fr, fg, fb, br, bg, bb, false);

    cli_get_cursor_pos(cst);
    cli_draw_padding(cst, -1, -1, (x + length) - cst->cursor_x, -1, -1, -1, -1, -1, -1);

    WriteConsole(cst->out.handle, "\x1b[0m", 4, NULL, NULL);
}

static void cli_draw_timestamp(CLIState *cst, int x, int y, int r, int g, int b)
{
    sb_appendf(overlay_sb,
               "\x1b[38;2;%d;%d;%dm%.2fs / %.2fs\x1b[0m",
               r, g, b,
               (double)cst->media_timestamp / 1000.0,
               (double)cst->media_duration / 1000.0);

    char *str = sb_concat(overlay_sb);

    cli_cursor_to(cst->out.handle, x, y);
    WriteConsole(cst->out.handle, str, strlen(str), NULL, NULL);

    free(str);
    sb_reset(overlay_sb);
}

static void cli_draw_volume(CLIState *cst, int x, int y, int r, int g, int b)
{
    char *volume_icon;

    if (cst->media_volume - 1e-3 < 0.0f)
        volume_icon = wchar2mbs(L"🔈");
    else if (cst->media_volume < 0.5f)
        volume_icon = wchar2mbs(L"🔉");
    else
        volume_icon = wchar2mbs(L"🔊");

    sb_appendf(overlay_sb,
               "%s\x1b[38;2;%d;%d;%dm%.0f\x1b[0m",
               volume_icon,
               r, g, b,
               cst->media_volume * 100.0f);

    char *str = sb_concat(overlay_sb);
    wchar_t strw[256];
    int strw_len = MultiByteToWideChar(CP_UTF8, 0, str, -1, strw, sizeof(strw) / sizeof(wchar_t));

    cli_cursor_to(cst->out.handle, x, y);
    WriteConsoleW(cst->out.handle, strw, strw_len, NULL, NULL);

    free(str);
    free(volume_icon);
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

    cli_draw_rect(cst, 0, cst->height - 3, cst->width + 10, 3, 10, 10, 10);
    cli_draw_timestamp(cst,
                       timestamp_left_pad,
                       cst->height - timestamp_bottom_pad,
                       230, 200, 150);

    cli_get_cursor_pos(cst);

    cli_draw_volume(cst,
                    cst->width - volume_right_pad,
                    cst->height - volume_bottom_pad,
                    230, 200, 150);

    if (!cst->media_duration <= 0 || !cst->media_timestamp <= 0)
        cli_draw_progress(cst,
                          cst->cursor_x + timestamp_right_pad, cst->height - progress_bottom_pad,
                          cst->width - cst->cursor_x - (volume_right_pad + timestamp_right_pad + volume_left_pad),
                          (float)cst->media_timestamp,
                          (float)cst->media_duration,
                          255, 0, 0,
                          150, 150, 150);
}

static HANDLE out_main;
static HANDLE out_alt;
static HANDLE out_cur;
static HANDLE in;
static DWORD in_main_mode;

static void cli_init_input()
{
    in = CreateFile("CONIN$",
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ,
                    NULL,
                    OPEN_ALWAYS,
                    0,
                    NULL);

    GetConsoleMode(in, &in_main_mode);

    DWORD in_mode = in_main_mode;
    in_mode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
    in_mode &= ~ENABLE_QUICK_EDIT_MODE;
    SetConsoleMode(in, in_mode);

    SetStdHandle(STD_INPUT_HANDLE, in);
}

void cli_buffer_switch(BUFFER_TYPE type)
{
    switch (type)
    {
    case BUF_MAIN:
        if (out_alt)
        {
            cli_clear_screen(out_alt);
            Sleep(100);
        }
        if (!out_main)
            out_main = CreateFile("CONOUT$",
                                  GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_WRITE,
                                  NULL,
                                  OPEN_ALWAYS,
                                  0,
                                  NULL);

        if (in && in_main_mode)
            SetConsoleMode(in, in_main_mode);

        SetConsoleActiveScreenBuffer(out_main);
        out_cur = out_main;
        break;
    case BUF_ALTERNATE:
        out_main = CreateFile("CONOUT$",
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_WRITE,
                              NULL,
                              OPEN_ALWAYS,
                              0,
                              NULL);
        out_alt =
            CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_WRITE,
                                      NULL,
                                      0,
                                      NULL);

        DWORD out_alt_mode;
        GetConsoleMode(out_alt, &out_alt_mode);
        out_alt_mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
        SetConsoleMode(out_alt, out_alt_mode);

        cli_init_input();

        SetConsoleActiveScreenBuffer(out_alt);

        SetConsoleCursorInfo(out_alt, &(CONSOLE_CURSOR_INFO){.bVisible = 0, .dwSize = 1});
        out_cur = out_alt;
        break;
    }
}

static INPUT_RECORD in_rec[128];
static Event in_event[128];

Events cli_read_in()
{
    if (!in)
    {
        av_log(NULL, AV_LOG_FATAL, "Input is uninitialized.\n");
        return (Events){.event = NULL, .event_size = 0};
    }

    ULONG read;
    BOOL exit;
    ReadConsoleInput(in, in_rec, sizeof(in_rec) / sizeof(INPUT_RECORD), &read);

    for (int i = 0; i < read; i++)
    {
        INPUT_RECORD e = in_rec[i];
        KEY_EVENT_RECORD k = e.Event.KeyEvent;
        MOUSE_EVENT_RECORD m = e.Event.MouseEvent;
        WINDOW_BUFFER_SIZE_RECORD b = e.Event.WindowBufferSizeEvent;

        EventType type;
        KeyEvent key_event;
        MouseEvent mouse_event;
        BufferChangedEvent buf_event;

        switch (e.EventType)
        {
        case KEY_EVENT:
            type = KEY_EVENT_TYPE;

            key_event.key_down = k.bKeyDown;
            key_event.acsii_key = k.uChar.AsciiChar;
            key_event.unicode_key = k.uChar.UnicodeChar;
            key_event.vk_key = k.wVirtualKeyCode;

            DWORD ks = k.dwControlKeyState;
            key_event.modifier_key = ((ks & SHIFT_PRESSED) << 0) | ((ks & LEFT_CTRL_PRESSED) << 1) | ((ks & RIGHT_CTRL_PRESSED) << 1) | ((ks & LEFT_ALT_PRESSED) << 2) | ((ks & RIGHT_ALT_PRESSED) << 2);

            break;
        case MOUSE_EVENT:
            type = MOUSE_EVENT_TYPE;

            mouse_event.x = m.dwMousePosition.X;
            mouse_event.y = m.dwMousePosition.Y;
            DWORD ms = m.dwButtonState;
            mouse_event.state = ((ms & FROM_LEFT_1ST_BUTTON_PRESSED) << 0) | ((ms & FROM_LEFT_2ND_BUTTON_PRESSED) << 1) | ((ms & FROM_LEFT_3RD_BUTTON_PRESSED) << 2);
            mouse_event.double_clicked = m.dwEventFlags == DOUBLE_CLICK;
            mouse_event.moved = m.dwEventFlags & MOUSE_MOVED;
            mouse_event.scrolled = m.dwEventFlags & MOUSE_WHEELED;
            mouse_event.scroll_delta = GET_WHEEL_DELTA_WPARAM(m.dwButtonState);

            break;
        case WINDOW_BUFFER_SIZE_EVENT:
            type = BUFFER_CHANGED_EVENT_TYPE;
            buf_event.width = b.dwSize.X;
            buf_event.height = b.dwSize.Y;

            break;
        default:
            type = UNKNOWN_EVENT_TYPE;
            break;
        }

        if (type == UNKNOWN_EVENT_TYPE)
            continue;

        in_event[i] = (Event){
            .type = type,
            .key_event = key_event,
            .mouse_event = mouse_event,
            .buf_event = buf_event,
        };
    }

    return (Events){.event = in_event, .event_size = read};
}

Handle cli_get_handle(BUFFER_TYPE type)
{
    HANDLE ret;
    switch (type)
    {
    case BUF_MAIN:
        ret = out_main;
        break;
    case BUF_ALTERNATE:
        ret = out_alt;
        break;
    default:
        break;
    }

    if (!ret)
    {
        av_log(NULL, AV_LOG_FATAL, "Output handle is uninitialized.\n");
        return (Handle){.handle = NULL};
    }
    return (Handle){.handle = ret};
}