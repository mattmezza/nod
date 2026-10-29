/* SPDX-License-Identifier: MIT */
#include "cui/cui_textarea.h"
#include "cui/cui_base.h"
#include "cui/cui_multiline_events.h"
#include "cui_textarea_geometry.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* The edit model guarantees stable valid UTF-8 and row starts are cluster
 * boundaries. Seed an iterator here without rescanning the remaining document. */
static size_t cui_textarea_next(cui_str text, size_t at) {
    cui_grapheme_iterator iterator = {text, at};
    return cui_grapheme_iterator_next(&iterator);
}
static size_t cui_textarea_boundary_after(cui_str text, size_t start, size_t at) {
    cui_grapheme_iterator iterator = {text, start};
    while (iterator.offset < at)
        cui_grapheme_iterator_next(&iterator);
    return iterator.offset;
}
static size_t cui_textarea_boundary_before(cui_str text, size_t start, size_t at, bool strict) {
    cui_grapheme_iterator iterator = {text, start};
    size_t previous = start;
    while (iterator.offset < at) {
        previous = iterator.offset;
        cui_grapheme_iterator_next(&iterator);
    }
    return !strict && iterator.offset == at ? at : previous;
}
/* Layout has already validated these borrowed spans. Binary search avoids a
 * full content validation for each wrapping boundary. */
static size_t cui_textarea_token_boundary(cui_edit_content content, size_t offset, bool forward) {
    size_t lo = 0, hi = content.token_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (content.tokens[mid].end <= offset)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < content.token_count && content.tokens[lo].start < offset)
        return forward ? content.tokens[lo].end : content.tokens[lo].start;
    return offset;
}
static float cui_textarea_width(cui_context *ui, cui_str text, size_t start, size_t end, float font,
                                cui_text_style style) {
    return cui_measure_text_ex(ui, (cui_str){text.data + start, end - start}, font, style).width;
}
/* Exponential bracketing avoids measuring the entire remaining document for
 * every narrow wrapped row. Prefix widths use the host's text measurement. */
static size_t cui_textarea_fit(cui_context *ui, cui_str text, size_t start, size_t end, float font,
                               float width, cui_text_style style) {
    size_t low = start, high = end - start > 64 ? start + 64 : end;
    high = cui_textarea_boundary_after(text, start, high);
    while (cui_textarea_width(ui, text, start, high, font, style) <= width) {
        low = high;
        if (high == end)
            return end;
        size_t span = high - start;
        high = span > (end - start) / 2 ? end : start + span * 2;
        high = cui_textarea_boundary_after(text, start, high);
    }
    while (low < high) {
        size_t middle = low + (high - low + 1) / 2;
        middle = cui_textarea_boundary_after(text, low, middle);
        if (cui_textarea_width(ui, text, start, middle, font, style) <= width)
            low = middle;
        else
            high = cui_textarea_boundary_before(text, low, middle, true);
    }
    return low;
}
bool cui_textarea_state_init(cui_textarea_state *state, cui_textarea_line *lines, size_t capacity) {
    if (!state || !lines || !capacity)
        return false;
    *state = (cui_textarea_state){.lines = lines, .capacity = capacity};
    return true;
}
static bool cui_textarea_folds_valid(cui_str text, const cui_textarea_fold *folds, size_t count) {
    if (count && !folds)
        return false;
    if (!count)
        return true;
    size_t line_count = 1;
    for (size_t i = 0; i < text.length; ++i) {
        if (text.data[i] == '\r') {
            ++line_count;
            if (i + 1 < text.length && text.data[i + 1] == '\n')
                ++i;
        } else if (text.data[i] == '\n') {
            ++line_count;
        }
    }
    for (size_t i = 0; i < count; ++i)
        if (folds[i].end_line >= line_count || folds[i].start_line >= folds[i].end_line ||
            folds[i].end_line - folds[i].start_line < 2 ||
            (i && folds[i].start_line <= folds[i - 1].start_line))
            return false;
    return true;
}
bool cui_textarea_set_folds(cui_textarea_state *state, cui_edit *edit, cui_textarea_fold *folds,
                            size_t count) {
    if (!state || !edit || !cui_edit_get_state(edit).multiline ||
        !cui_textarea_folds_valid(cui_edit_get_text(edit), folds, count))
        return false;
    cui_edit_break_coalescing(edit);
    state->folds = folds;
    state->fold_count = count;
    state->fold_revision = cui_edit_get_state(edit).revision;
    state->preferred_valid = false;
    return true;
}
cui_textarea_options cui_textarea_default(void) {
    return (cui_textarea_options){0};
}
cui_id cui_textarea_viewport_id(cui_id id) {
    return id ^ UINT64_C(0x850b7bf0a81c9365);
}
bool cui_textarea_measure(cui_textarea_options options, float root, cui_textarea_metrics *out) {
    if (!out || options.size < CUI_TEXTAREA_MEDIUM || options.size > CUI_TEXTAREA_CUSTOM ||
        options.text_style.weight < CUI_TEXT_WEIGHT_NORMAL ||
        options.text_style.weight > CUI_TEXT_WEIGHT_BOLD ||
        !isfinite(options.text_style.letter_spacing) || options.tab_size > 4096 ||
        !isfinite(root) || root <= 0 || root > 4096)
        return false;
    float density = root / 16;
    float font = options.size == CUI_TEXTAREA_XSMALL  ? 12
                 : options.size == CUI_TEXTAREA_LARGE ? 16
                                                      : 14;
    float px = options.size == CUI_TEXTAREA_XSMALL  ? 4
               : options.size == CUI_TEXTAREA_SMALL ? 8
               : options.size == CUI_TEXTAREA_LARGE ? 12
                                                    : 10;
    float py = options.size == CUI_TEXTAREA_XSMALL  ? 0
               : options.size == CUI_TEXTAREA_SMALL ? 2
               : options.size == CUI_TEXTAREA_LARGE ? 10
                                                    : 8;
    if (options.size == CUI_TEXTAREA_CUSTOM) {
        if (!isfinite(options.custom_size) || options.custom_size <= 0 ||
            options.custom_size > 4096)
            return false;
        font = options.custom_size * .875f / density;
        px = 8;
        py = 2;
    }
    *out = (cui_textarea_metrics){font * density, 20 * density, px * density, py * density};
    return true;
}
static size_t cui_textarea_cursor_line_ex(const cui_textarea_state *state, size_t cursor,
                                          bool upstream_affinity) {
    size_t low = 0, high = state->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        cui_textarea_line line = state->lines[middle];
        bool contains = cursor < line.end ||
                        (cursor == line.end &&
                         (line.hard_break || upstream_affinity || middle + 1 == state->count)) ||
                        cursor < line.next;
        if (contains)
            high = middle;
        else
            low = middle + 1;
    }
    return low < state->count ? low : state->count ? state->count - 1 : 0;
}
static size_t cui_textarea_cursor_line(const cui_textarea_state *state, size_t cursor) {
    return cui_textarea_cursor_line_ex(state, cursor, state->upstream_affinity);
}
/* A point just outside a selection can land in a folded gap even though both
 * selection endpoints are visible. Clamp it in the movement direction. */
