# aplayer UI

The interface lives in `src/ui` and is built on the retained-mode toolkit in `src/tui`
(see `docs/TUI.md`). The rest of the program sees exactly one header, `src/include/ui.h`:

```c
ui_ctx *ui_create(app_instance *app);   // takes over the terminal, builds the screen
void    ui_event(ui_ctx *, const term_event *);
void    ui_frame(ui_ctx *);             // sync with app state, animate, render damage
bool    ui_quit_requested(const ui_ctx *);
void    ui_destroy(ui_ctx *);
```

## Layout

```
src/include/ui.h            public API (opaque handle)
src/ui/
  ui.c                      lifecycle, event routing, frame loop, screen skeleton
  ui_internal.h             private context (ui_ctx) + contracts between files
  ui_views.c                the tabs: registry, builders, show/hide
  ui_hotkeys.c              global keymap          -> calls ui_action_*
  ui_actions.c              what the user can ask for (play, seek, sort, ...)
  ui_theme.[ch]             every colour, as typed fields
  ui_options.[ch]           behaviour/layout knobs (scrolloff, VU style, ...)
  ui_format.[ch]            pure: durations -> "m:ss"
  ui_ansi.[ch]              pure: escape stream -> cell grid
  ui_palette.[ch]           pure: cover art -> colour palette
  ui_sprite.[ch]            pure: moving, overlapping pictures -> cell grid
  ui_ambience.[ch]          pure: palette cross-fade + music energy over time
  ui_skin.c                 applies the cover's palette to the theme (VISUAL only)
  widgets/
    ui_tab_bar.c            [1] PLAYLIST  [2] VISUAL  [3] METADATA
    ui_transport.c          time, seek bar, gain, status line, <<  []  >>
    ui_playlist.c           track list (cursor + scroll state live here)
    ui_vu_meter.c           level meters
    ui_lyrics.c             synced / plain lyrics, big current line, wrapped, auto-scroll / take over
    ui_cover_art.c          the cover art stack (all pictures of the track)
    ui_track_info.c         title / artists / album / technical line
    ui_spectrum.c           spectrum line (log frequency axis) or oscilloscope under the controls
```

`ui_internal.h` is private: CMake only puts `src/ui` on the include path of the UI's own
sources, so nothing else can include it.

## Data flow

The UI is a **consumer** of application state and owns all of its own.

```
audio analyzers ──> app->meters ─────┐
playback (utils.c) ─> app->track_serial ─┤  read every frame
playlist / mixer / sources ───────────┤
                                       v
                         widgets (on_update / paint)

keys, clicks ──> ui_hotkeys / widget events ──> ui_action_* ──> play_next(), mixer, playlist ...
```

* Nothing outside `src/ui` writes into UI state. `app_instance` has no UI member.
* When a new track starts, playback bumps `app->track_serial`. Widgets compare it with the value
  they last saw (playlist cursor follows the playing track, cover art reloads). No callbacks, no
  flags to poke, nothing to forget.
* Levels are plain fixed-size data (`app_meters`), written by the analyzers, read by the VU widget.

## Conventions

* **Widgets** are a toolkit node plus a private state struct, created by `ui_<name>_new(ui_ctx *)`
  under the current build cursor, freed from the node's `on_destroy`. They read `ui_ctx`
  (app, theme, options) and never reach into each other.
* Time-dependent state (easing, scrolling) advances in `on_update`; `paint` only draws state, so it
  may run any number of times. Call `tui_invalidate()` only when something visible changed.
* **Behaviour** goes in `ui_actions.c`, never in a widget callback or the keymap. Both mouse and
  keyboard call the same action.
* **Colours** are fields of `ui_theme`; **numbers** are fields of `ui_options`. No literals in widgets.

## Recipes

**New view (tab)**: add an id to `enum ui_view_id` (`ui_internal.h`), a builder in `ui_views.c`
that opens a box and returns its node, and a row in the `views` table. The tab bar, the `1`-`9`
keys and visibility handling pick it up automatically.

**New widget**: create `widgets/ui_foo.c` with `ui_foo_new()`, declare it in `ui_internal.h`, add
the file to `UI_SOURCES` in `CMakeLists.txt`, place it in a view.

**New hotkey**: add an `ui_action_*` if the behaviour is new, then one branch in `ui_hotkeys.c`.

**New colour / option**: add a field to `ui_theme` / `ui_options` and its default in the matching `.c`.

## The VISUAL page

