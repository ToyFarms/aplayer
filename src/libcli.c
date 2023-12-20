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

    cst->entries = NULL;
    cst->entry_size = 0;
    cst->entry_offset = 0;

    cst->hovered_idx = -1;
    cst->playing_idx = -1;
    cst->selected_idx = -1;

    cst->force_redraw = false;

    cst->out = NULL;
    cst->width = 0;
    cst->width = 0;
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

static void _cli_get_console_size(HANDLE out, int *out_width, int *out_height)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(out, &csbi);

    *out_width = csbi.dwSize.X;
    *out_height = csbi.dwSize.Y;
}

void cli_get_console_size(CLIState *cst)
{
    _cli_get_console_size(cst->out, &cst->width, &cst->height);
}

void cli_clear_screen(HANDLE out)
{
    int w, h;
    _cli_get_console_size(out, &w, &h);

    unsigned long written;

    FillConsoleOutputCharacter(out, ' ', w * h, (COORD){0, 0}, &written);
    FillConsoleOutputAttribute(out, 0, w * h, (COORD){0, 0}, &written);

    SetConsoleCursorPosition(out, (COORD){0, 0});
}

typedef enum LineState
{
    LINE_HOVERED,
    LINE_PLAYING,
    LINE_SELECTED,
    LINE_NORMAL,
} LineState;

static StringBuilder *sb;
static wchar_t strw[2048];

static wchar_t *_cli_line_routine(CLIState *cst, int idx, LineState line_state, int *out_strlen)
{
    sb_appendf(sb, "\x1b[0m%d ", idx);
    switch (line_state)
    {
    case LINE_PLAYING:
        sb_appendf(sb, "\x1b[38;2;0;0;0;48;2;91;201;77m%s\x1b[0m", cst->entries[idx]);
        break;
    case LINE_SELECTED:
        sb_appendf(sb, "\x1b[38;2;0;0;0;48;2;255;255;255m%s\x1b[0m", cst->entries[idx]);
        break;
    case LINE_HOVERED:
        sb_appendf(sb, "\x1b[38;2;0;0;0;48;2;150;150;150m%s\x1b[0m", cst->entries[idx]);
        break;
    case LINE_NORMAL:
        sb_appendf(sb, "%s", cst->entries[idx]);
        break;
    default:
        break;
    }

    char *str = sb_concat(sb);
    *out_strlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, strw, sizeof(strw) / sizeof(wchar_t));
    free(str);

    sb_reset(sb);

    return strw;
}

static LineState _cli_get_line_state(CLIState *cst, int idx)
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

static LineState *lines_state_cache;

static void _cli_draw(CLIState *cst)
{
    int str_len;
    wchar_t *str;
    LineState line_state;

    if (cst->force_redraw)
        cli_clear_screen(cst->out);

    for (int viewport_offset = 0;
         viewport_offset < cst->height && cst->entry_offset + viewport_offset < cst->entry_size;
         viewport_offset++)
    {
        int abs_entry_idx = cst->entry_offset + viewport_offset;
        line_state = _cli_get_line_state(cst, abs_entry_idx);

        if (!cst->force_redraw && lines_state_cache[abs_entry_idx] && line_state == lines_state_cache[abs_entry_idx])
            continue;

        lines_state_cache[abs_entry_idx] = line_state;

        SetConsoleCursorPosition(cst->out, (COORD){0, viewport_offset});

        str = _cli_line_routine(cst, abs_entry_idx, line_state, &str_len);
        WriteConsoleW(cst->out, str, str_len, NULL, NULL);
    }
}

void cli_draw(CLIState *cst)
{
    if (!sb)
        sb = sb_create();

    cli_get_console_size(cst);

    if (!lines_state_cache)
        lines_state_cache = (LineState *)malloc(cst->entry_size * sizeof(LineState));
    else if (sizeof(lines_state_cache) / sizeof(LineState) != cst->entry_size)
        realloc(lines_state_cache, cst->entry_size * sizeof(LineState));

    _cli_draw(cst);

    sb_reset(sb);
}

static HANDLE out_main;
static HANDLE out_alt;
static HANDLE out_cur;
static HANDLE in;
static DWORD in_main_mode;

static void _cli_init_input()
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

        _cli_init_input();

        SetConsoleActiveScreenBuffer(out_alt);

        SetConsoleCursorInfo(out_alt, &(CONSOLE_CURSOR_INFO){.bVisible = 0, .dwSize = 1});
        out_cur = out_alt;
        break;
    }
}

static INPUT_RECORD in_rec[128];

INPUT_RECORD *cli_read_in(unsigned long *out_size)
{
    if (!in)
    {
        av_log(NULL, AV_LOG_FATAL, "Input is uninitialized.\n");
        return NULL;
    }

    ULONG read;
    BOOL exit;
    ReadConsoleInput(in, in_rec, sizeof(in_rec) / sizeof(INPUT_RECORD), &read);

    if (out_size)
        *out_size = read;

    return in_rec;
}

HANDLE cli_get_handle(BUFFER_TYPE type)
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
    }

    if (!ret)
    {
        av_log(NULL, AV_LOG_FATAL, "Output handle is uninitialized.\n");
        return NULL;
    }
    return ret;
}