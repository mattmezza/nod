/* SPDX-License-Identifier: MIT */
#include "cui/cui_scroll.h"
#include "cui/cui_scroll_events.h"
#include <math.h>
static bool cui_scroll_persistent_lock(cui_scroll_options o) {
#if defined(__wasm__)
    (void)o;
    return false;
#else
    return o.gesture_lock;
#endif
}
static float cui_scroll_positive(float value, float fallback) {
    return isfinite(value) && value > 0 ? value : fallback;
}
static float cui_scroll_limit(float value, float maximum) {
    return isfinite(value) ? fminf(fmaxf(value, 0), maximum) : 0;
}
cui_scroll_options cui_scroll_resolve(const cui_theme *theme, cui_scroll_options o) {
    if (!o.scrollbar_style && theme)
        o.scrollbar_style = &theme->scrollbar;
    if (o.scrollbar_style) {
        if (isfinite(o.scrollbar_style->thickness) && o.scrollbar_style->thickness > 0)
            o.bar_thickness = o.scrollbar_style->thickness;
        if (o.scrollbar_style->visibility == CUI_SCROLLBAR_HIDDEN)
            o.hide_scrollbars = true;
    }
    return o;
}
typedef struct {
    double last_activity;
    float value;
    bool initialized, hovered, held;
} cui_scrollbar_timer;
float cui_scrollbar_opacity(cui_context *ui, cui_id owner, cui_id bar,
                            const cui_scrollbar_style *style, float value, bool dragging) {
    if (!ui || !owner || !bar)
        return 0;
    if (!style || style->visibility != CUI_SCROLLBAR_AUTO_HIDE)
        return style && style->visibility == CUI_SCROLLBAR_HIDDEN ? 0 : 1;
    cui_scrollbar_timer *timer =
        cui_resource_get(ui, bar, UINT64_C(0x7421c6e9a3540b87), sizeof(*timer), NULL, NULL);
    if (!timer)
        return 0;
    double now = cui_frame_time(ui);
    value = isfinite(value) ? value : 0;
    bool hovered = cui_hovered(ui, owner);
    bool held = dragging || cui_pressed(ui, bar) || cui_hovered(ui, bar) ||
                cui_focused(ui) == owner || cui_focused(ui) == bar || cui_focus_within(ui, owner);
    bool activity = !timer->initialized || timer->value != value || (hovered && !timer->hovered) ||
                    (timer->held && !held);
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count; i++)
        if ((hovered && events[i].type == CUI_EVENT_POINTER_MOVE) ||
            ((events[i].type == CUI_EVENT_SCROLL || events[i].type == CUI_EVENT_KEY_DOWN) &&
             (cui_event_target(ui, i) == owner || cui_event_target(ui, i) == bar)))
            activity = true;
    if (held || activity)
        timer->last_activity = now;
    timer->value = value;
    timer->hovered = hovered;
    timer->held = held;
    timer->initialized = true;
    if (held)
        return 1;
    double idle = fmin(60, cui_scroll_positive(style->idle_seconds, 1));
    double fade = style->no_fade || style->reduced_motion || cui_reduced_motion(ui)
                      ? 0
                      : fmin(5, cui_scroll_positive(style->fade_seconds, .15f));
    double elapsed = fmax(0, now - timer->last_activity);
    if (elapsed >= idle + fade)
        return 0;
    cui_invalidate(ui);
    return elapsed <= idle || fade == 0 ? 1 : (float)(1 - (elapsed - idle) / fade);
}
cui_scroll_geometry cui_scroll_layout(cui_scroll_options o) {
    o = cui_scroll_resolve(NULL, o);
    float width = cui_scroll_positive(o.width, 320), height = cui_scroll_positive(o.height, 240);
    float bar = fminf(cui_scroll_positive(o.bar_thickness, 10), fminf(width, height) * .5f);
    cui_size viewport = {width - (o.vertical && !o.hide_scrollbars ? bar : 0),
                         height - (o.horizontal && !o.hide_scrollbars ? bar : 0)};
    return (cui_scroll_geometry){
        viewport,
        o.horizontal
            ? fmaxf(0, cui_scroll_positive(o.content_width, viewport.width) - viewport.width)
            : 0,
        o.vertical
            ? fmaxf(0, cui_scroll_positive(o.content_height, viewport.height) - viewport.height)
            : 0};
}
cui_id cui_scrollbar_id(cui_id id, bool vertical) {
    return id ^ (vertical ? UINT64_C(0xc83f584c2b646a11) : UINT64_C(0x9c5a7247d91fa603));
}
cui_rect cui_scrollbar_thumb(cui_rect track, float scroll, float viewport, float content,
                             bool vertical) {
    return cui_scrollbar_thumb_ex(track, scroll, viewport, content, vertical, 12);
}
cui_rect cui_scrollbar_thumb_ex(cui_rect track, float scroll, float viewport, float content,
                                bool vertical, float minimum) {
    float length = vertical ? track.h : track.w;
    if (!isfinite(length) || length <= 0)
        return (cui_rect){0};
    viewport = cui_scroll_positive(viewport, length);
    content = cui_scroll_positive(content, viewport);
    float maximum = fmaxf(0, content - viewport);
    minimum = cui_scroll_positive(minimum, 12);
    float thumb =
        fminf(length, fmaxf(fminf(minimum, length), length * fminf(1, viewport / content)));
    float position =
        maximum > 0 ? cui_scroll_limit(scroll, maximum) / maximum * (length - thumb) : 0;
    if (vertical) {
        track.y += position;
        track.h = thumb;
    } else {
        track.x += position;
        track.w = thumb;
    }
    return track;
}
bool cui_base_scrollbar(cui_context *ui, cui_id root, cui_scroll_state *s,
                        cui_scroll_geometry geometry, cui_scroll_options o, bool vertical) {
    if (!ui || !root || !s)
        return false;
    o = cui_scroll_resolve(NULL, o);
    cui_id id = cui_scrollbar_id(root, vertical);
    float *value = vertical ? &s->y : &s->x;
    bool *dragging = vertical ? &s->dragging_y : &s->dragging_x;
    float *grab = vertical ? &s->grab_y : &s->grab_x;
    float maximum = vertical ? geometry.maximum_y : geometry.maximum_x;
    float viewport = vertical ? geometry.viewport.height : geometry.viewport.width;
    bool disabled = o.disabled || maximum <= 0;
    float before = *value;
    if (o.hide_scrollbars ||
        cui_scrollbar_opacity(ui, root, id, o.scrollbar_style, *value, *dragging) <= 0) {
        *dragging = false;
        return false;
    }
    cui_interact(ui, id, CUI_ROLE_SCROLLBAR,
                 vertical ? cui_string("Vertical scroll") : cui_string("Horizontal scroll"),
                 disabled, false, *value);
    cui_rect bounds = cui_bounds(ui, id);
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    if (disabled)
        *dragging = false;
    for (size_t i = 0; i < count; ++i) {
        cui_event e = events[i];
        if (e.type == CUI_EVENT_CANCEL)
            *dragging = false;
        if (disabled || cui_event_target(ui, i) != id)
            continue;
        if ((e.type == CUI_EVENT_POINTER_DOWN || e.type == CUI_EVENT_POINTER_UP) &&
            e.pointer_button != CUI_POINTER_PRIMARY)
            continue;
        cui_rect thumb =
            cui_scrollbar_thumb_ex(bounds, *value, viewport, viewport + maximum, vertical,
                                   o.scrollbar_style ? o.scrollbar_style->min_thumb : 0);
        float pointer = vertical ? e.y : e.x;
        float start = vertical ? thumb.y : thumb.x;
        float thumb_length = vertical ? thumb.h : thumb.w;
        float track_start = vertical ? bounds.y : bounds.x;
        float track_length = vertical ? bounds.h : bounds.w;
        if (e.type == CUI_EVENT_POINTER_DOWN) {
            *dragging = pointer >= start && pointer < start + thumb_length;
            *grab = pointer - start;
            if (!*dragging)
                *value += pointer < start ? -viewport : viewport;
        } else if (*dragging &&
                   (e.type == CUI_EVENT_POINTER_MOVE || e.type == CUI_EVENT_POINTER_UP) &&
                   track_length > thumb_length) {
            *value = (pointer - track_start - *grab) / (track_length - thumb_length) * maximum;
        } else if (e.type == CUI_EVENT_KEY_DOWN) {
            float step = cui_scroll_positive(o.wheel_step, 48);
            if (e.key == CUI_KEY_HOME)
                *value = 0;
            else if (e.key == CUI_KEY_END)
                *value = maximum;
            else if (e.key == CUI_KEY_PAGEUP)
                *value -= viewport;
            else if (e.key == CUI_KEY_PAGEDOWN)
                *value += viewport;
            else if (e.key == CUI_KEY_LEFT || e.key == CUI_KEY_UP)
                *value -= step;
            else if (e.key == CUI_KEY_RIGHT || e.key == CUI_KEY_DOWN)
                *value += step;
        }
        if (e.type == CUI_EVENT_POINTER_UP)
            *dragging = false;
        *value = cui_scroll_limit(*value, maximum);
    }
    cui_update_semantic(ui, id, false, *value);
    cui_set_semantic_details(ui, id,
                             (cui_semantic_details){.controls = root,
                                                    .parent = root,
                                                    .orientation = vertical
                                                                       ? CUI_ORIENTATION_VERTICAL
                                                                       : CUI_ORIENTATION_HORIZONTAL,
                                                    .minimum = 0,
                                                    .maximum = maximum});
    return before != *value;
}
static void cui_scroll_reduce_wheel(cui_context *ui, cui_id id, cui_scroll_state *s,
                                    cui_scroll_options o, cui_scroll_geometry g) {
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count && !o.disabled; ++i)
        if (events[i].type == CUI_EVENT_SCROLL && !cui_event_consumed(ui, i) &&
            cui_event_target(ui, i) == id) {
            float previous_x = s->x, previous_y = s->y;
            float step = events[i].scroll_precise ? 1 : cui_scroll_positive(o.wheel_step, 48);
            if (o.horizontal && isfinite(events[i].dx))
                s->x = cui_scroll_limit(s->x - events[i].dx * step, g.maximum_x);
            if (o.vertical && isfinite(events[i].dy))
                s->y = cui_scroll_limit(s->y - events[i].dy * step, g.maximum_y);
            if (s->x != previous_x || s->y != previous_y ||
                (o.horizontal && o.trap_horizontal && fabsf(events[i].dx) > fabsf(events[i].dy)))
                cui_consume_event(ui, i);
            else
                cui_defer_scroll_event(ui, i, id);
        }
}
bool cui_base_scrollable_finish(cui_context *ui, cui_id id, cui_scroll_state *s,
                                cui_scroll_options o) {
    if (!ui || !id || !s)
        return false;
    o = cui_scroll_resolve(NULL, o);
    cui_scroll_geometry g = cui_scroll_layout(o);
    float old_x = s->x, old_y = s->y;
    cui_scroll_reduce_wheel(ui, id, s, o, g);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.scrollable = !o.disabled,
                               .scroll_axis_lock = o.axis_lock,
                               .scroll_gesture_lock = cui_scroll_persistent_lock(o),
                               .scroll_trap_horizontal = o.horizontal && o.trap_horizontal,
                               .orientation = o.horizontal == o.vertical
                                                  ? CUI_ORIENTATION_UNSPECIFIED
                                              : o.horizontal ? CUI_ORIENTATION_HORIZONTAL
                                                             : CUI_ORIENTATION_VERTICAL,
                               .scroll_x = s->x,
                               .scroll_y = s->y,
                               .scroll_max_x = g.maximum_x,
                               .scroll_max_y = g.maximum_y});
    return s->x != old_x || s->y != old_y;
}
bool cui_base_scrollable(cui_context *ui, cui_id id, cui_scroll_state *s, cui_scroll_options o) {
    if (!ui || !id || !s)
        return false;
    o = cui_scroll_resolve(NULL, o);
    cui_scroll_geometry g = cui_scroll_layout(o);
    float old_x = s->x, old_y = s->y;
    if (o.hide_scrollbars)
        s->dragging_x = s->dragging_y = false;
    s->x = cui_scroll_limit(s->x, g.maximum_x);
    s->y = cui_scroll_limit(s->y, g.maximum_y);
    cui_scroll_reduce_wheel(ui, id, s, o, g);
    cui_interact(ui, id, CUI_ROLE_GROUP, cui_string("Scrollable content"), o.disabled, false, 0);
    if (o.horizontal && !o.hide_scrollbars)
        cui_base_scrollbar(ui, id, s, g, o, false);
    if (o.vertical && !o.hide_scrollbars)
        cui_base_scrollbar(ui, id, s, g, o, true);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.scrollable = !o.disabled,
                               .scroll_axis_lock = o.axis_lock,
                               .scroll_gesture_lock = cui_scroll_persistent_lock(o),
                               .scroll_trap_horizontal = o.horizontal && o.trap_horizontal,
                               .orientation = o.horizontal == o.vertical
                                                  ? CUI_ORIENTATION_UNSPECIFIED
                                              : o.horizontal ? CUI_ORIENTATION_HORIZONTAL
                                                             : CUI_ORIENTATION_VERTICAL,
                               .scroll_x = s->x,
                               .scroll_y = s->y,
                               .scroll_max_x = g.maximum_x,
                               .scroll_max_y = g.maximum_y});
    return s->x != old_x || s->y != old_y;
}

