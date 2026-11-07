#include "ui_internal.h"

#include "audio_source.h"
#include "audio_mixer.h"
#include "clock.h"
#include "image_renderer.h"
#include "metagen.h"
#include "ui_font.h"
#include "ui_grow.h"
#include "ui_sprite.h"

#include <limits.h>
#include <math.h>
#include <string.h>

/*
 * Lyrics.
 *
 *  - Synced lyrics follow the playback position: the current line is kept at
 *    a fixed height and glides there when the line changes.
 *  - Plain lyrics scroll at a constant speed (ui_options.lyrics.speed).
 *  - Either way the user can take over (mouse wheel, j/k, page keys) and
 *    hand control back (zz, or a click on the hint at the bottom). Clicking a
 *    synced line seeks to it.
 *
 * Lines are wrapped to the width, then drawn row by row, fading with their
 * distance from the reading position into whatever is behind them (the
 * page colour of the VISUAL page, or the list colour elsewhere).
 *
 * The line being sung is drawn big. Terminal text has one size, so that line
 * is not text at all: it is rasterized by ui_font into a sub-pixel
 * framebuffer (2x4 per cell, like the cover art), fitted to cells by
 * ui_fb_to_cells() and painted over the page. Because it is pixels it can be
 * any size, which is what makes the change from one line to the next smooth:
 *
 *   - Every line has a size 0..1 (ui_grow). The new line grows from 0 while
 *     the old one shrinks back, both eased, so for a moment the two share it.
 *   - A line's height in rows is its small height (its wrapped rows) blended
 *     towards its big height (see `big`) by that size. Everything below it is
 *     pushed along, so the page moves continuously instead of jumping.
 *   - The reading position is the size-weighted mean of the lines in
 *     transition, each taken where it will be when fully grown (see
 *     focus_centre()). Every line's place on screen is then a straight line
 *     between where it was and where it ends up: the page moves one way, and
 *     there is no overshoot to take back. For that the position is not eased
 *     any further while lines grow (an eased position lags the stretch of the
 *     page and the lines below swing past their mark, then back).
 *   - While a line is part-grown its text is cross-faded: the terminal text
 *     fades out as the pixel text, scaled to the line's current size, fades in.
 *
 * Without a font (none installed, or `lyrics.big_rows` is 0) none of this
 * happens and every line keeps its small height: the page behaves as it
 * always did.
 */

#define LEFT_PAD   2    /* room for the marker of the current line */
#define ANCHOR     0.38f /* height of the reading position, fraction of rows */
#define MARK       0x258E /* ▎ */
#define GAP_DOTS   3
#define KARA_DIM   0.40f /* most of the ink that what is still to be sung keeps */
#define KARA_INK_GAP 0.50f /* least luma gap, 0..1, of the sung text from the
                            * background: a theme colour nearer is pushed out */
#define KARA_SPLIT_GAP 0.30f /* least luma gap of sung from still-to-sing */
#define KARA_DIM_FLOOR 0.12f /* least luma gap of still-to-sing from the
                              * background, so it can still be read */
#define GAP_FALLBACK_MS 4000 /* assumed length of an empty last line */
#define WHEEL_ROWS 3
#define MIN_LEVEL  0.14f /* share of the text colour the faintest row keeps */

/* the big line */
#define BIG_EM       0.80f /* em of the text, as a share of its line box */
#define BIG_PAD      1.0f  /* rows of air around the block, in all */
#define BIG_MAX_ROWS UI_LYRICS_BIG_ROWS_MAX
#define BIG_MIN_ROWS_BLOCKS  4 /* blocks have 2x2 detail per cell: smaller */
#define BIG_MIN_ROWS_BRAILLE 3 /* than this the letters run together */
/* Characters of full width (CJK, kana, Hangul) have far more strokes in the
 * same square: they need about a quarter more rows to stay legible. */
#define BIG_MIN_ROWS_WIDE_BLOCKS  6
#define BIG_MIN_ROWS_WIDE_BRAILLE 5
#define BIG_MAX_SHARE 0.62f /* of the height one line may take */
#define BIG_MIN_H     8     /* rows the widget needs to show big text */
#define BIG_MIN_W     16
/* The two forms of a growing line must hardly overlap: pixel text scaled
 * below about three quarters of its size is a smudge, and its solid cells
 * would paint over the terminal text beneath. The terminal text is gone by
 * SMALL_FADE_OUT, the pixel text only appears after that. */
#define SMALL_FADE_OUT 0.30f /* size at which the terminal text has gone */
#define BIG_FADE_IN    0.30f /* size from which the pixel text starts to show */
#define BIG_FADE_SPAN  0.40f /* ... and is fully there this much later */

/* Where the fill of the line being sung has got to (word timing). */
typedef struct kfill
{
    bool valid; /* the line has timing; false: it is painted as a whole */
    int line;
    enum
    {
        KF_BEFORE, /* nothing sung yet */
        KF_INSIDE, /* in the unit below */
        KF_AFTER,  /* all of it sung */
    } state;
    int off, len;      /* bytes of the line's text the unit covers */
    int sub, nsub;     /* the part of them (a shared glyph), sub of nsub */
    float f;           /* how far through the unit, 0..1 */
    int idx;           /* its index, for the repaint signature */
} kfill;

typedef struct seg
{
    int line;
    int off, len; /* bytes of the line's text */
} seg;

/* The big form of one line: wrapped to the width at the largest size that
 * fits. Built when the line first starts to grow, dropped when it is small
 * again or the layout changes. */
typedef struct big
{
    int line; /* -1: free */
    int w, h; /* the widget size it was made for, in cells */
    bool ok;  /* false: does not fit, the line stays small */
    int rows; /* rows of one text line (the line box) */
    float em; /* sub-pixels */
    ui_font_span *sp;
    int n;
    float block; /* height of the whole block, in rows, padding included */
} big;

struct ui_lyrics
{
    ui_ctx *u;
    tui_node *node;

