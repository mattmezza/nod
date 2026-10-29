/* SPDX-License-Identifier: MIT */
#include "cui/cui_virtual_scroll.h"
#include "cui/cui_virtual_events.h"
#include <float.h>
#include <math.h>
bool cui_virtual_widget_scroll_options(const cui_virtual_list *v, cui_virtual_widget_options o,
                                       cui_scroll_options *output) {
    if (!v || !output || !isfinite(o.scroll.width) || o.scroll.width < 0 ||
        !isfinite(o.scroll.height) || o.scroll.height < 0 || !isfinite(o.scroll.content_width) ||
        o.scroll.content_width < 0 || !isfinite(o.scroll.content_height) ||
        o.scroll.content_height < 0 || !isfinite(o.scroll.bar_thickness) ||
        o.scroll.bar_thickness < 0 || !isfinite(o.scroll.wheel_step) || o.scroll.wheel_step < 0 ||
        !isfinite(o.available.width) || o.available.width < 0 || !isfinite(o.available.height) ||
        o.available.height < 0 ||
        (o.sizing != CUI_VIRTUAL_SIZE_AUTO && o.sizing != CUI_VIRTUAL_SIZE_INFER) ||
        o.scroll.gap != 0 || cui_virtual_total(v) > FLT_MAX / 4)
        return false;
    cui_scroll_options resolved = o.scroll;
    resolved.content_row = o.horizontal;
    if (o.horizontal) {
        resolved.horizontal = true;
        resolved.content_width = (float)cui_virtual_total(v);
    } else {
        resolved.vertical = true;
        resolved.content_height = (float)cui_virtual_total(v);
    }
    if (o.sizing == CUI_VIRTUAL_SIZE_INFER) {
        if (resolved.width == 0)
            resolved.width =
                o.available.width > 0 ? o.available.width : fmaxf(1, resolved.content_width);
        if (resolved.height == 0)
            resolved.height =
                o.available.height > 0 ? o.available.height : fmaxf(1, resolved.content_height);
    }
    *output = resolved;
    return true;
}
cui_virtual_widget_result cui_base_virtual_list(cui_context *ui, cui_id id,
                                                const cui_virtual_list *v,
                                                cui_virtual_widget_state *s,
                                                cui_virtual_widget_options o) {
    cui_virtual_widget_result out = {.error = CUI_VIRTUAL_INVALID};
    cui_scroll_options scroll;
    if (!ui || !id || !s || !cui_virtual_widget_scroll_options(v, o, &scroll))
        return out;
    float before_x = s->scroll.x, before_y = s->scroll.y;
    cui_base_scrollable(ui, id, &s->scroll, scroll);
    cui_scroll_geometry g = cui_scroll_layout(scroll);
    float *value = o.horizontal ? &s->scroll.x : &s->scroll.y;
    float maximum = o.horizontal ? g.maximum_x : g.maximum_y;
    float viewport = o.horizontal ? g.viewport.width : g.viewport.height;
    float step = scroll.wheel_step > 0 ? scroll.wheel_step : 48;
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count && !scroll.disabled; ++i) {
        if (events[i].type != CUI_EVENT_KEY_DOWN || cui_event_consumed(ui, i) ||
            cui_event_target(ui, i) != id ||
            (events[i].modifiers & (CUI_MOD_CTRL | CUI_MOD_META | CUI_MOD_ALT)))
            continue;
        cui_key key = events[i].key;
        if (key == CUI_KEY_HOME)
            *value = 0;
        else if (key == CUI_KEY_END)
            *value = maximum;
        else if (key == CUI_KEY_PAGEUP)
            *value -= viewport;
        else if (key == CUI_KEY_PAGEDOWN)
            *value += viewport;
        else if (key == (o.horizontal ? CUI_KEY_LEFT : CUI_KEY_UP))
            *value -= step;
        else if (key == (o.horizontal ? CUI_KEY_RIGHT : CUI_KEY_DOWN))
            *value += step;
        else
            continue;
        *value = fminf(maximum, fmaxf(0, *value));
        cui_consume_event(ui, i);
    }
    if (!scroll.hide_scrollbars) {
        if (scroll.horizontal)
            cui_update_semantic(ui, cui_scrollbar_id(id, false), false, s->scroll.x);
        if (scroll.vertical)
            cui_update_semantic(ui, cui_scrollbar_id(id, true), false, s->scroll.y);
    }
    size_t semantic_count;
    const cui_semantic *sem = cui_semantics(ui, &semantic_count);
    for (size_t i = 0; i < semantic_count; ++i)
        if (sem[i].id == id) {
            cui_semantic_details details = sem[i].details;
            details.focusable = !scroll.disabled;
            details.scroll_x = s->scroll.x;
            details.scroll_y = s->scroll.y;
            cui_set_semantic_details(ui, id, details);
            break;
        }
    s->last_viewport = (cui_size){g.viewport.width, g.viewport.height};
    out.error = CUI_VIRTUAL_OK;
    out.view = cui_virtual_viewport(v, *value, viewport, o.overscan);
    out.scroll_changed = before_x != s->scroll.x || before_y != s->scroll.y;
    return out;
}

cui_virtual_event_result cui_base_virtual_list_events(cui_context *ui, cui_id id,
                                                      const cui_virtual_list *model,
                                                      cui_virtual_widget_state *state,
                                                      cui_virtual_widget_options options,
                                                      cui_scroll_handler handler) {
    cui_virtual_event_result result = {.control_result = {.error = CUI_VIRTUAL_INVALID}};
    if (!ui || !id || !model || !state || !cui_frame_is_submitting(ui))
        return result;
    cui_scroll_offset previous = {state->scroll.x, state->scroll.y};
    uint64_t frame = cui_frame_sequence(ui);
    result.control_result = cui_base_virtual_list(ui, id, model, state, options);
    if (result.control_result.error != CUI_VIRTUAL_OK || !isfinite(state->scroll.x) ||
        !isfinite(state->scroll.y))
        return result;
    cui_scroll_change change = {0};
    change.has_previous = isfinite(previous.x) && isfinite(previous.y);
    if (change.has_previous)
        change.previous = previous;
    change.value = (cui_scroll_offset){state->scroll.x, state->scroll.y};
    result.valid = true;
    result.changed =
        !change.has_previous || previous.x != change.value.x || previous.y != change.value.y;
    if (result.changed && frame && handler.on_change) {
        (void)cui_component_event_init(&change.event, CUI_COMPONENT_EVENT_CHANGE, id,
                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
        change.event.flags |= CUI_COMPONENT_EVENT_HAS_FRAME;
        change.event.frame_sequence = frame;
        result.change_count = 1;
        handler.on_change(handler.user, &change);
    }
    return result;
}
