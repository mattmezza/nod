/* SPDX-License-Identifier: MIT */
#include "cui/cui_stepper_navigation.h"
static bool cui_stepper_navigation_valid(cui_context *ui, cui_id id, const cui_step *steps,
                                         size_t count, size_t selected) {
    if (!ui || !id || (!steps && count) || count > 100 ||
        (count ? selected >= count : selected != 0))
        return false;
    for (size_t i = 0; i < count; ++i)
        if ((steps[i].title.length && !steps[i].title.data) ||
            (steps[i].description.length && !steps[i].description.data) ||
            steps[i].status < CUI_STEP_PENDING || steps[i].status > CUI_STEP_ERROR)
            return false;
    return true;
}
cui_stepper_navigation_result cui_base_stepper_navigation(cui_context *ui, cui_id id,
                                                          const cui_step *steps, size_t count,
                                                          size_t selected, bool disabled,
                                                          cui_stepper_navigation_handler handler) {
    cui_stepper_navigation_result result = {0};
    if (!cui_stepper_navigation_valid(ui, id, steps, count, selected))
        return result;
    const cui_stepper_navigation_handler observer = handler;
    const uint64_t frame = cui_frame_sequence(ui);
    result.valid = true;
    cui_interact(ui, id, CUI_ROLE_GROUP, cui_string("Steps"), disabled, false, (float)selected);
    cui_set_tab_stop(ui, id, false);
    for (size_t i = 0; i < count; ++i) {
        cui_id part = cui_numeric_part_id(id, 1 + (unsigned)i);
        cui_interact(ui, part, CUI_ROLE_BUTTON, steps[i].title, disabled || steps[i].disabled,
                     false, (float)i);
        cui_set_semantic_details(ui, part,
                                 (cui_semantic_details){.selected = i == selected,
                                                        .description = steps[i].description,
                                                        .parent = id});
    }
    if (!frame || disabled)
        return result;
    size_t event_count = 0;
    (void)cui_frame_events(ui, &event_count);
    for (size_t e = 0; e < event_count; ++e) {
        cui_activation_snapshot evidence;
        if (frame != cui_frame_sequence(ui) || !cui_event_activation_snapshot(ui, e, &evidence))
            continue;
        for (size_t i = 0; i < count; ++i) {
            if (steps[i].disabled || evidence.target != cui_numeric_part_id(id, 1 + (unsigned)i))
                continue;
            cui_stepper_navigation_request request = {0};
            if (!cui_component_event_from_activation(&request.event, CUI_COMPONENT_EVENT_REQUEST,
                                                     &evidence))
                continue;
            request.event.target = id;
            request.selected = selected;
            request.requested = i;
            request.count = count;
            result.request_count++;
            if (observer.request)
                observer.request(observer.user, &request);
            break;
        }
    }
    return result;
}