    const metadata_t *meta; /* lyrics source the layout was built for */
    uint32_t serial;        /* ... and the track it belonged to: a new track's
                               metadata may reuse the old one's address */
    int layout_w;
    seg *segs; /* one per screen row */
    int nsegs, cap_segs;
    int *first; /* first row of each line, plus the total at [nlines] */
    int nlines;

    bool synced;
    int current;      /* highlighted line, -1 none */
    float pos;        /* row shown at the top, eased */
    bool locked;      /* pos is exactly what the music asks for (it only
                         moves with it); false: it is still gliding there */
    float base;       /* centre of the current line, eased: what the reading
                         position rests on when no line is growing */
    float auto_pos;   /* plain lyrics: where the clock says the top is */
    bool manual;      /* the user took over */
    float manual_pos; /* ... and put the top here */
    float idle;       /* seconds since the user last scrolled */

    int hint_y; /* row of the "resume" hint while manual, else -1 */

    ui_font *font; /* shared with the other lyrics; NULL: no big text */
    ui_grow grow;  /* which lines are big, and how much */
    big bigs[UI_GROW_MAX + 1];
    int view_w, view_h; /* content size at the last update / paint */
    ui_fb fb;           /* the big text, in sub-pixels ... */
    ui_cell *grid;      /* ... fitted to cells */
    size_t grid_cap;

    /* Word timing of the current line, copied out of the karaoke job so that
     * painting never touches it (see kfill_update). */
    kara_syl *ksyl;
    int kn, kcap;
    int kline; /* the line it is for */
    uint32_t kserial;
    const void *kowner;
    kfill kf;
    int sig_kf; /* the fill, as the last paint drew it */

    /* what the last paint was drawn from */
    int sig_top, sig_cur, sig_gap, sig_h, sig_w;
    bool sig_manual, sig_big;
    int sig_rows; /* lyrics.big_rows the cached big lines were made for */
    uint32_t sig_rev;
};

/* ------------------------------------------------------------------------ */
/* layout                                                                   */
/* ------------------------------------------------------------------------ */

static metadata_t *current_meta(ui_ctx *u)
{
    audio_source *src = app_current_source(u->app);
    if (src == NULL)
        return NULL;
    return src->get_metadata ? src->get_metadata(src) : NULL;
}

static size_t line_count(const metadata_t *m)
{
    return m->n_lyrics_synced > 0 ? m->n_lyrics_synced : m->n_lyrics;
}

static const char *line_text(const metadata_t *m, size_t i)
{
    const char *t = m->n_lyrics_synced > 0 ? m->lyrics_synced[i].text
                                           : m->lyrics[i].text;
    return t ? t : "";
}

static void push_seg(ui_lyrics *l, int line, int off, int len)
{
    if (l->nsegs == l->cap_segs)
    {
        l->cap_segs = l->cap_segs ? l->cap_segs * 2 : 128;
        l->segs = realloc(l->segs, (size_t)l->cap_segs * sizeof(seg));
        if (l->segs == NULL)
            abort();
    }
    l->segs[l->nsegs++] = (seg){line, off, len};
}

/* Greedy word wrap into rows of at most `width` columns; a word longer than a
 * row (or text without spaces, like Chinese) breaks wherever it has to. An
 * empty line is one empty row so the gap stays. */
static void wrap_line(ui_lyrics *l, int line, const char *s, int width)
{
    size_t len = strlen(s);
    size_t start = 0;
    if (len == 0)
    {
        push_seg(l, line, 0, 0);
        return;
    }

    while (start < len)
    {
        size_t i = start, brk = 0, resume = 0;
        int col = 0;
        bool fits_all = true;

        while (i < len)
        {
            uint32_t cp;
            size_t n = tui_utf8_decode(s + i, len - i, &cp);
            if (n == 0)
                break;
            int cw = tui_cp_width(cp);
            if (col + cw > width && col > 0)
            {
                fits_all = false;
                break;
            }
            if (cp == ' ' && i > start)
            {
                brk = i;
                resume = i + n;
            }
            col += cw;
            i += n;
        }

        size_t end = i;
        size_t next = i;
        if (!fits_all && brk > start)
        {
            end = brk;
            next = resume;
        }
        while (end > start && s[end - 1] == ' ')
            end--;
        push_seg(l, line, (int)start, (int)(end - start));
        while (next < len && s[next] == ' ' && !fits_all)
            next++;
        start = next > start ? next : len;
    }
}

static void big_free(big *b)
{
    free(b->sp);
    *b = (big){.line = -1};
}

static void bigs_free(ui_lyrics *l)
{
    for (int i = 0; i < UI_GROW_MAX + 1; i++)
        big_free(&l->bigs[i]);
}

static void relayout(ui_lyrics *l, const metadata_t *m, int width)
{
    bigs_free(l);
    l->nsegs = 0;
    free(l->first);
    l->first = NULL;
    l->nlines = m ? (int)line_count(m) : 0;
    l->meta = m;
    l->layout_w = width;

    l->first = calloc((size_t)l->nlines + 1, sizeof(int));
    if (l->first == NULL)
        abort();
    for (int i = 0; i < l->nlines; i++)
    {
        l->first[i] = l->nsegs;
        wrap_line(l, i, line_text(m, (size_t)i), MATH_MAX(width - LEFT_PAD, 1));
    }
    l->first[l->nlines] = l->nsegs;
    l->synced = m && m->n_lyrics_synced > 0;
}

static int rows_of(const ui_lyrics *l, int line)
{
    return l->first[line + 1] - l->first[line];
}

/* ------------------------------------------------------------------------ */
/* the big line                                                             */
/* ------------------------------------------------------------------------ */

static uint32_t glyph_blocks(uint8_t mask)
{
    return (uint32_t)image_mask_to_glyph(mask);
}

static uint32_t glyph_braille(uint8_t mask)
{
    return (uint32_t)image_mask_to_braille(mask);
}

/* Wraps `line` at the largest size that leaves the block no taller than a
 * share of the widget. Sizes are whole rows per text line, from the option
 * down to the smallest that still reads with the glyphs in use. */
