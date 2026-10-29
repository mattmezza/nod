/* SPDX-License-Identifier: MIT */
#include "cui/cui_disclosure_events.h"
#include <math.h>
void cui_disclosure_motion_step_ex(cui_disclosure_motion *motion, bool open, float dt,
                                   cui_spring_settings settings, bool reduced_motion) {
    if (!motion)
        return;
    float target = open ? 1 : 0;
    if (!isfinite(settings.response) || settings.response < 0)
        settings.response = .18f;
    if (!isfinite(settings.damping) || settings.damping < 0)
        settings.damping = 1;
    if (!isfinite(settings.epsilon) || settings.epsilon <= 0)
        settings.epsilon = .001f;
    if (!motion->initialized || reduced_motion || !settings.travel || settings.response == 0 ||
        !isfinite(motion->progress) || !isfinite(motion->velocity)) {
        motion->progress = target;
        motion->velocity = 0;
        motion->initialized = true;
        return;
    }
    if (!isfinite(dt) || dt <= 0)
        return;
    double omega = 6.28318530717958647692 / settings.response;
    double damping = settings.damping, displacement = motion->progress - target;
    double velocity = motion->velocity, time = dt, next, speed;
    if (fabs(damping - 1) < .0001) {
        double sum = velocity + omega * displacement, decay = exp(-omega * time);
        next = (displacement + sum * time) * decay;
        speed = (velocity - omega * sum * time) * decay;
    } else if (damping < 1) {
        double frequency = omega * sqrt(1 - damping * damping);
        double decay = exp(-damping * omega * time);
        double sine = sin(frequency * time), cosine = cos(frequency * time);
        next = decay * (displacement * cosine +
                        (velocity + damping * omega * displacement) / frequency * sine);
        speed = decay *
                (velocity * cosine -
                 (damping * omega * velocity + omega * omega * displacement) / frequency * sine);
    } else {
        double factor = damping + sqrt(damping * damping - 1);
        double slow = -omega / factor, fast = -omega * factor;
        double first = (velocity - fast * displacement) / (slow - fast);
        double second = displacement - first;
        next = first * exp(slow * time) + second * exp(fast * time);
        speed = slow * first * exp(slow * time) + fast * second * exp(fast * time);
    }
    motion->progress = (float)(target + next);
    motion->velocity = (float)speed;
    if (fabs(next) < settings.epsilon && fabs(speed) < settings.epsilon) {
        motion->progress = target;
        motion->velocity = 0;
    }
}
void cui_disclosure_motion_step(cui_disclosure_motion *motion, bool open, float dt, float response,
                                bool reduced_motion) {
    if (!isfinite(response) || response <= 0)
        response = .2f;
    cui_disclosure_motion_step_ex(motion, open, dt, (cui_spring_settings){response, 1, .001f, true},
                                  reduced_motion);
}

