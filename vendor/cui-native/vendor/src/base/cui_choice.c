/* SPDX-License-Identifier: MIT */
#include "cui/cui_choice.h"
#include "cui/cui_base.h"
#include "cui/cui_list_events.h"
#include "cui/cui_scroll.h"
#include "cui_dropdown_metadata_internal.h"
#include <math.h>
static void cui_tab_spring_axis(float *position, float *velocity, float target, float dt,
                                cui_spring_settings settings) {
    double omega = 6.28318530717958647692 / settings.response;
    double damping = settings.damping, displacement = *position - target;
    double speed = *velocity, time = dt, next, next_speed;
    if (fabs(damping - 1) < .0001) {
        double sum = speed + omega * displacement, decay = exp(-omega * time);
        next = (displacement + sum * time) * decay;
        next_speed = (speed - omega * sum * time) * decay;
    } else if (damping < 1) {
        double frequency = omega * sqrt(1 - damping * damping);
        double decay = exp(-damping * omega * time);
        double sine = sin(frequency * time), cosine = cos(frequency * time);
        next = decay * (displacement * cosine +
                        (speed + damping * omega * displacement) / frequency * sine);
        next_speed =
            decay * (speed * cosine -
                     (damping * omega * speed + omega * omega * displacement) / frequency * sine);
    } else {
        double factor = damping + sqrt(damping * damping - 1);
        double slow = -omega / factor, fast = -omega * factor;
        double first = (speed - fast * displacement) / (slow - fast);
        double second = displacement - first;
        next = first * exp(slow * time) + second * exp(fast * time);
        next_speed = slow * first * exp(slow * time) + fast * second * exp(fast * time);
    }
    *position = (float)(target + next);
    *velocity = (float)next_speed;
    if (fabs(next) < settings.epsilon && fabs(next_speed) < settings.epsilon) {
        *position = target;
        *velocity = 0;
    }
}
void cui_tab_motion_step(cui_tab_motion *motion, float target_left, float target_width, float dt,
                         cui_spring_settings settings, bool reduced_motion) {
    if (!motion || !isfinite(target_left) || !isfinite(target_width) || target_width < 0)
        return;
    if (!isfinite(settings.response) || settings.response < 0)
        settings.response = .28f;
    if (!isfinite(settings.damping) || settings.damping < 0)
        settings.damping = .85f;
    if (!isfinite(settings.epsilon) || settings.epsilon <= 0)
        settings.epsilon = .1f;
    if (!motion->initialized || reduced_motion || !settings.travel || settings.response == 0 ||
        !isfinite(motion->left) || !isfinite(motion->width) || !isfinite(motion->left_velocity) ||
        !isfinite(motion->width_velocity)) {
        *motion = (cui_tab_motion){.left = target_left, .width = target_width, .initialized = true};
        return;
    }
    if (!isfinite(dt) || dt <= 0)
        return;
    cui_tab_spring_axis(&motion->left, &motion->left_velocity, target_left, dt, settings);
    cui_tab_spring_axis(&motion->width, &motion->width_velocity, target_width, dt, settings);
}
cui_dropdown_button_metrics cui_dropdown_button_size_metrics(cui_dropdown_button_size size,
                                                             float density) {
    if (!isfinite(density) || density <= 0)
        density = 1;
    size_t index = size == CUI_DROPDOWN_BUTTON_XSMALL  ? 0
                   : size == CUI_DROPDOWN_BUTTON_SMALL ? 1
                   : size == CUI_DROPDOWN_BUTTON_LARGE ? 3
                                                       : 2;
    const float heights[] = {20, 24, 32, 32}, padding[] = {4, 8, 10, 12};
    const float fonts[] = {12, 14, 14, 16}, gaps[] = {4, 4, 8, 8}, caret[] = {12, 14, 16, 16};
    return (cui_dropdown_button_metrics){heights[index] * density, padding[index] * density,
                                         fonts[index] * density, gaps[index] * density,
                                         caret[index] * density};
}
static size_t cui_choice_navigate(const cui_choice_item *items, size_t count, size_t from,
                                  cui_key key, bool vertical) {
    int direction = 0;
    if (key == CUI_KEY_HOME) {
        from = count - 1;
        direction = 1;
    } else if (key == CUI_KEY_END) {
        from = 0;
        direction = -1;
    } else if (key == (vertical ? CUI_KEY_DOWN : CUI_KEY_RIGHT))
        direction = 1;
    else if (key == (vertical ? CUI_KEY_UP : CUI_KEY_LEFT))
        direction = -1;
    if (!direction)
        return count;
    for (size_t n = 0; n < count; ++n) {
        from = direction > 0 ? (from + 1) % count : (from ? from - 1 : count - 1);
        if (!items[from].disabled)
            return from;
    }
    return count;
}
bool cui_tab_scroll_reveal(cui_tab_scroll_state *state, float start, float extent, float viewport) {
    if (!state || !isfinite(start) || !isfinite(extent) || !isfinite(viewport) || start < 0 ||
        extent < 0 || viewport <= 0 || !isfinite(start + extent) || !isfinite(state->maximum) ||
        state->maximum < 0)
        return false;
    float before = state->offset;
    float offset = isfinite(before) ? fminf(state->maximum, fmaxf(0, before)) : 0;
    if (extent >= viewport || start < offset)
        offset = start;
    else if (start + extent > offset + viewport)
        offset = start + extent - viewport;
    state->offset = fminf(state->maximum, fmaxf(0, offset));
    state->changed |= before != state->offset;
    return before != state->offset;
}
void cui_tab_overflow_init(cui_tab_overflow *state, cui_id popup, cui_id trigger,
                           cui_menu_entry *entries, size_t capacity) {
    if (!state)
        return;
    *state = (cui_tab_overflow){.entries = entries, .capacity = capacity, .trigger = trigger};
    cui_menu_tree_init(&state->menu, popup);
}
cui_tab_metrics cui_tab_size_metrics(cui_tab_variant variant, cui_tab_size size, float density) {
    if (!isfinite(density) || density <= 0)
        density = 1;
    unsigned index = size == CUI_TAB_XSMALL  ? 0
                     : size == CUI_TAB_SMALL ? 1
                     : size == CUI_TAB_LARGE ? 3
                                             : 2;
    const float heights[] = {20, 24, 32, 36}, underline_heights[] = {26, 30, 36, 44};
    const float tab_inner[] = {18, 22, 30, 36}, rounded_inner[] = {18, 22, 26, 36};
    const float segmented_inner[] = {16, 18, 24, 28}, underline_inner[] = {20, 22, 26, 32};
    const float paddings[] = {8, 10, 12, 16}, fonts[] = {12, 14, 14, 16},
                icons[] = {10, 14, 16, 16};
    float inner = variant == CUI_TAB_UNDERLINE                            ? underline_inner[index]
                  : variant == CUI_TAB_SEGMENTED                          ? segmented_inner[index]
                  : variant == CUI_TAB_OUTLINE || variant == CUI_TAB_PILL ? rounded_inner[index]
                                                                          : tab_inner[index];
    const float outline_gap[] = {8, 8, 12, 16}, underline_gap[] = {10, 12, 16, 20};
    float gap = variant == CUI_TAB_OUTLINE     ? outline_gap[index]
                : variant == CUI_TAB_UNDERLINE ? underline_gap[index]
                : variant == CUI_TAB_PILL      ? 4
                : variant == CUI_TAB_SEGMENTED ? 2
                                               : 0;
    const float bar_padding[] = {2, 3, 4, 4}, top[] = {1, 2, 3, 5}, bottom[] = {2, 3, 4, 6};
    return (cui_tab_metrics){
        .height =
            (variant == CUI_TAB_UNDERLINE ? underline_heights[index] : heights[index]) * density,
        .inner_height = inner * density,
        .padding_x = variant == CUI_TAB_UNDERLINE ? 0 : paddings[index] * density,
        .font_size = fonts[index] * density,
        .icon_size = icons[index] * density,
        .gap = gap * density,
        .part_gap = 4 * density,
        .bar_padding_x = variant == CUI_TAB_SEGMENTED ? bar_padding[index] * density : 0,
        .margin_top = variant == CUI_TAB_UNDERLINE ? top[index] * density : 0,
        .margin_bottom = variant == CUI_TAB_UNDERLINE ? bottom[index] * density : 0};
}
static void cui_tab_scrollbar_reduce(cui_context *ui, cui_tabs_options options) {
    cui_tab_scroll_state *scroll = options.scroll;
    if (!scroll || !scroll->scrollbar_id || scroll->maximum <= 0) {
        if (scroll)
            scroll->scrollbar_dragging = false;
        return;
    }
    if (options.disabled)
        scroll->scrollbar_dragging = false;
    if (cui_scrollbar_opacity(ui,
                              scroll->receiving_id ? scroll->receiving_id : scroll->scrollbar_id,
                              scroll->scrollbar_id, options.scrollbar_style, scroll->offset,
                              scroll->scrollbar_dragging) <= 0) {
        scroll->scrollbar_dragging = false;
        return;
    }
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    cui_rect bounds = scroll->scrollbar_bounds;
    float track = options.vertical ? bounds.h : bounds.w;
    float thumb = track > 0 && options.bar_extent > 0
                      ? track * (float)((double)options.bar_extent /
                                        ((double)options.bar_extent + scroll->maximum))
                      : 0;
    float minimum = options.scrollbar_style && isfinite(options.scrollbar_style->min_thumb) &&
                            options.scrollbar_style->min_thumb > 0
                        ? options.scrollbar_style->min_thumb
                        : 16;
    thumb = fminf(track, fmaxf(minimum, thumb));
    float travel = track - thumb;
    for (size_t e = 0; e < count; e++) {
        if (cui_event_consumed(ui, e))
            continue;
        cui_event event = events[e];
        if (event.type == CUI_EVENT_CANCEL ||
            (event.type == CUI_EVENT_KEY_DOWN && event.key == CUI_KEY_ESCAPE))
            scroll->scrollbar_dragging = false;
        if (options.disabled || cui_event_target(ui, e) != scroll->scrollbar_id ||
            ((event.type == CUI_EVENT_POINTER_DOWN || event.type == CUI_EVENT_POINTER_UP) &&
             event.pointer_button != CUI_POINTER_PRIMARY))
            continue;
        float before = scroll->offset;
        float position = options.vertical ? event.y - bounds.y : event.x - bounds.x;
        if (event.type == CUI_EVENT_POINTER_DOWN && isfinite(position)) {
            float start = travel > 0 ? travel * scroll->offset / scroll->maximum : 0;
            scroll->scrollbar_grab =
                position >= start && position <= start + thumb ? position - start : thumb * .5f;
            scroll->scrollbar_dragging = true;
        }
        if (scroll->scrollbar_dragging && travel > 0 && isfinite(position) &&
            (event.type == CUI_EVENT_POINTER_DOWN || event.type == CUI_EVENT_POINTER_MOVE ||
             event.type == CUI_EVENT_POINTER_UP))
            scroll->offset =
                scroll->maximum * fminf(1, fmaxf(0, (position - scroll->scrollbar_grab) / travel));
        if (event.type == CUI_EVENT_POINTER_UP)
            scroll->scrollbar_dragging = false;
        if (event.type == CUI_EVENT_KEY_DOWN) {
            if (event.key == CUI_KEY_HOME)
                scroll->offset = 0;
            else if (event.key == CUI_KEY_END)
                scroll->offset = scroll->maximum;
            else if (event.key == CUI_KEY_PAGEUP)
                scroll->offset -= options.bar_extent;
            else if (event.key == CUI_KEY_PAGEDOWN)
                scroll->offset += options.bar_extent;
            else if (event.key == (options.vertical ? CUI_KEY_UP : CUI_KEY_LEFT))
                scroll->offset -= 32;
            else if (event.key == (options.vertical ? CUI_KEY_DOWN : CUI_KEY_RIGHT))
                scroll->offset += 32;
            scroll->offset = fminf(scroll->maximum, fmaxf(0, scroll->offset));
        }
        if (before != scroll->offset && event.type == CUI_EVENT_KEY_DOWN &&
            (event.key == CUI_KEY_LEFT || event.key == CUI_KEY_RIGHT))
            cui_consume_event(ui, e);
        scroll->changed |= before != scroll->offset;
    }
    cui_update_semantic(ui, scroll->scrollbar_id, false, scroll->offset);
}
static bool cui_tab_drag_event(cui_context *ui, const cui_choice_item *items, size_t count,
                               cui_tabs_options options, cui_event event, cui_id target) {
    cui_tab_drag_state *drag = options.drag;
    if (!drag ||
        (event.type == CUI_EVENT_POINTER_UP && event.pointer_button != CUI_POINTER_PRIMARY))
        return false;
    if (event.type == CUI_EVENT_CANCEL ||
        (event.type == CUI_EVENT_KEY_DOWN && event.key == CUI_KEY_ESCAPE)) {
        drag->candidate = 0;
        drag->active = false;
        return false;
    }
    if (event.type == CUI_EVENT_POINTER_DOWN && event.pointer_button == CUI_POINTER_PRIMARY) {
        drag->candidate = 0;
        drag->active = false;
        for (size_t i = 0; i < count; i++)
            if (items[i].id == target && !items[i].disabled) {
                drag->candidate = target;
                drag->start_x = event.x;
                drag->start_y = event.y;
                break;
            }
        return false;
    }
    if (!drag->candidate ||
        (event.type != CUI_EVENT_POINTER_MOVE && event.type != CUI_EVENT_POINTER_UP))
        return false;
    if (target != drag->candidate || !isfinite(event.x) || !isfinite(event.y)) {
        drag->candidate = 0;
        drag->active = false;
        return false;
    }
    if (fabsf(event.x - drag->start_x) >= 4 || fabsf(event.y - drag->start_y) >= 4)
        drag->active = true;
    if (!drag->active) {
        if (event.type == CUI_EVENT_POINTER_UP)
            drag->candidate = 0;
        return false;
    }
    bool inside = true;
    if (options.scroll) {
        cui_rect bounds = options.scroll->viewport;
        inside = event.x >= bounds.x && event.x < bounds.x + bounds.w && event.y >= bounds.y &&
                 event.y < bounds.y + bounds.h;
    }
    bool found = false;
    drag->before = 0;
    for (size_t i = 0; i < count && inside; i++) {
        cui_rect bounds = cui_bounds(ui, items[i].id);
        if (event.x < bounds.x || event.x >= bounds.x + bounds.w || event.y < bounds.y ||
            event.y >= bounds.y + bounds.h)
            continue;
        found = true;
        bool after = options.vertical ? event.y >= bounds.y + bounds.h * .5f
                                      : event.x >= bounds.x + bounds.w * .5f;
        size_t insertion = i + (after ? 1 : 0);
        if (insertion < count && items[insertion].id == drag->candidate)
            insertion++;
        drag->before = insertion < count ? items[insertion].id : 0;
        break;
    }
    drag->drop_valid = found;
    if (event.type == CUI_EVENT_POINTER_UP) {
        size_t source = count;
        for (size_t i = 0; i < count; i++)
            if (items[i].id == drag->candidate)
                source = i;
        cui_id next = source + 1 < count ? items[source + 1].id : 0;
        if (found && drag->before != drag->candidate && drag->before != next) {
            drag->moved = drag->candidate;
            drag->requested = true;
        }
        drag->candidate = 0;
        drag->active = false;
    }
    cui_invalidate(ui);
    return true;
}
bool cui_tabs_shadow_policy_valid(cui_tabs_options options) {
    return options.shadow_policy >= CUI_TAB_SHADOW_LEGACY &&
           options.shadow_policy <= CUI_TAB_SHADOW_SOURCE_BAR &&
           !(options.variant == CUI_TAB_SEGMENTED &&
             options.shadow_policy == CUI_TAB_SHADOW_SOURCE_BAR &&
             (!options.animation || options.vertical));
}
bool cui_base_tabs(cui_context *ui, const cui_choice_item *items, size_t count, cui_id *selected,
                   cui_tabs_options options) {
    if (!cui_tabs_shadow_policy_valid(options))
        return false;
    if (!count && options.overflow) {
        options.overflow->visible = false;
        if (options.overflow->menu.overlay.open)
            cui_overlay_close(ui, &options.overflow->menu.overlay);
        options.overflow->menu.depth = 0;
    }
    if (options.drag) {
        options.drag->requested = false;
        bool present = false;
        for (size_t i = 0; items && i < count; i++)
            present |= items[i].id == options.drag->candidate && !items[i].disabled;
        if (options.disabled || !present) {
            options.drag->candidate = 0;
            options.drag->active = false;
        }
    }
    if (!items || !selected || !count)
        return false;
    if (options.scroll) {
        cui_tab_scroll_state *scroll = options.scroll;
        scroll->changed = false;
        if (!isfinite(scroll->maximum) || scroll->maximum < 0)
            scroll->maximum = 0;
        if (!isfinite(scroll->offset) || scroll->offset < 0)
            scroll->offset = 0;
        if (scroll->offset > scroll->maximum)
            scroll->offset = scroll->maximum;
        size_t n = 0;
        const cui_event *events = cui_frame_events(ui, &n);
        for (size_t e = 0; e < n && !options.disabled; e++) {
            if (cui_event_consumed(ui, e))
                continue;
            cui_event event = events[e];
            cui_id target = cui_event_target(ui, e);
            bool eligible = (scroll->receiving_id && target == scroll->receiving_id) ||
                            (scroll->scrollbar_id && target == scroll->scrollbar_id);
            for (size_t i = 0; i < count; i++)
                eligible |= target == items[i].id;
            cui_rect bounds = scroll->viewport;
            if (event.type != CUI_EVENT_SCROLL || !eligible || bounds.w <= 0 || bounds.h <= 0 ||
                event.x < bounds.x || event.x >= bounds.x + bounds.w || event.y < bounds.y ||
                event.y >= bounds.y + bounds.h)
                continue;
            float delta = options.vertical ? event.dy : event.dx;
            if (!isfinite(delta))
                continue;
            float before = scroll->offset;
            float after = before - delta * (event.scroll_precise ? 1 : 32);
            if (!isfinite(after))
                after = delta < 0 ? scroll->maximum : 0;
            scroll->offset = fminf(scroll->maximum, fmaxf(0, after));
            if (before != scroll->offset) {
                scroll->changed = true;
                cui_consume_event(ui, e);
            } else
                cui_defer_scroll_event(ui, e, target);
        }
        if (scroll->changed)
            cui_invalidate(ui);
    }
    cui_tab_scrollbar_reduce(ui, options);
    if (options.scroll && options.scroll->changed)
        cui_invalidate(ui);
    if (options.scroll && options.scroll->changed && options.drag) {
        options.drag->candidate = 0;
        options.drag->active = false;
        options.drag->drop_valid = false;
    }
    cui_id old = *selected;
    if (options.overflow) {
        cui_tab_overflow *overflow = options.overflow;
        overflow->visible = options.scroll && options.scroll->maximum > 0 && overflow->trigger &&
                            overflow->menu.overlay.id && overflow->entries &&
                            overflow->capacity >= count;
        if (!overflow->visible) {
            if (overflow->menu.overlay.open)
                cui_overlay_close(ui, &overflow->menu.overlay);
            overflow->menu.depth = 0;
        } else {
            for (size_t i = 0; i < count; i++)
                overflow->entries[i] =
                    (cui_menu_entry){.id = items[i].id ^ UINT64_C(0x8322717be8bd65e1),
                                     .label = items[i].label,
                                     .disabled = items[i].disabled,
                                     .kind = CUI_MENU_ACTION};
            cui_id action = 0;
            if (cui_base_menu_tree(ui, overflow->trigger, cui_string("All tabs"), overflow->entries,
                                   count, &action, &overflow->menu,
                                   options.disabled || overflow->options.popup.disabled,
                                   overflow->options.popup.dt, false)) {
                for (size_t i = 0; i < count; i++)
                    if (overflow->entries[i].id == action)
                        *selected = items[i].id;
            }
        }
    }

    for (size_t i = 0; i < count; ++i) {
        cui_interact(ui, items[i].id, CUI_ROLE_TAB, items[i].label,
                     options.disabled || items[i].disabled, *selected == items[i].id, (float)i);
    }
    size_t event_count = 0, cursor = count;
    const cui_event *events = cui_frame_events(ui, &event_count);
    for (size_t e = 0; e < event_count && !options.disabled; ++e) {
        if (cui_event_consumed(ui, e))
            continue;
        if (options.scroll && options.scroll->changed &&
            (events[e].type == CUI_EVENT_POINTER_DOWN || events[e].type == CUI_EVENT_POINTER_UP)) {
            if (options.drag) {
                options.drag->candidate = 0;
                options.drag->active = false;
            }
            continue;
        }
        if (cui_tab_drag_event(ui, items, count, options, events[e], cui_event_target(ui, e)))
            continue;
        if (events[e].type == CUI_EVENT_KEY_DOWN && events[e].key == CUI_KEY_TAB)
            cursor = count;
        size_t from = count;
        for (size_t i = 0; i < count; ++i)
            if (items[i].id == cui_event_target(ui, e))
                from = i;
        if (from == count || items[from].disabled)
            continue;
        if (cursor == count)
            cursor = from;
        if (cui_event_activation(ui, e) && (events[e].type != CUI_EVENT_KEY_UP ||
                                            items[cursor].id == cui_event_activation(ui, e))) {
            if (events[e].type == CUI_EVENT_POINTER_UP)
                cursor = from;
            *selected = items[cursor].id;
        }
        size_t next =
            events[e].type == CUI_EVENT_KEY_DOWN
                ? cui_choice_navigate(items, count, cursor, events[e].key, options.vertical)
                : count;
        if (next != count) {
            if (next != cursor && (events[e].key == CUI_KEY_LEFT || events[e].key == CUI_KEY_RIGHT))
                cui_consume_event(ui, e);
            cursor = next;
            cui_focus(ui, items[next].id);
            if (!options.manual_activation)
                *selected = items[next].id;
        }
    }
    size_t stop = count;
    for (size_t i = 0; i < count; ++i)
        if (!items[i].disabled && items[i].id == cui_focused(ui))
            stop = i;
    if (stop == count)
        for (size_t i = 0; i < count; ++i)
            if (!items[i].disabled && items[i].id == *selected)
                stop = i;
    if (stop == count)
        for (size_t i = 0; i < count; ++i)
            if (!items[i].disabled) {
                stop = i;
                break;
            }
    for (size_t i = 0; i < count; ++i) {
        cui_update_semantic(ui, items[i].id, *selected == items[i].id, (float)i);
        cui_set_semantic_details(
            ui, items[i].id,
            (cui_semantic_details){.selected = *selected == items[i].id,
                                   .controls = items[i].id ^ UINT64_C(0x691a17db80e22f45)});
        cui_set_tab_stop(ui, items[i].id, i == stop);
    }
    return old != *selected;
}
static cui_component_event_metadata cui_choice_toggle_metadata(cui_context *ui, cui_id control,
                                                               size_t index, cui_event event) {
    cui_component_event_metadata metadata = {0};
    cui_activation_snapshot snapshot;
    if (!cui_event_activation_snapshot(ui, index, &snapshot) ||
        !cui_component_event_from_activation(&metadata, CUI_COMPONENT_EVENT_CHANGE, &snapshot)) {
        cui_component_event_source source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN;
        if (event.type == CUI_EVENT_POINTER_UP)
            source = CUI_COMPONENT_EVENT_SOURCE_POINTER;
        else if (event.type == CUI_EVENT_KEY_DOWN || event.type == CUI_EVENT_KEY_UP)
            source = CUI_COMPONENT_EVENT_SOURCE_KEYBOARD;
        else if (event.type == CUI_EVENT_ACCESSIBILITY_ACTION)
            source = CUI_COMPONENT_EVENT_SOURCE_ACCESSIBILITY;
        (void)cui_component_event_init(&metadata, CUI_COMPONENT_EVENT_CHANGE, control, source);
        (void)cui_component_event_set_frame_index(&metadata, cui_frame_sequence(ui), index);
    }
    metadata.target = control;
    return metadata;
}
static void cui_choice_toggle_notify(cui_choice_items_event_result *result,
                                     cui_choice_item_handler handler,
                                     cui_component_event_metadata metadata, cui_id item,
                                     cui_id trigger, bool previous, bool value) {
    cui_choice_item_change change = {metadata, item, trigger, previous, value};
    if (result->change_count == SIZE_MAX)
        result->count_overflow = true;
    else
        result->change_count++;
    handler.on_change(handler.user, &change);
}
static bool cui_base_toggle_group_common(cui_context *ui, const cui_choice_item *items,
                                         size_t count, bool *selected,
                                         cui_toggle_group_options options, cui_id control,
                                         const cui_choice_item_handler *handler,
                                         cui_choice_items_event_result *result) {
    if (!items || !selected || !count)
        return false;
    if (result) {
        result->valid = true;
        result->submitted = cui_frame_sequence(ui) != 0;
    }
    bool changed = false;
    for (size_t i = 0; i < count; ++i) {
        cui_interact(ui, items[i].id, CUI_ROLE_BUTTON, items[i].label,
                     options.disabled || items[i].disabled, selected[i], selected[i]);
    }
    size_t event_count = 0, cursor = count;
    const cui_event *events = cui_frame_events(ui, &event_count);
    for (size_t e = 0; e < event_count && !options.disabled; ++e) {
        if (cui_event_consumed(ui, e))
            continue;
        if (events[e].type == CUI_EVENT_KEY_DOWN && events[e].key == CUI_KEY_TAB)
            cursor = count;
        size_t from = count;
        for (size_t i = 0; i < count; ++i)
            if (items[i].id == cui_event_target(ui, e))
                from = i;
        if (from == count || items[from].disabled)
            continue;
        if (cursor == count)
            cursor = from;
        if (cui_event_activation(ui, e) && (events[e].type != CUI_EVENT_KEY_UP ||
                                            items[cursor].id == cui_event_activation(ui, e))) {
            if (events[e].type == CUI_EVENT_POINTER_UP)
                cursor = from;
            bool next_selected = !selected[cursor];
            size_t active_count = 0;
            for (size_t j = 0; j < count; j++)
                active_count += selected[j];
            if (next_selected || options.allow_empty || active_count > 1) {
                bool notify = handler && handler->on_change && cui_frame_sequence(ui) != 0;
                cui_component_event_metadata metadata = {0};
                cui_id trigger = items[cursor].id;
                bool previous = selected[cursor];
                if (notify)
                    metadata = cui_choice_toggle_metadata(ui, control, e, events[e]);
                if (next_selected && !options.multiple)
                    for (size_t j = 0; j < count; j++) {
                        bool was_selected = selected[j];
                        selected[j] = false;
                        if (notify && was_selected)
                            cui_choice_toggle_notify(result, *handler, metadata, items[j].id,
                                                     trigger, true, false);
                    }
                selected[cursor] = next_selected;
                if (notify && previous != next_selected)
                    cui_choice_toggle_notify(result, *handler, metadata, trigger, trigger, previous,
                                             next_selected);
                changed = true;
            }
        }
        size_t next =
            events[e].type == CUI_EVENT_KEY_DOWN
                ? cui_choice_navigate(items, count, cursor, events[e].key, options.vertical)
                : count;
        if (next != count) {
            if (next != cursor && (events[e].key == CUI_KEY_LEFT || events[e].key == CUI_KEY_RIGHT))
                cui_consume_event(ui, e);
            cursor = next;
            cui_focus(ui, items[next].id);
        }
    }
    for (size_t i = 0; i < count; ++i)
        cui_update_semantic(ui, items[i].id, selected[i], selected[i]);
    return changed;
}

