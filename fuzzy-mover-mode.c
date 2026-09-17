#include <glib.h>
#include <gmodule.h>

#include <rofi/helper.h>
#include <rofi/mode-private.h>
#include <rofi/mode.h>

G_MODULE_EXPORT Mode mode;

typedef struct {
    guint chunks;
    guint start;
} SubsequenceMatch;

typedef struct {
    GPtrArray *entries;
    GPtrArray *folded_entries;
    GArray *entry_order;
    guint *longest_runs;
    guint *chunk_counts;
    guint *start_positions;
    gchar *query;
    gchar *folded_query;
    gchar *result_file;
} FuzzyMoverModeData;

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

static SubsequenceMatch best_subsequence_match(const gchar *query,
                                               const gchar *entry)
{
    glong query_length = 0;
    glong entry_length = 0;
    gunichar *query_chars = g_utf8_to_ucs4_fast(
        query,
        -1,
        &query_length);
    gunichar *entry_chars = g_utf8_to_ucs4_fast(
        entry,
        -1,
        &entry_length);
    const guint impossible = G_MAXUINT / 2;
    guint *previous_chunks = g_new(guint, entry_length);
    guint *current_chunks = g_new(guint, entry_length);
    guint *previous_starts = g_new(guint, entry_length);
    guint *current_starts = g_new(guint, entry_length);

    if (query_length == 0) {
        g_free(current_starts);
        g_free(previous_starts);
        g_free(current_chunks);
        g_free(previous_chunks);
        g_free(entry_chars);
        g_free(query_chars);
        return (SubsequenceMatch){0, 0};
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

    guint chunks = impossible;
    guint start = impossible;
    for (glong j = 0; j < entry_length; ++j) {
        if (previous_chunks[j] < chunks ||
            (previous_chunks[j] == chunks && previous_starts[j] < start)) {
            chunks = previous_chunks[j];
            start = previous_starts[j];
        }
    }

    g_free(current_starts);
    g_free(previous_starts);
    g_free(current_chunks);
    g_free(previous_chunks);
    g_free(entry_chars);
    g_free(query_chars);

    if (chunks == impossible) {
        return (SubsequenceMatch){(guint)query_length, (guint)entry_length};
    }
    return (SubsequenceMatch){chunks, start};
}

static gint compare_entry_order(gconstpointer left_pointer,
                                gconstpointer right_pointer,
                                gpointer user_data)
{
    const FuzzyMoverModeData *data = user_data;
    const guint left = *(const guint *)left_pointer;
    const guint right = *(const guint *)right_pointer;

    if (data->query != NULL && *data->query != '\0') {
        if (data->longest_runs[left] != data->longest_runs[right]) {
            return data->longest_runs[left] > data->longest_runs[right]
                       ? -1
                       : 1;
        }
        if (data->chunk_counts[left] != data->chunk_counts[right]) {
            return data->chunk_counts[left] < data->chunk_counts[right]
                       ? -1
                       : 1;
        }
        if (data->start_positions[left] != data->start_positions[right]) {
            return data->start_positions[left] < data->start_positions[right]
                       ? -1
                       : 1;
        }
    }

    if (left == right) {
        return 0;
    }
    return left < right ? -1 : 1;
}

static guint ordered_entry_index(const FuzzyMoverModeData *data, guint index)
{
    return data->entry_order == NULL
               ? index
               : g_array_index(data->entry_order, guint, index);
}

static int fuzzy_mover_mode_init(Mode *sw)
{
    const gchar *choices_file = g_getenv("FUZZY_MOVER_CHOICES_FILE");
    const gchar *result_file = g_getenv("FUZZY_MOVER_RESULT_FILE");
    gchar *contents = NULL;
    gsize contents_length = 0;
    GError *error = NULL;

    if (choices_file == NULL || result_file == NULL) {
        g_warning("fuzzy-mover: picker file paths are missing");
        return FALSE;
    }

    if (!g_file_get_contents(choices_file, &contents, &contents_length, &error)) {
        g_warning("fuzzy-mover: cannot read choices: %s", error->message);
        g_clear_error(&error);
        return FALSE;
    }

    FuzzyMoverModeData *data = g_new0(FuzzyMoverModeData, 1);
    data->entries = g_ptr_array_new_with_free_func(g_free);
    data->folded_entries = g_ptr_array_new_with_free_func(g_free);
    data->result_file = g_strdup(result_file);

    gchar **lines = g_strsplit(contents, "\n", -1);
    for (gchar **line = lines; *line != NULL; ++line) {
        if (**line == '\0') {
            continue;
        }

        g_ptr_array_add(data->entries, g_strdup(*line));
        g_ptr_array_add(data->folded_entries, g_utf8_casefold(*line, -1));
    }

    g_strfreev(lines);
    g_free(contents);
    data->entry_order = g_array_sized_new(
        FALSE,
        FALSE,
        sizeof(guint),
        data->entries->len);
    for (guint i = 0; i < data->entries->len; ++i) {
        g_array_append_val(data->entry_order, i);
    }
    mode_set_private_data(sw, data);
    return TRUE;
}

static void fuzzy_mover_mode_destroy(Mode *sw)
{
    FuzzyMoverModeData *data = mode_get_private_data(sw);

    if (data == NULL) {
        return;
    }

    g_ptr_array_free(data->entries, TRUE);
    g_ptr_array_free(data->folded_entries, TRUE);
    g_array_free(data->entry_order, TRUE);
    g_free(data->longest_runs);
    g_free(data->chunk_counts);
    g_free(data->start_positions);
    g_free(data->query);
    g_free(data->folded_query);
    g_free(data->result_file);
    g_free(data);
    mode_set_private_data(sw, NULL);
}

static unsigned int fuzzy_mover_get_num_entries(const Mode *sw)
{
    const FuzzyMoverModeData *data = mode_get_private_data(sw);
    return data == NULL ? 0 : data->entries->len;
}

static char *fuzzy_mover_get_display_value(const Mode *sw,
                                           unsigned int selected_line,
                                           int *state,
                                           GList **attribute_list,
                                           int get_entry)
{
    const FuzzyMoverModeData *data = mode_get_private_data(sw);

    (void)attribute_list;
    if (state != NULL) {
        *state = 0;
    }
    if (!get_entry || data == NULL || selected_line >= data->entries->len) {
        return NULL;
    }

    const guint entry_index = ordered_entry_index(data, selected_line);
    return g_strdup(g_ptr_array_index(data->entries, entry_index));
}

static char *fuzzy_mover_get_completion(const Mode *sw,
                                        unsigned int selected_line)
{
    const FuzzyMoverModeData *data = mode_get_private_data(sw);

    if (data == NULL || selected_line >= data->entries->len) {
        return g_strdup("");
    }

    const guint entry_index = ordered_entry_index(data, selected_line);
    return g_strdup(g_ptr_array_index(data->entries, entry_index));
}

static char *fuzzy_mover_preprocess_input(Mode *sw, const char *input)
{
    FuzzyMoverModeData *data = mode_get_private_data(sw);

    g_free(data->query);
    g_free(data->folded_query);
    g_free(data->longest_runs);
    g_free(data->chunk_counts);
    g_free(data->start_positions);
    data->query = g_strdup(input);
    data->folded_query = g_utf8_casefold(input, -1);
    data->longest_runs = g_new0(guint, data->entries->len);
    data->chunk_counts = g_new0(guint, data->entries->len);
    data->start_positions = g_new0(guint, data->entries->len);

    for (guint i = 0; i < data->folded_entries->len; ++i) {
        g_array_index(data->entry_order, guint, i) = i;
        if (*data->folded_query != '\0') {
            const gchar *entry = g_ptr_array_index(data->folded_entries, i);
            const SubsequenceMatch subsequence = best_subsequence_match(
                data->folded_query,
                entry);

            data->longest_runs[i] = longest_common_substring(
                data->folded_query,
                entry);
            data->chunk_counts[i] = subsequence.chunks;
            data->start_positions[i] = subsequence.start;
        }
    }
    g_array_sort_with_data(data->entry_order, compare_entry_order, data);

    return g_strdup(input);
}

static int fuzzy_mover_token_match(const Mode *sw,
                                   rofi_int_matcher **tokens,
                                   unsigned int index)
{
    const FuzzyMoverModeData *data = mode_get_private_data(sw);

    if (data == NULL || index >= data->entries->len) {
        return FALSE;
    }
    const guint entry_index = ordered_entry_index(data, index);
    return helper_token_match(
        (rofi_int_matcher *const *)tokens,
        g_ptr_array_index(data->entries, entry_index));
}

static ModeMode fuzzy_mover_mode_result(Mode *sw,
                                        int menu_retv,
                                        char **input,
                                        unsigned int selected_line)
{
    const FuzzyMoverModeData *data = mode_get_private_data(sw);

    (void)input;
    if ((menu_retv & MENU_OK) && data != NULL &&
        selected_line < data->entries->len) {
        const guint entry_index = ordered_entry_index(data, selected_line);
        const gchar *entry = g_ptr_array_index(data->entries, entry_index);
        GError *error = NULL;

        if (!g_file_set_contents(data->result_file, entry, -1, &error)) {
            g_warning("fuzzy-mover: cannot save selection: %s", error->message);
            g_clear_error(&error);
        }
    }

    return MODE_EXIT;
}

Mode mode = {
    .abi_version = ABI_VERSION,
    .name = "fuzzy-mover",
    .cfg_name_key = "display-fuzzy-mover",
    ._init = fuzzy_mover_mode_init,
    ._destroy = fuzzy_mover_mode_destroy,
    ._get_num_entries = fuzzy_mover_get_num_entries,
    ._result = fuzzy_mover_mode_result,
    ._token_match = fuzzy_mover_token_match,
    ._get_display_value = fuzzy_mover_get_display_value,
    ._get_completion = fuzzy_mover_get_completion,
    ._preprocess_input = fuzzy_mover_preprocess_input,
    .private_data = NULL,
    .free = NULL,
#if ABI_VERSION >= 7u
    .type = MODE_TYPE_SWITCHER,
#endif
};
