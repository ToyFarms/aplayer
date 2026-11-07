# aplayer TUI

A retained-mode, flexbox-layout terminal UI library in C17.
Files: `src/include/tui.h` (core), `tui_widgets.h` (elements), `tui_term.h` (term_event adapter);
implementation in `src/tui/`.

## Model

Everything is a **node** in a tree. Two primitives, like HTML:

| tui           | html     | notes                                   |
|---------------|----------|-----------------------------------------|
| `TUI_BOX`/`TUI_DIV`   | `<div>`  | flex container, draws bg/border |
| `TUI_TEXT`/`TUI_SPAN` | `<span>` | wrapping / ellipsis / aligned text leaf |

Elements (`tui_widgets.h`) are boxes/text with callbacks attached:
`hr, vr, spacer, button, input, checkbox, progress, slider, scroll`.

Build declaratively (`TUI_BOX(ctx, .dir = TUI_COLUMN) { ... }`) or imperatively
(`tui_node_new`, `tui_append`, `tui_set_attr`, `tui_detach`, `tui_node_free`). The tree stays
alive; mutate it any time. `TUI_UNDER(ctx, node)` builds into an existing node.

`tui_attr` is a flat struct whose zero value is the CSS default; set only what you need.
Read-modify-write a live node with `tui_get_attr` / `tui_set_attr`; only what changed is
invalidated.

## Callbacks (the extension points)

All receive the node's `userdata`.

| field        | when                                                         |
|--------------|--------------------------------------------------------------|
| `paint`      | draw custom content *before* children; canvas = content box |
| `paint_over` | draw *after* children (scrollbars, overlays)                 |
| `on_event`   | mouse/key/focus events, bubbling; return true to consume     |
| `on_update`  | once per `tui_update(ctx, dt)` for visible nodes (animation) |
| `on_resize`  | content box changed size (and after first layout)            |
| `on_destroy` | before the node is freed: release your state                 |
| `measure`    | intrinsic size of a leaf with custom content                 |

Call `tui_invalidate(node)` / `tui_invalidate_rect(node, r)` when your paint output changes;
that is the only thing a custom widget has to do to stay correct.

### Writing a widget

```c
static void meter_paint(tui_node *n, tui_canvas *c, void *ud) { /* tui_canvas_put/text/fill */ }
static void meter_tick (tui_node *n, double dt, void *ud)     { /* update state */ tui_invalidate(n); }
TUI_BOX(ctx, .height = TUI_CELLS(1), .grow = 1, .paint = meter_paint, .on_update = meter_tick, .userdata = st);
```
`src/tui/tui_widgets*.c` use only the public API and are the reference implementations.

## Layout

Integer-cell flexbox, border-box: direction/reverse, wrap, justify, align-items/self/content,
grow/shrink/basis, width/height/min/max (cells, percent, auto), margin/padding/border, gap,
`display:none`, `position:absolute`, scroll offsets (`.scroll` / `tui_scroll_begin`).
Definite-size items keep `min(specified, content)` (CSS `min-height:auto`), so fixed bars are not
squeezed by tall siblings.

## Minimal redraw

1. Mutations mark nodes dirty; layout is invalidated only up to the nearest fixed-size boundary.
2. `tui_render` re-lays-out only dirty subtrees; moved/resized nodes emit old+new damage rects.
3. Paint-only changes (colour, text of same size, custom paint) add just their rect as damage.
4. Damage is coalesced, widened for wide glyphs, repainted into a back buffer.
5. Per damaged row span, back is diffed against front; only changed cells are emitted
   (cursor/SGR state tracked, short gaps bridged), in one `write`, optionally in sync-output mode.

A frame with nothing dirty costs O(1) and emits zero bytes.

## Events

`tui_dispatch` takes `tui_event`s (use `tui_term.h` to convert aplayer `term_event`s).
Hit testing respects scroll and absolute children; handlers bubble; hover enter/leave follow the
ancestor chain (like `:hover`, see `tui_is_hovered`); a pressed node captures drag/release;
Tab/Shift-Tab move focus between `focusable` nodes.

### Input router and cursor

`tui_router` (keymaps, modes, focus layers, langmap), `TUI_EV_TEXT`, `tui_set_key_hook` and the hardware-cursor API (`tui_set_cursor`) are described in [INPUT.md](INPUT.md).

## Using it

```c
tui_ctx *ctx = tui_create(w, h, &(tui_options){.sync_output = true});
tui_terminal_enter(ctx, false);          // mouse comes from the term layer
build_ui(ctx);
for (;;) {
    tui_term_pump(&in, ctx, &queue);     // input
    tui_update(ctx, dt);                 // on_update hooks
    tui_render(ctx);                     // layout + paint + diff + write
}
```
See `src/ui/` (documented in `docs/UI.md`) and `src/main.c` for a full application.

## Verification

`tests/tui_fuzz.h` is a differential fuzzer: random mutations, then checks terminal emulator ==
front buffer, back == front, incremental layout == from-scratch layout, incremental paint == full
repaint (ASan/UBSan clean over hundreds of thousands of frames). Run with
`tui_fuzz_run(seeds, steps)`. The UI was smoke-tested in a pty.

## Known limitations

- No auto margins, baseline alignment, wrap-reverse.
- Zero-width / combining characters are dropped.
- Everything clips to its parent.
- Detach/re-attach relays out the subtree: prefer `display:none` for tabs.
- Hit test / paint iterate all children (O(children)); no virtualisation for huge lists.
- Auto min-size applies only to definite-size items and text leaves, not auto-sized containers.
- Windows: `tui_term.c` is written against `term.h` but only built/tested on Linux; CMake wiring untested here
  (no network to fetch deps).
- Cover art: only cell-based methods (glyph, braille) are drawn; sixel / kitty fall back to glyphs because they bypass the cell grid.

## The aplayer UI (`src/ui`)

The `aplayer` target is built on this library; see **`docs/UI.md`** for the module layout, data
flow and how to add views, widgets and hotkeys. `src/main.c` runs startup, session restore and the
playback loop, and calls `ui_event` / `ui_frame`.

| element | implementation |
|---|---|
| tabs | `ui_tab_bar.c`: `tui_button`s; views are toggled with `display:none` (`ui_views.c`) |
| list | `ui_playlist.c`: one custom-paint node, scrolloff/recenter, click and wheel |
| timestamp / volume / loudness / statusline | `ui_transport.c`: text nodes updated every frame (no-op when unchanged) |
| progress | `tui_slider` styled with the theme; click/drag seeks |
| media control | three `tui_button`s with hover colours |
| vu_meter | `ui_vu_meter.c`: custom paint; easing and peak-hold advance in `on_update` |
| lyrics | `ui_lyrics.c`: custom paint; follows playback, current line highlighted |
| art | `ui_cover_art.c`: `image_render()` output decoded (`ui_ansi.c`) into a cell grid and painted through the canvas |

Hotkeys run when no focused widget consumes the key; `q`/Ctrl-C quit.

Dropped relative to the very first hand-placed UI: the F3 debug toggle and F9 slow-redraw mode (the log widget was already marked broken).
