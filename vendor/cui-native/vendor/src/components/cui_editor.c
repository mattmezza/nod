/* SPDX-License-Identifier: MIT */
#include "cui/cui_editor.h"
#include "../base/cui_textarea_geometry.h"
#include "cui/cui_components.h"
#include "cui/cui_editor_events.h"
#include "cui/cui_icon.h"
#include "cui/cui_scroll.h"
#include <math.h>
#include <stdio.h>
static cui_id cui_editor_child(cui_id id, size_t row, unsigned part) {
    return id ^ UINT64_C(0x7cf5d0e25db29861) ^
           ((uint64_t)(row + 1) * UINT64_C(0x9e3779b97f4a7c15)) ^ part;
}
static cui_id cui_editor_cursor_child(cui_id id, size_t row, uint64_t cursor, unsigned part) {
    uint64_t mixed = cursor + UINT64_C(0xd6e8feb86659fd93);
    mixed = (mixed ^ (mixed >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    mixed = (mixed ^ (mixed >> 27)) * UINT64_C(0x94d049bb133111eb);
    return cui_editor_child(id, row, part) ^ mixed ^ (mixed >> 31);
}
static size_t cui_editor_visible_rows(cui_textarea_options options, size_t count) {
    if (!options.auto_grow)
        return options.rows ? options.rows : 1;
    size_t minimum = options.min_rows ? options.min_rows : 1;
    size_t maximum = options.max_rows ? options.max_rows : 10;
    if (maximum < minimum)
        maximum = minimum;
    return count < minimum ? minimum : count > maximum ? maximum : count;
}
static float cui_editor_prefix(cui_context *ui, cui_str text, cui_textarea_line line, size_t offset,
                               float font, cui_text_style style) {
    return cui_textarea_visual_x(ui, text, line, offset, font, style, CUI_TEXT_DOWNSTREAM);
}
static float cui_editor_gutter(cui_context *ui, cui_editor_state *editor, cui_str document,
                               float font, cui_text_style style, bool numbers, size_t fold_count) {
    if (!numbers)
        return 0;
    uint64_t revision = cui_edit_get_state(editor->edit).revision;
    if (!editor->gutter_valid || editor->gutter_model != editor->edit ||
        editor->gutter_revision != revision) {
        const cui_textarea_state *view = &editor->view;
        bool indexed = view->initialized && view->error == CUI_TEXTAREA_OK &&
                       view->layout_model == editor->edit && view->revision == revision &&
                       !view->layout_composing && !view->fold_count && !view->layout_fold_count &&
                       !view->layout_token_count && view->lines &&
                       view->lines == view->layout_lines && view->count &&
                       view->count <= view->capacity && view->capacity == view->layout_capacity &&
                       view->lines[view->count - 1].next == document.length;
        /* Event handling has already refreshed these rows. Logical line numbers
         * count hard breaks, independently of wrapping and soft separators. */
        size_t count = indexed ? view->lines[view->count - 1].logical_line + 1 : 1;
        for (size_t i = 0; !indexed && i < document.length; ++i) {
            if (document.data[i] == '\r') {
                ++count;
                if (i + 1 < document.length && document.data[i + 1] == '\n')
                    ++i;
            } else if (document.data[i] == '\n')
                ++count;
        }
        editor->gutter_model = editor->edit;
        editor->gutter_revision = revision;
        editor->gutter_line_count = count;
        editor->gutter_valid = true;
    }
    size_t count = editor->gutter_line_count;
    if (editor->view.layout_composing && editor->view.composition &&
        editor->view.composition->active && editor->view.count)
        count = editor->view.lines[editor->view.count - 1].logical_line + 1;
    char digits[32];
    int length = snprintf(digits, sizeof digits, "%zu", count);
    return cui_measure_text_ex(ui, (cui_str){digits, (size_t)length}, font, style).width + 16 +
           (fold_count ? 16 : 0);
}
static cui_edit_result cui_editor_completion_popup(cui_context *ui, cui_id id,
                                                   const cui_theme *theme, cui_editor_state *editor,
                                                   cui_editor_options options,
                                                   cui_textarea_metrics metrics) {
    cui_editor_completion_state *menu = &editor->completion;
    cui_textarea_state *view = &editor->view;
    if (!menu->open || cui_focused(ui) != id || options.textarea.disabled ||
        options.textarea.readonly)
        return CUI_EDIT_OK;
    float row_height = metrics.line_height + 8;
    if (view->viewport.w <= 0 || view->viewport.h < row_height + 4)
        return CUI_EDIT_OK;
    size_t rows = options.completion_rows ? options.completion_rows : 8;
    size_t fitting = (size_t)floorf((view->viewport.h - 4) / row_height);
    if (rows > fitting)
        rows = fitting;
    size_t first = menu->selected >= rows ? menu->selected - rows + 1 : 0;
    size_t count = menu->count - first < rows ? menu->count - first : rows;
    float width =
        fminf(view->viewport.w, options.completion_width > 0 ? options.completion_width : 320);
    float height = count * row_height + 4;
    cui_edit_state caret = cui_edit_get_state(editor->edit);
    cui_str text = cui_edit_get_text(editor->edit);
    cui_textarea_line line = view->lines[view->caret_line];
    float x = view->viewport.x +
              cui_editor_prefix(ui, text, line, caret.cursor, metrics.font_size,
                                options.textarea.text_style) -
              view->scroll_x;
    x = fmaxf(view->viewport.x, fminf(x, view->viewport.x + view->viewport.w - width));
    float caret_y = view->viewport.y + view->caret_line * metrics.line_height - view->scroll_y;
    float y = caret_y + metrics.line_height + 4;
    if (y + height > view->viewport.y + view->viewport.h &&
        caret_y - height - 4 >= view->viewport.y)
        y = caret_y - height - 4;
    y = fmaxf(view->viewport.y, fminf(y, view->viewport.y + view->viewport.h - height));
    cui_id popup = id ^ UINT64_C(0x2b31c8fa65700394);
    size_t semantic_count;
    const cui_semantic *semantics = cui_semantics(ui, &semantic_count);
    for (size_t i = 0; i < semantic_count; ++i)
        if (semantics[i].id == id) {
            cui_semantic_details details = semantics[i].details;
            details.controls = popup;
            details.expanded = true;
            cui_set_semantic_details(ui, id, details);
            break;
        }
    cui_begin_portal(ui, popup,
                     (cui_box){.background = theme->surface,
                               .border = theme->border,
                               .border_width = 1,
                               .radius = theme->radius,
                               .padding = 2,
                               .clip = true},
                     (cui_rect){x, y, width, height}, 30);
    cui_interact(ui, popup, CUI_ROLE_LISTBOX, cui_string("Code completions"), false, false, 0);
    cui_set_semantic_details(ui, popup, (cui_semantic_details){.parent = id, .no_focus = true});
    size_t accepted = SIZE_MAX;
    for (size_t i = first; i < first + count; ++i) {
        const cui_editor_completion_item *item = &menu->items[i];
        cui_id row = cui_editor_completion_item_id(id, i);
        cui_begin_box(
            ui, row,
            (cui_box){.width = -1,
                      .height = row_height,
                      .fixed_height = true,
                      .row = true,
                      .padding = 4,
                      .gap = 8,
                      .clip = true,
                      .background = i == menu->selected ? theme->selection : (cui_color){0}});
        cui_interaction interaction =
            cui_interact(ui, row, CUI_ROLE_OPTION, item->label, false, i == menu->selected, 0);
        cui_set_semantic_details(ui, row,
                                 (cui_semantic_details){.parent = popup,
                                                        .selected = i == menu->selected,
                                                        .pos_in_set = (unsigned)i + 1,
                                                        .set_size = (unsigned)menu->count,
                                                        .no_focus = true,
                                                        .pointer_activatable = true});
        cui_set_tab_stop(ui, row, false);
        if (interaction.activated)
            accepted = i;
        cui_begin_box(ui, row ^ UINT64_C(0x0eacf3958b100cea),
                      (cui_box){.width = -1, .height = metrics.line_height, .clip = true});
        cui_text_line(ui, item->label, metrics.font_size, theme->text, metrics.line_height,
                      options.textarea.text_style);
        cui_end_box(ui);
        if (item->detail.length) {
            float detail_width =
                fminf(width * .45f, cui_measure_text_ex(ui, item->detail, metrics.font_size,
                                                        options.textarea.text_style)
                                        .width);
            cui_begin_box(
                ui, row ^ UINT64_C(0x3a84e1f926792cb5),
                (cui_box){.width = detail_width, .height = metrics.line_height, .clip = true});
            cui_text_line(ui, item->detail, metrics.font_size, theme->muted, metrics.line_height,
                          options.textarea.text_style);
            cui_end_box(ui);
        }
        cui_end_box(ui);
    }
    cui_end_box(ui);
    if (accepted != SIZE_MAX) {
        menu->selected = accepted;
        cui_edit_result result = cui_editor_accept_completion(editor);
        cui_focus(ui, id);
        cui_invalidate(ui);
        return result;
    }
    return CUI_EDIT_OK;
}
cui_textarea_result cui_editor(cui_context *ui, cui_id id, const cui_theme *theme, cui_str name,
                               cui_editor_state *editor, cui_editor_options config) {
    if (!editor || !editor->edit || cui_edit_get_content(editor->edit).token_count ||
        (config.language.length && !config.language.data))
        return (cui_textarea_result){.error = CUI_TEXTAREA_INVALID};
    cui_edit *edit = editor->edit;
    cui_textarea_state *state = &editor->view;
    cui_textarea_options options = config.textarea;
    bool had_bounds = state->initialized;
    cui_textarea_result result = {.error = CUI_TEXTAREA_INVALID};
    cui_textarea_metrics metrics;
    if (!ui || !id || !theme || !edit || !state ||
        !cui_textarea_measure(options, theme->font_size, &metrics) || !isfinite(options.width) ||
        !isfinite(options.height) || options.height < 0 || options.height > 1000000 ||
        options.rows > 65536 || options.min_rows > 65536 || options.max_rows > 65536 ||
        !isfinite(config.completion_width) || config.completion_width < 0 ||
        config.completion_width > 1000000 || config.completion_rows > 32 ||
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
    if (state->fold_count && state->fold_revision != cui_edit_get_state(edit).revision) {
        state->folds = NULL;
        state->fold_count = 0;
    }
    cui_str document = cui_edit_get_text(edit);
    float gutter = cui_editor_gutter(ui, editor, document, metrics.font_size, options.text_style,
                                     config.line_numbers, state->fold_count);
    size_t toggle_fold = SIZE_MAX;
    float px = metrics.padding_x + border, py = metrics.padding_y + border;
    float width = options.width > 0       ? options.width - 2 * px - gutter
                  : state->viewport.w > 0 ? state->viewport.w
                                          : 280 - 2 * px - gutter;
    width = fmaxf(1, width);
    float height =
        options.height > 0
            ? options.height
            : cui_editor_visible_rows(options, state->count) * metrics.line_height + 2 * py;
    if (!isfinite(height) || height > 1000000)
        return result;
    state->viewport.w = width;
    state->viewport.h = fmaxf(0, height - 2 * py);
    state->composition =
        cui_base_composition(ui, id, state->composition, &state->composition_owned);
    if (state->composition && state->composition_owned)
        state->composition->grow_view = true;
    result = cui_base_editor(ui, id, name, editor, config, metrics);
    if (result.error != CUI_TEXTAREA_OK)
        return result;
    document = cui_edit_composition_view(edit, state->composition);
    float updated_gutter =
        cui_editor_gutter(ui, editor, document, metrics.font_size, options.text_style,
                          config.line_numbers, state->fold_count);
    width = fmaxf(1, width + gutter - updated_gutter);
    gutter = updated_gutter;
    state->viewport.w = width;
    height = options.height > 0
                 ? options.height
                 : cui_editor_visible_rows(options, state->count) * metrics.line_height + 2 * py;
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
    cui_begin_box(
        ui, id,
        (cui_box){.width = options.width ? options.width : 280,
                  .fixed_width = options.width >= 0,
                  .max_width = options.width >= 0 ? (options.width ? options.width : 280) : 0,
                  .height = height,
                  .fixed_height = true,
                  .clip = true,
                  .radius = options.no_appearance ? 0 : theme->radius,
                  .background = options.no_appearance ? (cui_color){0} : background,
                  .border = options.invalid ? theme->danger
                            : focused       ? theme->focus
                                            : theme->input,
                  .border_width = border});
    cui_begin_box(ui, cui_editor_child(id, 0, 1), (cui_box){.height = py, .fixed_height = true});
    cui_end_box(ui);
    cui_begin_box(
        ui, cui_editor_child(id, 0, 2),
        (cui_box){.width = -1, .row = true, .height = state->viewport.h, .fixed_height = true});
    cui_begin_box(ui, cui_editor_child(id, 0, 3), (cui_box){.width = px, .fixed_width = true});
    cui_end_box(ui);
    if (config.line_numbers) {
        cui_begin_box(
            ui, id ^ UINT64_C(0x147dc03f2198af04),
            (cui_box){
                .width = gutter, .fixed_width = true, .height = state->viewport.h, .clip = true});
        size_t first_row = (size_t)floorf(state->scroll_y / metrics.line_height);
        if (first_row >= state->count)
            first_row = state->count - 1;
        size_t visible_rows = (size_t)ceilf(state->viewport.h / metrics.line_height) + 2;
        size_t last_row =
            visible_rows > state->count - first_row ? state->count : first_row + visible_rows;
        for (size_t row = first_row; row < last_row; ++row) {
            size_t logical = state->lines[row].logical_line;
            if (state->lines[row].start == state->lines[row].paragraph_start) {
                char number[32];
                int n = snprintf(number, sizeof number, "%zu", logical + 1);
                float y = row * metrics.line_height - state->scroll_y;
                if (y + metrics.line_height > 0 && y < state->viewport.h) {
                    float number_x = state->fold_count ? 16 : 0;
                    for (size_t f = 0; f < state->fold_count; ++f) {
                        cui_editor_fold fold = state->folds[f];
                        if (fold.start_line != logical)
                            continue;
                        cui_id control = cui_editor_child(id, fold.start_line, 21);
                        cui_begin_absolute_box(ui, control, (cui_box){0},
                                               (cui_rect){0, y, 16, metrics.line_height});
                        char label[64];
                        int length = snprintf(label, sizeof label, "%s line %zu",
                                              fold.collapsed ? "Expand" : "Collapse", logical + 1);
                        cui_interaction interaction = cui_base_button(
                            ui, control, (cui_str){label, (size_t)length}, options.disabled);
                        cui_set_semantic_details(
                            ui, control,
                            (cui_semantic_details){.parent = id, .expanded = !fold.collapsed});
                        if (interaction.activated && !options.disabled)
                            toggle_fold = f;
                        (void)cui_icon_named(
                            ui, control ^ UINT64_C(0xe2b452217c9631d0), theme,
                            cui_string(fold.collapsed ? "chevron-right" : "chevron-down"),
                            (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                               .custom_size = 14,
                                               .custom_color = true,
                                               .color = theme->muted});
                        cui_end_box(ui);
                        break;
                    }
                    cui_begin_absolute_box(
                        ui, cui_editor_child(id, row, 20), (cui_box){0},
                        (cui_rect){number_x, y, gutter - number_x, metrics.line_height});
                    cui_text_line(ui, (cui_str){number, (size_t)n}, metrics.font_size, theme->muted,
                                  metrics.line_height, options.text_style);
                    cui_end_box(ui);
                }
            }
        }
        cui_end_box(ui);
    }
    cui_begin_box(ui, cui_textarea_viewport_id(id),
                  (cui_box){.width = width,
                            .fixed_width = true,
                            .max_width = width,
                            .height = state->viewport.h,
                            .fixed_height = true,
                            .clip = true,
                            .offset_x = -state->scroll_x,
                            .offset_y = -state->scroll_y});
    cui_track_bounds(ui, cui_textarea_viewport_id(id), &state->viewport);
    float content_width = width + state->maximum_x;
    cui_begin_box(ui, cui_editor_child(id, 0, 4),
                  (cui_box){.width = content_width,
                            .fixed_width = true,
                            .height = state->count * metrics.line_height,
                            .fixed_height = true});
    cui_str text = cui_edit_composition_view(edit, state->composition);
    cui_edit_state selection = cui_edit_composition_selection(edit, state->composition);
    size_t selection_count;
    const cui_edit_selection *selections = cui_edit_get_selections(edit, &selection_count);
    cui_edit_selection primary = {0, selection.anchor, selection.cursor};
    if (!selections || (state->composition && state->composition->active)) {
        selections = &primary;
        selection_count = 1;
    }
    if (focused && state->count) {
        cui_textarea_line caret_row = state->lines[state->caret_line];
        float cx = cui_editor_prefix(ui, text, caret_row, selection.cursor, metrics.font_size,
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
        cui_begin_box(ui, cui_editor_child(id, begin, 5),
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
        cui_begin_box(ui, cui_editor_child(id, row, 10),
                      (cui_box){.width = content_width,
                                .fixed_width = true,
                                .height = metrics.line_height,
                                .fixed_height = true});
        if (config.search && config.search->active && config.search->count) {
            cui_editor_matcher *matcher = config.search;
            size_t low = 0, high = matcher->count;
            while (low < high) {
                size_t middle = low + (high - low) / 2;
                if (matcher->matches[middle].end <= line.start)
                    low = middle + 1;
                else
                    high = middle;
            }
            for (size_t match = low;
                 match < matcher->count && matcher->matches[match].start < line.next; ++match) {
                cui_editor_match range = matcher->matches[match];
                float left = cui_editor_prefix(ui, text, line, range.start, metrics.font_size,
                                               options.text_style);
                float right = cui_editor_prefix(ui, text, line, range.end, metrics.font_size,
                                                options.text_style);
                if (line.hard_break && range.end > line.end)
                    right += cui_measure_text_ex(ui, cui_string(" "), metrics.font_size,
                                                 options.text_style)
                                 .width;
                if (right > left) {
                    cui_color color = theme->selection;
                    if (config.custom_search_colors)
                        color = match == matcher->current ? config.search_current_color
                                                          : config.search_color;
                    else if (match != matcher->current) {
                        unsigned gray = ((unsigned)color.r + color.g + color.b) / 3;
                        color.r = (uint8_t)((color.r + gray) / 2);
                        color.g = (uint8_t)((color.g + gray) / 2);
                        color.b = (uint8_t)((color.b + gray) / 2);
                    }
                    cui_begin_absolute_box(ui, cui_editor_cursor_child(id, row, range.start, 72),
                                           (cui_box){.background = color},
                                           (cui_rect){left, 0, right - left, metrics.line_height});
                    cui_end_box(ui);
                }
            }
        }
        for (size_t cursor = 0; cursor < selection_count; ++cursor) {
            cui_edit_selection selected = selections[cursor];
            size_t first = selected.anchor < selected.cursor ? selected.anchor : selected.cursor;
            size_t last = selected.anchor > selected.cursor ? selected.anchor : selected.cursor;
            if (first >= line.next || last <= line.start)
                continue;
            if (cui_textarea_visual_paint_range(
                    ui, cui_editor_cursor_child(id, row, selected.id, 75), text, line, first, last,
                    metrics.font_size, options.text_style, theme->selection, 0,
                    metrics.line_height))
                continue;
            float left =
                cui_editor_prefix(ui, text, line, first, metrics.font_size, options.text_style);
            float right =
                cui_editor_prefix(ui, text, line, last, metrics.font_size, options.text_style);
            if (line.hard_break && last > line.end)
                right +=
                    cui_measure_text_ex(ui, cui_string(" "), metrics.font_size, options.text_style)
                        .width;
            if (right > left) {
                cui_begin_absolute_box(ui, cui_editor_cursor_child(id, row, selected.id, 73),
                                       (cui_box){.background = theme->selection},
                                       (cui_rect){left, 0, right - left, metrics.line_height});
                cui_end_box(ui);
            }
        }
        if (!text.length && options.placeholder.length)
            cui_text_line(ui, options.placeholder, metrics.font_size, theme->muted,
                          metrics.line_height, options.text_style);
        else if (line.end > line.start && config.highlight && visual_status == CUI_TEXT_LAYOUT_OK) {
            cui_text_color_span spans[64];
            size_t count = 0, start = line.start;
            while (start < line.end && count < 64) {
                cui_color color = foreground;
                size_t next = config.highlight(config.highlight_user, config.language, text, start,
                                               line.end, &color);
                if (next <= start || next > line.end ||
                    (next < text.length && ((unsigned char)text.data[next] & 0xc0) == 0x80)) {
                    next = line.end;
                    color = foreground;
                }
                spans[count++] = (cui_text_color_span){start - line.paragraph_start,
                                                       next - line.paragraph_start, color};
                start = next;
            }
            /* Bounded workspace: excessive highlight spans preserve a correct plain line. */
            if (start == line.end)
                cui_text_context_line_colored(ui, cui_textarea_line_source(text, line),
                                              metrics.font_size, foreground, metrics.line_height,
                                              options.text_style, spans, count);
            else {
                result.error = CUI_TEXTAREA_CAPACITY;
                cui_text_context_line(ui, cui_textarea_line_source(text, line), metrics.font_size,
                                      foreground, metrics.line_height, options.text_style);
            }
        } else if (line.end > line.start && !config.highlight)
            cui_text_context_line(ui, cui_textarea_line_source(text, line), metrics.font_size,
                                  foreground, metrics.line_height, options.text_style);
        else if (line.end > line.start) {
            size_t start = line.start;
            while (start < line.end) {
                cui_color color = foreground;
                size_t next = config.highlight
                                  ? config.highlight(config.highlight_user, config.language, text,
                                                     start, line.end, &color)
                                  : line.end;
                if (next <= start || next > line.end ||
                    (next < text.length && ((unsigned char)text.data[next] & 0xc0) == 0x80)) {
                    next = line.end;
                    color = foreground;
                }
                float x =
                    cui_editor_prefix(ui, text, line, start, metrics.font_size, options.text_style);
                cui_begin_absolute_box(ui, cui_editor_child(id, start, 30), (cui_box){0},
                                       (cui_rect){x, 0, content_width - x, metrics.line_height});
                cui_text_line(ui, (cui_str){text.data + start, next - start}, metrics.font_size,
                              color, metrics.line_height, options.text_style);
                cui_end_box(ui);
                start = next;
            }
        }
        if (state->composition && state->composition->active) {
            cui_edit_composition *c = state->composition;
            size_t a = c->start > line.start ? c->start : line.start;
            size_t b = c->start + c->length < line.end ? c->start + c->length : line.end;
            if (a < b && !cui_textarea_visual_paint_range(
                             ui, cui_editor_child(id, row, 76), text, line, a, b, metrics.font_size,
                             options.text_style, theme->caret, metrics.line_height - 1, 1)) {
                float left =
                    cui_editor_prefix(ui, text, line, a, metrics.font_size, options.text_style);
                float right =
                    cui_editor_prefix(ui, text, line, b, metrics.font_size, options.text_style);
                cui_set_underlay(ui, (cui_rect){left, metrics.line_height - 1, right - left, 1},
                                 theme->caret, (cui_corner_radii){0});
            }
        }
        if (focused)
            for (size_t cursor = 0; cursor < selection_count; ++cursor) {
                bool upstream = cursor == 0 ? state->upstream_affinity : false;
                for (size_t anchor = 0; anchor < editor->cursor_anchor_count; ++anchor)
                    if (editor->cursor_anchors[anchor].id == selections[cursor].id)
                        upstream = editor->cursor_anchors[anchor].upstream;
                if (row != cui_textarea_row_for_offset(state, selections[cursor].cursor, upstream))
                    continue;
                float x = cui_textarea_visual_x(ui, text, line, selections[cursor].cursor,
                                                metrics.font_size, options.text_style,
                                                cursor == 0 && state->visual_affinity_valid &&
                                                        state->visual_caret_byte ==
                                                            selections[cursor].cursor
                                                    ? state->visual_affinity
                                                    : CUI_TEXT_DOWNSTREAM);
                cui_begin_absolute_box(
                    ui, cui_editor_cursor_child(id, row, selections[cursor].id, 74),
                    (cui_box){.background = caret ? theme->caret : (cui_color){0}},
                    (cui_rect){x, 0, 1, metrics.line_height});
                if (cursor == 0)
                    cui_set_text_input_geometry_rect(
                        ui, id, cui_editor_cursor_child(id, row, selections[cursor].id, 74),
                        metrics.line_height);
                cui_end_box(ui);
            }
        cui_end_box(ui);
    }
    if (end < state->count) {
        cui_begin_box(
            ui, cui_editor_child(id, end, 12),
            (cui_box){.height = (state->count - end) * metrics.line_height, .fixed_height = true});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    cui_scrollbar_overlay_paint(ui, id, theme, state->scroll_x, state->scroll_y, bar_viewport,
                                bar_content, bars);
    cui_end_box(ui);
    cui_begin_box(ui, cui_editor_child(id, 0, 6), (cui_box){.width = px, .fixed_width = true});
    cui_end_box(ui);
    cui_end_box(ui);
    cui_begin_box(ui, cui_editor_child(id, 0, 7), (cui_box){.height = py, .fixed_height = true});
    cui_end_box(ui);
    cui_end_box(ui);
    if (toggle_fold != SIZE_MAX) {
        cui_edit_state before_toggle = cui_edit_get_state(edit);
        cui_editor_set_folded(editor, toggle_fold, !state->folds[toggle_fold].collapsed);
        cui_edit_state after_toggle = cui_edit_get_state(edit);
        result.selection_changed |= before_toggle.cursor != after_toggle.cursor ||
                                    before_toggle.anchor != after_toggle.anchor;
        cui_invalidate(ui); /* The next draw rebuilds the projected rows. */
    }
    if (editor->completion.open) {
        if (had_bounds) {
            cui_edit_state before_completion = cui_edit_get_state(edit);
            cui_edit_result completion_error =
                cui_editor_completion_popup(ui, id, theme, editor, config, metrics);
            cui_edit_state after_completion = cui_edit_get_state(edit);
            result.changed |= before_completion.revision != after_completion.revision;
            result.selection_changed |= before_completion.cursor != after_completion.cursor ||
                                        before_completion.anchor != after_completion.anchor;
            if (completion_error != CUI_EDIT_OK)
                result.edit_error = completion_error;
        } else
            cui_invalidate(ui); /* Anchor uses the first completed layout. */
    }
    return result;
}

cui_editor_search_result cui_editor_find_bar(cui_context *ui, cui_id id, cui_id editor_id,
                                             const cui_theme *theme, cui_editor_state *editor,
                                             cui_editor_search_state *state, float width,
                                             bool disabled) {
    cui_editor_search_result result = {.error = CUI_EDIT_INVALID_ARGUMENT};
    if (!ui || !id || !editor_id || id == editor_id || !theme || !editor || !editor->edit ||
        !state || !state->query || cui_edit_get_state(state->query).multiline || !isfinite(width) ||
        width < 0 || width > 1000000)
        return result;
    bool replace_enabled =
        state->matcher && state->replacement && state->scratch && state->scratch_capacity;
    if (replace_enabled &&
        (state->replacement == state->query || state->replacement == editor->edit ||
         state->query == editor->edit || cui_edit_get_state(state->replacement).multiline))
        return result;
    cui_id replacement_field = id ^ UINT64_C(0x1b4a6c72d005ab81);
    cui_id field = id ^ UINT64_C(0x790a4100f05b190d);
    bool navigate = false, reverse = false, close = false, replace_current = false,
         replace_all = false;
    size_t event_count;
    const cui_event *events = cui_frame_events(ui, &event_count);
    for (size_t i = 0; i < event_count && !disabled; ++i) {
        cui_event e = events[i];
        if (e.type != CUI_EVENT_KEY_DOWN || cui_event_consumed(ui, i))
            continue;
        cui_id target = cui_event_target(ui, i);
        bool replacement_shortcut =
            replace_enabled && !state->replace_disabled &&
            ((e.key == CUI_KEY_H && (e.modifiers & CUI_MOD_CTRL)) ||
             (e.key == CUI_KEY_F && (e.modifiers & CUI_MOD_META) && (e.modifiers & CUI_MOD_SHIFT)));
        if ((replacement_shortcut ||
             (e.key == CUI_KEY_F && (e.modifiers & (CUI_MOD_CTRL | CUI_MOD_META)))) &&
            (target == editor_id ||
             (state->open && (target == field || target == replacement_field)))) {
            cui_edit_result opened = cui_editor_search_open(editor, state, replacement_shortcut);
            if (opened != CUI_EDIT_OK) {
                result.error = opened;
                return result;
            }
            cui_edit_select(state->query, 0, cui_edit_get_text(state->query).length);
            cui_focus(ui, field);
            cui_consume_event(ui, i);
        } else if (state->open && target == field && e.key == CUI_KEY_ENTER) {
            navigate = true;
            reverse = (e.modifiers & CUI_MOD_SHIFT) != 0;
            cui_consume_event(ui, i);
        } else if (state->open && state->replace_mode && target == replacement_field &&
                   e.key == CUI_KEY_ENTER) {
            if (!state->replace_disabled)
                replace_current = true;
            cui_consume_event(ui, i);
        } else if (state->open &&
                   (target == field || (replace_enabled && target == replacement_field)) &&
                   e.key == CUI_KEY_ESCAPE) {
            close = true;
            cui_consume_event(ui, i);
        }
    }
    result.error = CUI_EDIT_OK;
    if (!state->open)
        return result;
    if (replace_enabled)
        cui_begin_box(ui, id ^ UINT64_C(0x0bc45b2e8d412020),
                      (cui_box){.width = width > 0 ? width : 500,
                                .fixed_width = true,
                                .max_width = width > 0 ? width : 500,
                                .gap = 4});
    cui_begin_box(
        ui, id,
        (cui_box){
            .width = width > 0 ? width : 500, .row = true, .gap = 4, .background = theme->surface});
    cui_input_state input = {.behavior = state->input, .scroll_x = state->scroll_x};
    cui_input_result edited = cui_input_ex(
        ui, field, theme, cui_string("Find"), state->query, &input,
        (cui_input_options){
            .width = -1, .disabled = disabled, .placeholder = cui_string("Find literal text")});
    state->input = input.behavior;
    state->scroll_x = input.scroll_x;
    result.query_changed = edited.changed;
    if (edited.error != CUI_EDIT_OK)
        result.error = edited.error;
    cui_str query = cui_edit_get_text(state->query);
    if (replace_enabled) {
        cui_edit_result synced = cui_editor_matcher_set_query(editor, state->matcher, query,
                                                              state->matcher->case_insensitive);
        if (synced != CUI_EDIT_OK)
            result.error = synced;
        result.matches = synced == CUI_EDIT_OK ? state->matcher->count : 0;
    } else
        result.matches = cui_editor_match_count(editor, query);
    if (cui_button(ui, id ^ UINT64_C(0x391cfd020af58d20), theme, cui_string("Prev"),
                   disabled || !result.matches)) {
        navigate = true;
        reverse = true;
    }
    if (cui_button(ui, id ^ UINT64_C(0x73bbf8e9cae70b45), theme, cui_string("Next"),
                   disabled || !result.matches)) {
        navigate = true;
        reverse = false;
    }
    char status[48];
    int length = replace_enabled
                     ? snprintf(status, sizeof status, "%zu/%zu",
                                result.matches ? state->matcher->current + 1 : 0, result.matches)
                     : snprintf(status, sizeof status, "%zu matches", result.matches);
    cui_text(ui, (cui_str){status, (size_t)length}, theme->font_size, theme->muted);
    close =
        cui_button(ui, id ^ UINT64_C(0x0092b880a17f63dd), theme, cui_string("Close"), disabled) ||
        close;
    cui_end_box(ui);
    if (replace_enabled) {
        cui_begin_box(ui, id ^ UINT64_C(0x44a08126b4c71f12), (cui_box){.row = true, .gap = 4});
        if (cui_button_ex(ui, id ^ UINT64_C(0x51a814631c060f34), theme, cui_string("Match case"),
                          (cui_button_options){.disabled = disabled,
                                               .selected = !state->matcher->case_insensitive})) {
            result.error = cui_editor_matcher_set_query(editor, state->matcher, query,
                                                        !state->matcher->case_insensitive);
            result.matches = result.error == CUI_EDIT_OK ? state->matcher->count : 0;
        }
        cui_update_semantic(ui, id ^ UINT64_C(0x51a814631c060f34),
                            !state->matcher->case_insensitive, 0);
        if (cui_button_ex(ui, id ^ UINT64_C(0x43b926eab194d100), theme, cui_string("Replace mode"),
                          (cui_button_options){.disabled = disabled || state->replace_disabled,
                                               .selected = state->replace_mode}))
            state->replace_mode = !state->replace_mode;
        cui_update_semantic(ui, id ^ UINT64_C(0x43b926eab194d100), state->replace_mode, 0);
        cui_end_box(ui);
        if (state->replace_mode) {
            cui_input_state replacement_input = {.behavior = state->replacement_input,
                                                 .scroll_x = state->replacement_scroll_x};
            cui_input_result changed =
                cui_input_ex(ui, replacement_field, theme, cui_string("Replace with"),
                             state->replacement, &replacement_input,
                             (cui_input_options){.width = width > 0 ? width : 500,
                                                 .disabled = disabled || state->replace_disabled,
                                                 .placeholder = cui_string("Replacement text")});
            state->replacement_input = replacement_input.behavior;
            state->replacement_scroll_x = replacement_input.scroll_x;
            result.replacement_changed = changed.changed;
            if (changed.error != CUI_EDIT_OK)
                result.error = changed.error;
            cui_begin_box(ui, id ^ UINT64_C(0x620f8a41db96ab24), (cui_box){.row = true, .gap = 4});
            bool blocked = disabled || state->replace_disabled || !result.matches ||
                           result.error != CUI_EDIT_OK;
            replace_current = cui_button(ui, id ^ UINT64_C(0x311b89208297ba14), theme,
                                         cui_string("Replace"), blocked) ||
                              replace_current;
            replace_all = cui_button(ui, id ^ UINT64_C(0x4891147fe2443891), theme,
                                     cui_string("Replace all"), blocked) ||
                          replace_all;
            cui_end_box(ui);
        }
        cui_end_box(ui);
    }
    if (navigate && !disabled) {
        result.error = replace_enabled
                           ? cui_editor_matcher_navigate(editor, state->matcher, reverse)
                           : cui_editor_find(editor, query, reverse, true);
        result.navigated = result.error == CUI_EDIT_OK;
        cui_invalidate(ui);
    }
    if (replace_enabled && !disabled && !state->replace_disabled && result.error == CUI_EDIT_OK &&
        (replace_current || replace_all)) {
        cui_str replacement = cui_edit_get_text(state->replacement);
        if (replace_all)
            result.error =
                cui_editor_matcher_replace_all(editor, state->matcher, replacement, state->scratch,
                                               state->scratch_capacity, &result.replaced);
        else {
            result.error = cui_editor_matcher_replace_current(
                editor, state->matcher, replacement, state->scratch, state->scratch_capacity);
            result.replaced = result.error == CUI_EDIT_OK ? 1 : 0;
        }
        result.matches = state->matcher->count;
        cui_invalidate(ui);
    }
    if (close && !disabled) {
        if (replace_enabled)
            cui_editor_matcher_close(state->matcher);
        state->open = false;
        result.closed = true;
        cui_focus(ui, editor_id);
        cui_invalidate(ui);
    }
    return result;
}

cui_multiline_event_result cui_editor_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                             cui_str name, cui_editor_state *state,
                                             cui_editor_options options,
                                             cui_component_text_handler handler) {
    cui_multiline_event_result result = {0};
    cui_edit *edit = state ? state->edit : NULL;
    cui_edit_state before = edit ? cui_edit_get_state(edit) : (cui_edit_state){0};
    result.control = cui_editor(ui, id, theme, name, state, options);
    cui_edit_state after = edit ? cui_edit_get_state(edit) : (cui_edit_state){0};
    if (ui && edit && handler.callback && before.revision != after.revision) {
        uint64_t frame = cui_frame_sequence(ui);
        cui_component_text_event event = {
            .event = {.kind = CUI_COMPONENT_EVENT_CHANGE,
                      .target = id,
                      .source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN,
                      .flags = frame ? CUI_COMPONENT_EVENT_HAS_FRAME : 0,
                      .frame_sequence = frame},
            .before = before,
            .after = after,
            .has_text = true,
            .text = cui_edit_get_text(edit)};
        handler.callback(handler.user, &event);
        result.change_count = 1;
    }
    return result;
}
