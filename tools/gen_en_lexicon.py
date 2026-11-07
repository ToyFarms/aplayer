#!/usr/bin/env python3
"""Writes lexicon/en_syllables.txt: one line per English word, "<word> <stress digits>".

The digits are the stress of each syllable of the word's first pronunciation in
CMUdict (1 primary, 2 secondary, 0 none), so "water" is "water 10" ("mm" has none: "mm -"). The player
only needs the count and the stress, and a few MB of text is much smaller than
the dictionary it comes from.

    pip install cmudict
    python3 tools/gen_en_lexicon.py [out_file]

CMUdict is BSD-licensed (see lexicon/CMUDICT_LICENSE); the Python package that
wraps it is a separate thing and is not needed at run time.
"""
import re
import sys

import cmudict


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "lexicon/en_syllables.txt"
    rows = []
    for word, prons in cmudict.dict().items():
        if not re.fullmatch(r"[a-z']+", word) or not prons:
            continue
        stress = "".join(p[-1] for p in prons[0] if p[-1].isdigit())
        rows.append(f"{word} {stress or '-'}\n")  # "-": a word with no vowel (mm)
    rows.sort()
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("# Syllable stress per word, from CMUdict (BSD, see CMUDICT_LICENSE).\n")
        f.writelines(rows)
    print(f"{len(rows)} words -> {out}")


if __name__ == "__main__":
    main()
