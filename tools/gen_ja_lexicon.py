#!/usr/bin/env python3
"""Writes lexicon/ja_readings.txt: "<surface><TAB><hiragana reading>" per line.

Without this file the player still times every line of Japanese lyrics, but it
has to guess how many morae a kanji is sung over (two), which is wrong often
enough to shift the highlight within a line. Kana is always exact.

    pip install pykakasi
    python3 tools/gen_ja_lexicon.py [out_file]

LICENSE: the readings come from pykakasi's dictionary, which is derived from
KAKASI and is GPL-3.0-or-later. aplayer itself is MIT, so this script does NOT
run as part of the build; its output is committed as lexicon/ja_readings.txt
and carries the GPL. Another source of readings (any "surface -> reading" list in
this format) works the same way.
"""
import pickle
import sys

from pykakasi.properties import Configurations as C


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "lexicon/ja_readings.txt"
    with open(C.dictpath(C.jisyo_kanwa), "rb") as f:
        table = pickle.load(f)
    rows = []
    for words in table.values():
        for surface, readings in words.items():
            if not readings:
                continue
            # prefer a reading with no context condition: it is the plain one
            plain = [r for r, con in readings if con is None]
            reading = plain[0] if plain else readings[0][0]
            if reading and "\t" not in surface and "\n" not in surface:
                rows.append(f"{surface}\t{reading}\n")
    rows.sort()
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(rows)
    print(f"{len(rows)} entries -> {out}")


if __name__ == "__main__":
    main()