bool cui_base_scrollbar_overlay(cui_context *ui, cui_id id, float *x, float *y, cui_rect viewport,
                                cui_size content, cui_scroll_options o) {
    if (!ui || !id || !x || !y || x == y || !isfinite(*x) || !isfinite(*y) ||
        !isfinite(viewport.x) || !isfinite(viewport.y) || !isfinite(viewport.w) ||
        !isfinite(viewport.h) || viewport.w <= 0 || viewport.h <= 0 || !isfinite(content.width) ||
        !isfinite(content.height) || content.width < 0 || content.height < 0)
        return false;
    o = cui_scroll_resolve(NULL, o);
    if (o.hide_scrollbars || !o.scrollbar_style ||
        (o.scrollbar_style->visibility != CUI_SCROLLBAR_VISIBLE &&
         o.scrollbar_style->visibility != CUI_SCROLLBAR_AUTO_HIDE)) {
        cui_scroll_state *previous = cui_resource_find(ui, id, UINT64_C(0x30914be927c650a8));
        if (previous)
            previous->dragging_x = previous->dragging_y = false;
        return false;
    }
    cui_scroll_state *state =
        cui_resource_get(ui, id, UINT64_C(0x30914be927c650a8), sizeof(*state), NULL, NULL);
    if (!state)
        return false;
    state->x = isfinite(*x) ? fmaxf(0, *x) : 0;
    state->y = isfinite(*y) ? fmaxf(0, *y) : 0;
    float bar =
        fminf(cui_scroll_positive(o.bar_thickness, 10), fminf(viewport.w, viewport.h) * .5f);
    cui_scroll_geometry g = {.viewport = {viewport.w, viewport.h},
                             .maximum_x = o.horizontal ? fmaxf(0, content.width - viewport.w) : 0,
                             .maximum_y = o.vertical ? fmaxf(0, content.height - viewport.h) : 0};
    float old_x = state->x, old_y = state->y;
    state->x = cui_scroll_limit(state->x, g.maximum_x);
    state->y = cui_scroll_limit(state->y, g.maximum_y);
    o.width = viewport.w + bar;
    o.height = viewport.h + bar;
    if (o.horizontal && g.maximum_x > 0)
        cui_base_scrollbar(ui, id, state, g, o, false);
    if (o.vertical && g.maximum_y > 0)
        cui_base_scrollbar(ui, id, state, g, o, true);
    *x = state->x;
    *y = state->y;
    size_t semantic_count;
    const cui_semantic *semantics = cui_semantics(ui, &semantic_count);
    for (size_t i = 0; i < semantic_count; i++)
        if (semantics[i].id == id) {
            cui_semantic_details details = semantics[i].details;
            details.scroll_x = state->x;
            details.scroll_y = state->y;
            details.scroll_max_x = g.maximum_x;
            details.scroll_max_y = g.maximum_y;
            cui_set_semantic_details(ui, id, details);
            break;
        }
    bool changed = old_x != state->x || old_y != state->y;
    if (changed)
        cui_invalidate(ui);
    return changed;
}

