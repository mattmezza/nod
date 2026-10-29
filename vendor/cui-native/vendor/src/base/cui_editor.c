/* SPDX-License-Identifier: MIT */
#include "cui/cui_editor.h"
#include "cui/cui_editor_events.h"
#include "cui_textarea_geometry.h"
#include <math.h>
#include <string.h>
bool cui_editor_state_init(cui_editor_state *s, cui_edit *edit, cui_textarea_line *lines,
                           size_t capacity) {
    if (!s || !edit || !cui_edit_get_state(edit).multiline ||
        cui_edit_get_content(edit).token_count)
        return false;
    cui_editor_state next = {.edit = edit};
    if (!cui_textarea_state_init(&next.view, lines, capacity))
        return false;
    *s = next;
    return true;
}
cui_editor_options cui_editor_default(void) {
    cui_editor_options o = {.textarea = cui_textarea_default(), .line_numbers = true};
    o.textarea.no_wrap = true;
    o.textarea.text_style.monospace = true;
    o.textarea.rows = 12;
    return o;
}
static bool cui_editor_boundary(cui_str text, size_t i) {
    return i == text.length || (((unsigned char)text.data[i] & 0xc0) != 0x80);
}
cui_edit_result cui_editor_find(cui_editor_state *s, cui_str query, bool reverse, bool wrap) {
    if (!s || !s->edit || !query.length || !query.data || !cui_edit_valid_utf8(query))
        return CUI_EDIT_INVALID_ARGUMENT;
    cui_str text = cui_edit_get_text(s->edit);
    if (query.length > text.length)
        return CUI_EDIT_BOUNDARY;
    cui_edit_state selection = cui_edit_get_state(s->edit);
    size_t first = selection.cursor < selection.anchor ? selection.cursor : selection.anchor;
    size_t last = selection.cursor > selection.anchor ? selection.cursor : selection.anchor;
    size_t found = SIZE_MAX, wrapped = SIZE_MAX;
    for (size_t i = 0; i <= text.length - query.length; ++i) {
        if (!cui_editor_boundary(text, i) || !cui_editor_boundary(text, i + query.length) ||
            memcmp(text.data + i, query.data, query.length))
            continue;
        if (reverse) {
            wrapped = i;
            if (i + query.length <= first)
                found = i;
        } else {
            if (wrapped == SIZE_MAX)
                wrapped = i;
            if (found == SIZE_MAX && i >= last)
                found = i;
        }
    }
    if (found == SIZE_MAX && wrap)
        found = wrapped;
    return found == SIZE_MAX ? CUI_EDIT_BOUNDARY
                             : cui_edit_select(s->edit, found, found + query.length);
}
cui_id cui_editor_completion_item_id(cui_id id, size_t index) {
    return id ^ UINT64_C(0x6641ab75da98e031) ^
           ((uint64_t)(index + 1) * UINT64_C(0x9e3779b97f4a7c15));
}
static cui_edit_result cui_editor_completion_events(cui_context *ui, cui_id id,
                                                    cui_editor_state *state,
                                                    cui_textarea_options options) {
    if (!state->completion.open)
        return CUI_EDIT_OK;
    cui_edit_state current = cui_edit_get_state(state->edit);
    if (state->completion.revision != current.revision ||
        state->completion.cursor != current.cursor || state->completion.anchor != current.anchor ||
        (cui_focused(ui) && cui_focused(ui) != id)) {
        cui_editor_cancel_completion(state);
        return CUI_EDIT_OK;
    }
    if (!state->completion.items || !state->completion.count ||
        state->completion.selected >= state->completion.count) {
        cui_editor_cancel_completion(state);
        return CUI_EDIT_INVALID_ARGUMENT;
    }
    if (options.disabled || options.readonly)
        return CUI_EDIT_OK;
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count && state->completion.open; ++i) {
        cui_event e = events[i];
        if (cui_event_consumed(ui, i))
            continue;
        cui_id target = cui_event_target(ui, i);
        bool option = false;
        for (size_t row = 0; target != id && !option && row < state->completion.count; ++row)
            option = target == cui_editor_completion_item_id(id, row);
        if (e.type == CUI_EVENT_CANCEL || (e.type == CUI_EVENT_POINTER_DOWN && !option)) {
            cui_editor_cancel_completion(state);
            break;
        }
        if (target != id && !option)
            continue;
        /* Never accept before an earlier queued text/navigation edit. Let the
         * textarea process that queue in order, then discard stale proposals. */
        if (e.type == CUI_EVENT_TEXT || e.type >= CUI_EVENT_COMPOSITION_START)
            break;
        if (e.type != CUI_EVENT_KEY_DOWN)
            continue;
        if (e.ime_composing || e.prefer_character_input ||
            (e.modifiers & (CUI_MOD_CTRL | CUI_MOD_META | CUI_MOD_ALT)))
            break;
        if (e.key == CUI_KEY_UP) {
            state->completion.selected = state->completion.selected ? state->completion.selected - 1
                                                                    : state->completion.count - 1;
        } else if (e.key == CUI_KEY_DOWN) {
            state->completion.selected = (state->completion.selected + 1) % state->completion.count;
        } else if (e.key == CUI_KEY_ENTER && !(e.modifiers & CUI_MOD_SHIFT)) {
            cui_edit_result result = cui_editor_accept_completion(state);
            cui_consume_event(ui, i);
            return result;
        } else if (e.key == CUI_KEY_ESCAPE) {
            cui_editor_cancel_completion(state);
        } else
            break;
        cui_consume_event(ui, i);
        cui_invalidate(ui);
    }
    return CUI_EDIT_OK;
}
static void cui_editor_reveal_all_folds(cui_editor_state *editor) {
    cui_textarea_state *view = &editor->view;
    if (!view->fold_count || view->fold_revision != cui_edit_get_state(editor->edit).revision)
        return;
    size_t count;
    const cui_edit_selection *selections = cui_edit_get_selections(editor->edit, &count);
    cui_str text = cui_edit_get_text(editor->edit);
    for (size_t cursor = 0; cursor < count; ++cursor) {
        size_t offsets[2] = {selections[cursor].anchor, selections[cursor].cursor};
        for (size_t point = 0; point < 2; ++point) {
            size_t line = 0;
            for (size_t i = 0; i < offsets[point]; ++i)
                line += text.data[i] == '\n';
            for (size_t fold = 0; fold < view->fold_count; ++fold)
                if (line > view->folds[fold].start_line && line < view->folds[fold].end_line)
                    view->folds[fold].collapsed = false;
        }
    }
}
static bool cui_editor_style_equal(cui_text_style a, cui_text_style b) {
    return a.family == b.family && a.weight == b.weight && a.italic == b.italic &&
           a.monospace == b.monospace && a.letter_spacing == b.letter_spacing;
}
static void cui_editor_cursor_sync(cui_context *ui, cui_editor_state *editor, cui_text_style style,
                                   float font) {
    size_t count;
    const cui_edit_selection *selections = cui_edit_get_selections(editor->edit, &count);
    size_t column_count = 0;
    const cui_edit_column_anchor *columns = cui_edit_get_columns(editor->edit, &column_count);
    cui_editor_cursor_anchor next[CUI_EDIT_MAX_SELECTIONS];
    bool same = editor->cursor_anchor_revision == cui_edit_get_state(editor->edit).revision &&
                editor->cursor_anchor_font_size == font &&
                cui_editor_style_equal(editor->cursor_anchor_style, style);
    for (size_t i = 0; i < count; ++i) {
        next[i] = (cui_editor_cursor_anchor){
            .id = selections[i].id, .anchor = selections[i].anchor, .cursor = selections[i].cursor};
        if (same)
            for (size_t j = 0; j < editor->cursor_anchor_count; ++j)
                if (editor->cursor_anchors[j].id == selections[i].id &&
                    editor->cursor_anchors[j].anchor == selections[i].anchor &&
                    editor->cursor_anchors[j].cursor == selections[i].cursor)
                    next[i] = editor->cursor_anchors[j];
        if ((!same || !next[i].valid) && columns)
            for (size_t j = 0; j < column_count; ++j)
                if (columns[j].id == selections[i].id &&
                    columns[j].cursor == selections[i].cursor && columns[j].valid) {
                    next[i].x = columns[j].x;
                    if (ui && (columns[j].font_size != font ||
                               !cui_editor_style_equal(columns[j].style, style))) {
                        size_t row = cui_textarea_row_for_offset(
                            &editor->view, selections[i].cursor, columns[j].upstream);
                        if (row != SIZE_MAX) {
                            cui_str text = cui_edit_get_text(editor->edit);
                            size_t at = editor->view.lines[row].start;
                            size_t position = 0;
                            while (position < columns[j].column &&
                                   at < editor->view.lines[row].end) {
                                at = cui_grapheme_next(text, at);
                                position++;
                            }
                            next[i].x = cui_textarea_visual_x(ui, text, editor->view.lines[row], at,
                                                              font, style, CUI_TEXT_DOWNSTREAM);
                            if (position < columns[j].column)
                                next[i].x +=
                                    (columns[j].column - position) *
                                    cui_measure_text_ex(ui, cui_string(" "), font, style).width;
                            if (!isfinite(next[i].x))
                                next[i].x = columns[j].x;
                        }
                    }
                    next[i].valid = true;
                    next[i].upstream = columns[j].upstream;
                    break;
                }
    }
    memcpy(editor->cursor_anchors, next, count * sizeof(*next));
    editor->cursor_anchor_count = count;
    editor->cursor_anchor_revision = cui_edit_get_state(editor->edit).revision;
    editor->cursor_anchor_style = style;
    editor->cursor_anchor_font_size = font;
}
static void cui_editor_save_columns(cui_editor_state *editor,
                                    const cui_edit_column_anchor *previous, size_t previous_count,
                                    const cui_edit_selection *before, size_t before_count,
                                    bool vertical) {
    size_t count;
    const cui_edit_selection *selections = cui_edit_get_selections(editor->edit, &count);
    if (!cui_edit_get_columns(editor->edit, NULL) || count != editor->cursor_anchor_count)
        return;
    cui_edit_column_anchor next[CUI_EDIT_MAX_SELECTIONS];
    cui_str text = cui_edit_get_text(editor->edit);
    for (size_t i = 0; i < count; ++i) {
        cui_editor_cursor_anchor anchor = editor->cursor_anchors[i];
        next[i] = (cui_edit_column_anchor){.id = selections[i].id,
                                           .cursor = selections[i].cursor,
                                           .x = anchor.x,
                                           .font_size = editor->view.font_size,
                                           .style = editor->view.text_style,
                                           .valid = anchor.valid,
                                           .upstream = anchor.upstream};
        bool inherited = false;
        for (size_t j = 0; j < previous_count; ++j)
            if (previous[j].id == next[i].id && previous[j].valid && next[i].valid &&
                fabsf(previous[j].x - next[i].x) < 0.01f) {
                next[i].column = previous[j].column;
                inherited = true;
                break;
            }
        if (!inherited && anchor.valid) {
            size_t offset = anchor.cursor;
            bool upstream = anchor.upstream;
            if (vertical)
                for (size_t j = 0; j < before_count; ++j)
                    if (before[j].id == next[i].id) {
                        offset = before[j].cursor;
                        upstream = false;
                        break;
                    }
            size_t row = cui_textarea_row_for_offset(&editor->view, offset, upstream);
            if (row == SIZE_MAX)
                continue;
            size_t start = editor->view.lines[row].start;
            for (size_t at = start; at < offset && at < editor->view.lines[row].end;) {
                at = cui_grapheme_next(text, at);
                next[i].column++;
            }
        }
    }
    (void)cui_edit_set_columns(editor->edit, next, count);
}
static uint64_t cui_editor_next_cursor(cui_editor_state *editor,
                                       const cui_edit_selection *selections, size_t count) {
    uint64_t next = editor->next_cursor_id;
    for (size_t i = 0; i < count; ++i) {
        if (selections[i].id == UINT64_MAX)
            return UINT64_MAX;
        if (next <= selections[i].id)
            next = selections[i].id + 1;
    }
    return next;
}
static void cui_editor_multi_reveal(cui_context *ui, cui_editor_state *editor, size_t offset,
                                    bool upstream) {
    cui_textarea_state *view = &editor->view;
    size_t row = cui_textarea_row_for_offset(view, offset, upstream);
    if (row == SIZE_MAX)
        return;
    float x = cui_textarea_row_x(ui, editor->edit, view, row, offset), y = row * view->line_height;
    if (x < view->scroll_x)
        view->scroll_x = x;
    if (x + 1 > view->scroll_x + view->viewport.w)
        view->scroll_x = x + 1 - view->viewport.w;
    if (y < view->scroll_y)
        view->scroll_y = y;
    if (y + view->line_height > view->scroll_y + view->viewport.h)
        view->scroll_y = y + view->line_height - view->viewport.h;
}
static size_t cui_editor_clip_grapheme(cui_str text, size_t offset, bool forward) {
    cui_grapheme_iterator iterator = {text, 0};
    size_t previous = 0;
    while (iterator.offset < offset) {
        previous = iterator.offset;
        cui_grapheme_iterator_next(&iterator);
    }
    return iterator.offset == offset || forward ? iterator.offset : previous;
}
cui_edit_result cui_editor_select_rectangle(cui_editor_state *editor, cui_editor_column_point start,
                                            cui_editor_column_point end) {
    if (!editor || !editor->edit || editor->clipboard_active ||
        editor->view.revision != cui_edit_get_state(editor->edit).revision ||
        !cui_edit_selection_capacity(editor->edit))
        return CUI_EDIT_INVALID_ARGUMENT;
    cui_str text = cui_edit_get_text(editor->edit);
    if (!cui_grapheme_boundary(text, start.offset) || !cui_grapheme_boundary(text, end.offset))
        return CUI_EDIT_BOUNDARY;
    size_t first = cui_textarea_row_for_offset(&editor->view, start.offset, false),
           last = cui_textarea_row_for_offset(&editor->view, end.offset, false);
    if (first == SIZE_MAX || last == SIZE_MAX)
        return CUI_EDIT_INVALID_ARGUMENT;
    if (start.offset < editor->view.lines[first].start ||
        start.offset > editor->view.lines[first].end ||
        end.offset < editor->view.lines[last].start || end.offset > editor->view.lines[last].end)
        return CUI_EDIT_BOUNDARY;
    size_t a = start.offset - editor->view.lines[first].start,
           b = end.offset - editor->view.lines[last].start;
    if (start.columns_past_end > SIZE_MAX - a || end.columns_past_end > SIZE_MAX - b)
        return CUI_EDIT_BOUNDARY;
    a += start.columns_past_end;
    b += end.columns_past_end;
    size_t left = a < b ? a : b, right = a > b ? a : b;
    if (first > last) {
        size_t swap = first;
        first = last;
        last = swap;
    }
    size_t count = last - first + 1;
    if (count > cui_edit_selection_capacity(editor->edit))
        return CUI_EDIT_CAPACITY;
    size_t old_count;
    const cui_edit_selection *live = cui_edit_get_selections(editor->edit, &old_count);
    uint64_t next = cui_editor_next_cursor(editor, live, old_count);
    if (count > UINT64_MAX - next)
        return CUI_EDIT_CAPACITY;
    cui_edit_selection rows[CUI_EDIT_MAX_SELECTIONS];
    for (size_t i = 0; i < count; ++i) {
        cui_textarea_line line = editor->view.lines[first + i];
        size_t length = line.end - line.start;
        size_t anchor = line.start + (left < length ? left : length),
               cursor = line.start + (right < length ? right : length);
        anchor = cui_editor_clip_grapheme(text, anchor, false);
        cursor = cui_editor_clip_grapheme(text, cursor, false);
        rows[i] = (cui_edit_selection){next + i, anchor, cursor};
    }
    cui_edit_result error = cui_edit_select_disjoint(editor->edit, rows, count);
    if (error == CUI_EDIT_OK)
        editor->next_cursor_id = next + count;
    return error;
}
static cui_edit_result cui_editor_column_hit(cui_context *ui, cui_editor_state *editor, float x,
                                             float y, cui_editor_column_point *point) {
    bool affinity = editor->view.upstream_affinity;
    size_t offset = cui_textarea_hit_test(ui, editor->edit, &editor->view, x, y);
    editor->view.upstream_affinity = affinity;
    if (offset == SIZE_MAX)
        return CUI_EDIT_BOUNDARY;
    float relative =
        fmaxf(0, (y - editor->view.viewport.y + editor->view.scroll_y) / editor->view.line_height);
    size_t row = relative >= editor->view.count ? editor->view.count - 1 : (size_t)relative;
    cui_textarea_line line = editor->view.lines[row];
    size_t extra = 0;
    if (offset == line.end && (line.hard_break || row + 1 == editor->view.count)) {
        float width = cui_textarea_row_x(ui, editor->edit, &editor->view, row, line.end);
        float space = cui_measure_text_ex(ui, cui_string(" "), editor->view.font_size,
                                          editor->view.text_style)
                          .width;
        double past = (double)x - editor->view.viewport.x + editor->view.scroll_x - width;
        if (space > 0 && isfinite(space) && past > 0) {
            double columns = round(past / space);
            if (!isfinite(columns) || columns >= (double)SIZE_MAX)
                return CUI_EDIT_BOUNDARY;
            extra = (size_t)columns;
        }
    }
    *point = (cui_editor_column_point){offset, extra};
    return CUI_EDIT_OK;
}
static float cui_editor_column_scroll_delta(cui_editor_state *editor, float y) {
    float bottom = editor->view.viewport.y + editor->view.viewport.h - 16,
          top = editor->view.viewport.y + 16;
    if (y > bottom)
        return 12 + fminf(1, (y - bottom) / 96) * 52;
    if (y < top)
        return -(12 + fminf(1, (top - y) / 96) * 52);
    return 0;
}
static bool cui_editor_storage_overlap(const void *, size_t, const void *, size_t);
static cui_edit_result cui_editor_multi_clipboard(cui_editor_state *editor,
                                                  cui_editor_options options, cui_key key) {
    const cui_platform *host = options.textarea.clipboard;
    bool paste = key == CUI_KEY_V;
    if (!host || !(host->capabilities & CUI_PLATFORM_CLIPBOARD) ||
        (paste ? !host->clipboard_get : !host->clipboard_set))
        return CUI_EDIT_CLIPBOARD_UNAVAILABLE;
    if (options.textarea.readonly && key == CUI_KEY_V)
        return CUI_EDIT_OK;
    size_t count;
    const cui_edit_selection *live = cui_edit_get_selections(editor->edit, &count);
    cui_edit_selection before[CUI_EDIT_MAX_SELECTIONS];
    memcpy(before, live, count * sizeof(*live));
    uint64_t revision = cui_edit_get_state(editor->edit).revision;
    cui_str text = {0};
    cui_edit_result error = CUI_EDIT_OK;
    if (!paste) {
        char *scratch = options.clipboard_scratch;
        size_t capacity = options.clipboard_capacity;
        if (!scratch || !capacity ||
            cui_editor_storage_overlap(scratch, capacity, editor, sizeof(*editor)) ||
            cui_editor_storage_overlap(scratch, capacity, editor->view.lines,
                                       editor->view.capacity * sizeof(*editor->view.lines)) ||
            cui_editor_storage_overlap(scratch, capacity, editor->cursor_anchors,
                                       editor->cursor_anchor_capacity *
                                           sizeof(*editor->cursor_anchors)))
            return CUI_EDIT_INVALID_ARGUMENT;
        error = cui_edit_copy_selections(editor->edit, scratch, capacity, &text);
        if (error != CUI_EDIT_OK)
            return error;
        if (key == CUI_KEY_X && !options.textarea.readonly) {
            error = cui_edit_can_replace_selections(editor->edit, (cui_str){0});
            if (error != CUI_EDIT_OK)
                return error;
        }
    }
    editor->clipboard_active = true;
    bool copied = true;
    if (paste)
        text = host->clipboard_get(host->user);
    else
        copied = host->clipboard_set(host->user, text);
    editor->clipboard_active = false;
    size_t after_count;
    live = cui_edit_get_selections(editor->edit, &after_count);
    if (revision != cui_edit_get_state(editor->edit).revision || count != after_count ||
        memcmp(before, live, count * sizeof(*live)))
        return CUI_EDIT_INVALID_ARGUMENT;
    if (!copied)
        return CUI_EDIT_CLIPBOARD_UNAVAILABLE;
    if (paste)
        return cui_edit_paste_selections(editor->edit, text);
    return key == CUI_KEY_X && !options.textarea.readonly
               ? cui_edit_replace_selections(editor->edit, (cui_str){0})
               : CUI_EDIT_OK;
}
static cui_textarea_result cui_editor_multi_events(cui_context *ui, cui_id id,
                                                   cui_editor_state *editor,
                                                   cui_editor_options editor_options,
                                                   cui_textarea_metrics metrics, float width) {
    cui_textarea_options options = editor_options.textarea;
    cui_textarea_result result = {.error = CUI_TEXTAREA_OK};
    size_t count;
    const cui_edit_selection *live = cui_edit_get_selections(editor->edit, &count);
    if (!live)
        return result;
    cui_edit_selection before[CUI_EDIT_MAX_SELECTIONS];
    memcpy(before, live, count * sizeof(*before));
    size_t before_count = count;
    float before_x = editor->view.scroll_x, before_y = editor->view.scroll_y;
    uint64_t before_revision = cui_edit_get_state(editor->edit).revision;
    if (editor->multi_pointer &&
        (options.disabled || cui_focused(ui) != id || editor->column_revision != before_revision ||
         editor->column_font_size != metrics.font_size || editor->column_wrap_width != width ||
         !cui_editor_style_equal(editor->column_style, options.text_style))) {
        editor->multi_pointer = false;
        editor->column_dragged = false;
    }
    size_t event_count;
    const cui_event *events = cui_frame_events(ui, &event_count);
    for (size_t event_index = 0; event_index < event_count; ++event_index) {
        cui_event event = events[event_index];
        if (editor->view.composition && editor->view.composition->active &&
            event.type != CUI_EVENT_CANCEL)
            continue;
        if (event.type == CUI_EVENT_CANCEL) {
            editor->multi_pointer = false;
            editor->column_dragged = false;
        }
        if (options.disabled || cui_event_consumed(ui, event_index) ||
            cui_event_target(ui, event_index) != id ||
            (event.type == CUI_EVENT_KEY_DOWN &&
             (event.ime_composing || event.prefer_character_input)))
            continue;
        if (editor->multi_pointer &&
            (cui_focused(ui) != id ||
             editor->column_revision != cui_edit_get_state(editor->edit).revision ||
             editor->column_font_size != metrics.font_size || editor->column_wrap_width != width ||
             !cui_editor_style_equal(editor->column_style, options.text_style))) {
            editor->multi_pointer = false;
            editor->column_dragged = false;
        }
        if (editor->multi_pointer && event.type == CUI_EVENT_POINTER_UP) {
            editor->multi_pointer = false;
            editor->column_dragged = false;
            cui_consume_event(ui, event_index);
            continue;
        }
        bool rectangle = editor->multi_pointer && event.type == CUI_EVENT_POINTER_MOVE;
        bool shift = (event.modifiers & CUI_MOD_SHIFT) != 0,
             alt = (event.modifiers & CUI_MOD_ALT) != 0;
        bool command = (event.modifiers & (CUI_MOD_CTRL | CUI_MOD_META)) != 0;
        bool vertical = event.type == CUI_EVENT_KEY_DOWN &&
                        (event.key == CUI_KEY_UP || event.key == CUI_KEY_DOWN ||
                         event.key == CUI_KEY_PAGEUP || event.key == CUI_KEY_PAGEDOWN);
        bool add = vertical && (event.key == CUI_KEY_UP || event.key == CUI_KEY_DOWN) && alt &&
                   (command || shift);
        bool pointer = event.type == CUI_EVENT_POINTER_DOWN &&
                       event.pointer_button == CUI_POINTER_PRIMARY && alt;
        bool clipboard =
            event.type == CUI_EVENT_KEY_DOWN && command &&
            (event.key == CUI_KEY_C || event.key == CUI_KEY_X || event.key == CUI_KEY_V);
        char character = cui_key_character(event.key);
        bool block_indent =
            event.type == CUI_EVENT_KEY_DOWN && command && (character == '[' || character == ']');
        bool indent = event.type == CUI_EVENT_KEY_DOWN && !alt &&
                      ((event.key == CUI_KEY_TAB && !command) || block_indent) &&
                      !options.no_indent && !options.readonly;
        bool key = event.type == CUI_EVENT_KEY_DOWN &&
                   (event.key == CUI_KEY_ESCAPE || event.key == CUI_KEY_LEFT ||
                    event.key == CUI_KEY_RIGHT || event.key == CUI_KEY_HOME ||
                    event.key == CUI_KEY_END || event.key == CUI_KEY_BACKSPACE ||
                    event.key == CUI_KEY_DELETE || event.key == CUI_KEY_ENTER);
        if (!vertical && !pointer && !rectangle && !key && !clipboard && !indent &&
            event.type != CUI_EVENT_TEXT)
            continue;
        if (event.type != CUI_EVENT_TEXT &&
            !(event.type == CUI_EVENT_KEY_DOWN &&
              (event.key == CUI_KEY_BACKSPACE || event.key == CUI_KEY_DELETE) && !command && !alt))
            cui_edit_break_coalescing(editor->edit);
        if (event.key == CUI_KEY_ESCAPE && count == 1)
            continue;
        if (options.readonly && (indent || (clipboard && event.key == CUI_KEY_V) ||
                                 event.type == CUI_EVENT_TEXT || event.key == CUI_KEY_ENTER ||
                                 event.key == CUI_KEY_BACKSPACE || event.key == CUI_KEY_DELETE)) {
            cui_consume_event(ui, event_index);
            continue;
        }
        cui_textarea_error layout = cui_textarea_layout_ex(
            ui, editor->edit, &editor->view, metrics, width, !options.no_wrap, options.text_style);
        if (layout != CUI_TEXTAREA_OK) {
            result.error = layout;
            break;
        }
        live = cui_edit_get_selections(editor->edit, &count);
        cui_editor_cursor_sync(ui, editor, options.text_style, metrics.font_size);
        cui_editor_cursor_anchor anchors[CUI_EDIT_MAX_SELECTIONS];
        memcpy(anchors, editor->cursor_anchors, count * sizeof(*anchors));
        cui_edit_selection starting[CUI_EDIT_MAX_SELECTIONS];
        memcpy(starting, live, count * sizeof(*starting));
        size_t starting_count = count;
        cui_edit_column_anchor old_columns[CUI_EDIT_MAX_SELECTIONS];
        size_t old_column_count = 0;
        const cui_edit_column_anchor *column_live =
            cui_edit_get_columns(editor->edit, &old_column_count);
        if (column_live)
            memcpy(old_columns, column_live, old_column_count * sizeof(*old_columns));
        size_t reveal = SIZE_MAX;
        bool reveal_upstream = false, preserve = false, reveal_cursor = true;
        cui_edit_result error = CUI_EDIT_OK;
        if (rectangle) {
            cui_editor_column_point point;
            error = cui_editor_column_hit(ui, editor, event.x, event.y, &point);
            if (error == CUI_EDIT_OK)
                error = cui_editor_select_rectangle(editor, editor->column_start, point);
            if (error == CUI_EDIT_OK) {
                editor->column_pointer_x = event.x;
                editor->column_pointer_y = event.y;
                editor->column_dragged = true;
                reveal_cursor = false;
            }
        } else if (clipboard) {
            uint64_t old_revision = cui_edit_get_state(editor->edit).revision;
            cui_edit_selection old[CUI_EDIT_MAX_SELECTIONS];
            memcpy(old, live, count * sizeof(*old));
            error = cui_editor_multi_clipboard(editor, editor_options, event.key);
            size_t new_count;
            const cui_edit_selection *new_live = cui_edit_get_selections(editor->edit, &new_count);
            if (old_revision == cui_edit_get_state(editor->edit).revision && count == new_count &&
                !memcmp(old, new_live, count * sizeof(*old))) {
                preserve = true;
                reveal_cursor = false;
            }
        } else if (indent) {
            error = cui_edit_indent_selections(
                editor->edit, block_indent ? character == '[' : shift, block_indent,
                options.tab_size ? options.tab_size : 2, options.hard_tabs);
        } else if (pointer || add) {
            cui_edit_selection additions[CUI_EDIT_MAX_SELECTIONS];
            cui_editor_cursor_anchor added_anchors[CUI_EDIT_MAX_SELECTIONS];
            size_t added = 0;
            uint64_t next_id = cui_editor_next_cursor(editor, live, count);
            if (pointer) {
                bool affinity = editor->view.upstream_affinity;
                size_t offset =
                    cui_textarea_hit_test(ui, editor->edit, &editor->view, event.x, event.y);
                bool target_affinity = editor->view.upstream_affinity;
                editor->view.upstream_affinity = affinity;
                if (offset == SIZE_MAX || next_id == UINT64_MAX)
                    error = CUI_EDIT_BOUNDARY;
                else {
                    additions[0] = (cui_edit_selection){next_id, offset, offset};
                    added = 1;
                    size_t row =
                        cui_textarea_row_for_offset(&editor->view, offset, target_affinity);
                    added_anchors[0] = (cui_editor_cursor_anchor){
                        .id = next_id,
                        .anchor = offset,
                        .cursor = offset,
                        .x = cui_textarea_row_x(ui, editor->edit, &editor->view, row, offset),
                        .valid = true,
                        .upstream = target_affinity};
                }
                if (error == CUI_EDIT_OK) {
                    cui_editor_column_point point;
                    error = cui_editor_column_hit(ui, editor, event.x, event.y, &point);
                    if (error == CUI_EDIT_OK) {
                        editor->column_start = point;
                        editor->column_revision = cui_edit_get_state(editor->edit).revision;
                        editor->column_style = options.text_style;
                        editor->column_font_size = metrics.font_size;
                        editor->column_wrap_width = width;
                        editor->column_pointer_x = event.x;
                        editor->column_pointer_y = event.y;
                        editor->column_dragged = false;
                        editor->multi_pointer = true;
                        if (shift || (event.modifiers & CUI_MOD_CTRL)) {
                            error = cui_edit_select(editor->edit, point.offset, point.offset);
                            anchors[0] = added_anchors[0];
                            anchors[0].id = live[0].id;
                            added = 0;
                        }
                    }
                }
            } else {
                bool up = event.key == CUI_KEY_UP;
                for (size_t i = 0; i < count && error == CUI_EDIT_OK; ++i) {
                    size_t row = cui_textarea_row_for_offset(&editor->view, live[i].cursor,
                                                             anchors[i].upstream);
                    if (row == SIZE_MAX) {
                        error = CUI_EDIT_BOUNDARY;
                        break;
                    }
                    if (!anchors[i].valid) {
                        anchors[i].x = cui_textarea_row_x(ui, editor->edit, &editor->view, row,
                                                          live[i].cursor);
                        anchors[i].valid = true;
                    }
                    size_t target_row =
                        up ? (row ? row - 1 : row) : (row + 1 < editor->view.count ? row + 1 : row);
                    size_t offset = cui_textarea_row_offset(ui, editor->edit, &editor->view,
                                                            target_row, anchors[i].x);
                    if (offset == live[i].cursor)
                        continue;
                    if (next_id == UINT64_MAX) {
                        error = CUI_EDIT_CAPACITY;
                        break;
                    }
                    additions[added] = (cui_edit_selection){next_id, offset, offset};
                    added_anchors[added] = (cui_editor_cursor_anchor){
                        .id = next_id,
                        .anchor = offset,
                        .cursor = offset,
                        .x = anchors[i].x,
                        .valid = true,
                        .upstream = offset == editor->view.lines[target_row].end &&
                                    !editor->view.lines[target_row].hard_break};
                    ++added;
                    ++next_id;
                }
            }
            if (error == CUI_EDIT_OK)
                error = cui_edit_add_cursors(editor->edit, additions, added);
            if (error == CUI_EDIT_OK) {
                editor->next_cursor_id = next_id + (pointer && added ? 1 : 0);
                live = cui_edit_get_selections(editor->edit, &count);
                for (size_t i = 0; i < count; ++i) {
                    bool found = false;
                    for (size_t j = 0; j < editor->cursor_anchor_count; ++j)
                        if (live[i].id == anchors[j].id) {
                            anchors[i] = anchors[j];
                            found = true;
                            break;
                        }
                    if (!found)
                        for (size_t j = 0; j < added; ++j)
                            if (live[i].id == added_anchors[j].id) {
                                anchors[i] = added_anchors[j];
                                reveal = live[i].cursor;
                                reveal_upstream = anchors[i].upstream;
                                break;
                            }
                }
                preserve = true;
            }
        } else if (event.type == CUI_EVENT_TEXT || event.key == CUI_KEY_ENTER) {
            if (!options.readonly) {
                if (event.key == CUI_KEY_ENTER) {
                    result.pressed_enter = true;
                    result.enter_secondary = command;
                    result.enter_shift = shift;
                    if (!options.submit_on_enter || shift)
                        error = cui_edit_replace_selections(editor->edit, cui_string("\n"));
                } else {
                    size_t length = 0;
                    while (length < sizeof event.text && event.text[length])
                        ++length;
                    error =
                        event.text_data
                            ? cui_edit_type_selections(
                                  editor->edit, (cui_str){event.text_data, event.text_length})
                        : length == sizeof event.text
                            ? CUI_EDIT_INVALID_UTF8
                            : cui_edit_type_selections(editor->edit, (cui_str){event.text, length});
                }
            }
        } else if (event.key == CUI_KEY_BACKSPACE || event.key == CUI_KEY_DELETE) {
            if (!options.readonly)
                error = cui_edit_erase_selections_interactive(
                    editor->edit, event.key == CUI_KEY_BACKSPACE, command || alt);
        } else if (event.key == CUI_KEY_ESCAPE) {
            error = cui_edit_select(editor->edit, live[0].anchor, live[0].cursor);
        } else if (vertical &&
                   (!command || event.key == CUI_KEY_PAGEUP || event.key == CUI_KEY_PAGEDOWN)) {
            cui_edit_selection moved[CUI_EDIT_MAX_SELECTIONS];
            bool up = event.key == CUI_KEY_UP || event.key == CUI_KEY_PAGEUP;
            size_t step =
                event.key == CUI_KEY_PAGEUP || event.key == CUI_KEY_PAGEDOWN
                    ? (size_t)fmaxf(1, floorf(editor->view.viewport.h / metrics.line_height))
                    : 1;
            cui_str text = cui_edit_get_text(editor->edit);
            for (size_t i = 0; i < count; ++i) {
                moved[i] = live[i];
                size_t effective = live[i].cursor;
                if (!shift && live[i].anchor != live[i].cursor &&
                    (event.key == CUI_KEY_UP || event.key == CUI_KEY_DOWN)) {
                    size_t first =
                        live[i].anchor < live[i].cursor ? live[i].anchor : live[i].cursor;
                    size_t last = live[i].anchor > live[i].cursor ? live[i].anchor : live[i].cursor;
                    effective = up ? (first > 1 ? first - 2 : 0) : last;
                    effective = cui_editor_clip_grapheme(text, effective, !up);
                    effective = cui_textarea_project_offset(&editor->view, effective, up);
                    anchors[i].valid = false;
                    if (up || effective != live[i].cursor)
                        anchors[i].upstream = false;
                }
                size_t row =
                    cui_textarea_row_for_offset(&editor->view, effective, anchors[i].upstream);
                if (!anchors[i].valid) {
                    anchors[i].x =
                        cui_textarea_row_x(ui, editor->edit, &editor->view, row, effective);
                    anchors[i].valid = true;
                }
                size_t target =
                    up ? (step > row ? 0 : row - step)
                       : (step >= editor->view.count - row ? editor->view.count - 1 : row + step);
                size_t offset = shift && target == row && step == 1
                                    ? (up ? 0 : text.length)
                                    : cui_textarea_row_offset(ui, editor->edit, &editor->view,
                                                              target, anchors[i].x);
                moved[i].cursor = offset;
                if (!shift)
                    moved[i].anchor = offset;
                anchors[i].anchor = moved[i].anchor;
                anchors[i].cursor = offset;
                anchors[i].upstream = offset == editor->view.lines[target].end &&
                                      !editor->view.lines[target].hard_break;
            }
            error = cui_edit_select_many(editor->edit, moved, count);
            preserve = error == CUI_EDIT_OK;
        } else if (event.key == CUI_KEY_HOME || event.key == CUI_KEY_END) {
            if (command)
                error = cui_edit_move_selections(
                    editor->edit, event.key == CUI_KEY_HOME ? CUI_EDIT_START : CUI_EDIT_END, shift);
            else {
                cui_edit_selection moved[CUI_EDIT_MAX_SELECTIONS];
                for (size_t i = 0; i < count; ++i) {
                    moved[i] = live[i];
                    size_t row = cui_textarea_row_for_offset(&editor->view, live[i].cursor,
                                                             anchors[i].upstream);
                    moved[i].cursor = event.key == CUI_KEY_HOME ? editor->view.lines[row].start
                                                                : editor->view.lines[row].end;
                    if (!shift)
                        moved[i].anchor = moved[i].cursor;
                    anchors[i].anchor = moved[i].anchor;
                    anchors[i].cursor = moved[i].cursor;
                    anchors[i].valid = false;
                    anchors[i].upstream =
                        event.key == CUI_KEY_END && !editor->view.lines[row].hard_break;
                }
                error = cui_edit_select_many(editor->edit, moved, count);
                preserve = error == CUI_EDIT_OK;
            }
        } else if (vertical && command)
            error = cui_edit_move_selections(
                editor->edit, event.key == CUI_KEY_UP ? CUI_EDIT_START : CUI_EDIT_END, shift);
        else {
            cui_edit_selection moved[CUI_EDIT_MAX_SELECTIONS];
            bool left = event.key == CUI_KEY_LEFT;
            cui_edit_motion motion =
                left
                    ? (command || alt ? CUI_EDIT_UNICODE_WORD_PREVIOUS : CUI_EDIT_GRAPHEME_PREVIOUS)
                    : (command || alt ? CUI_EDIT_UNICODE_WORD_NEXT : CUI_EDIT_GRAPHEME_NEXT);
            for (size_t i = 0; i < count && error == CUI_EDIT_OK; ++i) {
                moved[i] = live[i];
                size_t row =
                    cui_textarea_row_for_offset(&editor->view, live[i].cursor, anchors[i].upstream);
                bool affinity =
                    !command && !alt && live[i].anchor == live[i].cursor &&
                    ((!left && anchors[i].upstream && !editor->view.lines[row].hard_break &&
                      editor->view.lines[row].end == live[i].cursor &&
                      row + 1 < editor->view.count) ||
                     (left && !anchors[i].upstream && row &&
                      !editor->view.lines[row - 1].hard_break &&
                      editor->view.lines[row - 1].end == live[i].cursor));
                if (affinity)
                    anchors[i].upstream = left;
                else {
                    error =
                        cui_edit_selection_motion(editor->edit, live[i], motion, shift, &moved[i]);
                    anchors[i].upstream = false;
                }
                anchors[i].anchor = moved[i].anchor;
                anchors[i].cursor = moved[i].cursor;
                anchors[i].valid = false;
            }
            if (error == CUI_EDIT_OK)
                error = cui_edit_select_many(editor->edit, moved, count);
            preserve = error == CUI_EDIT_OK;
        }
        cui_consume_event(ui, event_index);
        if (error != CUI_EDIT_OK) {
            result.edit_error = error;
            continue;
        }
        size_t after_count;
        live = cui_edit_get_selections(editor->edit, &after_count);
        cui_editor_cursor_sync(ui, editor, options.text_style, metrics.font_size);
        if (preserve) {
            for (size_t i = 0; i < after_count; ++i)
                for (size_t j = 0; j < count; ++j)
                    if (editor->cursor_anchors[i].id == anchors[j].id) {
                        editor->cursor_anchors[i] = anchors[j];
                        break;
                    }
        } else
            for (size_t i = 0; i < after_count; ++i) {
                editor->cursor_anchors[i].valid = false;
                editor->cursor_anchors[i].upstream = false;
            }
        count = after_count;
        cui_editor_save_columns(editor, column_live ? old_columns : NULL, old_column_count,
                                starting, starting_count, vertical);
        editor->view.upstream_affinity = editor->cursor_anchors[0].upstream;
        editor->view.preferred_valid = false;
        editor->view.dragging = false;
        layout = cui_textarea_layout_ex(ui, editor->edit, &editor->view, metrics, width,
                                        !options.no_wrap, options.text_style);
        if (layout != CUI_TEXTAREA_OK) {
            result.error = layout;
            break;
        }
        if (reveal == SIZE_MAX) {
            reveal = live[0].cursor;
            reveal_upstream = editor->cursor_anchors[0].upstream;
        }
        if (reveal_cursor)
            cui_editor_multi_reveal(ui, editor, reveal, reveal_upstream);
        cui_invalidate(ui);
    }
    if (editor->multi_pointer && editor->column_dragged && !options.disabled &&
        cui_focused(ui) == id &&
        editor->column_revision == cui_edit_get_state(editor->edit).revision) {
        float delta = cui_editor_column_scroll_delta(editor, editor->column_pointer_y);
        float old_y = editor->view.scroll_y;
        float next = fminf(editor->view.maximum_y,
                           fmaxf(0, old_y + delta * fminf(.1f, cui_frame_delta(ui)) / .016f));
        if (next != old_y) {
            editor->view.scroll_y = next;
            cui_editor_column_point point;
            cui_edit_result error = cui_editor_column_hit(ui, editor, editor->column_pointer_x,
                                                          editor->column_pointer_y, &point);
            if (error == CUI_EDIT_OK)
                error = cui_editor_select_rectangle(editor, editor->column_start, point);
            if (error != CUI_EDIT_OK) {
                editor->view.scroll_y = old_y;
                result.edit_error = error;
            } else {
                cui_editor_cursor_sync(ui, editor, options.text_style, metrics.font_size);
                cui_textarea_layout_ex(ui, editor->edit, &editor->view, metrics, width,
                                       !options.no_wrap, options.text_style);
                cui_invalidate(ui);
            }
        }
    }
    live = cui_edit_get_selections(editor->edit, &count);
    result.scrolled = before_x != editor->view.scroll_x || before_y != editor->view.scroll_y;
    result.changed = before_revision != cui_edit_get_state(editor->edit).revision;
    result.selection_changed = before_count != count || memcmp(before, live, count * sizeof(*live));
    return result;
}
cui_textarea_result cui_base_editor(cui_context *ui, cui_id id, cui_str name, cui_editor_state *s,
                                    cui_editor_options o, cui_textarea_metrics metrics) {
    if (!ui || !id || !s || !s->edit || s->clipboard_active ||
        cui_edit_get_content(s->edit).token_count || !s->view.lines || !s->view.capacity ||
        !isfinite(metrics.font_size) || metrics.font_size <= 0 || !isfinite(metrics.line_height) ||
        metrics.line_height <= 0 || o.textarea.text_style.weight < CUI_TEXT_WEIGHT_NORMAL ||
        o.textarea.text_style.weight > CUI_TEXT_WEIGHT_BOLD ||
        !isfinite(o.textarea.text_style.letter_spacing) || (name.length && !name.data) ||
        (o.language.length && !o.language.data))
        return (cui_textarea_result){.error = CUI_TEXTAREA_INVALID};
    size_t selection_capacity = cui_edit_selection_capacity(s->edit);
    if (selection_capacity &&
        (!s->cursor_anchors || s->cursor_anchor_capacity < selection_capacity ||
         s->cursor_anchor_capacity > CUI_EDIT_MAX_SELECTIONS))
        return (cui_textarea_result){.error = CUI_TEXTAREA_INVALID,
                                     .edit_error = CUI_EDIT_INVALID_ARGUMENT};
    if (o.search && o.search->active) {
        cui_edit_result synced = cui_editor_matcher_sync(s, o.search);
        if (synced != CUI_EDIT_OK)
            return (cui_textarea_result){.error = CUI_TEXTAREA_INVALID, .edit_error = synced};
    }
    if (selection_capacity)
        cui_editor_reveal_all_folds(s);
    if (s->view.initialized && s->view.text_metrics_revision != cui_text_metrics_revision(ui)) {
        s->cursor_anchor_count = 0;
        s->view.preferred_valid = false;
        s->multi_pointer = false;
        s->column_dragged = false;
    }
    cui_textarea_state probe = s->view;
    float width = probe.viewport.w > 0 ? probe.viewport.w : probe.wrap_width;
    cui_textarea_error layout_error = cui_textarea_layout_ex(
        ui, s->edit, &probe, metrics, width, !o.textarea.no_wrap, o.textarea.text_style);
    if (layout_error != CUI_TEXTAREA_OK)
        return (cui_textarea_result){.error = layout_error};
    cui_edit_state before = cui_edit_get_state(s->edit);
    size_t saved_count;
    const cui_edit_selection *saved_live = cui_edit_get_selections(s->edit, &saved_count);
    cui_edit_selection saved[CUI_EDIT_MAX_SELECTIONS];
    if (saved_live)
        memcpy(saved, saved_live, saved_count * sizeof(*saved));
    if (s->view.composition && s->view.composition->active)
        cui_editor_cancel_completion(s);
    cui_edit_result completion_error = cui_editor_completion_events(ui, id, s, o.textarea);
    cui_textarea_result multiple = cui_editor_multi_events(ui, id, s, o, metrics, width);
    if (multiple.error != CUI_TEXTAREA_OK)
        return multiple;
    size_t current_count;
    cui_edit_get_selections(s->edit, &current_count);
    (void)current_count;
    cui_textarea_result result =
        cui_base_textarea(ui, id, name, s->edit, &s->view, o.textarea, metrics);
    result.changed |= multiple.changed;
    result.selection_changed |= multiple.selection_changed;
    result.scrolled |= multiple.scrolled;
    if (multiple.pressed_enter) {
        result.pressed_enter = true;
        result.enter_secondary = multiple.enter_secondary;
        result.enter_shift = multiple.enter_shift;
    }
    if (multiple.edit_error != CUI_EDIT_OK)
        result.edit_error = multiple.edit_error;
    if (selection_capacity && !(s->view.composition && s->view.composition->active))
        cui_editor_cursor_sync(ui, s, o.textarea.text_style, metrics.font_size);
    cui_edit_state after = cui_edit_get_state(s->edit);
    result.changed |= before.revision != after.revision;
    result.selection_changed |= before.cursor != after.cursor || before.anchor != after.anchor;
    if (saved_live) {
        size_t final_count;
        const cui_edit_selection *final_live = cui_edit_get_selections(s->edit, &final_count);
        result.selection_changed |=
            saved_count != final_count || memcmp(saved, final_live, final_count * sizeof(*saved));
    }
    if (completion_error != CUI_EDIT_OK)
        result.edit_error = completion_error;
    if (s->completion.open &&
        (s->completion.revision != after.revision || result.selection_changed))
        cui_editor_cancel_completion(s);
    if (o.search && o.search->active) {
        cui_edit_result synced = cui_editor_matcher_sync(s, o.search);
        if (synced != CUI_EDIT_OK) {
            result.error = CUI_TEXTAREA_INVALID;
            result.edit_error = synced;
        }
    }
    return result;
}