static size_t cui_textarea_visible_offset(const cui_textarea_state *state, size_t offset, bool up) {
    size_t low = 0, high = state->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (state->lines[middle].next <= offset)
            low = middle + 1;
        else
            high = middle;
    }
    if (low && low < state->count && offset < state->lines[low].start)
        return up ? state->lines[low - 1].end : state->lines[low].start;
    return offset;
}

typedef struct {
    char *preedit;
    size_t length, capacity, start, end;
    const cui_edit *model; /* Identity only. */
    uint64_t revision;
    bool valid;
} cui_textarea_preedit_snapshot;
static void cui_textarea_preedit_destroy(void *payload) {
    cui_textarea_preedit_snapshot *snapshot = payload;
    free(snapshot->preedit);
}
static cui_textarea_preedit_snapshot *cui_textarea_preedit_resource(cui_context *ui,
                                                                    cui_textarea_state *state) {
    if (!cui_frame_active(ui))
        return NULL;
    return cui_resource_get(ui, (cui_id)(uintptr_t)state, UINT64_C(0x13d8f6949e840242),
                            sizeof(cui_textarea_preedit_snapshot), NULL,
                            cui_textarea_preedit_destroy);
}
static bool cui_textarea_preedit_matches(const cui_textarea_preedit_snapshot *snapshot,
                                         const cui_edit *edit, const cui_edit_composition *c) {
    return snapshot && snapshot->valid && c && c->active && snapshot->model == edit &&
           snapshot->revision == c->revision && snapshot->start == c->start &&
           snapshot->end == c->end && snapshot->length == c->length &&
           (!c->length || !memcmp(snapshot->preedit, c->buffer, c->length));
}
static void cui_textarea_preedit_save(cui_textarea_preedit_snapshot *snapshot, const cui_edit *edit,
                                      const cui_edit_composition *c) {
    if (!snapshot || !c || !c->active || c->length > 65535)
        return;
    if (c->length + 1 > snapshot->capacity) {
        char *buffer = malloc(c->length + 1);
        if (!buffer)
            return;
        free(snapshot->preedit);
        snapshot->preedit = buffer;
        snapshot->capacity = c->length + 1;
    }
    if (c->length)
        memcpy(snapshot->preedit, c->buffer, c->length);
    snapshot->length = c->length;
    snapshot->start = c->start;
    snapshot->end = c->end;
    snapshot->model = edit;
    snapshot->revision = c->revision;
    snapshot->valid = true;
}
cui_textarea_error cui_textarea_layout_ex(cui_context *ui, cui_edit *edit,
                                          cui_textarea_state *state, cui_textarea_metrics metrics,
                                          float width, bool wrap, cui_text_style style) {
    if (!ui || !edit || !state || !state->lines || !state->capacity ||
        !cui_edit_get_state(edit).multiline || !isfinite(width) || width <= 0 ||
        !isfinite(metrics.font_size) || metrics.font_size <= 0 || !isfinite(metrics.line_height) ||
        metrics.line_height <= 0 || style.weight < CUI_TEXT_WEIGHT_NORMAL ||
        style.weight > CUI_TEXT_WEIGHT_BOLD || !isfinite(style.letter_spacing) ||
        (style.letter_spacing != 0 && !(cui_text_capabilities(ui) & CUI_TEXT_CAP_LETTER_SPACING))) {
        if (state)
            state->error = CUI_TEXTAREA_INVALID;
        return CUI_TEXTAREA_INVALID;
    }
    cui_edit_content content = cui_edit_get_content(edit);
    if (content.token_count && !cui_edit_content_valid(content))
        return state->error = CUI_TEXTAREA_INVALID;
    cui_str text = cui_edit_composition_view(edit, state->composition);
    cui_edit_state selection = cui_edit_composition_selection(edit, state->composition);
    bool composing = state->composition && state->composition->active;
    if (composing)
        content = (cui_edit_content){.text = text};
    cui_textarea_preedit_snapshot *snapshot = composing && cui_has_text_geometry_line(ui)
                                                  ? cui_textarea_preedit_resource(ui, state)
                                                  : NULL;
    bool composition_reusable = composing && state->layout_composing &&
                                cui_textarea_preedit_matches(snapshot, edit, state->composition);
    bool reusable =
        ((!composing && !state->layout_composing) || composition_reusable) && state->initialized &&
        state->layout_model == edit && state->error == CUI_TEXTAREA_OK && !state->fold_count &&
        !content.token_count && !state->layout_fold_count && !state->layout_token_count &&
        state->lines == state->layout_lines && state->capacity == state->layout_capacity &&
        state->text_metrics_revision == cui_text_metrics_revision(ui) &&
        state->font_size == metrics.font_size && state->line_height == metrics.line_height &&
        state->wrap_width == width && state->layout_wrap == wrap &&
        state->text_style.family == style.family && state->text_style.weight == style.weight &&
        state->text_style.italic == style.italic &&
        state->text_style.monospace == style.monospace &&
        state->text_style.letter_spacing == style.letter_spacing;
    if (reusable && state->revision == selection.revision) {
        state->caret_line = cui_textarea_cursor_line(state, selection.cursor);
        state->cursor = selection.cursor;
        state->anchor = selection.anchor;
        state->maximum_y =
            fmaxf(0, state->count * metrics.line_height - fmaxf(0, state->viewport.h));
        state->scroll_y =
            isfinite(state->scroll_y) ? fminf(state->maximum_y, fmaxf(0, state->scroll_y)) : 0;
        state->scroll_x =
            isfinite(state->scroll_x) ? fminf(state->maximum_x, fmaxf(0, state->scroll_x)) : 0;
        return CUI_TEXTAREA_OK;
    }
    if (composing && snapshot)
        snapshot->valid = false;
    state->layout_composing = composing;
    size_t prefix_count = 0;
    if (reusable && state->count && state->revision + 1 == selection.revision) {
        cui_edit_change change = cui_edit_get_change(edit);
        if (change.valid && change.revision == selection.revision &&
            change.first_byte <= state->lines[state->count - 1].next) {
            size_t low = 0, high = state->count;
            while (low < high) {
                size_t middle = low + (high - low) / 2;
                if (state->lines[middle].next <= change.first_byte)
                    low = middle + 1;
                else
                    high = middle;
            }
            if (low == state->count)
                low--;
            while (low && !state->lines[low - 1].hard_break)
                low--;
            prefix_count = low;
        }
    }
    if (state->fold_count && state->fold_revision != selection.revision) {
        state->folds = NULL;
        state->fold_count = 0;
    }
    size_t effective_fold_count = composing ? 0 : state->fold_count;
    if (!cui_textarea_folds_valid(text, state->folds, effective_fold_count))
        return state->error = CUI_TEXTAREA_INVALID;
    if (effective_fold_count) {
        size_t cursor_line = 0, anchor_line = 0;
        for (size_t i = 0; i < text.length; ++i) {
            if (text.data[i] != '\r' && text.data[i] != '\n')
                continue;
            size_t next = i + 1;
            if (text.data[i] == '\r' && next < text.length && text.data[next] == '\n')
                ++next;
            if (next <= selection.cursor)
                ++cursor_line;
            if (next <= selection.anchor)
                ++anchor_line;
            i = next - 1;
        }
        for (size_t i = 0; i < state->fold_count; ++i) {
            cui_textarea_fold *fold = &state->folds[i];
            if ((cursor_line > fold->start_line && cursor_line < fold->end_line) ||
                (anchor_line > fold->start_line && anchor_line < fold->end_line))
                fold->collapsed = false;
        }
    }
    state->text_style = style;
    size_t start = prefix_count ? state->lines[prefix_count].start : 0;
    float maximum = 0;
    size_t widest_row = 0;
    if (prefix_count && state->widest_row < prefix_count) {
        maximum = state->widest_width;
        widest_row = state->widest_row;
    } else
        for (size_t i = 0; i < prefix_count; ++i)
            if (state->lines[i].width > maximum) {
                maximum = state->lines[i].width;
                widest_row = i;
            }
    state->count = prefix_count;
    state->font_size = metrics.font_size;
    state->line_height = metrics.line_height;
    state->wrap_width = width;
    state->layout_wrap = wrap;
    state->layout_fold_count = state->fold_count;
    state->layout_token_count = content.token_count;
    state->layout_lines = state->lines;
    state->layout_capacity = state->capacity;
    state->text_metrics_revision = cui_text_metrics_revision(ui);
    size_t paragraph_start = prefix_count ? state->lines[prefix_count].paragraph_start : 0;
    size_t logical_line = prefix_count ? state->lines[prefix_count].logical_line : 0;
    size_t fold_index = 0, hidden_until = 0;
    for (;;) {
        while (fold_index < effective_fold_count &&
               state->folds[fold_index].start_line < logical_line) {
            cui_textarea_fold fold = state->folds[fold_index++];
            if (fold.collapsed && fold.end_line > hidden_until)
                hidden_until = fold.end_line;
        }
        if (logical_line < hidden_until) {
            while (start < text.length && text.data[start] != '\r' && text.data[start] != '\n')
                ++start;
            if (start < text.length) {
                char newline = text.data[start++];
                if (newline == '\r' && start < text.length && text.data[start] == '\n')
                    ++start;
            }
            paragraph_start = start;
            ++logical_line;
            continue;
        }
        if (state->count == state->capacity) {
            state->count = 0;
            return state->error = CUI_TEXTAREA_CAPACITY;
        }
        size_t end = start;
        while (end < text.length && text.data[end] != '\n' && text.data[end] != '\r')
            end++;
        size_t paragraph_end = end;
        bool soft = false;
        for (size_t at = start; at + 2 < end; at++)
            if ((unsigned char)text.data[at] == 0xe2 && (unsigned char)text.data[at + 1] == 0x80 &&
                (unsigned char)text.data[at + 2] == 0xa8) {
                end = at;
                soft = true;
                break;
            }
        size_t next = end;
        bool hard = !soft && end < text.length;
        if (soft)
            next += 3;
        else if (hard) {
            next++;
            if (text.data[end] == '\r' && next < text.length && text.data[next] == '\n')
                next++;
        }
        size_t finish = end;
        if (wrap && start < end) {
            finish = cui_textarea_fit(ui, text, start, end, metrics.font_size, width, style);
            finish = cui_textarea_token_boundary(content, finish, false);
            if (finish == start)
                finish = cui_textarea_token_boundary(content, cui_textarea_next(text, start), true);
            if (finish < end) {
                size_t word = finish;
                while (word > start && text.data[word - 1] != ' ' && text.data[word - 1] != '\t')
                    word--;
                word = cui_textarea_boundary_before(text, start, word, false);
                word = cui_textarea_token_boundary(content, word, false);
                if (word > start)
                    finish = word;
                next = finish;
                hard = false;
            }
        }
        float measured = cui_textarea_width(ui, text, start, finish, metrics.font_size, style);
        if (measured > maximum) {
            maximum = measured;
            widest_row = state->count;
        }
        state->lines[state->count++] = (cui_textarea_line){
            start, finish, next, measured, hard, paragraph_start, paragraph_end, logical_line};
        if (finish == text.length)
            break;
        start = next;
        if (hard) {
            paragraph_start = start;
            ++logical_line;
        }
    }
    cui_edit_state current = cui_edit_composition_selection(edit, state->composition);
    state->caret_line = cui_textarea_cursor_line(state, current.cursor);
    state->widest_width = maximum;
    state->widest_row = widest_row;
    state->revision = current.revision;
    state->cursor = current.cursor;
    state->anchor = current.anchor;
    state->initialized = true;
    state->maximum_x = wrap ? 0 : fmaxf(0, maximum + 1 - width);
    state->maximum_y = fmaxf(0, state->count * metrics.line_height - fmaxf(0, state->viewport.h));
    state->scroll_x =
        isfinite(state->scroll_x) ? fminf(state->maximum_x, fmaxf(0, state->scroll_x)) : 0;
    state->scroll_y =
        isfinite(state->scroll_y) ? fminf(state->maximum_y, fmaxf(0, state->scroll_y)) : 0;
    state->layout_model = edit;
    if (composing)
        cui_textarea_preedit_save(snapshot, edit, state->composition);
    return state->error = CUI_TEXTAREA_OK;
}
cui_textarea_error cui_textarea_layout(cui_context *ui, cui_edit *edit, cui_textarea_state *state,
                                       cui_textarea_metrics metrics, float width, bool wrap) {
    return cui_textarea_layout_ex(ui, edit, state, metrics, width, wrap, (cui_text_style){0});
}

