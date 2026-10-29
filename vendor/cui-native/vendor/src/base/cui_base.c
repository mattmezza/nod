/* SPDX-License-Identifier: MIT */
#include "cui/cui_base.h"
#include "cui/cui_slider_events.h"
#include "cui/cui_text_input_events.h"
#include <math.h>
static float cui_progress_fraction(float value) {
    return isfinite(value) ? fmaxf(0, fminf(1, value)) : 0;
}
static double cui_progress_quadratic(double value) {
    return value < .5 ? 2 * value * value : 1 - 2 * (1 - value) * (1 - value);
}
cui_progress_geometry cui_progress_measure(float fraction, bool indeterminate, bool source_motion,
                                           double now, bool reduced) {
    cui_progress_geometry result = {.fraction = cui_progress_fraction(fraction)};
    if (!indeterminate) {
        result.length = result.fraction;
        return result;
    }
    bool moving = !reduced && isfinite(now) && now >= 0;
    result.animating = moving;
    if (source_motion) {
        if (!moving) {
            result.start = .325f;
            result.length = .35f;
        } else {
            double delta = fmod(now, 1);
            double start = cui_progress_quadratic(fmax(0, fmin(1, (delta - .5) * 2)));
            double end = cui_progress_quadratic(1 - delta);
            result.start = (float)start;
            result.length = (float)fmax(0, 1 - start - end);
        }
    } else {
        double phase = moving ? .5 - .5 * cos(fmod(now, 1.6) * (6.283185307179586 / 1.6)) : .5;
        result.start = (float)(.7 * phase);
        result.length = .3f;
    }
    return result;
}
float cui_base_progress(cui_context *ui, cui_id id, cui_str name, float fraction,
                        bool indeterminate) {
    float value = cui_progress_fraction(fraction);
    if (!ui || !id || (name.length && !name.data))
        return value;
    cui_interact(ui, id, CUI_ROLE_PROGRESSBAR, name.length ? name : cui_string("Progress"), false,
                 false, indeterminate ? 0 : value);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){
            .minimum = 0, .maximum = 1, .busy = indeterminate, .no_focus = true});
    cui_set_tab_stop(ui, id, false);
    return value;
}
static float cui_progress_ease(float fraction) {
    if (fraction <= 0 || fraction >= 1)
        return fraction;
    double low = 0, high = 1, parameter = fraction;
    for (unsigned i = 0; i < 24; i++) {
        parameter = (low + high) * .5;
        double inverse = 1 - parameter;
        double x = 3 * inverse * inverse * parameter * .2 + parameter * parameter * parameter;
        if (x < fraction)
            low = parameter;
        else
            high = parameter;
    }
    return (float)(3 * (1 - parameter) * parameter * parameter + parameter * parameter * parameter);
}
static void cui_progress_motion_sample(cui_progress_motion *state, double now) {
    if (!state->active)
        return;
    double elapsed = now - state->start;
    float fraction = (float)fmax(0, fmin(1, elapsed / state->duration));
    state->value = state->from + (state->target - state->from) * cui_progress_ease(fraction);
    if (fraction >= 1) {
        state->value = state->target;
        state->active = false;
    }
}
bool cui_progress_animate(cui_progress_motion *state, float value, double now, float duration,
                          bool reduced, cui_progress_motion_result *result) {
    if (!state || !result || !isfinite(value) || !isfinite(now) || now < 0 || !isfinite(duration) ||
        duration < 0 || duration > 60 ||
        (state->ready &&
         (!isfinite(state->start) || !isfinite(state->last_time) || !isfinite(state->from) ||
          !isfinite(state->target) || !isfinite(state->value) || !isfinite(state->duration) ||
          state->duration < 0 || state->duration > 60 || state->start < 0 ||
          state->last_time < state->start || state->from < 0 || state->from > 1 ||
          state->target < 0 || state->target > 1 || state->value < 0 || state->value > 1 ||
          (state->active && state->duration <= 0))))
        return false;
    value = cui_progress_fraction(value);
    cui_progress_motion next = *state;
    if (!next.ready) {
        next = (cui_progress_motion){.ready = true,
                                     .from = value,
                                     .target = value,
                                     .value = value,
                                     .start = now,
                                     .last_time = now,
                                     .duration = duration};
    } else {
        now = fmax(now, next.last_time);
        cui_progress_motion_sample(&next, now);
        if (next.target != value) {
            next.from = next.value;
            next.target = value;
            next.start = now;
            next.duration = duration;
            next.active = next.from != value;
        }
        if (reduced || duration == 0) {
            next.from = next.target = next.value = value;
            next.active = false;
            next.duration = duration;
            next.start = now;
        }
        next.last_time = now;
        cui_progress_motion_sample(&next, now);
    }
    *state = next;
    *result = (cui_progress_motion_result){next.value, next.active};
    return true;
}
cui_interaction cui_base_button(cui_context *ui, cui_id id, cui_str name, bool disabled) {
    cui_interaction interaction = cui_interact(ui, id, CUI_ROLE_BUTTON, name, disabled, false, 0);
    if (disabled)
        cui_set_semantic_details(ui, id, (cui_semantic_details){.blocks_pointer_activation = true});
    return interaction;
}
static bool cui_base_binary(cui_context *ui, cui_id id, cui_str name, bool *value, bool disabled,
                            cui_role role) {
    if (!value)
        return false;
    cui_interaction s = cui_interact(ui, id, role, name, disabled, *value, 0);
    if (disabled || !s.activated)
        return false;
    *value = !*value;
    cui_update_semantic(ui, id, *value, *value ? 1 : 0);
    return true;
}
bool cui_base_checkbox(cui_context *u, cui_id i, cui_str n, bool *v, bool d) {
    return cui_base_binary(u, i, n, v, d, CUI_ROLE_CHECKBOX);
}
bool cui_base_switch(cui_context *u, cui_id i, cui_str n, bool *v, bool d) {
    return cui_base_binary(u, i, n, v, d, CUI_ROLE_SWITCH);
}
bool cui_base_toggle(cui_context *u, cui_id i, cui_str n, bool *v, bool d) {
    return cui_base_binary(u, i, n, v, d, CUI_ROLE_BUTTON);
}
bool cui_base_check(cui_context *ui, cui_id id, cui_str name, cui_check_state *v, bool d) {
    if (!v)
        return false;
    cui_interaction s =
        cui_interact(ui, id, CUI_ROLE_CHECKBOX, name, d, *v == CUI_CHECK_CHECKED, (float)*v);
    if (d || !s.activated)
        return false;
    *v = *v == CUI_CHECK_CHECKED ? CUI_CHECK_UNCHECKED : CUI_CHECK_CHECKED;
    cui_update_semantic(ui, id, *v == CUI_CHECK_CHECKED, (float)*v);
    return true;
}
bool cui_base_check_ex(cui_context *ui, cui_id id, cui_str name, cui_check_state *value,
                       cui_check_options o) {
    if (!ui || !cui_frame_is_submitting(ui) || !id || !value || *value < CUI_CHECK_UNCHECKED ||
        *value > CUI_CHECK_MIXED || (name.length && !name.data) ||
        (o.custom_role && (o.role < CUI_ROLE_NONE || o.role > CUI_ROLE_RADIOGROUP)))
        return false;
    cui_check_state before = *value;
    cui_check_state requested =
        before == CUI_CHECK_CHECKED ? CUI_CHECK_UNCHECKED : CUI_CHECK_CHECKED;
    cui_interact(ui, id, o.custom_role ? o.role : CUI_ROLE_CHECKBOX, name, o.disabled,
                 before == CUI_CHECK_CHECKED, (float)before);
    cui_set_semantic_details(ui, id,
                             (cui_semantic_details){.required = o.required,
                                                    .invalid = o.invalid,
                                                    .no_pointer_focus = o.no_pointer_focus,
                                                    .pointer_activatable = true,
                                                    .focusable = true});
    cui_set_tab_index(ui, id, o.tab_index);
    cui_set_tab_stop(ui, id, !o.skip_tab);
    bool activated = false;
    size_t count = 0;
    cui_frame_events(ui, &count);
    for (size_t e = 0; !o.disabled && e < count; ++e) {
        cui_activation_snapshot event;
        if (cui_event_accepted_activation(ui, e) != id ||
            !cui_event_activation_snapshot(ui, e, &event))
            continue;
        activated = true;
        if (o.on_change)
            o.on_change(o.change_user, requested, &event);
        else if (!o.controlled)
            *value = requested;
    }
    cui_update_semantic(ui, id, *value == CUI_CHECK_CHECKED, (float)*value);
    return activated;
}
bool cui_check_box_style(cui_check_state value, bool disabled, cui_check_box_styles styles,
                         cui_box *box) {
    if (!box || value < CUI_CHECK_UNCHECKED || value > CUI_CHECK_MIXED)
        return false;
    cui_check_box_style_fn layers[] = {styles.instance,
                                       value == CUI_CHECK_CHECKED ? styles.checked
                                       : value == CUI_CHECK_MIXED ? styles.mixed
                                                                  : NULL,
                                       disabled ? styles.disabled : NULL};
    for (size_t i = 0; i < sizeof layers / sizeof layers[0]; ++i)
        if (layers[i])
            layers[i](styles.user, box);
    return true;
}
bool cui_base_check_indicator(cui_context *ui, cui_id id, cui_check_state value, bool disabled,
                              cui_box box, cui_check_box_styles styles,
                              cui_check_content_fn content, void *user) {
    if (!ui || !cui_frame_is_submitting(ui) || !cui_check_box_style(value, disabled, styles, &box))
        return false;
    cui_begin_box(ui, id, box);
    if (content)
        content(ui, user);
    cui_end_box(ui);
    return cui_last_error(ui) == CUI_OK;
}
bool cui_base_radio(cui_context *ui, cui_id id, cui_str name, int *value, int option,
                    bool disabled) {
    if (!value)
        return false;
    cui_interaction s =
        cui_interact(ui, id, CUI_ROLE_RADIO, name, disabled, *value == option, (float)option);
    if (disabled || !s.activated || *value == option)
        return false;
    *value = option;
    /* Earlier siblings may already have painted the old controlled value. */
    cui_invalidate(ui);
    cui_update_semantic(ui, id, true, (float)option);
    return true;
}
static float cui_base_clamp(float x, float a, float b) {
    return x < a ? a : x > b ? b : x;
}
static float cui_base_quantize(float x, float a, float b, float step) {
    x = cui_base_clamp(x, a, b);
    if (step > 0 && isfinite(step) && x != b)
        x = a + roundf((x - a) / step) * step;
    return cui_base_clamp(x, a, b);
}
bool cui_base_slider(cui_context *ui, cui_id id, cui_str name, float *value, float min, float max,
                     float step, bool disabled) {
    if (!value || !isfinite(min) || !isfinite(max) || max < min)
        return false;
    float old = *value;
    float next = isfinite(old) ? cui_base_clamp(old, min, max) : min;
    cui_interaction s = cui_interact(ui, id, CUI_ROLE_SLIDER, name, disabled, false, next);
    cui_set_semantic_details(ui, id, (cui_semantic_details){.minimum = min, .maximum = max});
    if (disabled)
        return false;
    cui_rect b = cui_bounds(ui, id);
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    bool dragging = s.pressed;
    for (size_t i = 0; i < count; i++) {
        cui_event e = events[i];
        if ((e.type == CUI_EVENT_POINTER_DOWN || e.type == CUI_EVENT_POINTER_UP) &&
            e.pointer_button != CUI_POINTER_PRIMARY)
            continue;
        if (cui_event_target(ui, i) != id)
            continue;
        if (e.type == CUI_EVENT_POINTER_DOWN)
            dragging = true;
        if (dragging &&
            (e.type == CUI_EVENT_POINTER_DOWN || e.type == CUI_EVENT_POINTER_MOVE ||
             e.type == CUI_EVENT_POINTER_UP) &&
            b.w > 0)
            next = cui_base_quantize(min + (max - min) * cui_base_clamp((e.x - b.x) / b.w, 0, 1),
                                     min, max, step);
        if (e.type == CUI_EVENT_POINTER_UP || e.type == CUI_EVENT_CANCEL)
            dragging = false;
        if (e.type == CUI_EVENT_KEY_DOWN) {
            float increment = step > 0 && isfinite(step) ? step : (max - min) / 100;
            if (e.key == CUI_KEY_HOME)
                next = min;
            else if (e.key == CUI_KEY_END)
                next = max;
            else if (e.key == CUI_KEY_LEFT || e.key == CUI_KEY_DOWN)
                next = cui_base_quantize(next - increment, min, max, step);
            else if (e.key == CUI_KEY_RIGHT || e.key == CUI_KEY_UP)
                next = cui_base_quantize(next + increment, min, max, step);
        }
    }
    *value = next;
    cui_update_semantic(ui, id, false, next);
    return old != next;
}

