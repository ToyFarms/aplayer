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

typedef struct FileListState
{
    int offset;
    int cursor;
    APPlaylist *pl;
    APArrayT(LineDef) * lines;
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

static APArrayT(LineDef) * generate_lines(APWidget *w)
{
    FileListState *state = w->state.tui->internal;
    APPlaylist *pl = state->pl;

    APArrayT(LineDef) *lines = ap_array_alloc(1, sizeof(LineDef));

    int current_row = 0;
    for (int i = 0; i < pl->groups->len; i++)
    {
        if (current_row > w->size.y + state->offset)
            continue;

        APEntryGroup group = ARR_INDEX(pl->groups, APEntryGroup *, i);

        sds header = sdsempty();
        header = ap_draw_str(header, ESC TCMD_BGFG, -1);
        header = ap_draw_hline(header, w->size.x);
        header = ap_draw_strf(header, "%2d", i + 1);
        header = ap_draw_str(header, ESC TCMD_BGFG, -1);
        header = ap_draw_strf(header, " %s", group.name);
        ap_array_append_resize(lines, &(LineDef){header, LINE_GROUPNAME}, 1);

        for (int j = 0; j < group.entries->len; j++)
        {
            if (current_row > w->size.y + state->offset)
                continue;

            sds file = sdsempty();
            APFile entry = ARR_INDEX(group.entries, APFile *, j);

            file = ap_draw_str(file, ESC TCMD_BGFG, -1);

            int num_len = sdslen(file);
            file = ap_draw_strf(file, "  %3d ", j + 1);
            num_len = sdslen(file) - num_len;

            file = ap_draw_str(file, ESC TCMD_BGFG, -1);

            if (is_ascii(entry.filename, -1))
            {
                file = ap_draw_str(
                    file, entry.filename,
                    MATH_MIN(strlen(entry.filename), w->size.x - num_len));
                file = ap_draw_padding(
                    file, w->size.x - strlen(entry.filename) - num_len);
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
        footer = ap_draw_str(footer, ESC TCMD_BGFG, -1);
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

    APArrayT(char *) *query = ap_array_alloc(16, sizeof(char *));

    for (int i = 0; i < w->size.y; i++)
    {
        int absline = i + state->offset;
        if (absline > state->lines->len - 1 || absline < 0)
            break;

        LineDef line = ARR_INDEX(state->lines, LineDef *, absline);
        if (line.data == NULL)
            continue;

        c = ap_draw_pos(c, VEC(w->pos.x, w->pos.y + i));
        // c = ap_draw_str(c, line.data, -1);

        query->len = 0;
#define LIT(str) &(char *){str}
        ap_array_append_resize(query, LIT("filelist"), 1);

        if (line.type == LINE_ENTRY)
            ap_array_append_resize(query, LIT("entry"), 1);
        else if (line.type == LINE_GROUPNAME)
            ap_array_append_resize(query, LIT("groupname"), 1);
        else if (line.type == LINE_EMPTY)
            ap_array_append_resize(query, LIT("empty"), 1);

        if (absline == state->cursor)
            ap_array_append_resize(query, LIT("hovered"), 1);

        APColor name_bg = APCOLOR(255, 0, 0, 255);
        APColor name_fg = APCOLOR(255, 0, 0, 255);
        APColor num_bg  = APCOLOR(255, 0, 0, 255);
        APColor num_fg  = APCOLOR(255, 0, 0, 255);

#define GET_COLOR(query, name, temp_str)                                       \
    for (int i = 0; i < (query)->len; i++)                                     \
        temp_str =                                                             \
            sdscatprintf(temp_str, "%s%s", ARR_INDEX(query, char **, i),       \
                         i == query->len - 1 ? "" : "-");                      \
    {                                                                          \
        APColor *__result = ap_dict_get(w->theme, temp_str);                   \
        if (__result)                                                          \
            name = *__result;                                                  \
    }                                                                          \
    sdsclear(temp_str);

        sds temp = sdsempty();
        ap_array_append_resize(query, LIT("bg"), 1);
        ap_array_append_resize(query, LIT("num"), 1);
        GET_COLOR(query, num_bg, temp);

        query->len = query->len - 1;
        ap_array_append_resize(query, LIT("name"), 1);
        GET_COLOR(query, name_bg, temp);

        query->len = query->len - 2;
        ap_array_append_resize(query, LIT("fg"), 1);
        ap_array_append_resize(query, LIT("num"), 1);
        GET_COLOR(query, num_fg, temp);

        query->len = query->len - 1;
        ap_array_append_resize(query, LIT("name"), 1);
        GET_COLOR(query, name_fg, temp);
        sdsfree(temp);

        c = AP_DRAW_STRC(4, c, line.data, num_bg, num_fg, name_bg, name_fg);
    }
    ap_array_free(&query);

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