enum { CUI_TEXTAREA_VISUAL_LINES = 64, CUI_TEXTAREA_VISUAL_BYTES = 1048576 };
typedef struct {
    char *data;
    size_t length;
} cui_textarea_visual_source;
typedef struct {
    cui_text_geometry geometry;
    size_t source, start, end;
    float size;
    cui_text_style style;
} cui_textarea_visual_line;
typedef struct {
    cui_textarea_visual_source sources[CUI_TEXTAREA_VISUAL_LINES];
    cui_textarea_visual_line lines[CUI_TEXTAREA_VISUAL_LINES];
    size_t source_count, line_count, bytes;
} cui_textarea_visual_cache;
static void cui_textarea_visual_clear(void *payload) {
    cui_textarea_visual_cache *cache = payload;
    for (size_t i = 0; i < cache->line_count; i++)
        cui_release_text_geometry(&cache->lines[i].geometry);
    for (size_t i = 0; i < cache->source_count; i++)
        free(cache->sources[i].data);
    memset(cache, 0, sizeof *cache);
}
static size_t cui_textarea_paragraph_base(cui_textarea_line line) {
    return line.paragraph_end < line.end || line.paragraph_start > line.start
               ? line.start
               : line.paragraph_start;
}
cui_text_line_source cui_textarea_line_source(cui_str text, cui_textarea_line line) {
    size_t start = line.paragraph_start, end = line.paragraph_end;
    /* Compatibility for caller-constructed legacy rows. */
    if (end < line.end || start > line.start) {
        start = line.start;
        end = line.end;
    }
    if (end > text.length || line.start > line.end || line.end > end || (!text.data && text.length))
        return (cui_text_line_source){0};
    return (cui_text_line_source){
        {text.data ? text.data + start : "", end - start}, line.start - start, line.end - start};
}
cui_text_layout_status cui_textarea_visual_geometry(cui_context *ui, cui_str text,
                                                    cui_textarea_line line, float size,
                                                    cui_text_style style, cui_text_geometry *owned,
                                                    const cui_text_geometry **out) {
    if (!owned || !out)
        return CUI_TEXT_LAYOUT_INVALID;
    cui_text_line_source source = cui_textarea_line_source(text, line);
    if (!source.paragraph.data)
        return CUI_TEXT_LAYOUT_INVALID;
    if (!cui_has_text_geometry_line(ui))
        return CUI_TEXT_LAYOUT_UNAVAILABLE;
    if (!cui_frame_active(ui)) {
        cui_text_layout_status status = cui_get_text_geometry_line(ui, source, size, style, owned);
        if (status == CUI_TEXT_LAYOUT_OK)
            *out = owned;
        return status;
    }
    cui_textarea_visual_cache *cache =
        cui_resource_get(ui, UINT64_C(0xbc812b9fd2e07a15), UINT64_C(0x6b9fcfd365caa233),
                         sizeof *cache, cui_textarea_visual_clear, cui_textarea_visual_clear);
    if (!cache)
        return CUI_TEXT_LAYOUT_CAPACITY;
    size_t index = 0;
    for (; index < cache->source_count; index++)
        if (cache->sources[index].length == source.paragraph.length &&
            (!source.paragraph.length ||
             !memcmp(cache->sources[index].data, source.paragraph.data, source.paragraph.length)))
            break;
    for (size_t i = 0; i < cache->line_count; i++) {
        cui_textarea_visual_line *cached = &cache->lines[i];
        if (cached->source == index && cached->start == source.byte_start &&
            cached->end == source.byte_end && cached->size == size &&
            cached->style.weight == style.weight && cached->style.italic == style.italic &&
            cached->style.monospace == style.monospace && cached->style.family == style.family &&
            cached->style.letter_spacing == style.letter_spacing) {
            *out = &cached->geometry;
            return CUI_TEXT_LAYOUT_OK;
        }
    }
    if (cache->line_count == CUI_TEXTAREA_VISUAL_LINES ||
        (index == cache->source_count &&
         source.paragraph.length >= CUI_TEXTAREA_VISUAL_BYTES - cache->bytes))
        return CUI_TEXT_LAYOUT_CAPACITY;
    char *copy = NULL;
    if (index == cache->source_count) {
        copy = malloc(source.paragraph.length + 1);
        if (!copy)
            return CUI_TEXT_LAYOUT_MEMORY;
        if (source.paragraph.length)
            memcpy(copy, source.paragraph.data, source.paragraph.length);
        copy[source.paragraph.length] = 0;
        source.paragraph.data = copy;
    } else
        source.paragraph.data = cache->sources[index].data;
    cui_text_geometry geometry = {0};
    cui_text_layout_status status = cui_get_text_geometry_line(ui, source, size, style, &geometry);
    if (status != CUI_TEXT_LAYOUT_OK) {
        free(copy);
        return status;
    }
    if (copy) {
        cache->sources[cache->source_count++] =
            (cui_textarea_visual_source){copy, source.paragraph.length};
        cache->bytes += source.paragraph.length + 1;
    }
    cui_textarea_visual_line *cached = &cache->lines[cache->line_count++];
    *cached = (cui_textarea_visual_line){geometry,        index, source.byte_start,
                                         source.byte_end, size,  style};
    *out = &cached->geometry;
    return CUI_TEXT_LAYOUT_OK;
}
float cui_textarea_visual_x(cui_context *ui, cui_str text, cui_textarea_line line, size_t offset,
                            float size, cui_text_style style, cui_text_affinity affinity) {
    if (offset < line.start)
        offset = line.start;
    if (offset > line.end)
        offset = line.end;
    cui_text_geometry owned = {0};
    const cui_text_geometry *geometry = NULL;
    cui_text_caret carets[2];
    size_t count = 0;
    if (cui_textarea_visual_geometry(ui, text, line, size, style, &owned, &geometry) ==
            CUI_TEXT_LAYOUT_OK &&
        geometry->ops->carets(geometry->session, offset - cui_textarea_paragraph_base(line), carets,
                              2, &count) == CUI_TEXT_LAYOUT_OK &&
        count) {
        float x = carets[0].x;
        for (size_t i = 0; i < count; i++)
            if (carets[i].affinity == affinity)
                x = carets[i].x;
        cui_release_text_geometry(&owned);
        return x;
    }
    cui_release_text_geometry(&owned);
    return cui_textarea_width(ui, text, line.start, offset, size, style);
}
static size_t cui_textarea_offset(cui_context *ui, cui_str text, const cui_textarea_state *state,
                                  size_t row, float x) {
    cui_textarea_line line = state->lines[row];
    cui_text_geometry owned = {0};
    const cui_text_geometry *geometry = NULL;
    cui_text_caret caret;
    if (cui_textarea_visual_geometry(ui, text, line, state->font_size, state->text_style, &owned,
                                     &geometry) == CUI_TEXT_LAYOUT_OK &&
        geometry->ops->hit_test(geometry->session, x, 0, CUI_TEXT_DOWNSTREAM, &caret) ==
            CUI_TEXT_LAYOUT_OK) {
        size_t result = caret.byte_offset + cui_textarea_paragraph_base(line);
        cui_release_text_geometry(&owned);
        return result;
    }
    cui_release_text_geometry(&owned);
    if (x <= 0)
        return line.start;
    if (x >= line.width)
        return line.end;
    size_t left =
        cui_textarea_fit(ui, text, line.start, line.end, state->font_size, x, state->text_style);
    size_t right = cui_textarea_next(text, left);
    if (right > line.end)
        right = line.end;
    float a = cui_textarea_width(ui, text, line.start, left, state->font_size, state->text_style);
    float b = cui_textarea_width(ui, text, line.start, right, state->font_size, state->text_style);
    return x - a < b - x ? left : right;
}
static bool cui_textarea_geometry_valid(const cui_textarea_state *state) {
    return state && state->initialized && state->lines && state->count &&
           state->count <= state->capacity && isfinite(state->font_size) && state->font_size > 0;
}
size_t cui_textarea_row_for_offset(const cui_textarea_state *state, size_t offset, bool upstream) {
    return cui_textarea_geometry_valid(state) && offset <= state->lines[state->count - 1].next
               ? cui_textarea_cursor_line_ex(state, offset, upstream)
               : SIZE_MAX;
}
size_t cui_textarea_project_offset(const cui_textarea_state *state, size_t offset, bool upwards) {
    return cui_textarea_geometry_valid(state) && offset <= state->lines[state->count - 1].next
               ? cui_textarea_visible_offset(state, offset, upwards)
               : SIZE_MAX;
}
size_t cui_textarea_row_offset(cui_context *ui, cui_edit *edit, const cui_textarea_state *state,
                               size_t row, float x) {
    if (!ui || !edit || !cui_textarea_geometry_valid(state) || row >= state->count ||
        !isfinite(x) || state->revision != cui_edit_get_state(edit).revision)
        return SIZE_MAX;
    return cui_textarea_offset(ui, cui_edit_get_text(edit), state, row, x);
}
float cui_textarea_row_x(cui_context *ui, cui_edit *edit, const cui_textarea_state *state,
                         size_t row, size_t offset) {
    if (!ui || !edit || !cui_textarea_geometry_valid(state) || row >= state->count ||
        state->revision != cui_edit_get_state(edit).revision ||
        offset > cui_edit_get_text(edit).length)
        return NAN;
    cui_textarea_line line = state->lines[row];
    if (offset < line.start)
        offset = line.start;
    if (offset > line.end)
        offset = line.end;
    return cui_textarea_visual_x(
        ui, cui_edit_get_text(edit), line, offset, state->font_size, state->text_style,
        state->visual_affinity_valid && state->visual_model == edit &&
                state->visual_caret_byte == offset && state->visual_revision == state->revision
            ? state->visual_affinity
            : CUI_TEXT_DOWNSTREAM);
}
size_t cui_textarea_hit_test(cui_context *ui, cui_edit *edit, cui_textarea_state *state, float x,
                             float y) {
    if (!ui || !edit || !state || !state->count || !isfinite(x) || !isfinite(y))
        return SIZE_MAX;
    float relative = fmaxf(0, (y - state->viewport.y + state->scroll_y) / state->line_height);
    size_t row = relative >= state->count ? state->count - 1 : (size_t)relative;
    size_t cursor = cui_textarea_offset(ui, cui_edit_get_text(edit), state, row,
                                        x - state->viewport.x + state->scroll_x);
    if (cui_edit_get_content(edit).token_count &&
        cui_edit_token_boundary(cui_edit_get_content(edit), cursor, false, &cursor) != CUI_EDIT_OK)
        return SIZE_MAX;
    state->visual_affinity_valid = false;
    cui_text_geometry owned = {0};
    const cui_text_geometry *geometry = NULL;
    cui_text_caret caret;
    cui_str text = cui_edit_get_text(edit);
    if (cui_textarea_visual_geometry(ui, text, state->lines[row], state->font_size,
                                     state->text_style, &owned, &geometry) == CUI_TEXT_LAYOUT_OK &&
        geometry->ops->hit_test(geometry->session, x - state->viewport.x + state->scroll_x, 0,
                                CUI_TEXT_DOWNSTREAM, &caret) == CUI_TEXT_LAYOUT_OK) {
        state->visual_affinity = caret.affinity;
        state->visual_caret_byte = cursor;
        state->visual_model = edit;
        state->visual_revision = cui_edit_get_state(edit).revision;
        state->visual_affinity_valid = true;
    }
    cui_release_text_geometry(&owned);
    state->upstream_affinity = cursor == state->lines[row].end && !state->lines[row].hard_break;
    return cursor;
}
static void cui_textarea_reveal(cui_context *ui, cui_edit *edit, cui_textarea_state *state) {
    cui_str text = cui_edit_composition_view(edit, state->composition);
    cui_textarea_line line = state->lines[state->caret_line];
    size_t cursor = state->cursor < line.end ? state->cursor : line.end;
    float x = cui_textarea_visual_x(
        ui, text, line, cursor, state->font_size, state->text_style,
        state->visual_affinity_valid && state->visual_caret_byte == cursor ? state->visual_affinity
                                                                           : CUI_TEXT_DOWNSTREAM);
    float y = state->caret_line * state->line_height;
    if (x < state->scroll_x)
        state->scroll_x = x;
    if (x + 1 > state->scroll_x + state->viewport.w)
        state->scroll_x = x + 1 - state->viewport.w;
    if (y < state->scroll_y)
        state->scroll_y = y;
    if (y + state->line_height > state->scroll_y + state->viewport.h)
        state->scroll_y = y + state->line_height - state->viewport.h;
    state->scroll_x = fminf(state->maximum_x, fmaxf(0, state->scroll_x));
    state->scroll_y = fminf(state->maximum_y, fmaxf(0, state->scroll_y));
}
static cui_edit_result cui_textarea_clipboard(cui_edit *edit, cui_event event,
                                              cui_textarea_options options) {
    cui_edit_state selection = cui_edit_get_state(edit);
    size_t first = selection.anchor < selection.cursor ? selection.anchor : selection.cursor;
    size_t last = selection.anchor > selection.cursor ? selection.anchor : selection.cursor;
    bool paste = event.key == CUI_KEY_V;
    if ((paste && options.readonly) || (!paste && first == last))
        return CUI_EDIT_OK;
    const cui_platform *host = options.clipboard;
    if (!host || !(host->capabilities & CUI_PLATFORM_CLIPBOARD) ||
        (paste ? !host->clipboard_get : !host->clipboard_set))
        return CUI_EDIT_CLIPBOARD_UNAVAILABLE;
    if (paste)
        return cui_edit_insert(edit, host->clipboard_get(host->user));
    cui_str text = cui_edit_get_text(edit);
    if (!host->clipboard_set(host->user, (cui_str){text.data + first, last - first}))
        return CUI_EDIT_CLIPBOARD_UNAVAILABLE;
    return event.key == CUI_KEY_X && !options.readonly ? cui_edit_delete(edit) : CUI_EDIT_OK;
}
cui_textarea_result cui_base_textarea(cui_context *ui, cui_id id, cui_str name, cui_edit *edit,
                                      cui_textarea_state *state, cui_textarea_options options,
                                      cui_textarea_metrics metrics) {
    cui_textarea_result result = {0};
    if (!ui || !id || !edit || !state || (name.length && !name.data)) {
        result.error = CUI_TEXTAREA_INVALID;
        return result;
    }
    if (state->visual_model != edit ||
        state->visual_revision != cui_edit_get_state(edit).revision ||
        state->visual_caret_byte != cui_edit_get_state(edit).cursor)
        state->visual_affinity_valid = false;
    if (cui_edit_get_content(edit).token_count &&
        !cui_edit_content_valid(cui_edit_get_content(edit))) {
        result.error = CUI_TEXTAREA_INVALID;
        result.edit_error = CUI_EDIT_INVALID_ARGUMENT;
        return result;
    }
    if (cui_edit_get_content(edit).token_count)
        options.no_indent = true;
    cui_edit_state before = cui_edit_get_state(edit);
    bool external =
        state->initialized && (state->revision != before.revision ||
                               state->cursor != before.cursor || state->anchor != before.anchor);
    external =
        external ||
        (state->initialized &&
         (state->text_style.family != options.text_style.family ||
          state->text_style.weight != options.text_style.weight ||
          state->text_style.italic != options.text_style.italic ||
          state->text_style.monospace != options.text_style.monospace ||
          state->text_style.letter_spacing != options.text_style.letter_spacing ||
          state->font_size != metrics.font_size || state->line_height != metrics.line_height));
    if (external) {
        state->preferred_valid = false;
        state->upstream_affinity = false;
        state->dragging = false;
    }
    state->composition =
        cui_base_composition(ui, id, state->composition, &state->composition_owned);
    if (state->composition && state->composition->active)
        (void)cui_edit_composition_event(edit, state->composition,
                                         (cui_event){.type = CUI_EVENT_GESTURE}, true);
    if (state->composition && (options.disabled || options.readonly || cui_focused(ui) != id))
        (void)cui_edit_composition_event(edit, state->composition,
                                         (cui_event){.type = CUI_EVENT_COMPOSITION_CANCEL}, false);
    float width = state->viewport.w > 0 ? state->viewport.w : state->wrap_width;
    result.error = cui_textarea_layout_ex(ui, edit, state, metrics, width, !options.no_wrap,
                                          options.text_style);
    if (result.error != CUI_TEXTAREA_OK)
        return result;
    cui_interaction interaction =
        cui_interact(ui, id, CUI_ROLE_TEXTBOX, name, options.disabled, false, (float)before.length);
    cui_set_tab_stop(ui, id, !options.no_tab_stop);
    cui_set_semantic_details(ui, id,
                             (cui_semantic_details){.handles_tab = !options.disabled &&
                                                                   !options.readonly &&
                                                                   !options.no_indent});
    if (options.custom_role && !cui_set_semantic_role(ui, id, options.role)) {
        result.error = CUI_TEXTAREA_INVALID;
        return result;
    }
    if (options.disabled)
        state->dragging = false;
    float old_x = state->scroll_x, old_y = state->scroll_y;
    bool old_affinity = state->upstream_affinity;
    size_t old_row = state->caret_line;
    bool reveal = external && cui_focused(ui) == id && !options.disabled;
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count; i++) {
        cui_event event = events[i];
        if (event.type == CUI_EVENT_CANCEL)
            state->dragging = false;
        if (options.disabled || cui_event_consumed(ui, i) || cui_event_target(ui, i) != id ||
            (event.type == CUI_EVENT_KEY_DOWN &&
             (event.ime_composing || event.prefer_character_input)))
            continue;
        if (!state->composition && event.type >= CUI_EVENT_COMPOSITION_START &&
            event.type <= CUI_EVENT_COMPOSITION_COMMIT) {
            result.edit_error = CUI_EDIT_CAPACITY;
            cui_consume_event(ui, i);
            continue;
        }
        if (state->composition && ((event.type == CUI_EVENT_CANCEL && state->composition->active) ||
                                   (event.type >= CUI_EVENT_COMPOSITION_START &&
                                    event.type <= CUI_EVENT_COMPOSITION_CANCEL))) {
            uint64_t revision = cui_edit_get_state(edit).revision;
            cui_edit_result r =
                cui_edit_composition_event(edit, state->composition, event, !options.readonly);
            if (result.edit_error == CUI_EDIT_OK)
                result.edit_error = r;
            cui_consume_event(ui, i);
            result.changed |= revision != cui_edit_get_state(edit).revision;
            result.error = cui_textarea_layout_ex(ui, edit, state, metrics, width, !options.no_wrap,
                                                  options.text_style);
            reveal = true;
            continue;
        }
        if (state->composition && state->composition->active) {
            if (event.type == CUI_EVENT_KEY_DOWN || event.type == CUI_EVENT_TEXT) {
                cui_consume_event(ui, i);
                continue;
            }
            if (event.type == CUI_EVENT_POINTER_DOWN) {
                (void)cui_edit_composition_event(edit, state->composition,
                                                 (cui_event){.type = CUI_EVENT_COMPOSITION_CANCEL},
                                                 true);
                result.error = cui_textarea_layout_ex(ui, edit, state, metrics, width,
                                                      !options.no_wrap, options.text_style);
                if (result.error != CUI_TEXTAREA_OK)
                    break;
            }
        }
        cui_edit_state event_before = cui_edit_get_state(edit);
        cui_edit_result error = CUI_EDIT_OK;
        bool handled = false, move = false;
        bool command = (event.modifiers & (CUI_MOD_CTRL | CUI_MOD_META)) != 0;
        bool shift = (event.modifiers & CUI_MOD_SHIFT) != 0;
        bool primary = event.pointer_button == CUI_POINTER_PRIMARY;
        if (event.type == CUI_EVENT_SCROLL && isfinite(event.dx) && isfinite(event.dy)) {
            state->scroll_x =
                fminf(state->maximum_x,
                      fmaxf(0, state->scroll_x -
                                   event.dx * (event.scroll_precise ? 1 : metrics.line_height)));
            state->scroll_y =
                fminf(state->maximum_y,
                      fmaxf(0, state->scroll_y -
                                   event.dy * (event.scroll_precise ? 1 : metrics.line_height)));
            /* Explicit viewport movement wins over an earlier caret-reveal request. */
            reveal = false;
            handled = state->scroll_x != old_x || state->scroll_y != old_y;
        } else if (primary && (event.type == CUI_EVENT_POINTER_DOWN ||
                               ((state->dragging || interaction.pressed) &&
                                (event.type == CUI_EVENT_POINTER_MOVE ||
                                 event.type == CUI_EVENT_POINTER_UP)))) {
            size_t cursor = cui_textarea_hit_test(ui, edit, state, event.x, event.y);
            cui_edit_state selection = cui_edit_get_state(edit);
            size_t anchor =
                event.type == CUI_EVENT_POINTER_DOWN && !shift ? cursor : selection.anchor;
            error = cui_edit_select_atomic(edit, anchor, cursor);
            state->dragging = event.type != CUI_EVENT_POINTER_UP;
            move = handled = true;
        } else if (event.type == CUI_EVENT_KEY_DOWN && command &&
                   (event.key == CUI_KEY_C || event.key == CUI_KEY_X || event.key == CUI_KEY_V)) {
            error = cui_textarea_clipboard(edit, event, options);
            handled = true;
        } else if (event.type == CUI_EVENT_KEY_DOWN && command &&
                   (event.key == CUI_KEY_UP || event.key == CUI_KEY_DOWN)) {
            error =
                cui_edit_move(edit, event.key == CUI_KEY_UP ? CUI_EDIT_START : CUI_EDIT_END, shift);
            state->upstream_affinity = false;
            state->preferred_valid = false;
            move = handled = true;
        } else if (event.type == CUI_EVENT_KEY_DOWN &&
                   (event.key == CUI_KEY_UP || event.key == CUI_KEY_DOWN ||
                    event.key == CUI_KEY_PAGEUP || event.key == CUI_KEY_PAGEDOWN)) {
            cui_edit_state selection = cui_edit_get_state(edit);
            cui_str text = cui_edit_composition_view(edit, state->composition);
            bool up = event.key == CUI_KEY_UP || event.key == CUI_KEY_PAGEUP;
            bool collapse = !shift && selection.anchor != selection.cursor &&
                            (event.key == CUI_KEY_UP || event.key == CUI_KEY_DOWN);
            size_t effective = selection.cursor, row = state->caret_line;
            if (collapse) {
                size_t first =
                    selection.anchor < selection.cursor ? selection.anchor : selection.cursor;
                size_t last =
                    selection.anchor > selection.cursor ? selection.anchor : selection.cursor;
                /* Frozen move_vertical: previous_boundary(start-1) subtracts
                 * another byte before left clipping; next_boundary(end-1)
                 * right-clips end. Clip to CUI's whole-grapheme boundaries. */
                effective =
                    up ? cui_textarea_boundary_before(text, 0, first > 1 ? first - 2 : 0, false)
                       : cui_textarea_boundary_after(text, 0, last);
                effective = cui_textarea_token_boundary(cui_edit_get_content(edit), effective, !up);
                effective = cui_textarea_visible_offset(state, effective, up);
                bool affinity = !up && effective == selection.cursor && state->upstream_affinity;
                row = cui_textarea_cursor_line_ex(state, effective, affinity);
                state->preferred_valid = false;
            }
            cui_textarea_line line = state->lines[row];
            if (!state->preferred_valid) {
                state->preferred_x = cui_textarea_visual_x(
                    ui, text, line, effective, metrics.font_size, options.text_style,
                    state->visual_affinity_valid && state->visual_caret_byte == effective
                        ? state->visual_affinity
                        : CUI_TEXT_DOWNSTREAM);
                state->preferred_valid = true;
            }
            size_t step = event.key == CUI_KEY_PAGEUP || event.key == CUI_KEY_PAGEDOWN
                              ? (size_t)fmaxf(1, floorf(state->viewport.h / metrics.line_height))
                              : 1;
            size_t previous_row = row;
            row = up ? (step > row ? 0 : row - step)
                     : (step >= state->count - row ? state->count - 1 : row + step);
            bool selection_edge = shift && row == previous_row &&
                                  (event.key == CUI_KEY_UP || event.key == CUI_KEY_DOWN);
            size_t cursor = selection_edge ? (up ? 0 : cui_edit_get_text(edit).length)
                                           : cui_textarea_offset(ui, cui_edit_get_text(edit), state,
                                                                 row, state->preferred_x);
            state->upstream_affinity =
                !selection_edge && cursor == state->lines[row].end && !state->lines[row].hard_break;
            error = cui_edit_select_atomic(edit, shift ? selection.anchor : cursor, cursor);
            move = handled = true;
        } else if (event.type == CUI_EVENT_KEY_DOWN && !command &&
                   ((event.key == CUI_KEY_RIGHT && state->upstream_affinity &&
                     state->caret_line + 1 < state->count &&
                     state->cursor == state->lines[state->caret_line].end &&
                     !state->lines[state->caret_line].hard_break) ||
                    (event.key == CUI_KEY_LEFT && !state->upstream_affinity && state->caret_line &&
                     state->cursor == state->lines[state->caret_line].start &&
                     !state->lines[state->caret_line - 1].hard_break)) &&
                   (shift || state->cursor == state->anchor)) {
            state->upstream_affinity = event.key == CUI_KEY_LEFT;
            state->preferred_valid = false;
            move = handled = true;
        } else if (event.type == CUI_EVENT_KEY_DOWN && !command &&
                   (event.key == CUI_KEY_HOME || event.key == CUI_KEY_END)) {
            cui_edit_state selection = cui_edit_get_state(edit);
            cui_textarea_line line = state->lines[state->caret_line];
            size_t cursor = event.key == CUI_KEY_HOME ? line.start : line.end;
            state->upstream_affinity = event.key == CUI_KEY_END && !line.hard_break;
            error = cui_edit_select_atomic(edit, shift ? selection.anchor : cursor, cursor);
            move = handled = true;
            state->preferred_valid = false;
        } else if (event.type == CUI_EVENT_KEY_DOWN && event.key == CUI_KEY_ENTER) {
            if (!options.readonly) {
                result.pressed_enter = true;
                result.enter_secondary = command;
                result.enter_shift = shift;
                if (!options.submit_on_enter || shift) {
                    error = cui_edit_apply_event(edit, event);
                    move = true;
                }
            }
            state->upstream_affinity = false;
            state->preferred_valid = false;
            handled = true;
        } else if (event.type == CUI_EVENT_KEY_DOWN && event.key == CUI_KEY_TAB && !command &&
                   !options.readonly && !options.no_indent) {
            error = cui_edit_indent(edit, shift, options.tab_size ? options.tab_size : 2,
                                    options.hard_tabs);
            state->upstream_affinity = false;
            state->preferred_valid = false;
            move = handled = true;
        } else if (event.type == CUI_EVENT_TEXT ||
                   (event.type == CUI_EVENT_KEY_DOWN &&
                    (event.key == CUI_KEY_LEFT || event.key == CUI_KEY_RIGHT ||
                     event.key == CUI_KEY_HOME || event.key == CUI_KEY_END ||
                     event.key == CUI_KEY_ENTER || event.key == CUI_KEY_BACKSPACE ||
                     event.key == CUI_KEY_DELETE ||
                     (command && (event.key == CUI_KEY_A || event.key == CUI_KEY_Z ||
                                  event.key == CUI_KEY_Y))))) {
            bool navigation = event.type == CUI_EVENT_KEY_DOWN &&
                              (event.key == CUI_KEY_LEFT || event.key == CUI_KEY_RIGHT ||
                               event.key == CUI_KEY_HOME || event.key == CUI_KEY_END ||
                               (command && event.key == CUI_KEY_A));
            if (!options.readonly || navigation) {
                if (event.modifiers & CUI_MOD_META)
                    event.modifiers |= CUI_MOD_CTRL;
                error = cui_edit_apply_event(edit, event);
                move = true;
            }
            state->upstream_affinity = false;
            state->preferred_valid = false;
            handled = true;
        }
        cui_edit_state event_after = cui_edit_get_state(edit);
        if (event_after.revision != event_before.revision) {
            state->preferred_valid = false;
            state->upstream_affinity = false;
            move = true;
        }
        if (handled)
            cui_consume_event(ui, i);
        if (result.edit_error == CUI_EDIT_OK && error != CUI_EDIT_OK)
            result.edit_error = error;
        result.error = cui_textarea_layout_ex(ui, edit, state, metrics, width, !options.no_wrap,
                                              options.text_style);
        if (result.error != CUI_TEXTAREA_OK)
            break;
        reveal |= move;
    }
    if (result.error == CUI_TEXTAREA_OK && reveal)
        cui_textarea_reveal(ui, edit, state);
    cui_edit_state after = cui_edit_get_state(edit);
    result.changed = after.revision != before.revision;
    result.selection_changed = after.cursor != before.cursor || after.anchor != before.anchor ||
                               old_affinity != state->upstream_affinity ||
                               old_row != state->caret_line;
    result.scrolled = old_x != state->scroll_x || old_y != state->scroll_y;
    cui_update_semantic(ui, id, false, (float)after.length);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.multiline = true,
                               .readonly = options.readonly,
                               .handles_tab =
                                   !options.disabled && !options.readonly && !options.no_indent,
                               .required = options.required,
                               .invalid = options.invalid || result.edit_error != CUI_EDIT_OK ||
                                          result.error != CUI_TEXTAREA_OK,
                               .cursor = CUI_CURSOR_TEXT,
                               .scrollable = true,
                               .scroll_x = state->scroll_x,
                               .scroll_y = state->scroll_y,
                               .scroll_max_x = state->maximum_x,
                               .scroll_max_y = state->maximum_y});
    if (result.changed || result.selection_changed || result.scrolled)
        cui_invalidate(ui);
    return result;
}

