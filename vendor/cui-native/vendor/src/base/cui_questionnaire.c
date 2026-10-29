/* SPDX-License-Identifier: MIT */
#include "cui/cui_questionnaire.h"
#include "cui_questionnaire_observer_internal.h"
#include <math.h>
#include <string.h>
static bool cui_questionnaire_equal(cui_str a, cui_str b) {
    return a.length == b.length &&
           (!a.length || (a.data && b.data && !memcmp(a.data, b.data, a.length)));
}
static bool cui_questionnaire_valid(const cui_questionnaire_state *s, size_t i) {
    return s && s->initialized && i < s->count;
}
static bool cui_questionnaire_blank(cui_str t) {
    for (size_t i = 0; i < t.length;) {
        unsigned c = (unsigned char)t.data[i++];
        if (c >= 128) {
            unsigned n = c < 224 ? 1 : c < 240 ? 2 : 3;
            c &= n == 1 ? 31 : n == 2 ? 15 : 7;
            while (n-- && i < t.length)
                c = (c << 6) | ((unsigned char)t.data[i++] & 63);
        }
        if (!(c == 32 || (c >= 9 && c <= 13) || c == 0x85 || c == 0xa0 || c == 0x1680 ||
              (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f ||
              c == 0x205f || c == 0x3000))
            return false;
    }
    return true;
}
static void cui_questionnaire_emit_scope(cui_questionnaire_state *s,
                                         cui_questionnaire_event_kind kind, size_t item,
                                         size_t previous,
                                         cui_questionnaire_observation_scope *scope) {
    cui_questionnaire_observation_deliver(scope, kind);
    if (s->event)
        s->event(s, (cui_questionnaire_event){kind, item, previous}, s->event_user);
}
static void cui_questionnaire_emit(cui_questionnaire_state *s, cui_questionnaire_event_kind kind,
                                   size_t item, size_t previous) {
    cui_questionnaire_observation_scope scope =
        cui_questionnaire_observation_begin(s, kind, item, previous);
    cui_questionnaire_emit_scope(s, kind, item, previous, &scope);
    cui_questionnaire_observation_end(&scope);
}
size_t cui_questionnaire_find(const cui_questionnaire_state *s, cui_str name) {
    if (s && s->initialized)
        for (size_t i = 0; i < s->count; i++)
            if (cui_questionnaire_equal(s->items[i].name, name))
                return i;
    return SIZE_MAX;
}
size_t cui_questionnaire_total(const cui_questionnaire_state *s) {
    size_t n = 0;
    if (s && s->initialized)
        for (size_t i = 0; i < s->count; i++)
            n += !s->storage.items[i].disabled;
    return n;
}
size_t cui_questionnaire_position(const cui_questionnaire_state *s) {
    if (!s || !s->initialized || s->current >= s->count)
        return SIZE_MAX;
    size_t n = 0;
    for (size_t i = 0; i < s->current; i++)
        n += !s->storage.items[i].disabled;
    return n;
}
bool cui_questionnaire_choice_selected(const cui_questionnaire_state *s, size_t i, size_t c) {
    if (!cui_questionnaire_valid(s, i) || c >= s->items[i].choice_count ||
        s->storage.items[i].disabled)
        return false;
    cui_questionnaire_choice_runtime r = s->storage.choices[s->storage.items[i].choice_offset + c];
    return r.selected && !r.disabled;
}
bool cui_questionnaire_choice_position(const cui_questionnaire_state *s, size_t item, size_t choice,
                                       size_t *position, size_t *total) {
    if (!position || !total || !cui_questionnaire_valid(s, item) ||
        choice >= s->items[item].choice_count || s->storage.items[item].disabled)
        return false;
    const cui_questionnaire_choice_runtime *choices =
        s->storage.choices + s->storage.items[item].choice_offset;
    if (choices[choice].disabled)
        return false;
    size_t count = 0, index = 0;
    for (size_t i = 0; i < s->items[item].choice_count; i++) {
        if (!choices[i].disabled)
            count++;
        if (i == choice)
            index = count;
    }
    *position = index;
    *total = count;
    return true;
}
cui_str cui_questionnaire_freeform(const cui_questionnaire_state *s, size_t i) {
    if (!cui_questionnaire_valid(s, i) || s->storage.items[i].disabled ||
        s->items[i].input_disabled)
        return (cui_str){0};
    const cui_questionnaire_item_runtime *r = &s->storage.items[i];
    return (cui_str){r->answer, r->answer_length};
}
cui_questionnaire_status cui_questionnaire_item_status(const cui_questionnaire_state *s, size_t i) {
    if (!cui_questionnaire_valid(s, i))
        return CUI_QUESTIONNAIRE_UNANSWERED;
    if (s->storage.items[i].skipped)
        return CUI_QUESTIONNAIRE_SKIPPED;
    if (cui_questionnaire_freeform(s, i).length)
        return CUI_QUESTIONNAIRE_ANSWERED;
    for (size_t c = 0; c < s->items[i].choice_count; c++)
        if (cui_questionnaire_choice_selected(s, i, c))
            return CUI_QUESTIONNAIRE_ANSWERED;
    return CUI_QUESTIONNAIRE_UNANSWERED;
}
cui_questionnaire_error cui_questionnaire_item_error(const cui_questionnaire_state *s, size_t i) {
    if (!cui_questionnaire_valid(s, i) || s->storage.items[i].disabled ||
        s->storage.items[i].skipped)
        return (cui_questionnaire_error){0};
    const cui_questionnaire_item_runtime *r = &s->storage.items[i];
    if (r->has_external_error)
        return (cui_questionnaire_error){CUI_QUESTIONNAIRE_MESSAGE, r->external_error};
    return r->validation_attempted ? r->internal_error : (cui_questionnaire_error){0};
}
cui_questionnaire_result cui_questionnaire_init(cui_questionnaire_state *s,
                                                const cui_questionnaire_item *items, size_t count,
                                                cui_questionnaire_workspace w,
                                                cui_questionnaire_event_fn event, void *user) {
    if (!s || s->initialized || (count && !items) || (w.item_capacity && !w.items) ||
        (w.choice_capacity && !w.choices) || (w.text_capacity && !w.text) || count > 4096)
        return CUI_QUESTIONNAIRE_INVALID;
    if (count > w.item_capacity)
        return CUI_QUESTIONNAIRE_CAPACITY;
    if (!w.max_answer_bytes)
        w.max_answer_bytes = 4096;
    if (w.max_answer_bytes > 1024 * 1024)
        return CUI_QUESTIONNAIRE_CAPACITY;
    size_t choices = 0, bytes = 0;
    for (size_t i = 0; i < count; i++) {
        const cui_questionnaire_item *d = &items[i];
        if (!cui_edit_valid_utf8(d->name) || !cui_edit_valid_utf8(d->label) ||
            !cui_edit_valid_utf8(d->description) || !cui_edit_valid_utf8(d->input_label) ||
            (d->choice_count && !d->choices))
            return CUI_QUESTIONNAIRE_INVALID;
        for (size_t j = 0; j < i; j++)
            if (cui_questionnaire_equal(d->name, items[j].name))
                return CUI_QUESTIONNAIRE_DUPLICATE_ITEM;
        if (d->choice_count > w.choice_capacity - choices)
            return CUI_QUESTIONNAIRE_CAPACITY;
        size_t defaults = 0;
        for (size_t c = 0; c < d->choice_count; c++) {
            const cui_questionnaire_choice *v = &d->choices[c];
            if (!cui_edit_valid_utf8(v->value) || !cui_edit_valid_utf8(v->label) ||
                !cui_edit_valid_utf8(v->description))
                return CUI_QUESTIONNAIRE_INVALID;
            for (size_t p = 0; p < c; p++)
                if (cui_questionnaire_equal(v->value, d->choices[p].value))
                    return CUI_QUESTIONNAIRE_DUPLICATE_CHOICE;
            defaults += v->default_selected;
        }
        if (!d->multiple && defaults > 1)
            return CUI_QUESTIONNAIRE_MULTIPLE_DEFAULTS;
        choices += d->choice_count;
        if (d->input) {
            size_t needed = 3 * (w.max_answer_bytes + 1);
            if (cui_edit_get_text(d->input).length > w.max_answer_bytes ||
                needed > w.text_capacity - bytes)
                return CUI_QUESTIONNAIRE_CAPACITY;
            bytes += needed;
        }
    }
    *s = (cui_questionnaire_state){.items = items,
                                   .count = count,
                                   .current = SIZE_MAX,
                                   .initial_current = SIZE_MAX,
                                   .storage = w,
                                   .event = event,
                                   .event_user = user,
                                   .initialized = true};
    choices = bytes = 0;
    for (size_t i = 0; i < count; i++) {
        const cui_questionnaire_item *d = &items[i];
        cui_questionnaire_item_runtime *r = &w.items[i];
        *r = (cui_questionnaire_item_runtime){.choice_offset = choices, .disabled = d->disabled};
        bool input_answer = false;
        if (d->input) {
            cui_str initial = cui_edit_get_text(d->input);
            size_t stride = w.max_answer_bytes + 1;
            r->answer = w.text + bytes;
            r->initial_answer = r->answer + stride;
            r->initial_input = r->initial_answer + stride;
            bytes += 3 * stride;
            memcpy(r->initial_input, initial.data, initial.length);
            r->initial_input[initial.length] = 0;
            r->initial_input_length = initial.length;
            input_answer = !d->input_disabled && !cui_questionnaire_blank(initial);
            r->answer_length = r->initial_answer_length = input_answer ? initial.length : 0;
            memcpy(r->answer, initial.data, r->answer_length);
            r->answer[r->answer_length] = 0;
            memcpy(r->initial_answer, r->answer, r->answer_length + 1);
        }
        for (size_t c = 0; c < d->choice_count; c++) {
            bool selected = d->choices[c].default_selected && !d->choices[c].disabled &&
                            (d->multiple || !input_answer);
            w.choices[choices++] =
                (cui_questionnaire_choice_runtime){selected, selected, d->choices[c].disabled};
        }
        if (s->current == SIZE_MAX && !d->disabled)
            s->current = i;
    }
    s->initial_current = s->current;
    return CUI_QUESTIONNAIRE_OK;
}
bool cui_questionnaire_validate_item(cui_questionnaire_state *s, size_t i) {
    if (!cui_questionnaire_valid(s, i))
        return false;
    cui_questionnaire_item_runtime *r = &s->storage.items[i];
    if (r->disabled || r->skipped)
        return true;
    r->validation_attempted = true;
    r->internal_error = (cui_questionnaire_error){0};
    if (cui_questionnaire_item_status(s, i) == CUI_QUESTIONNAIRE_UNANSWERED)
        r->internal_error.kind =
            s->items[i].required ? CUI_QUESTIONNAIRE_REQUIRED : CUI_QUESTIONNAIRE_UNFILLED;
    else if (s->items[i].validate) {
        cui_str error = s->items[i].validate(s, i, s->items[i].validate_user);
        if (error.length)
            r->internal_error = (cui_questionnaire_error){CUI_QUESTIONNAIRE_MESSAGE, error};
    }
    return cui_questionnaire_item_error(s, i).kind == CUI_QUESTIONNAIRE_VALID;
}
static void cui_questionnaire_changed(cui_questionnaire_state *s, size_t i, bool emit) {
    s->pending_confirm = false;
    s->complete = false;
    if (s->storage.items[i].validation_attempted)
        (void)cui_questionnaire_validate_item(s, i);
    else
        s->storage.items[i].internal_error = (cui_questionnaire_error){0};
    if (emit)
        cui_questionnaire_emit(s, CUI_QUESTIONNAIRE_ANSWER_CHANGED, i, SIZE_MAX);
}
static void cui_questionnaire_current(cui_questionnaire_state *s, size_t next, bool emit) {
    if (s->current == next)
        return;
    size_t previous = s->current;
    s->pending_confirm = false;
    s->current = next;
    if (emit)
        cui_questionnaire_emit(s, CUI_QUESTIONNAIRE_CURRENT_CHANGED, next, previous);
}
cui_questionnaire_result cui_questionnaire_set_current(cui_questionnaire_state *s, size_t i,
                                                       bool initial) {
    if (!cui_questionnaire_valid(s, i))
        return CUI_QUESTIONNAIRE_UNKNOWN_ITEM;
    if (!s->storage.items[i].disabled) {
        s->pending_confirm = false;
        s->current = i;
        if (initial)
            s->initial_current = i;
    }
    return CUI_QUESTIONNAIRE_OK;
}
cui_questionnaire_result cui_questionnaire_set_answer(cui_questionnaire_state *s, size_t i,
                                                      const size_t *selected, size_t count,
                                                      cui_str freeform) {
    if (!cui_questionnaire_valid(s, i))
        return CUI_QUESTIONNAIRE_UNKNOWN_ITEM;
    if ((count && !selected) || !cui_edit_valid_utf8(freeform))
        return CUI_QUESTIONNAIRE_INVALID;
    if (cui_questionnaire_blank(freeform))
        freeform = (cui_str){0};
    const cui_questionnaire_item *d = &s->items[i];
    cui_questionnaire_item_runtime *r = &s->storage.items[i];
    size_t unique = 0;
    for (size_t n = 0; n < count; n++) {
        if (selected[n] >= d->choice_count)
            return CUI_QUESTIONNAIRE_UNKNOWN_CHOICE;
        if (s->storage.choices[r->choice_offset + selected[n]].disabled)
            return CUI_QUESTIONNAIRE_ANSWER_MISMATCH;
        bool prior = false;
        for (size_t p = 0; p < n; p++)
            prior |= selected[p] == selected[n];
        unique += !prior;
    }
    if ((!d->multiple && unique + (freeform.length != 0) > 1) || (freeform.length && !d->input))
        return CUI_QUESTIONNAIRE_ANSWER_MISMATCH;
    if (freeform.length > s->storage.max_answer_bytes)
        return CUI_QUESTIONNAIRE_CAPACITY;
    if (freeform.length) {
        cui_edit_result e =
            cui_edit_replace_range(d->input, 0, cui_edit_get_text(d->input).length, freeform);
        if (e != CUI_EDIT_OK)
            return CUI_QUESTIONNAIRE_CAPACITY;
        freeform = cui_edit_get_text(d->input);
    }
    cui_str effective_freeform = r->disabled || d->input_disabled ? (cui_str){0} : freeform;
    bool changed = r->skipped ||
                   !cui_questionnaire_equal(cui_questionnaire_freeform(s, i), effective_freeform);
    for (size_t c = 0; c < d->choice_count; c++) {
        bool value = false;
        for (size_t n = 0; n < count; n++)
            value |= selected[n] == c;
        cui_questionnaire_choice_runtime *choice = &s->storage.choices[r->choice_offset + c];
        changed |= !r->disabled && !choice->disabled && choice->selected != value;
        choice->selected = value;
    }
    r->skipped = false;
    r->answer_length = freeform.length;
    if (r->answer) {
        if (freeform.length)
            memmove(r->answer, freeform.data, freeform.length);
        r->answer[freeform.length] = 0;
    }
    if (changed)
        cui_questionnaire_changed(s, i, false);
    return CUI_QUESTIONNAIRE_OK;
}
cui_questionnaire_result cui_questionnaire_sync_input(cui_questionnaire_state *s, size_t i,
                                                      bool emit) {
    if (!cui_questionnaire_valid(s, i))
        return CUI_QUESTIONNAIRE_UNKNOWN_ITEM;
    const cui_questionnaire_item *d = &s->items[i];
    cui_questionnaire_item_runtime *r = &s->storage.items[i];
    if (!d->input)
        return CUI_QUESTIONNAIRE_ANSWER_MISMATCH;
    if (r->disabled || d->input_disabled)
        return CUI_QUESTIONNAIRE_OK;
    cui_str text = cui_edit_get_text(d->input);
    if (text.length > s->storage.max_answer_bytes)
        return CUI_QUESTIONNAIRE_CAPACITY;
    if (cui_questionnaire_blank(text))
        text.length = 0;
    bool changed = r->answer_length != text.length ||
                   (text.length && memcmp(r->answer, text.data, text.length));
    if (text.length && !d->multiple)
        for (size_t c = 0; c < d->choice_count; c++) {
            cui_questionnaire_choice_runtime *choice = &s->storage.choices[r->choice_offset + c];
            changed |= choice->selected;
            choice->selected = false;
        }
    if (text.length) {
        changed |= r->skipped;
        r->skipped = false;
    }
    if (text.length)
        memmove(r->answer, text.data, text.length);
    r->answer[text.length] = 0;
    r->answer_length = text.length;
    if (changed)
        cui_questionnaire_changed(s, i, emit);
    return CUI_QUESTIONNAIRE_OK;
}
cui_questionnaire_result cui_questionnaire_set_input(cui_questionnaire_state *s, size_t i,
                                                     cui_str text) {
    if (!cui_questionnaire_valid(s, i))
        return CUI_QUESTIONNAIRE_UNKNOWN_ITEM;
    if (!s->items[i].input)
        return CUI_QUESTIONNAIRE_ANSWER_MISMATCH;
    if (!cui_edit_valid_utf8(text))
        return CUI_QUESTIONNAIRE_INVALID;
    if (text.length > s->storage.max_answer_bytes)
        return CUI_QUESTIONNAIRE_CAPACITY;
    if (cui_edit_replace_range(s->items[i].input, 0, cui_edit_get_text(s->items[i].input).length,
                               text) != CUI_EDIT_OK)
        return CUI_QUESTIONNAIRE_CAPACITY;
    return cui_questionnaire_sync_input(s, i, false);
}
cui_questionnaire_result cui_questionnaire_disable_item(cui_questionnaire_state *s, size_t i,
                                                        bool disabled) {
    if (!cui_questionnaire_valid(s, i))
        return CUI_QUESTIONNAIRE_UNKNOWN_ITEM;
    cui_questionnaire_item_runtime *r = &s->storage.items[i];
    if (r->disabled == disabled)
        return CUI_QUESTIONNAIRE_OK;
    r->disabled = disabled;
    s->pending_confirm = false;
    s->complete = false;
    if (s->current == i && disabled) {
        size_t next = SIZE_MAX;
        for (size_t n = i + 1; n < s->count; n++)
            if (!s->storage.items[n].disabled) {
                next = n;
                break;
            }
        if (next == SIZE_MAX)
            for (size_t n = i; n > 0; n--)
                if (!s->storage.items[n - 1].disabled) {
                    next = n - 1;
                    break;
                }
        s->current = next;
    } else if (s->current == SIZE_MAX && !disabled)
        s->current = i;
    return CUI_QUESTIONNAIRE_OK;
}
cui_questionnaire_result cui_questionnaire_disable_choice(cui_questionnaire_state *s, size_t i,
                                                          size_t c, bool disabled) {
    if (!cui_questionnaire_valid(s, i))
        return CUI_QUESTIONNAIRE_UNKNOWN_ITEM;
    if (c >= s->items[i].choice_count)
        return CUI_QUESTIONNAIRE_UNKNOWN_CHOICE;
    cui_questionnaire_choice_runtime *r =
        &s->storage.choices[s->storage.items[i].choice_offset + c];
    if (r->disabled != disabled) {
        r->disabled = disabled;
        cui_questionnaire_changed(s, i, false);
    }
    return CUI_QUESTIONNAIRE_OK;
}
cui_questionnaire_result cui_questionnaire_external_error(cui_questionnaire_state *s, size_t i,
                                                          const cui_str *error) {
    if (!cui_questionnaire_valid(s, i))
        return CUI_QUESTIONNAIRE_UNKNOWN_ITEM;
    if (error && !cui_edit_valid_utf8(*error))
        return CUI_QUESTIONNAIRE_INVALID;
    s->storage.items[i].has_external_error = error != NULL;
    s->storage.items[i].external_error = error ? *error : (cui_str){0};
    if (error)
        s->complete = false;
    return CUI_QUESTIONNAIRE_OK;
}
cui_questionnaire_result cui_questionnaire_activate(cui_questionnaire_state *s, size_t i,
                                                    size_t c) {
    if (!cui_questionnaire_valid(s, i))
        return CUI_QUESTIONNAIRE_UNKNOWN_ITEM;
    const cui_questionnaire_item *d = &s->items[i];
    cui_questionnaire_item_runtime *r = &s->storage.items[i];
    if (c >= d->choice_count)
        return CUI_QUESTIONNAIRE_UNKNOWN_CHOICE;
    cui_questionnaire_choice_runtime *choice = &s->storage.choices[r->choice_offset + c];
    if (r->disabled || choice->disabled)
        return CUI_QUESTIONNAIRE_OK;
    bool changed = r->skipped || !choice->selected || d->multiple || r->answer_length;
    if (d->multiple)
        choice->selected = !choice->selected;
    else {
        for (size_t n = 0; n < d->choice_count; n++)
            s->storage.choices[r->choice_offset + n].selected = n == c;
        r->answer_length = 0;
        if (r->answer)
            r->answer[0] = 0;
    }
    r->skipped = false;
    if (changed)
        cui_questionnaire_changed(s, i, true);
    return CUI_QUESTIONNAIRE_OK;
}
cui_questionnaire_result cui_questionnaire_choose(cui_questionnaire_state *s, size_t i, size_t c,
                                                  double now, bool reduced) {
    if (!isfinite(now) || !isfinite(now + .15))
        return CUI_QUESTIONNAIRE_INVALID;
    if (!cui_questionnaire_valid(s, i))
        return CUI_QUESTIONNAIRE_UNKNOWN_ITEM;
    if (c >= s->items[i].choice_count)
        return CUI_QUESTIONNAIRE_UNKNOWN_CHOICE;
    if (s->storage.items[i].disabled ||
        s->storage.choices[s->storage.items[i].choice_offset + c].disabled)
        return CUI_QUESTIONNAIRE_OK;
    bool same = cui_questionnaire_choice_selected(s, i, c) && !s->storage.items[i].answer_length;
    cui_questionnaire_result result = cui_questionnaire_activate(s, i, c);
    if (result == CUI_QUESTIONNAIRE_OK && !s->items[i].multiple && s->current == i &&
        !s->storage.items[i].disabled) {
        if (same)
            (void)cui_questionnaire_confirm(s);
        else {
            s->pending_confirm = true;
            s->pending_item = i;
            s->pending_deadline = now + (reduced ? 0 : .15);
        }
    }
    return result;
}
bool cui_questionnaire_previous(cui_questionnaire_state *s) {
    if (!s || !s->initialized)
        return false;
    s->pending_confirm = false;
    if (s->current >= s->count)
        return false;
    for (size_t i = s->current; i > 0; i--)
        if (!s->storage.items[i - 1].disabled) {
            cui_questionnaire_current(s, i - 1, true);
            return true;
        }
    return false;
}
bool cui_questionnaire_next(cui_questionnaire_state *s) {
    if (!s || !s->initialized)
        return false;
    s->pending_confirm = false;
    if (!cui_questionnaire_validate_item(s, s->current))
        return false;
    for (size_t i = s->current + 1; i < s->count; i++)
        if (!s->storage.items[i].disabled) {
            cui_questionnaire_current(s, i, true);
            return true;
        }
    return false;
}
bool cui_questionnaire_submit(cui_questionnaire_state *s) {
    if (!s || !s->initialized)
        return false;
    s->pending_confirm = false;
    size_t first = SIZE_MAX;
    for (size_t i = 0; i < s->count; i++)
        if (!s->storage.items[i].disabled && !cui_questionnaire_validate_item(s, i) &&
            first == SIZE_MAX)
            first = i;
    if (first != SIZE_MAX) {
        cui_questionnaire_current(s, first, true);
        return false;
    }
    cui_questionnaire_observation_scope scope =
        cui_questionnaire_observation_begin(s, CUI_QUESTIONNAIRE_SUBMIT, SIZE_MAX, SIZE_MAX);
    if (!s->complete) {
        s->complete = true;
        cui_questionnaire_emit_scope(s, CUI_QUESTIONNAIRE_COMPLETED, SIZE_MAX, SIZE_MAX, &scope);
    }
    cui_questionnaire_emit_scope(s, CUI_QUESTIONNAIRE_SUBMIT, SIZE_MAX, SIZE_MAX, &scope);
    cui_questionnaire_observation_end(&scope);
    return true;
}
bool cui_questionnaire_confirm(cui_questionnaire_state *s) {
    size_t position = cui_questionnaire_position(s);
    if (position == SIZE_MAX)
        return false;
    return position + 1 == cui_questionnaire_total(s) ? cui_questionnaire_submit(s)
                                                      : cui_questionnaire_next(s);
}
bool cui_questionnaire_skip(cui_questionnaire_state *s) {
    if (!s || !cui_questionnaire_valid(s, s->current) || s->items[s->current].required)
        return false;
    size_t i = s->current;
    cui_questionnaire_item_runtime *r = &s->storage.items[i];
    for (size_t c = 0; c < s->items[i].choice_count; c++)
        s->storage.choices[r->choice_offset + c].selected = false;
    r->answer_length = 0;
    if (r->answer)
        r->answer[0] = 0;
    r->skipped = true;
    s->complete = false;
    cui_questionnaire_emit(s, CUI_QUESTIONNAIRE_ANSWER_CHANGED, i, SIZE_MAX);
    return cui_questionnaire_confirm(s);
}
bool cui_questionnaire_tick(cui_questionnaire_state *s, double now) {
    if (!s || !s->initialized || !s->pending_confirm || !isfinite(now) || now < s->pending_deadline)
        return false;
    size_t item = s->pending_item;
    s->pending_confirm = false;
    return s->current == item && cui_questionnaire_confirm(s);
}
bool cui_questionnaire_reset(cui_questionnaire_state *s) {
    if (!s || !s->initialized)
        return false;
    for (size_t i = 0; i < s->count; i++) {
        cui_questionnaire_item_runtime *r = &s->storage.items[i];
        const cui_questionnaire_item *d = &s->items[i];
        if (d->input && cui_edit_replace_range(
                            d->input, 0, cui_edit_get_text(d->input).length,
                            (cui_str){r->initial_input, r->initial_input_length}) != CUI_EDIT_OK)
            return false;
        r->skipped = r->validation_attempted = false;
        r->internal_error = (cui_questionnaire_error){0};
        r->answer_length = r->initial_answer_length;
        if (r->answer)
            memcpy(r->answer, r->initial_answer, r->answer_length + 1);
        for (size_t c = 0; c < d->choice_count; c++) {
            cui_questionnaire_choice_runtime *choice = &s->storage.choices[r->choice_offset + c];
            choice->selected = choice->initial_selected;
        }
    }
    s->complete = s->pending_confirm = false;
    s->current = s->initial_current;
    if (s->current < s->count && s->storage.items[s->current].disabled) {
        s->current = SIZE_MAX;
        for (size_t i = 0; i < s->count; i++)
            if (!s->storage.items[i].disabled) {
                s->current = i;
                break;
            }
    }
    return true;
}
char cui_questionnaire_shortcut(const cui_questionnaire_state *s, size_t i, size_t c,
                                bool numbers) {
    if (!cui_questionnaire_valid(s, i) || c >= s->items[i].choice_count ||
        s->storage.items[i].disabled)
        return 0;
    size_t offset = s->storage.items[i].choice_offset, n = 0;
    if (s->storage.choices[offset + c].disabled)
        return 0;
    for (size_t p = 0; p < c; p++)
        n += !s->storage.choices[offset + p].disabled;
    return n < (numbers ? 9 : 26) ? (char)((numbers ? '1' : 'A') + n) : 0;
}
bool cui_questionnaire_activate_shortcut(cui_questionnaire_state *s, char key, bool numbers,
                                         double now, bool reduced) {
    if (!s || !cui_questionnaire_valid(s, s->current))
        return false;
    if (key >= 'a' && key <= 'z')
        key = (char)(key - 'a' + 'A');
    for (size_t c = 0; c < s->items[s->current].choice_count; c++)
        if (key && cui_questionnaire_shortcut(s, s->current, c, numbers) == key)
            return cui_questionnaire_choose(s, s->current, c, now, reduced) == CUI_QUESTIONNAIRE_OK;
    return false;
}