static double cui_slider_limit(double value, cui_slider_options o) {
    value = fmax(o.minimum, fmin(o.maximum, value));
    if (o.step > 0 && value != o.maximum) {
        double units = (value - o.minimum) / o.step;
        if (isfinite(units))
            value = o.minimum + round(units) * o.step;
    }
    return fmax(o.minimum, fmin(o.maximum, value));
}
typedef struct {
    cui_context *ui;
    cui_id id;
    double current;
    cui_component_slider_handler handler;
    cui_component_slider_result *result;
} cui_slider_dispatch;
static void cui_slider_count(size_t *count, bool *overflow) {
    if (*count == SIZE_MAX)
        *overflow = true;
    else
        ++*count;
}
static void cui_slider_emit(cui_slider_options o, cui_slider_phase phase, double value,
                            cui_event event, size_t index, cui_slider_event_reason reason,
                            cui_slider_dispatch *dispatch) {
    if (o.on_request) {
        cui_slider_request request = {.phase = phase, .value = value, .event = event};
        o.on_request(o.user, &request);
    }
    if (!dispatch || !dispatch->handler.callback)
        return;
    cui_component_slider_request request = {.phase = phase,
                                            .current = dispatch->current,
                                            .requested = value,
                                            .reason = reason,
                                            .has_input = index != SIZE_MAX};
    cui_component_event_kind kind = phase == CUI_SLIDER_UPDATE   ? CUI_COMPONENT_EVENT_REQUEST
                                    : phase == CUI_SLIDER_COMMIT ? CUI_COMPONENT_EVENT_COMMIT
                                                                 : CUI_COMPONENT_EVENT_LIFECYCLE;
    cui_component_event_source source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN;
    if (index != SIZE_MAX) {
        if (event.type == CUI_EVENT_KEY_DOWN)
            source = CUI_COMPONENT_EVENT_SOURCE_KEYBOARD;
        else if (event.type == CUI_EVENT_POINTER_DOWN || event.type == CUI_EVENT_POINTER_MOVE ||
                 event.type == CUI_EVENT_POINTER_UP)
            source = CUI_COMPONENT_EVENT_SOURCE_POINTER;
        request.input = (cui_slider_event_input){.type = event.type,
                                                 .x = event.x,
                                                 .y = event.y,
                                                 .key = event.key,
                                                 .modifiers = event.modifiers,
                                                 .repeat = event.repeat,
                                                 .pointer_type = event.pointer_type,
                                                 .pointer_button = event.pointer_button,
                                                 .pointer_id = event.pointer_id,
                                                 .pointer_device = event.pointer_device};
    }
    (void)cui_component_event_init(&request.event, kind, dispatch->id, source);
    if (index != SIZE_MAX)
        (void)cui_component_event_set_frame_index(&request.event, cui_frame_sequence(dispatch->ui),
                                                  index);
    size_t *count = phase == CUI_SLIDER_BEGIN    ? &dispatch->result->begin_count
                    : phase == CUI_SLIDER_UPDATE ? &dispatch->result->request_count
                    : phase == CUI_SLIDER_COMMIT ? &dispatch->result->commit_count
                                                 : &dispatch->result->cancel_count;
    cui_slider_count(count, &dispatch->result->count_overflow);
    dispatch->handler.callback(dispatch->handler.user, &request);
}
static bool cui_slider_controlled(cui_context *ui, cui_id id, cui_str name, double value,
                                  cui_slider_state *state, cui_slider_options o,
                                  cui_slider_dispatch *dispatch) {
    if (!ui || !cui_frame_is_submitting(ui) || !id || !state || (name.length && !name.data) ||
        (o.value_description.length && !o.value_description.data) || !isfinite(value) ||
        !isfinite(o.minimum) || !isfinite(o.maximum) || o.maximum < o.minimum ||
        !isfinite(o.maximum - o.minimum) || value < o.minimum || value > o.maximum ||
        !isfinite(o.step) || o.step < 0 || (state->id && state->id != id) ||
        (state->dragging && (!isfinite(state->preview) || !isfinite(state->start))))
        return false;
    bool readonly = o.readonly || (!o.on_request && !(dispatch && dispatch->handler.callback));
    double span = o.maximum - o.minimum;
    float ratio = span > 0 ? (float)((value - o.minimum) / span) : 0;
    cui_interact(ui, id, CUI_ROLE_SLIDER, name, o.disabled, false, ratio);
    cui_set_semantic_details(ui, id,
                             (cui_semantic_details){.minimum = 0,
                                                    .maximum = 1,
                                                    .readonly = readonly,
                                                    .description = o.value_description,
                                                    .touch_drag_priority = true});
    state->id = id;
    if (o.disabled || readonly || span == 0) {
        if (state->dragging)
            cui_slider_emit(o, CUI_SLIDER_CANCEL, state->start, (cui_event){0}, SIZE_MAX,
                            o.disabled ? CUI_SLIDER_EVENT_DISABLED
                            : readonly ? CUI_SLIDER_EVENT_READONLY
                                       : CUI_SLIDER_EVENT_ZERO_SPAN,
                            dispatch);
        state->dragging = false;
        state->preview = value;
        return true;
    }
    if (!cui_frame_paint_enabled(ui))
        return true;
    cui_rect bounds = cui_bounds(ui, id);
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    if (!state->dragging)
        state->preview = value;
    for (size_t i = 0; i < count; ++i) {
        cui_event e = events[i];
        if (cui_event_target(ui, i) != id || cui_event_consumed(ui, i))
            continue;
        if ((e.type == CUI_EVENT_POINTER_DOWN || e.type == CUI_EVENT_POINTER_UP) &&
            e.pointer_button != CUI_POINTER_PRIMARY)
            continue;
        if (e.type == CUI_EVENT_POINTER_DOWN && bounds.w > 0) {
            state->dragging = true;
            state->start = value;
            cui_slider_emit(o, CUI_SLIDER_BEGIN, value, e, i, CUI_SLIDER_EVENT_INPUT, dispatch);
        }
        if (state->dragging && bounds.w > 0 &&
            (e.type == CUI_EVENT_POINTER_DOWN || e.type == CUI_EVENT_POINTER_MOVE ||
             e.type == CUI_EVENT_POINTER_UP)) {
            double fraction = fmax(0, fmin(1, ((double)e.x - bounds.x) / bounds.w));
            state->preview = cui_slider_limit(o.minimum + span * fraction, o);
            cui_slider_emit(o, CUI_SLIDER_UPDATE, state->preview, e, i, CUI_SLIDER_EVENT_INPUT,
                            dispatch);
        }
        if (state->dragging && e.type == CUI_EVENT_POINTER_UP) {
            state->dragging = false;
            cui_slider_emit(o, CUI_SLIDER_COMMIT, state->preview, e, i, CUI_SLIDER_EVENT_INPUT,
                            dispatch);
        } else if (state->dragging && (e.type == CUI_EVENT_CANCEL ||
                                       (e.type == CUI_EVENT_KEY_DOWN && e.key == CUI_KEY_ESCAPE))) {
            state->dragging = false;
            state->preview = state->start;
            cui_slider_emit(o, CUI_SLIDER_CANCEL, state->start, e, i, CUI_SLIDER_EVENT_INPUT,
                            dispatch);
        } else if (!state->dragging && e.type == CUI_EVENT_KEY_DOWN) {
            double increment = o.step > 0 ? o.step : span / 100;
            double next = value;
            if (e.key == CUI_KEY_HOME)
                next = o.minimum;
            else if (e.key == CUI_KEY_END)
                next = o.maximum;
            else if (e.key == CUI_KEY_LEFT || e.key == CUI_KEY_DOWN)
                next = cui_slider_limit(value - increment, o);
            else if (e.key == CUI_KEY_RIGHT || e.key == CUI_KEY_UP)
                next = cui_slider_limit(value + increment, o);
            else
                continue;
            cui_slider_emit(o, CUI_SLIDER_BEGIN, value, e, i, CUI_SLIDER_EVENT_INPUT, dispatch);
            cui_slider_emit(o, CUI_SLIDER_UPDATE, next, e, i, CUI_SLIDER_EVENT_INPUT, dispatch);
            cui_slider_emit(o, CUI_SLIDER_COMMIT, next, e, i, CUI_SLIDER_EVENT_INPUT, dispatch);
            state->preview = next;
        }
    }
    return true;
}
bool cui_base_slider_controlled(cui_context *ui, cui_id id, cui_str name, double value,
                                cui_slider_state *state, cui_slider_options o) {
    return cui_slider_controlled(ui, id, name, value, state, o, NULL);
}
cui_component_slider_result cui_base_slider_events(cui_context *ui, cui_id id, cui_str name,
                                                   double value, cui_slider_state *state,
                                                   cui_slider_options o,
                                                   cui_component_slider_handler handler) {
    cui_component_slider_result result = {0};
    if (o.on_request || o.user)
        return result;
    cui_slider_dispatch dispatch = {ui, id, value, handler, &result};
    result.valid = cui_slider_controlled(ui, id, name, value, state, o, &dispatch);
    result.submitted = result.valid;
    return result;
}

