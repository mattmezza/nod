/* SPDX-License-Identifier: MIT */
#include "cui/cui_numeric.h"
#include <errno.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
bool cui_number_parse(cui_str text, double *value) {
    if (!value || !text.data || !text.length || text.length > 127)
        return false;
    size_t i = 0, digits = 0;
    if (text.data[i] == '+' || text.data[i] == '-')
        ++i;
    while (i < text.length && text.data[i] >= '0' && text.data[i] <= '9') {
        ++i;
        ++digits;
    }
    if (i < text.length && text.data[i] == '.') {
        ++i;
        while (i < text.length && text.data[i] >= '0' && text.data[i] <= '9') {
            ++i;
            ++digits;
        }
    }
    if (!digits)
        return false;
    if (i < text.length && (text.data[i] == 'e' || text.data[i] == 'E')) {
        ++i;
        if (i < text.length && (text.data[i] == '+' || text.data[i] == '-'))
            ++i;
        size_t start = i;
        while (i < text.length && text.data[i] >= '0' && text.data[i] <= '9')
            ++i;
        if (i == start)
            return false;
    }
    if (i != text.length)
        return false;
    const char *radix = localeconv()->decimal_point;
    size_t radix_length = strlen(radix);
    if (!radix_length || radix_length > 16)
        return false;
    char adapted[144];
    size_t n = 0;
    for (i = 0; i < text.length; ++i) {
        if (text.data[i] == '.') {
            memcpy(adapted + n, radix, radix_length);
            n += radix_length;
        } else
            adapted[n++] = text.data[i];
    }
    adapted[n] = 0;
    char *end = NULL;
    errno = 0;
    double parsed = strtod(adapted, &end);
    if (!end || *end || !isfinite(parsed) || (errno == ERANGE && parsed == 0))
        return false;
    *value = parsed;
    return true;
}
size_t cui_number_format(double value, char *out, size_t capacity) {
    if (!isfinite(value)) {
        if (out && capacity)
            out[0] = 0;
        return 0;
    }
    char buffer[96];
    int written = snprintf(buffer, sizeof(buffer), "%.17g", value);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        if (out && capacity)
            out[0] = 0;
        return 0;
    }
    const char *radix = localeconv()->decimal_point;
    size_t radix_length = strlen(radix), length = 0;
    for (size_t i = 0; i < (size_t)written;) {
        char c = buffer[i];
        if (radix_length && !strncmp(buffer + i, radix, radix_length)) {
            c = '.';
            i += radix_length;
        } else
            ++i;
        if (out && length + 1 < capacity)
            out[length] = c;
        ++length;
    }
    if (out && capacity)
        out[length < capacity ? length : capacity - 1] = 0;
    return length;
}
bool cui_number_step(double *value, int direction, cui_number_options o) {
    if (!value || !isfinite(*value) || !direction || o.disabled || o.readonly)
        return false;
    if ((o.has_minimum && !isfinite(o.minimum)) || (o.has_maximum && !isfinite(o.maximum)) ||
        (o.has_minimum && o.has_maximum && o.minimum > o.maximum))
        return false;
    double step = o.step_by ? o.step_by(o.step_user, *value, direction < 0 ? -1 : 1) : o.step;
    if (!isfinite(step) || step <= 0)
        return false;
    double before = *value;
    if (direction < 0 && o.has_minimum && before <= o.minimum)
        return false;
    if (direction > 0 && o.has_maximum && before >= o.maximum)
        return false;
    double after = direction < 0 ? before - step : before + step;
    if (o.has_minimum && after < o.minimum)
        after = o.minimum;
    if (o.has_maximum && after > o.maximum)
        after = o.maximum;
    if (!isfinite(after) || after == before)
        return false;
    *value = after;
    return true;
}
size_t cui_pagination_items(size_t current, size_t total, unsigned radius, cui_page_item out[11]) {
    if (!out || !total)
        return 0;
    if (current < 1)
        current = 1;
    if (current > total)
        current = total;
    if (radius > 3)
        radius = 3;
    size_t candidates[9], count = 0;
    candidates[count++] = 1;
    size_t begin = current > radius ? current - radius : 1;
    size_t end = total - current < radius ? total : current + radius;
    for (size_t page = begin;; ++page) {
        if (page != 1 && page != total)
            candidates[count++] = page;
        if (page == end)
            break;
    }
    if (total != 1)
        candidates[count++] = total;
    size_t n = 0, previous = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t page = candidates[i];
        if (previous && page - previous == 2)
            out[n++] = (cui_page_item){.page = previous + 1};
        else if (previous && page - previous > 2)
            out[n++] = (cui_page_item){.ellipsis = true};
        out[n++] = (cui_page_item){.page = page};
        previous = page;
    }
    return n;
}

