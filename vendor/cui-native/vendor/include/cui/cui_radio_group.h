/* SPDX-License-Identifier: MIT */
#ifndef CUI_RADIO_GROUP_H
#define CUI_RADIO_GROUP_H
#include "cui_components.h"
#include "cui_selection_events.h"
#ifdef __cplusplus
extern "C" {
#endif
#define CUI_RADIO_GROUP_MAX 64
typedef struct {
    cui_id id;
    cui_str label;
    bool disabled;
} cui_radio_group_item;
typedef struct {
    cui_id focused;
    bool had_focus;
} cui_radio_group_state;
typedef void (*cui_radio_group_change_fn)(void *user, cui_id requested);
typedef struct {
    bool vertical, disabled, required, invalid, hide_focus_ring, skip_tab;
    float gap; /* zero defaults to 12 logical pixels; negative/NaN invalid. */
    cui_control_size size;
    cui_design_size design_size;
    cui_str name;
    cui_radio_group_change_fn on_change;
    void *change_user;
} cui_radio_group_options;
typedef struct {
    bool valid, requested, changed;
    cui_id tab_stop;
} cui_radio_group_result;
/* Stable nonzero unique item IDs, <=64 items, selected0 means none. Callback
 * requests never mutate *selected; caller may synchronously accept or reject.
 * Without callback activation writes *selected. No callback for already selected.
 * Invalid/removed selection paints none; caller owns cleanup of its model. */
cui_radio_group_result cui_base_radio_group(cui_context *, cui_id, const cui_radio_group_item *,
                                            size_t count, cui_id *selected, cui_radio_group_state *,
                                            cui_radio_group_options);
cui_radio_group_result cui_radio_group(cui_context *, cui_id, const cui_theme *,
                                       const cui_radio_group_item *, size_t count, cui_id *selected,
                                       cui_radio_group_state *, cui_radio_group_options);
/* Additive post-transition observers. Legacy on_change remains a synchronous
 * owner request and runs first. Missing observer preserves ordinary mutation.
 * No context destruction/freeing active state or recursive submission in callbacks. */
typedef struct {
    cui_selection_event_result events;
    cui_radio_group_result legacy;
} cui_radio_group_event_result;
cui_radio_group_event_result cui_base_radio_group_events(cui_context *, cui_id,
                                                         const cui_radio_group_item *, size_t,
                                                         cui_id *, cui_radio_group_state *,
                                                         cui_radio_group_options,
                                                         cui_selection_handler);
cui_radio_group_event_result cui_radio_group_events(cui_context *, cui_id, const cui_theme *,
                                                    const cui_radio_group_item *, size_t, cui_id *,
                                                    cui_radio_group_state *,
                                                    cui_radio_group_options, cui_selection_handler);
#ifdef __cplusplus
}
#endif
#endif
