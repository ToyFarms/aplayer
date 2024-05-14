#include "ap_widgets.h"

typedef enum LineType
{
    LINE_EMPTY,
    LINE_GROUPNAME,
    LINE_ENTRY,
} LineType;

typedef struct LineDef
{
    sds data;
    LineType type;
} LineDef;

typedef T(LineDef) APArray *LineDefs;

typedef struct FileListState
{
    int offset;
    int cursor;
    APPlaylist *pl;
    LineDefs lines;
} FileListState;

void ap_widget_filelist_init(APWidget *w, APPlaylist *pl)
{
    w->state.tui->internal = calloc(1, sizeof(FileListState));
    FileListState *state = w->state.tui->internal;

    state->pl = pl;
}

static bool is_in_offset(int i, int *offsets, int offset_len)
{
    for (int j = 0; j < offset_len; j++)
        if (i == offsets[j])
            return true;

    return false;
}

static int get_current_group_index(int i, int *offsets, int offset_len)
{
    for (int j = 0; j < offset_len - 1; j++)
        if (i < offsets[j])
            return j - 1;

    if (i < offsets[offset_len])
        return offset_len;

    return -1;
}

static LineDefs generate_lines(APWidget *w)
{
    FileListState *state = w->state.tui->internal;
    APPlaylist *pl = state->pl;

    LineDefs lines = ap_array_alloc(1, sizeof(LineDef));

    int current_row = 0;
    for (int i = 0; i < pl->groups->len; i++)
    {
        if (current_row > w->size.y + state->offset)
            continue;

        APEntryGroup group = ARR_INDEX(pl->groups, APEntryGroup *, i);
        // TODO: Add colorscheme (dict) for each widget

        sds header = sdsempty();
        header = ap_draw_color(header, APCOLOR(0, 0, 0, 255),
                               APCOLOR(152, 195, 121, 255));
        header = ap_draw_hline(header, w->size.x);
        header = ap_draw_strf(header, "%2d", i + 1);
        header = ap_draw_strf(header, " %s", group.name);
        header = ap_draw_color(header, APCOLOR(255, 255, 255, 255),
                               APCOLOR(0, 0, 0, 255));
        ap_array_append_resize(lines, &(LineDef){header, LINE_GROUPNAME}, 1);

        for (int j = 0; j < group.entries->len; j++)
        {
            if (current_row > w->size.y + state->offset)
                continue;

            sds file = sdsempty();
            APFile entry = ARR_INDEX(group.entries, APFile *, j);

            file = ap_draw_color(file, w->fg, w->bg);

            int num_len = sdslen(file);
            file = ap_draw_strf(file, "  %3d ", j + 1);
            num_len = sdslen(file) - num_len;

            file =
                ap_draw_color(file, APCOLOR(200, 200, 200, 255), APCOLOR_ZERO);

            if (is_ascii(entry.filename, -1))
            {
                file = ap_draw_str(
                    file, entry.filename,
                    MATH_MIN(strlen(entry.filename), w->size.x - num_len));
                file = ap_draw_padding(
                    file,
                    w->size.x - strlen(entry.filename) - num_len);
            }
            else
            {
                int filename_width;
                int filename_len = strw_fit_into_column(
                    entry.filenamew, -1, w->size.x - num_len, &filename_width);
                file = ap_draw_strw(file, entry.filenamew, filename_len);
                file =
                    ap_draw_padding(file, w->size.x - filename_width - num_len);
            }

            file = ap_draw_reset_attr(file);
            ap_array_append_resize(lines, &(LineDef){file, LINE_ENTRY}, 1);
        }

        sds footer = sdsempty();
        footer = ap_draw_color(footer, APCOLOR_ZERO, w->bg);
        footer = ap_draw_hline(footer, w->size.x);
        ap_array_append_resize(lines, &(LineDef){footer, LINE_EMPTY}, 1);
    }

    return lines;
}

void ap_widget_filelist_draw(APWidget *w)
{
    FileListState *state = w->state.tui->internal;
    APPlaylist *pl = state->pl;
    sds c = w->state.tui->draw_cmd;

    if (!pl || !pl->groups->len)
        return;

    if (!state->lines)
        state->lines = generate_lines(w);

    for (int i = 0; i < w->size.y; i++)
    {
        int absline = i + state->offset;
        if (absline > state->lines->len - 1 || absline < 0)
            break;

        LineDef line = ARR_INDEX(state->lines, LineDef *, absline);
        if (line.data == NULL)
            continue;

        c = ap_draw_pos(c, VEC(w->pos.x, w->pos.y + i));

        if (absline == state->cursor)
        {
            c = ap_draw_hline(c, w->size.x);
            c = ap_draw_strf(c, "%d", state->cursor);
            continue;
        }

        c = ap_draw_strf(c, "%s", line.data);
    }

    w->state.tui->draw_cmd = c;
}

void ap_widget_filelist_on_event(APWidget *w, APEvent e)
{
    FileListState *state = w->state.tui->internal;

    if (e.type == AP_EVENT_KEY && e.event.key.keydown)
    {
        if (e.event.key.ascii == 'j')
            state->cursor += 1;
        else if (e.event.key.ascii == 'k')
            state->cursor = MATH_MAX(0, state->cursor - 1);
    }

    if (state->cursor - state->offset >= w->size.y - 10)
        state->offset += 1;
    else if (state->cursor - state->offset < 10)
        state->offset = MATH_MAX(0, state->offset - 1);
}

void ap_widget_filelist_free(APWidget *w)
{
    FileListState *state = w->state.tui->internal;
    for (int i = 0; i < state->lines->len; i++)
    {
        LineDef line = ARR_INDEX(state->lines, LineDef *, i);
        sdsfree(line.data);
        line.data = NULL;
    }
    ap_array_free(&state->lines);
    free(state);
    state = NULL;
}
