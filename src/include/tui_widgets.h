#ifndef __TUI_WIDGETS_H
#define __TUI_WIDGETS_H

/*
 * tui_widgets - HTML-like elements on top of tui.h.
 *
 * Primitives (tui.h):     TUI_BOX  == <div>        TUI_TEXT == <span>
 * Elements (this file):   hr / vr / spacer, button, input, checkbox,
 *                         progress, slider, scroll
 *
 * Every element here is an ordinary node: a box or text node plus callbacks
 * (paint, on_event, on_update, ...) and a small state struct released from
 * on_destroy.  Nothing in tui_widgets.c touches library internals, so it is
 * also the reference for writing your own widgets (see docs/TUI.md).
 *
 * The attrs you pass are applied on top of each widget's defaults.  The
 * callback fields (`on_event`, `on_update`, `userdata`, `paint`, `on_destroy`)
 * belong to the widget; use the constructor's own callbacks instead.
 */

#include "tui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* theme                                                                    */
/* ------------------------------------------------------------------------ */

typedef struct tui_theme
{
    color_t fg;             /* normal text */
    color_t fg_muted;       /* placeholders, disabled, scrollbar track */
    color_t surface;        /* buttons, inputs */
    color_t surface_hover;
    color_t surface_active;
    color_t accent;         /* focus, filled progress, thumbs */
    color_t accent_fg;      /* text on top of accent */
    color_t track;          /* empty part of progress / slider */
    color_t border;         /* hr, vr, scrollbar */
} tui_theme;

/* Widgets read the theme when they are created and every time they repaint,
 * so after tui_theme_set() call tui_force_redraw() (and re-style by hand any
 * widget whose colours you set through attrs). */
const tui_theme *tui_theme_get(void);
void tui_theme_set(const tui_theme *theme);

/* ------------------------------------------------------------------------ */
/* HTML-style aliases                                                       */
/* ------------------------------------------------------------------------ */

#define TUI_DIV  TUI_BOX
#define TUI_SPAN TUI_TEXT

/* ------------------------------------------------------------------------ */
/* separators / spacing                                                     */
/* ------------------------------------------------------------------------ */

tui_node *tui_hr(tui_ctx *ctx, const tui_attr *attr);     /* <hr>: ─────  */
tui_node *tui_vr(tui_ctx *ctx, const tui_attr *attr);     /* vertical │   */
tui_node *tui_spacer(tui_ctx *ctx, const tui_attr *attr); /* grow = 1     */

#define TUI_HR(ctx, ...)     tui_hr((ctx), &(tui_attr){._init = 0, __VA_ARGS__})
#define TUI_VR(ctx, ...)     tui_vr((ctx), &(tui_attr){._init = 0, __VA_ARGS__})
#define TUI_SPACER(ctx, ...) tui_spacer((ctx), &(tui_attr){._init = 0, __VA_ARGS__})

/* ------------------------------------------------------------------------ */
/* button                                                                   */
/* ------------------------------------------------------------------------ */

typedef void (*tui_click_fn)(tui_node *self, void *ud);

/* Focusable; hover / pressed / focus styling is automatic.  Fires on mouse
 * release inside the button, or Enter / Space while focused. */
tui_node *tui_button(tui_ctx *ctx, const char *label, tui_click_fn on_click,
                     void *ud, const tui_attr *attr);
void tui_button_set_label(tui_node *button, const char *label);
/* Colours for the idle and hovered/pressed states; a colour with a == 0
 * (COLOR_NONE) falls back to the theme-derived default. */
void tui_button_set_style(tui_node *button, color_t bg, color_t fg,
                          color_t hover_bg, color_t hover_fg);

#define TUI_BUTTON(ctx, label, on_click, ud, ...)                              \
    tui_button((ctx), (label), (on_click), (ud),                               \
               &(tui_attr){._init = 0, __VA_ARGS__})

/* ------------------------------------------------------------------------ */
/* text input (single line)                                                 */
/* ------------------------------------------------------------------------ */

typedef void (*tui_text_fn)(tui_node *self, const char *text, void *ud);