bool cui_base_toggle_group(cui_context *ui, const cui_choice_item *items, size_t count,
                           bool *selected, cui_toggle_group_options options) {
    return cui_base_toggle_group_common(ui, items, count, selected, options, 0, NULL, NULL);
}
cui_choice_items_event_result cui_base_toggle_group_events(cui_context *ui, cui_id control,
                                                           const cui_choice_item *items,
                                                           size_t count, bool *selected,
                                                           cui_toggle_group_options options,
                                                           cui_choice_item_handler handler) {
    cui_choice_items_event_result result = {0};
    if (!ui || !control)
        return result;
    result.legacy_result = cui_base_toggle_group_common(ui, items, count, selected, options,
                                                        control, &handler, &result);
    return result;
}

static unsigned char cui_choice_lower(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}
static bool cui_choice_prefix(cui_str label, const char *prefix, size_t length) {
    if (!label.data || label.length < length)
        return false;
    for (size_t i = 0; i < length; ++i)
        if (cui_choice_lower((unsigned char)label.data[i]) !=
            cui_choice_lower((unsigned char)prefix[i]))
            return false;
    return true;
}
void cui_choice_popup_init(cui_choice_popup *state, cui_id popup_id) {
    if (!state)
        return;
    *state = (cui_choice_popup){0};
    cui_overlay_init(&state->overlay, popup_id, true);
}
static bool cui_choice_scroll_event(cui_context *ui, const cui_choice_item *items, size_t count,
                                    cui_list_state *state, size_t length, size_t *begin,
                                    cui_rect bounds, cui_id scope, cui_id receiving, cui_id routed,
                                    cui_event event) {
    if ((!receiving || (routed != receiving && (!scope || routed != scope))) || !count || !length ||
        event.type != CUI_EVENT_SCROLL || !isfinite(event.dy) || !isfinite(bounds.x) ||
        !isfinite(bounds.y) || !isfinite(bounds.w) || !isfinite(bounds.h) || bounds.w <= 0 ||
        bounds.h <= 0 || event.x < bounds.x || event.x >= bounds.x + bounds.w ||
        event.y < bounds.y || event.y >= bounds.y + bounds.h ||
        !cui_pointer_scope_allowed(ui, scope))
        return false;
    if (length > count)
        length = count;
    if (*begin > count - length)
        *begin = count - length;
    float amount = state->scroll_remainder -
                   event.dy / (event.scroll_precise ? fmaxf(1, bounds.h / (float)length) : 1);
    if (!isfinite(amount))
        amount = event.dy > 0 ? -(float)count : (float)count;
    float whole = truncf(amount);
    state->scroll_remainder = amount - whole;
    size_t before = *begin;
    if (whole < 0) {
        float steps = -whole;
        *begin = steps >= (float)*begin ? 0 : *begin - (size_t)steps;
    } else {
        size_t remaining = count - length - *begin;
        *begin += whole >= (float)remaining ? remaining : (size_t)whole;
    }
    if (before == *begin)
        return false;
    bool focused_option = false, active_visible = false;
    for (size_t i = 0; i < count; i++) {
        focused_option |= cui_focused(ui) == items[i].id;
        if (i >= *begin && i < *begin + length && !items[i].disabled &&
            items[i].id == state->active)
            active_visible = true;
    }
    if (!active_visible) {
        state->active = 0;
        for (size_t i = *begin; i < *begin + length; i++)
            if (!items[i].disabled) {
                state->active = items[i].id;
                break;
            }
        if (focused_option)
            cui_focus(ui, state->active);
    }
    cui_invalidate(ui);
    return true;
}
typedef struct {
    bool *selected;
    cui_choice_item_handler handler;
    cui_choice_items_event_result *result;
} cui_choice_multi_observer;
/* Private forwarding seam; legacy options/layout and callers stay unchanged. */
static bool
cui_choice_list_reduce(cui_context *ui, const cui_choice_item *items, size_t count,
                       cui_id *selected, cui_list_state *state, cui_list_options options,
                       cui_id control, cui_selection_handler handler,
                       cui_selection_event_result *observed, cui_menu_command_handler commands,
                       cui_menu_command_result *command_result, cui_choice_multi_observer *multi) {
    if ((!items && count) || !selected || !state)
        return false;
    if (!count) {
        state->first_visible = 0;
        state->secondary_active = 0;
        size_t event_count = 0;
        const cui_event *events = cui_frame_events(ui, &event_count);
        for (size_t e = 0; e < event_count && !options.disabled; e++)
            if (events[e].type == CUI_EVENT_KEY_DOWN && events[e].key == CUI_KEY_ESCAPE &&
                !events[e].repeat && options.on_cancel &&
                ((options.scroll_target && cui_event_target(ui, e) == options.scroll_target) ||
                 (options.scroll_scope && cui_event_target(ui, e) == options.scroll_scope)))
                options.on_cancel(options.activation_user);
        return false;
    }
    bool activated = false;
    if (state->search_length >= sizeof(state->search))
        state->search_length = 0;
    if (options.dt > 0 && options.dt < 60)
        state->search_time += options.dt;
    if (state->search_time > .75f)
        state->search_length = 0;
    size_t event_count = 0;
    const cui_event *events = cui_frame_events(ui, &event_count);
    size_t window_length = options.visible_count ? options.visible_count : count;
    if (window_length > count)
        window_length = count;
    size_t window_begin = options.visible_count ? options.visible_begin : 0;
    if (window_begin > count - window_length)
        window_begin = count - window_length;
    bool scrolled = options.window_changed;
    for (size_t e = 0; e < event_count && !options.disabled; ++e) {
        if (cui_event_consumed(ui, e))
            continue;
        cui_event event = events[e];
        scrolled |= cui_choice_scroll_event(
            ui, items, count, state, options.visible_count, &window_begin, options.scroll_bounds,
            options.scroll_scope, options.scroll_target, cui_event_target(ui, e), event);
        if (event.type == CUI_EVENT_SCROLL)
            continue;
        size_t target = count, from = count;
        for (size_t i = 0; i < count; ++i) {
            if (items[i].id == cui_event_target(ui, e))
                target = i;
            if (items[i].id == state->active)
                from = i;
        }
        bool owned_target =
            (target < count && !items[target].disabled) ||
            (options.scroll_target && cui_event_target(ui, e) == options.scroll_target) ||
            (options.scroll_scope && cui_event_target(ui, e) == options.scroll_scope);
        if (owned_target && event.type == CUI_EVENT_KEY_DOWN && event.key == CUI_KEY_ESCAPE &&
            !event.repeat) {
            state->secondary_active = 0;
            if (options.on_cancel)
                options.on_cancel(options.activation_user);
        }
        if (target == count || items[target].disabled)
            continue;
        if (options.track_secondary && event.type == CUI_EVENT_POINTER_DOWN &&
            event.pointer_button == CUI_POINTER_SECONDARY) {
            state->secondary_active = items[target].id;
            cui_invalidate(ui);
        }
        if (from == count || items[from].disabled)
            from = target;
        if (cui_event_activation(ui, e) && !(scrolled && event.type == CUI_EVENT_POINTER_UP) &&
            (events[e].type != CUI_EVENT_KEY_UP || items[from].id == cui_event_activation(ui, e))) {
            size_t chosen = events[e].type == CUI_EVENT_POINTER_UP ? target : from;
            cui_id previous = *selected, item = items[chosen].id;
            cui_component_event_metadata metadata;
            memset(&metadata, 0, sizeof(metadata));
            bool notify = observed && handler.on_change && cui_frame_sequence(ui) != 0;
            bool command_notify =
                command_result && commands.on_activate && cui_frame_sequence(ui) != 0;
            bool multi_notify = multi && multi->handler.on_change && cui_frame_sequence(ui) != 0;
            size_t multi_index = count;
            bool previous_bool = false;
            if (multi_notify) {
                /* Match the legacy multi adapter's first matching stable ID. */
                for (size_t i = 0; i < count; ++i)
                    if (items[i].id == item) {
                        multi_index = i;
                        break;
                    }
                if (multi_index < count)
                    previous_bool = multi->selected[multi_index];
            }
            cui_component_event_kind kind =
                command_notify ? CUI_COMPONENT_EVENT_ACTIVATE : CUI_COMPONENT_EVENT_CHANGE;
            if (notify || command_notify || multi_notify) {
                cui_activation_snapshot snapshot;
                if (!cui_event_activation_snapshot(ui, e, &snapshot) ||
                    !cui_component_event_from_activation(&metadata, kind, &snapshot)) {
                    cui_component_event_source source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN;
                    if (event.type == CUI_EVENT_POINTER_UP)
                        source = CUI_COMPONENT_EVENT_SOURCE_POINTER;
                    else if (event.type == CUI_EVENT_KEY_DOWN || event.type == CUI_EVENT_KEY_UP)
                        source = CUI_COMPONENT_EVENT_SOURCE_KEYBOARD;
                    else if (event.type == CUI_EVENT_ACCESSIBILITY_ACTION)
                        source = CUI_COMPONENT_EVENT_SOURCE_ACCESSIBILITY;
                    (void)cui_component_event_init(&metadata, kind, control, source);
                    (void)cui_component_event_set_frame_index(&metadata, cui_frame_sequence(ui), e);
                }
                metadata.target = control;
            }
            *selected = state->active = item;
            activated = true;
            state->secondary_active = 0;
            if (options.on_activate)
                options.on_activate(items[chosen].id, options.activation_user);
            if (options.on_confirm) {
                unsigned modifiers = options.secondary_modifiers ? options.secondary_modifiers
                                                                 : CUI_MOD_CTRL | CUI_MOD_META;
                options.on_confirm(items[chosen].id, (event.modifiers & modifiers) != 0,
                                   options.activation_user);
            }
            cui_id value = *selected;
            if (notify && previous != value) {
                cui_selection_change change = {metadata, previous, value, item,
                                               CUI_SELECTION_REASON_ACTIVATION};
                if (observed->change_count == SIZE_MAX)
                    observed->count_overflow = true;
                else
                    observed->change_count++;
                handler.on_change(handler.user, &change);
            }
            if (multi_notify && multi_index < count &&
                previous_bool != multi->selected[multi_index]) {
                cui_choice_item_change change = {metadata, item, item, previous_bool,
                                                 multi->selected[multi_index]};
                if (multi->result->change_count == SIZE_MAX)
                    multi->result->count_overflow = true;
                else
                    multi->result->change_count++;
                multi->handler.on_change(multi->handler.user, &change);
            }
            if (command_notify) {
                cui_menu_command command = {metadata, item};
                if (command_result->command_count == SIZE_MAX)
                    command_result->count_overflow = true;
                else
                    command_result->command_count++;
                commands.on_activate(commands.user, &command);
            }
        }
        if (events[e].type == CUI_EVENT_KEY_DOWN) {
            size_t next = cui_choice_navigate(items, count, from, events[e].key, true);
            if (next != count) {
                state->active = items[next].id;
                cui_focus(ui, state->active);
                state->search_length = 0;
            }
        } else if (events[e].type == CUI_EVENT_TEXT) {
            size_t len = 0;
            while (len < sizeof(events[e].text) && events[e].text[len])
                ++len;
            if (!len || len >= sizeof(events[e].text))
                continue;
            if (len > sizeof(state->search) - 1 - state->search_length)
                state->search_length = 0;
            if (len > sizeof(state->search) - 1)
                continue;
            bool cycle = len == 1 && state->search_length == 1 &&
                         cui_choice_lower((unsigned char)events[e].text[0]) ==
                             cui_choice_lower((unsigned char)state->search[0]);
            if (!cycle) {
                for (size_t i = 0; i < len; ++i)
                    state->search[state->search_length++] = events[e].text[i];
            }
            state->search_time = 0;
            for (size_t n = 0; n < count; ++n) {
                size_t i = (from + 1 + n) % count;
                if (!items[i].disabled &&
                    cui_choice_prefix(items[i].label, state->search, state->search_length)) {
                    state->active = items[i].id;
                    cui_focus(ui, state->active);
                    break;
                }
            }
        }
    }
    if (!options.track_secondary || options.disabled)
        state->secondary_active = 0;
    if (state->secondary_active) {
        bool retained = false;
        for (size_t i = window_begin; i < window_begin + window_length; i++)
            retained |= items[i].id == state->secondary_active && !items[i].disabled;
        if (!retained)
            state->secondary_active = 0;
    }
    size_t begin = window_begin;
    if (begin >= count)
        begin = count - 1;
    size_t length = options.visible_count ? options.visible_count : count;
    if (length > count - begin)
        length = count - begin;
    if (options.visible_count) {
        for (size_t i = 0; i < count; ++i)
            if (items[i].id == state->active && !items[i].disabled) {
                if (i < begin)
                    begin = i;
                else if (i >= begin + length)
                    begin = i + 1 - length;
            }
    }
    state->first_visible = begin;
    bool pointer_moved = false, keyboard_used = false;
    for (size_t e = 0; e < event_count; ++e) {
        if (cui_event_consumed(ui, e))
            continue;
        if (events[e].type == CUI_EVENT_POINTER_MOVE)
            pointer_moved = true;
        if (events[e].type == CUI_EVENT_KEY_DOWN || events[e].type == CUI_EVENT_KEY_UP)
            keyboard_used = true;
    }
    for (size_t i = begin; i < begin + length; ++i) {
        cui_interaction interaction = cui_interact(
            ui, items[i].id, options.menu ? CUI_ROLE_MENUITEM : CUI_ROLE_OPTION, items[i].label,
            options.disabled || items[i].disabled, *selected == items[i].id, (float)i);
        if (!options.disabled && !items[i].disabled && interaction.hovered && pointer_moved &&
            !keyboard_used)
            state->active = items[i].id;
    }
    size_t stop = count;
    for (size_t i = begin; i < begin + length; ++i)
        if (!items[i].disabled && items[i].id == state->active)
            stop = i;
    if (stop == count)
        for (size_t i = begin; i < begin + length; ++i)
            if (!items[i].disabled) {
                stop = i;
                state->active = items[i].id;
                break;
            }
    for (size_t i = begin; i < begin + length; ++i) {
        cui_update_semantic(ui, items[i].id, *selected == items[i].id, (float)i);
        cui_set_semantic_details(ui, items[i].id,
                                 (cui_semantic_details){.selected = *selected == items[i].id});
        cui_set_tab_stop(ui, items[i].id, i == stop);
    }
    return activated;
}