bool cui_editor_search_init(cui_editor_search_state *s, cui_edit *query) {
    if (!s || !query || cui_edit_get_state(query).multiline)
        return false;
    *s = (cui_editor_search_state){.query = query};
    return true;
}
size_t cui_editor_match_count(const cui_editor_state *s, cui_str query) {
    if (!s || !s->edit || !query.length || !query.data || !cui_edit_valid_utf8(query))
        return 0;
    cui_str text = cui_edit_get_text(s->edit);
    if (query.length > text.length)
        return 0;
    size_t count = 0, i = 0;
    while (i <= text.length - query.length) {
        if (cui_editor_boundary(text, i) && cui_editor_boundary(text, i + query.length) &&
            !memcmp(text.data + i, query.data, query.length)) {
            ++count;
            i += query.length;
        } else {
            ++i;
        }
    }
    return count;
}

static bool cui_editor_replace_valid(cui_editor_state *s, cui_str query, cui_str replacement) {
    return s && s->edit && !cui_edit_get_content(s->edit).token_count && query.length &&
           cui_edit_valid_utf8(query) && cui_edit_valid_utf8(replacement);
}
cui_edit_result cui_editor_replace_current(cui_editor_state *s, cui_str query,
                                           cui_str replacement) {
    if (!cui_editor_replace_valid(s, query, replacement))
        return CUI_EDIT_INVALID_ARGUMENT;
    cui_edit_state state = cui_edit_get_state(s->edit);
    size_t start = state.cursor < state.anchor ? state.cursor : state.anchor;
    size_t end = state.cursor > state.anchor ? state.cursor : state.anchor;
    cui_str text = cui_edit_get_text(s->edit);
    if (end - start != query.length || memcmp(text.data + start, query.data, query.length))
        return CUI_EDIT_BOUNDARY;
    cui_edit_result result = cui_edit_replace_range(s->edit, start, end, replacement);
    if (result == CUI_EDIT_OK)
        cui_editor_cancel_completion(s);
    return result;
}
static bool cui_editor_storage_overlap(const void *a, size_t an, const void *b, size_t bn) {
    if (!an || !bn)
        return false;
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return x >= y ? x - y < bn : y - x < an;
}
cui_edit_result cui_editor_replace_all(cui_editor_state *s, cui_str query, cui_str replacement,
                                       char *scratch, size_t capacity, size_t *count) {
    if (!cui_editor_replace_valid(s, query, replacement) || !scratch || !count)
        return CUI_EDIT_INVALID_ARGUMENT;
    cui_str text = cui_edit_get_text(s->edit);
    if (cui_editor_storage_overlap(scratch, capacity, text.data, text.length + 1) ||
        cui_editor_storage_overlap(scratch, capacity, query.data, query.length) ||
        cui_editor_storage_overlap(scratch, capacity, replacement.data, replacement.length) ||
        cui_editor_storage_overlap(scratch, capacity, count, sizeof(*count)) ||
        cui_editor_storage_overlap(count, sizeof(*count), text.data, text.length + 1))
        return CUI_EDIT_INVALID_ARGUMENT;
    size_t matches = cui_editor_match_count(s, query);
    if (!matches) {
        *count = 0;
        return CUI_EDIT_OK;
    }
    size_t length = text.length - matches * query.length;
    if (replacement.length && matches > (SIZE_MAX - length) / replacement.length)
        return CUI_EDIT_CAPACITY;
    length += matches * replacement.length;
    if (length >= capacity)
        return CUI_EDIT_CAPACITY;
    size_t at = 0, out = 0;
    while (at < text.length) {
        if (query.length <= text.length - at && cui_editor_boundary(text, at) &&
            cui_editor_boundary(text, at + query.length) &&
            !memcmp(text.data + at, query.data, query.length)) {
            if (replacement.length)
                memcpy(scratch + out, replacement.data, replacement.length);
            out += replacement.length;
            at += query.length;
        } else
            scratch[out++] = text.data[at++];
    }
    scratch[out] = 0;
    cui_edit_result result =
        cui_edit_replace_range(s->edit, 0, text.length, (cui_str){scratch, length});
    if (result != CUI_EDIT_OK)
        return result;
    cui_edit_select(s->edit, 0, 0);
    s->view.scroll_x = s->view.scroll_y = 0;
    cui_editor_cancel_completion(s);
    *count = matches;
    return CUI_EDIT_OK;
}

