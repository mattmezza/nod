/* SPDX-License-Identifier: MIT */
#ifndef CUI_COMMAND_H
#define CUI_COMMAND_H
#include "cui_base.h"
#include "cui_scroll.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
#define CUI_COMMAND_LIMIT 4096u
typedef struct {
    size_t section, row;
} cui_command_index;
typedef enum { CUI_COMMAND_ITEM, CUI_COMMAND_GROUP, CUI_COMMAND_SEPARATOR } cui_command_entry_kind;
typedef struct cui_command_state cui_command_state;
typedef void (*cui_command_content_fn)(cui_context *, const cui_theme *, const cui_command_state *,
                                       void *);
typedef struct {
    cui_str label, icon;
    const cui_str *keywords;
    size_t keyword_count;
    bool disabled, checked;
    /* An action's resolver supplies its active binding, not independent shortcut text. */
    void (*action)(void *);
    cui_str (*binding)(void *);
    void *action_user;
    cui_command_content_fn content;
    void *content_user;
    /* Complete outer row height; zero uses default text measurement. */
    float (*measure)(float content_width, float font_size, void *);
} cui_command_item;
typedef struct {
    cui_command_entry_kind kind;
    cui_command_item item;
    cui_str heading;
    const cui_command_item *items;
    size_t item_count;
} cui_command_entry;
typedef enum {
    CUI_COMMAND_ROW_ITEM,
    CUI_COMMAND_ROW_HEADING,
    CUI_COMMAND_ROW_SEPARATOR
} cui_command_row_kind;
typedef struct {
    cui_command_row_kind kind;
    cui_command_index index;
    const cui_command_item *item;
    cui_str heading;
    float offset, height;
} cui_command_row;
typedef struct {
    bool searchable, filterable, bordered, disabled;
    float width, max_height;
    cui_str placeholder, empty_message, label;
    const cui_platform *clipboard;
    bool (*matches)(const cui_command_item *, cui_str trimmed_query, void *);
    void *match_user;
    void (*on_select)(cui_command_index, void *);
    void (*on_query)(cui_str, void *);
    void (*on_confirm)(cui_command_index, void *);
    void (*on_cancel)(void *);
    void *user;
    cui_command_content_fn header, footer, empty;
    void *header_user, *footer_user, *empty_user;
} cui_command_options;
struct cui_command_state {
    cui_command_row *rows;
    size_t capacity, row_count, matched_count, selected;
    cui_edit *query;
    cui_input_behavior_state input;
    float input_scroll_x;
    cui_scroll_state scroll;
    bool loading, preserve_no_selection, pending_scroll, initialized;
    uint64_t query_hash;
    cui_id hovered_row;
};
cui_command_options cui_command_default(void);
/* Workspace and editor are caller-owned and must outlive state. No allocation.
 * Query may be NULL for pure model use; styled rendering requires an editor. */
bool cui_command_init(cui_command_state *, cui_command_row *, size_t capacity, cui_edit *query);
bool cui_command_index_equal(cui_command_index, cui_command_index);
bool cui_command_matches(const cui_command_item *, cui_str query);
/* Transactional capacity/geometry validation; rebuild borrows entries until next rebuild.
 * query_changed resets highlight; install preserves an eligible original path. */
bool cui_command_rebuild(cui_command_state *, const cui_command_entry *, size_t, cui_str query,
                         bool query_changed, cui_command_options);
bool cui_command_selected(const cui_command_state *, cui_command_index *);
bool cui_command_select(cui_command_state *, cui_command_index, bool scroll, cui_command_options);
void cui_command_clear_selection(cui_command_state *);
bool cui_command_move(cui_command_state *, bool forward, cui_command_options);
bool cui_command_confirm(cui_command_state *, cui_command_options);
cui_id cui_command_input_id(cui_id);
cui_id cui_command_list_id(cui_id);
cui_id cui_command_row_id(cui_id, cui_command_index);
void cui_command_focus(cui_context *, cui_id, cui_command_options);
/* Inline palette. Compose within an Overlay/Dialog to obtain modal hosting.
 * Returns false for invalid model/workspace/options. Invalid custom measurement
 * after an input edit also returns false after balancing submitted containers. */
bool cui_command(cui_context *, cui_id, const cui_theme *, cui_command_state *,
                 const cui_command_entry *, size_t, cui_command_options);
#ifdef __cplusplus
}
#endif
#endif
