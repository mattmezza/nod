/* SPDX-License-Identifier: MIT */
#ifndef CUI_DISCLOSURE_EVENTS_H
#define CUI_DISCLOSURE_EVENTS_H
#include "cui_component_events.h"
#include "cui_disclosure.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Applied bool transition; item_index is a model slot, not an input index.
 * event.target is the compound; copied activation.target retains its origin. */
typedef struct {
    cui_component_event_metadata event;
    cui_id item;
    size_t item_index;
    bool previous, expanded;
} cui_disclosure_change;
typedef void (*cui_disclosure_change_fn)(void *, const cui_disclosure_change *);
typedef struct {
    cui_disclosure_change_fn on_change;
    void *user;
} cui_disclosure_handler;
typedef struct {
    bool changed;        /* Exact legacy return value. */
    size_t change_count; /* Actual callbacks; saturates at SIZE_MAX. */
    bool count_overflow;
} cui_disclosure_event_result;
/* Immediate notification after each write, before final semantics/Styled paint.
 * Missing observer permits writes. No passive/layout-only/disabled observations.
 * Callback/payload/user borrowed during call; payload is pointer-free/copyable.
 * Do not mutate active owner state/arrays/input/context or submit UI in observer.
 * Unrelated flags allowed; defer teardown until outer return and renderer use. */
cui_disclosure_event_result cui_base_collapsible_events(cui_context *, cui_id, cui_str, bool *open,
                                                        bool disabled, cui_disclosure_handler);
cui_disclosure_event_result cui_collapsible_events(cui_context *, cui_id, const cui_theme *,
                                                   cui_str, bool *open, cui_collapsible_options,
                                                   cui_disclosure_panel_fn, void *panel_user,
                                                   cui_disclosure_handler);
/* Single-open closes true siblings in model order (including disabled siblings),
 * then opens the selected slot. Observer sees intermediate arrays and old semantics. */
cui_disclosure_event_result cui_base_accordion_events(cui_context *, cui_id target,
                                                      const cui_disclosure_item *, size_t count,
                                                      bool *open, cui_accordion_options,
                                                      cui_disclosure_handler);
cui_disclosure_event_result cui_accordion_events(cui_context *, cui_id container, const cui_theme *,
                                                 const cui_disclosure_item *, size_t count,
                                                 bool *open, cui_accordion_options,
                                                 cui_disclosure_panel_fn, void *panel_user,
                                                 cui_disclosure_handler);
#ifdef __cplusplus
}
#endif
#endif
