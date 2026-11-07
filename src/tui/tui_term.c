#include "tui_term.h"

#include <stdlib.h>
#include <string.h>

void tui_term_input_init(tui_term_input *in)
{
    memset(in, 0, sizeof(*in));
}

void tui_term_input_free(tui_term_input *in)
{
    free(in->paste);
    in->paste = NULL;
}

void tui_term_input_set_router(tui_term_input *in, tui_router *router)
{
    in->router = router;
}

static uint32_t map_virtual(uint32_t v)
{
    if (v == TERM_KEY_ENTER)     return TUI_KEY_ENTER;
    if (v == TERM_KEY_TAB)       return TUI_KEY_TAB;
    if (v == TERM_KEY_BACKSPACE) return TUI_KEY_BACKSPACE;
    if (v == TERM_KEY_ESC)       return TUI_KEY_ESC;
    if (v == TERM_KEY_DEL)       return TUI_KEY_DELETE;
    if (v == TERM_KEY_INS)       return TUI_KEY_INSERT;
    if (v == TERM_KEY_UP)        return TUI_KEY_UP;
    if (v == TERM_KEY_DOWN)      return TUI_KEY_DOWN;
    if (v == TERM_KEY_LEFT)      return TUI_KEY_LEFT;
    if (v == TERM_KEY_RIGHT)     return TUI_KEY_RIGHT;
    if (v == TERM_KEY_HOME)      return TUI_KEY_HOME;
    if (v == TERM_KEY_END)       return TUI_KEY_END;
    if (v == TERM_KEY_PAGEUP)    return TUI_KEY_PAGEUP;
    if (v == TERM_KEY_PAGEDOWN)  return TUI_KEY_PAGEDOWN;
    for (int i = 1; i <= 12; i++)
        if (v == (uint32_t)TERM_KEY_F(i))
            return TUI_KEY_F1 + (uint32_t)(i - 1);
    return TUI_KEY_NONE;
}

static uint32_t map_mod(uint32_t m)
{
    return ((m & TERM_KMOD_SHIFT) ? TUI_MOD_SHIFT : 0) |
           ((m & TERM_KMOD_CTRL) ? TUI_MOD_CTRL : 0) |
           ((m & TERM_KMOD_ALT) ? TUI_MOD_ALT : 0);
}

static bool g_consumed;

static void emit(tui_term_input *in, tui_ctx *ctx, tui_event *e)
{
    g_consumed |= in->router ? tui_router_dispatch(in->router, e)
                             : tui_dispatch(ctx, e);
}

static void send_key(tui_term_input *in, tui_ctx *ctx, uint32_t key,
                     uint32_t mod)
{
    tui_event e = {.type = TUI_EV_KEY, .key = key, .mod = mod};
    emit(in, ctx, &e);
}

static void send_text(tui_term_input *in, tui_ctx *ctx, const char *s, size_t n,
                      bool paste)
{
    if (n == 0)
        return;
    tui_event e = {.type = TUI_EV_TEXT, .text = s, .text_len = n, .paste = paste};
    emit(in, ctx, &e);
}

static void flush_text(tui_term_input *in, tui_ctx *ctx)
{
    if (in->ntext == 0)
        return;
    int n = in->ntext;
    in->ntext = 0;
    send_text(in, ctx, in->text, (size_t)n, false);
}

void tui_term_flush(tui_term_input *in, tui_ctx *ctx)
{
    flush_text(in, ctx);
    if (in->router)
        tui_router_flush(in->router);
}

static void paste_put(tui_term_input *in, char c)
{
    if (in->npaste + 1 > in->cap_paste)
    {
        in->cap_paste = in->cap_paste ? in->cap_paste * 2 : 1024;
        in->paste = realloc(in->paste, in->cap_paste);
        if (in->paste == NULL)
            abort();
    }
    in->paste[in->npaste++] = c;
}

/* Bytes between the paste markers: keep text, turn Enter/Tab into \n / \t. */
static void on_paste_key(tui_term_input *in, const term_event_key *k)
{
    if (k->virtual == TERM_KEY_ENTER)
        paste_put(in, '\n');
    else if (k->virtual == TERM_KEY_TAB)
        paste_put(in, '\t');
    else if (k->virtual != 0)
        return;
    else if (k->mod & TERM_KMOD_CTRL)
    {
        if (k->ascii == 'j' || k->ascii == 'm')
            paste_put(in, '\n');
        else if (k->ascii == 'i')
            paste_put(in, '\t');
    }
    else if (k->ascii != 0)
        paste_put(in, k->ascii);
}

