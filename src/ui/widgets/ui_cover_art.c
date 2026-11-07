#include "ui_internal.h"

#include "audio_source.h"
#include "image.h"
#include "image_renderer.h"
#include "ui_sprite.h"

#include <libavutil/mem.h>
#include <libswscale/swscale.h>
#include <math.h>
#include <pthread.h>
#include <string.h>

/*
 * The cover art of the playing track, as a stack of cards.
 *
 * Every picture attached to the track is a card. The one in front is drawn
 * upright; the others sit behind it, each a little further up and to the
 * right, tilted the other way, dimmer and smaller, like a pile of sleeves.
 * Switching pictures plays a short animation: the front card sweeps out to the
 * left, drops behind the pile and the others move up one place (previous does
 * it in reverse). A fresh stack is dealt in from the right.
 *
 * Because the cards move and overlap they cannot go through image_render()
 * (one still picture, exhaustive and opaque): they are composited per frame by
 * ui_sprite.h into cells and painted through the canvas, so whatever the page
 * paints underneath shows around them.
 *
 * Pictures are shrunk once, when loaded, to WORK_PX and the full-size decode is
 * dropped; the front card's palette is what the whole page takes its colours
 * from (see ui_skin.c).
 *
 * The art is (re)loaded lazily, the first time the widget is visible after the
 * track changed (observed via app->track_serial). Decoding, shrinking and the
 * palette take tens to hundreds of milliseconds for big pictures, so they run
 * on a worker thread; the UI thread only copies the compressed bytes and
 * installs the result when it is ready.
 */

#define WORK_PX      384 /* longest side of the kept picture */
#define MAX_COVERS   16
#define VISIBLE_BACK 3   /* cards shown behind the front one */

/* the pile: where the k-th card behind the front one sits (fractions of the
 * card's side) */
#define SLOT_DX      0.11f
#define SLOT_DY      (-0.065f)
#define SLOT_SCALE   0.035f
#define SLOT_SHADE   0.22f
#define CORNER       0.045f

/* the sweep of the card that changes places */
#define SWEEP_X      (-0.66f)
#define SWEEP_LIFT   0.05f
#define SWEEP_TURN   (-0.30f) /* radians */
#define SWEEP_SPLIT  0.5f     /* progress at which it passes behind the pile */

#define DEAL_STAGGER 0.18f

typedef struct cover
{
    image_t img; /* img.repr is the picture kept; img.data was released */
    ui_palette pal;
} cover;

typedef struct pose
{
    float x, y, turn, scale, shade, alpha;
} pose;

enum
{
    LOAD_IDLE,
    LOAD_RUNNING,
    LOAD_DONE,
};

struct ui_cover_art
{
    ui_ctx *u;
    tui_node *node;

    enum image_render_method method; /* as requested by the user */

    uint32_t track_serial;
    bool loaded; /* the track's pictures have been looked up */

    /* loader: one worker at a time, started from the UI thread, finished when
     * `state` reaches LOAD_DONE (then joined and installed) */
    pthread_t worker;
    volatile int state; /* LOAD_IDLE / LOAD_RUNNING / LOAD_DONE */
    uint32_t job_serial;
    array(art_blob) job_blobs;
    cover job_covers[MAX_COVERS];
    int job_n, job_keep;
    cover covers[MAX_COVERS];
    int n;

    int front; /* card in front when still */
    int goal;  /* card that will be, once the animation is done */
    int dir;   /* +1 next, -1 previous: direction of the running animation */
    int pending; /* further steps queued while animating */
    float t;     /* progress of the running animation, 1: still */
    float deal;  /* progress of dealing the stack in, 1: done */
    uint32_t rev; /* changes whenever `goal` or the set of pictures does */

    ui_fb fb;
    ui_cell *grid;
    int gw, gh; /* cells */
    uint8_t blank[3]; /* stands in when the track has no picture */

    ui_sprite drawn[MAX_COVERS]; /* last frame, back to front */
    int ndrawn;

    bool dirty;
    int built_w, built_h, built_level;
    float built_fade;
    enum image_render_method built_method;
};

