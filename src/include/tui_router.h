#ifndef __TUI_ROUTER_H
#define __TUI_ROUTER_H

/*
 * tui_router - input routing on top of tui.h: vim-style keymaps, focus layers
 * and input-method (IME) friendly text handling.  See docs/INPUT.md.
 *
 *   keys      "gg", "<C-w>j", "<Space>f", "<S-Tab>", "<A-1>"  -> tui_keyspec[]
 *   keymap    modes (normal / insert / ...), each a trie of key sequences.
 *             Multi-key bindings, `5j` counts, ambiguity timeouts ('g' and 'gg'
 *             both bound), unmatched prefixes are replayed to the widgets
 *             (so an insert-mode `jk` map still lets you type "jx").
 *   router    tui_router: one per tui_ctx.  Keymaps attach to nodes (a node's
 *             keymap runs before the node's own on_event, then bubbling
 *             continues to the parent) or globally (last).  Focus *layers*
 *             push focus + a keymap, optionally modally, and restore the
 *             previous focus when popped.
 *   langmap   vim's 'langmap': keys typed on a non-Latin layout (or as
 *             full-width forms by an IME) are mapped to the Latin key they
 *             stand for, for keymap lookup only; text fields get the real text.
 *
 * Events: KEY = a key press (ASCII, special keys, modifiers).  TEXT = text
 * produced by an input method (non-ASCII commits) or a bracketed paste.  TEXT
 * is offered to the focused widget chain first; if nobody takes it, it is
 * replayed as KEY presses so keymaps still work (through the langmap).
 */

#include "tui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* key specs                                                                */
/* ------------------------------------------------------------------------ */

typedef struct tui_keyspec
{
    uint32_t key; /* codepoint or enum tui_key */
    uint32_t mod; /* TUI_MOD_*; SHIFT only matters for special keys */
} tui_keyspec;

#define TUI_KEYSEQ_MAX 16

/* Parses vim notation. Returns the number of keys, or -1 on a syntax error.
 * <leader> expands to '\\' here (keymaps use their own leader). */
int tui_keys_parse(const char *s, tui_keyspec *out, int cap);
int tui_keys_format(const tui_keyspec *k, int n, char *buf, size_t cap);

/* ------------------------------------------------------------------------ */
/* keymaps                                                                  */
/* ------------------------------------------------------------------------ */

typedef struct tui_router tui_router;
typedef struct tui_keymap tui_keymap;

typedef struct tui_keyctx
{
    tui_router *input;
    tui_node *node;       /* the node the keymap is attached to (NULL: global) */
    int mode;
    bool has_count;       /* a numeric prefix was typed */
    int count;            /* ... its value (0 without one; use max(count, 1)) */
    const tui_keyspec *keys; /* the sequence that matched */
    int nkeys;
    const tui_event *event;  /* the last key event */
} tui_keyctx;

/* Return false to let the key keep bubbling (it counts as unhandled). */
typedef bool (*tui_key_fn)(const tui_keyctx *kc, void *ud);

enum
{
    TUI_MODE_TEXT = 1 << 0,  /* typing mode: no langmap / full-width folding */
    TUI_MODE_COUNT = 1 << 1, /* digits build a count (`12j`) unless bound */
};

typedef struct tui_keybinding
{
    const char *keys;
    tui_key_fn fn;
    void *ud;
} tui_keybinding;

tui_keymap *tui_keymap_new(void);
void tui_keymap_free(tui_keymap *km);
/* Finds or creates a mode (and sets its flags). Returns its id. */
int tui_keymap_mode(tui_keymap *km, const char *name, unsigned flags);
bool tui_keymap_bind(tui_keymap *km, int mode, const char *keys, tui_key_fn fn,
                     void *ud);
/* Binds a table; returns how many entries parsed. */
int tui_keymap_bind_all(tui_keymap *km, int mode, const tui_keybinding *t,
                        size_t n);
bool tui_keymap_unbind(tui_keymap *km, int mode, const char *keys);
void tui_keymap_set_leader(tui_keymap *km, const char *key);

/* ------------------------------------------------------------------------ */
/* langmap                                                                  */
/* ------------------------------------------------------------------------ */

typedef struct tui_langmap tui_langmap;

bool tui_langmap_add(tui_langmap *lm, uint32_t from, uint32_t to);
/* Pairs the code points of `from` with those of `to`, in order. */
void tui_langmap_add_strings(tui_langmap *lm, const char *from, const char *to);
/* Built-ins: "ru", "uk", "el" (layouts), "cjk" (IME punctuation). */
bool tui_langmap_preset(tui_langmap *lm, const char *name);
/* What a key stands for: langmap entry, else full-width ASCII -> ASCII. */
uint32_t tui_langmap_apply(const tui_langmap *lm, uint32_t cp);

/* ------------------------------------------------------------------------ */
/* router                                                                   */
/* ------------------------------------------------------------------------ */

typedef struct tui_router_options
{
    int timeout_ms; /* ambiguous / incomplete sequence timeout, 0 = 500 */
} tui_router_options;

/* Installs itself as the context's key hook. */
tui_router *tui_router_new(tui_ctx *ctx, const tui_router_options *opt);
void tui_router_free(tui_router *in);

/* Keymap consulted last (after the focus chain, unless a modal layer is up). */
void tui_router_set_global(tui_router *in, tui_keymap *km, int mode);
/* Keymap consulted when a key bubbles through `node`, before its on_event. */
void tui_router_attach(tui_router *in, tui_node *node, tui_keymap *km, int mode);
void tui_router_detach(tui_router *in, tui_node *node);
/* Current mode of a scope (node == NULL: the global one). */
void tui_router_set_mode(tui_router *in, tui_node *node, int mode);
int tui_router_mode(const tui_router *in, tui_node *node);

tui_langmap *tui_router_langmap(tui_router *in);

/* Focus layers: focus moves to `focus`, `keymap` is attached to it, and with
 * `modal` nothing below the layer sees keys and focus cannot leave it. */
typedef struct tui_layer_desc
{
    tui_node *focus;
    tui_keymap *keymap; /* may be NULL */
    int mode;
    bool modal;
    void (*on_close)(void *ud); /* called after the layer is popped */
    void *ud;
} tui_layer_desc;

void tui_router_push_layer(tui_router *in, const tui_layer_desc *d);
/* Pops the top layer; focus returns to what it was (if it still can). */
void tui_router_pop_layer(tui_router *in);
int tui_router_layer_depth(const tui_router *in);

/* Feed events (KEY, TEXT, mouse, resize). Esc is held until tui_router_flush()
 * so that Esc+key arriving together reads as Alt+key. */
bool tui_router_dispatch(tui_router *in, tui_event *ev);
/* End of an input batch: releases a held Esc. */
void tui_router_flush(tui_router *in);
/* Advances the sequence timeout clock. */
void tui_router_tick(tui_router *in, double dt_seconds);
/* Keys typed so far in an unfinished sequence, like vim's showcmd ("2g"). */
const char *tui_router_pending(const tui_router *in);

#ifdef __cplusplus
}
#endif

#endif /* __TUI_ROUTER_H */
