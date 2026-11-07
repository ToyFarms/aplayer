#ifndef __TUI_TERM_H
#define __TUI_TERM_H

/* Adapter: aplayer's term_event stream -> tui events.
 *
 *  - ASCII / special keys become TUI_EV_KEY.
 *  - Non-ASCII UTF-8 (what an IME commits) is reassembled from the byte stream
 *    and delivered as TUI_EV_TEXT; consecutive characters form one event.
 *  - Bracketed paste becomes one TUI_EV_TEXT with .paste set.
 *
 * With a router attached (tui_router.h) events go through it, otherwise
 * straight to tui_dispatch(). */

#include "term.h"
#include "tui.h"
#include "tui_router.h"

typedef struct tui_term_input
{
    tui_router *router;      /* optional */
    unsigned char utf8[4];  /* partial UTF-8 sequence (chars arrive bytewise) */
    int have, need;
    char text[512];         /* committed non-ASCII run, not yet delivered */
    int ntext;
    bool pasting;
    char *paste;            /* bracketed paste being collected */
    size_t npaste, cap_paste;
    bool btn[3];
    int mx, my;
} tui_term_input;

void tui_term_input_init(tui_term_input *in);
void tui_term_input_free(tui_term_input *in);
void tui_term_input_set_router(tui_term_input *in, tui_router *router);

/* Translate and dispatch one terminal event (resize included).
 * Returns true if some handler consumed it (not thread-safe). */
bool tui_term_dispatch(tui_term_input *in, tui_ctx *ctx, const term_event *ev);

/* Deliver text collected so far and release a held Esc. Call at the end of a
 * batch of events (once per frame is enough). */
void tui_term_flush(tui_term_input *in, tui_ctx *ctx);

/* term_get_events() + dispatch everything queued + flush. */
void tui_term_pump(tui_term_input *in, tui_ctx *ctx, queue_t *scratch);

#endif
