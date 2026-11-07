#ifndef __UI_H
#define __UI_H

/*
 * ui - the aplayer terminal interface.
 *
 * Public surface of the UI module (src/ui). The interface is built on the
 * retained-mode toolkit in tui.h; everything behind this header is private to
 * src/ui, so the rest of the program only ever sees an opaque handle.
 *
 * The UI is a pure consumer of application state: it reads playback, playlist
 * and meter data from the app_instance it is given and drives playback through
 * the same public functions the rest of the program uses. It owns all of its
 * own state (cursor, selected view, animations); nothing outside src/ui writes
 * into it.
 *
 * Typical use (see main.c):
 *
 *   ui_ctx *ui = ui_create(app);
 *   while (!ui_quit_requested(ui)) {
 *       ...for each terminal event: ui_event(ui, ev);
 *       ui_frame(ui);
 *   }
 *   ui_destroy(ui);
 */

#include "app.h"
#include "term.h"

#include <stdbool.h>

typedef struct ui_ctx ui_ctx;

/* Takes over the terminal (alternate screen, mouse) and builds the interface.
 * Returns NULL on failure. */
ui_ctx *ui_create(app_instance *app);

/* Restores the terminal and releases everything created by ui_create(). */
void ui_destroy(ui_ctx *ui);

/* Feeds one terminal event: widget interaction first, global hotkeys for
 * whatever no widget consumed. */
void ui_event(ui_ctx *ui, const term_event *e);

/* Advances animations, syncs widgets with application state and renders the
 * damaged region. Call once per loop iteration. */
void ui_frame(ui_ctx *ui);

bool ui_quit_requested(const ui_ctx *ui);

#endif /* __UI_H */
