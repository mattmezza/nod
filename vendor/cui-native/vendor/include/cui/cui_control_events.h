/* SPDX-License-Identifier: MIT */
#ifndef CUI_CONTROL_EVENTS_H
#define CUI_CONTROL_EVENTS_H
#include "cui_base.h"
#include "cui_component_events.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*cui_component_activation_fn)(void *, const cui_component_event_metadata *);
typedef struct {
    cui_component_activation_fn callback;
    void *user;
} cui_component_activation_handler;
typedef struct {
    cui_component_event_metadata event; /* REQUEST; copyable by value. */
    cui_check_state current, requested;
} cui_component_check_request;
typedef void (*cui_component_check_request_fn)(void *, const cui_component_check_request *);
typedef struct {
    cui_component_check_request_fn callback;
    void *user;
} cui_component_check_handler;
typedef struct {
    bool valid, submitted, count_overflow;
    size_t activation_count, request_count;
} cui_component_action_result;
/* Private forwarding bookkeeping; no allocation, registration or dispatch. */
static inline void cui_control_events_private_increment(size_t *count, bool *overflow) {
    if (*count == SIZE_MAX)
        *overflow = true;
    else
        ++*count;
}
typedef struct {
    cui_check_state current;
    cui_component_check_handler handler;
    cui_component_action_result *result;
} cui_control_events_private_check;
static inline void
cui_control_events_private_check_request(void *user, cui_check_state requested,
                                         const cui_activation_snapshot *snapshot) {
    cui_control_events_private_check *forward = (cui_control_events_private_check *)user;
    cui_component_check_request request;
    if (!cui_component_event_from_activation(&request.event, CUI_COMPONENT_EVENT_REQUEST, snapshot))
        return;
    request.current = forward->current;
    request.requested = requested;
    cui_control_events_private_increment(&forward->result->activation_count,
                                         &forward->result->count_overflow);
    if (forward->handler.callback) {
        cui_control_events_private_increment(&forward->result->request_count,
                                             &forward->result->count_overflow);
        forward->handler.callback(forward->handler.user, &request);
    }
}
/* Immutable entry snapshot. Every accepted activation proposes the same next
 * value; missing handler is read-only. Old on_change/change_user must be NULL.
 * Base pointer focus follows options; no hidden CHANGE/commit notification.
 * Payload/handler/user borrowed only through this call; payload copyable by value.
 * Do not recursively submit this control/frame, destroy context, or free active
 * state/resources/users during dispatch. Request teardown in application state
 * and perform it after frame completion. There is no runtime destruction guard. */
static inline cui_component_action_result
cui_base_check_events(cui_context *ui, cui_id id, cui_str name, cui_check_state value,
                      cui_check_options options, cui_component_check_handler handler) {
    cui_component_action_result result;
    memset(&result, 0, sizeof(result));
    if (!ui || !id || (name.length && !name.data) || value < CUI_CHECK_UNCHECKED ||
        value > CUI_CHECK_MIXED || options.on_change || options.change_user ||
        (options.custom_role &&
         (options.role < CUI_ROLE_NONE || options.role > CUI_ROLE_RADIOGROUP)))
        return result;
    result.valid = true;
    result.submitted = cui_frame_is_submitting(ui) && cui_frame_sequence(ui) != 0;
    cui_control_events_private_check forward = {value, handler, &result};
    options.controlled = true;
    options.on_change = cui_control_events_private_check_request;
    options.change_user = &forward;
    (void)cui_base_check_ex(ui, id, name, &value, options);
    return result;
}
#ifdef __cplusplus
}
#endif
#endif