static bool big_build(ui_lyrics *l, big *b, int line)
{
    const ui_options *o = &l->u->opt;
    if (l->font == NULL || l->meta == NULL)
        return false;

    const char *text = line_text(l->meta, (size_t)line);
    size_t len = strlen(text);
    float max_w = (float)((l->view_w - LEFT_PAD) * UI_SUBCELL_W) - 2.0f;
    if (len == 0 || max_w < 8.0f || l->view_w < BIG_MIN_W ||
        l->view_h < BIG_MIN_H)
        return false;

    int top = MATH_CLAMP(o->lyrics.big_rows, 0, BIG_MAX_ROWS);
    if (top <= 0)
        return false;

    bool braille = o->lyrics.big_braille;
    int floor_rows = braille ? BIG_MIN_ROWS_BRAILLE : BIG_MIN_ROWS_BLOCKS;
    int low = MATH_MIN(floor_rows, top);
    for (size_t i = 0; i < len;)
    {
        uint32_t cp;
        size_t n = tui_utf8_decode(text + i, len - i, &cp);
        if (n == 0)
            break;
        if (tui_cp_width(cp) > 1)
        {
            /* bigger than asked for, rather than unreadable */
            low = braille ? BIG_MIN_ROWS_WIDE_BRAILLE : BIG_MIN_ROWS_WIDE_BLOCKS;
            top = MATH_MAX(top, low);
            break;
        }
        i += n;
    }

    int cap = (int)len + 1;
    ui_font_span *sp = malloc((size_t)cap * sizeof(*sp));
    if (sp == NULL)
        abort();
    for (int rows = top; rows >= low; rows--)
    {
        float em = (float)(rows * UI_SUBCELL_H) * BIG_EM;
        int n = ui_font_wrap(l->font, text, len, em, max_w, sp, cap);
        float block = (float)(n * rows) + BIG_PAD;
        if (block <= (float)l->view_h * BIG_MAX_SHARE)
        {
            b->rows = rows;
            b->em = em;
            b->sp = sp;
            b->n = n;
            b->block = block;
            return true;
        }
    }
    free(sp);
    return false;
}

/* The big form of `line`, or NULL when it has none (no font, does not fit,
 * empty). Cached; `view_w` / `view_h` say what it has to fit. */
static const big *big_for(ui_lyrics *l, int line)
{
    big *slot = NULL;
    for (int i = 0; i < UI_GROW_MAX + 1; i++)
    {
        big *b = &l->bigs[i];
        if (b->line == line)
        {
            if (b->w == l->view_w && b->h == l->view_h)
                return b->ok ? b : NULL;
            slot = b; /* made for another size: redo it in place */
            break;
        }
        if (b->line < 0 && slot == NULL)
            slot = b;
    }
    if (slot == NULL)
    {
        /* every slot busy: reuse one whose line is no longer in motion */
        for (int i = 0; i < UI_GROW_MAX + 1 && slot == NULL; i++)
            if (ui_grow_get(&l->grow, l->bigs[i].line) <= 0.0f &&
                l->bigs[i].line != l->current)
                slot = &l->bigs[i];
        if (slot == NULL)
            slot = &l->bigs[0];
    }
    big_free(slot);
    slot->line = line;
    slot->w = l->view_w;
    slot->h = l->view_h;
    slot->ok = big_build(l, slot, line);
    return slot->ok ? slot : NULL;
}

/* How big `line` is right now, 0 when it cannot be big at all. */
static float size_of(ui_lyrics *l, int line)
{
    float e = ui_grow_get(&l->grow, line);
    if (e <= 0.0f)
        return 0.0f;
    return big_for(l, line) != NULL ? e : 0.0f;
}

/* ------------------------------------------------------------------------ */
/* geometry                                                                 */
/* ------------------------------------------------------------------------ */

/*
 * Rows are what a line occupies; with nothing big, line i starts at first[i]
 * and is rows_of(i) tall, exactly the wrapped rows. A line that is part-big
 * is taller (blending its small height into its big block by its size), and
 * every line after it moves down by the difference. All positions below are
 * in that stretched space, in rows from the top of the lyrics, fractional.
 */

static float line_h(ui_lyrics *l, int line)
{
    float small = (float)rows_of(l, line);
    float e = size_of(l, line);
    if (e <= 0.0f)
        return small;
    return small + (big_for(l, line)->block - small) * e;
}

static float line_y(ui_lyrics *l, int line)
{
    float y = (float)l->first[line];
    for (int i = 0; i < l->grow.n; i++)
    {
        int s = l->grow.s[i].line;
        if (s < line)
            y += line_h(l, s) - (float)rows_of(l, s);
    }
    return y;
}

static float total_rows(ui_lyrics *l)
{
    float y = (float)l->nsegs;
    for (int i = 0; i < l->grow.n; i++)
    {
        int s = l->grow.s[i].line;
        y += line_h(l, s) - (float)rows_of(l, s);
    }
    return y;
}

/* The last line that starts at or above `y`. */
static int first_visible(ui_lyrics *l, float y)
{
    int lo = 0, hi = l->nlines - 1;
    while (lo < hi)
    {
        int mid = (lo + hi + 1) / 2;
        if (line_y(l, mid) <= y)
            lo = mid;
        else
            hi = mid - 1;
    }
    return lo;
}

/* Which line covers the row `y`; -1 above the first line or below the last. */
static int line_at_row(ui_lyrics *l, float y)
{
    if (y < 0.0f || l->nlines == 0)
        return -1;
    int i = first_visible(l, y);
    return y < line_y(l, i) + line_h(l, i) ? i : -1;
}

/* Centre of the current line (as a row index, like the old `first + (rows -
 * 1) / 2`) in the page with nothing stretched: where the reading position
 * rests while no line grows. */
static float rest_centre(const ui_lyrics *l, int line)
{
    if (line < 0 || line >= l->nlines)
        return 0.0f;
    return (float)l->first[line] + ((float)rows_of(l, line) - 1.0f) * 0.5f;
}