cui_multiline_event_result cui_base_textarea_events(cui_context *ui, cui_id id, cui_str name,
                                                    cui_edit *edit, cui_textarea_state *state,
                                                    cui_textarea_options options,
                                                    cui_textarea_metrics metrics,
                                                    cui_component_text_handler handler) {
    cui_multiline_event_result result = {0};
    cui_edit_state before = edit ? cui_edit_get_state(edit) : (cui_edit_state){0};
    result.control = cui_base_textarea(ui, id, name, edit, state, options, metrics);
    cui_edit_state after = edit ? cui_edit_get_state(edit) : (cui_edit_state){0};
    if (ui && edit && handler.callback && before.revision != after.revision) {
        cui_component_text_event event = {.event = {.kind = CUI_COMPONENT_EVENT_CHANGE,
                                                    .target = id,
                                                    .source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN,
                                                    .flags = CUI_COMPONENT_EVENT_HAS_FRAME,
                                                    .frame_sequence = cui_frame_sequence(ui)},
                                          .before = before,
                                          .after = after,
                                          .has_text = true,
                                          .text = cui_edit_get_text(edit)};
        handler.callback(handler.user, &event);
        result.change_count = 1;
    }
    return result;
}

bool cui_textarea_visual_paint_range(cui_context *ui, cui_id id, cui_str text,
                                     cui_textarea_line line, size_t first, size_t last, float size,
                                     cui_text_style style, cui_color color, float y, float height) {
    if (first < line.start)
        first = line.start;
    if (last > line.end)
        last = line.end;
    if (first >= last)
        return false;
    cui_text_geometry owned = {0};
    const cui_text_geometry *geometry = NULL;
    cui_text_selection_rect rects[64];
    size_t count = 0;
    size_t base = cui_textarea_paragraph_base(line);
    if (cui_textarea_visual_geometry(ui, text, line, size, style, &owned, &geometry) !=
            CUI_TEXT_LAYOUT_OK ||
        geometry->ops->selection(geometry->session, first - base, last - base, rects, 64, &count) !=
            CUI_TEXT_LAYOUT_OK) {
        cui_release_text_geometry(&owned);
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        cui_begin_absolute_box(ui, id ^ ((i + 1) * UINT64_C(0xdcd85d8e00cda997)),
                               (cui_box){.background = color},
                               (cui_rect){rects[i].bounds.x, y, rects[i].bounds.w, height});
        cui_end_box(ui);
    }
    cui_release_text_geometry(&owned);
    return true;
}
