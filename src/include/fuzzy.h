#ifndef __FUZZY_H
#define __FUZZY_H

/*
 * fuzzy - fzf-style fuzzy matching over a large, mostly static list of strings,
 * built for "filter as you type".
 *
 *   fuzzy_index   the haystack. Each string is decoded + folded ONCE
 *                 (case, diacritics, full-width forms, katakana -> hiragana,
 *                 combining marks dropped) and gets a 64-bit character mask.
 *   fuzzy_query   the needle: space separated terms, ALL of which must match
 *                 (each one fuzzily). Smart-case per term: a term containing an
 *                 upper-case letter is case sensitive, otherwise it is not.
 *   fuzzy_search  one run of a query over an index. It is
 *                   - incremental: when the new query extends the previous one
 *                     ("ab" -> "abc") only the previous matches are re-scored,
 *                     because a longer query can never match more;
 *                   - time-sliced: fuzzy_search_run(budget) can be called from
 *                     a frame loop and stops after `budget` nanoseconds, so
 *                     typing never blocks on a huge library;
 *                   - O(n) to order: results are counting-sorted by
 *                     (score desc, length asc, id asc), no comparison sort.
 *
 * Scoring follows fzf's v2 idea: every matched character earns a base score
 * plus a bonus for sitting on a word boundary / camelCase hump / consecutive
 * run, and gaps are penalised (more for opening a gap than for extending it).
 * The best alignment is found with a small DP over the window that can still
 * hold the match.
 *
 * Pure C, no dependencies, not thread safe per object (separate objects are
 * independent).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fuzzy_index fuzzy_index;
typedef struct fuzzy_query fuzzy_query;
typedef struct fuzzy_search fuzzy_search;

typedef struct fuzzy_hit
{
    uint32_t id;   /* the value fuzzy_index_add() returned */
    int32_t score; /* higher is better */
} fuzzy_hit;

/* ------------------------------------------------------------------------ */
/* haystack                                                                 */
/* ------------------------------------------------------------------------ */

fuzzy_index *fuzzy_index_new(void);
void fuzzy_index_free(fuzzy_index *idx);
/* Optional: avoid regrowing for `items` strings of `avg_len` bytes. */
void fuzzy_index_reserve(fuzzy_index *idx, size_t items, size_t avg_len);
/* Appends a UTF-8 string (not retained). Returns its id: 0, 1, 2, ... */
uint32_t fuzzy_index_add(fuzzy_index *idx, const char *utf8, size_t len);
size_t fuzzy_index_count(const fuzzy_index *idx);

/* ------------------------------------------------------------------------ */
/* needle                                                                   */
/* ------------------------------------------------------------------------ */

fuzzy_query *fuzzy_query_new(const char *utf8);
void fuzzy_query_free(fuzzy_query *q);
/* No terms (empty or only blanks): everything matches. */
bool fuzzy_query_empty(const fuzzy_query *q);
const char *fuzzy_query_text(const fuzzy_query *q);
/* True when every match of `newer` is guaranteed to be a match of `older`
 * (newer's text starts with older's). This is what makes narrowing valid. */
bool fuzzy_query_extends(const fuzzy_query *newer, const fuzzy_query *older);

/* Matches one string without an index (reference implementation, tooling).
 * Returns false when it does not match. `score` may be NULL. */
bool fuzzy_query_match(const fuzzy_query *q, const char *utf8, size_t len,
                       int32_t *score);

/* Where did the query match? Writes the indices (in code points of the ORIGINAL
 * string, ascending, unique) of the matched characters to `out` (at most `cap`)
 * and returns how many, or -1 if the string does not match. For highlighting
 * the visible rows only; it re-folds the text, so it is not for bulk use. */
int fuzzy_query_positions(const fuzzy_query *q, const char *utf8, size_t len,
                          uint16_t *out, int cap);

/* ------------------------------------------------------------------------ */
/* search                                                                   */
/* ------------------------------------------------------------------------ */

/* Starts a search of `query` over `idx`. `parent` is the previous (finished)
 * search of the same index: if `query` extends the parent's query only the
 * parent's matches are scanned. It is read here and may be freed right away.
 * `query` is copied. Nothing is scanned until fuzzy_search_run(). */
fuzzy_search *fuzzy_search_new(const fuzzy_index *idx, const char *query,
                               const fuzzy_search *parent);
void fuzzy_search_free(fuzzy_search *s);

/* Scan candidates until done or `budget_ns` elapsed (0 = one small chunk,
 * UINT64_MAX = no limit). Returns true once the search is complete (and the
 * hits are sorted). */
bool fuzzy_search_run(fuzzy_search *s, uint64_t budget_ns);
bool fuzzy_search_done(const fuzzy_search *s);

/* Matches found so far. Complete + best-first once done; while running it is a
 * valid but unsorted prefix of the final set (in id order), so a UI can show it
 * immediately. The pointer is invalidated by the next fuzzy_search_run(). */
const fuzzy_hit *fuzzy_search_hits(const fuzzy_search *s, size_t *count);
/* How many candidates were scanned / have to be scanned in total. */
size_t fuzzy_search_scanned(const fuzzy_search *s);
size_t fuzzy_search_total(const fuzzy_search *s);
const fuzzy_query *fuzzy_search_query(const fuzzy_search *s);
/* True if this search narrowed its parent (diagnostics, tests). */
bool fuzzy_search_was_refined(const fuzzy_search *s);

#ifdef __cplusplus
}
#endif

#endif /* __FUZZY_H */
