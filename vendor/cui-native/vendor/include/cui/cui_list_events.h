/* SPDX-License-Identifier: MIT */
#ifndef CUI_LIST_EVENTS_H
#define CUI_LIST_EVENTS_H
#include "cui_choice.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_selection_event_result events;
    bool legacy_activated;
} cui_list_event_result;
/* Per-transition CHANGE after legacy activate/confirm, before final semantics/
 * Styled paint. Same-item confirmation is activation, not necessarily CHANGE.
 * Callback/user borrowed synchronously; missing callback ignores user and reduces.
 * Do not mutate/rebind owner, state, descriptors or borrowed resources, recurse,
 * destroy storage/context or unwind foreign exceptions in typed callbacks.
 * Copy pointer-free payloads or change unrelated application state; defer model
 * edits/removal/teardown until submission and renderer consumption finish.
 * Existing legacy callbacks retain their old timing and owner-update behavior. */
cui_list_event_result cui_base_list_events(cui_context *, cui_id control, const cui_choice_item *,
                                           size_t count, cui_id *selected, cui_list_state *,
                                           cui_list_options, cui_selection_handler);
cui_list_event_result cui_list_events(cui_context *, cui_id container, const cui_theme *,
                                      const cui_choice_item *, size_t count, cui_id *selected,
                                      cui_list_state *, cui_list_options, cui_selection_handler);
typedef struct {
    cui_component_event_metadata event;
    cui_id item, trigger_item;
    bool previous, value;
} cui_choice_item_change;
typedef void (*cui_choice_item_change_fn)(void *user, const cui_choice_item_change *event);
typedef struct {
    cui_choice_item_change_fn on_change;
    void *user;
} cui_choice_item_handler;
typedef struct {
    bool valid, submitted, legacy_result, count_overflow;
    size_t change_count;
} cui_choice_items_event_result;
/* Multi List observes each real bool transition after legacy activate/confirm.
 * Same lifetime/mutation restrictions as single List. Arrays borrow count entries;
 * descriptors and selected-array ordering must remain stable through submission. */
cui_choice_items_event_result cui_base_list_multi_events(cui_context *, cui_id control,
                                                         const cui_choice_item *, size_t count,
                                                         bool *selected, cui_list_state *,
                                                         cui_list_options, cui_choice_item_handler);
cui_choice_items_event_result cui_list_multi_events(cui_context *, cui_id container,
                                                    const cui_theme *, const cui_choice_item *,
                                                    size_t count, bool *selected, cui_list_state *,
                                                    cui_list_options, cui_choice_item_handler);
/* ToggleGroup emits each actual bool write: sibling clears in array order,
 * then the activated item's new value. Callbacks see intermediate arrays.
 * trigger_item stays the causal activated item for all clears. No workspace. */
cui_choice_items_event_result cui_base_toggle_group_events(cui_context *, cui_id control,
                                                           const cui_choice_item *, size_t count,
                                                           bool *selected, cui_toggle_group_options,
                                                           cui_choice_item_handler);
cui_choice_items_event_result cui_toggle_group_events(cui_context *, cui_id container,
                                                      const cui_theme *, const cui_choice_item *,
                                                      size_t count, bool *selected,
                                                      cui_toggle_group_options,
                                                      cui_choice_item_handler);
#ifdef __cplusplus
}
#endif
#endif