static bool cui_input_readonly_event(cui_event event) {
    if (event.type != CUI_EVENT_KEY_DOWN)
        return false;
    return event.key == CUI_KEY_LEFT || event.key == CUI_KEY_RIGHT || event.key == CUI_KEY_UP ||
           event.key == CUI_KEY_DOWN || event.key == CUI_KEY_HOME || event.key == CUI_KEY_END ||
           (event.key == CUI_KEY_A && (event.modifiers & (CUI_MOD_CTRL | CUI_MOD_META)));
}
/* Pinned source text_boundary::word_range_from_chars, not UAX word motion.
 * Scalar decoding is bounded by the already-valid model and 128 neighbors. */
static size_t cui_input_scalar_next(cui_str text, size_t at) {
    if (at >= text.length)
        return text.length;
    at++;
    while (at < text.length && ((unsigned char)text.data[at] & 0xc0u) == 0x80u)
        at++;
    return at;
}
static size_t cui_input_scalar_previous(cui_str text, size_t at) {
    if (!at)
        return 0;
    at--;
    while (at && ((unsigned char)text.data[at] & 0xc0u) == 0x80u)
        at--;
    return at;
}
static unsigned cui_input_character_kind(cui_str text, size_t at) {
    size_t end = cui_input_scalar_next(text, at), width = end - at;
    const unsigned char *p = (const unsigned char *)text.data + at;
    uint32_t cp = p[0] & (width == 1 ? 127u : width == 2 ? 31u : width == 3 ? 15u : 7u);
    for (size_t i = 1; i < width; i++)
        cp = (cp << 6) | (p[i] & 63u);
    if (cp == '_' || (cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') ||
        (cp >= 'a' && cp <= 'z') || (cp >= 0xc0 && cp <= 0x24f) || (cp >= 0x400 && cp <= 0x4ff) ||
        (cp >= 0x1e00 && cp <= 0x1eff) || (cp >= 0x300 && cp <= 0x36f))
        return 1;
    if (cp == '\n' || cp == '\r')
        return 3;
    if ((cp >= 9 && cp <= 13) || cp == 0x20 || cp == 0x85 || cp == 0xa0 || cp == 0x1680 ||
        (cp >= 0x2000 && cp <= 0x200a) || cp == 0x2028 || cp == 0x2029 || cp == 0x202f ||
        cp == 0x205f || cp == 0x3000)
        return 2;
    return 0;
}
static bool cui_input_pointer_word(cui_str text, size_t offset, size_t *start, size_t *end) {
    if (offset > text.length)
        offset = text.length;
    while (offset < text.length && offset && ((unsigned char)text.data[offset] & 0xc0u) == 0x80u)
        offset--;
    if (offset == text.length)
        return false;
    unsigned kind = cui_input_character_kind(text, offset);
    *start = offset;
    *end = cui_input_scalar_next(text, offset);
    if (kind != 1 && kind != 2)
        return true;
    for (unsigned i = 0; i < 128 && *start; i++) {
        size_t previous = cui_input_scalar_previous(text, *start);
        if (cui_input_character_kind(text, previous) != kind)
            break;
        *start = previous;
    }
    for (unsigned i = 0; i < 128 && *end < text.length; i++) {
        if (cui_input_character_kind(text, *end) != kind)
            break;
        *end = cui_input_scalar_next(text, *end);
    }
    return true;
}
static void cui_input_history_focus(cui_context *ui, cui_id id, cui_edit *edit,
                                    cui_input_behavior_state *state, bool disabled) {
    bool focused = !disabled && cui_focused(ui) == id;
    if (disabled || !focused || state->bound_edit_identity != (uintptr_t)edit ||
        state->pointer_word_revision != cui_edit_get_state(edit).revision)
        state->pointer_word_active = false;
    if (!state->focus_initialized || state->bound_edit_identity != (uintptr_t)edit ||
        state->was_focused != focused)
        cui_edit_break_coalescing(edit);
    state->bound_edit_identity = (uintptr_t)edit;
    state->focus_initialized = true;
    state->was_focused = focused;
}
static void cui_input_secure_lifecycle(cui_edit *edit, cui_input_behavior_state *state,
                                       cui_base_input_options options) {
    if (state->secure_initialized && state->was_masked != options.masked)
        state->pointer_word_active = false;
    if (state->secure_initialized &&
        (state->was_masked != options.masked ||
         state->previous_content_type != options.content_type) &&
        state->composition && state->composition->active)
        (void)cui_edit_composition_event(edit, state->composition,
                                         (cui_event){.type = CUI_EVENT_COMPOSITION_CANCEL}, false);
    state->secure_initialized = true;
    state->was_masked = options.masked;
    state->previous_content_type = options.content_type;
}
static cui_edit_result cui_input_checked(cui_edit *edit, cui_edit_request request,
                                         cui_base_input_options options, bool *rejected) {
    request.normalization = options.normalization;
    request.format = options.format;
    request.format_workspace = options.format_workspace;
    request.matcher = options.matcher;
    request.presentation = options.presentation;
    request.masked_words = options.masked;
    cui_edit_checked_result result = cui_edit_apply_checked(
        edit, request, (cui_edit_acceptance){options.validate, options.validate_user});
    if (result.presentation_prepared && options.presentation_complete)
        options.presentation_complete(options.presentation_user, true, result.presentation_applied);
    *rejected |= result.rejected;
    return result.error;
}
cui_input_result cui_base_input_reduce_event(cui_context *ui, cui_id id, cui_edit *edit,
                                             cui_input_behavior_state *state,
                                             cui_base_input_options options, size_t i) {
    cui_input_result result = {.error = CUI_EDIT_OK};
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    if (!ui || !id || !edit || !state || !events || i >= count) {
        result.error = CUI_EDIT_INVALID_ARGUMENT;
        return result;
    }
    if (options.content_type < CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED ||
        options.content_type > CUI_TEXT_INPUT_PURPOSE_NEW_PASSWORD ||
        ((options.masked || options.content_type != CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED) &&
         (cui_edit_get_state(edit).multiline || cui_edit_get_content(edit).token_count)))
        return (cui_input_result){.error = CUI_EDIT_INVALID_ARGUMENT};
    cui_edit_checked_result configured =
        cui_edit_apply_checked(edit,
                               (cui_edit_request){.kind = CUI_EDIT_REQUEST_PRESENT,
                                                  .format = options.format,
                                                  .format_workspace = options.format_workspace},
                               (cui_edit_acceptance){0});
    if (configured.error != CUI_EDIT_OK)
        return (cui_input_result){.error = configured.error};
    /* Owned sessions may have expired during an omitted/rejected submission.
     * Reacquire without dereferencing the old pointer before lifecycle checks. */
    state->composition =
        cui_base_composition(ui, id, state->composition, &state->composition_owned);
    cui_input_secure_lifecycle(edit, state, options);
    cui_input_history_focus(ui, id, edit, state, options.disabled);
    cui_edit_state before = cui_edit_get_state(edit);
    if (state->composition && state->composition->active)
        (void)cui_edit_composition_event(edit, state->composition,
                                         (cui_event){.type = CUI_EVENT_GESTURE}, true);
    if (state->composition && (options.disabled || options.readonly || cui_focused(ui) != id))
        (void)cui_edit_composition_event(edit, state->composition,
                                         (cui_event){.type = CUI_EVENT_COMPOSITION_CANCEL}, false);
    if (options.disabled) {
        state->dragging = false;
        state->pointer_word_active = false;
    }
    cui_event event = events[i];
    if (event.type == CUI_EVENT_KEY_DOWN || event.type == CUI_EVENT_TEXT)
        state->pointer_word_active = false;
    if (event.type == CUI_EVENT_CANCEL) {
        state->dragging = false;
        state->pointer_word_active = false;
    }
    if (cui_event_consumed(ui, i) && event.type == CUI_EVENT_POINTER_UP &&
        event.pointer_button == CUI_POINTER_PRIMARY) {
        state->dragging = false;
        state->pointer_word_active = false;
    }
    if (options.disabled || cui_event_consumed(ui, i) || cui_event_target(ui, i) != id ||
        (event.type == CUI_EVENT_KEY_DOWN && (event.ime_composing || event.prefer_character_input)))
        return result;
    if (!state->composition && event.type >= CUI_EVENT_COMPOSITION_START &&
        event.type <= CUI_EVENT_COMPOSITION_COMMIT) {
        result.error = CUI_EDIT_CAPACITY;
        cui_consume_event(ui, i);
        return result;
    }
    if (state->composition &&
        (event.type == CUI_EVENT_CANCEL || (event.type >= CUI_EVENT_COMPOSITION_START &&
                                            event.type <= CUI_EVENT_COMPOSITION_CANCEL))) {
        result.error = cui_input_checked(edit,
                                         (cui_edit_request){.kind = CUI_EDIT_REQUEST_COMPOSITION,
                                                            .composition = state->composition,
                                                            .event = event,
                                                            .editable = !options.readonly},
                                         options, &result.rejected);
        /* Host COMMIT ends the native/browser session even if acceptance
         * rejected the document edit. Engine rollback is a separate operation. */
        if ((result.rejected || result.error != CUI_EDIT_OK) &&
            event.type == CUI_EVENT_COMPOSITION_COMMIT)
            (void)cui_edit_composition_event(
                edit, state->composition, (cui_event){.type = CUI_EVENT_COMPOSITION_CANCEL}, false);
        result.changed = before.revision != cui_edit_get_state(edit).revision;
        cui_consume_event(ui, i);
        return result;
    }
    if (state->composition && state->composition->active) {
        if (event.type == CUI_EVENT_KEY_DOWN || event.type == CUI_EVENT_TEXT) {
            cui_consume_event(ui, i);
            return result;
        }
        if (event.type == CUI_EVENT_POINTER_DOWN)
            (void)cui_edit_composition_event(
                edit, state->composition, (cui_event){.type = CUI_EVENT_COMPOSITION_CANCEL}, true);
    }
    if (options.delegate_up_down && event.type == CUI_EVENT_KEY_DOWN &&
        (event.key == CUI_KEY_UP || event.key == CUI_KEY_DOWN))
        return result;
    /* The pinned single-line Input has no vertical text rows. Composite
     * delegation above keeps Up/Down available to popup/list owners. */
    if (!before.multiline && event.type == CUI_EVENT_KEY_DOWN &&
        (event.key == CUI_KEY_UP || event.key == CUI_KEY_DOWN)) {
        cui_consume_event(ui, i);
        return result;
    }
    if (options.delegate_home_end && event.type == CUI_EVENT_KEY_DOWN &&
        (event.key == CUI_KEY_HOME || event.key == CUI_KEY_END))
        return result;
    if (event.type == CUI_EVENT_KEY_DOWN &&
        (event.key == CUI_KEY_LEFT || event.key == CUI_KEY_RIGHT || event.key == CUI_KEY_UP ||
         event.key == CUI_KEY_DOWN || event.key == CUI_KEY_HOME || event.key == CUI_KEY_END))
        cui_consume_event(ui, i);
    cui_edit_result error = CUI_EDIT_OK;
    bool primary = event.pointer_button == CUI_POINTER_PRIMARY;
    if (primary && event.type == CUI_EVENT_POINTER_DOWN) {
        state->dragging = true;
        state->pointer_word_active = false;
    }
    if (options.hit_test && primary &&
        (event.type == CUI_EVENT_POINTER_DOWN ||
         ((state->dragging || cui_pressed(ui, id)) &&
          (event.type == CUI_EVENT_POINTER_MOVE || event.type == CUI_EVENT_POINTER_UP)))) {
        cui_edit_state current = cui_edit_get_state(edit);
        size_t cursor =
            options.hit_test(options.hit_test_user, cui_edit_get_text(edit), event.x, event.y);
        size_t snapped;
        if (event.type != CUI_EVENT_POINTER_DOWN &&
            cui_edit_token_boundary(cui_edit_get_content(edit), cursor, false, &snapped) ==
                CUI_EDIT_OK &&
            snapped == current.cursor)
            cursor = snapped;
        if (options.masked && event.type == CUI_EVENT_POINTER_DOWN && event.click_count >= 2) {
            error = cui_edit_select_atomic(edit, 0, current.length);
            state->dragging = false;
            result.error = error;
            return result;
        }
        size_t anchor = event.type == CUI_EVENT_POINTER_DOWN && !(event.modifiers & CUI_MOD_SHIFT)
                            ? cursor
                            : current.anchor;
        if (!current.multiline && event.type == CUI_EVENT_POINTER_DOWN && event.click_count >= 3) {
            /* Single-line source line selection; dragging then moves its end. */
            anchor = 0;
            cursor = current.length;
        } else if (!current.multiline && event.type == CUI_EVENT_POINTER_DOWN &&
                   event.click_count == 2) {
            size_t start, end;
            if (!cui_input_pointer_word(cui_edit_get_text(edit), cursor, &start, &end)) {
                state->dragging = false;
                return result; /* Source end-of-value double-click is a no-op. */
            }
            anchor = start;
            cursor = end;
            state->pointer_word_start = start;
            state->pointer_word_end = end;
            state->pointer_word_revision = current.revision;
            state->pointer_word_active = true;
        } else if (state->pointer_word_active) {
            if (cursor < state->pointer_word_start) {
                anchor = state->pointer_word_end;
            } else {
                anchor = state->pointer_word_start;
                if (cursor < state->pointer_word_end)
                    cursor = state->pointer_word_end;
            }
        }
        error = cui_edit_select_atomic(edit, anchor, cursor);
        if (error != CUI_EDIT_OK)
            state->pointer_word_active = false;
    } else if (event.type == CUI_EVENT_KEY_DOWN &&
               (event.modifiers & (CUI_MOD_CTRL | CUI_MOD_META)) &&
               (event.key == CUI_KEY_C || event.key == CUI_KEY_X || event.key == CUI_KEY_V)) {
        const cui_platform *host = options.clipboard;
        cui_edit_state current = cui_edit_get_state(edit);
        size_t start = current.anchor < current.cursor ? current.anchor : current.cursor;
        size_t end = current.anchor > current.cursor ? current.anchor : current.cursor;
        bool paste = event.key == CUI_KEY_V;
        if (options.masked && !paste) {
            cui_consume_event(ui, i);
            return result;
        }
        if ((paste && !options.readonly) || (!paste && start != end)) {
            if (!host || !(host->capabilities & CUI_PLATFORM_CLIPBOARD) ||
                (paste ? !host->clipboard_get : !host->clipboard_set))
                error = CUI_EDIT_CLIPBOARD_UNAVAILABLE;
            else if (paste)
                error =
                    cui_input_checked(edit,
                                      (cui_edit_request){.kind = CUI_EDIT_REQUEST_INSERT,
                                                         .text = host->clipboard_get(host->user)},
                                      options, &result.rejected);
            else {
                cui_str text = cui_edit_get_text(edit);
                if (!host->clipboard_set(host->user, (cui_str){text.data + start, end - start}))
                    error = CUI_EDIT_CLIPBOARD_UNAVAILABLE;
                else if (event.key == CUI_KEY_X && !options.readonly)
                    error = cui_input_checked(
                        edit, (cui_edit_request){.kind = CUI_EDIT_REQUEST_DELETE_SELECTION},
                        options, &result.rejected);
            }
        }
    } else if (!options.readonly || cui_input_readonly_event(event))
        error = cui_input_checked(
            edit, (cui_edit_request){.kind = CUI_EDIT_REQUEST_EVENT, .event = event}, options,
            &result.rejected);
    if (primary && event.type == CUI_EVENT_POINTER_UP) {
        state->dragging = false;
        state->pointer_word_active = false;
    }
    if (!before.multiline && event.type == CUI_EVENT_KEY_DOWN && event.key == CUI_KEY_ENTER &&
        !event.repeat)
        result.submitted = true;
    result.error = error;
    result.changed = before.revision != cui_edit_get_state(edit).revision;
    return result;
}
static void cui_input_notify(cui_context *ui, cui_id id, cui_edit *edit,
                             cui_base_input_options options, cui_component_text_handler handler,
                             cui_edit_state before, cui_edit_state after, cui_event_type type,
                             size_t index, cui_component_event_kind kind,
                             cui_input_event_result *result) {
    cui_component_text_event event = {.before = before, .after = after};
    cui_component_event_source source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN;
    if (type == CUI_EVENT_KEY_DOWN || type == CUI_EVENT_KEY_UP)
        source = CUI_COMPONENT_EVENT_SOURCE_KEYBOARD;
    else if (type == CUI_EVENT_POINTER_DOWN || type == CUI_EVENT_POINTER_UP ||
             type == CUI_EVENT_POINTER_MOVE)
        source = CUI_COMPONENT_EVENT_SOURCE_POINTER;
    (void)cui_component_event_init(&event.event, kind, id, source);
    (void)cui_component_event_set_frame_index(&event.event, cui_frame_sequence(ui), index);
    event.has_text = !options.masked && options.content_type == CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED;
    if (event.has_text)
        event.text = cui_edit_get_text(edit);
    size_t *count =
        kind == CUI_COMPONENT_EVENT_CHANGE ? &result->change_count : &result->submit_count;
    if (*count == SIZE_MAX)
        result->count_overflow = true;
    else
        ++*count;
    handler.callback(handler.user, &event);
}
cui_input_event_result
cui_base_input_reduce_event_events(cui_context *ui, cui_id id, cui_edit *edit,
                                   cui_input_behavior_state *state, cui_base_input_options options,
                                   size_t index, cui_component_text_handler handler) {
    cui_input_event_result result = {0};
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    bool snapshot = ui && id && edit && state && events && index < count;
    cui_edit_state before = {0};
    cui_event_type type = (cui_event_type)0;
    if (snapshot) {
        before = cui_edit_get_state(edit);
        type = events[index].type;
    }
    result.control = cui_base_input_reduce_event(ui, id, edit, state, options, index);
    if (!snapshot || !handler.callback)
        return result;
    cui_edit_state after = cui_edit_get_state(edit);
    if (result.control.changed && before.revision != after.revision)
        cui_input_notify(ui, id, edit, options, handler, before, after, type, index,
                         CUI_COMPONENT_EVENT_CHANGE, &result);
    if (result.control.submitted)
        cui_input_notify(ui, id, edit, options, handler, before, after, type, index,
                         CUI_COMPONENT_EVENT_SUBMIT, &result);
    return result;
}
static void cui_input_add_count(size_t *count, size_t added, bool *overflow) {
    if (added > SIZE_MAX - *count) {
        *count = SIZE_MAX;
        *overflow = true;
    } else
        *count += added;
}
static cui_input_result cui_input_events_common(cui_context *ui, cui_id id, cui_str name,
                                                cui_edit *edit, cui_input_behavior_state *state,
                                                cui_base_input_options options,
                                                cui_component_text_handler handler,
                                                cui_input_event_result *notifications) {
    cui_input_result result = {.error = CUI_EDIT_OK};
    if (!edit || !state || !ui || !id) {
        result.error = CUI_EDIT_INVALID_ARGUMENT;
        return result;
    }
    cui_edit_state before = cui_edit_get_state(edit);
    (void)cui_interact(ui, id, CUI_ROLE_TEXTBOX, name, options.disabled, false,
                       (float)before.length);
    if (options.content_type < CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED ||
        options.content_type > CUI_TEXT_INPUT_PURPOSE_NEW_PASSWORD ||
        ((options.masked || options.content_type != CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED) &&
         (cui_edit_get_state(edit).multiline || cui_edit_get_content(edit).token_count)))
        return (cui_input_result){.error = CUI_EDIT_INVALID_ARGUMENT};
    cui_edit_checked_result configured =
        cui_edit_apply_checked(edit,
                               (cui_edit_request){.kind = CUI_EDIT_REQUEST_PRESENT,
                                                  .format = options.format,
                                                  .format_workspace = options.format_workspace},
                               (cui_edit_acceptance){0});
    if (configured.error != CUI_EDIT_OK)
        return (cui_input_result){.error = configured.error};
    /* Owned sessions may have expired during an omitted/rejected submission.
     * Reacquire without dereferencing the old pointer before lifecycle checks. */
    state->composition =
        cui_base_composition(ui, id, state->composition, &state->composition_owned);
    cui_input_secure_lifecycle(edit, state, options);
    cui_input_history_focus(ui, id, edit, state, options.disabled);
    if (state->composition && state->composition->active)
        (void)cui_edit_composition_event(edit, state->composition,
                                         (cui_event){.type = CUI_EVENT_GESTURE}, true);
    if (state->composition && (options.disabled || options.readonly || cui_focused(ui) != id))
        (void)cui_edit_composition_event(edit, state->composition,
                                         (cui_event){.type = CUI_EVENT_COMPOSITION_CANCEL}, false);
    size_t count = 0;
    (void)cui_frame_events(ui, &count);
    if (options.disabled)
        state->dragging = false;
    for (size_t i = 0; i < count; ++i) {
        cui_input_result event;
        if (notifications) {
            cui_input_event_result observed =
                cui_base_input_reduce_event_events(ui, id, edit, state, options, i, handler);
            event = observed.control;
            cui_input_add_count(&notifications->change_count, observed.change_count,
                                &notifications->count_overflow);
            cui_input_add_count(&notifications->submit_count, observed.submit_count,
                                &notifications->count_overflow);
            notifications->count_overflow |= observed.count_overflow;
        } else
            event = cui_base_input_reduce_event(ui, id, edit, state, options, i);
        result.submitted |= event.submitted;
        result.rejected |= event.rejected;
        if (result.error == CUI_EDIT_OK && event.error != CUI_EDIT_OK)
            result.error = event.error;
    }
    cui_edit_state after = cui_edit_get_state(edit);
    result.changed = before.revision != after.revision;
    bool secret = options.masked || options.content_type != CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED;
    cui_update_semantic(ui, id, false, secret ? 0 : (float)after.length);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.readonly = options.readonly,
                               .required = options.required,
                               .invalid = options.invalid || result.error != CUI_EDIT_OK,
                               .multiline = after.multiline,
                               .secret = secret,
                               .password =
                                   options.content_type != CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED});
    (void)cui_set_text_input_config(
        ui, id,
        (cui_text_input_config){.type = options.masked ? CUI_TEXT_INPUT_PASSWORD_HIDDEN
                                        : options.content_type != CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED
                                            ? CUI_TEXT_INPUT_PASSWORD_VISIBLE
                                            : CUI_TEXT_INPUT_TEXT,
                                .purpose = options.content_type});
    return result;
}
cui_input_result cui_base_input_ex(cui_context *ui, cui_id id, cui_str name, cui_edit *edit,
                                   cui_input_behavior_state *state,
                                   cui_base_input_options options) {
    return cui_input_events_common(ui, id, name, edit, state, options,
                                   (cui_component_text_handler){0}, NULL);
}
cui_input_event_result cui_base_input_events(cui_context *ui, cui_id id, cui_str name,
                                             cui_edit *edit, cui_input_behavior_state *state,
                                             cui_base_input_options options,
                                             cui_component_text_handler handler) {
    cui_input_event_result result = {0};
    result.control = cui_input_events_common(ui, id, name, edit, state, options, handler, &result);
    return result;
}

