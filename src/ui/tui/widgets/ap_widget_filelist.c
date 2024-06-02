#include "ap_widgets.h"

typedef struct LineDef LineDef;

static void generate_lines_template(APWidget *w);
static uint64_t generate_line_signature(APWidget *w, LineDef line,
                                        int line_index);
static void calculate_offset(APWidget *w);
static void scroll_cursor(APWidget *w, int n, bool absolute);
static void scroll_window(APWidget *w, int n, bool absolute);

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
    int index;
    int rel_index;
    int group_index;
} LineDef;

typedef struct FileListState
{
    int offset;
    int cursor;
    APPlaylist *pl;
    APArrayT(LineDef) * lines;
    APArrayT(uint64_t) * lines_sig;
} FileListState;

#define GET_STATE(widget) ((FileListState *)(widget)->state.tui->internal)

void ap_widget_filelist_init(APWidget *w, APPlaylist *pl)
{
    w->state.tui->internal = calloc(1, sizeof(FileListState));
    FileListState *state = GET_STATE(w);

    state->pl = pl;
    state->lines = ap_array_alloc(w->size.y, sizeof(LineDef));
    state->lines_sig = ap_array_alloc(w->size.y, sizeof(uint64_t));
    generate_lines_template(w);
}

void ap_widget_filelist_draw(APWidget *w)
{
    FileListState *state = GET_STATE(w);
    APPlaylist *pl = state->pl;
    sds c = w->state.tui->draw_cmd;

    if (!pl || !pl->groups->len)
        return;

    APArrayT(char *) *query = ap_array_alloc(16, sizeof(char *));

    for (int i = 0; i < w->size.y; i++)
    {
        int absline = i + state->offset;
        if (absline > state->lines->len - 1 || absline < 0)
            break;

        LineDef line = ARR_INDEX(state->lines, LineDef *, absline);
        if (line.data == NULL)
            continue;

        uint64_t *line_signature = &ARR_INDEX(state->lines_sig, uint64_t *, i);
        uint64_t cline_signature = generate_line_signature(w, line, absline);

        if (cline_signature == *line_signature)
            continue;

        *line_signature = cline_signature;

        c = ap_draw_pos(c, VEC(w->pos.x, w->pos.y + i));

        query->len = 0;

#define LIT(str)                                                               \
    &(char *)                                                                  \
    {                                                                          \
        str                                                                    \
    }

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

        ap_array_append_resize(query, LIT("filelist"), 1);

        if (line.type == LINE_ENTRY)
            ap_array_append_resize(query, LIT("entry"), 1);
        else if (line.type == LINE_GROUPNAME)
            ap_array_append_resize(query, LIT("groupname"), 1);
        else if (line.type == LINE_EMPTY)
            ap_array_append_resize(query, LIT("empty"), 1);

        if (absline == state->cursor)
            ap_array_append_resize(query, LIT("hovered"), 1);

        sds temp = sdsempty();
        if (line.type == LINE_EMPTY)
        {
            APColor empty_bg = APCOLOR(255, 0, 0, 255);
            APColor empty_fg = APCOLOR(255, 0, 0, 255);

            ap_array_append_resize(query, LIT("bg"), 1);
            GET_COLOR(query, empty_bg, temp);
            query->len = query->len - 1;

            ap_array_append_resize(query, LIT("fg"), 1);
            GET_COLOR(query, empty_fg, temp);

            c = AP_DRAW_STRC(2, c, line.data, empty_bg, empty_fg);
            continue;
        }

        APColor name_bg = APCOLOR(255, 0, 0, 255);
        APColor name_fg = APCOLOR(255, 0, 0, 255);
        APColor num_bg = APCOLOR(255, 0, 0, 255);
        APColor num_fg = APCOLOR(255, 0, 0, 255);

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

static int find_next_groupname(APWidget *w)
{
    FileListState *state = GET_STATE(w);
    for (int i = state->cursor; i < state->lines->len; i++)
    {
        LineDef line = ARR_INDEX(state->lines, LineDef *, i);
        if (line.type == LINE_GROUPNAME && i != state->cursor)
            return i;
    }
    return state->cursor;
}

static int find_prev_groupname(APWidget *w)
{
    FileListState *state = GET_STATE(w);
    for (int i = state->cursor; i >= 0; i--)
    {
        LineDef line = ARR_INDEX(state->lines, LineDef *, i);
        if (line.type == LINE_GROUPNAME && i != state->cursor)
            return i;
    }
    return state->cursor;
}

static int get_current_group(APWidget *w)
{
    FileListState *state = GET_STATE(w);
    return ARR_INDEX(state->lines, LineDef *, find_prev_groupname(w))
        .group_index;
}

void ap_widget_filelist_on_event(APWidget *w, APEvent e)
{
    FileListState *state = GET_STATE(w);

    if (e.type == AP_EVENT_KEY && e.event.key.keydown)
    {
        char ascii = e.event.key.ascii;
        uint32_t virtual = e.event.key.virtual;
        if (ascii == 'j')
            scroll_cursor(w, 1, false);
        else if (ascii == 'k')
            scroll_cursor(w, -1, false);
        else if (ascii == '}')
            scroll_cursor(w, find_next_groupname(w), true);
        else if (ascii == '{')
            scroll_cursor(w, find_prev_groupname(w), true);
        else if (virtual == VK_RETURN) // TODO: Abstract Windows virtual key
        {
            if (w->listeners)
            {
                APEntryGroup group = ARR_INDEX(
                    state->pl->groups, APEntryGroup *, get_current_group(w));
                int cursor_rel =
                    ARR_INDEX(state->lines, LineDef *, state->cursor).rel_index;
                if (cursor_rel < 0)
                    return;
                APFile file = ARR_INDEX(group.entries, APFile *, cursor_rel);
                char *temp = calloc(
                    strlen(file.directory) + strlen(file.filename) + 10, 1);
                snprintf(temp,
                         strlen(file.directory) + strlen(file.filename) + 10,
                         "%s/%s", file.directory, file.filename);
                ((void (*)(const char *))ap_dict_get(w->listeners,
                                                     "request_music"))(temp);
                free(temp);
            }
        }
    }
}

void ap_widget_filelist_free(APWidget *w)
{
    FileListState *state = GET_STATE(w);
    for (int i = 0; i < state->lines->len; i++)
    {
        LineDef line = ARR_INDEX(state->lines, LineDef *, i);
        sdsfree(line.data);
        line.data = NULL;
    }
    ap_array_free(&state->lines);
    ap_array_free(&state->lines_sig);
    free(state);
    state = NULL;
}

static void generate_lines_template(APWidget *w)
{
    FileListState *state = GET_STATE(w);
    APPlaylist *pl = state->pl;

    int index = 0;
    for (int i = 0; i < pl->groups->len; i++)
    {
        APEntryGroup group = ARR_INDEX(pl->groups, APEntryGroup *, i);

        sds header = sdsempty();
        header = ap_draw_str(header, ESC TCMD_BGFG, -1);
        header = ap_draw_hline(header, w->size.x);
        header = ap_draw_strf(header, "%2d", i + 1);
        header = ap_draw_str(header, ESC TCMD_BGFG, -1);
        header = ap_draw_strf(header, " %s", group.name);
        ap_array_append_resize(
            state->lines, &(LineDef){header, LINE_GROUPNAME, index++, -1, i},
            1);

        int j;
        for (j = 0; j < group.entries->len; j++)
        {
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
            ap_array_append_resize(
                state->lines, &(LineDef){file, LINE_ENTRY, index++, j, i}, 1);
        }

        sds footer = sdsempty();
        footer = ap_draw_str(footer, ESC TCMD_BGFG, -1);
        footer = ap_draw_hline(footer, w->size.x);
        ap_array_append_resize(
            state->lines, &(LineDef){footer, LINE_EMPTY, index++, -1, i}, 1);
    }
}

static uint64_t generate_line_signature(APWidget *w, LineDef line,
                                        int line_index)
{
    FileListState *state = GET_STATE(w);
    int is_hovered = line_index == state->cursor;
    return ap_hash_djb2(line.data) + is_hovered;
}

static void calculate_offset(APWidget *w)
{
    FileListState *state = GET_STATE(w);

    int margin = 5;
    if (state->cursor > (state->offset + w->size.y) - margin)
        state->offset += state->cursor - (state->offset + w->size.y) + margin;
    else if (state->cursor < state->offset + margin)
        state->offset -= state->offset + margin - state->cursor;
    state->offset = MATH_CLAMP(state->offset, 0, state->lines->len - w->size.y);
}

static void scroll_cursor(APWidget *w, int n, bool absolute)
{
    FileListState *state = GET_STATE(w);

    state->cursor = absolute ? n : state->cursor + n;
    state->cursor = MATH_CLAMP(state->cursor, 0, state->lines->len);
    calculate_offset(w);
}

static void scroll_window(APWidget *w, int n, bool absolute)
{
    FileListState *state = GET_STATE(w);

    state->offset = absolute ? n : state->offset + n;
}
