# fonts/

Font files put here (`.ttf`, `.otf`, `.ttc`, `.otc`) are used for the big line
of lyrics, without any configuration. The build copies this folder next to the
executable; the player also looks in `./fonts` and in the directory named by the
environment variable `APLAYER_FONT_DIR`.

Order of preference, first match per character:

1. `lyrics.font` (the option) and `APLAYER_LYRICS_FONT` (a file)
2. this folder, by file name — so `01-latin.ttf`, `02-cjk.ttc` decide the order
3. the bold sans and CJK fonts the system ships
4. GNU Unifont, last of all: a file here whose name starts with `unifont`
   (e.g. `unifont.otf`), else the copy the system installed
   (`/usr/share/fonts/opentype/unifont/unifont.otf` on Debian / Ubuntu)

Glyphs are drawn hinted and monochrome, a pixel wide stem at a time, which is
what keeps text three or four terminal rows tall legible. Variable fonts work:
the weight axis follows `lyrics.weight` (700 by default). Static fonts keep the
weight they have, so prefer a Bold or Medium file to a Regular one.

Unifont is exact at 16 pixels (4 rows of braille) and has a glyph for nearly
every character, which makes it a good last resort, not a first choice.
