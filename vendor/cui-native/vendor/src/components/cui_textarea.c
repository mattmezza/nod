/* SPDX-License-Identifier: MIT */
#include "cui/cui_textarea.h"
#include "../base/cui_textarea_geometry.h"
#include "cui/cui_base.h"
#include "cui/cui_multiline_events.h"
#include "cui/cui_scroll.h"
#include <math.h>
static cui_id cui_textarea_child(cui_id id, size_t row, unsigned part) {
    return id ^ UINT64_C(0x7cf5d0e25db29861) ^
           ((uint64_t)(row + 1) * UINT64_C(0x9e3779b97f4a7c15)) ^ part;
}
static size_t cui_textarea_visible_rows(cui_textarea_options options, size_t count) {
    if (!options.auto_grow)
        return options.rows ? options.rows : 1;
    size_t minimum = options.min_rows ? options.min_rows : 1;
    size_t maximum = options.max_rows ? options.max_rows : 10;
    if (maximum < minimum)
        maximum = minimum;
    return count < minimum ? minimum : count > maximum ? maximum : count;
}
static float cui_textarea_prefix(cui_context *ui, cui_str text, cui_textarea_line line,
                                 size_t offset, float font, cui_text_style style) {
    return cui_textarea_visual_x(ui, text, line, offset, font, style, CUI_TEXT_DOWNSTREAM);
}
static cui_textarea_result
cui_textarea_common(cui_context *ui, cui_id id, const cui_theme *theme, cui_str name,
                    cui_edit *edit, cui_textarea_state *state, cui_textarea_options options,
                    cui_component_text_handler handler, cui_multiline_event_result *events) {
    cui_textarea_result result = {.error = CUI_TEXTAREA_INVALID};
    cui_textarea_metrics metrics;
    if (!ui || !id || !theme || !edit || !state ||
        (cui_edit_get_content(edit).token_count &&
         !cui_edit_content_valid(cui_edit_get_content(edit))) ||
        !cui_textarea_measure(options, theme->font_size, &metrics) || !isfinite(options.width) ||
        !isfinite(options.height) || options.height < 0 || options.height > 1000000 ||
        options.rows > 65536 || options.min_rows > 65536 || options.max_rows > 65536 ||
        (options.placeholder.length && !options.placeholder.data))
        return result;
    if (options.design_size != CUI_DESIGN_INHERIT) {
        cui_design_metrics design;
        if (!cui_design_measure(options.design_size, theme->font_size / 16, &design))
            return result;
        metrics = (cui_textarea_metrics){design.font_size, design.line_height, design.padding_x,
                                         design.padding_y};
    }
    float border = !options.no_appearance && !options.no_border ? 1 : 0;
    float px = metrics.padding_x + border, py = metrics.padding_y + border;
    float width = options.width > 0       ? options.width - 2 * px
                  : state->viewport.w > 0 ? state->viewport.w
                                          : 280 - 2 * px;
    width = fmaxf(1, width);
    float height =
        options.height > 0
            ? options.height
            : cui_textarea_visible_rows(options, state->count) * metrics.line_height + 2 * py;
    if (!isfinite(height) || height > 1000000)
        return result;
    state->viewport.w = width;
    state->viewport.h = fmaxf(0, height - 2 * py);
    state->composition =
        cui_base_composition(ui, id, state->composition, &state->composition_owned);
    if (state->composition && state->composition_owned)
        state->composition->grow_view = true;
    if (events) {
        *events = cui_base_textarea_events(ui, id, name, edit, state, options, metrics, handler);
        result = events->control;
    } else {
        result = cui_base_textarea(ui, id, name, edit, state, options, metrics);
    }
    if (result.error != CUI_TEXTAREA_OK)
        return result;
    height = options.height > 0
                 ? options.height
                 : cui_textarea_visible_rows(options, state->count) * metrics.line_height + 2 * py;
    if (!isfinite(height) || height > 1000000)
        return (cui_textarea_result){.error = CUI_TEXTAREA_INVALID};
    state->viewport.h = fmaxf(0, height - 2 * py);
    result.error = cui_textarea_layout_ex(ui, edit, state, metrics, width, !options.no_wrap,
                                          options.text_style);
    if (result.error != CUI_TEXTAREA_OK)
        return result;
    cui_scroll_options bars =
        cui_scroll_resolve(theme, (cui_scroll_options){.horizontal = options.no_wrap,
                                                       .vertical = true,
                                                       .disabled = options.disabled,
                                                       .wheel_step = metrics.line_height,
                                                       .scrollbar_style = options.scrollbar_style});
    cui_rect bar_viewport = {0, 0, width, state->viewport.h};
    cui_size bar_content = {width + state->maximum_x, state->viewport.h + state->maximum_y};
    result.scrolled |= cui_base_scrollbar_overlay(ui, id, &state->scroll_x, &state->scroll_y,
                                                  bar_viewport, bar_content, bars);
    bool focused = cui_focused(ui) == id && !options.disabled;
    if ((focused && !state->focused) || result.changed || result.selection_changed)
        state->blink_started = cui_frame_time(ui);
    state->focused = focused;
    bool caret = focused && (cui_reduced_motion(ui) ||
                             fmod(cui_frame_time(ui) - state->blink_started, 1) < .5);
    if (focused && !cui_reduced_motion(ui))
        cui_invalidate(ui);
    cui_color background = theme->input_background;
    if (options.disabled) {
        background = theme->input;
        background.a = (uint8_t)(background.a * .2f);
    }
    cui_color foreground = options.disabled ? theme->muted : theme->text;
    cui_begin_box(ui, id,
                  (cui_box){.width = options.width ? options.width : 280,
                            .height = height,
                            .fixed_height = true,
                            .clip = true,
                            .radius = options.no_appearance ? 0 : theme->radius,
                            .background = options.no_appearance ? (cui_color){0} : background,
                            .border = options.invalid ? theme->danger
                                      : focused       ? theme->focus
                                                      : theme->input,
                            .border_width = border});
    cui_begin_box(ui, cui_textarea_child(id, 0, 1), (cui_box){.height = py, .fixed_height = true});
    cui_end_box(ui);
    cui_begin_box(
        ui, cui_textarea_child(id, 0, 2),
        (cui_box){.width = -1, .row = true, .height = state->viewport.h, .fixed_height = true});
    cui_begin_box(ui, cui_textarea_child(id, 0, 3), (cui_box){.width = px, .fixed_width = true});
    cui_end_box(ui);
    cui_begin_box(ui, cui_textarea_viewport_id(id),
                  (cui_box){.width = -1,
                            .height = state->viewport.h,
                            .fixed_height = true,
                            .clip = true,
                            .offset_x = -state->scroll_x,
                            .offset_y = -state->scroll_y});
    cui_track_bounds(ui, cui_textarea_viewport_id(id), &state->viewport);
    float content_width = width + state->maximum_x;
    cui_begin_box(ui, cui_textarea_child(id, 0, 4),
                  (cui_box){.width = content_width,
                            .fixed_width = true,
                            .height = state->count * metrics.line_height,
                            .fixed_height = true});
    cui_str text = cui_edit_composition_view(edit, state->composition);
    cui_edit_state selection = cui_edit_composition_selection(edit, state->composition);
    size_t first = selection.anchor < selection.cursor ? selection.anchor : selection.cursor;
    size_t last = selection.anchor > selection.cursor ? selection.anchor : selection.cursor;
    if (focused && state->count) {
        cui_textarea_line caret_row = state->lines[state->caret_line];
        float cx = cui_textarea_prefix(ui, text, caret_row, selection.cursor, metrics.font_size,
                                       options.text_style);
        cui_set_text_input_rect(ui, id,
                                (cui_rect){state->viewport.x + cx - state->scroll_x,
                                           state->viewport.y +
                                               state->caret_line * metrics.line_height -
                                               state->scroll_y,
                                           1, metrics.line_height});
    }
    size_t begin = (size_t)floorf(state->scroll_y / metrics.line_height);
    if (begin >= state->count)
        begin = state->count - 1;
    size_t visible = (size_t)ceilf(state->viewport.h / metrics.line_height) + 2;
    size_t end = visible > state->count - begin ? state->count : begin + visible;
    if (begin) {
        cui_begin_box(ui, cui_textarea_child(id, begin, 5),
                      (cui_box){.height = begin * metrics.line_height, .fixed_height = true});
        cui_end_box(ui);
    }
    for (size_t row = begin; row < end; row++) {
        cui_textarea_line line = state->lines[row];
        cui_text_geometry owned_geometry = {0};
        const cui_text_geometry *line_geometry = NULL;
        cui_text_layout_status visual_status = cui_textarea_visual_geometry(
            ui, text, line, metrics.font_size, options.text_style, &owned_geometry, &line_geometry);
        if (visual_status != CUI_TEXT_LAYOUT_OK && result.visual_error == CUI_TEXT_LAYOUT_OK)
            result.visual_error = visual_status;
        cui_release_text_geometry(&owned_geometry);
        cui_begin_box(ui, cui_textarea_child(id, row, 10),
                      (cui_box){.width = content_width,
                                .fixed_width = true,
                                .height = metrics.line_height,
                                .fixed_height = true});
        if (first < line.next && last > line.start &&
            !cui_textarea_visual_paint_range(ui, cui_textarea_child(id, row, 61), text, line, first,
                                             last, metrics.font_size, options.text_style,
                                             theme->selection, 0, metrics.line_height)) {
            float left =
                cui_textarea_prefix(ui, text, line, first, metrics.font_size, options.text_style);
            float right =
                cui_textarea_prefix(ui, text, line, last, metrics.font_size, options.text_style);
            if (line.hard_break && last > line.end)
                right +=
                    cui_measure_text_ex(ui, cui_string(" "), metrics.font_size, options.text_style)
                        .width;
            if (right > left)
                cui_set_underlay(ui, (cui_rect){left, 0, right - left, metrics.line_height},
                                 theme->selection, (cui_corner_radii){0});
        }
        if (!text.length && options.placeholder.length)
            cui_text_line(ui, options.placeholder, metrics.font_size, theme->muted,
                          metrics.line_height, options.text_style);
        else if (line.end > line.start)
            cui_text_context_line(ui, cui_textarea_line_source(text, line), metrics.font_size,
                                  foreground, metrics.line_height, options.text_style);
        if (state->composition && state->composition->active) {
            cui_edit_composition *c = state->composition;
            size_t a = c->start > line.start ? c->start : line.start;
            size_t b = c->start + c->length < line.end ? c->start + c->length : line.end;
            if (a < b &&
                !cui_textarea_visual_paint_range(ui, cui_textarea_child(id, row, 62), text, line, a,
                                                 b, metrics.font_size, options.text_style,
                                                 theme->caret, metrics.line_height - 1, 1)) {
                float left =
                    cui_textarea_prefix(ui, text, line, a, metrics.font_size, options.text_style);
                float right =
                    cui_textarea_prefix(ui, text, line, b, metrics.font_size, options.text_style);
                cui_set_underlay(ui, (cui_rect){left, metrics.line_height - 1, right - left, 1},
                                 theme->caret, (cui_corner_radii){0});
            }
        }
        if (focused && row == state->caret_line) {
            float x = cui_textarea_visual_x(
                ui, text, line, selection.cursor, metrics.font_size, options.text_style,
                state->visual_affinity_valid && state->visual_caret_byte == selection.cursor
                    ? state->visual_affinity
                    : CUI_TEXT_DOWNSTREAM);
            cui_begin_absolute_box(ui, cui_textarea_child(id, row, 11),
                                   (cui_box){.background = caret ? theme->caret : (cui_color){0}},
                                   (cui_rect){x, 0, 1, metrics.line_height});
            cui_set_text_input_geometry_rect(ui, id, cui_textarea_child(id, row, 11),
                                             metrics.line_height);
            cui_end_box(ui);
        }
        cui_end_box(ui);
    }
    if (end < state->count) {
        cui_begin_box(
            ui, cui_textarea_child(id, end, 12),
            (cui_box){.height = (state->count - end) * metrics.line_height, .fixed_height = true});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    cui_scrollbar_overlay_paint(ui, id, theme, state->scroll_x, state->scroll_y, bar_viewport,
                                bar_content, bars);
    cui_end_box(ui);
    cui_begin_box(ui, cui_textarea_child(id, 0, 6), (cui_box){.width = px, .fixed_width = true});
    cui_end_box(ui);
    cui_end_box(ui);
    cui_begin_box(ui, cui_textarea_child(id, 0, 7), (cui_box){.height = py, .fixed_height = true});
    cui_end_box(ui);
    cui_end_box(ui);
    return result;
}

cui_textarea_result cui_textarea(cui_context *ui, cui_id id, const cui_theme *theme, cui_str name,
                                 cui_edit *edit, cui_textarea_state *state,
                                 cui_textarea_options options) {
    return cui_textarea_common(ui, id, theme, name, edit, state, options,
                               (cui_component_text_handler){0}, NULL);
}
cui_multiline_event_result cui_textarea_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                               cui_str name, cui_edit *edit,
                                               cui_textarea_state *state,
                                               cui_textarea_options options,
                                               cui_component_text_handler handler) {
    cui_multiline_event_result result = {0};
    result.control =
        cui_textarea_common(ui, id, theme, name, edit, state, options, handler, &result);
    return result;
}