bool cui_choice_private_list_events(cui_context *ui, const cui_choice_item *items, size_t count,
                                    cui_id *selected, cui_list_state *state,
                                    cui_list_options options, cui_id control,
                                    cui_selection_handler handler,
                                    cui_selection_event_result *observed) {
    return cui_choice_list_reduce(ui, items, count, selected, state, options, control, handler,
                                  observed, (cui_menu_command_handler){0}, NULL, NULL);
}
cui_list_event_result cui_base_list_events(cui_context *ui, cui_id control,
                                           const cui_choice_item *items, size_t count,
                                           cui_id *selected, cui_list_state *state,
                                           cui_list_options options,
                                           cui_selection_handler handler) {
    cui_list_event_result result = {0};
    if (!ui || !control || (!items && count) || !selected || !state)
        return result;
    result.events.valid = true;
    result.events.submitted = cui_frame_sequence(ui) != 0;
    result.legacy_activated = cui_choice_private_list_events(
        ui, items, count, selected, state, options, control, handler, &result.events);
    return result;
}
bool cui_choice_private_list_commands(cui_context *ui, const cui_choice_item *items, size_t count,
                                      cui_id *selected, cui_list_state *state,
                                      cui_list_options options, cui_id control,
                                      cui_menu_command_handler handler,
                                      cui_menu_command_result *observed) {
    return cui_choice_list_reduce(ui, items, count, selected, state, options, control,
                                  (cui_selection_handler){0}, NULL, handler, observed, NULL);
}

