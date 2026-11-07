#include "tui_router.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Layout: key specs | keymap trie | langmap | router.
 * The router never touches toolkit internals; it uses tui.h only.
 */

/* ------------------------------------------------------------------------ */
/* key specs                                                                */
/* ------------------------------------------------------------------------ */

static const struct
{
    const char *name;
    uint32_t key;
} KEY_NAMES[] = {
    {"Esc", TUI_KEY_ESC},         {"CR", TUI_KEY_ENTER},
    {"Enter", TUI_KEY_ENTER},     {"Return", TUI_KEY_ENTER},
    {"Tab", TUI_KEY_TAB},         {"BS", TUI_KEY_BACKSPACE},
    {"Backspace", TUI_KEY_BACKSPACE}, {"Del", TUI_KEY_DELETE},
    {"Delete", TUI_KEY_DELETE},   {"Insert", TUI_KEY_INSERT},
    {"Ins", TUI_KEY_INSERT},      {"Up", TUI_KEY_UP},
    {"Down", TUI_KEY_DOWN},       {"Left", TUI_KEY_LEFT},
    {"Right", TUI_KEY_RIGHT},     {"Home", TUI_KEY_HOME},
    {"End", TUI_KEY_END},         {"PageUp", TUI_KEY_PAGEUP},
    {"PgUp", TUI_KEY_PAGEUP},     {"PageDown", TUI_KEY_PAGEDOWN},
    {"PgDn", TUI_KEY_PAGEDOWN},   {"Space", ' '},
    {"lt", '<'},                  {"Bar", '|'},
    {"Bslash", '\\'},
};
#define NKEY_NAMES (sizeof(KEY_NAMES) / sizeof(KEY_NAMES[0]))

