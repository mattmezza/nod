/* SPDX-License-Identifier: MIT */
#include "cui/cui_picker_events.h"
static cui_calendar_event_result
cui_calendar_events_common(cui_context *ui, cui_id id, cui_calendar_state *state,
                           cui_date_selection *selection, cui_calendar_options options,
                           cui_date_selection_handler handler, bool reduce_only) {
    cui_calendar_event_result result = {0};
    if (!ui || !id || !state || !selection)
        return result;
    cui_date_selection before = *selection;
    uint64_t frame = !options.disabled && cui_frame_paint_enabled(ui) ? cui_frame_sequence(ui) : 0;
    result.control = reduce_only ? cui_calendar_reduce(ui, id, state, selection, options)
                                 : cui_base_calendar(ui, id, state, selection, options);
    if (frame && handler.on_change &&
        (cui_date_compare(before.start, selection->start) ||
         cui_date_compare(before.end, selection->end))) {
        cui_date_selection_change event = {.event = {.kind = CUI_COMPONENT_EVENT_CHANGE,
                                                     .target = id,
                                                     .source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN,
                                                     .flags = CUI_COMPONENT_EVENT_HAS_FRAME,
                                                     .frame_sequence = frame},
                                           .previous = before,
                                           .value = *selection};
        handler.on_change(handler.user, &event);
        result.change_count = 1;
    }
    return result;
}
cui_calendar_event_result cui_calendar_reduce_events(cui_context *ui, cui_id id,
                                                     cui_calendar_state *state,
                                                     cui_date_selection *selection,
                                                     cui_calendar_options options,
                                                     cui_date_selection_handler handler) {
    return cui_calendar_events_common(ui, id, state, selection, options, handler, true);
}
cui_calendar_event_result cui_base_calendar_events(cui_context *ui, cui_id id,
                                                   cui_calendar_state *state,
                                                   cui_date_selection *selection,
                                                   cui_calendar_options options,
                                                   cui_date_selection_handler handler) {
    return cui_calendar_events_common(ui, id, state, selection, options, handler, false);
}

cui_calendar_event_result
cui_base_date_picker_events(cui_context *ui, cui_id trigger, cui_id calendar, cui_str name,
                            cui_date_picker_state *state, cui_date_selection *selection,
                            cui_date_picker_options options, cui_date_selection_handler handler) {
    cui_calendar_event_result result = {0};
    if (!ui || !trigger || !calendar || !state || !selection)
        return result;
    cui_date_selection before = *selection;
    uint64_t frame = !options.disabled && !options.calendar.disabled && cui_frame_paint_enabled(ui)
                         ? cui_frame_sequence(ui)
                         : 0;
    result.control = cui_base_date_picker(ui, trigger, calendar, name, state, selection, options);
    if (frame && handler.on_change &&
        (cui_date_compare(before.start, selection->start) ||
         cui_date_compare(before.end, selection->end))) {
        cui_date_selection_change event = {.event = {.kind = CUI_COMPONENT_EVENT_CHANGE,
                                                     .target = trigger,
                                                     .source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN,
                                                     .flags = CUI_COMPONENT_EVENT_HAS_FRAME,
                                                     .frame_sequence = frame},
                                           .previous = before,
                                           .value = *selection};
        handler.on_change(handler.user, &event);
        result.change_count = 1;
    }
    return result;
}

cui_picker_event_result cui_base_color_picker_events(cui_context *ui, cui_id id,
                                                     cui_color_state *state, cui_color *color,
                                                     cui_color_picker_options options,
                                                     cui_color_handler handler) {
    cui_picker_event_result result = {0};
    if (!ui || !id || !state || !color)
        return result;
    cui_color before = *color;
    uint64_t frame = !options.disabled && !options.readonly && cui_frame_paint_enabled(ui)
                         ? cui_frame_sequence(ui)
                         : 0;
    result.changed = cui_base_color_picker(ui, id, state, color, options);
    if (frame && handler.on_change &&
        (before.r != color->r || before.g != color->g || before.b != color->b ||
         before.a != color->a)) {
        cui_color_change event = {.event = {.kind = CUI_COMPONENT_EVENT_CHANGE,
                                            .target = id,
                                            .source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN,
                                            .flags = CUI_COMPONENT_EVENT_HAS_FRAME,
                                            .frame_sequence = frame},
                                  .previous = before,
                                  .value = *color};
        handler.on_change(handler.user, &event);
        result.change_count = 1;
    }
    return result;
}

cui_picker_event_result cui_base_time_field_events(cui_context *ui, cui_id id, cui_str name,
                                                   cui_time_state *state, cui_time *time,
                                                   cui_time_options options,
                                                   cui_time_handler handler) {
    cui_picker_event_result result = {0};
    if (!ui || !id || !state || !time)
        return result;
    cui_time before = *time;
    uint64_t frame = !options.disabled && !options.readonly && cui_frame_paint_enabled(ui)
                         ? cui_frame_sequence(ui)
                         : 0;
    result.changed = cui_base_time_field(ui, id, name, state, time, options);
    if (frame && handler.on_change &&
        (before.hour != time->hour || before.minute != time->minute ||
         before.second != time->second)) {
        cui_time_change event = {.event = {.kind = CUI_COMPONENT_EVENT_CHANGE,
                                           .target = id,
                                           .source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN,
                                           .flags = CUI_COMPONENT_EVENT_HAS_FRAME,
                                           .frame_sequence = frame},
                                 .previous = before,
                                 .value = *time};
        handler.on_change(handler.user, &event);
        result.change_count = 1;
    }
    return result;
}