/*
 * The row the reading position should rest on.
 *
 * Every line in motion counts once, with the weight of its size, at the
 * centre it will have when it is fully grown and nothing else is stretched:
 * `first[line] + (block - 1) / 2`. A line `L` below them all sits at
 *
 *     first[L] + sum e_i (block_i - small_i)
 *
 * and the position is  sum e_i (first[i] + (block_i - 1) / 2)  (the weights
 * add up to one), so on screen `L` is at a constant plus a weighted sum of the
 * e_i: a straight line in them, and they all move one way in time. Lines above
 * and between are the same. The centre of the new line as it stands (taller as
 * it grows) overshoots, then comes back as the old one shrinks; a plain mean
 * of the *current* centres of the two does too, since a line's own growth moves
 * its centre faster than the weights move.
 *
 * When the sizes do not add up to a whole line (a seek, the first line, a line
 * that does not fit) the rest comes from `base`, the centre of the current
 * line, eased: that is the part of the motion that can jump, so it is the part
 * that glides.
 */
static float focus_centre(ui_lyrics *l)
{
    float sum = 0.0f, weight = 0.0f;
    for (int i = 0; i < l->grow.n; i++)
    {
        int line = l->grow.s[i].line;
        float e = size_of(l, line);
        if (e <= 0.0f)
            continue;
        sum += e * ((float)l->first[line] + (big_for(l, line)->block - 1.0f) * 0.5f);
        weight += e;
    }
    if (weight >= 1.0f)
        return sum / weight;
    return sum + (1.0f - weight) * l->base;
}

/* ------------------------------------------------------------------------ */
/* taking over                                                              */
/* ------------------------------------------------------------------------ */

static float focus_target(ui_lyrics *l, int h)
{
    float anchor = ANCHOR * (float)h;
    if (l->synced)
        return focus_centre(l) - anchor;
    return l->auto_pos;
}

void ui_lyrics_scroll(ui_lyrics *l, int rows)
{
    int h = tui_get_content_rect(l->node).h;
    if (!l->manual)
    {
        l->manual = true;
        l->manual_pos = focus_target(l, h);
    }
    float lo = -ANCHOR * (float)h;
    float hi = total_rows(l) - ANCHOR * (float)h;
    l->manual_pos = MATH_CLAMP(l->manual_pos + (float)rows, lo, hi);
    l->idle = 0.0f;
    tui_invalidate(l->node);
}

void ui_lyrics_follow(ui_lyrics *l)
{
    if (!l->manual)
        return;
    l->manual = false;
    l->idle = 0.0f;
    tui_invalidate(l->node);
}

/* ------------------------------------------------------------------------ */
/* per frame                                                                */
/* ------------------------------------------------------------------------ */

static void lyrics_free(tui_node *n, void *ud)
{
    free(((ui_lyrics *)ud)->ksyl);
    (void)n;
    ui_lyrics *l = ud;
    bigs_free(l);
    ui_fb_free(&l->fb);
    free(l->grid);
    free(l->segs);
    free(l->first);
    free(l);
}

static int line_at(const metadata_t *m, int count, int64_t ts)
{
    int cur = -1;
    for (int i = 0; i < count; i++)
    {
        if (m->lyrics_synced[i].start_ms > ts)
            break;
        cur = i;
    }
    return cur;
}

/* 0..GAP_DOTS: how many dots of an instrumental gap have lit up */
static int gap_dots(const ui_lyrics *l, const metadata_t *m, int64_t ts)
{
    if (!l->synced || l->current < 0 ||
        line_text(m, (size_t)l->current)[0] != '\0')
        return -1;
    int start = m->lyrics_synced[l->current].start_ms;
    int end = l->current + 1 < l->nlines
                  ? m->lyrics_synced[l->current + 1].start_ms
                  : start + GAP_FALLBACK_MS;
    float f = end > start ? (float)(ts - start) / (float)(end - start) : 1.0f;
    return (int)MATH_CLAMP(f * (float)(GAP_DOTS + 1), 0.0f, (float)GAP_DOTS);
}

/* The fill state of the current line at `ts`. The karaoke job is asked about
 * the line being sung first (so its timing is the first to be ready), and
 * its result for that line is copied here once. */
static void kfill_update(ui_lyrics *l, audio_source *src, const metadata_t *m,
                         int64_t ts)
{
    const ui_options *o = &l->u->opt;
    l->kf.valid = false;
    if (!o->lyrics.karaoke || !l->synced || l->current < 0 || m == NULL ||
        m->n_lyrics_synced == 0 || src == NULL || src->get_karaoke == NULL)
    {
        l->kn = 0;
        l->kline = -1;
        return;
    }
    karaoke *k = src->get_karaoke(src);
    if (k == NULL)
    {
        l->kn = 0;
        return;
    }
    kara_focus(k, l->current);

    uint32_t serial = l->u->app->track_serial;
    if (l->kline != l->current || l->kserial != serial || l->kowner != k)
    {
        l->kn = 0;
        l->kline = l->current;
        l->kserial = serial;
        l->kowner = k;
    }
    if (l->kn == 0)
    {
        const kara_line *kl = kara_line_get(k, l->current);
        if (kl != NULL && kl->n > 0)
        {
            if (kl->n > l->kcap)
            {
                kara_syl *p = realloc(l->ksyl, (size_t)kl->n * sizeof(*p));
                if (p == NULL)
                    return;
                l->ksyl = p;
                l->kcap = kl->n;
            }
            memcpy(l->ksyl, kl->syl, (size_t)kl->n * sizeof(*kl->syl));
            l->kn = kl->n;
        }
    }
    if (l->kn == 0)
        return;

    int64_t t = ts - o->lyrics.karaoke_offset_ms;
    const kara_syl *s = l->ksyl;
    kfill *kf = &l->kf;
    kf->valid = true;
    kf->line = l->current;
    kf->idx = 0;
    if (t < s[0].start_ms)
    {
        kf->state = KF_BEFORE;
        return;
    }
    if (t >= s[l->kn - 1].end_ms)
    {
        kf->state = KF_AFTER;
        kf->idx = l->kn;
        return;
    }
    int i = 0;
    while (i + 1 < l->kn && t >= s[i + 1].start_ms)
        i++;
    kf->state = KF_INSIDE;
    kf->idx = i;
    kf->off = s[i].off;
    kf->len = s[i].len;
    kf->sub = s[i].sub;
    kf->nsub = s[i].nsub > 0 ? s[i].nsub : 1;
    float span = (float)(s[i].end_ms - s[i].start_ms);
    kf->f = span > 0 ? MATH_CLAMP((float)(t - s[i].start_ms) / span, 0.0f, 1.0f)
                     : 1.0f;
}

