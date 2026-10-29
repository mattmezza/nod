/* SPDX-License-Identifier: MIT */
#include "cui/cui_tabs_events.h"
cui_tabs_event_result cui_base_tabs_events(cui_context *ui, cui_id target,
                                           const cui_choice_item *items, size_t count,
                                           cui_id *selected, cui_tabs_options options,
                                           cui_tabs_handler handler) {
    cui_tabs_event_result result = {0};
    if (!ui || !target || !selected)
        return result;
    cui_id previous = *selected;
    bool observe = !options.disabled && cui_frame_paint_enabled(ui);
    uint64_t frame = observe ? cui_frame_sequence(ui) : 0;
    result.changed = cui_base_tabs(ui, items, count, selected, options);
    if (frame && handler.on_change && previous != *selected) {
        cui_tabs_selection_change event = {.event = {.kind = CUI_COMPONENT_EVENT_CHANGE,
                                                     .target = target,
                                                     .source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN,
                                                     .flags = CUI_COMPONENT_EVENT_HAS_FRAME,
                                                     .frame_sequence = frame},
                                           .previous = previous,
                                           .value = *selected};
        handler.on_change(handler.user, &event);
        result.change_count = 1;
    }
    return result;
}
