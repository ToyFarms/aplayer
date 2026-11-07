# Lexicon (optional)

Word lists the word timing (docs/KARAOKE.md) uses to count syllables. The
player works without them; with them the highlight is more accurate. The folder
is copied next to the executable by the build and searched in this order: the
`APLAYER_LEXICON_DIR` environment variable, `lexicon/` next to the executable,
`lexicon/` in the working directory.

## `en_syllables.txt` (English)

`<word> <stress digits>` per line, e.g. `water 10` (one digit per syllable:
1 primary stress, 2 secondary, 0 none; `-` for a word with no vowel).
Without it English words are counted by vowel groups.

Generate it with `pip install cmudict && python3 tools/gen_en_lexicon.py`.
The data is CMUdict (BSD licence, `CMUDICT_LICENSE`); only the word and its
stress pattern are kept.

## `ja_readings.txt` (Japanese kanji)

`<surface><TAB><hiragana>` per line, e.g. `水面<TAB>みなも`, matched longest
first. Without it a kanji counts as two morae. Kana needs no list.

**It is shipped, generated from pykakasi's dictionary** (`tools/gen_ja_lexicon.py`
rebuilds it). That dictionary derives from KAKASI and is GPL-3.0-or-later, so
this file carries that licence even though the rest of aplayer is MIT; delete
it (the player then falls back to two morae per kanji) or replace it with any
other "surface -> reading" list in this format if that matters for how you
distribute the player.