cui_input_result cui_base_input(cui_context *ui, cui_id id, cui_str name, cui_edit *edit,
                                bool disabled) {
    /* This legacy wrapper has no retained lifecycle state. Preserve its edit
     * engine coalescing; stateful ex/styled callers track focus transitions. */
    cui_input_behavior_state state = {.bound_edit_identity = (uintptr_t)edit,
                                      .focus_initialized = true,
                                      .was_focused = ui && !disabled && cui_focused(ui) == id};
    return cui_base_input_ex(ui, id, name, edit, &state,
                             (cui_base_input_options){.disabled = disabled});
}

size_t cui_base_input_visual_hit_test(void *user, cui_str text, float x, float y) {
    cui_input_visual_hit_geometry *hit = user;
    if (!hit || !hit->visual || !hit->visual->ops || !hit->visual->ops->hit_test ||
        (!text.data && text.length) || text.length != hit->visual->byte_length || !isfinite(x) ||
        !isfinite(y))
        return SIZE_MAX;
    cui_text_caret caret;
    if (hit->visual->ops->hit_test(
            hit->visual->session, x - hit->measured.viewport.x + hit->measured.scroll_x,
            y - hit->measured.viewport.y, CUI_TEXT_DOWNSTREAM, &caret) != CUI_TEXT_LAYOUT_OK ||
        caret.byte_offset > text.length || !isfinite(caret.x) || !isfinite(caret.y) ||
        !isfinite(caret.height) || caret.height < 0 ||
        (unsigned)caret.affinity > CUI_TEXT_DOWNSTREAM ||
        (caret.byte_offset < text.length &&
         ((unsigned char)text.data[caret.byte_offset] & 0xc0) == 0x80))
        return SIZE_MAX;
    if (hit->affinity)
        *hit->affinity = caret.affinity;
    return caret.byte_offset;
}
size_t cui_base_input_hit_test(void *user, cui_str text, float x, float y) {
    cui_input_hit_geometry *hit = user;
    if (!hit || !hit->ui || !isfinite(x) || !isfinite(y) || !isfinite(hit->font_size) ||
        hit->font_size <= 0 || !cui_edit_valid_utf8(text))
        return SIZE_MAX;
    cui_text_geometry visual;
    if (cui_get_text_geometry(hit->ui, text, hit->font_size, (cui_text_style){0}, &visual) ==
        CUI_TEXT_LAYOUT_OK) {
        cui_input_visual_hit_geometry geometry = {*hit, &visual, NULL};
        size_t byte = cui_base_input_visual_hit_test(&geometry, text, x, y);
        cui_release_text_geometry(&visual);
        if (byte != SIZE_MAX)
            return byte;
    }
    return cui_base_input_measured_hit_test(user, text, x, y);
}
size_t cui_base_input_measured_hit_test(void *user, cui_str text, float x, float y) {
    (void)y;
    cui_input_hit_geometry *hit = user;
    if (!hit || !hit->ui || !isfinite(x) || !isfinite(hit->font_size) || hit->font_size <= 0 ||
        !cui_edit_valid_utf8(text))
        return SIZE_MAX;
    float target = x - hit->viewport.x + hit->scroll_x;
    if (target <= 0 || !text.length)
        return 0;
    size_t low = 0, high = text.length;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        while (middle > low && ((unsigned char)text.data[middle] & 0xc0) == 0x80)
            middle--;
        size_t next = middle + 1;
        while (next < text.length && ((unsigned char)text.data[next] & 0xc0) == 0x80)
            next++;
        float width = cui_measure_text(hit->ui, (cui_str){text.data, next}, hit->font_size).width;
        if (width <= target)
            low = next;
        else
            high = middle;
    }
    if (low == text.length)
        return low;
    size_t next = low + 1;
    while (next < text.length && ((unsigned char)text.data[next] & 0xc0) == 0x80)
        next++;
    float left = cui_measure_text(hit->ui, (cui_str){text.data, low}, hit->font_size).width;
    float right = cui_measure_text(hit->ui, (cui_str){text.data, next}, hit->font_size).width;
    return target < (left + right) * .5f ? low : next;
}

