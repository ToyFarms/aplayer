#include "ui_internal.h"

/*
 * Views are the screens behind the tabs. Each one is a function that builds
 * its subtree and returns the root node, which ui_view_select() shows or
 * hides (display:none keeps the tree, state and layout of hidden views, so
 * switching is cheap and nothing is rebuilt).
 *
 * To add a view: a new ui_view_id, a builder below, a row in `views`.
 */

typedef tui_node *(*ui_view_build_fn)(ui_ctx *u);

/* PLAYLIST: track list with the level meters on its right, search bar below. */
static tui_node *build_playlist(ui_ctx *u)
{
    tui_ctx *ctx = u->tui;
    tui_node *root = NULL;

    TUI_BOX(ctx, .grow = 1, .dir = TUI_COLUMN)
    {
        root = tui_cursor(ctx);
        TUI_BOX(ctx, .grow = 1)
        {
            u->playlist = ui_playlist_new(u);
            ui_vu_meter_new(u);
        }
        u->search = ui_search_new(u);
    }
    return root;
}

/*
 * VISUAL: the meters, the cover art next to them, the lyrics in what is left.
 *
 *   [ level ]  [      cover art      ]  [ lyrics ............ ]
 *   [ meters ] [                     ]  [                    ]
 *   [        ] [ title, artist, album]  [                    ]
 *   [        ] [ time [=== seek ===] ]  [                    ]
 *   [        ] [ << [> >>  gain  loud]  [                    ]
 *   [        ] [ spectrum line       ]  [                    ]
 *
 * The middle column has one width, `W`, set from the screen's size; the art,
 * the info, the controls and the spectrum are exactly that wide, and the art is
 * as tall as the rows left over by what hangs under it, so it gets as big as
 * the screen allows. The meters sit at the left edge of the screen with the
 * column right next to them, and the lyrics take whatever width remains, up
 * to the right edge. Narrow screens drop the lyrics first, then the meters;
 * without lyrics the meters and the column are centred.
 */
#define INFO_ROWS     4
#define CONTROL_ROWS  4 /* time + seek bar, buttons, each with a shadow row */
#define SPECTRUM_ROWS 5
#define GAP_ROWS      3 /* above the info, above the controls, above the
                           spectrum */
#define VU_GAP        6 /* columns between the meters and the art */
#define LYRICS_PAD_L  7 /* ... between the column and the lyrics */
#define LYRICS_PAD_R  7 /* ... and between the lyrics and the screen edge */
#define LYRICS_MIN    24 /* columns the lyrics need to be shown */
#define COLUMN_MIN    34
#define COLUMN_MAX    160

typedef struct visual_layout
{
    ui_ctx *u;
    tui_node *page;
    tui_node *center;    /* the middle column */
    tui_node *art;       /* holds the cover art, as tall as the art may be */
    tui_node *column[4]; /* things as wide as the middle column */
    int ncolumn;
    tui_node *vu, *lyrics_box;
    int vu_w; /* the meters' width the last layout went by */
} visual_layout;

static void visual_free(tui_node *n, void *ud)
{
    (void)n;
    free(ud);
}

static void set_width(tui_node *n, int w)
{
    tui_set_size(n, TUI_CELLS(w), tui_get_attr(n).height);
}

static void visual_resize(tui_node *page, tui_size c, void *ud)
{
    (void)page;
    visual_layout *v = ud;

    /* The art is a square of about 2:1 cells, so w columns make w / 2 rows
     * (plus the row under the pile that says which picture is in front). It
     * takes the rows that the info, the controls and the spectrum leave, or
     * fewer when the screen is narrower than that is wide. */
    int below = INFO_ROWS + CONTROL_ROWS + SPECTRUM_ROWS + GAP_ROWS;
    int avail = MATH_MAX(c.h - below, 5); /* rows the art may take */
    int w = MATH_CLAMP((avail - 1) * 2, COLUMN_MIN, COLUMN_MAX);
    w = MATH_MIN(w, MATH_MAX(c.w - 4, 10));
    /* on a short screen the column stays wide enough for the text and the art
     * simply gets smaller inside it */
    int art_rows = MATH_MIN(w / 2 + 1, avail);

    set_width(v->center, w);
    for (int i = 0; i < v->ncolumn; i++)
        set_width(v->column[i], w);
    tui_set_size(v->art, TUI_CELLS(w), TUI_CELLS(art_rows));

    /* what is left of the row once the column is placed: first the meters
     * (they follow the channel count, so ask them), then the lyrics */
    int vu_w = (int)tui_get_attr(v->vu).width.v;
    v->vu_w = vu_w;
    int after_art = c.w - w;
    bool show_vu = after_art - VU_GAP >= vu_w;
    bool show_lyrics =
        show_vu && after_art - VU_GAP - vu_w - LYRICS_PAD_L - LYRICS_PAD_R >= LYRICS_MIN;
    tui_set_visible(v->vu, show_vu);
    tui_set_visible(v->lyrics_box, show_lyrics);

    /* with nothing on the right the pair sits in the middle of the screen */
    tui_attr pa = tui_get_attr(v->page);
    tui_justify want = show_lyrics ? TUI_JUSTIFY_START : TUI_JUSTIFY_CENTER;
    if (pa.justify != want)
    {
        pa.justify = want;
        tui_set_attr(v->page, &pa);
    }
}