bool cui_editor_multicursor_init(cui_editor_state *editor, cui_editor_cursor_anchor *anchors,
                                 size_t capacity) {
    if (!editor || !editor->edit || !anchors ||
        capacity < cui_edit_selection_capacity(editor->edit) ||
        !cui_edit_selection_capacity(editor->edit) || capacity > CUI_EDIT_MAX_SELECTIONS)
        return false;
    size_t count;
    const cui_edit_selection *live = cui_edit_get_selections(editor->edit, &count);
    cui_str text = cui_edit_get_text(editor->edit);
    size_t bytes = capacity * sizeof(*anchors);
    if (cui_editor_storage_overlap(anchors, bytes, editor->view.lines,
                                   editor->view.capacity * sizeof(*editor->view.lines)) ||
        cui_editor_storage_overlap(anchors, bytes, editor, sizeof(*editor)) ||
        cui_editor_storage_overlap(anchors, bytes, text.data, text.length + 1) ||
        cui_editor_storage_overlap(anchors, bytes, live,
                                   cui_edit_selection_capacity(editor->edit) * sizeof(*live)))
        return false;
    editor->cursor_anchors = anchors;
    editor->cursor_anchor_capacity = capacity;
    editor->cursor_anchor_count = 0;
    editor->cursor_anchor_revision = UINT64_MAX;
    editor->cursor_anchor_font_size = 0;
    editor->next_cursor_id = 1;
    editor->multi_pointer = false;
    return true;
}
static unsigned char cui_editor_ascii_fold(unsigned char c, bool insensitive) {
    return insensitive && c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}