static void lyrics_update(tui_node *n, double dt, void *ud)
{
    ui_lyrics *l = ud;
    ui_ctx *u = l->u;
    const ui_options *o = &u->opt;
    metadata_t *m = current_meta(u);
    audio_source *src = app_current_source(u->app);
    tui_rect c = tui_get_content_rect(n);
    int h = c.h;
    float dts = (float)dt;

    /* on the VISUAL page the text sits straight on the page */
    tui_attr at = tui_get_attr(n);
    color_t want_bg = u->skin_active ? COLOR_NONE : u->theme.list.normal_bg;
    if (memcmp(&at.bg, &want_bg, sizeof(color_t)) != 0)
    {
        at.bg = want_bg;
        tui_set_attr(n, &at);
    }

    /* a new size or a new big size: the page glides to where it now belongs
     * instead of being held to the music */
    if (c.w != l->view_w || h != l->view_h)
        l->locked = false;
    l->view_w = c.w;
    l->view_h = h;
    if (o->lyrics.big_rows != l->sig_rows)
    {
        l->sig_rows = o->lyrics.big_rows;
        bigs_free(l); /* made for the old size */
        l->locked = false;
        tui_invalidate(n);
    }

    uint32_t serial = u->app->track_serial;
    if (m != l->meta || serial != l->serial || c.w != l->layout_w)
    {
        bool new_track = m != l->meta || serial != l->serial;
        l->serial = serial;
        relayout(l, m, c.w);
        l->base = rest_centre(l, l->current);
        l->locked = false;
        if (new_track)
        {
            l->current = -1;
            l->auto_pos = 0.0f;
            l->manual = false;
            ui_grow_reset(&l->grow);
            l->base = rest_centre(l, -1);
            l->pos = focus_target(l, h);
            l->locked = true;
        }
    }

    int64_t ts = src ? (int64_t)US2MS(src->timestamp) : 0;
    bool playing = src != NULL && !u->app->audio->mixer.paused;

    if (l->synced)
        l->current = line_at(m, l->nlines, ts);
    else
    {
        l->current = -1;
        if (playing)
            l->auto_pos += o->lyrics.speed * dts;
        float end = MATH_MAX((float)l->nsegs - (float)h, 0.0f);
        l->auto_pos = MATH_CLAMP(l->auto_pos, 0.0f, end);
    }

    kfill_update(l, src, m, ts);

    /* which line is to be big: the one being sung, if there is a font and the
     * line is more than a pause */
    int want = -1;
    if (l->synced && l->current >= 0 && o->lyrics.big_rows > 0 &&
        line_text(m, (size_t)l->current)[0] != '\0' && l->view_h >= BIG_MIN_H &&
        l->view_w >= BIG_MIN_W)
    {
        if (l->font == NULL)
            l->font = ui_ctx_font(u);
        if (l->font != NULL)
            want = l->current;
    }
    ui_grow_step(&l->grow, want, dts,
                 o->lyrics.grow_ms > 0 ? (float)o->lyrics.grow_ms * 0.001f
                                       : 0.0f);

    if (l->manual)
    {
        l->idle += dts;
        if (o->lyrics.resume_s > 0 && l->idle >= (float)o->lyrics.resume_s)
            l->manual = false;
    }

    float ease = o->lyrics.ease_ms;
    if (l->synced)
    {
        /* what the position rests on glides to the current line */
        float rest = rest_centre(l, l->current);
        if (ease <= 0.0f || fabsf(rest - l->base) < 0.02f)
            l->base = rest;
        else
            l->base += (rest - l->base) * (1.0f - expf(-dts * 1000.0f / ease));
    }

    float target = l->manual ? l->manual_pos : focus_target(l, h);
    if (l->synced && !l->manual && l->locked)
        l->pos = target; /* already eased where it has to be (focus_centre) */
    else
    {
        if (ease <= 0.0f || fabsf(target - l->pos) < 0.02f)
        {
            l->pos = target;
            l->locked = l->synced && !l->manual;
        }
        else
            l->pos += (target - l->pos) * (1.0f - expf(-dts * 1000.0f / ease));
    }
    if (l->manual)
        l->locked = false;

    /* big text moves in quarter rows (one sub-pixel), terminal text in whole */
    bool big_live = l->grow.n > 0;
    int top = big_live ? (int)floorf(l->pos * (float)UI_SUBCELL_H + 0.5f)
                       : (int)floorf(l->pos + 0.5f);
    int gap = m ? gap_dots(l, m, ts) : -1;
    uint32_t rev = u->skin_active ? u->ambience.rev : 0;
    /* the fill moves while a line is sung: repaint when it has moved a pixel
     * or so (64 steps per unit), and not at all while paused */
    int kq = l->kf.valid ? l->kf.idx * 128 + l->kf.state * 64 +
                               (l->kf.state == KF_INSIDE ? (int)(l->kf.f * 63.0f)
                                                         : 0)
                         : -1;
    if (kq != l->sig_kf)
    {
        l->sig_kf = kq;
        tui_invalidate(n);
    }
    if (ui_grow_busy(&l->grow, want) || big_live != l->sig_big ||
        top != l->sig_top || l->current != l->sig_cur || gap != l->sig_gap ||
        h != l->sig_h || c.w != l->sig_w || l->manual != l->sig_manual ||
        rev != l->sig_rev)
    {
        l->sig_top = top;
        l->sig_cur = l->current;
        l->sig_gap = gap;
        l->sig_h = h;
        l->sig_w = c.w;
        l->sig_manual = l->manual;
        l->sig_big = big_live;
        l->sig_rev = rev;
        tui_invalidate(n);
    }
}

