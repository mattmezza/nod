/* SPDX-License-Identifier: MIT */
#include "cui/cui_numeric_selection_events.h"
#include <math.h>
cui_numeric_selection_event_result
cui_base_pagination_events(cui_context *ui, cui_id id, size_t *page, size_t total, bool compact,
                           bool disabled, cui_pagination_event_handler handler) {
    uint64_t frame = cui_frame_sequence(ui);
    bool observe = handler.change && frame && id && page && !disabled;
    size_t before = observe ? *page : 0;
    cui_numeric_selection_event_result result = {0};
    result.control = cui_base_pagination(ui, id, page, total, compact, disabled);
    if (observe && cui_frame_sequence(ui) == frame && before != *page) {
        cui_pagination_change_event event = {0};
        (void)cui_component_event_init(&event.event, CUI_COMPONENT_EVENT_CHANGE, id,
                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
        event.event.flags = CUI_COMPONENT_EVENT_HAS_FRAME;
        event.event.frame_sequence = frame;
        event.before = before;
        event.after = *page;
        event.total = total;
        result.change_count = 1;
        handler.change(handler.user, &event);
    }
    return result;
}
cui_numeric_selection_event_result cui_base_stepper_events(cui_context *ui, cui_id id,
                                                           const cui_step *steps, size_t count,
                                                           size_t *current, bool disabled,
                                                           cui_stepper_event_handler handler) {
    uint64_t frame = cui_frame_sequence(ui);
    bool observe =
        handler.change && frame && id && current && !disabled && (steps || !count) && count <= 100;
    size_t before = observe ? *current : 0;
    cui_numeric_selection_event_result result = {0};
    result.control = cui_base_stepper(ui, id, steps, count, current, disabled);
    if (observe && cui_frame_sequence(ui) == frame && before != *current) {
        cui_stepper_change_event event = {0};
        (void)cui_component_event_init(&event.event, CUI_COMPONENT_EVENT_CHANGE, id,
                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
        event.event.flags = CUI_COMPONENT_EVENT_HAS_FRAME;
        event.event.frame_sequence = frame;
        event.before = before;
        event.after = *current;
        event.count = count;
        result.change_count = 1;
        handler.change(handler.user, &event);
    }
    return result;
}
cui_numeric_selection_event_result cui_base_rating_events(cui_context *ui, cui_id id, cui_str name,
                                                          float *value, cui_rating_options options,
                                                          cui_rating_event_handler handler) {
    uint64_t frame = cui_frame_sequence(ui);
    bool observe = handler.change && frame && id && value && !options.disabled && !options.readonly;
    float before = observe ? *value : 0;
    cui_numeric_selection_event_result result = {0};
    result.control = cui_base_rating(ui, id, name, value, options);
    if (observe && cui_frame_sequence(ui) == frame && isfinite(*value) &&
        (!isfinite(before) || before != *value)) {
        cui_rating_change_event event = {0};
        (void)cui_component_event_init(&event.event, CUI_COMPONENT_EVENT_CHANGE, id,
                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
        event.event.flags = CUI_COMPONENT_EVENT_HAS_FRAME;
        event.event.frame_sequence = frame;
        event.before = before;
        event.after = *value;
        event.before_finite = isfinite(before);
        event.maximum = options.maximum ? options.maximum : 5;
        if (event.maximum > 100)
            event.maximum = 100;
        result.change_count = 1;
        handler.change(handler.user, &event);
    }
    return result;
}
