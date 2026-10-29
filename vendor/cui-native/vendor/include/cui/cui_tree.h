/* SPDX-License-Identifier: MIT */
#ifndef CUI_TREE_H
#define CUI_TREE_H
#include "cui.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_id key;
    cui_str label;
    unsigned depth;
    bool disabled;
} cui_tree_node;
typedef struct cui_tree_model cui_tree_model;
typedef enum {
    CUI_TREE_OK,
    CUI_TREE_INVALID,
    CUI_TREE_CAPACITY,
    CUI_TREE_DUPLICATE_KEY
} cui_tree_error;
cui_tree_model *cui_tree_create(size_t capacity);
void cui_tree_destroy(cui_tree_model *);
/* Preorder: first depth=0, depth may increase only by one, maximum depth=63.
 * Borrowed nodes/expanded arrays must remain valid until next successful rebuild.
 * Rebuild is atomic on validation failure; no allocation after create. */
cui_tree_error cui_tree_rebuild(cui_tree_model *, const cui_tree_node *, bool *expanded,
                                size_t count);
size_t cui_tree_count(const cui_tree_model *);
size_t cui_tree_visible_count(const cui_tree_model *);
size_t cui_tree_memory_bytes(const cui_tree_model *);
bool cui_tree_get_node(const cui_tree_model *, size_t index, cui_tree_node *);
bool cui_tree_expanded(const cui_tree_model *, size_t index);
bool cui_tree_visible_node(const cui_tree_model *, size_t visible_index, size_t *node_index);
bool cui_tree_node_index(const cui_tree_model *, cui_id key, size_t *node_index);
bool cui_tree_parent(const cui_tree_model *, size_t node_index, size_t *parent_index);
bool cui_tree_is_branch(const cui_tree_model *, size_t node_index);
bool cui_tree_set_expanded(cui_tree_model *, cui_id key, bool expanded);
typedef struct {
    size_t begin, end;
    float before, after, total, scroll;
} cui_tree_view;
cui_tree_view cui_tree_viewport(const cui_tree_model *, float scroll, float height,
                                float row_height, size_t overscan);
typedef struct {
    cui_id cursor;
    char search[64];
    size_t search_length;
    float search_time, scroll;
} cui_tree_state;
typedef struct {
    bool disabled, manual_selection;
    float width, height, row_height, dt;
    size_t overscan;
    const cui_scrollbar_style *scrollbar_style;
} cui_tree_options;
typedef struct {
    bool selection_changed, expansion_changed;
    cui_id activated;
    cui_tree_error error;
} cui_tree_result;
cui_id cui_tree_row_id(cui_id container, cui_id node_key);
cui_id cui_tree_toggle_id(cui_id container, cui_id node_key);
/* Applied input observers: see cui_tree_events.h. */
/* Base handles input/semantics without drawing; layout visible row/toggle IDs. */
cui_tree_result cui_tree_reduce(cui_context *, cui_id, cui_tree_model *, cui_tree_state *,
                                cui_id *selected, cui_tree_options);
void cui_tree_submit(cui_context *, cui_id, cui_tree_model *, cui_tree_state *, cui_id selected,
                     cui_tree_options);
cui_tree_result cui_base_tree(cui_context *, cui_id, cui_tree_model *, cui_tree_state *,
                              cui_id *selected, cui_tree_options);
cui_tree_result cui_tree(cui_context *, cui_id, const cui_theme *, cui_tree_model *,
                         cui_tree_state *, cui_id *selected, cui_tree_options);
#ifdef __cplusplus
}
#endif
#endif
