#ifndef __KARA_TEXT_H
#define __KARA_TEXT_H

/*
 * Lyrics text -> singable units (syllables, or morae in Japanese) and a
 * relative duration weight for each. Port of the Stage 1 text front-end
 * (kara/text_units.py) with no models: phonetically regular languages are
 * handled by rules, English by a pronunciation lexicon when one is installed
 * (see kara_lex.h) and by a vowel-group heuristic when it is not.
 *
 * Every unit knows which bytes of the line's text it covers, so the interface
 * can draw the text it was given and paint it as the unit is sung.
 *
 * Pure: no threads, no I/O except the optional lexicon passed in.
 */

#include "kara_lex.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum kara_lang
{
    KARA_LANG_EN,
    KARA_LANG_ID, /* Indonesian, and Malay */
    KARA_LANG_JA,
    KARA_LANG_KO,
    KARA_LANG_ZH,
    KARA_LANG_OTHER, /* other Latin script: vowel groups */
} kara_lang;

typedef struct kara_unit
{
    int off, len;    /* bytes of the line's text this unit is sung over */
    float weight;    /* relative duration; 1.0 is an ordinary syllable */
    bool word_start; /* first unit of a word */
    /* A kanji read with several morae is one glyph sung over several units:
     * they share [off, off + len) and each takes the part sub / nsub of it.
     * nsub is 1 for every other unit. */
    uint8_t sub, nsub;
} kara_unit;

typedef struct kara_units
{
    kara_lang lang;
    kara_unit *u;
    int n;
} kara_units;

/* A two-letter language code ("id", "ja", "en", ...) or a longer tag
 * ("id-ID", "ind"; only the first two letters count); anything the code does
 * not know is KARA_LANG_OTHER. NULL or empty: no hint. */
bool kara_lang_from_tag(const char *tag, kara_lang *out);

/* Script first (kana: Japanese, Hangul: Korean, ideographs: Chinese unless the
 * hint says Japanese), then the hint, then English. `has_hint` false: `hint`
 * is ignored. Latin text under a Japanese hint is not Japanese: it is handed
 * on as Latin (see kara_guess_latin_lang). */
kara_lang kara_detect_lang(const char *text, kara_lang hint, bool has_hint);

/* Indonesian or English, by a vote of common words over all the lyrics. */
kara_lang kara_guess_latin_lang(const char *all_text);

bool kara_text_has_kana(const char *text);

/* Nothing to sing: no letter or digit (blank, a music note, "..."). */
bool kara_is_instrumental(const char *text);

/* The units of one line, or NULL on allocation failure. n may be 0.
 * `lex` may be NULL. */
kara_units *kara_units_build(const char *text, kara_lang lang,
                             const kara_lex *lex);
void kara_units_free(kara_units *u);

#endif /* __KARA_TEXT_H */