static float cui_numeric_semantic_value(double value) {
    return (float)(value > FLT_MAX ? FLT_MAX : value < -FLT_MAX ? -FLT_MAX : value);
}
cui_id cui_numeric_part_id(cui_id id, unsigned index) {
    return id ^ (UINT64_C(0xa620ffb851de91c3) + UINT64_C(0x9e3779b97f4a7c15) * index);
}
static cui_edit_result cui_numeric_set_draft(cui_edit *edit, double value) {
    char text[96];
    size_t n = cui_number_format(value, text, sizeof(text));
    if (!n)
        return CUI_EDIT_INVALID_ARGUMENT;
    cui_edit_state state = cui_edit_get_state(edit);
    cui_edit_result result = cui_edit_select(edit, 0, state.length);
    if (result != CUI_EDIT_OK)
        return result;
    return cui_edit_insert(edit, (cui_str){text, n});
}
static bool cui_numeric_prefix(const char *s, size_t n, bool exponent) {
    size_t i = 0, digits = 0;
    if (i < n && (s[i] == '+' || s[i] == '-'))
        ++i;
    while (i < n && s[i] >= '0' && s[i] <= '9') {
        ++i;
        ++digits;
    }
    if (i < n && s[i] == '.') {
        ++i;
        while (i < n && s[i] >= '0' && s[i] <= '9') {
            ++i;
            ++digits;
        }
    }
    if (exponent && digits && i < n && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < n && (s[i] == '+' || s[i] == '-'))
            ++i;
        while (i < n && s[i] >= '0' && s[i] <= '9')
            ++i;
    }
    return i == n;
}
static cui_edit_result cui_numeric_insert(cui_edit *edit, cui_str text, bool exponent,
                                          cui_edit_composition *composition) {
    char normalized[128];
    size_t n = 0;
    for (size_t i = 0; i < text.length;) {
        unsigned char c = (unsigned char)text.data[i];
        if (c < 128) {
            normalized[n++] = (char)c;
            ++i;
        } else if (i + 2 < text.length && c == 0xef && (unsigned char)text.data[i + 1] == 0xbc) {
            unsigned char tail = (unsigned char)text.data[i + 2];
            if (tail >= 0x90 && tail <= 0x99)
                normalized[n++] = (char)('0' + tail - 0x90);
            else if (tail == 0x8b)
                normalized[n++] = '+';
            else if (tail == 0x8d)
                normalized[n++] = '-';
            else if (tail == 0x8e)
                normalized[n++] = '.';
            else
                return CUI_EDIT_INVALID_ARGUMENT;
            i += 3;
        } else if (i + 2 < text.length && c == 0xe3 && (unsigned char)text.data[i + 1] == 0x80 &&
                   (unsigned char)text.data[i + 2] == 0x82) {
            normalized[n++] = '.';
            i += 3;
        } else
            return CUI_EDIT_INVALID_ARGUMENT;
        if (n >= sizeof(normalized))
            return CUI_EDIT_CAPACITY;
    }
    cui_edit_state state = cui_edit_get_state(edit);
    cui_str old = cui_edit_get_text(edit);
    size_t begin = state.anchor < state.cursor ? state.anchor : state.cursor;
    size_t end = state.anchor > state.cursor ? state.anchor : state.cursor;
    if (old.length > 127 || n > 127 - (old.length - (end - begin)))
        return CUI_EDIT_CAPACITY;
    char candidate[128];
    size_t length = old.length - (end - begin) + n;
    memcpy(candidate, old.data, begin);
    memcpy(candidate + begin, normalized, n);
    memcpy(candidate + begin + n, old.data + end, old.length - end);
    if (!cui_numeric_prefix(candidate, length, exponent))
        return CUI_EDIT_INVALID_ARGUMENT;
    if (composition)
        return cui_edit_composition_event(edit, composition,
                                          (cui_event){.type = CUI_EVENT_COMPOSITION_COMMIT,
                                                      .text_data = normalized,
                                                      .text_length = n},
                                          true);
    return cui_edit_insert(edit, (cui_str){normalized, n});
}
static double cui_numeric_clamp(double value, cui_number_options o) {
    if (o.has_minimum && isfinite(o.minimum) && value < o.minimum)
        value = o.minimum;
    if (o.has_maximum && isfinite(o.maximum) && value > o.maximum)
        value = o.maximum;
    return value;
}
static void cui_numeric_try_step(cui_number_state *s, double *value, int direction,
                                 cui_number_options options, cui_number_result *result) {
    double draft = *value;
    cui_number_parse(cui_edit_get_text(s->edit), &draft);
    result->step_direction = direction;
    if (cui_number_step(&draft, direction, options)) {
        cui_edit_result error = cui_numeric_set_draft(s->edit, draft);
        if (error == CUI_EDIT_OK)
            *value = draft;
        else if (result->error == CUI_EDIT_OK)
            result->error = error;
    }
}
static cui_edit_composition *cui_numeric_composition(cui_context *ui, cui_id id,
                                                     cui_number_state *s) {
    if (s->composition && !s->composition_owned)
        return s->composition;
    bool needed = s->composition_owned;
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count && !needed; i++)
        needed = cui_event_target(ui, i) == id && events[i].type >= CUI_EVENT_COMPOSITION_START &&
                 events[i].type <= CUI_EVENT_COMPOSITION_COMMIT;
    if (!needed)
        return NULL;
    typedef struct {
        cui_edit_composition session;
        char text[4096];
    } storage;
    storage *r =
        cui_resource_get(ui, id, UINT64_C(0x8ec3f32cbd7a9510), sizeof(storage), NULL, NULL);
    if (!r)
        return NULL;
    if (!r->session.buffer) {
        r->session.buffer = r->text;
        r->session.capacity = sizeof(r->text);
        r->session.grow_view = true;
    }
    s->composition_owned = true;
    return &r->session;
}
cui_number_result cui_base_number_input(cui_context *ui, cui_id id, cui_str name,
                                        cui_number_state *s, double *value, cui_number_options o) {
    cui_number_result result = {0};
    if (!ui || !s || !s->edit || !value || !isfinite(*value) ||
        cui_edit_get_state(s->edit).multiline || (o.has_minimum && !isfinite(o.minimum)) ||
        (o.has_maximum && !isfinite(o.maximum)) ||
        (o.has_minimum && o.has_maximum && o.minimum > o.maximum)) {
        result.error = CUI_EDIT_INVALID_ARGUMENT;
        result.invalid = true;
        return result;
    }
    bool focused = cui_focused(ui) == id;
    if (!s->initialized || *value != s->last_value) {
        result.error = cui_numeric_set_draft(s->edit, *value);
        s->initialized = true;
        s->invalid = false;
    }
    double original = *value;
    bool editable = !o.disabled && !o.readonly;
    s->composition = cui_numeric_composition(ui, id, s);
    if (s->composition_owned && s->composition)
        s->composition->grow_view = true;
    if (s->composition) {
        bool active = s->composition->active;
        (void)cui_edit_composition_event(s->edit, s->composition,
                                         (cui_event){.type = CUI_EVENT_GESTURE}, true);
        if (active && !s->composition->active)
            s->composition_stale = true;
        if (!editable || !focused) {
            s->composition_stale |= s->composition->active;
            (void)cui_edit_composition_event(
                s->edit, s->composition, (cui_event){.type = CUI_EVENT_COMPOSITION_CANCEL}, false);
        }
    }
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    bool commit = s->was_focused && !focused && editable;
    for (size_t i = 0; i < count && editable; ++i) {
        cui_id activation = cui_event_activation(ui, i);
        if (activation == cui_numeric_part_id(id, 1) || activation == cui_numeric_part_id(id, 2)) {
            if (s->composition && s->composition->active) {
                s->composition_stale = true;
                (void)cui_edit_composition_event(s->edit, s->composition,
                                                 (cui_event){.type = CUI_EVENT_COMPOSITION_CANCEL},
                                                 true);
            }
            cui_numeric_try_step(s, value, activation == cui_numeric_part_id(id, 2) ? 1 : -1, o,
                                 &result);
            commit = false;
            continue;
        }
        const cui_event *e = &events[i];
        if (cui_event_target(ui, i) != id || cui_event_consumed(ui, i))
            continue;
        if (e->type == CUI_EVENT_KEY_DOWN && e->ime_composing)
            continue;
        if (e->type == CUI_EVENT_CANCEL ||
            (e->type >= CUI_EVENT_COMPOSITION_START && e->type <= CUI_EVENT_COMPOSITION_CANCEL)) {
            cui_edit_result r =
                e->type == CUI_EVENT_CANCEL || e->type == CUI_EVENT_COMPOSITION_CANCEL
                    ? CUI_EDIT_OK
                    : CUI_EDIT_CAPACITY;
            if (s->composition) {
                if (e->type == CUI_EVENT_COMPOSITION_COMMIT && s->composition_stale) {
                    cui_consume_event(ui, i);
                    continue;
                }
                if (e->type == CUI_EVENT_COMPOSITION_COMMIT) {
                    size_t n = 0;
                    while (n < sizeof(e->text) && e->text[n])
                        ++n;
                    cui_str text = e->text_data ? (cui_str){e->text_data, e->text_length}
                                                : (cui_str){e->text, n};
                    r = cui_numeric_insert(s->edit, text, o.allow_exponent, s->composition);
                } else
                    r = cui_edit_composition_event(s->edit, s->composition, *e, true);
                if (r == CUI_EDIT_OK && e->type == CUI_EVENT_COMPOSITION_COMMIT)
                    s->composition_stale = true;
                if (r == CUI_EDIT_OK && (e->type == CUI_EVENT_COMPOSITION_START ||
                                         e->type == CUI_EVENT_COMPOSITION_UPDATE))
                    s->composition_stale = false;
            }
            if (result.error == CUI_EDIT_OK)
                result.error = r;
            cui_consume_event(ui, i);
            continue;
        }
        if (s->composition && s->composition->active) {
            if (e->type == CUI_EVENT_KEY_DOWN || e->type == CUI_EVENT_TEXT)
                continue;
            if (e->type == CUI_EVENT_POINTER_DOWN) {
                s->composition_stale = true;
                (void)cui_edit_composition_event(s->edit, s->composition,
                                                 (cui_event){.type = CUI_EVENT_COMPOSITION_CANCEL},
                                                 true);
            }
        }
        cui_edit_result error = CUI_EDIT_OK;
        if (e->type == CUI_EVENT_TEXT) {
            size_t n = 0;
            while (n < sizeof(e->text) && e->text[n])
                ++n;
            error = cui_numeric_insert(s->edit,
                                       e->text_data ? (cui_str){e->text_data, e->text_length}
                                                    : (cui_str){e->text, n},
                                       o.allow_exponent, NULL);
        } else if (e->type == CUI_EVENT_KEY_DOWN) {
            bool extend = (e->modifiers & CUI_MOD_SHIFT) != 0;
            if (e->key == CUI_KEY_Z && (e->modifiers & (CUI_MOD_CTRL | CUI_MOD_META)))
                error =
                    e->modifiers & CUI_MOD_SHIFT ? cui_edit_redo(s->edit) : cui_edit_undo(s->edit);
            else if (e->key == CUI_KEY_Y && (e->modifiers & (CUI_MOD_CTRL | CUI_MOD_META)))
                error = cui_edit_redo(s->edit);
            else if (e->key == CUI_KEY_ENTER) {
                commit = true;
                result.submitted = true;
            } else if (e->key == CUI_KEY_UP || e->key == CUI_KEY_DOWN)
                cui_numeric_try_step(s, value, e->key == CUI_KEY_UP ? 1 : -1, o, &result);
            else if (e->key == CUI_KEY_A && (e->modifiers & (CUI_MOD_CTRL | CUI_MOD_META)))
                error = cui_edit_select(s->edit, 0, cui_edit_get_state(s->edit).length);
            else if (e->key == CUI_KEY_LEFT || e->key == CUI_KEY_RIGHT)
                error = cui_edit_move(
                    s->edit, e->key == CUI_KEY_LEFT ? CUI_EDIT_PREVIOUS : CUI_EDIT_NEXT, extend);
            else if (e->key == CUI_KEY_HOME || e->key == CUI_KEY_END)
                error = cui_edit_move(
                    s->edit, e->key == CUI_KEY_HOME ? CUI_EDIT_START : CUI_EDIT_END, extend);
            else if (e->key == CUI_KEY_BACKSPACE)
                error = cui_edit_backspace(s->edit);
            else if (e->key == CUI_KEY_DELETE)
                error = cui_edit_delete(s->edit);
        }
        if (result.error == CUI_EDIT_OK && error != CUI_EDIT_OK)
            result.error = error;
    }
    double parsed;
    s->invalid = !cui_number_parse(cui_edit_get_text(s->edit), &parsed);
    if (!s->invalid && editable) {
        *value = commit ? cui_numeric_clamp(parsed, o) : parsed;
        if (commit) {
            cui_edit_result error = cui_numeric_set_draft(s->edit, *value);
            if (result.error == CUI_EDIT_OK)
                result.error = error;
            if (error != CUI_EDIT_OK)
                *value = parsed;
        }
    }
    cui_interact(ui, id, CUI_ROLE_TEXTBOX, name, o.disabled, false,
                 cui_numeric_semantic_value(*value));
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){
            .readonly = o.readonly,
            .invalid = s->invalid,
            .minimum = cui_numeric_semantic_value(o.has_minimum ? o.minimum : -FLT_MAX),
            .maximum = cui_numeric_semantic_value(o.has_maximum ? o.maximum : FLT_MAX)});
    cui_interact(ui, cui_numeric_part_id(id, 1), CUI_ROLE_BUTTON, cui_string("Decrement"),
                 !editable, false, 0);
    cui_interact(ui, cui_numeric_part_id(id, 2), CUI_ROLE_BUTTON, cui_string("Increment"),
                 !editable, false, 0);
    for (unsigned part = 1; part <= 2; part++) {
        cui_id button = cui_numeric_part_id(id, part);
        cui_set_tab_stop(ui, button, false);
        cui_set_semantic_details(ui, button,
                                 (cui_semantic_details){.controls = id,
                                                        .no_focus = true,
                                                        .focus_target = id,
                                                        .pointer_activatable = true,
                                                        .cursor = CUI_CURSOR_POINTER});
    }
    s->last_value = *value;
    s->was_focused = focused;
    result.invalid = s->invalid;
    result.changed = *value != original;
    if (result.changed)
        cui_invalidate(ui);
    return result;
}
bool cui_base_pagination(cui_context *ui, cui_id id, size_t *page, size_t total, bool compact,
                         bool disabled) {
    if (!ui || !page)
        return false;
    size_t original = *page;
    if (!total)
        *page = 0;
    else if (*page < 1)
        *page = 1;
    else if (*page > total)
        *page = total;
    size_t previous = *page;
    cui_page_item old_items[11];
    size_t old_count = compact ? 0 : cui_pagination_items(previous, total, 2, old_items);
    size_t n;
    cui_frame_events(ui, &n);
    for (size_t e = 0; e < n && !disabled; ++e) {
        cui_id activation = cui_event_activation(ui, e);
        if (activation == cui_numeric_part_id(id, 1) && total)
            *page = 1;
        else if (activation == cui_numeric_part_id(id, 2) && *page > 1)
            --*page;
        else if (activation == cui_numeric_part_id(id, 3) && *page < total)
            ++*page;
        else if (activation == cui_numeric_part_id(id, 4) && total)
            *page = total;
        else
            for (size_t i = 0; i < old_count; ++i)
                if (!old_items[i].ellipsis &&
                    activation == cui_numeric_part_id(id, 10 + (unsigned)i))
                    *page = old_items[i].page;
    }
    cui_interact(ui, id, CUI_ROLE_GROUP, cui_string("Pagination"), disabled, false, (float)*page);
    cui_set_tab_stop(ui, id, false);
    const char *names[] = {"First", "Previous", "Next", "Last"};
    for (unsigned i = 0; i < 4; ++i)
        cui_interact(ui, cui_numeric_part_id(id, 1 + i), CUI_ROLE_BUTTON, cui_string(names[i]),
                     disabled || !total || (i < 2 ? *page <= 1 : *page >= total), false, 0);
    cui_page_item items[11];
    size_t count = compact ? 0 : cui_pagination_items(*page, total, 2, items);
    for (size_t i = 0; i < count; ++i)
        if (!items[i].ellipsis) {
            char label[32];
            snprintf(label, sizeof(label), "%zu", items[i].page);
            cui_id part = cui_numeric_part_id(id, 10 + (unsigned)i);
            cui_interact(ui, part, CUI_ROLE_BUTTON, cui_string(label),
                         disabled || items[i].page == *page, false, 0);
            cui_set_semantic_details(
                ui, part, (cui_semantic_details){.selected = items[i].page == *page, .parent = id});
        }
    if (*page != original)
        cui_invalidate(ui);
    return *page != original;
}
bool cui_base_rating(cui_context *ui, cui_id id, cui_str name, float *value, cui_rating_options o) {
    if (!ui || !value)
        return false;
    unsigned maximum = o.maximum ? o.maximum : 5;
    if (maximum > 100)
        maximum = 100;
    float before = *value, step = o.half_steps ? .5f : 1;
    if (!isfinite(*value))
        *value = 0;
    *value = fmaxf(0, fminf((float)maximum, roundf(*value / step) * step));
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t e = 0; e < count && !o.disabled && !o.readonly; ++e) {
        cui_id activation = cui_event_activation(ui, e), target = cui_event_target(ui, e);
        if (o.clearable && activation == cui_numeric_part_id(id, 1)) {
            *value = 0;
            cui_focus(ui, id);
        }
        for (unsigned i = 0; i < maximum; ++i)
            if (activation == cui_numeric_part_id(id, 10 + i)) {
                *value = (float)i + 1;
                cui_rect bounds = cui_bounds(ui, activation);
                if (o.half_steps && events[e].type == CUI_EVENT_POINTER_UP &&
                    events[e].x < bounds.x + bounds.w / 2)
                    *value -= .5f;
                cui_focus(ui, id);
            }
        if (target == id && events[e].type == CUI_EVENT_KEY_DOWN) {
            cui_key key = events[e].key;
            if (key == CUI_KEY_HOME ||
                (o.clearable && (key == CUI_KEY_DELETE || key == CUI_KEY_BACKSPACE)))
                *value = 0;
            else if (key == CUI_KEY_END)
                *value = (float)maximum;
            else if (key == CUI_KEY_LEFT || key == CUI_KEY_DOWN)
                *value = fmaxf(0, *value - step);
            else if (key == CUI_KEY_RIGHT || key == CUI_KEY_UP)
                *value = fminf((float)maximum, *value + step);
        }
    }
    cui_interact(ui, id, CUI_ROLE_SLIDER, name, o.disabled, false, *value);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.readonly = o.readonly, .minimum = 0, .maximum = (float)maximum});
    for (unsigned i = 0; i < maximum; ++i) {
        char label[40];
        snprintf(label, sizeof(label), "%u of %u", i + 1, maximum);
        cui_id part = cui_numeric_part_id(id, 10 + i);
        cui_interact(ui, part, o.readonly ? CUI_ROLE_LABEL : CUI_ROLE_BUTTON, cui_string(label),
                     o.disabled, false, *value);
        cui_set_tab_stop(ui, part, false);
        cui_set_semantic_details(ui, part,
                                 (cui_semantic_details){.selected = *value >= (float)i + 1,
                                                        .parent = id,
                                                        .readonly = o.readonly});
    }
    if (o.clearable)
        cui_interact(ui, cui_numeric_part_id(id, 1), CUI_ROLE_BUTTON, cui_string("Clear"),
                     o.disabled || o.readonly, false, 0);
    bool changed = !isfinite(before) || *value != before;
    if (changed)
        cui_invalidate(ui);
    return changed;
}
bool cui_base_stepper(cui_context *ui, cui_id id, const cui_step *steps, size_t count,
                      size_t *current, bool disabled) {
    if (!ui || !current || (!steps && count) || count > 100)
        return false;
    size_t before = *current;
    if (count && *current >= count)
        *current = count - 1;
    else if (!count)
        *current = 0;
    size_t events;
    cui_frame_events(ui, &events);
    for (size_t e = 0; e < events && !disabled; ++e)
        for (size_t i = 0; i < count; ++i)
            if (!steps[i].disabled &&
                cui_event_activation(ui, e) == cui_numeric_part_id(id, 1 + (unsigned)i))
                *current = i;
    cui_interact(ui, id, CUI_ROLE_GROUP, cui_string("Steps"), disabled, false, (float)*current);
    cui_set_tab_stop(ui, id, false);
    for (size_t i = 0; i < count; ++i) {
        cui_id part = cui_numeric_part_id(id, 1 + (unsigned)i);
        cui_interact(ui, part, CUI_ROLE_BUTTON, steps[i].title, disabled || steps[i].disabled,
                     false, (float)i);
        cui_set_semantic_details(ui, part,
                                 (cui_semantic_details){.selected = i == *current,
                                                        .description = steps[i].description,
                                                        .parent = id});
    }
    if (*current != before)
        cui_invalidate(ui);
    return *current != before;
}