static bool cui_disclosure_evidence(cui_context *ui, cui_id target, size_t index,
                                    cui_component_event_metadata *out) {
    if (!target || !cui_frame_paint_enabled(ui) || !cui_frame_sequence(ui))
        return false;
    cui_activation_snapshot snapshot;
    if (cui_event_activation_snapshot(ui, index, &snapshot) &&
        cui_component_event_from_activation(out, CUI_COMPONENT_EVENT_CHANGE, &snapshot)) {
        out->target = target;
        return true;
    }
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    if (index >= count)
        return false;
    cui_component_event_source source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN;
    switch (events[index].type) {
    case CUI_EVENT_POINTER_UP:
        source = CUI_COMPONENT_EVENT_SOURCE_POINTER;
        break;
    case CUI_EVENT_KEY_DOWN:
    case CUI_EVENT_KEY_UP:
        source = CUI_COMPONENT_EVENT_SOURCE_KEYBOARD;
        break;
    case CUI_EVENT_ACCESSIBILITY_ACTION:
        source = CUI_COMPONENT_EVENT_SOURCE_ACCESSIBILITY;
        break;
    default:
        break;
    }
    return cui_component_event_init(out, CUI_COMPONENT_EVENT_CHANGE, target, source) &&
           cui_component_event_set_frame_index(out, cui_frame_sequence(ui), index);
}
static void cui_disclosure_notify(cui_disclosure_event_result *result,
                                  cui_disclosure_handler handler,
                                  cui_component_event_metadata evidence, cui_id item, size_t index,
                                  bool previous, bool expanded) {
    cui_disclosure_change change = {evidence, item, index, previous, expanded};
    handler.on_change(handler.user, &change);
    if (result->change_count == SIZE_MAX)
        result->count_overflow = true;
    else
        ++result->change_count;
}
static cui_disclosure_event_result cui_collapsible_common(cui_context *ui, cui_id id, cui_str label,
                                                          bool *open, bool disabled,
                                                          cui_disclosure_handler handler) {
    cui_disclosure_event_result result = {0};
    if (!open)
        return result;
    cui_interaction interaction =
        cui_interact(ui, id, CUI_ROLE_BUTTON, label, disabled, *open, *open);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.expanded = *open, .controls = id ^ UINT64_C(0x1d8f322539deca61)});
    (void)interaction;
    if (disabled)
        return result;
    size_t event_count = 0;
    cui_frame_events(ui, &event_count);
    for (size_t e = 0; e < event_count; e++)
        if (cui_event_activation(ui, e) == id) {
            cui_component_event_metadata evidence;
            bool notify = handler.on_change && cui_disclosure_evidence(ui, id, e, &evidence);
            bool previous = *open;
            *open = !*open;
            result.changed = true;
            if (notify)
                cui_disclosure_notify(&result, handler, evidence, id, 0, previous, *open);
        }
    if (!result.changed)
        return result;
    cui_update_semantic(ui, id, *open, *open);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.expanded = *open, .controls = id ^ UINT64_C(0x1d8f322539deca61)});
    return result;
}
bool cui_base_collapsible(cui_context *ui, cui_id id, cui_str label, bool *open, bool disabled) {
    return cui_collapsible_common(ui, id, label, open, disabled, (cui_disclosure_handler){0})
        .changed;
}
cui_disclosure_event_result cui_base_collapsible_events(cui_context *ui, cui_id id, cui_str label,
                                                        bool *open, bool disabled,
                                                        cui_disclosure_handler handler) {
    if (!ui || !id || !open)
        return (cui_disclosure_event_result){0};
    return cui_collapsible_common(ui, id, label, open, disabled, handler);
}
static cui_disclosure_event_result
cui_accordion_common(cui_context *ui, cui_id target, const cui_disclosure_item *items, size_t count,
                     bool *open, cui_accordion_options options, cui_disclosure_handler handler) {
    cui_disclosure_event_result result = {0};
    if (!items || !open || !count)
        return result;
    for (size_t i = 0; i < count; ++i) {
        cui_interact(ui, items[i].id, CUI_ROLE_BUTTON, items[i].label,
                     options.disabled || items[i].disabled, open[i], open[i]);
    }
    size_t event_count = 0;
    const cui_event *events = cui_frame_events(ui, &event_count);
    size_t cursor = count;
    for (size_t e = 0; e < event_count && !options.disabled; ++e) {
        if (events[e].type == CUI_EVENT_KEY_DOWN && events[e].key == CUI_KEY_TAB)
            cursor = count;
        size_t from = count;
        cui_id routed_target = cui_event_target(ui, e);
        for (size_t i = 0; i < count; ++i)
            if (items[i].id == routed_target)
                from = i;
        if (from == count || items[from].disabled)
            continue;
        if (cursor == count)
            cursor = from;
        if (cui_event_activation(ui, e) && (events[e].type != CUI_EVENT_KEY_UP ||
                                            items[cursor].id == cui_event_activation(ui, e))) {
            if (events[e].type == CUI_EVENT_POINTER_UP)
                cursor = from;
            if (!open[cursor] || options.collapsible) {
                cui_component_event_metadata evidence;
                bool notify =
                    handler.on_change && cui_disclosure_evidence(ui, target, e, &evidence);
                bool next_open = !open[cursor];
                if (next_open && !options.multiple)
                    for (size_t j = 0; j < count; j++) {
                        bool previous = open[j];
                        open[j] = false;
                        if (notify && previous)
                            cui_disclosure_notify(&result, handler, evidence, items[j].id, j,
                                                  previous, false);
                    }
                bool previous = open[cursor];
                open[cursor] = next_open;
                result.changed = true;
                if (notify && previous != next_open)
                    cui_disclosure_notify(&result, handler, evidence, items[cursor].id, cursor,
                                          previous, next_open);
            }
        }
        if (events[e].type != CUI_EVENT_KEY_DOWN)
            continue;
        cui_key key = events[e].key;
        int direction = key == CUI_KEY_DOWN || key == CUI_KEY_HOME ? 1 : -1;
        if (key != CUI_KEY_UP && key != CUI_KEY_DOWN && key != CUI_KEY_HOME && key != CUI_KEY_END)
            continue;
        size_t next = key == CUI_KEY_HOME ? count - 1 : key == CUI_KEY_END ? 0 : cursor;
        for (size_t n = 0; n < count; ++n) {
            next = direction > 0 ? (next + 1) % count : (next ? next - 1 : count - 1);
            if (!items[next].disabled) {
                cursor = next;
                cui_focus(ui, items[next].id);
                break;
            }
        }
    }
    for (size_t i = 0; i < count; ++i) {
        cui_update_semantic(ui, items[i].id, open[i], open[i]);
        cui_set_semantic_details(
            ui, items[i].id,
            (cui_semantic_details){.expanded = open[i],
                                   .controls = items[i].id ^ UINT64_C(0x1d8f322539deca61)});
    }
    return result;
}

bool cui_base_accordion(cui_context *ui, const cui_disclosure_item *items, size_t count, bool *open,
                        cui_accordion_options options) {
    return cui_accordion_common(ui, 0, items, count, open, options, (cui_disclosure_handler){0})
        .changed;
}
cui_disclosure_event_result cui_base_accordion_events(cui_context *ui, cui_id target,
                                                      const cui_disclosure_item *items,
                                                      size_t count, bool *open,
                                                      cui_accordion_options options,
                                                      cui_disclosure_handler handler) {
    if (!ui || !target || !items || !count || !open)
        return (cui_disclosure_event_result){0};
    for (size_t i = 0; i < count; ++i)
        if (!items[i].id)
            return (cui_disclosure_event_result){0};
    return cui_accordion_common(ui, target, items, count, open, options, handler);
}
