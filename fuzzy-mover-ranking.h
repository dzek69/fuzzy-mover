#ifndef FUZZY_MOVER_RANKING_H
#define FUZZY_MOVER_RANKING_H

#include <glib.h>

typedef struct {
    guint longest_run;
    guint chunks;
    guint start;
    gboolean matched;
} FuzzyMoverMatch;

gchar *fuzzy_mover_normalize_query(const gchar *query);

FuzzyMoverMatch fuzzy_mover_score_match(const gchar *normalized_query,
                                        const gchar *folded_entry);

gint fuzzy_mover_compare_matches(const FuzzyMoverMatch *left,
                                 const FuzzyMoverMatch *right);

#endif
