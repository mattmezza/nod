/* SPDX-License-Identifier: MIT */
#include "cui/cui_surface_events.h"
#include <string.h>
static void cui_surface_events_closed(void *user, cui_surface_result result) {
    cui_surface_event_state *s = user;
    if (!s || !s->attached || s->notifying || !s->handler.closed)
        return;
    cui_surface_lifecycle_event payload;
    memset(&payload, 0, sizeof(payload));
    if (!cui_component_event_init(&payload.event, CUI_COMPONENT_EVENT_LIFECYCLE,
                                  s->control.overlay.id, CUI_COMPONENT_EVENT_SOURCE_UNKNOWN))
        return;
    payload.result = result;
    cui_surface_event_handler handler = s->handler;
    s->notifying = true;
    if (s->active)
        s->call_close_count++;
    handler.closed(handler.user, &payload);
    s->notifying = false;
}
static bool cui_surface_events_ready(const cui_surface_event_state *s) {
    return s && s->attached && !s->active && !s->notifying &&
           s->control.on_close == cui_surface_events_closed && s->control.close_user == s;
}
void cui_surface_events_init(cui_surface_event_state *s, cui_id id, cui_surface_kind kind) {
    if (!s)
        return;
    memset(s, 0, sizeof(*s));
    cui_surface_init(&s->control, id, kind);
    if (!id)
        return;
    s->attached = true;
    s->control.on_close = cui_surface_events_closed;
    s->control.close_user = s;
}
bool cui_surface_events_set_handler(cui_surface_event_state *s, cui_surface_event_handler handler) {
    if (!cui_surface_events_ready(s))
        return false;
    if (!handler.closed)
        handler.user = NULL;
    s->handler = handler;
    return true;
}
bool cui_surface_events_detach(cui_surface_event_state *s) {
    if (!cui_surface_events_ready(s) || s->control.overlay.open || s->control.sheet_closing)
        return false;
    s->control.on_close = NULL;
    s->control.close_user = NULL;
    s->handler = (cui_surface_event_handler){0};
    s->attached = false;
    return true;
}
bool cui_surface_events_options(cui_surface_event_state *s, cui_surface_options input,
                                cui_surface_options *output) {
    if (!output || !cui_surface_events_ready(s) || input.on_close || input.close_user)
        return false;
    input.on_close = cui_surface_events_closed;
    input.close_user = s;
    *output = input;
    return true;
}
cui_surface_event_result cui_base_surface_update_events(cui_context *ui, cui_surface_event_state *s,
                                                        cui_surface_options options) {
    cui_surface_event_result result = {0};
    if (!ui || !cui_surface_events_options(s, options, &options))
        return result;
    s->active = true;
    s->call_close_count = 0;
    result.control_result = cui_base_surface_update(ui, &s->control, options);
    result.close_count = s->call_close_count;
    result.valid = true;
    s->active = false;
    return result;
}
cui_surface_event_result cui_base_surface_begin_events(cui_context *ui, cui_surface_event_state *s,
                                                       cui_str title, cui_str description,
                                                       cui_surface_options options) {
    cui_surface_event_result result = {0};
    if (!ui || !cui_surface_events_options(s, options, &options))
        return result;
    s->active = true;
    s->call_close_count = 0;
    result.control_result = cui_base_surface_begin(ui, &s->control, title, description, options);
    result.close_count = s->call_close_count;
    result.valid = true;
    s->active = false;
    return result;
}
