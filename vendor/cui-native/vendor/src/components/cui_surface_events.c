/* SPDX-License-Identifier: MIT */
#include "cui/cui_surface_events.h"
cui_surface_event_result cui_surface_begin_events(cui_context *ui, const cui_theme *theme,
                                                  cui_surface_event_state *s, cui_str title,
                                                  cui_str description,
                                                  cui_surface_options options) {
    cui_surface_event_result result = {0};
    if (!ui || !theme || !cui_surface_events_options(s, options, &options))
        return result;
    s->active = true;
    s->call_close_count = 0;
    result.control_result = cui_surface_begin(ui, theme, &s->control, title, description, options);
    result.close_count = s->call_close_count;
    result.valid = true;
    s->active = false;
    return result;
}
cui_surface_event_result cui_alert_dialog_events(cui_context *ui, const cui_theme *theme,
                                                 cui_surface_event_state *s, cui_str title,
                                                 cui_str description, cui_str confirm,
                                                 cui_str cancel, cui_surface_options options) {
    cui_surface_event_result result = {0};
    if (!ui || !theme || !s || s->control.kind != CUI_SURFACE_ALERT ||
        !cui_surface_events_options(s, options, &options))
        return result;
    s->active = true;
    s->call_close_count = 0;
    cui_alert_dialog(ui, theme, &s->control, title, description, confirm, cancel, options);
    result.close_count = s->call_close_count;
    result.valid = true;
    s->active = false;
    return result;
}
