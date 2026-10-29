/* SPDX-License-Identifier: MIT */
#include "cui_form_private.h"
#include <math.h>
static size_t cui_form_private_columns(cui_form_options o) {
    return o.columns ? o.columns : 1;
}
static float cui_form_private_width(cui_form_options o) {
    return o.width > 0 ? o.width : 480;
}
static float cui_form_private_gap(cui_form_options o) {
    return o.size == CUI_FORM_LARGE                                ? 12
           : o.size == CUI_FORM_SMALL || o.size == CUI_FORM_XSMALL ? 6
                                                                   : 8;
}
cui_form_options cui_form_options_default(void) {
    return (cui_form_options){.columns = 1, .label_width = 140};
}
cui_id cui_form_label_id(cui_id id) {
    return id ^ UINT64_C(0x3a468db90d956242);
}
cui_id cui_form_description_id(cui_id id) {
    return id ^ UINT64_C(0xd29c8f326b970fea);
}
static int cui_form_private_line(int value, size_t n) {
    return value < 0 ? (int)n + 2 + value : value;
}
static bool cui_form_private_string_valid(cui_str s) {
    return !s.length || s.data;
}
cui_form_error cui_form_plan(const cui_form_field *f, size_t count, cui_form_options o,
                             cui_form_cell *cells, size_t capacity, size_t *rows) {
    size_t n = cui_form_private_columns(o);
    if ((count && (!f || !cells)) || !rows || n > 64 || count > 256 || !isfinite(o.width) ||
        !isfinite(o.label_width) || !isfinite(o.label_text_size) || !isfinite(o.gap_x) ||
        !isfinite(o.gap_y) || o.width < 0 || o.label_width < 0 || o.label_text_size < 0 ||
        o.gap_x < 0 || o.gap_y < 0 || o.size < CUI_FORM_MEDIUM || o.size > CUI_FORM_LARGE ||
        !cui_form_private_string_valid(o.name))
        return CUI_FORM_INVALID;
    if (capacity < count)
        return CUI_FORM_CAPACITY;
    float gx = o.gap_x > 0 ? o.gap_x : cui_form_private_gap(o) * 3;
    if (cui_form_private_width(o) <= gx * (n - 1))
        return CUI_FORM_INVALID;
    for (size_t i = 0; i < count; i++) {
        if (!f[i].id || !cui_form_private_string_valid(f[i].label) ||
            !cui_form_private_string_valid(f[i].description) ||
            !cui_form_private_string_valid(f[i].error) || f[i].align < CUI_ALIGN_START ||
            f[i].align > CUI_ALIGN_END)
            return CUI_FORM_INVALID;
        int start = cui_form_private_line(f[i].column_start, n),
            end = cui_form_private_line(f[i].column_end, n);
        size_t span = f[i].column_span ? f[i].column_span : 1;
        if (span > n || (start && (start < 1 || start > (int)n)) ||
            (end && (end < 2 || end > (int)n + 1)) || (start && end && end <= start) ||
            (end && !start && end - (int)span < 1) ||
            (start && !end && (size_t)(start - 1) + span > n))
            return CUI_FORM_INVALID;
        for (size_t j = 0; j < i; j++)
            if (f[j].id == f[i].id)
                return CUI_FORM_DUPLICATE_ID;
    }
    size_t row = 0, cursor = 0, visible = 0;
    float unit = (cui_form_private_width(o) - gx * (n - 1)) / n;
    for (size_t i = 0; i < count; i++) {
        cells[i] = (cui_form_cell){0};
        if (f[i].hidden)
            continue;
        int start = cui_form_private_line(f[i].column_start, n),
            end = cui_form_private_line(f[i].column_end, n);
        size_t span = f[i].column_span ? f[i].column_span : 1;
        if (start && end)
            span = (size_t)(end - start);
        else if (end)
            start = end - (int)span;
        size_t col = start ? (size_t)(start - 1) : cursor;
        if (col < cursor || col + span > n) {
            row++;
            cursor = 0;
            col = start ? (size_t)(start - 1) : 0;
        }
        cells[i] = (cui_form_cell){row, col, span, unit * span + gx * (span - 1), true};
        cursor = col + span;
        visible++;
    }
    *rows = visible ? row + 1 : 0;
    return CUI_FORM_OK;
}
void cui_form_associate_control(cui_context *ui, const cui_form_field *f) {
    if (!ui || !f || !f->control)
        return;
    size_t count;
    const cui_semantic *sem = cui_semantics(ui, &count);
    for (size_t i = 0; i < count; i++)
        if (sem[i].id == f->control) {
            cui_semantic_details d = sem[i].details;
            d.parent = f->id;
            d.labelled_by = !f->no_label_indent && (f->label.length || f->render_label)
                                ? cui_form_label_id(f->id)
                                : 0;
            d.required |= f->required;
            d.invalid |= f->invalid;
            d.readonly |= f->readonly;
            d.description = f->invalid && f->error.length ? f->error : f->description;
            cui_set_semantic_details(ui, f->control, d);
            return;
        }
}
cui_form_error cui_base_form(cui_context *ui, cui_id id, const cui_form_field *f, size_t count,
                             cui_form_options o) {
    cui_form_cell cells[256];
    size_t rows;
    cui_form_error e = cui_form_plan(f, count, o, cells, 256, &rows);
    if (e != CUI_FORM_OK)
        return e;
    if (!ui || !id)
        return CUI_FORM_INVALID;
    for (size_t i = 0; i < count; i++)
        if (f[i].id == id)
            return CUI_FORM_DUPLICATE_ID;
    (void)cui_interact(ui, id, CUI_ROLE_GROUP, o.name, false, false, 0);
    for (size_t i = 0; i < count; i++)
        if (!f[i].hidden) {
            (void)cui_interact(ui, f[i].id, CUI_ROLE_GROUP, f[i].label, f[i].disabled, false, 0);
            cui_set_semantic_details(
                ui, f[i].id,
                (cui_semantic_details){.parent = id,
                                       .required = f[i].required,
                                       .invalid = f[i].invalid,
                                       .readonly = f[i].readonly,
                                       .controls = f[i].control,
                                       .description = f[i].invalid && f[i].error.length
                                                          ? f[i].error
                                                          : f[i].description});
            if (!f[i].no_label_indent && (f[i].label.length || f[i].render_label)) {
                (void)cui_interact(ui, cui_form_label_id(f[i].id), CUI_ROLE_GROUP, f[i].label,
                                   false, false, 0);
                cui_set_semantic_details(ui, cui_form_label_id(f[i].id),
                                         (cui_semantic_details){.parent = f[i].id});
            }
            cui_form_associate_control(ui, &f[i]);
        }
    return CUI_FORM_OK;
}
static bool cui_form_private_group_valid(cui_id group, cui_id input, const cui_input_group_addon *a,
                                         size_t count, cui_input_group_options o) {
    if (!group || !input || group == input || count > 64 || (count && !a) ||
        !cui_form_private_string_valid(o.name) || !cui_form_private_string_valid(o.placeholder) ||
        !isfinite(o.width) || !isfinite(o.font_size) || o.width < 0 || o.font_size < 0 ||
        o.size < CUI_FORM_MEDIUM || o.size > CUI_FORM_LARGE)
        return false;
    for (size_t i = 0; i < count; i++) {
        if (!a[i].id || a[i].id == group || a[i].id == input ||
            !cui_form_private_string_valid(a[i].label) || !isfinite(a[i].width) || a[i].width < 0 ||
            a[i].alignment < CUI_INPUT_GROUP_INLINE_START ||
            a[i].alignment > CUI_INPUT_GROUP_BLOCK_END || a[i].kind < CUI_INPUT_GROUP_TEXT ||
            a[i].kind > CUI_INPUT_GROUP_CUSTOM || a[i].variant < CUI_INPUT_GROUP_GHOST ||
            a[i].variant > CUI_INPUT_GROUP_DANGER)
            return false;
        for (size_t j = 0; j < i; j++)
            if (a[j].id == a[i].id)
                return false;
    }
    return true;
}
cui_input_group_result cui_form_private_input_group(
    cui_context *ui, cui_id group, cui_id input, cui_edit *edit, cui_input_group_state *state,
    const cui_input_group_addon *a, size_t count, cui_input_group_options o,
    cui_input_group_handler handler, cui_input_group_event_result *notifications,
    cui_form_private_input_fn input_fn, void *input_user) {
    cui_input_group_result result = {.input.error = CUI_EDIT_OK};
    if (!ui || !edit || !state || !cui_form_private_group_valid(group, input, a, count, o)) {
        result.error = CUI_FORM_INVALID;
        result.input.error = CUI_EDIT_INVALID_ARGUMENT;
        return result;
    }
    (void)cui_interact(ui, group, CUI_ROLE_GROUP, o.name, o.input.disabled, false, 0);
    cui_set_semantic_details(ui, group,
                             (cui_semantic_details){.readonly = o.input.readonly,
                                                    .invalid = o.input.invalid,
                                                    .controls = input,
                                                    .focus_target = input});
    for (size_t i = 0; i < count; i++) {
        bool disabled = o.input.disabled || a[i].disabled || a[i].loading;
        if (a[i].kind == CUI_INPUT_GROUP_BUTTON) {
            cui_interaction button = cui_base_button(ui, a[i].id, a[i].label, disabled);
            if (button.activated)
                result.activated_addon = a[i].id;
        } else
            (void)cui_interact(ui, a[i].id, CUI_ROLE_GROUP, a[i].label, disabled, false, 0);
        cui_set_semantic_details(
            ui, a[i].id,
            (cui_semantic_details){.parent = group,
                                   .busy = a[i].loading,
                                   .focus_target =
                                       a[i].kind == CUI_INPUT_GROUP_BUTTON ? 0 : input});
        if (notifications && handler.on_addon && !disabled && a[i].kind == CUI_INPUT_GROUP_BUTTON &&
            cui_frame_paint_enabled(ui) && cui_frame_sequence(ui)) {
            size_t event_count;
            const cui_event *events = cui_frame_events(ui, &event_count);
            for (size_t e = 0; e < event_count; ++e) {
                if (cui_event_accepted_activation(ui, e) != a[i].id)
                    continue;
                cui_component_event_metadata event;
                cui_activation_snapshot snapshot;
                if (!cui_event_activation_snapshot(ui, e, &snapshot) ||
                    !cui_component_event_from_activation(&event, CUI_COMPONENT_EVENT_ACTIVATE,
                                                         &snapshot)) {
                    cui_component_event_source source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN;
                    if (events[e].type == CUI_EVENT_KEY_DOWN || events[e].type == CUI_EVENT_KEY_UP)
                        source = CUI_COMPONENT_EVENT_SOURCE_KEYBOARD;
                    else if (events[e].type == CUI_EVENT_POINTER_UP)
                        source = CUI_COMPONENT_EVENT_SOURCE_POINTER;
                    else if (events[e].type == CUI_EVENT_ACCESSIBILITY_ACTION)
                        source = CUI_COMPONENT_EVENT_SOURCE_ACCESSIBILITY;
                    (void)cui_component_event_init(&event, CUI_COMPONENT_EVENT_ACTIVATE, a[i].id,
                                                   source);
                    (void)cui_component_event_set_frame_index(&event, cui_frame_sequence(ui), e);
                }
                if (notifications->addon_count == SIZE_MAX)
                    notifications->count_overflow = true;
                else
                    ++notifications->addon_count;
                handler.on_addon(handler.user, &event);
            }
        }
    }
    cui_base_input_options behavior = o.input;
    cui_input_hit_geometry hit = {ui, state->viewport, o.font_size > 0 ? o.font_size : 14,
                                  state->scroll_x};
    bool synthetic_hit = !behavior.hit_test && !cui_edit_get_state(edit).multiline;
    if (synthetic_hit) {
        behavior.hit_test = cui_base_input_hit_test;
        behavior.hit_test_user = &hit;
    }
    if (input_fn || (notifications && handler.on_text)) {
        cui_component_text_handler text_handler = {notifications ? handler.on_text : NULL,
                                                   handler.user};
        cui_input_event_result observed =
            input_fn ? input_fn(input_user, ui, input, o.name, edit, &state->behavior, behavior,
                                synthetic_hit, text_handler)
                     : cui_base_input_events(ui, input, o.name, edit, &state->behavior, behavior,
                                             text_handler);
        result.input = observed.control;
        if (notifications) {
            notifications->change_count = observed.change_count;
            notifications->submit_count = observed.submit_count;
            notifications->count_overflow |= observed.count_overflow;
        }
    } else
        result.input = cui_base_input_ex(ui, input, o.name, edit, &state->behavior, behavior);
    size_t sc;
    const cui_semantic *sem = cui_semantics(ui, &sc);
    for (size_t i = 0; i < sc; i++)
        if (sem[i].id == input) {
            cui_semantic_details d = sem[i].details;
            d.parent = group;
            cui_set_semantic_details(ui, input, d);
            break;
        }
    return result;
}

cui_input_group_result cui_base_input_group(cui_context *ui, cui_id group, cui_id input,
                                            cui_edit *edit, cui_input_group_state *state,
                                            const cui_input_group_addon *addons, size_t count,
                                            cui_input_group_options options) {
    return cui_form_private_input_group(ui, group, input, edit, state, addons, count, options,
                                        (cui_input_group_handler){0}, NULL, NULL, NULL);
}
cui_input_group_event_result
cui_base_input_group_events(cui_context *ui, cui_id group, cui_id input, cui_edit *edit,
                            cui_input_group_state *state, const cui_input_group_addon *addons,
                            size_t count, cui_input_group_options options,
                            cui_input_group_handler handler) {
    cui_input_group_event_result result = {0};
    result.control = cui_form_private_input_group(ui, group, input, edit, state, addons, count,
                                                  options, handler, &result, NULL, NULL);
    return result;
}