static void on_key(tui_term_input *in, tui_ctx *ctx, const term_event_key *k)
{
#ifdef _WIN32
    if (!k->keydown)
        return;
#endif
    if (k->virtual == TERM_KEY_PASTE_BEGIN)
    {
        flush_text(in, ctx);
        in->pasting = true;
        in->npaste = 0;
        in->have = in->need = 0;
        return;
    }
    if (k->virtual == TERM_KEY_PASTE_END)
    {
        in->pasting = false;
        if (in->paste)
            send_text(in, ctx, in->paste, in->npaste, true);
        in->npaste = 0;
        return;
    }
    if (in->pasting)
    {
        on_paste_key(in, k);
        return;
    }

    uint32_t mod = map_mod(k->mod);
    unsigned char c = (unsigned char)k->ascii;

    /* bytes of a multi-byte UTF-8 character (what an IME commits) */
    if (k->virtual == 0 && c >= 0x80 && !(mod & TUI_MOD_CTRL))
    {
        if (c >= 0xC0)
        {
            in->have = 0;
            in->need = c >= 0xF8 ? 0 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
        }
        else if (in->need == 0)
            return; /* stray continuation byte */
        if (in->need == 0)
            return;
        in->utf8[in->have++] = c;
        if (in->have == in->need)
        {
            if (in->ntext + 4 > (int)sizeof(in->text))
                flush_text(in, ctx);
            memcpy(in->text + in->ntext, in->utf8, (size_t)in->have);
            in->ntext += in->have;
            in->have = in->need = 0;
        }
        return;
    }
    /* anything else ends a run of committed text and an unfinished sequence */
    in->have = in->need = 0;
    flush_text(in, ctx);

    uint32_t sp = map_virtual(k->virtual);
    if (sp != TUI_KEY_NONE)
    {
        send_key(in, ctx, sp, mod);
        return;
    }
    if (c == 0)
        return;
    if (c == 127 || c == 8)
    {
        send_key(in, ctx, TUI_KEY_BACKSPACE, mod);
        return;
    }
    if (mod & TUI_MOD_CTRL)
    {
        send_key(in, ctx, c, mod);
        return;
    }
    if (c >= 0x20)
        send_key(in, ctx, c, mod & ~(uint32_t)TUI_MOD_SHIFT);
}

static void on_mouse(tui_term_input *in, tui_ctx *ctx, const term_event_mouse *m)
{
    flush_text(in, ctx);
    uint32_t mod = map_mod(m->mod);
    tui_event e = {.x = m->x, .y = m->y, .mod = mod};

    if (m->x != in->mx || m->y != in->my || m->moved)
    {
        e.type = TUI_EV_MOUSE_MOVE;
        emit(in, ctx, &e);
        in->mx = m->x;
        in->my = m->y;
    }
    for (int b = 0; b < 3; b++)
    {
        if (m->state[b] == in->btn[b])
            continue;
        in->btn[b] = m->state[b];
        e.type = m->state[b] ? TUI_EV_MOUSE_DOWN : TUI_EV_MOUSE_UP;
        e.button = b;
        emit(in, ctx, &e);
    }
    if (m->scrolled && m->scroll_delta)
    {
        e.type = TUI_EV_SCROLL;
        e.button = 0;
        e.scroll_delta = m->scroll_delta;
        emit(in, ctx, &e);
    }
}

bool tui_term_dispatch(tui_term_input *in, tui_ctx *ctx, const term_event *ev)
{
    g_consumed = false;
    switch (ev->type)
    {
    case TERM_EVENT_KEY:    on_key(in, ctx, &ev->key); break;
    case TERM_EVENT_MOUSE:  on_mouse(in, ctx, &ev->mouse); break;
    case TERM_EVENT_RESIZE: tui_resize(ctx, ev->resize.width, ev->resize.height); break;
    default: break;
    }
    return g_consumed;
}

void tui_term_pump(tui_term_input *in, tui_ctx *ctx, queue_t *scratch)
{
    term_get_events(scratch);
    term_event *ev;
    while ((ev = queue_pop(scratch)) != NULL)
    {
        tui_term_dispatch(in, ctx, ev);
        free(ev);
    }
    tui_term_flush(in, ctx);
}