/* ------------------------------------------------------------------------ */
/* easing and poses                                                         */
/* ------------------------------------------------------------------------ */

static float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float ease_out(float t)
{
    t = MATH_CLAMP(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

static float ease_in_out(float t)
{
    t = MATH_CLAMP(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static float ease_out_back(float t)
{
    t = MATH_CLAMP(t, 0.0f, 1.0f);
    const float c1 = 1.2f, c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

static pose pose_lerp(pose a, pose b, float t)
{
    return (pose){lerpf(a.x, b.x, t),         lerpf(a.y, b.y, t),
                  lerpf(a.turn, b.turn, t),   lerpf(a.scale, b.scale, t),
                  lerpf(a.shade, b.shade, t), lerpf(a.alpha, b.alpha, t)};
}

/* Place k (0: front) of the pile; k may be fractional while cards move. The
 * tilt alternates from card to card so the pile fans out instead of leaning. */
static pose slot_pose(float k)
{
    static const float TILT[] = {0.0f, 0.11f, -0.16f, 0.21f, -0.26f};
    k = MATH_MAX(k, 0.0f);
    int lo = MATH_MIN((int)k, VISIBLE_BACK);
    int hi = MATH_MIN(lo + 1, VISIBLE_BACK + 1);
    float f = MATH_MIN(k - (float)lo, 1.0f);
    return (pose){
        .x = SLOT_DX * k,
        .y = SLOT_DY * k,
        .turn = lerpf(TILT[lo], TILT[hi], f),
        .scale = 1.0f - SLOT_SCALE * k,
        .shade = MATH_MIN(SLOT_SHADE * k, 0.85f),
        /* the card that is about to join the pile fades in */
        .alpha = MATH_CLAMP((float)VISIBLE_BACK + 1.0f - k, 0.0f, 1.0f),
    };
}

/* The card that changes places, p = 0 in front .. 1 at the back of the pile:
 * out to the left and up a little, then behind the pile and into its slot. */
static pose sweep_pose(float p, float back_k)
{
    float out = ease_out(p / SWEEP_SPLIT);
    pose gone = {.x = SWEEP_X,
                 .y = 0.0f,
                 .turn = SWEEP_TURN,
                 .scale = 1.0f,
                 .shade = 0.0f,
                 .alpha = 1.0f};
    if (p < SWEEP_SPLIT)
    {
        pose a = slot_pose(0.0f);
        pose b = pose_lerp(a, gone, out);
        b.y -= SWEEP_LIFT * sinf((float)M_PI * p / SWEEP_SPLIT);
        b.scale += 0.03f * sinf((float)M_PI * p / SWEEP_SPLIT);
        return b;
    }
    return pose_lerp(gone, slot_pose(back_k),
                     ease_in_out((p - SWEEP_SPLIT) / (1.0f - SWEEP_SPLIT)));
}

/* ------------------------------------------------------------------------ */
/* pictures                                                                 */
/* ------------------------------------------------------------------------ */

static void drop_covers(ui_cover_art *a)
{
    for (int i = 0; i < a->n; i++)
        image_free(&a->covers[i].img);
    memset(a->covers, 0, sizeof(a->covers));
    a->n = 0;
}

static void *load_worker(void *arg)
{
    ui_cover_art *a = arg;
    art_blob *b;
    a->job_n = 0;
    ARR_FOREACH_BYREF(a->job_blobs, b, i)
    {
        cover *c = &a->job_covers[a->job_n];
        imgconv_frame f = {0};
        if (a->job_n < a->job_keep)
            f = imgconv_decode(b->data, b->size, AV_CODEC_ID_NONE,
                               AV_PIX_FMT_RGB24);
        if (f.buffer != NULL)
        {
            image_t img = image_from_frame(&f);
            img.title = b->title;
            img.comment = b->comment;
            b->title = b->comment = NULL; /* the picture owns them now */
            /* shrink once; the decoded original can be many megabytes */
            if (image_resize(&img, SWS_AREA, WORK_PX, WORK_PX) == 0 &&
                ui_palette_extract(img.repr->data, img.repr->width,
                                   img.repr->height, &c->pal))
            {
                av_free(img.data);
                img.data = NULL;
                c->img = img;
                a->job_n++;
            }
            else
                image_free(&img);
        }
        free(b->data);
        free(b->title);
        free(b->comment);
    }
    array_free(&a->job_blobs);
    a->state = LOAD_DONE;
    return NULL;
}

static void install_covers(ui_cover_art *a)
{
    drop_covers(a);
    for (int i = 0; i < a->job_n; i++)
        a->covers[i] = a->job_covers[i];
    a->n = a->job_n;
    memset(a->job_covers, 0, sizeof(a->job_covers));
    a->job_n = 0;

    a->front = a->goal = 0;
    a->dir = 0;
    a->pending = 0;
    a->t = 1.0f;
    a->deal = 0.0f;
    a->loaded = true;
    a->dirty = true;
    a->rev++;
}

/* Copies the compressed pictures of `src` and starts decoding them. */
static void start_load(ui_cover_art *a, audio_source *src)
{
    a->job_blobs = array_create(4, sizeof(art_blob));
    if (src->get_art_blobs)
        src->get_art_blobs(src, &a->job_blobs);
    a->job_serial = a->track_serial;
    a->job_keep = MATH_MIN(a->u->opt.cover_art.max_covers, MAX_COVERS);
    a->state = LOAD_RUNNING;
    if (pthread_create(&a->worker, NULL, load_worker, a) != 0)
    {
        /* no thread: do it here */
        load_worker(a);
        pthread_join(a->worker, NULL);
        a->state = LOAD_DONE;
        a->worker = (pthread_t){0};
    }
}

/* Called every frame: finishes a load that is done and, if the track moved on
 * meanwhile, drops it and starts over. Returns true while still busy. */
static bool pump_loader(ui_cover_art *a)
{
    if (a->state == LOAD_RUNNING)
        return true;
    if (a->state != LOAD_DONE)
        return false;
    pthread_join(a->worker, NULL);
    a->state = LOAD_IDLE;
    if (a->job_serial == a->track_serial)
        install_covers(a);
    else
    {
        for (int i = 0; i < a->job_n; i++)
            image_free(&a->job_covers[i].img);
        memset(a->job_covers, 0, sizeof(a->job_covers));
        a->job_n = 0;
    }
    return false;
}

bool ui_cover_art_palette(const ui_cover_art *a, ui_palette *out,
                          uint32_t *rev)
{
    *rev = a->rev;
    if (a->n == 0)
        return false;
    *out = a->covers[a->goal].pal;
    return true;
}

void ui_cover_art_next_method(ui_cover_art *a)
{
    a->method =
        (enum image_render_method)((a->method + 1) % IMAGE_RENDER_LENGTH);
}

static enum image_render_method effective_method(const ui_cover_art *a)
{
    return (a->method == IMAGE_RENDER_SIXEL || a->method == IMAGE_RENDER_TGP)
               ? IMAGE_RENDER_GLYPH
               : a->method;
}

static uint32_t glyph_blocks(uint8_t mask)
{
    return (uint32_t)image_mask_to_glyph(mask);
}

static uint32_t glyph_braille(uint8_t mask)
{
    return (uint32_t)image_mask_to_braille(mask);
}

/* ------------------------------------------------------------------------ */
/* animation                                                                */
/* ------------------------------------------------------------------------ */

static void start_step(ui_cover_art *a, int dir)
{
    a->dir = dir;
    a->goal = (a->front + dir + a->n) % a->n;
    a->t = 0.0f;
    a->rev++; /* the page starts moving towards this picture's colours */
    a->dirty = true;
}

void ui_cover_art_step(ui_cover_art *a, int dir)
{
    if (a->n < 2 || dir == 0)
        return;
    dir = dir > 0 ? 1 : -1;

    if (a->t >= 1.0f && a->deal >= 1.0f)
        start_step(a, dir);
    else
        a->pending = MATH_CLAMP(a->pending + dir, -a->n, a->n);
}

static void advance(ui_cover_art *a, double dt)
{
    float ms = (float)MATH_MAX(a->u->opt.cover_art.anim_ms, 1);

    if (a->deal < 1.0f)
    {
        a->deal = MATH_MIN(a->deal + (float)dt * 1000.0f / (ms * 1.3f), 1.0f);
        a->dirty = true;
    }
    if (a->t < 1.0f)
    {
        /* hurry when more steps are waiting */
        float speed = a->pending != 0 ? 1.8f : 1.0f;
        a->t = MATH_MIN(a->t + (float)dt * 1000.0f * speed / ms, 1.0f);
        a->dirty = true;
        if (a->t >= 1.0f)
        {
            a->front = a->goal;
            a->dir = 0;
            if (a->pending != 0)
            {
                int dir = a->pending > 0 ? 1 : -1;
                a->pending -= dir;
                start_step(a, dir);
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/* composing                                                                */
/* ------------------------------------------------------------------------ */

typedef struct placed
{
    int index;
    pose p;
    float depth; /* larger = further back */
} placed;

/* Where every card is right now (stills, sweeps and the deal-in). */
static int place_all(const ui_cover_art *a, placed out[MAX_COVERS])
{
    int count = 0;
    float back_k = (float)MATH_MIN(a->n - 1, VISIBLE_BACK);
    int moving = -1; /* the card that sweeps */
    if (a->t < 1.0f)
        moving = a->dir > 0 ? a->front : a->goal;

    float s = a->t;
    float shift = ease_in_out((s - 0.1f) / 0.9f);

    for (int i = 0; i < a->n; i++)
    {
        int k0 = (i - a->front + a->n) % a->n; /* place when the step began */
        placed pl = {.index = i};

        if (i == moving)
        {
            float p = a->dir > 0 ? s : 1.0f - s;
            pl.p = sweep_pose(p, back_k);
            pl.depth = p < SWEEP_SPLIT ? -1.0f : (float)a->n + 1.0f;
        }
        else
        {
            float k = (float)k0;
            if (a->t < 1.0f)
                k += (float)(-a->dir) * shift;
            pl.p = slot_pose(k);
            pl.depth = k;
        }

        if (a->deal < 1.0f)
        {
            /* arriving: the back cards first, the front one last */
            float steps = DEAL_STAGGER * (float)(a->n > 1 ? back_k : 0.0f);
            float k = pl.depth < 0.0f ? 0.0f : MATH_MIN(pl.depth, back_k);
            float local = MATH_CLAMP(a->deal * (1.0f + steps) -
                                         DEAL_STAGGER * (back_k - k),
                                     0.0f, 1.0f);
            pose from = {.x = 0.85f, .y = 0.30f, .turn = 0.45f,
                         .scale = 1.0f, .shade = 0.0f, .alpha = 0.0f};
            pose to = pl.p;
            pl.p = pose_lerp(from, to, ease_out_back(local));
            pl.p.alpha = to.alpha * ease_out(local);
        }
        out[count++] = pl;
    }

    /* back to front */
    for (int i = 1; i < count; i++)
        for (int j = i; j > 0 && out[j].depth > out[j - 1].depth; j--)
        {
            placed tmp = out[j];
            out[j] = out[j - 1];
            out[j - 1] = tmp;
        }
    return count;
}

static void compose(ui_cover_art *a, int cells_w, int cells_h)
{
    ui_ctx *u = a->u;
    const ui_palette *pal = &u->ambience.now;
    ui_ambience_params ap = ui_ambience_params_of(&u->opt);

    ui_fb_resize(&a->fb, cells_w, cells_h);
    a->ndrawn = 0;
    float W = (float)a->fb.w, H = (float)a->fb.h;

    placed list[MAX_COVERS];
    int count;
    if (a->n == 0)
    {
        /* no picture: a quiet card, so the page keeps its shape */
        color_t c = color_mix(pal->bg, pal->accent, 0.22f);
        a->blank[0] = c.r;
        a->blank[1] = c.g;
        a->blank[2] = c.b;
        float side = MATH_MIN(H * 0.86f, W * 0.70f);
        ui_fb_draw(&a->fb, &(ui_sprite){.rgb = a->blank, .w = 1, .h = 1,
                                        .cx = W * 0.5f, .cy = H * 0.5f,
                                        .width = side, .height = side,
                                        .radius = side * CORNER,
                                        .alpha = 1.0f});
        count = 0;
    }
    else
        count = place_all(a, list);

    int behind = MATH_MIN(a->n - 1, VISIBLE_BACK);
    float side = a->n > 1 ? MATH_MIN(H * 0.80f, W * 0.70f)
                          : MATH_MIN(H * 0.96f, W * 0.96f);
    /* centre the whole pile, not just its front card */
    float cx = W * 0.5f - SLOT_DX * side * (float)MATH_MAX(behind, 0) * 0.5f;
    float cy = H * 0.5f - SLOT_DY * side * (float)MATH_MAX(behind, 0) * 0.5f;

    float pulse = ui_ambience_pulse(&u->ambience, &ap) * ap.depth;
    float bump = u->opt.cover_art.bump * pulse;

    for (int i = 0; i < count; i++)
    {
        const cover *c = &a->covers[list[i].index];
        const pose *p = &list[i].p;
        const image_t *pic = c->img.repr;
        float aspect = (float)pic->width / (float)pic->height;
        float w = aspect >= 1.0f ? side : side * aspect;
        float h = aspect >= 1.0f ? side / aspect : side;
        float grow = list[i].depth >= 0.0f && list[i].depth < 0.5f
                         ? bump
                         : 0.0f;

        ui_sprite s = {
            .rgb = pic->data,
            .w = pic->width,
            .h = pic->height,
            .cx = cx + p->x * side,
            .cy = cy + p->y * side,
            .width = (w + grow) * p->scale,
            .height = (h + grow) * p->scale,
            .angle = p->turn,
            .radius = side * CORNER,
            .shade = p->shade,
            .fog = pal->bg,
            .alpha = p->alpha,
        };
        ui_fb_draw(&a->fb, &s);
        a->drawn[a->ndrawn++] = s;
    }

    enum image_render_method m = effective_method(a);
    free(a->grid);
    a->gw = cells_w;
    a->gh = cells_h;
    a->grid = calloc((size_t)cells_w * (size_t)cells_h, sizeof(ui_cell));
    if (a->grid == NULL)
        abort();
    ui_fb_to_cells(&a->fb, a->grid,
                   m == IMAGE_RENDER_BRAILLE ? UI_FB_BRAILLE : UI_FB_BLOCKS,
                   m == IMAGE_RENDER_BRAILLE ? glyph_braille : glyph_blocks);
}

/* ------------------------------------------------------------------------ */
/* toolkit hooks                                                            */
/* ------------------------------------------------------------------------ */

static void cover_free(tui_node *n, void *ud)
{
    (void)n;
    ui_cover_art *a = ud;
    if (a->state != LOAD_IDLE)
    {
        pthread_join(a->worker, NULL);
        for (int i = 0; i < a->job_n; i++)
            image_free(&a->job_covers[i].img);
    }
    drop_covers(a);
    ui_fb_free(&a->fb);
    free(a->grid);
    free(a);
}

/* the row under the art that tells which picture is in front */
static int caption_rows(const ui_cover_art *a, int h)
{
    return a->n > 1 && h >= 8 ? 1 : 0;
}

static void cover_update(tui_node *n, double dt, void *ud)
{
    ui_cover_art *a = ud;
    ui_ctx *u = a->u;
    app_instance *app = u->app;

    if (a->track_serial != app->track_serial)
    {
        a->track_serial = app->track_serial;
        a->loaded = false;
    }

    bool busy = pump_loader(a);
    if (!a->loaded && !busy)
    {
        audio_source *src = app_current_source(app);
        if (src == NULL)
            return;
        a->loaded = true; /* until the track changes again */
        start_load(a, src);
    }
    if (busy || a->state == LOAD_RUNNING)
        return; /* the art of the previous track stays until the new is ready */

    advance(a, dt);

    tui_rect c = tui_get_content_rect(n);
    if (c.w <= 0 || c.h <= 0)
        return;

    bool bumps = u->opt.cover_art.bump > 0.0f && a->n > 0;
    if (c.w != a->built_w || c.h != a->built_h ||
        effective_method(a) != a->built_method ||
        u->ambience.fade_shown != a->built_fade ||
        (bumps && u->ambience.level != a->built_level))
        a->dirty = true;

    if (!a->dirty)
        return;

    compose(a, c.w, c.h - caption_rows(a, c.h));
    a->built_w = c.w;
    a->built_h = c.h;
    a->built_method = effective_method(a);
    a->built_fade = u->ambience.fade_shown;
    a->built_level = u->ambience.level;
    a->dirty = false;
    tui_invalidate(n);
}

static void paint_caption(const ui_cover_art *a, tui_canvas *cv, int y)
{
    const ui_theme *th = &a->u->theme;
    int W = tui_canvas_width(cv);
    const char *title = a->covers[a->goal].img.title;
    if (title && !title[0])
        title = NULL;

    /* "● ○ ○  title", centred as a whole */
    int dots = a->n * 2 - 1;
    int tw = title ? tui_text_width(title, strlen(title)) : 0;
    int total = dots + (tw ? 2 + tw : 0);
    if (total > W)
        total = dots, title = NULL;
    int x = (W - total) / 2;

    for (int i = 0; i < a->n; i++)
        tui_canvas_put(cv, x + i * 2, y, i == a->goal ? 0x25CF : 0x25CB,
                       ui_pen(i == a->goal ? th->primary : th->primary_dim,
                              COLOR_NONE));
    if (title)
        tui_canvas_text(cv, x + dots + 2, y, title,
                        ui_pen(th->primary_dim, COLOR_NONE));
}

static void cover_paint(tui_node *n, tui_canvas *cv, void *ud)
{
    (void)n;
    ui_cover_art *a = ud;
    if (a->grid == NULL)
        return;

    for (int y = 0; y < a->gh; y++)
        for (int x = 0; x < a->gw; x++)
        {
            const ui_cell *c = &a->grid[y * a->gw + x];
            if (c->cp == 0)
                continue;
            tui_pen pen = {c->fg, c->bg, c->reverse ? TUI_REVERSE : 0};
            tui_canvas_put(cv, x, y, c->cp, pen);
        }

    int H = tui_canvas_height(cv);
    if (a->n == 0)
    {
        int W = tui_canvas_width(cv);
        tui_canvas_text(cv, W / 2 - 1, a->gh / 2, "\u266a",
                        ui_pen(a->u->theme.primary_dim, COLOR_NONE));
    }
    else if (caption_rows(a, H))
        paint_caption(a, cv, H - 1);
}

static bool cover_event(tui_node *n, tui_event *ev, void *ud)
{
    (void)n;
    ui_cover_art *a = ud;

    if (ev->type == TUI_EV_SCROLL)
    {
        ui_action_cover_step(a->u, ev->scroll_delta > 0 ? 1 : -1);
        return true;
    }
    if (ev->type != TUI_EV_MOUSE_DOWN)
        return false;

    /* a click on a card: forward on the front one or any in the pile, back
     * with the right button; clicks beside the pile do nothing */
    float px = (float)ev->lx * UI_SUBCELL_W + UI_SUBCELL_W * 0.5f;
    float py = (float)ev->ly * UI_SUBCELL_H + UI_SUBCELL_H * 0.5f;
    for (int i = a->ndrawn - 1; i >= 0; i--)
        if (ui_sprite_contains(&a->drawn[i], px, py))
        {
            ui_action_cover_step(a->u, ev->button == 2 ? -1 : 1);
            return true;
        }
    return false;
}

ui_cover_art *ui_cover_art_new(ui_ctx *u)
{
    ui_cover_art *a = calloc(1, sizeof(*a));
    if (a == NULL)
        abort();
    a->u = u;
    a->method = u->opt.cover_art.method;
    a->t = 1.0f;
    a->deal = 1.0f;

    a->node = tui__open(u->tui, TUI_KIND_BOX,
                        &(tui_attr){._init = 0,
                                    .grow = 1,
                                    .paint = cover_paint,
                                    .on_update = cover_update,
                                    .on_event = cover_event,
                                    .on_destroy = cover_free,
                                    .userdata = a});
    tui__close(u->tui);
    return a;
}
