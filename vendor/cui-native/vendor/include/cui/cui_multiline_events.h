/* SPDX-License-Identifier: MIT */
#ifndef CUI_MULTILINE_EVENTS_H
#define CUI_MULTILINE_EVENTS_H
#include "cui_text_input_events.h"
#include "cui_textarea.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Aggregate notification: at most one CHANGE per submission, never SUBMIT.
 * Payload before/after describes committed state and primary selection only.
 * Metadata has the actual frame, UNKNOWN source, no event index/activation.
 * Text is callback-borrowed; multiline controls do not provide password masking.
 * Observers must not mutate/rebind/destroy model, state, context or borrowed
 * storage, nor recursively submit UI. Unrelated app state is allowed; defer
 * omission/destruction until frame and renderer consumption. No runtime guard. */
typedef struct {
    cui_textarea_result control; /* Unchanged legacy result, including errors. */
    size_t change_count;         /* Zero or one actual callback invocation. */
} cui_multiline_event_result;
cui_multiline_event_result cui_base_textarea_events(cui_context *, cui_id, cui_str name, cui_edit *,
                                                    cui_textarea_state *, cui_textarea_options,
                                                    cui_textarea_metrics,
                                                    cui_component_text_handler);
cui_multiline_event_result cui_textarea_events(cui_context *, cui_id, const cui_theme *,
                                               cui_str name, cui_edit *, cui_textarea_state *,
                                               cui_textarea_options, cui_component_text_handler);
#ifdef __cplusplus
}
#endif
#endif
