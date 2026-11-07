# Input, keybindings and search

## Pieces

| Piece | Files | Job |
|---|---|---|
| Fuzzy matcher | `src/include/fuzzy.h`, `src/struct/fuzzy.c` | fzf-style scoring over an index, incremental + time-sliced |
| Router | `src/include/tui_router.h`, `src/tui/tui_router.c` | keymaps (vim-style tries), modes, focus layers, langmap |
| Terminal adapter | `src/include/tui_term.h`, `src/tui/tui_term.c` | `term_event` -> router; assembles UTF-8 / bracketed paste into TEXT |
| Input widget | `tui_input()` in `src/tui/tui_widgets_data.c` | single-line editor, hardware cursor on the caret |
| Search bar | `src/ui/widgets/ui_search.c` | the `/` bar; owns index + current search |
| Bindings | `src/ui/ui_hotkeys.c` | the normal and search keymaps (tables) |

## Events

* `TUI_EV_KEY` - ASCII, special keys and modifiers.
* `TUI_EV_TEXT` - non-ASCII commits (IME) and bracketed paste. Offered to the
  focused widget first; if nobody takes it, it is replayed as KEYs through the
  keymaps (so a full-width `／` still opens search).
* A key hook runs at every bubbling step before the node's `on_event`, then
  once with `NULL` for the global scope. The router uses it to apply keymaps
  per focus layer.

## Keymaps

Per mode, a trie of key sequences. Supports multi-key (`gg`, `<leader>x`),
optional counts, an ambiguity timeout (`keys.timeout_ms`, default 500) and
replay of an unmatched prefix. `Esc` is held until the end of the read so
Esc+key arriving together is Alt+key. Spec syntax is vim-like: `gg`,
`<C-n>`, `<S-Tab>`, `<Esc>`.

## Focus layers

`tui_router_push_layer` sets the focus, keymap and mode; `pop` restores the
previous focus. A modal layer swallows everything the layer's keymap and
focused widget do not handle, so `q` in the search bar types a `q`.

## Other languages

* The hardware terminal cursor sits on the input caret, so the IME pre-edit
  and candidate window anchor there.
* Non-TEXT keys go through the **langmap** (presets `ru`, `uk`, `el`, `cjk`,
  plus full-width ASCII folding) so normal-mode bindings work on a non-Latin
  layout. Text typed into an input is never remapped.
* The fuzzy index folds case, diacritics, full-width forms and kana, so
  `bjork` finds `Björk` and `ニホン`-style queries match hiragana titles.

## Search flow

1. `/` -> `ui_search_open`: builds the index, pushes a modal layer
   (`km_search`), shows the bar.
2. Each edit starts a `fuzzy_search`. Extending the query rescoring only the
   previous hits; the first slice runs immediately, the rest is continued once
   per frame in `ui_search_sync` (`search.slice_us`).
3. The playlist shows the hits through a `ui_filter` (best match first,
   matched characters highlighted). Up/Down/`C-n`/`C-p`/mouse move the cursor.
4. `Enter` -> play that track, close, back to the full list on it.
   `Esc` -> close, nothing played, cursor back where it was.

## Known limits

* Windows input paths (`term_win.c`) are unchanged and untested with TEXT.
* Digits `1`-`9` are view switches, so counts work in the engine but are not
  enabled in the app keymap.
* The terminal must report a UTF-8 locale for IME commits to arrive intact.
