#include "fuzzy-mover-ranking.h"

#include <string.h>

static guint longest_common_substring(const gchar *left, const gchar *right)
{
    glong left_length = 0;
    glong right_length = 0;
    gunichar *left_chars = g_utf8_to_ucs4_fast(left, -1, &left_length);
    gunichar *right_chars = g_utf8_to_ucs4_fast(right, -1, &right_length);
    guint *previous = g_new0(guint, right_length + 1);
    guint *current = g_new0(guint, right_length + 1);
    guint longest = 0;

    for (glong i = 0; i < left_length; ++i) {
        for (glong j = 0; j < right_length; ++j) {
            if (left_chars[i] == right_chars[j]) {
                current[j + 1] = previous[j] + 1;
                longest = MAX(longest, current[j + 1]);
            } else {
                current[j + 1] = 0;
            }
        }

        guint *temporary = previous;
        previous = current;
        current = temporary;
        memset(current, 0, sizeof(guint) * (right_length + 1));
    }

    g_free(current);
    g_free(previous);
    g_free(right_chars);
    g_free(left_chars);
    return longest;
}

gchar *fuzzy_mover_normalize_query(const gchar *query)
{
    gchar *folded = g_utf8_casefold(query, -1);
    GString *normalized = g_string_sized_new(strlen(folded));

    for (const gchar *position = folded;
         *position != '\0';
         position = g_utf8_next_char(position)) {
        const gunichar character = g_utf8_get_char(position);

        if (!g_unichar_isspace(character)) {
            g_string_append_unichar(normalized, character);
        }
    }

    g_free(folded);
    return g_string_free(normalized, FALSE);
}

FuzzyMoverMatch fuzzy_mover_score_match(const gchar *normalized_query,
                                        const gchar *folded_entry)
{
    glong query_length = 0;
    glong entry_length = 0;
    gunichar *query_chars = g_utf8_to_ucs4_fast(
        normalized_query,
        -1,
        &query_length);
    gunichar *entry_chars = g_utf8_to_ucs4_fast(
        folded_entry,
        -1,
        &entry_length);
    const guint impossible = G_MAXUINT / 2;
    guint *previous_chunks = g_new(guint, entry_length);
    guint *current_chunks = g_new(guint, entry_length);
    guint *previous_starts = g_new(guint, entry_length);
    guint *current_starts = g_new(guint, entry_length);
    FuzzyMoverMatch result = {
        .longest_run = longest_common_substring(
            normalized_query,
            folded_entry),
        .chunks = 0,
        .start = 0,
        .matched = TRUE,
    };

    if (query_length == 0) {
        goto cleanup;
    }

    for (glong j = 0; j < entry_length; ++j) {
        previous_chunks[j] = impossible;
        previous_starts[j] = impossible;
    }

    for (glong i = 0; i < query_length; ++i) {
        guint best_noncontiguous_chunks = impossible;
        guint best_noncontiguous_start = impossible;

        for (glong j = 0; j < entry_length; ++j) {
            current_chunks[j] = impossible;
            current_starts[j] = impossible;

            if (j >= 2) {
                const guint candidate_chunks = previous_chunks[j - 2];
                const guint candidate_start = previous_starts[j - 2];

                if (candidate_chunks < best_noncontiguous_chunks ||
                    (candidate_chunks == best_noncontiguous_chunks &&
                     candidate_start < best_noncontiguous_start)) {
                    best_noncontiguous_chunks = candidate_chunks;
                    best_noncontiguous_start = candidate_start;
                }
            }
            if (query_chars[i] != entry_chars[j]) {
                continue;
            }

            if (i == 0) {
                current_chunks[j] = 1;
                current_starts[j] = (guint)j;
                continue;
            }
            if (j > 0 && previous_chunks[j - 1] < impossible) {
                current_chunks[j] = previous_chunks[j - 1];
                current_starts[j] = previous_starts[j - 1];
            }
            if (best_noncontiguous_chunks < impossible) {
                const guint split_chunks = best_noncontiguous_chunks + 1;

                if (split_chunks < current_chunks[j] ||
                    (split_chunks == current_chunks[j] &&
                     best_noncontiguous_start < current_starts[j])) {
                    current_chunks[j] = split_chunks;
                    current_starts[j] = best_noncontiguous_start;
                }
            }
        }

        guint *temporary = previous_chunks;
        previous_chunks = current_chunks;
        current_chunks = temporary;
        temporary = previous_starts;
        previous_starts = current_starts;
        current_starts = temporary;
    }

    result.chunks = impossible;
    result.start = impossible;
    for (glong j = 0; j < entry_length; ++j) {
        if (previous_chunks[j] < result.chunks ||
            (previous_chunks[j] == result.chunks &&
             previous_starts[j] < result.start)) {
            result.chunks = previous_chunks[j];
            result.start = previous_starts[j];
        }
    }

    if (result.chunks == impossible) {
        result.chunks = (guint)query_length;
        result.start = (guint)entry_length;
        result.matched = FALSE;
    }

cleanup:
    g_free(current_starts);
    g_free(previous_starts);
    g_free(current_chunks);
    g_free(previous_chunks);
    g_free(entry_chars);
    g_free(query_chars);
    return result;
}

gint fuzzy_mover_compare_matches(const FuzzyMoverMatch *left,
                                 const FuzzyMoverMatch *right)
{
    if (left->matched != right->matched) {
        return left->matched ? -1 : 1;
    }
    if (left->longest_run != right->longest_run) {
        return left->longest_run > right->longest_run ? -1 : 1;
    }
    if (left->chunks != right->chunks) {
        return left->chunks < right->chunks ? -1 : 1;
    }
    if (left->start != right->start) {
        return left->start < right->start ? -1 : 1;
    }
    return 0;
}
