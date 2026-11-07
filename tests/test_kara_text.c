#include "base_test.h"

INCLUDE_BEGIN
#include "kara_text.h"

/* the unit count and weights of `text` in `lang` */
static kara_units *units(const char *text, kara_lang lang, const kara_lex *lex)
{
    kara_units *u = kara_units_build(text, lang, lex);
    return u;
}

/* units must be in text order and stay inside the text */
static bool well_formed(const kara_units *u, const char *text)
{
    int len = (int)strlen(text), prev = 0;
    for (int i = 0; i < u->n; i++)
    {
        const kara_unit *k = &u->u[i];
        if (k->off < prev || k->len < 0 || k->off + k->len > len ||
            k->weight <= 0.0f || k->nsub < 1 || k->sub >= k->nsub)
            return false;
        prev = k->off;
    }
    return true;
}
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 -Isrc/karaoke
 src/karaoke/kara_text.c
 src/karaoke/kara_lex.c
 -lm
 */ CFLAGS_END

TEST_BEGIN(indonesian_syllables_follow_the_vowels)
{
    /* counts agree with the Stage 1 Python front-end */
    struct { const char *text; int n; } c[] = {
        {"cinta", 2},       {"menguras", 3}, {"mata", 2},
        {"air", 2},         /* ai is a diphthong only at the end */
        {"pantai", 2},      {"nyanyian", 3}, {"Akhirnya cinta yang tak", 7},
    };
    for (size_t i = 0; i < sizeof(c) / sizeof(*c); i++)
    {
        kara_units *u = units(c[i].text, KARA_LANG_ID, NULL);
        ASSERT_INT_EQ(u->n, c[i].n);
        ASSERT_TRUE(well_formed(u, c[i].text));
        kara_units_free(u);
    }
}
TEST_END()

TEST_BEGIN(a_unit_knows_where_its_word_starts)
{
    const char *t = "aku cinta kamu";
    kara_units *u = units(t, KARA_LANG_ID, NULL);
    int words = 0;
    for (int i = 0; i < u->n; i++)
        words += u->u[i].word_start;
    ASSERT_INT_EQ(words, 3);
    ASSERT_TRUE(u->u[0].word_start);
    /* "cinta" starts at byte 4 and its second syllable at "ta" */
    ASSERT_INT_EQ(u->u[2].off, 4);
    ASSERT_INT_EQ(u->u[3].off, 7);
    kara_units_free(u);
}
TEST_END()

TEST_BEGIN(english_without_a_lexicon_counts_vowel_groups)
{
    kara_units *u = units("water", KARA_LANG_EN, NULL);
    ASSERT_INT_EQ(u->n, 2);
    ASSERT_FLOAT_EQ(u->u[0].weight, 1.15f);
    ASSERT_FLOAT_EQ(u->u[1].weight, 0.85f);
    kara_units_free(u);

    /* a silent final e, and a function word that is sung short */
    u = units("make the", KARA_LANG_EN, NULL);
    ASSERT_INT_EQ(u->n, 2);
    ASSERT_FLOAT_EQ(u->u[1].weight, 0.75f);
    kara_units_free(u);
}
TEST_END()

TEST_BEGIN(english_with_a_lexicon_uses_its_stress)
{
    kara_lex *lex = kara_lex_open("tests/data/lexicon");
    ASSERT_TRUE(kara_lex_has_en(lex));
    kara_units *u = units("hello", KARA_LANG_EN, lex);
    ASSERT_INT_EQ(u->n, 2);
    ASSERT_FLOAT_EQ(u->u[0].weight, 1.15f);
    kara_units_free(u);

    /* a word with no vowel is in the lexicon with no syllables: one unit */
    u = units("mm", KARA_LANG_EN, lex);
    ASSERT_INT_EQ(u->n, 1);
    ASSERT_FLOAT_EQ(u->u[0].weight, 0.85f);
    kara_units_free(u);
    kara_lex_close(lex);
}
TEST_END()

TEST_BEGIN(japanese_kana_is_counted_in_morae)
{
    struct { const char *text; int n; } c[] = {
        {"ありがとう", 5}, {"きょう", 2},   {"がっこう", 4},
        {"ラーメン", 4},   {"しんぶん", 4}, {"ふぁいと", 3},
    };
    for (size_t i = 0; i < sizeof(c) / sizeof(*c); i++)
    {
        kara_units *u = units(c[i].text, KARA_LANG_JA, NULL);
        ASSERT_INT_EQ(u->n, c[i].n);
        ASSERT_TRUE(well_formed(u, c[i].text));
        kara_units_free(u);
    }

    kara_units *u = units("がっこう", KARA_LANG_JA, NULL);
    ASSERT_FLOAT_EQ(u->u[1].weight, 0.6f); /* the small tsu is short */
    kara_units_free(u);
    u = units("ほん", KARA_LANG_JA, NULL);
    ASSERT_FLOAT_EQ(u->u[1].weight, 0.9f); /* so is the moraic n */
    kara_units_free(u);
}
TEST_END()