static bool name_eq(const char *a, size_t alen, const char *b)
{
    if (strlen(b) != alen)
        return false;
    for (size_t i = 0; i < alen; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return false;
    return true;
}

static int parse_special(const char *s, size_t len, uint32_t *key)
{
    for (size_t i = 0; i < NKEY_NAMES; i++)
        if (name_eq(s, len, KEY_NAMES[i].name))
        {
            *key = KEY_NAMES[i].key;
            return 0;
        }
    if (len >= 2 && (s[0] == 'F' || s[0] == 'f'))
    {
        int n = 0;
        for (size_t i = 1; i < len; i++)
        {
            if (!isdigit((unsigned char)s[i]))
                return -1;
            n = n * 10 + (s[i] - '0');
        }
        if (n >= 1 && n <= 12)
        {
            *key = TUI_KEY_F1 + (uint32_t)(n - 1);
            return 0;
        }
    }
    return -1;
}

static int parse_keys(const char *s, tui_keyspec *out, int cap,
                      tui_keyspec leader)
{
    int n = 0;
    while (*s)
    {
        if (n >= cap)
            return -1;
        tui_keyspec k = {0, 0};
        if (*s == '<')
        {
            const char *end = strchr(s, '>');
            if (end == NULL || end == s + 1)
                return -1;
            const char *p = s + 1;
            size_t len = (size_t)(end - p);
            while (len > 2 && p[1] == '-' && strchr("CSAMcsam", p[0]))
            {
                switch (tolower((unsigned char)p[0]))
                {
                case 'c': k.mod |= TUI_MOD_CTRL; break;
                case 's': k.mod |= TUI_MOD_SHIFT; break;
                default:  k.mod |= TUI_MOD_ALT; break;
                }
                p += 2;
                len -= 2;
            }
            uint32_t cp;
            size_t used = tui_utf8_decode(p, len, &cp);
            if (used == len && used > 0)
            {
                k.key = cp;
                if (cp < 0x80 && isalpha((int)cp))
                {
                    if (k.mod & TUI_MOD_CTRL)
                        k.key = (uint32_t)tolower((int)cp);
                    else if (k.mod & TUI_MOD_SHIFT)
                        k.key = (uint32_t)toupper((int)cp);
                }
                if (k.key >= 0x20 && k.key < 0x110000 && !(k.mod & TUI_MOD_CTRL))
                    k.mod &= ~(uint32_t)TUI_MOD_SHIFT;
            }
            else if (name_eq(p, len, "leader"))
                k = leader, k.mod |= 0;
            else if (parse_special(p, len, &k.key) == 0)
            {
                if (k.key < 0x110000) /* Space, lt, ...: printable */
                    k.mod &= ~(uint32_t)TUI_MOD_SHIFT;
            }
            else
                return -1;
            s = end + 1;
        }
        else
        {
            uint32_t cp;
            size_t used = tui_utf8_decode(s, strlen(s), &cp);
            if (used == 0)
                return -1;
            k.key = cp;
            s += used;
        }
        out[n++] = k;
    }
    return n;
}

int tui_keys_parse(const char *s, tui_keyspec *out, int cap)
{
    return parse_keys(s, out, cap, (tui_keyspec){'\\', 0});
}

static void fmt_put(char *buf, size_t cap, size_t *len, const char *s)
{
    size_t n = strlen(s);
    for (size_t i = 0; i < n; i++)
    {
        if (*len + 1 < cap)
            buf[*len] = s[i];
        (*len)++;
    }
}

int tui_keys_format(const tui_keyspec *k, int n, char *buf, size_t cap)
{
    size_t len = 0;
    for (int i = 0; i < n; i++)
    {
        const char *name = NULL;
        char tmp[16];
        uint32_t key = k[i].key;
        if (key >= TUI_KEY_F1 && key <= TUI_KEY_F12)
        {
            snprintf(tmp, sizeof(tmp), "F%u", key - TUI_KEY_F1 + 1);
            name = tmp;
        }
        for (size_t j = 0; !name && j < NKEY_NAMES; j++)
            if (KEY_NAMES[j].key == key)
                name = KEY_NAMES[j].name;
        bool plain = name == NULL && k[i].mod == 0 && key != '<';
        if (plain)
        {
            char enc[5] = {0};
            tui_utf8_encode(key, enc);
            fmt_put(buf, cap, &len, enc);
            continue;
        }
        fmt_put(buf, cap, &len, "<");
        if (k[i].mod & TUI_MOD_CTRL) fmt_put(buf, cap, &len, "C-");
        if (k[i].mod & TUI_MOD_ALT)  fmt_put(buf, cap, &len, "A-");
        if (k[i].mod & TUI_MOD_SHIFT) fmt_put(buf, cap, &len, "S-");
        if (name == NULL)
        {
            char enc[5] = {0};
            tui_utf8_encode(key == '<' ? 0 : key, enc);
            name = key == '<' ? "lt" : enc;
            fmt_put(buf, cap, &len, name);
        }
        else
            fmt_put(buf, cap, &len, name);
        fmt_put(buf, cap, &len, ">");
    }
    if (cap)
        buf[len < cap ? len : cap - 1] = '\0';
    return (int)len;
}

/* ------------------------------------------------------------------------ */
/* keymap                                                                   */
/* ------------------------------------------------------------------------ */

typedef struct knode
{
    tui_keyspec k;
    struct knode *child, *next;
    tui_key_fn fn;
    void *ud;
} knode;

typedef struct kmode
{
    char *name;
    unsigned flags;
    knode root;
} kmode;

struct tui_keymap
{
    kmode *modes;
    int n;
    tui_keyspec leader;
};

static bool key_eq(tui_keyspec a, tui_keyspec b)
{
    return a.key == b.key && a.mod == b.mod;
}

static void knode_free(knode *n)
{
    for (knode *c = n->child; c;)
    {
        knode *next = c->next;
        knode_free(c);
        free(c);
        c = next;
    }
}

tui_keymap *tui_keymap_new(void)
{
    tui_keymap *km = calloc(1, sizeof(*km));
    if (km == NULL)
        abort();
    km->leader = (tui_keyspec){'\\', 0};
    return km;
}

void tui_keymap_free(tui_keymap *km)
{
    if (km == NULL)
        return;
    for (int i = 0; i < km->n; i++)
    {
        knode_free(&km->modes[i].root);
        free(km->modes[i].name);
    }
    free(km->modes);
    free(km);
}

int tui_keymap_mode(tui_keymap *km, const char *name, unsigned flags)
{
    for (int i = 0; i < km->n; i++)
        if (strcmp(km->modes[i].name, name) == 0)
        {
            km->modes[i].flags = flags;
            return i;
        }
    km->modes = realloc(km->modes, (size_t)(km->n + 1) * sizeof(*km->modes));
    if (km->modes == NULL)
        abort();
    kmode *m = &km->modes[km->n];
    memset(m, 0, sizeof(*m));
    m->name = strdup(name);
    m->flags = flags;
    return km->n++;
}

void tui_keymap_set_leader(tui_keymap *km, const char *key)
{
    tui_keyspec k;
    if (tui_keys_parse(key, &k, 1) == 1)
        km->leader = k;
}

static knode *child_of(const knode *n, tui_keyspec k)
{
    for (knode *c = n->child; c; c = c->next)
        if (key_eq(c->k, k))
            return c;
    return NULL;
}

bool tui_keymap_bind(tui_keymap *km, int mode, const char *keys, tui_key_fn fn,
                     void *ud)
{
    tui_keyspec seq[TUI_KEYSEQ_MAX];
    int n = parse_keys(keys, seq, TUI_KEYSEQ_MAX, km->leader);
    if (n <= 0 || mode < 0 || mode >= km->n)
        return false;
    knode *cur = &km->modes[mode].root;
    for (int i = 0; i < n; i++)
    {
        knode *c = child_of(cur, seq[i]);
        if (c == NULL)
        {
            c = calloc(1, sizeof(*c));
            if (c == NULL)
                abort();
            c->k = seq[i];
            c->next = cur->child;
            cur->child = c;
        }
        cur = c;
    }
    cur->fn = fn;
    cur->ud = ud;
    return true;
}

int tui_keymap_bind_all(tui_keymap *km, int mode, const tui_keybinding *t,
                        size_t n)
{
    int ok = 0;
    for (size_t i = 0; i < n; i++)
        ok += tui_keymap_bind(km, mode, t[i].keys, t[i].fn, t[i].ud);
    return ok;
}

bool tui_keymap_unbind(tui_keymap *km, int mode, const char *keys)
{
    tui_keyspec seq[TUI_KEYSEQ_MAX];
    int n = parse_keys(keys, seq, TUI_KEYSEQ_MAX, km->leader);
    if (n <= 0 || mode < 0 || mode >= km->n)
        return false;
    knode *cur = &km->modes[mode].root;
    for (int i = 0; i < n && cur; i++)
        cur = child_of(cur, seq[i]);
    if (cur == NULL || cur->fn == NULL)
        return false;
    cur->fn = NULL;
    return true;
}

static bool live(const knode *n)
{
    if (n->fn)
        return true;
    for (const knode *c = n->child; c; c = c->next)
        if (live(c))
            return true;
    return false;
}

enum match
{
    M_NONE,
    M_PENDING, /* proper prefix of a binding */
    M_EXACT,   /* a binding, nothing longer */
    M_AMBIG,   /* a binding AND a prefix of a longer one */
};

static knode *trie_find(const kmode *md, const tui_keyspec *seq, int n)
{
    const knode *cur = &md->root;
    for (int i = 0; i < n && cur; i++)
        cur = child_of(cur, seq[i]);
    return (knode *)cur;
}

static enum match trie_match(const kmode *md, const tui_keyspec *seq, int n,
                             knode **out)
{
    knode *nd = trie_find(md, seq, n);
    *out = nd;
    if (nd == NULL)
        return M_NONE;
    bool kids = false;
    for (const knode *c = nd->child; c && !kids; c = c->next)
        kids = live(c);
    if (nd->fn)
        return kids ? M_AMBIG : M_EXACT;
    return kids ? M_PENDING : M_NONE;
}

/* ------------------------------------------------------------------------ */
/* langmap                                                                  */
/* ------------------------------------------------------------------------ */

typedef struct lpair
{
    uint32_t from, to;
} lpair;

struct tui_langmap
{
    lpair *p; /* sorted by `from` */
    size_t n, cap;
};

bool tui_langmap_add(tui_langmap *lm, uint32_t from, uint32_t to)
{
    if (from < 0x80)
        return false; /* ASCII is never remapped */
    size_t lo = 0, hi = lm->n;
    while (lo < hi)
    {
        size_t mid = (lo + hi) / 2;
        if (lm->p[mid].from < from)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < lm->n && lm->p[lo].from == from)
    {
        lm->p[lo].to = to;
        return true;
    }
    if (lm->n == lm->cap)
    {
        lm->cap = lm->cap ? lm->cap * 2 : 64;
        lm->p = realloc(lm->p, lm->cap * sizeof(*lm->p));
        if (lm->p == NULL)
            abort();
    }
    memmove(&lm->p[lo + 1], &lm->p[lo], (lm->n - lo) * sizeof(*lm->p));
    lm->p[lo] = (lpair){from, to};
    lm->n++;
    return true;
}

void tui_langmap_add_strings(tui_langmap *lm, const char *from, const char *to)
{
    size_t fl = strlen(from), tl = strlen(to);
    while (fl && tl)
    {
        uint32_t a, b;
        size_t ka = tui_utf8_decode(from, fl, &a);
        size_t kb = tui_utf8_decode(to, tl, &b);
        if (!ka || !kb)
            break;
        tui_langmap_add(lm, a, b);
        from += ka, fl -= ka;
        to += kb, tl -= kb;
    }
}

bool tui_langmap_preset(tui_langmap *lm, const char *name)
{
    if (strcmp(name, "ru") == 0)
    {
        tui_langmap_add_strings(lm, "ёйцукенгшщзхъфывапролджэячсмитьбю",
                                "`qwertyuiop[]asdfghjkl;'zxcvbnm,.");
        tui_langmap_add_strings(lm, "ЁЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮ",
                                "~QWERTYUIOP{}ASDFGHJKL:\"ZXCVBNM<>");
        return true;
    }
    if (strcmp(name, "uk") == 0)
    {
        tui_langmap_add_strings(lm, "ґйцукенгшщзхїфівапролджєячсмитьбю",
                                "`qwertyuiop[]asdfghjkl;'zxcvbnm,.");
        tui_langmap_add_strings(lm, "ҐЙЦУКЕНГШЩЗХЇФІВАПРОЛДЖЄЯЧСМИТЬБЮ",
                                "~QWERTYUIOP{}ASDFGHJKL:\"ZXCVBNM<>");
        return true;
    }
    if (strcmp(name, "el") == 0)
    {
        tui_langmap_add_strings(lm, "ςερτυθιοπασδφγηξκλζχψωβνμ",
                                "wertyuiopasdfghjklzxcvbnm");
        tui_langmap_add_strings(lm, "ΕΡΤΥΘΙΟΠΑΣΔΦΓΗΞΚΛΖΧΨΩΒΝΜ",
                                "ERTYUIOPASDFGHJKLZXCVBNM");
        return true;
    }
    if (strcmp(name, "cjk") == 0)
    {
        /* what Chinese / Japanese IMEs emit for ASCII punctuation keys
         * (the full-width block U+FF01..FF5E is folded for everybody) */
        tui_langmap_add_strings(lm, "。、・「」【】《》“”‘’",
                                ".\\/[][]<>\"\"''");
        return true;
    }
    return false;
}

uint32_t tui_langmap_apply(const tui_langmap *lm, uint32_t cp)
{
    if (cp < 0x80)
        return cp;
    if (cp >= 0xFF01 && cp <= 0xFF5E)
        return cp - 0xFEE0;
    if (cp == 0x3000)
        return ' ';
    size_t lo = 0, hi = lm->n;
    while (lo < hi)
    {
        size_t mid = (lo + hi) / 2;
        if (lm->p[mid].from < cp)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < lm->n && lm->p[lo].from == cp ? lm->p[lo].to : cp;
}

/* ------------------------------------------------------------------------ */
/* router                                                                   */
/* ------------------------------------------------------------------------ */

typedef struct scope
{
    tui_node *node; /* NULL: global */
    tui_keymap *km;
    int mode;
} scope;

typedef struct layer
{
    tui_node *focus, *prev;
    scope *sc;
    bool modal;
    void (*on_close)(void *);
    void *ud;
} layer;

typedef struct pend
{
    scope *sc;
    tui_event ev[TUI_KEYSEQ_MAX];
    tui_keyspec k[TUI_KEYSEQ_MAX];
    int n;
    int count;
    bool has_count;
    double t0;
} pend;

struct tui_router
{
    tui_ctx *ctx;
    int timeout_ms;
    double clock; /* ms */
    tui_langmap lm;

    scope global;
    scope **scopes;
    int nscopes;
    layer *layers;
    int nlayers;

    pend pend;
    const scope *skip; /* scope whose keymap is bypassed while replaying */
    bool esc_held;
    tui_event esc_ev;
    char show[64];
};

static void pend_clear(tui_router *in)
{
    memset(&in->pend, 0, sizeof(in->pend));
}

static scope *find_scope(tui_router *in, const tui_node *n)
{
    for (int i = 0; i < in->nscopes; i++)
        if (in->scopes[i]->node == n)
            return in->scopes[i];
    return NULL;
}

static bool scope_alive(tui_router *in, const scope *sc)
{
    if (sc == &in->global)
        return sc->km != NULL;
    for (int i = 0; i < in->nscopes; i++)
        if (in->scopes[i] == sc)
            return true;
    return false;
}

static layer *top_layer(tui_router *in)
{
    return in->nlayers ? &in->layers[in->nlayers - 1] : NULL;
}

static bool modal_active(tui_router *in)
{
    layer *l = top_layer(in);
    return l && l->modal;
}

static bool route_key(tui_router *in, tui_event *ev);

/* The key as the keymap sees it. */
static tui_keyspec norm_key(tui_router *in, const kmode *md, const tui_event *ev)
{
    tui_keyspec k = {ev->key, ev->mod};
    if (!(md->flags & TUI_MODE_TEXT) && k.key >= 0x80 && k.key < 0x110000)
        k.key = tui_langmap_apply(&in->lm, k.key);
    if (k.key < 0x110000 && !(k.mod & TUI_MOD_CTRL))
        k.mod &= ~(uint32_t)TUI_MOD_SHIFT; /* shift is in the character */
    return k;
}

static bool run_binding(tui_router *in, scope *sc, const knode *nd,
                        const pend *p, int nkeys)
{
    tui_keyctx kc = {
        .input = in,
        .node = sc->node,
        .mode = sc->mode,
        .has_count = p->has_count,
        .count = p->count,
        .keys = p->k,
        .nkeys = nkeys,
        .event = &p->ev[nkeys > 0 ? nkeys - 1 : 0],
    };
    return nd->fn(&kc, nd->ud);
}

/* A sequence that cannot continue: run the longest binding among its first
 * `maxk` keys, or hand the first key to the widgets; then replay the rest. */
static void resolve(tui_router *in, scope *sc, int maxk)
{
    pend p = in->pend;
    pend_clear(in);
    const kmode *md = &sc->km->modes[sc->mode];
    int k = maxk;
    knode *nd = NULL;
    for (; k >= 1; k--)
    {
        nd = trie_find(md, p.k, k);
        if (nd && nd->fn)
            break;
        nd = NULL;
    }
    int used;
    if (nd)
    {
        run_binding(in, sc, nd, &p, k);
        used = k;
    }
    else
    {
        in->skip = sc;
        tui_event e = p.ev[0];
        tui_dispatch_from(in->ctx, sc->node, &e);
        in->skip = NULL;
        used = 1;
    }
    for (int i = used; i < p.n; i++)
    {
        tui_event e = p.ev[i];
        route_key(in, &e);
    }
}

static bool first_bound(const kmode *md, tui_keyspec k)
{
    const knode *c = child_of(&md->root, k);
    return c && live(c);
}

static bool scope_feed(tui_router *in, scope *sc, tui_event *ev)
{
    const kmode *md = &sc->km->modes[sc->mode];
    tui_keyspec k = norm_key(in, md, ev);
    pend *p = &in->pend;
    if (p->sc != sc)
        pend_clear(in);

    if ((md->flags & TUI_MODE_COUNT) && p->n == 0 && k.mod == 0 &&
        k.key >= '0' && k.key <= '9')
    {
        bool start = !p->has_count;
        if (!start || (k.key != '0' && !first_bound(md, k)))
        {
            if (p->count < 100000)
                p->count = p->count * 10 + (int)(k.key - '0');
            p->has_count = true;
            p->sc = sc;
            p->t0 = in->clock;
            return true;
        }
    }
    if (p->n == TUI_KEYSEQ_MAX)
    {
        pend_clear(in);
        return false;
    }
    p->ev[p->n] = *ev;
    p->ev[p->n].text = NULL;
    p->k[p->n] = k;
    p->n++;
    p->sc = sc;
    p->t0 = in->clock;

    knode *nd;
    switch (trie_match(md, p->k, p->n, &nd))
    {
    case M_EXACT: {
        pend snap = *p;
        pend_clear(in);
        return run_binding(in, sc, nd, &snap, snap.n);
    }
    case M_AMBIG:
    case M_PENDING:
        return true;
    case M_NONE:
        break;
    }
    if (p->n == 1)
    {
        bool had_count = p->has_count;
        pend_clear(in);
        return had_count; /* "5" then an unbound key: swallowed, like vim */
    }
    resolve(in, sc, p->n - 1);
    return true;
}

static bool hook(tui_node *n, tui_event *ev, void *ud)
{
    tui_router *in = ud;
    scope *sc;
    if (n)
        sc = find_scope(in, n);
    else
        sc = (!modal_active(in) && in->global.km) ? &in->global : NULL;
    if (sc == NULL || sc->km == NULL || sc == in->skip)
        return false;
    return scope_feed(in, sc, ev);
}

static bool within(const tui_node *root, const tui_node *n)
{
    for (; n; n = tui_parent(n))
        if (n == root)
            return true;
    return false;
}

/* modal layers keep the focus inside themselves */
static void trap(tui_router *in)
{
    layer *l = top_layer(in);
    if (l && l->modal && l->focus && !within(l->focus, tui_focused(in->ctx)))
        tui_focus(in->ctx, l->focus);
}

static bool route_key(tui_router *in, tui_event *ev)
{
    if (in->pend.sc)
    {
        if (!scope_alive(in, in->pend.sc))
            pend_clear(in);
        else if (ev->key == TUI_KEY_ESC && ev->mod == 0)
        {
            pend_clear(in); /* Esc abandons a half-typed command */
            return true;
        }
        else
        {
            scope_feed(in, in->pend.sc, ev);
            return true;
        }
    }
    bool used = tui_dispatch(in->ctx, ev);
    trap(in);
    return used;
}

static bool route_text(tui_router *in, tui_event *ev)
{
    if (!in->pend.sc)
    {
        if (tui_dispatch(in->ctx, ev))
        {
            trap(in);
            return true;
        }
    }
    /* nobody wanted the text: it is key presses after all */
    const char *s = ev->text;
    size_t len = ev->text_len;
    while (len > 0)
    {
        uint32_t cp;
        size_t k = tui_utf8_decode(s, len, &cp);
        if (k == 0)
            break;
        tui_event e = {.type = TUI_EV_KEY, .key = cp};
        route_key(in, &e);
        s += k;
        len -= k;
    }
    return true;
}

void tui_router_flush(tui_router *in)
{
    if (!in->esc_held)
        return;
    in->esc_held = false;
    tui_event e = in->esc_ev;
    route_key(in, &e);
}

bool tui_router_dispatch(tui_router *in, tui_event *ev)
{
    switch (ev->type)
    {
    case TUI_EV_KEY:
        if (in->esc_held)
        {
            if (ev->key == TUI_KEY_ESC && ev->mod == 0)
                tui_router_flush(in);
            else
            {
                in->esc_held = false; /* Esc + key in one batch = Alt + key */
                tui_event e = *ev;
                e.mod |= TUI_MOD_ALT;
                return route_key(in, &e);
            }
        }
        if (ev->key == TUI_KEY_ESC && ev->mod == 0)
        {
            in->esc_held = true;
            in->esc_ev = *ev;
            return true;
        }
        return route_key(in, ev);
    case TUI_EV_TEXT:
        tui_router_flush(in);
        return route_text(in, ev);
    case TUI_EV_MOUSE_DOWN:
        tui_router_flush(in);
        pend_clear(in);
        /* fallthrough */
    default: {
        if (ev->type != TUI_EV_MOUSE_DOWN && ev->type != TUI_EV_RESIZE)
            tui_router_flush(in);
        bool used = tui_dispatch(in->ctx, ev);
        trap(in);
        return used;
    }
    }
}

void tui_router_tick(tui_router *in, double dt)
{
    in->clock += dt * 1000.0;
    pend *p = &in->pend;
    if (p->sc && p->n > 0 && in->timeout_ms > 0 &&
        in->clock - p->t0 >= in->timeout_ms)
    {
        if (!scope_alive(in, p->sc))
            pend_clear(in);
        else
            resolve(in, p->sc, p->n);
    }
}

const char *tui_router_pending(const tui_router *in)
{
    tui_router *m = (tui_router *)in;
    size_t len = 0;
    m->show[0] = '\0';
    if (in->pend.has_count)
        len = (size_t)snprintf(m->show, sizeof(m->show), "%d", in->pend.count);
    if (in->pend.n > 0 && len < sizeof(m->show))
        tui_keys_format(in->pend.k, in->pend.n, m->show + len,
                        sizeof(m->show) - len);
    return m->show;
}

/* ---- scopes, layers --------------------------------------------------- */

static void node_freed(tui_node *n, void *ud)
{
    tui_router *in = ud;
    for (int i = 0; i < in->nlayers; i++)
    {
        if (in->layers[i].prev == n)
            in->layers[i].prev = NULL;
        if (in->layers[i].focus == n)
        {
            if (in->pend.sc == in->layers[i].sc)
                pend_clear(in);
            memmove(&in->layers[i], &in->layers[i + 1],
                    (size_t)(in->nlayers - i - 1) * sizeof(layer));
            in->nlayers--;
            i--;
        }
    }
    for (int i = 0; i < in->nscopes; i++)
        if (in->scopes[i]->node == n)
        {
            if (in->pend.sc == in->scopes[i])
                pend_clear(in);
            free(in->scopes[i]);
            in->scopes[i] = in->scopes[--in->nscopes];
            break;
        }
}

tui_router *tui_router_new(tui_ctx *ctx, const tui_router_options *opt)
{
    tui_router *in = calloc(1, sizeof(*in));
    if (in == NULL)
        abort();
    in->ctx = ctx;
    in->timeout_ms = opt && opt->timeout_ms ? opt->timeout_ms : 500;
    tui_set_key_hook(ctx, hook, in);
    tui_set_node_free_hook(ctx, node_freed, in);
    return in;
}

void tui_router_free(tui_router *in)
{
    if (in == NULL)
        return;
    tui_set_key_hook(in->ctx, NULL, NULL);
    tui_set_node_free_hook(in->ctx, NULL, NULL);
    for (int i = 0; i < in->nscopes; i++)
        free(in->scopes[i]);
    free(in->scopes);
    free(in->layers);
    free(in->lm.p);
    free(in);
}

void tui_router_set_global(tui_router *in, tui_keymap *km, int mode)
{
    in->global.km = km;
    in->global.mode = mode;
}

void tui_router_attach(tui_router *in, tui_node *node, tui_keymap *km, int mode)
{
    scope *sc = find_scope(in, node);
    if (sc == NULL)
    {
        sc = calloc(1, sizeof(*sc));
        if (sc == NULL)
            abort();
        in->scopes = realloc(in->scopes, (size_t)(in->nscopes + 1) * sizeof(*in->scopes));
        if (in->scopes == NULL)
            abort();
        in->scopes[in->nscopes++] = sc;
    }
    sc->node = node;
    sc->km = km;
    sc->mode = mode;
}

void tui_router_detach(tui_router *in, tui_node *node)
{
    for (int i = 0; i < in->nscopes; i++)
        if (in->scopes[i]->node == node)
        {
            if (in->pend.sc == in->scopes[i])
                pend_clear(in);
            free(in->scopes[i]);
            in->scopes[i] = in->scopes[--in->nscopes];
            return;
        }
}

void tui_router_set_mode(tui_router *in, tui_node *node, int mode)
{
    scope *sc = node ? find_scope(in, node) : &in->global;
    if (sc)
    {
        sc->mode = mode;
        if (in->pend.sc == sc)
            pend_clear(in);
    }
}

int tui_router_mode(const tui_router *in, tui_node *node)
{
    scope *sc = node ? find_scope((tui_router *)in, node) : (scope *)&in->global;
    return sc ? sc->mode : -1;
}

tui_langmap *tui_router_langmap(tui_router *in)
{
    return &in->lm;
}

void tui_router_push_layer(tui_router *in, const tui_layer_desc *d)
{
    in->layers = realloc(in->layers, (size_t)(in->nlayers + 1) * sizeof(*in->layers));
    if (in->layers == NULL)
        abort();
    pend_clear(in);
    layer *l = &in->layers[in->nlayers++];
    *l = (layer){.focus = d->focus,
                 .prev = tui_focused(in->ctx),
                 .modal = d->modal,
                 .on_close = d->on_close,
                 .ud = d->ud};
    if (d->keymap)
    {
        tui_router_attach(in, d->focus, d->keymap, d->mode);
        l->sc = find_scope(in, d->focus);
    }
    tui_focus(in->ctx, d->focus);
}

void tui_router_pop_layer(tui_router *in)
{
    if (in->nlayers == 0)
        return;
    layer l = in->layers[--in->nlayers];
    pend_clear(in);
    if (l.sc)
        tui_router_detach(in, l.focus);
    tui_node *back = l.prev && tui_is_visible(l.prev) ? l.prev : NULL;
    tui_focus(in->ctx, back);
    if (l.on_close)
        l.on_close(l.ud);
}

int tui_router_layer_depth(const tui_router *in)
{
    return in->nlayers;
}