bool cui_progress_circle_measure(cui_progress_geometry progress, float diameter,
                                 cui_progress_circle_geometry *out) {
    if (!out || !isfinite(diameter) || diameter < 1 || diameter > 4096 ||
        !isfinite(progress.start) || !isfinite(progress.length) || progress.start < 0 ||
        progress.length < 0 || progress.start + progress.length > 1)
        return false;
    float stroke = fminf(diameter * .15f, 5);
    *out = (cui_progress_circle_geometry){diameter,
                                          stroke,
                                          diameter * .5f - stroke,
                                          diameter * .5f,
                                          progress.start * 6.28318530718f,
                                          (progress.start + progress.length) * 6.28318530718f};
    return true;
}
bool cui_progress_circle_raster(cui_progress_circle_geometry geometry, cui_color color, float scale,
                                unsigned char *pixels, size_t capacity, cui_bitmap *out) {
    const float tau = 6.28318530718f;
    if (!out || !pixels || !isfinite(scale) || scale <= 0 || scale > 4 ||
        !isfinite(geometry.diameter) || geometry.diameter < 1 || geometry.diameter > 4096 ||
        !isfinite(geometry.inner_radius) || !isfinite(geometry.outer_radius) ||
        geometry.inner_radius < 0 || geometry.outer_radius > geometry.diameter * .5f ||
        geometry.outer_radius <= geometry.inner_radius || !isfinite(geometry.start_angle) ||
        !isfinite(geometry.end_angle) || geometry.start_angle < 0 ||
        geometry.end_angle < geometry.start_angle || geometry.end_angle > tau)
        return false;
    unsigned extent = (unsigned)ceilf(geometry.diameter * scale);
    if (extent > 16384 || extent > SIZE_MAX / extent / 4 || capacity < (size_t)extent * extent * 4)
        return false;
    float alpha = color.a / 255.f;
    float track = alpha * .2f;
    float filled = alpha + track * (1 - alpha);
    for (unsigned y = 0; y < extent; y++) {
        for (unsigned x = 0; x < extent; x++) {
            float sum = 0;
            for (unsigned sy = 0; sy < 4; sy++) {
                for (unsigned sx = 0; sx < 4; sx++) {
                    float dx = (x + (sx + .5f) / 4) / scale - geometry.diameter * .5f;
                    float dy = (y + (sy + .5f) / 4) / scale - geometry.diameter * .5f;
                    float radius = sqrtf(dx * dx + dy * dy);
                    if (radius < geometry.inner_radius || radius > geometry.outer_radius)
                        continue;
                    float angle = atan2f(dy, dx) + tau * .25f;
                    if (angle < 0)
                        angle += tau;
                    bool in_arc = geometry.end_angle > geometry.start_angle &&
                                  angle >= geometry.start_angle && angle <= geometry.end_angle;
                    sum += in_arc ? filled : track;
                }
            }
            size_t offset = ((size_t)y * extent + x) * 4;
            pixels[offset] = color.r;
            pixels[offset + 1] = color.g;
            pixels[offset + 2] = color.b;
            pixels[offset + 3] = (unsigned char)lroundf(sum * 255 / 16);
        }
    }
    *out = (cui_bitmap){pixels, extent, extent, (size_t)extent * 4, 1};
    return true;
}

cui_edit_composition *cui_base_composition(cui_context *ui, cui_id id, cui_edit_composition *c,
                                           bool *owned) {
    if (!ui || !owned || (c && !*owned))
        return c;
    bool needed = *owned;
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count && !needed; i++)
        needed = cui_event_target(ui, i) == id && events[i].type >= CUI_EVENT_COMPOSITION_START &&
                 events[i].type <= CUI_EVENT_COMPOSITION_COMMIT;
    if (!needed)
        return NULL;
    typedef struct {
        cui_edit_composition session;
        char preedit[4096], view[65536];
    } storage;
    storage *r =
        cui_resource_get(ui, id, UINT64_C(0xd8c8d89f55a451e1), sizeof(storage), NULL, NULL);
    if (!r)
        return NULL;
    if (!r->session.buffer) {
        r->session.buffer = r->preedit;
        r->session.capacity = sizeof(r->preedit);
        r->session.view_buffer = r->view;
        r->session.view_capacity = sizeof(r->view);
    }
    *owned = true;
    return &r->session;
}
