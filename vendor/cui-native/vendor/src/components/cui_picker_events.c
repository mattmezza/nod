/* SPDX-License-Identifier: MIT */
#include "cui/cui_picker_events.h"
cui_calendar_event_result cui_calendar_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                              cui_calendar_state *state,
                                              cui_date_selection *selection,
                                              cui_calendar_options options,
                                              cui_date_selection_handler handler) {
    cui_calendar_event_result result = {0};
    if (!ui || !id || !theme || !state || !selection)
        return result;
    cui_date_selection before = *selection;
    uint64_t frame = !options.disabled && cui_frame_paint_enabled(ui) ? cui_frame_sequence(ui) : 0;
    result.control = cui_calendar(ui, id, theme, state, selection, options);
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

cui_picker_event_result
cui_date_picker_events(cui_context *ui, cui_id trigger, const cui_theme *theme, cui_str name,
                       cui_date_picker_state *state, cui_date_selection *selection,
                       cui_date_picker_options options, cui_date_selection_handler handler) {
    cui_picker_event_result result = {0};
    if (!ui || !trigger || !theme || !state || !selection)
        return result;
    cui_date_selection before = *selection;
    uint64_t frame = !options.disabled && !options.calendar.disabled && cui_frame_paint_enabled(ui)
                         ? cui_frame_sequence(ui)
                         : 0;
    result.changed = cui_date_picker(ui, trigger, theme, name, state, selection, options);
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

cui_picker_event_result cui_color_picker_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                                cui_color_state *state, cui_color *color,
                                                cui_color_picker_options options,
                                                cui_color_handler handler) {
    cui_picker_event_result result = {0};
    if (!ui || !id || !state || !color || !theme)
        return result;
    cui_color before = *color;
    uint64_t frame = !options.disabled && !options.readonly && cui_frame_paint_enabled(ui)
                         ? cui_frame_sequence(ui)
                         : 0;
    result.changed = cui_color_picker(ui, id, theme, state, color, options);
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

cui_picker_event_result cui_time_field_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                              cui_str name, cui_time_state *state, cui_time *time,
                                              cui_time_options options, cui_time_handler handler) {
    cui_picker_event_result result = {0};
    if (!ui || !id || !state || !time || !theme)
        return result;
    cui_time before = *time;
    uint64_t frame = !options.disabled && !options.readonly && cui_frame_paint_enabled(ui)
                         ? cui_frame_sequence(ui)
                         : 0;
    result.changed = cui_time_field(ui, id, theme, name, state, time, options);
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
