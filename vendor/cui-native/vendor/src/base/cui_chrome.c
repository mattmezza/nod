/* SPDX-License-Identifier: MIT */
#include "cui/cui_chrome.h"
cui_align cui_status_bar_center_alignment(bool has_left, bool has_right) {
    return has_left ? has_right ? CUI_ALIGN_CENTER : CUI_ALIGN_END : CUI_ALIGN_START;
}

#include <math.h>
cui_id cui_title_bar_control_id(cui_id id, unsigned part) {
    return id ^ (UINT64_C(0x817402dbae953cf1) * (part + 1));
}
static bool cui_chrome_window_perform(cui_title_bar_options o, cui_window_info info,
                                      cui_window_action action, float x, float y) {
    return (info.actions & (1u << action)) && o.host && o.host->perform &&
           o.host->perform(o.host->user, action, x, y);
}
bool cui_base_title_bar(cui_context *ui, cui_id id, cui_title_bar_state *state,
                        cui_window_info info, cui_title_bar_options o) {
    if (!ui || !id || !state)
        return false;
    cui_interact(ui, id, CUI_ROLE_GROUP, o.name, false, false, 0);
    cui_set_tab_stop(ui, id, false);
    cui_set_semantic_details(ui, id, (cui_semantic_details){.pointer_activatable = true});
    bool acted = false;
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count; ++i) {
        cui_event event = events[i];
        bool target = cui_event_target(ui, i) == id && !cui_event_consumed(ui, i);
        if (event.type == CUI_EVENT_CANCEL || (event.type == CUI_EVENT_POINTER_DOWN && !target)) {
            state->move_armed = state->moved = false;
            state->click_pending = false;
        }
        if (target && event.type == CUI_EVENT_POINTER_DOWN) {
            if (event.pointer_button == CUI_POINTER_PRIMARY) {
                state->move_armed = true;
                state->moved = false;
            } else if (event.pointer_button == CUI_POINTER_SECONDARY &&
                       info.decoration == CUI_WINDOW_CLIENT_DECORATED) {
                acted |= cui_chrome_window_perform(o, info, CUI_WINDOW_MENU, event.x, event.y);
                cui_consume_event(ui, i);
            }
        }
        if (target && event.type == CUI_EVENT_POINTER_MOVE && state->move_armed) {
            state->move_armed = false;
            state->moved = true;
            state->click_pending = false;
            acted |= cui_chrome_window_perform(o, info, CUI_WINDOW_MOVE, event.x, event.y);
            cui_consume_event(ui, i);
        }
        if (event.type == CUI_EVENT_POINTER_UP && event.pointer_button == CUI_POINTER_PRIMARY) {
            if (target && !state->moved && cui_event_activation(ui, i) == id) {
                double now = cui_frame_time(ui);
                float duration = isfinite(o.double_click_seconds) && o.double_click_seconds > 0
                                     ? o.double_click_seconds
                                     : .4f;
                float distance = isfinite(o.double_click_distance) && o.double_click_distance > 0
                                     ? o.double_click_distance
                                     : 4;
                bool close = fabsf(event.x - state->click_x) <= distance &&
                             fabsf(event.y - state->click_y) <= distance;
                if (state->click_pending && now - state->click_time <= duration && close) {
                    acted |= cui_chrome_window_perform(o, info, CUI_WINDOW_TITLEBAR_DOUBLE_CLICK,
                                                       event.x, event.y);
                    state->click_pending = false;
                } else {
                    state->click_pending = true;
                    state->click_time = now;
                    state->click_x = event.x;
                    state->click_y = event.y;
                }
            }
            state->move_armed = state->moved = false;
        }
    }
    return acted;
}