An ambient "now playing" screen: the cover art on top with what is playing, the controls and a
spectrum line directly under it, the level meters at the left edge and the lyrics at the right, on
a flat page. Its colours and motion come from two sources:

* **The cover art** gives the palette. `ui_cover_art` keeps every picture of the track (shrunk once
  to 384 px), extracts a palette per picture (`ui_palette_extract`: dominant vivid hues, forced into
  a range that reads on a dark page) and exposes the front picture's palette. `ui_skin_sync` hands
  it to the ambience, which cross-fades; while the page is shown `ui_ctx.theme` is
  `ui_theme_derive(base_theme, palette)`, so *every* widget (VU meter, lyrics, tab bar, transport)
  follows the art without knowing it. Leaving the page restores `base_theme`. The page colour only
  moves with that fade, never with the music.
* **The music** gives the energy: the loudest channel of the activity analyzer (the "ACT" row of the
  VU meter), with instant attack and a configurable release. It is quantised to
  `ambience.levels` steps and limited to `ambience.pulse_hz` changes per second. The front card
  (`cover_art.bump`), the title and the current lyric line read the level; nothing else does.
  Paused / no source counts as silence (the analyzers stop reporting, their last value must not
  stick).

The middle column is as tall as the screen allows: the art takes every row that the track info,
the controls and the spectrum leave (`visual_resize` in `ui_views.c`), the rest hangs directly
under it. Side regions run the full height and are dropped when too narrow (lyrics first).

The spectrum: the FFT analyzer (Hann window, mono mix, amplitudes where 1.0 is a full-scale sine)
feeds `audio_spectrum_bands`, which reduces it to `APP_SPECTRUM_BANDS` levels on a log-frequency axis,
0..1 on a dB scale with a +4.5 dB/octave tilt (`audio_spectrum_defaults()`; change the dB range
there if the line sits too high or low). `ui_spectrum` maps the bands onto its braille columns,
eases them (`spectrum.attack_ms` / `release_ms`) and draws the line, plus a faint dotted area under
it (`spectrum.fill`).

The same widget has a second mode, an oscilloscope (`w` switches; `spectrum.mode` is the one shown at
start). `audio_analyzer_scope` hands every audio block to a callback in `utils.c`, which pushes its mono
mix into the circular buffer `app->scope` (`audio_scope.h`: one writer, one reader, no locks). Each
frame `ui_spectrum` reads the most recent `scope.window_ms` of it, shows the extremes of the samples
under each braille column (amplitude -> dot row, silence on the centre row), and moves the window's
right edge on at real-time speed, `scope.delay_ms` behind the newest sample, so the trace scrolls
left smoothly. `scope.auto_gain` scales the height to the recent peak (otherwise 1:1), `scope.gain` multiplies that, `scope.graticule` draws the dotted centre line.
Paused or nothing loaded fades the trace to a flat line. `test_audio_scope.c` covers the buffer.

The art stack: the front picture upright, the others behind it, each further up and right, tilted
alternately, dimmer and smaller. `[` / `]` (or a click / the wheel on the art) steps through them:
the front card sweeps out, drops behind the pile, the rest move up. A new track deals its stack in.
Cards are composited by `ui_sprite` (sub-pixel framebuffer, 2x4 per cell, alpha, rotation) rather
than `image_render()`, which is exhaustive per cell and cannot run per frame; both use the glyph
tables of `image_renderer.c` (`image_mask_to_glyph` / `image_mask_to_braille`). Cells a card does not
fully cover are left transparent so the glow shows around them.

Lyrics: synced lines follow the playback position; plain lyrics scroll at `lyrics.speed` rows per
second. The wheel, `j`/`k` and the page keys take over; `zz` or a click on the hint hands control
back (`lyrics.resume_s` can do it after a while). A click on a synced line seeks to it. Long lines
wrap; rows fade with their distance from the reading position into whatever is behind them.

**The big line.** The line being sung is drawn large. Terminal text has one size, so it is not text:
`ui_font` (a wrapper around FreeType, fetched in `CMakeLists.txt`) rasterizes it, hinted and monochrome, into the same
sub-pixel framebuffer the cover art uses (`ui_fb`, 2x4 per cell), `ui_fb_to_cells()` fits that to
braille (or block) glyphs, and the widget paints the cells over the page. Because it is pixels it can
be any size, and that is what makes the change from one line to the next smooth:

