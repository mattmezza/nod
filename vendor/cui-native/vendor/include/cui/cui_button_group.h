/* SPDX-License-Identifier: MIT */
#ifndef CUI_BUTTON_GROUP_H
#define CUI_BUTTON_GROUP_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { CUI_BUTTON_GROUP_MAX_ITEMS = 256 };
typedef enum {
    CUI_BUTTON_GROUP_SOURCE_POINTER_CALLBACKS,
    CUI_BUTTON_GROUP_ALL_ACTIVATIONS
} cui_button_group_activation_policy;
typedef struct {
    cui_id activated_id;
    size_t item_index; /* SIZE_MAX for collection background. */
    size_t event_index;
    const size_t *selected_indices; /* Borrowed until callback returns. */
    size_t selected_count;
} cui_button_group_request;
typedef void (*cui_button_group_change_fn)(void *, const cui_button_group_request *);
typedef void (*cui_button_group_child_click_fn)(void *);
typedef void (*cui_button_group_child_event_fn)(void *, cui_context *, size_t event_index);
/* Preparation hook, before legacy/post-legacy notifications. Snapshot is borrowed
 * for this call and can be copied. Excluded from result.callbacks. */
typedef void (*cui_button_group_child_snapshot_fn)(void *, cui_context *,
                                                   const cui_activation_snapshot *);
typedef struct {
    cui_id id;
    cui_str label;
    bool disabled, loading, skip_tab, prevent_pointer_focus, custom_cursor;
    cui_cursor cursor;
    cui_role role; /* Zero/NONE derives BUTTON; otherwise BUTTON or LINK. */
    cui_button_group_child_click_fn on_click;
    void *action_user;
    cui_button_group_child_event_fn on_click_event; /* After legacy; on_change overrides both. */
    void *event_user;
    cui_button_group_child_snapshot_fn
        prepare_click_event; /* on_change overrides all child hooks. */
} cui_button_group_item;
typedef struct {
    bool multiple, disabled, vertical;
    cui_button_group_activation_policy activation_policy;
    cui_str accessibility_label;
    cui_button_group_change_fn on_change;
    void *change_user;
} cui_button_group_options;
typedef struct {
    bool valid;
    size_t accepted_activations, callbacks;
} cui_button_group_result;
/* Caller-owned transient workspace. Zero initialize; no cleanup/allocation.
 * One active group per workspace, begin/item/end in the same ordinary frame.
 * Descriptors are copied; selected remains caller-owned and is read between
 * callbacks. Reset before context recreation or a complete uint64_t frame cycle. */
typedef struct {
    cui_context *context;
    uint64_t frame_sequence;
    bool *selected;
    size_t count, registered_count;
    cui_id id;
    cui_button_group_options options;
    cui_button_group_item items[CUI_BUTTON_GROUP_MAX_ITEMS];
    cui_interaction interactions[CUI_BUTTON_GROUP_MAX_ITEMS];
    bool registered[CUI_BUTTON_GROUP_MAX_ITEMS];
    size_t requested_indices[CUI_BUTTON_GROUP_MAX_ITEMS];
    bool active, failed, dispatching;
} cui_button_group_workspace;
/* Caller opens/submits actual group/child layout boxes. Begin submits collection
 * semantics only; item submits one Button/Link; end dispatches accepted events
 * in input order, then publishes final checked/value. No implicit model mutation. */
bool cui_base_button_group_begin(cui_context *, cui_id, const cui_button_group_item *,
                                 bool *selected, size_t count, cui_button_group_workspace *,
                                 cui_button_group_options);
cui_interaction cui_base_button_group_item(cui_context *, cui_button_group_workspace *, size_t);
cui_button_group_result cui_base_button_group_end(cui_context *, cui_button_group_workspace *);
#ifdef __cplusplus
}
#endif
#endif
