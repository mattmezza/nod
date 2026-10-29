/* SPDX-License-Identifier: MIT */
#ifndef CUI_NUMBER_EVENTS_H
#define CUI_NUMBER_EVENTS_H
#include "cui_component_events.h"
#include "cui_numeric.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_component_event_metadata event;
    double before, after;
} cui_number_change_event;
typedef void (*cui_number_change_fn)(void *user, const cui_number_change_event *event);
typedef struct {
    cui_number_change_fn change;
    void *user;
} cui_number_event_handler;
typedef struct {
    cui_number_result control;
    size_t change_count; /* Actual synchronous calls: zero or one. */
} cui_number_event_result;
/* Aggregate numeric CHANGE, not draft revision, REQUEST, COMMIT or SUBMIT.
 * Finite exact inequality; signed zeros compare equal. Callback/user borrowed.
 * A missing callback ignores user and still reduces. Passive, disabled and
 * readonly calls retain legacy reduction but never notify. */
cui_number_event_result cui_base_number_input_events(cui_context *, cui_id, cui_str,
                                                     cui_number_state *, double *,
                                                     cui_number_options, cui_number_event_handler);
cui_number_event_result cui_number_input_events(cui_context *, cui_id, const cui_theme *, cui_str,
                                                cui_number_state *, double *, cui_number_options,
                                                cui_number_event_handler);
/* Callback may copy the pointer-free event and assign a finite owner value;
 * current semantics/paint retain the reduced draft, next submission syncs owner.
 * No editor/state mutation, rebind, recursive submission, destruction or foreign
 * exception unwinding during callbacks. Storage must outlive the outer call. */
#ifdef __cplusplus
}
#endif
#endif
