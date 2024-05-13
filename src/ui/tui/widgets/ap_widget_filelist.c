#include "ap_widgets.h"

typedef struct FileListState
{
    int offset;
    int *cache;
    int cache_size;
    APPlaylist *pl;
} FileListState;

void ap_widget_filelist_init(APWidget *w, APPlaylist *pl)
{
    w->state.tui->internal = calloc(1, sizeof(FileListState));
    FileListState *state = w->state.tui->internal;

    state->cache_size = w->size.y;
    state->cache = malloc(state->cache_size * sizeof(*state->cache));
    memset(state->cache, 0xDEADBEEF, state->cache_size);
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

static inline int __fixsubtract(int a, int b)
{
    return a - b;
}

void ap_widget_filelist_draw(APWidget *w)
{
    FileListState *state = w->state.tui->internal;
    APPlaylist *pl = state->pl;
    sds c = w->state.tui->draw_cmd;

    if (!pl || !pl->groups->len)
        return;

    if (state->cache_size != w->size.y)
    {
        state->cache = realloc(state->cache, w->size.y);
        state->cache_size = w->size.y;
    }

    T(sds) APArray buf;
    ap_array_init(&buf, w->size.y, sizeof(sds));

    int current_row = 0;
    for (int i = 0; i < pl->groups->len; i++)
    {
        if (current_row > w->size.y + state->offset)
            continue;

        sds header = sdsempty();

        APEntryGroup group = ARR_INDEX(pl->groups, APEntryGroup *, i);
        // TODO: Add colorscheme (dict) for each widget

        header = ap_draw_color(header, APCOLOR(0, 0, 0, 255),
                               APCOLOR(152, 195, 121, 255));
        header = ap_draw_hline(header, w->size.x);
        header = ap_draw_strf(header, "%2d", i + 1);
        header = ap_draw_strf(header, " %s", group.name);
        header = ap_draw_color(header, APCOLOR(255, 255, 255, 255),
                               APCOLOR(0, 0, 0, 255));
        ap_array_append_resize(&buf, &header, 1);
        current_row++;

        for (int j = 0; j < group.entries->len; j++)
        {
            if (current_row > w->size.y + state->offset)
                continue;

            sds file = sdsempty();
            APFile entry = ARR_INDEX(group.entries, APFile *, j);

            file = ap_draw_color(file, w->fg, w->bg);

            int num_len = sdslen(file);
            file = ap_draw_strf(file, "  %3d ", j);
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
                    __fixsubtract(w->size.x, strlen(entry.filename)) - num_len);

                // caused (null) to the whole row
                // int a = w->size.x - strlen(entry.filename);
                /* NOTE: subtracting "w->size.x - strlen(entry.filename)" inline
                    would caused the row to be (null). I dont know why, even
                    though the operation didn't involve any other variable at
                    all, this issues only appear in Debug mode on Windows
                    Terminal; testing with mingw terminal, only the first frame
                    are (null) but its fine after
                 */
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

            ap_array_append_resize(&buf, &file, 1);
            current_row++;
        }

        sds footer = sdsempty();
        footer = ap_draw_color(footer, APCOLOR_ZERO, w->bg);
        footer = ap_draw_hline(footer, w->size.x);
        ap_array_append_resize(&buf, &footer, 1);
        current_row++;
    }

    for (int i = 0; i < w->size.y; i++)
    {
        if (i + state->offset > buf.len - 1 || i + state->offset < 0)
            break;

        sds row = ARR_INDEX(&buf, sds *, i + state->offset);
        c = ap_draw_pos(c, VEC(w->pos.x, w->pos.y + i));
        c = ap_draw_strf(c, "%s", row);
    }

    for (int i = 0; i < buf.len; i++)
        sdsfree(ARR_INDEX(&buf, sds *, i));

    w->state.tui->draw_cmd = c;
}

void ap_widget_filelist_on_event(APWidget *w, APEvent e)
{
    FileListState *state = w->state.tui->internal;

    if (e.type == AP_EVENT_KEY && e.event.key.keydown)
    {
        if (e.event.key.ascii == 'j')
            state->offset += 1;
        else if (e.event.key.ascii == 'k')
            state->offset = MATH_MAX(0, state->offset - 1);
    }
}
