#include <glib.h>
#include <gmodule.h>

#include <rofi/helper.h>
#include <rofi/mode-private.h>
#include <rofi/mode.h>

G_MODULE_EXPORT Mode mode;

typedef struct {
    GPtrArray *entries;
    GPtrArray *folded_entries;
    guint *longest_runs;
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
    g_free(data->longest_runs);
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

    return g_strdup(g_ptr_array_index(data->entries, selected_line));
}

static char *fuzzy_mover_get_completion(const Mode *sw,
                                        unsigned int selected_line)
{
    const FuzzyMoverModeData *data = mode_get_private_data(sw);

    if (data == NULL || selected_line >= data->entries->len) {
        return g_strdup("");
    }

    if (data->query == NULL || *data->query == '\0') {
        return g_strdup(g_ptr_array_index(data->entries, selected_line));
    }

    const glong query_length = g_utf8_strlen(data->query, -1);
    const glong prefix_length = MIN(
        (glong)data->longest_runs[selected_line],
        query_length);
    const gchar *end = g_utf8_offset_to_pointer(data->query, prefix_length);
    return g_strndup(data->query, end - data->query);
}

static char *fuzzy_mover_preprocess_input(Mode *sw, const char *input)
{
    FuzzyMoverModeData *data = mode_get_private_data(sw);

    g_free(data->query);
    g_free(data->folded_query);
    g_free(data->longest_runs);
    data->query = g_strdup(input);
    data->folded_query = g_utf8_casefold(input, -1);
    data->longest_runs = g_new0(guint, data->entries->len);

    if (*data->folded_query != '\0') {
        for (guint i = 0; i < data->folded_entries->len; ++i) {
            const gchar *entry = g_ptr_array_index(data->folded_entries, i);
            data->longest_runs[i] = longest_common_substring(
                data->folded_query,
                entry);
        }
    }

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
    return helper_token_match(
        (rofi_int_matcher *const *)tokens,
        g_ptr_array_index(data->entries, index));
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
        const gchar *entry = g_ptr_array_index(data->entries, selected_line);
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