* `ui_grow` gives every line a size 0..1. The new line grows from 0 while the old one shrinks back,
  eased with a symmetric curve, so for a moment the two share the size (`lyrics.grow_ms`).
* A line's height is its wrapped rows blended towards its big block by that size, and everything below
  it is pushed along: the page *moves* instead of jumping. Positions are therefore fractional rows
  ("stretched space", see the comment above `line_h` in `ui_lyrics.c`).
* The reading position is the size-weighted mean of the lines in transition, each taken at the centre
  it has when fully grown (`focus_centre`). Every line's place on screen is then linear in the sizes,
  so the page moves one way through a transition. The position is not eased further while a line
  grows (an eased position lags the stretch and the lines below swing past their mark and back);
  only the part that can jump - the centre of the current line, `base` - glides. A resize, a manual
  scroll or a size change unlock it, and it glides back to the music. `tests/test_ui_lyrics.c` runs
  the real widget frame by frame with a fake toolkit and checks this.
* A part-grown line is cross-faded: the terminal text is gone by size 0.3, the pixel text, scaled to
  the line's current size, appears after that. They hardly overlap, because scaled-down pixel text is
  a smudge and its solid cells would paint over the terminal text beneath.
* Big text is only as big as is legible. `lyrics.big_rows` is at most 16 and can be changed while playing: `+` / `-` one row taller / shorter, `=` back to the default (cached layouts are rebuilt). Braille needs 3 rows per text line, blocks 4; lines with
* Word timing: `lyrics.karaoke` (default on) fills the line being sung syllable by syllable; `lyrics.karaoke_offset_ms` shifts it. Worked out from the audio in the background, see [KARAOKE.md](KARAOKE.md).
  full-width characters (CJK, kana, Hangul) 5 and 6. A line that does not fit in `BIG_MAX_SHARE` of
  the height at that size simply stays small. Without a font, or with `lyrics.big_rows` 0, nothing
  changes at all: every line keeps its wrapped height and the page behaves as before.

*The font* is, in order: `lyrics.font`, the file in `APLAYER_LYRICS_FONT`, the files of the fonts folder
(`APLAYER_FONT_DIR`, `fonts/` next to the executable, `./fonts`; sorted by name, see `fonts/README.md`),
the bold sans fonts the system usually has (DejaVu, Liberation, Noto; Segoe UI / Arial on Windows), and
last GNU Unifont (a `unifont*` file in the fonts folder, else the system's copy). Files after the first
are fallbacks for characters it lacks and are only opened when a lyric needs them; FreeType reads them
from disk as needed. `.ttc` collections, CFF/OpenType and variable fonts work; `lyrics.weight` (700)
sets a variable font's weight axis, static fonts keep theirs, so prefer Bold files. Glyphs are hinted,
monochrome and placed on whole pixels (stb_truetype's unhinted rendering fell apart at about 13
sub-pixels). Invisible characters (variation selectors, joiners, zero-width spaces) take no room.

## Input and search

Keybindings, focus layers, IME handling and the `/` fuzzy search are described in [INPUT.md](INPUT.md).

## Tests

`tests/test_ui_format.c`, `test_ui_ansi.c`, `test_ui_palette.c`, `test_ui_sprite.c`,
`test_ui_font.c`, `test_ui_grow.c`, `test_ui_lyrics.c`, `test_ui_ambience.c`, `test_audio_spectrum.c` and `test_color.c` cover the pure helpers
(`python3 test.py tests/test_ui_palette.c`). Widgets are verified end-to-end by driving the binary
in a pty and comparing the emulated screen.

## Known quirks (kept from the previous UI)

* F1 cycles all four image render methods, but sixel and kitty are drawn as glyphs (they bypass the
  cell grid), so two of the four presses look identical.
* The METADATA tab currently shows only lyrics.
* The big lyric line needs a font file on the machine; without one it silently stays small. The first
  big line opens the primary font, the first CJK lyric a fallback, each once, on the UI thread.
* Test builds of `test_ui_font.c` / `test_ui_lyrics.c` need FreeType (found with `pkg-config`) and, for
  the latter, the ffmpeg and PortAudio headers.
* The VISUAL page assumes cells about twice as tall as wide (as the glyph renderer does).
* A fresh palette fade repaints a large area; on slow links set `ambience.fade_ms` to 0. Beats
  only repaint the art, title and current lyric (`ambience.levels` 0 turns that off).
* Playlist-level analyzers in `app.c` are disabled; only per-source meters are fed.
