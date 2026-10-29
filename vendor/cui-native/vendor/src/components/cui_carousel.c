/* SPDX-License-Identifier: MIT */
#include "cui/cui_carousel.h"
#include "cui/cui_scroll.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
static cui_id cui_carousel_part(cui_id root, unsigned part) {
    uint64_t v = root ^ (UINT64_C(0x9e3779b97f4a7c15) * (part + 1));
    v ^= v >> 30;
    v *= UINT64_C(0xbf58476d1ce4e5b9);
    v ^= v >> 27;
    return v ? v : 1;
}
cui_carousel_options cui_carousel_default(void) {
    return (cui_carousel_options){.width = -1, .height = 200, .gap = 16};
}
static bool cui_carousel_string_valid(cui_str s) {
    return s.data || !s.length;
}
static bool cui_carousel_valid(cui_id id, const cui_carousel_item *items, size_t count,
                               cui_carousel_options o) {
    if (!id || count > CUI_CAROUSEL_ITEM_LIMIT || (count && !items) || !isfinite(o.width) ||
        !isfinite(o.height) || !isfinite(o.gap) || o.width < -1 || o.height <= 0 || o.gap < 0 ||
        !isfinite(o.now_seconds) || !cui_carousel_string_valid(o.label))
        return false;
    for (size_t i = 0; i < count; ++i) {
        if (!items[i].id || !items[i].render || !isfinite(items[i].extent) || items[i].extent < 0 ||
            !cui_carousel_string_valid(items[i].label) || items[i].id == id ||
            items[i].id == cui_carousel_part(id, 0))
            return false;
        for (size_t j = 0; j < i; ++j)
            if (items[j].id == items[i].id)
                return false;
    }
    return true;
}
static void cui_carousel_pointer_events(cui_context *ui, cui_id id, cui_carousel_state *s,
                                        cui_carousel_options o) {
    if (!cui_pointer_scope_allowed(ui, id)) {
        cui_carousel_cancel(s);
        return;
    }
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count; ++i) {
        const cui_event *e = &events[i];
        if (cui_event_consumed(ui, i) && e->type != CUI_EVENT_SCROLL)
            continue;
        if (e->type == CUI_EVENT_CANCEL) {
            cui_carousel_cancel(s);
            continue;
        }
        if (e->type == CUI_EVENT_POINTER_DOWN && e->pointer_button == CUI_POINTER_PRIMARY &&
            cui_event_within_scope(ui, i, id) && e->x >= s->frame_bounds.x &&
            e->y >= s->frame_bounds.y && e->x < s->frame_bounds.x + s->frame_bounds.w &&
            e->y < s->frame_bounds.y + s->frame_bounds.h) {
            if (cui_event_target(ui, i) == id)
                s->focus_ring_suppressed = true;
            if (cui_carousel_begin_drag(s, e->x, e->y))
                cui_invalidate(ui);
        } else if (e->type == CUI_EVENT_POINTER_MOVE && s->dragging &&
                   cui_pointer_scope_allowed(ui, id)) {
            if (cui_carousel_update_drag(s, e->x, e->y))
                cui_consume_event(ui, i);
            cui_invalidate(ui);
        } else if (e->type == CUI_EVENT_POINTER_UP && e->pointer_button == CUI_POINTER_PRIMARY &&
                   (s->dragging || s->suppress_click)) {
            bool suppress = s->axis_locked || s->suppress_click;
            cui_carousel_finish_drag(s);
            if (suppress) {
                cui_cancel_event_activation(ui, i);
                cui_consume_event(ui, i);
            }
            cui_invalidate(ui);
        } else if (e->type == CUI_EVENT_SCROLL) {
            bool routed = cui_event_target(ui, i) == id;
            bool raw = cui_event_hit_within_scope(ui, i, id);
            bool edge = raw && !cui_event_within_scope(ui, i, id);
            bool ending = e->scroll_precise && (e->scroll_phase == CUI_SCROLL_PHASE_ENDED ||
                                                e->scroll_phase == CUI_SCROLL_PHASE_CANCELLED);
            if (e->dx == 0 && e->dy == 0 && !ending)
                continue;
            if (!routed && !s->scrolling && !(s->scroll_deferred && ending) && !edge)
                continue;
            bool matching = s->axis == CUI_ORIENTATION_HORIZONTAL ? fabsf(e->dx) > fabsf(e->dy)
                                                                  : fabsf(e->dy) >= fabsf(e->dx);
            if (!matching && !s->scrolling && !ending)
                continue;
            bool owned_burst = s->wheel_owned && o.now_seconds >= s->last_scroll &&
                               o.now_seconds - s->last_scroll < .028;
            if (edge && !s->scrolling && !owned_burst && !ending) {
                s->scroll_deferred = e->scroll_precise;
                s->wheel_owned = false;
                s->scroll_seen = true;
                s->last_scroll = o.now_seconds;
                continue;
            }
            if (cui_event_consumed(ui, i))
                continue;
            bool handled;
            if (e->scroll_precise)
                handled =
                    e->pointer_type == CUI_POINTER_TOUCH
                        ? cui_carousel_touch_scroll(s, e->dx, e->dy, e->scroll_phase, o.now_seconds)
                        : cui_carousel_scroll(s, e->dx, e->dy, e->scroll_phase, o.now_seconds);
            else {
                float delta = fabsf(e->dx) > fabsf(e->dy) ? e->dx : e->dy;
                handled = cui_carousel_wheel(s, delta, o.now_seconds);
            }
            if (handled) {
                cui_consume_event(ui, i);
                cui_invalidate(ui);
            } else if (routed)
                cui_defer_scroll_event(ui, i, id);
        }
    }
}
static void cui_carousel_keys(cui_context *ui, cui_id id, cui_carousel_state *s) {
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count; ++i) {
        const cui_event *e = &events[i];
        if (e->type != CUI_EVENT_KEY_DOWN || cui_event_consumed(ui, i) ||
            !cui_event_within_scope(ui, i, id))
            continue;
        bool changed = false;
        if (e->key == CUI_KEY_HOME)
            changed = cui_carousel_select(s, 0);
        else if (e->key == CUI_KEY_END)
            changed = cui_carousel_select(s, s->count ? s->count - 1 : CUI_CAROUSEL_NO_SELECTION);
        else if ((s->axis == CUI_ORIENTATION_HORIZONTAL && e->key == CUI_KEY_LEFT) ||
                 (s->axis == CUI_ORIENTATION_VERTICAL && e->key == CUI_KEY_UP))
            changed = cui_carousel_step(s, false);
        else if ((s->axis == CUI_ORIENTATION_HORIZONTAL && e->key == CUI_KEY_RIGHT) ||
                 (s->axis == CUI_ORIENTATION_VERTICAL && e->key == CUI_KEY_DOWN))
            changed = cui_carousel_step(s, true);
        if (changed) {
            cui_consume_event(ui, i);
            cui_invalidate(ui);
        }
    }
}
static void cui_carousel_semantics(cui_context *ui, cui_id id, cui_carousel_state *s,
                                   cui_carousel_options o) {
    bool owned = s->wheel_owned && o.now_seconds - s->last_scroll < .028;
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){
            .focusable = true,
            .scrollable = !o.disabled,
            .orientation = s->axis,
            .scroll_axis_lock = true,
            .scroll_x =
                s->axis == CUI_ORIENTATION_HORIZONTAL
                    ? -s->offset + ((owned || s->scrolling || s->runtime_looping) ? s->viewport : 0)
                    : 0,
            .scroll_y =
                s->axis == CUI_ORIENTATION_VERTICAL
                    ? -s->offset + ((owned || s->scrolling || s->runtime_looping) ? s->viewport : 0)
                    : 0,
            .scroll_max_x =
                s->axis == CUI_ORIENTATION_HORIZONTAL
                    ? s->maximum +
                          ((owned || s->scrolling || s->runtime_looping) ? 2 * s->viewport : 0)
                    : 0,
            .scroll_max_y =
                s->axis == CUI_ORIENTATION_VERTICAL
                    ? s->maximum +
                          ((owned || s->scrolling || s->runtime_looping) ? 2 * s->viewport : 0)
                    : 0});
    cui_update_semantic(ui, id, false, s->count ? (float)s->selected : -1);
}
bool cui_carousel(cui_context *ui, cui_id id, const cui_theme *theme, cui_carousel_state *s,
                  const cui_carousel_item *items, size_t count, cui_carousel_options o) {
    if (!ui || !theme || !s || !s->items || count > s->capacity ||
        !cui_carousel_valid(id, items, count, o))
        return false;
    float primary = s->axis == CUI_ORIENTATION_HORIZONTAL
                        ? (o.width > 0 ? o.width : s->frame_bounds.w)
                        : o.height;
    if (primary <= 0)
        primary = s->axis == CUI_ORIENTATION_HORIZONTAL ? (o.width > 0 ? o.width : 300) : o.height;
    float track_viewport = primary + o.gap;
    cui_carousel_geometry geometry[CUI_CAROUSEL_ITEM_LIMIT];
    double start = 0;
    for (size_t i = 0; i < count; ++i) {
        float extent = items[i].extent > 0 ? items[i].extent : track_viewport;
        if (extent <= o.gap || start + extent > FLT_MAX)
            return false;
        geometry[i] =
            (cui_carousel_geometry){.id = items[i].id, .start = (float)start, .extent = extent};
        start += extent;
    }
    if (!cui_carousel_set_count(s, count) ||
        !cui_carousel_set_geometry(s, geometry, count, track_viewport))
        return false;
    s->frame_width = o.width > 0 ? o.width : s->frame_bounds.w > 0 ? s->frame_bounds.w : 300;
    s->frame_height = o.height;
    if (cui_focused(ui) != id)
        s->focus_ring_suppressed = false;
    if (!cui_begin_focus_scope(ui, id, false))
        return false;
    cui_interact(ui, id, CUI_ROLE_REGION, o.label.length ? o.label : cui_string("Carousel"),
                 o.disabled, false, (float)s->selected);
    cui_carousel_semantics(ui, id, s, o);
    if (o.disabled)
        cui_carousel_cancel(s);
    else
        cui_carousel_pointer_events(ui, id, s, o);
    s->response = theme->spring_move.response;
    s->damping = theme->spring_move.damping;
    if (cui_carousel_tick(s, o.now_seconds, o.reduced_motion || !theme->spring_move.travel) ||
        (s->wheel_owned && o.now_seconds - s->last_scroll < .028))
        cui_invalidate(ui);
    cui_carousel_semantics(ui, id, s, o);
    cui_box root = o.custom_box ? o.box : (cui_box){.gap = 16};
    cui_scroll_options bars = cui_scroll_resolve(
        theme, (cui_scroll_options){.horizontal = s->axis == CUI_ORIENTATION_HORIZONTAL,
                                    .vertical = s->axis == CUI_ORIENTATION_VERTICAL,
                                    .disabled = o.disabled,
                                    .scrollbar_style = o.scrollbar_style});
    cui_scrollbar_style skin = *bars.scrollbar_style;
    skin.reduced_motion |= o.reduced_motion;
    bars.scrollbar_style = &skin;
    cui_rect bar_viewport = {0, 0, s->frame_width, s->frame_height};
    cui_size bar_content = {bar_viewport.w + (bars.horizontal ? s->maximum : 0),
                            bar_viewport.h + (bars.vertical ? s->maximum : 0)};
    float bar_x = bars.horizontal ? fmaxf(0, -s->offset) : 0;
    float bar_y = bars.vertical ? fmaxf(0, -s->offset) : 0;
    if (!s->runtime_looping &&
        cui_base_scrollbar_overlay(ui, id, &bar_x, &bar_y, bar_viewport, bar_content, bars)) {
        float offset = -(bars.horizontal ? bar_x : bar_y);
        cui_carousel_select(s, cui_carousel_nearest(s, offset));
        s->offset = offset;
        s->velocity = 0;
        cui_invalidate(ui);
    }
    root.width = o.width;
    cui_begin_box(ui, id, root);
    cui_id frame = cui_carousel_part(id, 0);
    cui_box frame_box = {.width = -1, .height = o.height, .clip = true};
    if (!o.hide_focus_ring && !s->focus_ring_suppressed && cui_focused(ui) == id) {
        frame_box.border = theme->focus;
        frame_box.border_width = 1;
        frame_box.radius = theme->radius;
    }
    cui_begin_box(ui, frame, frame_box);
    cui_track_bounds(ui, frame, &s->frame_bounds);
    for (size_t i = 0; i < count; ++i) {
        float position = cui_carousel_item_position(s, i) - o.gap;
        cui_box item = items[i].custom_box ? items[i].box : (cui_box){0};
        item.row = s->axis == CUI_ORIENTATION_HORIZONTAL;
        cui_rect bounds = s->axis == CUI_ORIENTATION_HORIZONTAL
                              ? (cui_rect){.x = position, .w = geometry[i].extent, .h = -1}
                              : (cui_rect){.y = position, .w = -1, .h = geometry[i].extent};
        char label[80];
        snprintf(label, sizeof(label), "Slide %zu of %zu", i + 1, count);
        cui_interact(ui, items[i].id, CUI_ROLE_GROUP,
                     items[i].label.length ? items[i].label : cui_string(label), false, false, 0);
        cui_set_semantic_details(
            ui, items[i].id,
            (cui_semantic_details){.pos_in_set = (unsigned)i + 1, .set_size = (unsigned)count});
        cui_begin_absolute_box(ui, items[i].id, item, bounds);
        cui_begin_box(ui, 0,
                      s->axis == CUI_ORIENTATION_HORIZONTAL
                          ? (cui_box){.width = o.gap, .height = 0}
                          : (cui_box){.height = o.gap, .width = 0});
        cui_end_box(ui);
        cui_begin_box(ui, 0, (cui_box){.width = -1, .height = -1});
        items[i].render(ui, theme, i, items[i].user);
        cui_end_box(ui);
        cui_end_box(ui);
    }
    if (!s->runtime_looping)
        cui_scrollbar_overlay_paint(ui, id, theme, bars.horizontal ? fmaxf(0, -s->offset) : 0,
                                    bars.vertical ? fmaxf(0, -s->offset) : 0, bar_viewport,
                                    bar_content, bars);
    cui_end_box(ui);
    if (o.controls)
        o.controls(ui, theme, s, id, o.controls_user);
    if (o.pagination)
        o.pagination(ui, theme, s, id, o.pagination_user);
    if (!o.disabled)
        cui_carousel_keys(ui, id, s);
    cui_carousel_semantics(ui, id, s, o);
    cui_end_box(ui);
    cui_end_focus_scope(ui);
    return true;
}
static bool cui_carousel_button(cui_context *ui, cui_id id, const cui_theme *theme,
                                cui_carousel_state *s, cui_id root, size_t index, bool next,
                                bool pagination, cui_carousel_control_options o) {
    if (!ui || !id || !theme || !s || !isfinite(o.custom_size) || o.custom_size < 0 ||
        o.size < CUI_CAROUSEL_CONTROL_DEFAULT || o.size > CUI_CAROUSEL_CONTROL_CUSTOM ||
        (o.size == CUI_CAROUSEL_CONTROL_CUSTOM && o.custom_size <= 0) ||
        !cui_carousel_string_valid(o.label))
        return false;
    bool disabled = pagination ? index >= s->count
                               : cui_carousel_navigation(s, next) == CUI_CAROUSEL_NO_SELECTION;
    char pagination_name[48];
    snprintf(pagination_name, sizeof(pagination_name), "Go to slide %zu", index + 1);
    cui_str name = o.label.length ? o.label
                   : pagination   ? cui_string("Select slide")
                   : next         ? cui_string("Next slide")
                                  : cui_string("Previous slide");
    cui_interaction interaction = cui_interact(ui, id, CUI_ROLE_BUTTON, name, disabled,
                                               pagination && s->selected == index, 0);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.selected = pagination && s->selected == index, .controls = root});
    cui_carousel_control_size variant = o.size;
    if (variant == CUI_CAROUSEL_CONTROL_DEFAULT)
        variant = pagination ? CUI_CAROUSEL_CONTROL_XSMALL : CUI_CAROUSEL_CONTROL_MEDIUM;
    float size = variant == CUI_CAROUSEL_CONTROL_CUSTOM   ? o.custom_size
                 : variant == CUI_CAROUSEL_CONTROL_XSMALL ? 20
                 : variant == CUI_CAROUSEL_CONTROL_SMALL  ? 24
                                                          : 32;
    float icon_size = variant == CUI_CAROUSEL_CONTROL_CUSTOM   ? size * .75f
                      : variant == CUI_CAROUSEL_CONTROL_XSMALL ? 12
                      : variant == CUI_CAROUSEL_CONTROL_SMALL  ? 14
                      : variant == CUI_CAROUSEL_CONTROL_LARGE  ? 24
                                                               : 16;
    cui_theme_button_colors palette = theme->buttons[CUI_THEME_BUTTON_DEFAULT];
    bool selected = pagination && s->selected == index;
    cui_color background = pagination            ? selected              ? palette.active
                                                   : interaction.hovered ? palette.hover
                                                                         : palette.normal
                           : interaction.hovered ? palette.outline_hover
                                                 : palette.outline_normal;
    cui_color foreground = disabled     ? theme->muted
                           : pagination ? palette.foreground
                                        : palette.outline_foreground;
    cui_box box = o.custom_box ? o.box
                               : (cui_box){.width = size,
                                           .height = size,
                                           .radius = pagination ? theme->radius : size / 2,
                                           .background = background,
                                           .border = theme->border,
                                           .border_width = pagination ? 0 : 1,
                                           .align_x = CUI_ALIGN_CENTER,
                                           .align_y = CUI_ALIGN_CENTER};
    if (interaction.focused) {
        box.border = theme->focus;
        box.border_width = 1;
    }
    if (pagination)
        cui_begin_box(ui, id, box);
    else {
        float width = s->frame_width, height = s->frame_height;
        cui_rect bounds = {.w = size, .h = size};
        if (s->axis == CUI_ORIENTATION_HORIZONTAL) {
            bounds.x = next ? width + 16 : -size - 16;
            bounds.y = (height - size) / 2;
        } else {
            bounds.x = (width - size) / 2;
            bounds.y = next ? height + 16 : -size - 16;
        }
        cui_begin_absolute_box(ui, id, box, bounds);
    }
    cui_theme inherited = *theme;
    inherited.text = foreground;
    if (o.content)
        o.content(ui, &inherited, pagination ? index : s->selected, o.user);
    else if (!pagination) {
        const char *icon = s->axis == CUI_ORIENTATION_HORIZONTAL
                               ? next ? "chevron-right" : "chevron-left"
                           : next ? "chevron-down"
                                  : "chevron-up";
        cui_icon_named(ui, cui_carousel_part(id, 0), theme, cui_string(icon),
                       (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                          .custom_size = icon_size,
                                          .custom_color = true,
                                          .color = foreground});
    }
    cui_end_box(ui);
    if (!pagination && o.tooltip) {
        cui_hover_options tooltip = o.tooltip_options;
        tooltip.trigger = id;
        if (tooltip.dt <= 0)
            tooltip.dt = .016f;
        if (tooltip.open_delay <= 0)
            tooltip.open_delay = .5f;
        cui_tooltip(ui, cui_carousel_part(id, 2), theme, o.tooltip, name, tooltip);
    }
    bool changed = false;
    if (!disabled && interaction.activated) {
        size_t count = 0;
        const cui_event *events = cui_frame_events(ui, &count);
        for (size_t i = 0; i < count; ++i) {
            if (cui_event_activation(ui, i) != id)
                continue;
            bool result = pagination ? cui_carousel_select(s, index) : cui_carousel_step(s, next);
            changed |= result;
            if (events[i].type == CUI_EVENT_POINTER_UP) {
                cui_focus(ui, root);
                s->focus_ring_suppressed = true;
            }
        }
        cui_invalidate(ui);
    }
    return changed;
}
bool cui_carousel_control(cui_context *ui, cui_id id, const cui_theme *theme, cui_carousel_state *s,
                          cui_id root, bool next, cui_carousel_control_options o) {
    return cui_carousel_button(ui, id, theme, s, root, 0, next, false, o);
}
bool cui_carousel_pagination_item(cui_context *ui, cui_id id, const cui_theme *theme,
                                  cui_carousel_state *s, cui_id root, size_t index,
                                  cui_carousel_control_options o) {
    return cui_carousel_button(ui, id, theme, s, root, index, false, true, o);
}

bool cui_carousel_pagination_begin(cui_context *ui, cui_id id, cui_str label,
                                   const cui_box *custom_box) {
    if (!ui || !id || !cui_carousel_string_valid(label))
        return false;
    cui_interact(ui, id, CUI_ROLE_GROUP, label.length ? label : cui_string("Carousel pagination"),
                 false, false, 0);
    cui_begin_box(ui, id,
                  custom_box ? *custom_box
                             : (cui_box){.width = -1,
                                         .row = true,
                                         .gap = 8,
                                         .align_x = CUI_ALIGN_CENTER,
                                         .align_y = CUI_ALIGN_CENTER});
    return true;
}
void cui_carousel_pagination_end(cui_context *ui) {
    if (ui)
        cui_end_box(ui);
}