/* ------------------------------------------------------------------------ */
/* painting                                                                 */
/* ------------------------------------------------------------------------ */

static float fade_at(float dist, float sigma)
{
    float d = dist / sigma;
    return expf(-d * d);
}

static float smooth(float t)
{
    t = MATH_CLAMP(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/* what the painters share for one frame */
typedef struct pctx
{
    ui_lyrics *l;
    tui_canvas *cv;
    const ui_theme *t;
    const metadata_t *m;
    int W, H;
    bool skin;
    float sigma;
    float focus; /* row (on screen) the fade is centred on */
    color_t behind;
    color_t cur_ink;
    int64_t ts;
} pctx;

static float kara_luma(color_t c)
{
    return (0.2126f * (float)c.r + 0.7152f * (float)c.g + 0.0722f * (float)c.b) /
           255.0f;
}

/* The two inks of a line being sung. `sung` is the theme's ink, moved away
 * from the background only when it is too close to it to stand out. `dim`
 * (still to be sung) lies between the background and `sung`, as near the
 * background as keeps it readable, so the two always differ clearly whatever
 * colours the theme or the ambience gives. */
static void kara_inks(color_t behind, color_t ink, color_t *sung, color_t *dim)
{
    float lb = kara_luma(behind);
    color_t out = ink;

    float gap = fabsf(kara_luma(out) - lb);
    if (gap < KARA_INK_GAP)
    {
        color_t far_ = lb < 0.5f ? COLOR(255, 255, 255) : COLOR(0, 0, 0);
        for (int i = 1; i <= 10 && gap < KARA_INK_GAP; i++)
        {
            out = color_mix(ink, far_, (float)i / 10.0f);
            gap = fabsf(kara_luma(out) - lb);
        }
    }

    float d = fabsf(kara_luma(out) - lb); /* span from background to ink */
    float k = KARA_DIM;
    if (d > 0.01f)
    {
        float k_split = 1.0f - KARA_SPLIT_GAP / d; /* far enough from sung */
        float k_read = KARA_DIM_FLOOR / d;         /* far enough from behind */
        if (k_split < k)
            k = k_split;
        if (k < k_read)
            k = k_read; /* readable wins over a wide split */
        k = MATH_CLAMP(k, 0.08f, 0.6f);
    }
    *sung = out;
    *dim = color_mix(behind, out, k);
}

/* The byte of the line's text where the fill has got to; INT_MAX: all of it.
 * Inside a unit it is placed by characters. */
static int kfill_byte(const kfill *kf, const char *text)
{
    if (kf->state == KF_BEFORE)
        return 0;
    if (kf->state == KF_AFTER)
        return INT_MAX;
    int nch = 0;
    for (int i = kf->off; i < kf->off + kf->len;)
    {
        uint32_t cp;
        size_t n = tui_utf8_decode(text + i, (size_t)(kf->off + kf->len - i), &cp);
        if (n == 0)
            break;
        i += (int)n;
        nch++;
    }
    float part = ((float)kf->sub + kf->f) / (float)kf->nsub;
    int fill = (int)(part * (float)nch + 0.5f);
    int at = kf->off;
    for (int c = 0; c < fill; c++)
    {
        uint32_t cp;
        size_t n = tui_utf8_decode(text + at, (size_t)(kf->off + kf->len - at), &cp);
        if (n == 0)
            break;
        at += (int)n;
    }
    return at;
}

/* Where, in sub-pixels, the fill ends within the wrapped span `sp` of the big
 * line drawn from x0 at `em`; -inf: none of it, +inf: all of it. */
static float kfill_clip_x(const kfill *kf, ui_font *font, const char *text,
                          const ui_font_span *sp, float em, float x0)
{
    if (kf->state == KF_BEFORE)
        return -INFINITY;
    if (kf->state == KF_AFTER)
        return INFINITY;
    int so = sp->off, se = sp->off + sp->len;
    int uo = kf->off, ue = kf->off + kf->len;
    if (se <= uo)
        return INFINITY; /* all before the unit: sung */
    if (so >= ue)
        return -INFINITY; /* all after it */
    int a = uo > so ? uo : so;
    int b = ue < se ? ue : se;
    float xa = x0 + ui_font_width(font, text + so, (size_t)(a - so), em);
    float xb = x0 + ui_font_width(font, text + so, (size_t)(b - so), em);
    float lo = xa, hi = xb;
    if (kf->nsub > 1 && a == uo && b == ue)
    {
        lo = xa + (xb - xa) * (float)kf->sub / (float)kf->nsub;
        hi = xa + (xb - xa) * (float)(kf->sub + 1) / (float)kf->nsub;
    }
    return lo + (hi - lo) * kf->f;
}

/* A line as terminal text, one wrapped row at a time. `e` is its size: the
 * text fades out as the big form takes over. */
static void paint_small(const pctx *p, int line, float top, float h, float e,
                        bool cur, bool big_cap, const kfill *kf)
{
    ui_lyrics *l = p->l;
    tui_canvas *cv = p->cv;
    const ui_theme *t = p->t;
    int rows = rows_of(l, line);
    float vis = e <= 0.0f ? 1.0f : 1.0f - smooth(e / SMALL_FADE_OUT);

    /* a line taller than its text keeps the text in the middle. Halves round
     * down, as when the whole page was rounded to a row at once. */
    int base = (int)ceilf(top + (h - (float)rows) * 0.5f - 0.5f);

    for (int k = 0; vis > 0.01f && k < rows; k++)
    {
        int y = base + k;
        if (y < 0 || y >= p->H)
            continue;
        const seg *sg = &l->segs[l->first[line] + k];

        float edge = smooth(MATH_MIN((float)y, (float)(p->H - 1 - y)) / 2.5f);
        float f = cur ? 1.0f
                      : fade_at(fabsf((float)y - p->focus), p->sigma) * edge;
        color_t ink = cur ? p->cur_ink : t->text;
        color_t fg = color_mix(p->behind, ink, MIN_LEVEL + (1.0f - MIN_LEVEL) * f);
        fg = color_mix(p->behind, fg, vis);
        color_t dim_fg = fg;
        if (kf != NULL)
            kara_inks(p->behind, fg, &fg, &dim_fg);
        tui_pen pen = ui_pen(fg, p->skin ? COLOR_NONE : t->list.normal_bg);
        if (cur)
            pen.attrs |= TUI_BOLD;

        if (!p->skin)
            tui_canvas_fill(cv, (tui_rect){0, y, p->W, 1}, ' ', pen);

        if (sg->len > 0)
        {
            const char *full = line_text(p->m, (size_t)sg->line);
            const char *row = full + sg->off;
            int sung = sg->len;
            if (kf != NULL)
            {
                int fb = kfill_byte(kf, full);
                sung = MATH_CLAMP(fb - sg->off, 0, sg->len);
            }
            if (sung >= sg->len)
                tui_canvas_textn(cv, LEFT_PAD, y, row, (size_t)sg->len, pen);
            else
            {
                /* a second cue beside the colour: sung is bold, the rest not */
                tui_pen dim = pen;
                dim.fg = dim_fg;
                dim.attrs &= (uint16_t)~TUI_BOLD;
                if (sung > 0)
                    tui_canvas_textn(cv, LEFT_PAD, y, row, (size_t)sung, pen);
                tui_canvas_textn(cv, LEFT_PAD + tui_text_width(row, (size_t)sung),
                                 y, row + sung, (size_t)(sg->len - sung), dim);
            }
        }
        else if (cur && k == 0)
        {
            /* an instrumental gap: dots that light up as it runs out */
            int lit = gap_dots(l, p->m, p->ts);
            for (int d = 0; d < GAP_DOTS; d++)
                tui_canvas_put(cv, LEFT_PAD + d * 2, y,
                               d < lit ? 0x25CF : 0x25CB,
                               ui_pen(d < lit ? ink : t->primary_dim, pen.bg));
        }
    }

    /* the marker of the line being read, and of the one it is leaving */
    if (!cur && e <= 0.02f)
        return;
    float m_level = big_cap ? e : 1.0f;
    int m0, m1;
    if (big_cap)
    {
        float pad = 0.5f * BIG_PAD * e;
        m0 = (int)floorf(top + pad + 0.5f);
        m1 = (int)floorf(top + h - pad + 0.5f);
        if (m1 <= m0)
            m1 = m0 + 1;
    }
    else
    {
        m0 = base;
        m1 = base + rows;
    }
    color_t mc = color_mix(p->behind, t->primary, m_level);
    for (int y = MATH_MAX(m0, 0); y < MATH_MIN(m1, p->H); y++)
        tui_canvas_put(cv, 0, y, MARK, ui_pen(mc, p->skin ? COLOR_NONE : t->list.normal_bg));
}

/* A line as pixels, scaled to its current size, into the framebuffer. The
 * block of text keeps its centre in the middle of the line's height while
 * that grows; its colour goes from the ordinary text colour to the current
 * line's as it grows. */
static bool paint_big(const pctx *p, int line, float top, float h, float e,
                      const kfill *kf)
{
    ui_lyrics *l = p->l;
    const big *b = big_for(l, line);
    if (b == NULL || l->font == NULL)
        return false;

    float s = MATH_CLAMP(h / b->block, 0.0f, 1.0f);
    float em = b->em * s;
    if (em < 2.0f)
        return false;

    float asc, desc;
    ui_font_metrics(l->font, em, &asc, &desc);
    float pitch = (float)(b->rows * UI_SUBCELL_H) * s;
    float y0 = (top + h * 0.5f) * (float)UI_SUBCELL_H -
               (float)b->n * pitch * 0.5f;
    float x0 = (float)(LEFT_PAD * UI_SUBCELL_W);

    float shown = smooth((e - BIG_FADE_IN) / BIG_FADE_SPAN);
    if (shown < 0.03f)
        return false;
    color_t ink = color_mix(p->t->text, p->cur_ink, smooth(e));
    color_t col = color_mix(p->behind, ink, shown);
    color_t dim_col = col;
    if (kf != NULL)
        kara_inks(p->behind, col, &col, &dim_col);

    const char *text = line_text(p->m, (size_t)line);
    for (int k = 0; k < b->n; k++)
    {
        float box = y0 + (float)k * pitch;
        float baseline = box + (pitch - (asc + desc)) * 0.5f + asc;
        const char *sp = text + b->sp[k].off;
        size_t sl = (size_t)b->sp[k].len;
        float clip = kf ? kfill_clip_x(kf, l->font, text, &b->sp[k], em, x0)
                        : INFINITY;
        if (clip >= 1e8f)
            ui_font_draw(l->font, &l->fb, sp, sl, x0, baseline, em, col, 1.0f);
        else
        {
            /* still to be sung: dim; what has been sung: the full ink, up to
             * the clip */
            ui_font_draw(l->font, &l->fb, sp, sl, x0, baseline, em, dim_col,
                         1.0f);
            if (clip > x0)
                ui_font_draw_clip(l->font, &l->fb, sp, sl, x0, baseline, em,
                                  col, 1.0f, -1e9f, clip);
        }
    }
    return true;
}

static void lyrics_paint(tui_node *n, tui_canvas *cv, void *ud)
{
    (void)n;
    ui_lyrics *l = ud;
    ui_ctx *u = l->u;
    const ui_theme *t = &u->theme;
    int W = tui_canvas_width(cv), H = tui_canvas_height(cv);
    metadata_t *m = current_meta(u);
    bool skin = u->skin_active;
    ui_ambience_params ap = ui_ambience_params_of(&u->opt);
    float pulse = skin ? ui_ambience_pulse(&u->ambience, &ap) * ap.depth : 0.0f;

    l->hint_y = -1;
    if (m == NULL || l->nlines == 0 || l->meta != m)
    {
        const char *msg = "no lyrics";
        int w = tui_text_width(msg, strlen(msg));
        tui_canvas_text(cv, (W - w) / 2, H / 2, msg,
                        ui_pen(t->primary_dim, COLOR_NONE));
        return;
    }
    l->view_w = W;
    l->view_h = H;

    float pos = l->pos;
    pctx p = {
        .l = l,
        .cv = cv,
        .t = t,
        .m = m,
        .W = W,
        .H = H,
        .skin = skin,
        .sigma = MATH_MAX(4.0f, 0.34f * (float)H),
        .focus = l->synced && l->current >= 0 ? focus_centre(l) - pos
                                              : ANCHOR * (float)H,
        .behind = skin ? t->control_bg : t->list.normal_bg,
        .cur_ink = color_mix(t->primary, skin ? u->ambience.now.hot : t->primary,
                             pulse * 0.7f),
    };
    audio_source *src = app_current_source(u->app);
    if (src)
        p.ts = (int64_t)US2MS(src->timestamp);

    bool big_live = l->grow.n > 0 && l->font != NULL;
    if (big_live)
        ui_fb_resize(&l->fb, W, H);
    bool drew_big = false;

    int line = first_visible(l, pos);
    float y = line_y(l, line);
    for (; line < l->nlines; line++)
    {
        float h = line_h(l, line);
        float top = y - pos;
        y += h;
        if (top >= (float)H)
            break;
        if (top + h <= 0.0f)
            continue;

        bool cur = l->synced && line == l->current;
        float e = size_of(l, line);
        bool big_cap = big_live && (cur || e > 0.0f) && big_for(l, line);
        const kfill *kf = (cur && l->kf.valid && l->kf.line == line) ? &l->kf
                                                                       : NULL;
        paint_small(&p, line, top, h, e, cur, big_cap, kf);
        if (e > 0.0f)
            drew_big |= paint_big(&p, line, top, h, e, kf);
    }

    if (drew_big)
    {
        size_t need = (size_t)W * (size_t)H;
        if (need > l->grid_cap)
        {
            free(l->grid);
            l->grid = calloc(need, sizeof(ui_cell));
            if (l->grid == NULL)
                abort();
            l->grid_cap = need;
        }
        bool braille = u->opt.lyrics.big_braille;
        ui_fb_to_cells(&l->fb, l->grid,
                       braille ? UI_FB_BRAILLE : UI_FB_BLOCKS,
                       braille ? glyph_braille : glyph_blocks);
        for (int cy = 0; cy < H; cy++)
            for (int cx = 0; cx < W; cx++)
            {
                const ui_cell *c = &l->grid[(size_t)cy * (size_t)W + (size_t)cx];
                if (c->cp == 0)
                    continue;
                tui_canvas_put(cv, cx, cy, c->cp,
                               (tui_pen){c->fg, c->bg, c->reverse ? TUI_REVERSE : 0});
            }
    }

    if (l->manual && H > 1)
    {
        static const char hint[] = "\u21bb follow the music (zz)";
        int w = tui_text_width(hint, sizeof(hint) - 1);
        l->hint_y = H - 1;
        tui_canvas_fill(cv, (tui_rect){W - w - 2, H - 1, w + 2, 1}, ' ',
                        ui_pen(t->primary, t->control_bg));
        tui_canvas_text(cv, W - w - 1, H - 1, hint,
                        ui_pen(t->primary, t->control_bg));
    }
}

static bool lyrics_event(tui_node *n, tui_event *ev, void *ud)
{
    (void)n;
    ui_lyrics *l = ud;

    if (ev->type == TUI_EV_SCROLL)
    {
        ui_lyrics_scroll(l, ev->scroll_delta > 0 ? WHEEL_ROWS : -WHEEL_ROWS);
        return true;
    }
    if (ev->type != TUI_EV_MOUSE_DOWN || ev->button != 0)
        return false;

    if (l->manual && ev->ly == l->hint_y)
    {
        ui_lyrics_follow(l);
        return true;
    }

    /* a synced line: play from there */
    audio_source *src = app_current_source(l->u->app);
    if (!l->synced || src == NULL || src->duration <= 0 || l->meta == NULL)
        return false;
    int line = line_at_row(l, l->pos + (float)ev->ly + 0.5f);
    if (line < 0)
        return false;
    int64_t start_us = MS2US(l->meta->lyrics_synced[line].start_ms);
    ui_action_seek_to(l->u, (float)((double)start_us / (double)src->duration));
    ui_lyrics_follow(l);
    return true;
}

ui_lyrics *ui_lyrics_new(ui_ctx *u, int height)
{
    ui_lyrics *l = calloc(1, sizeof(*l));
    if (l == NULL)
        abort();
    l->u = u;
    l->current = -1;
    l->hint_y = -1;
    l->sig_top = l->sig_cur = l->sig_gap = l->sig_h = l->sig_w = -2;
    l->sig_rows = -1;
    l->sig_kf = -2;
    l->kline = -1;
    for (int i = 0; i < UI_GROW_MAX + 1; i++)
        l->bigs[i].line = -1;

    tui_attr a = {
        ._init = 0,
        .grow = 1,
        .bg = u->theme.list.normal_bg,
        .paint = lyrics_paint,
        .on_update = lyrics_update,
        .on_event = lyrics_event,
        .on_destroy = lyrics_free,
        .userdata = l,
    };
    if (height > 0)
        a.height = TUI_CELLS(height);

    l->node = tui__open(u->tui, TUI_KIND_BOX, &a);
    tui__close(u->tui);
    return l;
}

tui_node *ui_lyrics_node(const ui_lyrics *l)
{
    return l->node;
}
