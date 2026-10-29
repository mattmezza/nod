/* SPDX-License-Identifier: MIT */
#include "cui/cui_otp_events.h"
#include <string.h>
cui_otp_event_result cui_base_otp_events(cui_context *ui, cui_id id, cui_otp_state *state,
                                         cui_otp_options options, cui_otp_event_handler handler) {
    uint64_t frame = cui_frame_sequence(ui);
    bool observe = handler.change && frame && id && state && state->buffer && state->capacity &&
                   state->length <= 256 && !options.disabled;
    cui_str before = {0};
    size_t before_count = 0;
    if (observe) {
        /* Capacity is caller-owned readable storage, never an unbounded strlen. */
        const char *end = memchr(state->buffer, 0, state->capacity);
        observe = end != NULL;
        if (observe) {
            before = (cui_str){state->buffer, (size_t)(end - state->buffer)};
            observe = cui_edit_valid_utf8(before);
            if (observe)
                before_count = cui_otp_count(state);
        }
    }
    cui_otp_event_result result = {0};
    result.control = cui_base_otp(ui, id, state, options);
    if (observe && cui_frame_sequence(ui) == frame && result.control.changed) {
        cui_str after = cui_otp_value(state);
        cui_otp_change_event event = {0};
        (void)cui_component_event_init(&event.event, CUI_COMPONENT_EVENT_CHANGE, id,
                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
        event.event.flags = CUI_COMPONENT_EVENT_HAS_FRAME;
        event.event.frame_sequence = frame;
        event.before_bytes = before.length;
        event.after_bytes = after.length;
        event.before_count = before_count;
        event.after_count = cui_otp_count(state);
        event.target_length = state->length;
        event.accepted_count = result.control.change_count;
        event.completion_count = result.control.complete_count;
        event.final_complete = event.after_count == event.target_length;
        event.has_text = !state->masked;
        if (event.has_text)
            event.text = after;
        result.change_count = 1;
        handler.change(handler.user, &event);
    }
    return result;
}
