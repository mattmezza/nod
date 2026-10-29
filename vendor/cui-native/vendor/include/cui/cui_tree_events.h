/* SPDX-License-Identifier: MIT */
#ifndef CUI_TREE_EVENTS_H
#define CUI_TREE_EVENTS_H
#include "cui_component_events.h"
#include "cui_tree.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_component_event_metadata event;
    cui_id previous, value;
    size_t node_index; /* Current model slot, not input index or stable identity. */
} cui_tree_selection_change;
typedef struct {
    cui_component_event_metadata event;
    cui_id key;
    size_t node_index;
    bool previous, expanded;
} cui_tree_expansion_change;
typedef struct {
    cui_component_event_metadata event;
    cui_id key;
    size_t node_index;
} cui_tree_activation;
typedef struct {
    void (*on_selection_change)(void *, const cui_tree_selection_change *);
    void (*on_expansion_change)(void *, const cui_tree_expansion_change *);
    void (*on_activate)(void *, const cui_tree_activation *);
    void *user;
} cui_tree_handler;
typedef struct {
    cui_tree_result result;  /* Exact legacy aggregate, including net-zero false. */
    size_t activation_count; /* Every row invocation, including already selected. */
    size_t expansion_count;  /* Applied bool differences, after model rebuild attempt. */
    size_t selection_count;  /* Actual calls; saturates at SIZE_MAX. */
    bool count_overflow;     /* Any actual-callback count saturated. */
} cui_tree_event_result;
/* Applied selection/expansion transitions and row invocations. Selection
 * notifications follow each different owned-key write; expansion follows setter/
 * rebuild attempt; row activation follows selected/cursor/legacy result assignment.
 * Missing observer permits writes. Payload pointer borrowed during call;
 * pointer-free value may be copied. event.target is compound; activation.target
 * retains genuine origin if available. Normal reduce-before-submit may have only
 * frame/index/type-source evidence. No passive/layout-only/disabled observation.
 * Do not mutate/rebuild/free active nodes/labels/expanded/model/state/selected,
 * input or context, or recursively submit UI. Unrelated flags allowed; defer
 * removal/teardown until outer return plus renderer consumption. */
cui_tree_event_result cui_tree_reduce_events(cui_context *, cui_id, cui_tree_model *,
                                             cui_tree_state *, cui_id *selected, cui_tree_options,
                                             cui_tree_handler);
cui_tree_event_result cui_base_tree_events(cui_context *, cui_id, cui_tree_model *,
                                           cui_tree_state *, cui_id *selected, cui_tree_options,
                                           cui_tree_handler);
cui_tree_event_result cui_tree_events(cui_context *, cui_id, const cui_theme *, cui_tree_model *,
                                      cui_tree_state *, cui_id *selected, cui_tree_options,
                                      cui_tree_handler);
#ifdef __cplusplus
}
#endif
#endif