bool cui_base_list(cui_context *ui, const cui_choice_item *items, size_t count, cui_id *selected,
                   cui_list_state *state, cui_list_options options) {
    return cui_choice_private_list_events(ui, items, count, selected, state, options, 0,
                                          (cui_selection_handler){0}, NULL);
}

void cui_base_choice_popup_trigger(cui_context *ui, cui_id trigger, cui_str name, bool disabled,
                                   const cui_choice_item *items, size_t count, cui_id selected,
                                   cui_choice_popup *state) {
    if (!state || !state->overlay.id)
        return;
    cui_overlay_update(ui, &state->overlay);
    if (disabled && state->overlay.open)
        cui_overlay_close(ui, &state->overlay);
    bool activated =
        cui_interact(ui, trigger, CUI_ROLE_BUTTON, name, disabled, state->overlay.open, 0)
            .activated;
    bool arrow = false, reverse = false;
    size_t event_count = 0;
    const cui_event *events = cui_frame_events(ui, &event_count);
    for (size_t e = 0; e < event_count; e++)
        if (cui_event_target(ui, e) == trigger && events[e].type == CUI_EVENT_KEY_DOWN &&
            (events[e].key == CUI_KEY_DOWN || events[e].key == CUI_KEY_UP)) {
            arrow = true;
            reverse = events[e].key == CUI_KEY_UP;
        }
    if (!disabled && (activated || arrow)) {
        if (state->overlay.open && activated)
            cui_overlay_close(ui, &state->overlay);
        else if (!state->overlay.open) {
            cui_overlay_open(ui, &state->overlay, trigger);
            state->list.search_length = 0;
            state->list.search_time = 0;
            state->list.active = 0;
            if (items) {
                for (size_t i = 0; i < count; i++)
                    if (!items[i].disabled && items[i].id == selected)
                        state->list.active = selected;
                if (!state->list.active)
                    for (size_t n = 0; n < count; n++) {
                        size_t i = reverse ? count - 1 - n : n;
                        if (!items[i].disabled) {
                            state->list.active = items[i].id;
                            break;
                        }
                    }
            }
            cui_focus(ui, state->list.active);
        }
    }
    cui_update_semantic(ui, trigger, state->overlay.open, 0);
    cui_set_semantic_details(
        ui, trigger,
        (cui_semantic_details){.expanded = state->overlay.open, .controls = state->overlay.id});
}

