/* SPDX-License-Identifier: MIT */
#include "cui/cui_overlay.h"
static bool cui_overlay_contains(cui_rect r, float x, float y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}
void cui_overlay_init(cui_overlay *overlay, cui_id id, bool modal) {
    if (overlay)
        *overlay = (cui_overlay){
            .id = id, .modal = modal, .dismiss_on_escape = true, .dismiss_on_outside = true};
}
void cui_overlay_open(cui_context *ui, cui_overlay *overlay, cui_id trigger) {
    if (!ui || !overlay || !overlay->id || overlay->open)
        return;
    overlay->trigger = trigger ? trigger : cui_focused(ui);
    overlay->open = true;
    overlay->has_bounds = false;
    cui_invalidate(ui);
}
void cui_overlay_close(cui_context *ui, cui_overlay *overlay) {
    if (!ui || !overlay || !overlay->open)
        return;
    overlay->open = false;
    if (!cui_focus_restore(ui, overlay->trigger))
        cui_focus(ui, 0);
    cui_invalidate(ui);
}
void cui_overlay_set_bounds(cui_overlay *overlay, cui_rect bounds) {
    if (overlay) {
        overlay->bounds = bounds;
        overlay->has_bounds = true;
    }
}
bool cui_overlay_update(cui_context *ui, cui_overlay *overlay) {
    if (!ui || !overlay || !overlay->open)
        return false;
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count; ++i) {
        if (cui_event_consumed(ui, i))
            continue;
        cui_event event = events[i];
        cui_id target = cui_event_target(ui, i);
        bool escape = overlay->dismiss_on_escape && event.type == CUI_EVENT_KEY_DOWN &&
                      event.key == CUI_KEY_ESCAPE && !event.repeat && target == overlay->id;
        bool outside = overlay->has_bounds && overlay->dismiss_on_outside &&
                       event.type == CUI_EVENT_POINTER_DOWN &&
                       (!overlay->modal || target == overlay->id) &&
                       !cui_overlay_contains(overlay->bounds, event.x, event.y);
        if (escape || outside) {
            cui_overlay_close(ui, overlay);
            return true;
        }
    }
    return false;
}
bool cui_overlay_begin(cui_context *ui, const cui_overlay *overlay) {
    return ui && overlay && overlay->open && cui_begin_focus_scope(ui, overlay->id, overlay->modal);
}
void cui_overlay_end(cui_context *ui) {
    cui_end_focus_scope(ui);
}