/* The meters change width with the channel count of what is playing, which
 * is no resize of the page: lay out again when it happens. */
static void visual_update(tui_node *page, double dt, void *ud)
{
    (void)dt;
    visual_layout *v = ud;
    if ((int)tui_get_attr(v->vu).width.v == v->vu_w)
        return;
    tui_rect r = tui_get_content_rect(page);
    visual_resize(page, (tui_size){r.w, r.h}, v);
}

static tui_node *build_visual(ui_ctx *u)
{
    tui_ctx *ctx = u->tui;
    visual_layout *v = calloc(1, sizeof(*v));
    if (v == NULL)
        abort();
    v->u = u;

    tui_node *root = NULL;
    TUI_BOX(ctx, .grow = 1, .padding = {1, 1, 1, 1},
            .display = TUI_DISPLAY_NONE, .on_resize = visual_resize,
            .on_update = visual_update, .on_destroy = visual_free,
            .userdata = v)
    {
        root = tui_cursor(ctx);
        v->page = root;

        /* left: the level meters, against the edge of the screen */
        v->vu = ui_vu_meter_new(u);
        {
            tui_attr va = tui_get_attr(v->vu);
            va.margin = (tui_edges){0, VU_GAP, 0, 0};
            tui_set_attr(v->vu, &va);
        }

        /* then: the art, then what is playing, from the top down */
        TUI_BOX(ctx, .width = TUI_CELLS(COLUMN_MIN), .shrink = TUI_NO_SHRINK,
                .dir = TUI_COLUMN)
        {
            v->center = tui_cursor(ctx);

            TUI_BOX(ctx, .width = TUI_CELLS(COLUMN_MIN),
                    .height = TUI_CELLS(COLUMN_MIN / 2),
                    .shrink = TUI_NO_SHRINK, .dir = TUI_COLUMN)
            {
                v->art = tui_cursor(ctx);
                u->cover_art = ui_cover_art_new(u);
            }

            TUI_BOX(ctx, .shrink = TUI_NO_SHRINK, .margin = {1, 0, 0, 0})
                v->column[v->ncolumn++] = ui_track_info_new(u);

            TUI_BOX(ctx, .shrink = TUI_NO_SHRINK, .margin = {1, 0, 0, 0})
            {
                u->transport_visual = ui_transport_new(u, true);
                v->column[v->ncolumn++] = ui_transport_root(u->transport_visual);
            }

            TUI_BOX(ctx, .shrink = TUI_NO_SHRINK, .margin = {1, 0, 0, 0})
                v->column[v->ncolumn++] = ui_spectrum_new(u, SPECTRUM_ROWS);
        }

        /* right: the lyrics, in the width that is left, held in from both
         * the art and the edge of the screen */
        TUI_BOX(ctx, .grow = 1, .basis = TUI_CELLS(0),
                .padding = {0, LYRICS_PAD_R, 0, LYRICS_PAD_L})
        {
            v->lyrics_box = tui_cursor(ctx);
            u->lyrics = ui_lyrics_new(u, 0);
        }
    }
    return root;
}

/* METADATA: full-height lyrics. */
static tui_node *build_metadata(ui_ctx *u)
{
    tui_ctx *ctx = u->tui;
    tui_node *root = NULL;

    TUI_BOX(ctx, .grow = 1, .dir = TUI_COLUMN, .display = TUI_DISPLAY_NONE)
    {
        root = tui_cursor(ctx);
        u->lyrics_full = ui_lyrics_new(u, 0);
    }
    return root;
}

static const struct
{
    const char *title;
    ui_view_build_fn build;
} views[UI_VIEW_COUNT] = {
    [UI_VIEW_PLAYLIST] = {"PLAYLIST", build_playlist},
    [UI_VIEW_VISUAL] = {"VISUAL",   build_visual  },
    [UI_VIEW_METADATA] = {"METADATA", build_metadata},
};

const char *ui_view_title(enum ui_view_id id)
{
    return (unsigned)id < UI_VIEW_COUNT ? views[id].title : "UNKNOWN";
}

void ui_views_build(ui_ctx *u)
{
    tui_ctx *ctx = u->tui;

    TUI_BOX(ctx, .grow = 1, .dir = TUI_COLUMN)
    {
        for (int i = 0; i < UI_VIEW_COUNT; i++)
            u->view_root[i] = views[i].build(u);
    }
}

void ui_view_select(ui_ctx *u, enum ui_view_id id)
{
    if ((unsigned)id >= UI_VIEW_COUNT)
        return;

    u->view = id;
    for (int i = 0; i < UI_VIEW_COUNT; i++)
        tui_set_visible(u->view_root[i], (enum ui_view_id)i == id);
    /* the page has its own, narrower controls under the art */
    tui_set_visible(ui_transport_root(u->transport), id != UI_VIEW_VISUAL);
    ui_skin_apply(u); /* also restyles the tab bar */
    ui_tab_bar_select(u->tab_bar, id);
}