static size_t cui_editor_scan_matches(cui_str text, cui_str query, bool insensitive,
                                      cui_editor_match *output) {
    if (!query.length || query.length > text.length)
        return 0;
    size_t count = 0, at = 0;
    while (at <= text.length - query.length) {
        bool match = cui_editor_boundary(text, at) && cui_editor_boundary(text, at + query.length);
        for (size_t j = 0; match && j < query.length; ++j)
            match = cui_editor_ascii_fold((unsigned char)text.data[at + j], insensitive) ==
                    cui_editor_ascii_fold((unsigned char)query.data[j], insensitive);
        if (match) {
            if (output)
                output[count] = (cui_editor_match){at, at + query.length};
            ++count;
            at += query.length;
        } else
            ++at;
    }
    return count;
}
static uint64_t cui_editor_query_hash(cui_str query) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < query.length; ++i) {
        hash ^= (unsigned char)query.data[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
bool cui_editor_matcher_init(cui_editor_matcher *matcher, cui_editor_match *matches,
                             size_t capacity) {
    if (!matcher || (capacity && !matches) || capacity > SIZE_MAX / sizeof(*matches) ||
        cui_editor_storage_overlap(matcher, sizeof(*matcher), matches, capacity * sizeof(*matches)))
        return false;
    *matcher =
        (cui_editor_matcher){.matches = matches, .capacity = capacity, .case_insensitive = true};
    return true;
}
static cui_edit_result cui_editor_matcher_preflight(cui_editor_state *editor,
                                                    const cui_editor_matcher *matcher,
                                                    cui_str query, cui_str text, bool insensitive,
                                                    size_t *count) {
    if (!editor || !editor->edit || !matcher || (matcher->capacity && !matcher->matches) ||
        matcher->capacity > SIZE_MAX / sizeof(*matcher->matches) ||
        cui_edit_get_content(editor->edit).token_count || !cui_edit_valid_utf8(query) ||
        cui_editor_storage_overlap(query.data, query.length, cui_edit_get_text(editor->edit).data,
                                   cui_edit_get_text(editor->edit).length + 1) ||
        cui_editor_storage_overlap(matcher->matches, matcher->capacity * sizeof(*matcher->matches),
                                   text.data, text.length + 1) ||
        cui_editor_storage_overlap(matcher->matches, matcher->capacity * sizeof(*matcher->matches),
                                   query.data, query.length))
        return CUI_EDIT_INVALID_ARGUMENT;
    *count = cui_editor_scan_matches(text, query, insensitive, NULL);
    return *count > matcher->capacity ? CUI_EDIT_CAPACITY : CUI_EDIT_OK;
}
cui_edit_result cui_editor_matcher_set_query(cui_editor_state *editor, cui_editor_matcher *matcher,
                                             cui_str query, bool insensitive) {
    if (!editor || !editor->edit)
        return CUI_EDIT_INVALID_ARGUMENT;
    cui_str text = cui_edit_get_text(editor->edit);
    size_t count;
    cui_edit_result result =
        cui_editor_matcher_preflight(editor, matcher, query, text, insensitive, &count);
    if (result != CUI_EDIT_OK)
        return result;
    uint64_t hash = cui_editor_query_hash(query),
             revision = cui_edit_get_state(editor->edit).revision;
    bool same = matcher->initialized && matcher->document == editor->edit &&
                matcher->query.length == query.length && matcher->query_hash == hash &&
                matcher->case_insensitive == insensitive && matcher->revision == revision;
    cui_editor_scan_matches(text, query, insensitive, matcher->matches);
    matcher->query = query;
    matcher->query_hash = hash;
    matcher->case_insensitive = insensitive;
    matcher->document = editor->edit;
    matcher->revision = revision;
    matcher->count = count;
    if (!same || matcher->current >= count)
        matcher->current = 0;
    matcher->active = matcher->initialized = true;
    return CUI_EDIT_OK;
}
cui_edit_result cui_editor_matcher_sync(cui_editor_state *editor, cui_editor_matcher *matcher) {
    if (!editor || !editor->edit || !matcher || !matcher->initialized)
        return CUI_EDIT_INVALID_ARGUMENT;
    bool active = matcher->active;
    cui_edit_result result =
        cui_editor_matcher_set_query(editor, matcher, matcher->query, matcher->case_insensitive);
    if (result == CUI_EDIT_OK)
        matcher->active = active;
    return result;
}
void cui_editor_matcher_close(cui_editor_matcher *matcher) {
    if (matcher)
        matcher->active = false;
}
cui_edit_result cui_editor_matcher_navigate(cui_editor_state *editor, cui_editor_matcher *matcher,
                                            bool reverse) {
    cui_edit_result result = cui_editor_matcher_sync(editor, matcher);
    if (result != CUI_EDIT_OK)
        return result;
    if (!matcher->count)
        return CUI_EDIT_BOUNDARY;
    size_t next = reverse ? (matcher->current ? matcher->current - 1 : matcher->count - 1)
                          : (matcher->current + 1 == matcher->count ? 0 : matcher->current + 1);
    cui_editor_match range = matcher->matches[next];
    result = cui_edit_select(editor->edit, range.start, range.end);
    if (result == CUI_EDIT_OK)
        matcher->current = next;
    return result;
}
cui_edit_result cui_editor_matcher_anchor(cui_editor_state *editor, cui_editor_matcher *matcher,
                                          size_t offset) {
    if (!editor || !editor->edit || offset > cui_edit_get_text(editor->edit).length ||
        !cui_editor_boundary(cui_edit_get_text(editor->edit), offset))
        return CUI_EDIT_BOUNDARY;
    cui_edit_result result = cui_editor_matcher_sync(editor, matcher);
    if (result != CUI_EDIT_OK)
        return result;
    if (!matcher->count)
        return CUI_EDIT_BOUNDARY;
    size_t current = 0;
    while (current + 1 < matcher->count && matcher->matches[current].end < offset)
        ++current;
    matcher->current = current;
    return CUI_EDIT_OK;
}
cui_edit_result cui_editor_matcher_replace_current(cui_editor_state *editor,
                                                   cui_editor_matcher *matcher, cui_str replacement,
                                                   char *scratch, size_t capacity) {
    if (!editor || !editor->edit || !matcher || !matcher->initialized || !scratch ||
        !cui_edit_valid_utf8(replacement))
        return CUI_EDIT_INVALID_ARGUMENT;
    /* Probe synchronization without modifying caller session/ranges on rejection. */
    cui_str text = cui_edit_get_text(editor->edit);
    size_t count;
    cui_edit_result result = cui_editor_matcher_preflight(editor, matcher, matcher->query, text,
                                                          matcher->case_insensitive, &count);
    if (result != CUI_EDIT_OK)
        return result;
    if (!count)
        return CUI_EDIT_BOUNDARY;
    size_t current = matcher->revision == cui_edit_get_state(editor->edit).revision &&
                             matcher->document == editor->edit && matcher->current < count
                         ? matcher->current
                         : 0;
    /* Obtain current range without writing match storage. */
    cui_editor_match range = {0};
    size_t at = 0, index = 0;
    while (at <= text.length - matcher->query.length) {
        cui_str suffix = {text.data + at, text.length - at};
        bool match =
            cui_editor_boundary(text, at) && cui_editor_boundary(text, at + matcher->query.length);
        for (size_t j = 0; match && j < matcher->query.length; ++j)
            match =
                cui_editor_ascii_fold((unsigned char)suffix.data[j], matcher->case_insensitive) ==
                cui_editor_ascii_fold((unsigned char)matcher->query.data[j],
                                      matcher->case_insensitive);
        if (match) {
            if (index++ == current) {
                range = (cui_editor_match){at, at + matcher->query.length};
                break;
            }
            at += matcher->query.length;
        } else
            ++at;
    }
    size_t remaining = text.length - (range.end - range.start);
    if (replacement.length > SIZE_MAX - remaining || remaining + replacement.length >= capacity)
        return CUI_EDIT_CAPACITY;
    size_t bytes = matcher->capacity * sizeof(*matcher->matches);
    if (cui_editor_storage_overlap(scratch, capacity, text.data, text.length + 1) ||
        cui_editor_storage_overlap(scratch, capacity, matcher->query.data, matcher->query.length) ||
        cui_editor_storage_overlap(scratch, capacity, replacement.data, replacement.length) ||
        cui_editor_storage_overlap(scratch, capacity, matcher->matches, bytes))
        return CUI_EDIT_INVALID_ARGUMENT;
    memcpy(scratch, text.data, range.start);
    if (replacement.length)
        memcpy(scratch + range.start, replacement.data, replacement.length);
    memcpy(scratch + range.start + replacement.length, text.data + range.end,
           text.length - range.end);
    cui_str candidate = {scratch, remaining + replacement.length};
    scratch[candidate.length] = 0;
    size_t next_count =
        cui_editor_scan_matches(candidate, matcher->query, matcher->case_insensitive, NULL);
    if (next_count > matcher->capacity)
        return CUI_EDIT_CAPACITY;
    result = cui_edit_replace_range(editor->edit, range.start, range.end, replacement);
    if (result != CUI_EDIT_OK)
        return result;
    cui_editor_scan_matches(cui_edit_get_text(editor->edit), matcher->query,
                            matcher->case_insensitive, matcher->matches);
    matcher->current = current + 1 < count ? current : 0;
    if (matcher->current >= next_count)
        matcher->current = next_count ? next_count - 1 : 0;
    matcher->count = next_count;
    matcher->document = editor->edit;
    matcher->revision = cui_edit_get_state(editor->edit).revision;
    matcher->active = true;
    cui_editor_cancel_completion(editor);
    return CUI_EDIT_OK;
}

cui_edit_result cui_editor_matcher_replace_all(cui_editor_state *editor,
                                               cui_editor_matcher *matcher, cui_str replacement,
                                               char *scratch, size_t capacity, size_t *replaced) {
    if (!editor || !editor->edit || !matcher || !matcher->initialized || !scratch || !replaced ||
        !cui_edit_valid_utf8(replacement))
        return CUI_EDIT_INVALID_ARGUMENT;
    cui_str text = cui_edit_get_text(editor->edit);
    size_t count;
    cui_edit_result result = cui_editor_matcher_preflight(editor, matcher, matcher->query, text,
                                                          matcher->case_insensitive, &count);
    if (result != CUI_EDIT_OK)
        return result;
    size_t bytes = matcher->capacity * sizeof(*matcher->matches);
    if (cui_editor_storage_overlap(scratch, capacity, text.data, text.length + 1) ||
        cui_editor_storage_overlap(scratch, capacity, matcher->query.data, matcher->query.length) ||
        cui_editor_storage_overlap(scratch, capacity, replacement.data, replacement.length) ||
        cui_editor_storage_overlap(scratch, capacity, matcher->matches, bytes) ||
        cui_editor_storage_overlap(scratch, capacity, replaced, sizeof(*replaced)) ||
        cui_editor_storage_overlap(replaced, sizeof(*replaced), text.data, text.length + 1) ||
        cui_editor_storage_overlap(replaced, sizeof(*replaced), matcher, sizeof(*matcher)) ||
        cui_editor_storage_overlap(replaced, sizeof(*replaced), matcher->matches, bytes) ||
        cui_editor_storage_overlap(replaced, sizeof(*replaced), matcher->query.data,
                                   matcher->query.length))
        return CUI_EDIT_INVALID_ARGUMENT;
    if (!count) {
        *replaced = 0;
        return CUI_EDIT_OK;
    }
    size_t length = text.length - count * matcher->query.length;
    if (replacement.length && count > (SIZE_MAX - length) / replacement.length)
        return CUI_EDIT_CAPACITY;
    length += count * replacement.length;
    if (length >= capacity)
        return CUI_EDIT_CAPACITY;
    size_t at = 0, out = 0;
    while (at < text.length) {
        bool match = matcher->query.length <= text.length - at && cui_editor_boundary(text, at) &&
                     cui_editor_boundary(text, at + matcher->query.length);
        for (size_t j = 0; match && j < matcher->query.length; ++j)
            match = cui_editor_ascii_fold((unsigned char)text.data[at + j],
                                          matcher->case_insensitive) ==
                    cui_editor_ascii_fold((unsigned char)matcher->query.data[j],
                                          matcher->case_insensitive);
        if (match) {
            if (replacement.length)
                memcpy(scratch + out, replacement.data, replacement.length);
            at += matcher->query.length;
            out += replacement.length;
        } else
            scratch[out++] = text.data[at++];
    }
    scratch[out] = 0;
    cui_str candidate = {scratch, length};
    size_t next_count =
        cui_editor_scan_matches(candidate, matcher->query, matcher->case_insensitive, NULL);
    if (next_count > matcher->capacity)
        return CUI_EDIT_CAPACITY;
    result = cui_edit_replace_range(editor->edit, 0, text.length, candidate);
    if (result != CUI_EDIT_OK)
        return result;
    cui_editor_scan_matches(cui_edit_get_text(editor->edit), matcher->query,
                            matcher->case_insensitive, matcher->matches);
    matcher->count = next_count;
    matcher->current = 0;
    matcher->document = editor->edit;
    matcher->revision = cui_edit_get_state(editor->edit).revision;
    cui_edit_select(editor->edit, 0, 0);
    editor->view.scroll_x = editor->view.scroll_y = 0;
    cui_editor_cancel_completion(editor);
    *replaced = count;
    return CUI_EDIT_OK;
}
bool cui_editor_search_enable_replace(cui_editor_search_state *state, cui_editor_matcher *matcher,
                                      cui_edit *replacement, char *scratch, size_t capacity) {
    if (!state || !state->query || !matcher || !replacement || replacement == state->query ||
        cui_edit_get_state(replacement).multiline || !scratch || !capacity ||
        matcher->capacity > SIZE_MAX / sizeof(*matcher->matches) ||
        (matcher->capacity && !matcher->matches))
        return false;
    state->matcher = matcher;
    state->replacement = replacement;
    state->scratch = scratch;
    state->scratch_capacity = capacity;
    state->replace_mode = false;
    return true;
}

cui_edit_result cui_editor_search_open(cui_editor_state *editor, cui_editor_search_state *search,
                                       bool replace_mode) {
    if (!editor || !editor->edit || !search || !search->query || search->query == editor->edit ||
        cui_edit_get_state(search->query).multiline ||
        cui_edit_get_content(search->query).token_count)
        return CUI_EDIT_INVALID_ARGUMENT;
    if (!search->matcher) {
        search->open = true;
        search->replace_mode = false;
        search->activation_revision++;
        return CUI_EDIT_OK;
    }
    cui_editor_matcher *matcher = search->matcher;
    cui_str text = cui_edit_get_text(editor->edit), query = cui_edit_get_text(search->query);
    cui_edit_state selected = cui_edit_get_state(editor->edit);
    size_t start = selected.anchor < selected.cursor ? selected.anchor : selected.cursor;
    size_t end = selected.anchor > selected.cursor ? selected.anchor : selected.cursor;
    cui_str candidate = start == end ? query : (cui_str){text.data + start, end - start};
    size_t ignored;
    cui_edit_result result = cui_editor_matcher_preflight(editor, matcher, (cui_str){0}, text,
                                                          matcher->case_insensitive, &ignored);
    if (result != CUI_EDIT_OK)
        return result;
    size_t bytes = matcher->capacity * sizeof(*matcher->matches);
    size_t query_bytes = (candidate.length > query.length ? candidate.length : query.length) + 1;
    if (cui_editor_storage_overlap(matcher->matches, bytes, query.data, query_bytes) ||
        cui_editor_storage_overlap(query.data, query_bytes, text.data, text.length + 1))
        return CUI_EDIT_INVALID_ARGUMENT;
    size_t count = cui_editor_scan_matches(text, candidate, matcher->case_insensitive, NULL);
    if (count > matcher->capacity)
        return CUI_EDIT_CAPACITY;
    bool changed = candidate.length != query.length ||
                   (candidate.length && memcmp(candidate.data, query.data, candidate.length));
    if (start != end) {
        result = cui_edit_replace_range(search->query, 0, query.length, candidate);
        if (result != CUI_EDIT_OK)
            return result;
    }
    result = cui_editor_matcher_set_query(editor, matcher, cui_edit_get_text(search->query),
                                          matcher->case_insensitive);
    if (result != CUI_EDIT_OK)
        return result;
    if (changed && matcher->count && editor->view.initialized && editor->view.count &&
        isfinite(editor->view.scroll_y) && editor->view.scroll_y >= 0 &&
        isfinite(editor->view.line_height) && editor->view.line_height > 0) {
        size_t row = (size_t)fminf(floorf(editor->view.scroll_y / editor->view.line_height),
                                   (float)(editor->view.count - 1));
        size_t offset = editor->view.lines[row].start;
        size_t current = 0;
        while (current + 1 < matcher->count && matcher->matches[current].end < offset)
            ++current;
        matcher->current = current;
    }
    search->open = true;
    search->replace_mode = replace_mode && search->replacement && !search->replace_disabled;
    search->activation_revision++;
    return CUI_EDIT_OK;
}

bool cui_editor_set_folds(cui_editor_state *state, cui_editor_fold *folds, size_t count) {
    return state && cui_textarea_set_folds(&state->view, state->edit, folds, count);
}
cui_edit_result cui_editor_set_folded(cui_editor_state *state, size_t index, bool collapsed) {
    if (!state || !state->edit || index >= state->view.fold_count ||
        state->view.fold_revision != cui_edit_get_state(state->edit).revision)
        return CUI_EDIT_INVALID_ARGUMENT;
    if (!cui_textarea_set_folds(&state->view, state->edit, state->view.folds,
                                state->view.fold_count))
        return CUI_EDIT_INVALID_ARGUMENT;
    cui_editor_fold *fold = &state->view.folds[index];
    cui_str text = cui_edit_get_text(state->edit);
    cui_edit_state selection = cui_edit_get_state(state->edit);
    size_t line = 0, cursor_line = 0, anchor_line = 0, opening_end = 0;
    for (size_t i = 0; i <= text.length; ++i) {
        if (i < text.length && text.data[i] != '\r' && text.data[i] != '\n')
            continue;
        if (line == fold->start_line)
            opening_end = i;
        if (i == text.length)
            break;
        size_t next = i + 1;
        if (text.data[i] == '\r' && next < text.length && text.data[next] == '\n')
            ++next;
        if (next <= selection.cursor)
            ++cursor_line;
        if (next <= selection.anchor)
            ++anchor_line;
        ++line;
        i = next - 1;
    }
    if (collapsed && ((cursor_line > fold->start_line && cursor_line < fold->end_line) ||
                      (anchor_line > fold->start_line && anchor_line < fold->end_line))) {
        cui_edit_result error = cui_edit_select(state->edit, opening_end, opening_end);
        if (error != CUI_EDIT_OK)
            return error;
    }
    fold->collapsed = collapsed;
    state->view.preferred_valid = false;
    return CUI_EDIT_OK;
}

void cui_editor_cancel_completion(cui_editor_state *state) {
    if (state)
        state->completion = (cui_editor_completion_state){0};
}
cui_edit_result cui_editor_show_completions(cui_editor_state *state,
                                            const cui_editor_completion_item *items, size_t count,
                                            size_t start, size_t end) {
    if (!state || !state->edit || count > 65536 || (count && !items))
        return CUI_EDIT_INVALID_ARGUMENT;
    if (!count) {
        cui_editor_cancel_completion(state);
        return CUI_EDIT_OK;
    }
    cui_str text = cui_edit_get_text(state->edit);
    if (start > end || end > text.length || !cui_editor_boundary(text, start) ||
        !cui_editor_boundary(text, end))
        return CUI_EDIT_INVALID_ARGUMENT;
    for (size_t i = 0; i < count; ++i) {
        if (!items[i].label.length || !items[i].label.data ||
            (items[i].detail.length && !items[i].detail.data) ||
            (items[i].insert_text.length && !items[i].insert_text.data))
            return CUI_EDIT_INVALID_ARGUMENT;
        if (!cui_edit_valid_utf8(items[i].label) || !cui_edit_valid_utf8(items[i].detail) ||
            !cui_edit_valid_utf8(items[i].insert_text))
            return CUI_EDIT_INVALID_UTF8;
    }
    cui_edit_state selection = cui_edit_get_state(state->edit);
    state->completion =
        (cui_editor_completion_state){.items = items,
                                      .count = count,
                                      .replace_start = start,
                                      .replace_end = end,
                                      .cursor = selection.cursor,
                                      .anchor = selection.anchor,
                                      .revision = cui_edit_get_state(state->edit).revision,
                                      .open = true};
    return CUI_EDIT_OK;
}
cui_edit_result cui_editor_accept_completion(cui_editor_state *state) {
    if (!state || !state->edit || !state->completion.open || !state->completion.items ||
        state->completion.selected >= state->completion.count)
        return CUI_EDIT_INVALID_ARGUMENT;
    if (state->completion.revision != cui_edit_get_state(state->edit).revision) {
        cui_editor_cancel_completion(state);
        return CUI_EDIT_INVALID_ARGUMENT;
    }
    cui_edit_result result = cui_edit_replace_range(
        state->edit, state->completion.replace_start, state->completion.replace_end,
        state->completion.items[state->completion.selected].insert_text);
    if (result == CUI_EDIT_OK)
        cui_editor_cancel_completion(state);
    return result;
}

cui_multiline_event_result cui_base_editor_events(cui_context *ui, cui_id id, cui_str name,
                                                  cui_editor_state *state,
                                                  cui_editor_options options,
                                                  cui_textarea_metrics metrics,
                                                  cui_component_text_handler handler) {
    cui_multiline_event_result result = {0};
    cui_edit *edit = state ? state->edit : NULL;
    cui_edit_state before = edit ? cui_edit_get_state(edit) : (cui_edit_state){0};
    result.control = cui_base_editor(ui, id, name, state, options, metrics);
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
