/* SPDX-License-Identifier: MIT */
#ifndef CUI_FORM_EVENTS_H
#define CUI_FORM_EVENTS_H
#include "cui_form.h"
#include "cui_text_input_events.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_component_text_fn on_text;
    void (*on_addon)(void *, const cui_component_event_metadata *);
    void *user;
} cui_input_group_handler;
typedef struct {
    cui_input_group_result control;
    size_t addon_count, change_count, submit_count;
    bool count_overflow;
} cui_input_group_event_result;
/* Addons notify accepted ACTIVATE inputs in descriptor order then per-addon input
 * order, after existing addon semantics. Nested Input CHANGE/SUBMIT follows using
 * its existing per-event reducer, before Styled paint. Counts actual callbacks;
 * legacy last-addon/aggregate input result unchanged. Missing handlers still edit.
 * Addon target is its ID; text target is nested Input ID, not group. Secure text
 * payloads inherit Input redaction; this does not promise Styled secure paint.
 * No passive/inert/layout-only callbacks or custom child action forwarding.
 * Handler/user/payload pointers borrowed; text borrowed only during callback.
 * Copy scalar metadata or use bounded UTF-8 copy to retain text. Do not mutate,
 * free/rebind active model/state/descriptors/input/context, recursively submit UI,
 * or destroy resources in observers. Unrelated flags allowed; defer teardown until
 * outer return plus frame/renderer consumption. No reject/COMMIT semantics. */
cui_input_group_event_result cui_base_input_group_events(cui_context *, cui_id group, cui_id input,
                                                         cui_edit *, cui_input_group_state *,
                                                         const cui_input_group_addon *, size_t,
                                                         cui_input_group_options,
                                                         cui_input_group_handler);
cui_input_group_event_result
cui_input_group_events(cui_context *, cui_id group, cui_id input, const cui_theme *, cui_edit *,
                       cui_input_group_state *, const cui_input_group_addon *, size_t,
                       cui_input_group_options, cui_input_group_handler);
#ifdef __cplusplus
}
#endif
#endif
