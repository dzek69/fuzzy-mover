#include <glib.h>
#include <gmodule.h>

#include <rofi/helper.h>
#include <rofi/mode-private.h>
#include <rofi/mode.h>

#include "fuzzy-mover-ranking.h"

G_MODULE_EXPORT Mode mode;

typedef struct {
    GPtrArray *entries;
    GPtrArray *folded_entries;
    GArray *entry_order;
    FuzzyMoverMatch *scores;
    gchar *folded_query;
    gchar *result_file;
} FuzzyMoverModeData;

static gint compare_entry_order(gconstpointer left_pointer,
                                gconstpointer right_pointer,
                                gpointer user_data)
{
    const FuzzyMoverModeData *data = user_data;
    const guint left = *(const guint *)left_pointer;
    const guint right = *(const guint *)right_pointer;

    if (data->folded_query != NULL && *data->folded_query != '\0') {
        const gint comparison = fuzzy_mover_compare_matches(
            &data->scores[left],
            &data->scores[right]);

        if (comparison != 0) {
            return comparison;
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
    g_free(data->scores);
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

    g_free(data->folded_query);
    g_free(data->scores);
    data->folded_query = fuzzy_mover_normalize_query(input);
    data->scores = g_new0(FuzzyMoverMatch, data->entries->len);

    for (guint i = 0; i < data->folded_entries->len; ++i) {
        const gchar *entry = g_ptr_array_index(data->folded_entries, i);

        g_array_index(data->entry_order, guint, i) = i;
        data->scores[i] = fuzzy_mover_score_match(
            data->folded_query,
            entry);
    }
    g_array_sort_with_data(data->entry_order, compare_entry_order, data);

    return g_strdup(data->folded_query);
}

static int fuzzy_mover_token_match(const Mode *sw,
                                   rofi_int_matcher **tokens,
                                   unsigned int index)
{
    const FuzzyMoverModeData *data = mode_get_private_data(sw);

    if (data == NULL || index >= data->entries->len) {
        return FALSE;
    }
    (void)tokens;
    const guint entry_index = ordered_entry_index(data, index);
    return data->scores[entry_index].matched;
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
