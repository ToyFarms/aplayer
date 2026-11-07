#ifndef __KARA_LEX_H
#define __KARA_LEX_H

/*
 * Optional pronunciation data for the text front-end. Both files are plain
 * UTF-8 text, looked up when the lexicon is opened:
 *
 *   en_syllables.txt   one word per line:  <word> <stress digits>
 *                      "water 10"  (one digit per syllable: 1 primary,
 *                      2 secondary, 0 none; "-" for none at all). From CMUdict (lexicon/README.md).
 *   ja_readings.txt    one entry per line: <surface>TAB<hiragana reading>
 *                      "水面<TAB>みなも". Matched longest first.
 *
 * A file that is missing is not an error: English falls back to counting
 * vowel groups, and Japanese kanji to two morae each (kana is always exact).
 *
 * Directory, first that exists: the argument, the environment variable
 * APLAYER_LEXICON_DIR, `lexicon` next to the executable, `lexicon` in the
 * current directory.
 */

#include <stddef.h>
#include <stdint.h>

typedef struct kara_lex kara_lex;

/* Never NULL unless out of memory; check kara_lex_has_*(). Reads the files
 * (a few MB) on the calling thread. */
kara_lex *kara_lex_open(const char *dir);
void kara_lex_close(kara_lex *lex);

int kara_lex_has_en(const kara_lex *lex);
int kara_lex_has_ja(const kara_lex *lex);

/* English: syllable count (-1: word not in the lexicon; 0: in it, but without
 * a vowel), stress digits (0, 1, 2) for up to `cap` of them. `word` is lower
 * case. */
int kara_lex_en(const kara_lex *lex, const char *word, uint8_t *stress,
                int cap);

/* Japanese: the longest entry that is a prefix of cps[0..n). Returns the
 * number of characters it covers (0: none) and writes its reading as code
 * points into `reading` (up to `cap`; count in *reading_n). */
int kara_lex_ja(const kara_lex *lex, const uint32_t *cps, int n,
                uint32_t *reading, int cap, int *reading_n);

#endif /* __KARA_LEX_H */
