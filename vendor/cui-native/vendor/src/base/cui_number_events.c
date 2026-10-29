/* SPDX-License-Identifier: MIT */
#include "cui/cui_number_events.h"
#include <math.h>
cui_number_event_result cui_base_number_input_events(cui_context *ui, cui_id id, cui_str name,
                                                     cui_number_state *state, double *value,
                                                     cui_number_options options,
                                                     cui_number_event_handler handler) {
    uint64_t frame = cui_frame_sequence(ui);
    bool observe = handler.change && frame && id && value && isfinite(*value) &&
                   !options.disabled && !options.readonly;
    double before = observe ? *value : 0;
    cui_number_event_result result = {0};
    result.control = cui_base_number_input(ui, id, name, state, value, options);
    if (observe && cui_frame_sequence(ui) == frame && isfinite(*value) && before != *value) {
        cui_number_change_event event = {0};
        (void)cui_component_event_init(&event.event, CUI_COMPONENT_EVENT_CHANGE, id,
                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
        event.event.flags = CUI_COMPONENT_EVENT_HAS_FRAME;
        event.event.frame_sequence = frame;
        event.before = before;
        event.after = *value;
        result.change_count = 1;
        handler.change(handler.user, &event);
    }
    return result;
}
