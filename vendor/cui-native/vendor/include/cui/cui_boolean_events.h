/* SPDX-License-Identifier: MIT */
#ifndef CUI_BOOLEAN_EVENTS_H
#define CUI_BOOLEAN_EVENTS_H
#include "cui_control_events.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Pointer-free request, copied before its synchronous callback. The owner may
 * accept requested or reject it. Both values remain the entry snapshot for all
 * accepted events in this invocation; no application storage is written. */
typedef struct {
    cui_component_event_metadata event;
    bool current, requested;
} cui_component_boolean_request;
typedef void (*cui_component_boolean_request_fn)(void *user,
                                                 const cui_component_boolean_request *request);
typedef struct {
    cui_component_boolean_request_fn callback;
    void *user;
} cui_component_boolean_handler;
typedef struct {
    bool disabled, skip_tab, no_pointer_focus;
    int tab_index;
} cui_boolean_event_options;
/* Submit once inside a same-ID box. Draws nothing. Missing handler is read-only.
 * Entry snapshot controls semantics, including after owner callback assignment.
 * Switch retains SWITCH role; Toggle retains BUTTON role. Disabled controls
 * preserve legacy Base parent routing, without Check's extra semantic policy.
 * Callback/user are borrowed through this call. Active-context destruction,
 * recursive frame/component submission and freeing active storage are unsupported;
 * defer teardown until submission/frame completion. No callback registration. */
cui_component_action_result cui_base_switch_events(cui_context *, cui_id, cui_str, bool value,
                                                   cui_boolean_event_options,
                                                   cui_component_boolean_handler);
cui_component_action_result cui_base_toggle_events(cui_context *, cui_id, cui_str, bool value,
                                                   cui_boolean_event_options,
                                                   cui_component_boolean_handler);
#ifdef __cplusplus
}
#endif
#endif