bool cui_base_scrollable_events(cui_context *ui, cui_id id, cui_scroll_state *state,
                                cui_scroll_options options, cui_scroll_event_scope *scope) {
    if (!ui || !id || !state || !scope || scope->private_active || !cui_frame_is_submitting(ui))
        return false;
    scope->private_context = ui;
    scope->private_state = state;
    scope->private_owner = id;
    scope->private_frame = cui_frame_sequence(ui);
    scope->private_entry = (cui_scroll_offset){state->x, state->y};
    scope->private_active = true;
    scope->private_styled = false;
    return cui_base_scrollable(ui, id, state, options);
}

cui_scroll_event_result cui_base_scrollable_finish_events(cui_context *ui, cui_id id,
                                                          cui_scroll_state *state,
                                                          cui_scroll_options options,
                                                          cui_scroll_event_scope *scope,
                                                          cui_scroll_handler handler) {
    cui_scroll_event_result result;
    memset(&result, 0, sizeof(result));
    if (!ui || !id || !state || !scope || !scope->private_active || scope->private_styled ||
        scope->private_context != ui || scope->private_owner != id ||
        scope->private_state != state || !cui_frame_is_submitting(ui) ||
        scope->private_frame != cui_frame_sequence(ui))
        return result;
    (void)cui_base_scrollable_finish(ui, id, state, options);
    cui_scroll_change change;
    memset(&change, 0, sizeof(change));
    change.has_previous = isfinite(scope->private_entry.x) && isfinite(scope->private_entry.y);
    if (change.has_previous)
        change.previous = scope->private_entry;
    change.value = (cui_scroll_offset){state->x, state->y};
    uint64_t frame = scope->private_frame;
    scope->private_active = false;
    if (!isfinite(change.value.x) || !isfinite(change.value.y))
        return result;
    result.valid = true;
    result.changed = !change.has_previous || change.previous.x != change.value.x ||
                     change.previous.y != change.value.y;
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
