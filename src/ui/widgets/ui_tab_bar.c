#include "ui_internal.h"

#include <stdio.h>

/* Row of view buttons across the top: "[1] PLAYLIST  [2] VISUAL ...". The
 * selected view's button is drawn inverted. Selection itself lives in
 * ui_ctx; this widget only reflects and requests it. */

struct ui_tab_bar
{
    ui_ctx *u;
    tui_node *button[UI_VIEW_COUNT];
};

static void tab_bar_free(tui_node *n, void *ud)
{
    (void)n;
    free(ud);
}

static void on_click(tui_node *n, void *ud)
{
    ui_tab_bar *bar = ud;
    for (int i = 0; i < UI_VIEW_COUNT; i++)
        if (bar->button[i] == n)
            ui_view_select(bar->u, (enum ui_view_id)i);
}

void ui_tab_bar_select(ui_tab_bar *bar, enum ui_view_id id)
{
    const ui_theme *t = &bar->u->theme;
    for (int i = 0; i < UI_VIEW_COUNT; i++)
    {
        if ((enum ui_view_id)i == id)
            tui_button_set_style(bar->button[i], t->primary, t->control_bg,
                                 COLOR_NONE, COLOR_NONE);
        else
            tui_button_set_style(bar->button[i], t->control_bg, t->primary,
                                 COLOR_NONE, COLOR_NONE);
    }
}

ui_tab_bar *ui_tab_bar_new(ui_ctx *u)
{
    ui_tab_bar *bar = calloc(1, sizeof(*bar));
    if (bar == NULL)
        abort();
    bar->u = u;

    tui_ctx *ctx = u->tui;
    TUI_BOX(ctx, .height = TUI_CELLS(1), .justify = TUI_JUSTIFY_CENTER,
            .col_gap = u->opt.tabs.gap, .on_destroy = tab_bar_free,
            .userdata = bar)
    {
        for (int i = 0; i < UI_VIEW_COUNT; i++)
        {
            char label[48];
            snprintf(label, sizeof(label), "[%d] %s", i + 1,
                     ui_view_title((enum ui_view_id)i));
            bar->button[i] =
                tui_button(ctx, label, on_click, bar,
                           &(tui_attr){
                               ._init = 0,
                               .padding = {0, u->opt.tabs.padding, 0,
                                           u->opt.tabs.padding}
            });
            ui_mouse_only(bar->button[i]);
        }
    }
    return bar;
}