static bool cui_choice_contains(cui_str label, cui_str query) {
    if (!query.length)
        return true;
    if (!label.data || !query.data || query.length > label.length)
        return false;
    for (size_t start = 0; start <= label.length - query.length; start++) {
        bool match = true;
        for (size_t j = 0; j < query.length; j++) {
            unsigned char a = (unsigned char)label.data[start + j],
                          b = (unsigned char)query.data[j];
            if (a >= 'A' && a <= 'Z')
                a = (unsigned char)(a + 'a' - 'A');
            if (b >= 'A' && b <= 'Z')
                b = (unsigned char)(b + 'a' - 'A');
            if (a != b) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}
static cui_combobox_result cui_choice_reduce_combobox(
    cui_context *ui, cui_id input, cui_str name, cui_edit *query, const cui_choice_item *items,
    size_t count, cui_choice_item *filtered, size_t capacity, cui_id *selected,
    cui_choice_popup *state, bool disabled, bool keep_open, cui_choice_activation_fn commit_fn,
    void *commit_user, cui_selection_handler handler, cui_selection_event_result *observed) {
    cui_combobox_result result = {.error = CUI_EDIT_OK};
    if (!query || !selected || !state || !state->overlay.id || (!items && count) ||
        (!filtered && capacity) || capacity < count || cui_edit_get_state(query).multiline) {
        result.error = capacity < count ? CUI_EDIT_CAPACITY : CUI_EDIT_INVALID_ARGUMENT;
        return result;
    }
    if (observed) {
        observed->valid = true;
        observed->submitted = cui_frame_is_submitting(ui) && cui_frame_sequence(ui) != 0;
    }
    state->overlay.modal = false;
    cui_overlay_update(ui, &state->overlay);
    if (disabled)
        cui_overlay_close(ui, &state->overlay);
    cui_id active_before_edit = state->list.active;
    cui_input_hit_geometry hit = {ui, state->query_viewport, state->query_font_size,
                                  state->query_scroll_x};
    cui_input_result input_result = cui_base_input_ex(
        ui, input, name, query, &state->query_behavior,
        (cui_base_input_options){.disabled = disabled,
                                 .delegate_home_end = state->overlay.open,
                                 .delegate_up_down = true,
                                 .hit_test =
                                     state->query_font_size > 0 ? cui_base_input_hit_test : NULL,
                                 .hit_test_user = &hit});
    result.query_changed = input_result.changed;
    result.error = input_result.error;
    cui_str text = cui_edit_get_text(query);
    size_t matched = 0;
    for (size_t i = 0; i < count; i++)
        if (cui_choice_contains(items[i].label, text))
            filtered[matched++] = items[i];
    result.matching_count = matched;
    size_t event_count = 0;
    const cui_event *events = cui_frame_events(ui, &event_count);
    bool dismiss = false;
    if (!disabled)
        for (size_t e = 0; e < event_count; e++) {
            if (cui_event_consumed(ui, e))
                continue;
            if (cui_event_target(ui, e) != input)
                continue;
            if (events[e].type == CUI_EVENT_KEY_DOWN && events[e].key == CUI_KEY_ESCAPE)
                dismiss = true;
            if ((events[e].type == CUI_EVENT_POINTER_DOWN &&
                 events[e].pointer_button == CUI_POINTER_PRIMARY) ||
                events[e].type == CUI_EVENT_TEXT ||
                (events[e].type == CUI_EVENT_KEY_DOWN &&
                 (events[e].key == CUI_KEY_DOWN || events[e].key == CUI_KEY_UP))) {
                if (!state->overlay.open)
                    cui_overlay_open(ui, &state->overlay, input);
            }
        }
    if (dismiss)
        cui_overlay_close(ui, &state->overlay);
    cui_set_semantic_details(ui, input,
                             (cui_semantic_details){.expanded = state->overlay.open,
                                                    .controls = state->overlay.id,
                                                    .description = text});
    if (!state->overlay.open)
        return result;
    size_t active = matched;
    for (size_t i = 0; i < matched; i++)
        if (filtered[i].id == state->list.active && !filtered[i].disabled)
            active = i;
    if (active == matched)
        for (size_t i = 0; i < matched; i++)
            if (!filtered[i].disabled) {
                active = i;
                break;
            }
    if (active != matched)
        state->list.active = filtered[active].id;
    bool commit = false, edit_after_commit = false;
    for (size_t e = 0; e < event_count && !disabled; e++) {
        if (cui_event_consumed(ui, e))
            continue;
        if (cui_choice_scroll_event(ui, filtered, matched, &state->list, state->visible_rows,
                                    &state->list.first_visible, state->overlay.bounds,
                                    state->overlay.id,
                                    state->overlay.id ^ UINT64_C(0xd1bc905ee4216f11),
                                    cui_event_target(ui, e), events[e])) {
            result.scrolled = true;
            active = matched;
            for (size_t i = 0; i < matched; i++)
                if (filtered[i].id == state->list.active && !filtered[i].disabled)
                    active = i;
        }
        if (cui_event_target(ui, e) != input || events[e].type != CUI_EVENT_KEY_DOWN)
            continue;
        size_t next = active == matched
                          ? matched
                          : cui_choice_navigate(filtered, matched, active, events[e].key, true);
        if (next != matched) {
            active = next;
            state->list.active = filtered[active].id;
            cui_consume_event(ui, e);
        }
        if (events[e].key == CUI_KEY_ENTER && !events[e].repeat) {
            bool later_text = false;
            for (size_t later = e + 1; later < event_count; later++)
                if (cui_event_target(ui, later) == input && events[later].type == CUI_EVENT_TEXT) {
                    later_text = true;
                    break;
                }
            cui_id commit_id = active != matched ? filtered[active].id : 0;
            if (later_text) {
                commit_id = 0;
                for (size_t i = 0; i < count; i++)
                    if (items[i].id == active_before_edit && !items[i].disabled) {
                        commit_id = items[i].id;
                        break;
                    }
            }
            if (!commit_id)
                continue;
            cui_component_event_metadata metadata;
            memset(&metadata, 0, sizeof(metadata));
            bool notify = observed && handler.on_change && cui_frame_sequence(ui) != 0;
            if (notify) {
                /* Enter confirmation is a direct key path, not Core activation. */
                (void)cui_component_event_init(&metadata, CUI_COMPONENT_EVENT_CHANGE, input,
                                               CUI_COMPONENT_EVENT_SOURCE_KEYBOARD);
                (void)cui_component_event_set_frame_index(&metadata, cui_frame_sequence(ui), e);
            }
            cui_id old = *selected;
            *selected = commit_id;
            state->list.active = *selected;
            if (commit_fn)
                commit_fn(*selected, commit_user);
            cui_id value = *selected;
            result.changed |= keep_open || old != value;
            if (notify && old != value) {
                cui_selection_change change = {metadata, old, value, commit_id,
                                               CUI_SELECTION_REASON_ACTIVATION};
                if (observed->change_count == SIZE_MAX)
                    observed->count_overflow = true;
                else
                    observed->change_count++;
                handler.on_change(handler.user, &change);
            }
            commit = true;
            edit_after_commit |= later_text;
            if (!keep_open)
                break;
        }
    }
    if (edit_after_commit && active != matched)
        state->list.active = filtered[active].id;
    if (commit && !keep_open && !edit_after_commit) {
        cui_overlay_close(ui, &state->overlay);
        cui_set_semantic_details(
            ui, input, (cui_semantic_details){.controls = state->overlay.id, .description = text});
    }
    return result;
}

typedef struct {
    const cui_choice_item *items;
    size_t count;
    bool *selected;
    cui_choice_activation_fn external;
    cui_choice_confirm_fn confirm;
    cui_choice_cancel_fn cancel;
    void *external_user;
} cui_choice_multi_reducer;
static void cui_choice_multi_activate(cui_id item, void *user) {
    cui_choice_multi_reducer *reducer = user;
    for (size_t i = 0; i < reducer->count; i++)
        if (reducer->items[i].id == item) {
            reducer->selected[i] = !reducer->selected[i];
            break;
        }
    if (reducer->external)
        reducer->external(item, reducer->external_user);
}
static void cui_choice_multi_confirm(cui_id item, bool secondary, void *user) {
    cui_choice_multi_reducer *reducer = user;
    reducer->confirm(item, secondary, reducer->external_user);
}
static void cui_choice_multi_cancel(void *user) {
    cui_choice_multi_reducer *reducer = user;
    reducer->cancel(reducer->external_user);
}
static bool cui_base_list_multi_common(cui_context *ui, const cui_choice_item *items, size_t count,
                                       bool *selected, cui_list_state *state,
                                       cui_list_options options, cui_id control,
                                       const cui_choice_item_handler *handler,
                                       cui_choice_items_event_result *result) {
    if (!items || !state || !selected || !count)
        return false;
    cui_choice_multi_reducer reducer = {items,
                                        count,
                                        selected,
                                        options.on_activate,
                                        options.on_confirm,
                                        options.on_cancel,
                                        options.activation_user};
    options.on_activate = cui_choice_multi_activate;
    options.on_confirm = reducer.confirm ? cui_choice_multi_confirm : NULL;
    options.on_cancel = reducer.cancel ? cui_choice_multi_cancel : NULL;
    options.activation_user = &reducer;
    cui_id action = 0;
    bool activated;
    if (handler) {
        result->valid = true;
        result->submitted = cui_frame_sequence(ui) != 0;
        cui_choice_multi_observer observer = {selected, *handler, result};
        activated = cui_choice_list_reduce(ui, items, count, &action, state, options, control,
                                           (cui_selection_handler){0}, NULL,
                                           (cui_menu_command_handler){0}, NULL, &observer);
    } else
        activated = cui_base_list(ui, items, count, &action, state, options);
    for (size_t i = 0; i < count; i++) {
        cui_update_semantic(ui, items[i].id, selected[i], (float)i);
        cui_set_semantic_details(ui, items[i].id, (cui_semantic_details){.selected = selected[i]});
    }
    return activated;
}

bool cui_base_list_multi(cui_context *ui, const cui_choice_item *items, size_t count,
                         bool *selected, cui_list_state *state, cui_list_options options) {
    return cui_base_list_multi_common(ui, items, count, selected, state, options, 0, NULL, NULL);
}
cui_choice_items_event_result cui_base_list_multi_events(cui_context *ui, cui_id control,
                                                         const cui_choice_item *items, size_t count,
                                                         bool *selected, cui_list_state *state,
                                                         cui_list_options options,
                                                         cui_choice_item_handler handler) {
    cui_choice_items_event_result result = {0};
    if (!ui || !control)
        return result;
    result.legacy_result = cui_base_list_multi_common(ui, items, count, selected, state, options,
                                                      control, &handler, &result);
    return result;
}

bool cui_base_selection_remove(cui_context *ui, cui_id chip, cui_str name, bool *selected,
                               bool disabled) {
    if (!selected)
        return false;
    cui_interact(ui, chip, CUI_ROLE_BUTTON, name, disabled, false, 0);
    cui_set_semantic_details(
        ui, chip, (cui_semantic_details){.description = cui_string("Remove selected option")});
    size_t event_count = 0;
    cui_frame_events(ui, &event_count);
    if (!disabled && *selected)
        for (size_t e = 0; e < event_count; e++)
            if (cui_event_activation(ui, e) == chip) {
                *selected = false;
                return true;
            }
    return false;
}

cui_combobox_result cui_base_combobox(cui_context *ui, cui_id input, cui_str name, cui_edit *query,
                                      const cui_choice_item *items, size_t count,
                                      cui_choice_item *filtered, size_t capacity, cui_id *selected,
                                      cui_choice_popup *state, bool disabled) {
    return cui_choice_reduce_combobox(ui, input, name, query, items, count, filtered, capacity,
                                      selected, state, disabled, false, NULL, NULL,
                                      (cui_selection_handler){0}, NULL);
}
cui_combobox_event_result cui_base_combobox_events(cui_context *ui, cui_id input, cui_str name,
                                                   cui_edit *query, const cui_choice_item *items,
                                                   size_t count, cui_choice_item *filtered,
                                                   size_t capacity, cui_id *selected,
                                                   cui_choice_popup *state, bool disabled,
                                                   cui_selection_handler handler) {
    cui_combobox_event_result result;
    memset(&result, 0, sizeof(result));
    if (!ui || !input) {
        result.legacy.error = CUI_EDIT_INVALID_ARGUMENT;
        return result;
    }
    result.legacy = cui_choice_reduce_combobox(ui, input, name, query, items, count, filtered,
                                               capacity, selected, state, disabled, false, NULL,
                                               NULL, handler, &result.events);
    return result;
}

cui_combobox_result cui_base_combobox_multi(cui_context *ui, cui_id input, cui_str name,
                                            cui_edit *query, const cui_choice_item *items,
                                            size_t count, cui_choice_item *filtered,
                                            bool *filtered_selected, size_t capacity,
                                            bool *selected, cui_choice_popup *state,
                                            bool disabled) {
    if (!selected || (!filtered_selected && capacity))
        return (cui_combobox_result){.error = CUI_EDIT_INVALID_ARGUMENT};
    cui_choice_multi_reducer reducer = {.items = items, .count = count, .selected = selected};
    cui_id action = 0;
    cui_combobox_result result = cui_choice_reduce_combobox(
        ui, input, name, query, items, count, filtered, capacity, &action, state, disabled, true,
        cui_choice_multi_activate, &reducer, (cui_selection_handler){0}, NULL);
    if (result.error != CUI_EDIT_OK)
        return result;
    size_t source = 0;
    for (size_t i = 0; i < result.matching_count; i++) {
        while (source < count && items[source].id != filtered[i].id)
            source++;
        filtered_selected[i] = source < count ? selected[source++] : false;
    }
    return result;
}

bool cui_menu_entry_is_disabled(const cui_menu_entry *entry) {
    if (!entry)
        return true;
    return entry->disabled || entry->kind == CUI_MENU_SEPARATOR || entry->kind == CUI_MENU_LABEL ||
           (entry->kind == CUI_MENU_CHECK && !entry->checked) ||
           (entry->kind == CUI_MENU_RADIO && !entry->radio_value) ||
           (entry->kind == CUI_MENU_SUBMENU && (!entry->child_count || !entry->children));
}
static size_t cui_menu_find(const cui_menu_entry *items, size_t count, cui_id id) {
    for (size_t i = 0; i < count; i++)
        if (items[i].id == id)
            return i;
    return count;
}
static size_t cui_menu_step(const cui_menu_entry *items, size_t count, size_t from, cui_key key) {
    if (!count)
        return count;
    int direction = 0;
    if (key == CUI_KEY_HOME) {
        from = count - 1;
        direction = 1;
    } else if (key == CUI_KEY_END) {
        from = 0;
        direction = -1;
    } else if (key == CUI_KEY_DOWN)
        direction = 1;
    else if (key == CUI_KEY_UP)
        direction = -1;
    if (!direction)
        return count;
    for (size_t n = 0; n < count; n++) {
        from = direction > 0 ? (from + 1) % count : from ? from - 1 : count - 1;
        if (!cui_menu_entry_is_disabled(&items[from]))
            return from;
    }
    return count;
}
void cui_menu_tree_init(cui_menu_tree_state *state, cui_id popup) {
    if (!state)
        return;
    *state = (cui_menu_tree_state){0};
    cui_overlay_init(&state->overlay, popup, true);
}
void cui_base_menu_entry(cui_context *ui, const cui_menu_entry *entry, bool disabled, bool expanded,
                         cui_id parent) {
    if (!entry)
        return;
    if (entry->kind == CUI_MENU_SEPARATOR)
        return;
    bool checked = entry->kind == CUI_MENU_CHECK && entry->checked ? *entry->checked
                   : entry->kind == CUI_MENU_RADIO && entry->radio_value
                       ? *entry->radio_value == entry->radio_option
                       : false;
    cui_role role = entry->kind == CUI_MENU_LABEL   ? CUI_ROLE_LABEL
                    : entry->kind == CUI_MENU_CHECK ? CUI_ROLE_MENUITEMCHECKBOX
                    : entry->kind == CUI_MENU_RADIO ? CUI_ROLE_MENUITEMRADIO
                                                    : CUI_ROLE_MENUITEM;
    cui_interact(ui, entry->id, role, entry->label, disabled || cui_menu_entry_is_disabled(entry),
                 checked,
                 entry->kind == CUI_MENU_RADIO ? (float)entry->radio_option
                 : checked                     ? 1
                                               : 0);
    cui_set_semantic_details(
        ui, entry->id,
        (cui_semantic_details){.parent = parent,
                               .expanded = expanded,
                               .controls = entry->kind == CUI_MENU_SUBMENU
                                               ? entry->id ^ UINT64_C(0x4b64d23e91a738c1)
                                               : 0,
                               .description = entry->shortcut});
}
static float cui_menu_retained_row_height(const cui_menu_tree_state *state,
                                          const cui_menu_entry *entry) {
    if (entry->kind == CUI_MENU_SEPARATOR && state->separator_height > 0)
        return state->separator_height;
    if (entry->kind == CUI_MENU_LABEL && state->label_height > 0)
        return state->label_height;
    return state->row_height - state->row_gap;
}
static size_t cui_menu_retained_fit(const cui_menu_tree_state *state, const cui_menu_entry *entries,
                                    size_t count, size_t begin) {
    size_t rows = 0;
    float height = 0;
    while (begin + rows < count && rows < 4096) {
        float next = cui_menu_retained_row_height(state, &entries[begin + rows]) +
                     (rows ? state->row_gap : 0);
        if (rows && height + next > state->available_height)
            break;
        height += next;
        rows++;
    }
    return rows;
}
static bool cui_menu_scroll_event(cui_context *ui, const cui_menu_entry *entries, size_t count,
                                  cui_menu_tree_state *state, size_t level, cui_id target,
                                  cui_event event) {
    cui_id scope =
        level ? state->path[level - 1] ^ UINT64_C(0x4b64d23e91a738c1) : state->overlay.id;
    cui_rect bounds = state->panels[level];
    size_t rows = state->visible_rows[level];
    if (event.type != CUI_EVENT_SCROLL || target != scope || !isfinite(event.dy) || !rows ||
        count <= rows || event.x < bounds.x || event.x >= bounds.x + bounds.w ||
        event.y < bounds.y || event.y >= bounds.y + bounds.h ||
        !cui_pointer_scope_allowed(ui, state->overlay.id))
        return false;
    cui_list_state *cursor = &state->levels[level];
    size_t maximum = count - rows;
    if (state->available_height > 0 && state->row_gap > 0) {
        maximum = count - 1;
        float height = cui_menu_retained_row_height(state, &entries[maximum]);
        while (maximum && count - maximum < 4096) {
            float next =
                cui_menu_retained_row_height(state, &entries[maximum - 1]) + state->row_gap;
            if (height + next > state->available_height)
                break;
            height += next;
            maximum--;
        }
    }
    if (cursor->first_visible > maximum)
        cursor->first_visible = maximum;
    float amount = cursor->scroll_remainder -
                   event.dy / (event.scroll_precise ? fmaxf(1, state->row_height) : 1);
    if (!isfinite(amount))
        amount = event.dy > 0 ? -(float)count : (float)count;
    float whole = truncf(amount);
    cursor->scroll_remainder = amount - whole;
    size_t before = cursor->first_visible;
    if (whole < 0)
        cursor->first_visible = -whole >= (float)before ? 0 : before - (size_t)-whole;
    else {
        size_t remaining = maximum - before;
        cursor->first_visible += whole >= (float)remaining ? remaining : (size_t)whole;
    }
    if (before == cursor->first_visible)
        return false;
    if (state->available_height > 0 && state->row_gap > 0)
        rows = cui_menu_retained_fit(state, entries, count, cursor->first_visible);
    bool focused = false, active_visible = false;
    for (size_t i = 0; i < count; i++) {
        focused |= cui_focused(ui) == entries[i].id;
        active_visible |= i >= cursor->first_visible && i < cursor->first_visible + rows &&
                          entries[i].id == cursor->active &&
                          !cui_menu_entry_is_disabled(&entries[i]);
    }
    if (!active_visible) {
        cursor->active = 0;
        for (size_t i = cursor->first_visible; i < cursor->first_visible + rows; i++)
            if (!cui_menu_entry_is_disabled(&entries[i])) {
                cursor->active = entries[i].id;
                break;
            }
        if (focused)
            cui_focus(ui, cursor->active);
    }
    if (state->depth > level + 1) {
        state->depth = level + 1;
        cui_focus(ui, cursor->active);
    }
    cui_invalidate(ui);
    return true;
}

bool cui_choice_dropdown_metadata_prepare(cui_dropdown_button_metadata *out,
                                          const cui_dropdown_button_metadata *input, bool action,
                                          bool trigger) {
    *out = input ? *input : (cui_dropdown_button_metadata){0};
    if (!action) {
        out->action = (cui_dropdown_button_part_metadata){0};
        out->toggle_semantics = out->toggled = false;
    }
    if (!trigger)
        out->trigger = (cui_dropdown_button_part_metadata){0};
    const cui_dropdown_button_part_metadata parts[] = {out->action, out->trigger};
    for (size_t i = 0; i < 2; i++)
        if (parts[i].custom_name && !cui_edit_valid_utf8(parts[i].name))
            return false;
    return true;
}
bool cui_choice_dropdown_metadata_active(const cui_dropdown_button_metadata *m) {
    return m && (m->action.custom_name || m->action.custom_tab_index || m->action.skip_tab ||
                 m->trigger.custom_name || m->trigger.custom_tab_index || m->trigger.skip_tab ||
                 m->toggle_semantics);
}
static void cui_dropdown_part_metadata(cui_context *ui, cui_id id,
                                       const cui_dropdown_button_part_metadata *m) {
    if (!m)
        return;
    if (m->custom_tab_index)
        cui_set_tab_index(ui, id, m->tab_index);
    cui_set_tab_stop(ui, id, !m->skip_tab);
}
static bool cui_choice_reduce_menu_tree(cui_context *ui, cui_id trigger, cui_str name,
                                        const cui_menu_entry *items, size_t count, cui_id *action,
                                        cui_menu_tree_state *state, bool disabled, float dt,
                                        bool keep_open_checks, cui_menu_command_handler handler,
                                        cui_menu_command_result *observed,
                                        const cui_dropdown_button_part_metadata *metadata) {
    if (!state || !state->overlay.id || !action || (!items && count))
        return false;
    if (observed) {
        observed->valid = true;
        observed->submitted = cui_frame_is_submitting(ui) && cui_frame_sequence(ui) != 0;
    }
    if (!state->drag_scroll)
        state->scroll_pending = state->scroll_dragging = false;
    bool was_open = state->overlay.open;
    if (state->contextual) {
        cui_overlay_update(ui, &state->overlay);
        if (disabled)
            cui_overlay_close(ui, &state->overlay);
    } else {
        cui_choice_popup temporary = {.overlay = state->overlay, .list = state->levels[0]};
        cui_base_choice_popup_trigger(ui, trigger, name, disabled, NULL, 0, 0, &temporary);
        state->overlay = temporary.overlay;
    }
    cui_dropdown_part_metadata(ui, trigger, metadata);
    if (!state->overlay.open) {
        state->depth = 0;
        state->scroll_pending = state->scroll_dragging = false;
        return false;
    }
    if (!was_open || !state->depth) {
        state->depth = 1;
        state->depth_limit = false;
        size_t queued = 0;
        bool reverse = false;
        const cui_event *opening_events = cui_frame_events(ui, &queued);
        for (size_t e = 0; e < queued; e++)
            if (cui_event_target(ui, e) == trigger &&
                opening_events[e].type == CUI_EVENT_KEY_DOWN && opening_events[e].key == CUI_KEY_UP)
                reverse = true;
        size_t first = cui_menu_step(items, count,
                                     reverse ? 0
                                     : count ? count - 1
                                             : 0,
                                     reverse ? CUI_KEY_UP : CUI_KEY_DOWN);
        state->levels[0].active = first < count ? items[first].id : 0;
        state->levels[0].search_length = 0;
        cui_focus(ui, state->levels[0].active);
    }
    if (state->depth > CUI_MENU_DEPTH_MAX)
        state->depth = CUI_MENU_DEPTH_MAX;
    const cui_menu_entry *menus[CUI_MENU_DEPTH_MAX] = {items};
    size_t counts[CUI_MENU_DEPTH_MAX] = {count};
    for (size_t level = 1; level < state->depth; level++) {
        size_t parent = cui_menu_find(menus[level - 1], counts[level - 1], state->path[level - 1]);
        if (parent == counts[level - 1] || menus[level - 1][parent].kind != CUI_MENU_SUBMENU ||
            cui_menu_entry_is_disabled(&menus[level - 1][parent])) {
            state->depth = level;
            break;
        }
        state->levels[level - 1].active = menus[level - 1][parent].id;
        menus[level] = menus[level - 1][parent].children;
        counts[level] = menus[level - 1][parent].child_count;
    }
    for (size_t level = 0; level < state->depth; level++) {
        cui_list_state *cursor = &state->levels[level];
        if (cursor->search_length >= sizeof(cursor->search))
            cursor->search_length = 0;
        if (dt > 0 && dt < 60)
            cursor->search_time += dt;
        if (cursor->search_time > .75f)
            cursor->search_length = 0;
        size_t current = cui_menu_find(menus[level], counts[level], cursor->active);
        if (current == counts[level] || cui_menu_entry_is_disabled(&menus[level][current])) {
            size_t first = cui_menu_step(menus[level], counts[level],
                                         counts[level] ? counts[level] - 1 : 0, CUI_KEY_DOWN);
            cursor->active = first < counts[level] ? menus[level][first].id : 0;
        }
    }
    size_t event_count = 0, logical_level = state->depth - 1;
    bool redirected = false, activated_any = false, pointer_window_changed = false;
    const cui_event *events = cui_frame_events(ui, &event_count);
    for (size_t e = 0; e < event_count; e++) {
        if (cui_event_consumed(ui, e))
            continue;
        cui_id target = cui_event_target(ui, e);
        cui_event event = events[e];
        if (!state->drag_scroll || event.type == CUI_EVENT_CANCEL ||
            (event.type == CUI_EVENT_KEY_DOWN && event.key == CUI_KEY_ESCAPE))
            state->scroll_pending = state->scroll_dragging = false;
        if (state->drag_scroll && event.type == CUI_EVENT_POINTER_DOWN &&
            event.pointer_button == CUI_POINTER_PRIMARY && isfinite(event.y) &&
            cui_pointer_scope_allowed(ui, state->overlay.id)) {
            for (size_t reverse = state->depth; reverse > 0; reverse--) {
                size_t level = reverse - 1;
                cui_rect bounds = state->panels[level];
                cui_id scope = level ? state->path[level - 1] ^ UINT64_C(0x4b64d23e91a738c1)
                                     : state->overlay.id;
                if (event.x >= bounds.x && event.x < bounds.x + bounds.w && event.y >= bounds.y &&
                    event.y < bounds.y + bounds.h &&
                    (target == scope ||
                     cui_menu_find(menus[level], counts[level], target) < counts[level]) &&
                    state->visible_rows[level] && counts[level] > state->visible_rows[level]) {
                    state->levels[level].scroll_remainder = 0;
                    state->scroll_level = level;
                    state->scroll_pending = true;
                    state->scroll_dragging = false;
                    state->scroll_start_y = state->scroll_last_y = event.y;
                    break;
                }
            }
        }
        if (state->scroll_pending && event.type == CUI_EVENT_POINTER_MOVE && isfinite(event.y)) {
            if (fabsf(event.y - state->scroll_start_y) > 4)
                state->scroll_dragging = true;
            if (state->scroll_dragging && state->scroll_level < state->depth &&
                state->row_height > 0) {
                size_t level = state->scroll_level;
                cui_rect bounds = state->panels[level];
                cui_id scope = level ? state->path[level - 1] ^ UINT64_C(0x4b64d23e91a738c1)
                                     : state->overlay.id;
                cui_menu_scroll_event(
                    ui, menus[level], counts[level], state, level, scope,
                    (cui_event){.type = CUI_EVENT_SCROLL,
                                .x = bounds.x + 1,
                                .y = bounds.y + 1,
                                .dy = (event.y - state->scroll_last_y) / state->row_height});
                state->scroll_last_y = event.y;
                logical_level = level;
                redirected = true;
                pointer_window_changed = true;
                continue;
            }
        }
        if (event.type == CUI_EVENT_POINTER_UP && event.pointer_button == CUI_POINTER_PRIMARY) {
            pointer_window_changed |= state->scroll_dragging;
            state->scroll_pending = state->scroll_dragging = false;
        }
        if (events[e].type == CUI_EVENT_SCROLL) {
            for (size_t level = 0; level < state->depth; level++)
                if (cui_menu_scroll_event(ui, menus[level], counts[level], state, level, target,
                                          events[e])) {
                    pointer_window_changed = true;
                    logical_level = level;
                    redirected = true;
                    break;
                }
            continue;
        }
        if (events[e].type == CUI_EVENT_KEY_DOWN && events[e].key == CUI_KEY_ESCAPE) {
            size_t escape_level = state->depth;
            if (target == state->overlay.id)
                escape_level = 0;
            for (size_t level = 1; level < state->depth; level++)
                if (target == (state->path[level - 1] ^ UINT64_C(0x4b64d23e91a738c1)))
                    escape_level = level;
            if (escape_level < state->depth) {
                if (!escape_level) {
                    cui_overlay_close(ui, &state->overlay);
                    state->depth = 0;
                    break;
                }
                state->depth = escape_level;
                logical_level = escape_level - 1;
                cui_focus(ui, state->path[logical_level]);
                redirected = true;
                continue;
            }
        }
        if (events[e].type == CUI_EVENT_POINTER_MOVE && state->row_height > 0) {
            for (size_t reverse = state->depth; reverse > 0; reverse--) {
                size_t level = reverse - 1;
                cui_rect panel = state->panels[level];
                if (events[e].x >= panel.x && events[e].x < panel.x + panel.w &&
                    events[e].y >= panel.y + state->padding &&
                    events[e].y < panel.y + panel.h - state->padding) {
                    float offset = events[e].y - panel.y - state->padding;
                    size_t row = state->levels[level].first_visible;
                    while (row < counts[level]) {
                        float height = cui_menu_retained_row_height(state, &menus[level][row]);
                        if (offset < height) {
                            if (!cui_menu_entry_is_disabled(&menus[level][row]))
                                target = menus[level][row].id;
                            break;
                        }
                        if (offset < height + state->row_gap)
                            break;
                        offset -= height + state->row_gap;
                        row++;
                    }
                    break;
                }
            }
        }
        size_t target_level = state->depth, target_index = 0;
        for (size_t level = 0; level < state->depth; level++) {
            size_t index = cui_menu_find(menus[level], counts[level], target);
            if (index < counts[level]) {
                target_level = level;
                target_index = index;
                break;
            }
        }
        if (target_level == state->depth)
            continue;
        bool keyboard = events[e].type == CUI_EVENT_KEY_DOWN ||
                        events[e].type == CUI_EVENT_KEY_UP || events[e].type == CUI_EVENT_TEXT;
        size_t level = keyboard && redirected ? logical_level : target_level;
        if (level >= state->depth)
            continue;
        const cui_menu_entry *entries = menus[level];
        size_t n = counts[level];
        size_t active = cui_menu_find(entries, n, state->levels[level].active);
        if (!keyboard)
            active = target_index;
        if (active == n || cui_menu_entry_is_disabled(&entries[active]))
            continue;
        const cui_menu_entry *entry = &entries[active];
        bool same_child = entry->kind == CUI_MENU_SUBMENU && level + 1 < state->depth &&
                          state->path[level] == entry->id;
        if (events[e].type == CUI_EVENT_POINTER_MOVE) {
            state->levels[level].active = entry->id;
            cui_focus(ui, entry->id);
            if (!same_child)
                state->depth = level + 1;
            logical_level = level;
            redirected = true;
        }
        bool activation = cui_event_activation(ui, e) != 0 &&
                          !(pointer_window_changed && events[e].type == CUI_EVENT_POINTER_UP);
        if (events[e].type == CUI_EVENT_KEY_UP && entry->id != cui_event_activation(ui, e))
            activation = false;
        bool open_child =
            entry->kind == CUI_MENU_SUBMENU &&
            (activation || events[e].type == CUI_EVENT_POINTER_MOVE ||
             (events[e].type == CUI_EVENT_KEY_DOWN && events[e].key == CUI_KEY_RIGHT));
        if (open_child) {
            if (events[e].type == CUI_EVENT_KEY_DOWN && events[e].key == CUI_KEY_RIGHT &&
                (same_child || level + 1 < CUI_MENU_DEPTH_MAX))
                cui_consume_event(ui, e);
            if (same_child) {
                cui_focus(ui, state->levels[level + 1].active);
                logical_level = level + 1;
                redirected = true;
                continue;
            }
            if (level + 1 == CUI_MENU_DEPTH_MAX) {
                state->depth_limit = true;
                continue;
            }
            state->path[level] = entry->id;
            state->depth = level + 2;
            menus[level + 1] = entry->children;
            counts[level + 1] = entry->child_count;
            size_t first =
                cui_menu_step(entry->children, entry->child_count,
                              entry->child_count ? entry->child_count - 1 : 0, CUI_KEY_DOWN);
            state->levels[level + 1] = (cui_list_state){
                .active = first < entry->child_count ? entry->children[first].id : 0};
            cui_focus(ui, state->levels[level + 1].active);
            logical_level = level + 1;
            redirected = true;
            continue;
        }
        if (activation && entry->kind != CUI_MENU_SUBMENU) {
            cui_menu_command command;
            memset(&command, 0, sizeof(command));
            command.command = entry->id;
            bool notify = observed && handler.on_activate && cui_frame_sequence(ui) != 0;
            if (notify) {
                cui_activation_snapshot snapshot;
                if (!cui_event_activation_snapshot(ui, e, &snapshot) ||
                    !cui_component_event_from_activation(&command.event,
                                                         CUI_COMPONENT_EVENT_ACTIVATE, &snapshot)) {
                    cui_component_event_source source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN;
                    if (event.type == CUI_EVENT_POINTER_UP)
                        source = CUI_COMPONENT_EVENT_SOURCE_POINTER;
                    else if (event.type == CUI_EVENT_KEY_DOWN || event.type == CUI_EVENT_KEY_UP)
                        source = CUI_COMPONENT_EVENT_SOURCE_KEYBOARD;
                    else if (event.type == CUI_EVENT_ACCESSIBILITY_ACTION)
                        source = CUI_COMPONENT_EVENT_SOURCE_ACCESSIBILITY;
                    (void)cui_component_event_init(&command.event, CUI_COMPONENT_EVENT_ACTIVATE,
                                                   trigger, source);
                    (void)cui_component_event_set_frame_index(&command.event,
                                                              cui_frame_sequence(ui), e);
                }
                command.event.target = trigger;
            }
            state->levels[level].active = entry->id;
            if (!keyboard)
                state->depth = level + 1;
            if (entry->kind == CUI_MENU_CHECK)
                *entry->checked = !*entry->checked;
            if (entry->kind == CUI_MENU_RADIO)
                *entry->radio_value = entry->radio_option;
            *action = entry->id;
            if (!keep_open_checks ||
                (entry->kind != CUI_MENU_CHECK && entry->kind != CUI_MENU_RADIO)) {
                cui_overlay_close(ui, &state->overlay);
                state->depth = 0;
            }
            activated_any = true;
            bool exit_after_command = !state->overlay.open;
            if (notify) {
                if (observed->command_count == SIZE_MAX)
                    observed->count_overflow = true;
                else
                    observed->command_count++;
                handler.on_activate(handler.user, &command);
            }
            if (exit_after_command)
                return true;
            continue;
        }
        if (events[e].type == CUI_EVENT_KEY_DOWN) {
            if (events[e].key == CUI_KEY_LEFT && level) {
                cui_consume_event(ui, e);
                state->depth = level;
                logical_level = level - 1;
                redirected = true;
                cui_focus(ui, state->path[level - 1]);
                continue;
            }
            size_t next = cui_menu_step(entries, n, active, events[e].key);
            if (next < n) {
                state->levels[level].active = entries[next].id;
                state->depth = level + 1;
                logical_level = level;
                redirected = true;
                cui_focus(ui, entries[next].id);
                state->levels[level].search_length = 0;
            }
        } else if (events[e].type == CUI_EVENT_TEXT) {
            cui_list_state *cursor = &state->levels[level];
            size_t length = 0;
            while (length < sizeof(events[e].text) && events[e].text[length])
                length++;
            if (!length || length >= sizeof(events[e].text))
                continue;
            if (length > sizeof(cursor->search) - 1 - cursor->search_length)
                cursor->search_length = 0;
            bool cycle = length == 1 && cursor->search_length == 1 &&
                         cui_choice_lower((unsigned char)events[e].text[0]) ==
                             cui_choice_lower((unsigned char)cursor->search[0]);
            if (!cycle)
                for (size_t j = 0; j < length; j++)
                    cursor->search[cursor->search_length++] = events[e].text[j];
            cursor->search_time = 0;
            for (size_t step = 0; step < n; step++) {
                size_t i = (active + 1 + step) % n;
                if (!cui_menu_entry_is_disabled(&entries[i]) &&
                    cui_choice_prefix(entries[i].label, cursor->search, cursor->search_length)) {
                    cursor->active = entries[i].id;
                    cui_focus(ui, cursor->active);
                    state->depth = level + 1;
                    logical_level = level;
                    redirected = true;
                    break;
                }
            }
        }
    }
    return activated_any;
}

bool cui_base_menu_tree(cui_context *ui, cui_id trigger, cui_str name, const cui_menu_entry *items,
                        size_t count, cui_id *action, cui_menu_tree_state *state, bool disabled,
                        float dt, bool keep_open_checks) {
    return cui_choice_reduce_menu_tree(ui, trigger, name, items, count, action, state, disabled, dt,
                                       keep_open_checks, (cui_menu_command_handler){0}, NULL, NULL);
}
cui_menu_command_result cui_base_menu_tree_events(cui_context *ui, cui_id trigger, cui_str name,
                                                  const cui_menu_entry *items, size_t count,
                                                  cui_id *action, cui_menu_tree_state *state,
                                                  bool disabled, float dt, bool keep_open_checks,
                                                  cui_menu_command_handler handler) {
    cui_menu_command_result result;
    memset(&result, 0, sizeof(result));
    if (!ui || !trigger)
        return result;
    result.legacy_activated =
        cui_choice_reduce_menu_tree(ui, trigger, name, items, count, action, state, disabled, dt,
                                    keep_open_checks, handler, &result, NULL);
    return result;
}

void cui_context_menu_open(cui_context *ui, cui_menu_tree_state *state, cui_id restore_focus,
                           cui_rect anchor) {
    if (!ui || !state || !state->overlay.id || !isfinite(anchor.x) || !isfinite(anchor.y) ||
        !isfinite(anchor.w) || !isfinite(anchor.h) || anchor.w < 0 || anchor.h < 0)
        return;
    state->contextual = true;
    state->anchor = anchor;
    state->depth = 0;
    cui_overlay_open(ui, &state->overlay, restore_focus);
    if (restore_focus)
        state->overlay.trigger = restore_focus;
    state->overlay.has_bounds = false;
    cui_invalidate(ui);
}
static bool cui_choice_reduce_context_menu(cui_context *ui, cui_id target,
                                           const cui_menu_entry *items, size_t count,
                                           cui_id *action, cui_menu_tree_state *state,
                                           bool disabled, float dt, bool keep_open_checks,
                                           cui_menu_command_handler handler,
                                           cui_menu_command_result *observed) {
    if (!state || !state->overlay.id || !target)
        return false;
    state->contextual = true;
    cui_rect area = cui_bounds(ui, target);
    size_t event_count = 0;
    const cui_event *events = cui_frame_events(ui, &event_count);
    if (!disabled && cui_pointer_scope_allowed(ui, state->overlay.id))
        for (size_t e = 0; e < event_count; e++) {
            if (cui_event_consumed(ui, e))
                continue;
            cui_event event = events[e];
            if (event.type == CUI_EVENT_POINTER_DOWN &&
                event.pointer_button == CUI_POINTER_SECONDARY && event.x >= area.x &&
                event.x < area.x + area.w && event.y >= area.y && event.y < area.y + area.h)
                cui_context_menu_open(
                    ui, state, state->overlay.open ? state->overlay.trigger : cui_focused(ui),
                    (cui_rect){event.x, event.y, 0, 0});
        }
    if (observed) {
        *observed = cui_base_menu_tree_events(ui, target, cui_string(""), items, count, action,
                                              state, disabled, dt, keep_open_checks, handler);
        return observed->legacy_activated;
    }
    return cui_base_menu_tree(ui, target, cui_string(""), items, count, action, state, disabled, dt,
                              keep_open_checks);
}

bool cui_base_context_menu(cui_context *ui, cui_id target, const cui_menu_entry *items,
                           size_t count, cui_id *action, cui_menu_tree_state *state, bool disabled,
                           float dt, bool keep_open_checks) {
    return cui_choice_reduce_context_menu(ui, target, items, count, action, state, disabled, dt,
                                          keep_open_checks, (cui_menu_command_handler){0}, NULL);
}
cui_menu_command_result cui_base_context_menu_events(cui_context *ui, cui_id target,
                                                     const cui_menu_entry *items, size_t count,
                                                     cui_id *action, cui_menu_tree_state *state,
                                                     bool disabled, float dt, bool keep_open_checks,
                                                     cui_menu_command_handler handler) {
    cui_menu_command_result result;
    memset(&result, 0, sizeof(result));
    if (!ui || !target)
        return result;
    result.legacy_activated = cui_choice_reduce_context_menu(
        ui, target, items, count, action, state, disabled, dt, keep_open_checks, handler, &result);
    return result;
}

static cui_dropdown_button_result cui_choice_reduce_dropdown(
    cui_context *ui, cui_id action_id, cui_id trigger_id, cui_str action_name, cui_str menu_name,
    const cui_menu_entry *items, size_t count, cui_id *menu_action, cui_menu_tree_state *state,
    bool disabled, bool action_disabled, bool action_loading, float dt, bool keep_open_checks,
    cui_dropdown_event_handlers handlers, cui_dropdown_event_result *observed,
    const cui_dropdown_button_metadata *metadata) {
    cui_dropdown_button_result result = {0};
    if (!ui || (!action_id && !trigger_id) || action_id == trigger_id || !menu_action || !state ||
        (!items && count))
        return result;
    if (observed) {
        observed->valid = true;
        observed->submitted = cui_frame_is_submitting(ui) && cui_frame_sequence(ui) != 0;
    }
    if (action_id)
        result.action_activated = cui_base_button(ui, action_id, action_name,
                                                  disabled || action_disabled || action_loading)
                                      .activated;
    if (action_id && action_loading)
        cui_set_semantic_details(
            ui, action_id,
            (cui_semantic_details){.description = cui_string("Loading"), .busy = true});
    if (metadata && action_id) {
        cui_dropdown_part_metadata(ui, action_id, &metadata->action);
        if (metadata->toggle_semantics)
            cui_update_semantic(ui, action_id, metadata->toggled, metadata->toggled ? 1 : 0);
    }
    /* Passive accepted-evidence enumeration after the original registration.
     * No second Button reducer, semantic update, or aggregate reconstruction. */
    if (observed && action_id && handlers.action.callback) {
        size_t event_count = 0;
        (void)cui_frame_events(ui, &event_count);
        for (size_t e = 0; e < event_count; e++) {
            cui_activation_snapshot snapshot;
            cui_component_event_metadata metadata;
            if (cui_event_accepted_activation(ui, e) != action_id ||
                !cui_event_activation_snapshot(ui, e, &snapshot) ||
                !cui_component_event_from_activation(&metadata, CUI_COMPONENT_EVENT_ACTIVATE,
                                                     &snapshot))
                continue;
            cui_control_events_private_increment(&observed->action_count,
                                                 &observed->count_overflow);
            handlers.action.callback(handlers.action.user, &metadata);
        }
    }
    if (trigger_id) {
        if (metadata) {
            cui_menu_command_result menu = {0};
            result.menu_activated = cui_choice_reduce_menu_tree(
                ui, trigger_id, menu_name, items, count, menu_action, state, disabled, dt,
                keep_open_checks, handlers.menu, observed ? &menu : NULL, &metadata->trigger);
            if (observed) {
                observed->command_count = menu.command_count;
                observed->count_overflow |= menu.count_overflow;
            }
        } else if (observed) {
            cui_menu_command_result menu =
                cui_base_menu_tree_events(ui, trigger_id, menu_name, items, count, menu_action,
                                          state, disabled, dt, keep_open_checks, handlers.menu);
            result.menu_activated = menu.legacy_activated;
            observed->command_count = menu.command_count;
            observed->count_overflow |= menu.count_overflow;
        } else
            result.menu_activated =
                cui_base_menu_tree(ui, trigger_id, menu_name, items, count, menu_action, state,
                                   disabled, dt, keep_open_checks);
    } else if (state->overlay.open) {
        cui_overlay_close(ui, &state->overlay);
        state->depth = 0;
    }
    if (trigger_id)
        cui_update_semantic(ui, trigger_id, false, 0);
    return result;
}

cui_dropdown_button_result cui_base_dropdown_button(
    cui_context *ui, cui_id action_id, cui_id trigger_id, cui_str action_name, cui_str menu_name,
    const cui_menu_entry *items, size_t count, cui_id *menu_action, cui_menu_tree_state *state,
    bool disabled, bool action_disabled, bool action_loading, float dt, bool keep_open_checks) {
    return cui_choice_reduce_dropdown(ui, action_id, trigger_id, action_name, menu_name, items,
                                      count, menu_action, state, disabled, action_disabled,
                                      action_loading, dt, keep_open_checks,
                                      (cui_dropdown_event_handlers){0}, NULL, NULL);
}
cui_dropdown_event_result
cui_base_dropdown_button_events(cui_context *ui, cui_id action_id, cui_id trigger_id,
                                cui_str action_name, cui_str menu_name, const cui_menu_entry *items,
                                size_t count, cui_id *menu_action, cui_menu_tree_state *state,
                                bool disabled, bool action_disabled, bool action_loading, float dt,
                                bool keep_open_checks, cui_dropdown_event_handlers handlers) {
    cui_dropdown_event_result result;
    memset(&result, 0, sizeof(result));
    result.legacy = cui_choice_reduce_dropdown(
        ui, action_id, trigger_id, action_name, menu_name, items, count, menu_action, state,
        disabled, action_disabled, action_loading, dt, keep_open_checks, handlers, &result, NULL);
    return result;
}

cui_dropdown_button_result cui_choice_dropdown_submit_metadata(
    cui_context *ui, cui_id action_id, cui_id trigger_id, cui_str action_name, cui_str menu_name,
    const cui_menu_entry *items, size_t count, cui_id *menu_action, cui_menu_tree_state *state,
    bool disabled, bool action_disabled, bool loading, float dt, bool keep_open_checks,
    cui_dropdown_event_handlers handlers, cui_dropdown_event_result *observed,
    const cui_dropdown_button_metadata *metadata) {
    return cui_choice_reduce_dropdown(
        ui, action_id, trigger_id,
        metadata->action.custom_name ? metadata->action.name : action_name,
        metadata->trigger.custom_name ? metadata->trigger.name : menu_name, items, count,
        menu_action, state, disabled, action_disabled, loading, dt, keep_open_checks, handlers,
        observed, metadata);
}
