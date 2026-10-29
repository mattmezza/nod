/* SPDX-License-Identifier: MIT */
#ifndef CUI_TEXT_INPUT_EVENTS_H
#define CUI_TEXT_INPUT_EVENTS_H
#include "cui_base.h"
#include "cui_component_events.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Post-mutation notification, never a rejectable proposed edit. Scalar snapshots
 * use UTF-8 byte offsets; not a complete multicursor selection or old text. */
typedef struct {
    cui_component_event_metadata event; /* CHANGE or SUBMIT; no activation. */
    cui_edit_state before, after;
    bool has_text;
    cui_str text; /* Committed model bytes borrowed only through this callback.
                   * Empty/redacted when masked or a password purpose is set. */
} cui_component_text_event;
typedef void (*cui_component_text_fn)(void *, const cui_component_text_event *);
typedef struct {
    cui_component_text_fn callback;
    void *user;
} cui_component_text_handler;
typedef struct {
    cui_input_result control;          /* Unchanged legacy aggregate result. */
    size_t change_count, submit_count; /* Actual callback invocations. */
    bool count_overflow;
} cui_input_event_result;
/* Existing owned-model semantics. Missing observer does not make input readonly.
 * Callback/user borrowed synchronously. No context/model/state/buffer mutation,
 * rebind, destruction or recursive submission during observer dispatch. Changes
 * to unrelated application state are allowed; defer edits/removal/teardown until
 * frame completion/renderer consumption. This is a precondition, not a guard.
 * Result reads never dispatch. No REQUEST/COMMIT or general selection event. */
cui_input_event_result cui_base_input_events(cui_context *, cui_id, cui_str, cui_edit *,
                                             cui_input_behavior_state *, cui_base_input_options,
                                             cui_component_text_handler);
/* Register textbox first; reduce exactly once per event in increasing order.
 * Do not also call the legacy reducer or whole-frame facade for those events. */
cui_input_event_result cui_base_input_reduce_event_events(cui_context *, cui_id, cui_edit *,
                                                          cui_input_behavior_state *,
                                                          cui_base_input_options,
                                                          size_t event_index,
                                                          cui_component_text_handler);
#ifdef __cplusplus
}
#endif
#endif