/* Editing: printable keys, Backspace/Delete, Left/Right (+Ctrl: by word),
 * Home/End, Ctrl-A/E/U/K/W/D, click to place the cursor, Enter = submit. */
tui_node *tui_input(tui_ctx *ctx, const char *initial, const tui_attr *attr);
void tui_input_on_change(tui_node *input, tui_text_fn fn, void *ud);
void tui_input_on_submit(tui_node *input, tui_text_fn fn, void *ud);
const char *tui_input_text(const tui_node *input);
void tui_input_set_text(tui_node *input, const char *utf8); /* no callback */
void tui_input_set_placeholder(tui_node *input, const char *utf8);
void tui_input_set_password(tui_node *input, bool password);

#define TUI_INPUT(ctx, initial, ...)                                           \
    tui_input((ctx), (initial), &(tui_attr){._init = 0, __VA_ARGS__})

/* ------------------------------------------------------------------------ */
/* checkbox                                                                 */
/* ------------------------------------------------------------------------ */

typedef void (*tui_toggle_fn)(tui_node *self, bool checked, void *ud);

tui_node *tui_checkbox(tui_ctx *ctx, const char *label, bool checked,
                       tui_toggle_fn on_change, void *ud, const tui_attr *attr);
bool tui_checkbox_checked(const tui_node *cb);
void tui_checkbox_set(tui_node *cb, bool checked); /* no callback */

#define TUI_CHECKBOX(ctx, label, checked, on_change, ud, ...)                  \
    tui_checkbox((ctx), (label), (checked), (on_change), (ud),                 \
                 &(tui_attr){._init = 0, __VA_ARGS__})

/* ------------------------------------------------------------------------ */
/* progress / slider                                                        */
/* ------------------------------------------------------------------------ */

typedef void (*tui_value_fn)(tui_node *self, float value, void *ud);

/* Display-only bar, value in [0, 1], 1/8-cell resolution.  Updating it only
 * repaints the cells between the old and the new fill edge. */
tui_node *tui_progress(tui_ctx *ctx, float value, const tui_attr *attr);
void tui_progress_set(tui_node *bar, float value);
float tui_progress_get(const tui_node *bar);
/* Override the theme colours of a progress bar / slider (COLOR_NONE = theme). */
void tui_progress_set_colors(tui_node *bar, color_t fill, color_t track);

/* Interactive bar: click / drag, arrow keys (+/-5%), Home/End. */
tui_node *tui_slider(tui_ctx *ctx, float value, tui_value_fn on_change, void *ud,
                     const tui_attr *attr);
#define tui_slider_set  tui_progress_set /* no callback */
#define tui_slider_get  tui_progress_get

#define TUI_PROGRESS(ctx, value, ...)                                          \
    tui_progress((ctx), (value), &(tui_attr){._init = 0, __VA_ARGS__})
#define TUI_SLIDER(ctx, value, on_change, ud, ...)                             \
    tui_slider((ctx), (value), (on_change), (ud),                              \
               &(tui_attr){._init = 0, __VA_ARGS__})

/* ------------------------------------------------------------------------ */
/* scroll container                                                         */
/* ------------------------------------------------------------------------ */

/* A <div> whose content may overflow: children never shrink, mouse wheel and
 * (when focused) Up/Down/PageUp/PageDown/Home/End scroll it, and a scrollbar
 * is drawn over the right edge of its padding box.  Use it as a scope:
 *
 *   TUI_SCROLL(ctx, .dir = TUI_COLUMN, .grow = 1) { ... rows ... }
 *
 * tui_scroll_into_view(node) (tui.h) scrolls a child into sight. */
tui_node *tui_scroll_begin(tui_ctx *ctx, const tui_attr *attr);

#define TUI_SCROLL(ctx, ...)                                                   \
    for (int TUI__CAT(_tui_i_, __LINE__) =                                     \
             (tui_scroll_begin((ctx), &(tui_attr){._init = 0, __VA_ARGS__}),   \
              0);                                                              \
         TUI__CAT(_tui_i_, __LINE__) == 0;                                     \
         TUI__CAT(_tui_i_, __LINE__) = (tui__close((ctx)), 1))

#ifdef __cplusplus
}
#endif

#endif /* __TUI_WIDGETS_H */
