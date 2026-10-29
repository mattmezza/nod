/* SPDX-License-Identifier: MIT */
#include "cui/cui_boolean_events.h"

static void count_event(size_t *count, bool *overflow) {
    if (*count == SIZE_MAX)
        *overflow = true;
    else
        ++*count;
}
static cui_component_action_result boolean_events(cui_context *ui, cui_id id, cui_str name,
                                                  bool value, cui_role role,
                                                  cui_boolean_event_options options,
                                                  cui_component_boolean_handler handler) {
    cui_component_action_result result = {0};
    if (!ui || !id || (name.length && !name.data))
        return result;
    result.valid = true;
    /* Frame sequence also excludes inert/layout-only and protected phases. */
    if (!cui_frame_sequence(ui))
        return result;
    result.submitted = true;
    (void)cui_interact(ui, id, role, name, options.disabled, value, 0);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.no_pointer_focus = options.no_pointer_focus,
                               .pointer_activatable = options.no_pointer_focus});
    cui_set_tab_index(ui, id, options.tab_index);
    cui_set_tab_stop(ui, id, !options.skip_tab);
    size_t count = 0;
    (void)cui_frame_events(ui, &count);
    for (size_t index = 0; !options.disabled && index < count; ++index) {
        cui_activation_snapshot snapshot;
        if (cui_event_accepted_activation(ui, index) != id ||
            !cui_event_activation_snapshot(ui, index, &snapshot))
            continue;
        cui_component_boolean_request request = {.current = value, .requested = !value};
        if (!cui_component_event_from_activation(&request.event, CUI_COMPONENT_EVENT_REQUEST,
                                                 &snapshot))
            continue;
        count_event(&result.activation_count, &result.count_overflow);
        if (handler.callback) {
            count_event(&result.request_count, &result.count_overflow);
            handler.callback(handler.user, &request);
        }
    }
    return result;
}
cui_component_action_result cui_base_switch_events(cui_context *ui, cui_id id, cui_str name,
                                                   bool value, cui_boolean_event_options options,
                                                   cui_component_boolean_handler handler) {
    return boolean_events(ui, id, name, value, CUI_ROLE_SWITCH, options, handler);
}
cui_component_action_result cui_base_toggle_events(cui_context *ui, cui_id id, cui_str name,
                                                   bool value, cui_boolean_event_options options,
                                                   cui_component_boolean_handler handler) {
    return boolean_events(ui, id, name, value, CUI_ROLE_BUTTON, options, handler);
}