TEST_BEGIN(a_kanji_is_two_morae_unless_the_lexicon_knows_it)
{
    kara_units *u = units("夢", KARA_LANG_JA, NULL);
    ASSERT_INT_EQ(u->n, 2);
    /* one glyph sung over two units: they share it */
    ASSERT_INT_EQ(u->u[0].off, u->u[1].off);
    ASSERT_INT_EQ(u->u[0].nsub, 2);
    ASSERT_INT_EQ(u->u[1].sub, 1);
    kara_units_free(u);

    kara_lex *lex = kara_lex_open("tests/data/lexicon");
    ASSERT_TRUE(kara_lex_has_ja(lex));
    u = units("私は", KARA_LANG_JA, lex);
    ASSERT_INT_EQ(u->n, 4); /* わ た し + は */
    ASSERT_TRUE(well_formed(u, "私は"));
    kara_units_free(u);

    /* the longest entry wins: 水面 is みなも (3), not 水 + 面 */
    u = units("水面", KARA_LANG_JA, lex);
    ASSERT_INT_EQ(u->n, 3);
    /* three units over two glyphs: the first glyph has two, the second one */
    ASSERT_INT_EQ(u->u[0].nsub + u->u[2].nsub, 3);
    kara_units_free(u);
    kara_lex_close(lex);
}
TEST_END()

TEST_BEGIN(korean_and_chinese_are_a_unit_per_character)
{
    kara_units *u = units("사랑해요", KARA_LANG_KO, NULL);
    ASSERT_INT_EQ(u->n, 4);
    kara_units_free(u);
    u = units("我爱你", KARA_LANG_ZH, NULL);
    ASSERT_INT_EQ(u->n, 3);
    ASSERT_TRUE(well_formed(u, "我爱你"));
    kara_units_free(u);
}
TEST_END()

TEST_BEGIN(language_comes_from_the_script_then_the_tag)
{
    kara_lang l;
    ASSERT_TRUE(kara_lang_from_tag("id-ID", &l) && l == KARA_LANG_ID);
    ASSERT_TRUE(kara_lang_from_tag("MS", &l) && l == KARA_LANG_ID);
    ASSERT_TRUE(kara_lang_from_tag("es", &l) && l == KARA_LANG_OTHER);
    ASSERT_FALSE(kara_lang_from_tag("", &l));
    ASSERT_FALSE(kara_lang_from_tag(NULL, &l));

    ASSERT_TRUE(kara_detect_lang("ありがとう", KARA_LANG_EN, true) == KARA_LANG_JA);
    ASSERT_TRUE(kara_detect_lang("사랑", KARA_LANG_EN, true) == KARA_LANG_KO);
    ASSERT_TRUE(kara_detect_lang("我爱你", KARA_LANG_EN, true) == KARA_LANG_ZH);
    ASSERT_TRUE(kara_detect_lang("我爱你", KARA_LANG_JA, true) == KARA_LANG_JA);
    ASSERT_TRUE(kara_detect_lang("hello", KARA_LANG_ID, true) == KARA_LANG_ID);
    /* Latin text in a file tagged Japanese is not Japanese */
    ASSERT_TRUE(kara_detect_lang("oh the night", KARA_LANG_JA, true) == KARA_LANG_EN);

    ASSERT_TRUE(kara_guess_latin_lang("yang dan di ke dari ini itu") == KARA_LANG_ID);
    ASSERT_TRUE(kara_guess_latin_lang("the and you of to in it") == KARA_LANG_EN);
    ASSERT_TRUE(kara_text_has_kana("a の b"));
    ASSERT_FALSE(kara_text_has_kana("漢字"));
}
TEST_END()

TEST_BEGIN(lines_with_nothing_to_sing_are_instrumental)
{
    ASSERT_TRUE(kara_is_instrumental(""));
    ASSERT_TRUE(kara_is_instrumental("   "));
    ASSERT_TRUE(kara_is_instrumental("\xE2\x99\xAA")); /* ♪ */
    ASSERT_TRUE(kara_is_instrumental("..."));
    ASSERT_FALSE(kara_is_instrumental("la"));
    ASSERT_FALSE(kara_is_instrumental("1 2 3"));
    ASSERT_FALSE(kara_is_instrumental("♪ la ♪"));
    ASSERT_FALSE(kara_is_instrumental("あ"));
}
TEST_END()
