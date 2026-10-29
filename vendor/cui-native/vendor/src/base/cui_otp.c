/* SPDX-License-Identifier: MIT */
#include "cui/cui_otp.h"
#include <math.h>
#include <string.h>
cui_str cui_otp_value(const cui_otp_state *s) {
    if (!s || !s->buffer || !s->capacity)
        return (cui_str){0};
    const char *end = memchr(s->buffer, 0, s->capacity);
    return end ? (cui_str){s->buffer, (size_t)(end - s->buffer)} : (cui_str){0};
}
size_t cui_otp_count(const cui_otp_state *s) {
    cui_str value = cui_otp_value(s);
    size_t count = 0;
    for (size_t i = 0; i < value.length; i++)
        count += ((unsigned char)value.data[i] & 0xc0) != 0x80;
    return count;
}
cui_edit_result cui_otp_init(cui_otp_state *s, char *buffer, size_t capacity, size_t length) {
    if (!s || !buffer || !capacity || length > 256 || !memchr(buffer, 0, capacity))
        return CUI_EDIT_INVALID_ARGUMENT;
    cui_otp_state next = {
        .buffer = buffer, .capacity = capacity, .length = length, .cursor_visible = true};
    if (!cui_edit_valid_utf8(cui_otp_value(&next)))
        return CUI_EDIT_INVALID_UTF8;
    *s = next;
    return CUI_EDIT_OK;
}
cui_edit_result cui_otp_set_value(cui_otp_state *s, cui_str value) {
    if (!s || !s->buffer || !s->capacity || (!value.data && value.length))
        return CUI_EDIT_INVALID_ARGUMENT;
    if (!cui_edit_valid_utf8(value))
        return CUI_EDIT_INVALID_UTF8;
    if (value.length >= s->capacity)
        return CUI_EDIT_CAPACITY;
    if (value.length)
        memmove(s->buffer, value.data, value.length);
    s->buffer[value.length] = 0;
    if (s->composition && !s->composition_owned) {
        s->composition->active = false;
        s->composition->stale = true;
    }
    return CUI_EDIT_OK;
}
cui_otp_options cui_otp_options_default(void) {
    return (cui_otp_options){.groups = 2};
}
size_t cui_otp_groups(size_t length, size_t requested) {
    size_t max = length ? length : 1;
    return requested < 1 ? 1 : requested > max ? max : requested;
}
cui_id cui_otp_cell_id(cui_id id, size_t cell) {
    return id ^ (UINT64_C(0x954ab27d73c658f1) + cell);
}
static char cui_otp_private_digit(const char *text) {
    unsigned char c = (unsigned char)text[0];
    if (c >= '0' && c <= '9')
        return (char)c;
    if (c == 0xef && (unsigned char)text[1] == 0xbc && (unsigned char)text[2] >= 0x90 &&
        (unsigned char)text[2] <= 0x99)
        return (char)('0' + (unsigned char)text[2] - 0x90);
    return 0;
}
static uint64_t cui_otp_private_hash(cui_str value) {
    uint64_t h = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < value.length; i++)
        h = (h ^ (unsigned char)value.data[i]) * UINT64_C(1099511628211);
    return h;
}
static bool cui_otp_private_overlap(const void *a, size_t an, const void *b, size_t bn) {
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return an && bn && (x <= y ? y - x < an : x - y < bn);
}
static cui_otp_composition *cui_otp_private_session(cui_context *ui, cui_id id, cui_otp_state *s,
                                                    const cui_event *events, size_t count) {
    if (s->composition && !s->composition_owned)
        return s->composition;
    bool needed = s->composition_owned;
    for (size_t i = 0; i < count && !needed; i++)
        needed = cui_event_target(ui, i) == id && events[i].type >= CUI_EVENT_COMPOSITION_START &&
                 events[i].type <= CUI_EVENT_COMPOSITION_COMMIT;
    if (!needed)
        return NULL;
    typedef struct {
        cui_otp_composition session;
        char text[4096];
    } storage;
    storage *r =
        cui_resource_get(ui, id, UINT64_C(0xecbcb812017385bd), sizeof(storage), NULL, NULL);
    if (!r)
        return NULL;
    if (!r->session.buffer) {
        r->session.buffer = r->text;
        r->session.capacity = sizeof(r->text);
    }
    s->composition_owned = true;
    return &r->session;
}
static cui_edit_result cui_otp_private_composition(cui_otp_state *s, cui_event e,
                                                   cui_otp_result *out) {
    cui_otp_composition *c = s->composition;
    if (!c)
        return e.type == CUI_EVENT_CANCEL || e.type == CUI_EVENT_COMPOSITION_CANCEL
                   ? CUI_EDIT_OK
                   : CUI_EDIT_CAPACITY;
    if (!c->buffer || !c->capacity)
        return CUI_EDIT_CAPACITY;
    if (cui_otp_private_overlap(c->buffer, c->capacity, s->buffer, s->capacity) ||
        cui_otp_private_overlap(c->buffer, c->capacity, c, sizeof(*c)) ||
        cui_otp_private_overlap(c->buffer, c->capacity, s, sizeof(*s)))
        return CUI_EDIT_INVALID_ARGUMENT;
    if (e.type == CUI_EVENT_CANCEL || e.type == CUI_EVENT_COMPOSITION_CANCEL) {
        c->active = false;
        c->length = 0;
        c->buffer[0] = 0;
        return CUI_EDIT_OK;
    }
    size_t inline_length = 0;
    while (inline_length < sizeof(e.text) && e.text[inline_length])
        inline_length++;
    cui_str text =
        e.text_data ? (cui_str){e.text_data, e.text_length} : (cui_str){e.text, inline_length};
    if (!cui_edit_valid_utf8(text))
        return CUI_EDIT_INVALID_UTF8;
    if (e.composition_anchor > text.length || e.composition_cursor > text.length ||
        (e.composition_anchor < text.length &&
         ((unsigned char)text.data[e.composition_anchor] & 0xc0) == 0x80) ||
        (e.composition_cursor < text.length &&
         ((unsigned char)text.data[e.composition_cursor] & 0xc0) == 0x80))
        return CUI_EDIT_BOUNDARY;
    if (text.length >= c->capacity)
        return CUI_EDIT_CAPACITY;
    cui_str value = cui_otp_value(s);
    if (e.type == CUI_EVENT_COMPOSITION_COMMIT) {
        if (c->stale)
            return CUI_EDIT_OK;
        char digit = text.length && ((unsigned char)text.data[0] < 128 || text.length >= 3)
                         ? cui_otp_private_digit(text.data)
                         : 0;
        size_t filled = cui_otp_count(s);
        if (digit && filled < s->length) {
            if (value.length + 1 >= s->capacity)
                return CUI_EDIT_CAPACITY;
            s->buffer[value.length] = digit;
            s->buffer[value.length + 1] = 0;
            out->changed = true;
            out->change_count++;
            if (filled + 1 == s->length) {
                out->complete = true;
                out->complete_count++;
            }
        }
        c->active = false;
        c->stale = true;
        c->length = 0;
        c->buffer[0] = 0;
        return CUI_EDIT_OK;
    }
    if (text.length)
        memmove(c->buffer, text.data, text.length);
    c->buffer[text.length] = 0;
    c->length = text.length;
    c->anchor = e.composition_anchor;
    c->cursor = e.composition_cursor;
    c->document = s->buffer;
    c->document_length = value.length;
    c->document_hash = cui_otp_private_hash(value);
    c->active = true;
    c->stale = false;
    return CUI_EDIT_OK;
}
cui_otp_result cui_base_otp(cui_context *ui, cui_id id, cui_otp_state *s, cui_otp_options o) {
    cui_otp_result out = {.error = CUI_EDIT_OK};
    if (!ui || !id || !s || !s->buffer || !s->capacity || s->length > 256 ||
        !memchr(s->buffer, 0, s->capacity) || !cui_edit_valid_utf8(cui_otp_value(s)) ||
        (o.name.length && !o.name.data) || !isfinite(o.cell_size) || o.cell_size < 0 ||
        !isfinite(o.font_size) || o.font_size < 0 || !isfinite(o.gap) || o.gap < 0 ||
        !isfinite(o.group_gap) || o.group_gap < 0 || !isfinite(o.now_seconds) ||
        o.now_seconds < 0 || !isfinite(o.blink_interval) || o.blink_interval < 0 ||
        o.size < CUI_OTP_MEDIUM || o.size > CUI_OTP_LARGE) {
        out.error = CUI_EDIT_INVALID_ARGUMENT;
        return out;
    }
    (void)cui_interact(ui, id, CUI_ROLE_TEXTBOX, o.name, o.disabled, false,
                       (float)cui_otp_count(s));
    bool focused = cui_focused(ui) == id && !o.disabled;
    out.focused = focused && !s->focused;
    out.blurred = !focused && s->focused;
    s->focused = focused;
    if (out.focused)
        s->blink_epoch = o.now_seconds;
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    s->composition = cui_otp_private_session(ui, id, s, events, count);
    if (s->composition && s->composition->active) {
        cui_str value = cui_otp_value(s);
        if (!focused || s->composition->document != s->buffer ||
            s->composition->document_length != value.length ||
            s->composition->document_hash != cui_otp_private_hash(value)) {
            s->composition->stale = true;
            (void)cui_otp_private_composition(s, (cui_event){.type = CUI_EVENT_COMPOSITION_CANCEL},
                                              &out);
        }
    }
    for (size_t i = 0; i < count && !o.disabled; i++) {
        if (cui_event_target(ui, i) != id || cui_event_consumed(ui, i))
            continue;
        if (events[i].type == CUI_EVENT_KEY_DOWN && events[i].ime_composing)
            continue;
        if (events[i].type == CUI_EVENT_CANCEL ||
            (events[i].type >= CUI_EVENT_COMPOSITION_START &&
             events[i].type <= CUI_EVENT_COMPOSITION_CANCEL)) {
            cui_edit_result error = cui_otp_private_composition(s, events[i], &out);
            if (out.error == CUI_EDIT_OK)
                out.error = error;
            cui_consume_event(ui, i);
            s->blink_epoch = o.now_seconds;
            cui_invalidate(ui);
            continue;
        }
        if (s->composition && s->composition->active) {
            if (events[i].type == CUI_EVENT_KEY_DOWN || events[i].type == CUI_EVENT_TEXT)
                continue;
            if (events[i].type == CUI_EVENT_POINTER_DOWN) {
                s->composition->stale = true;
                (void)cui_otp_private_composition(
                    s, (cui_event){.type = CUI_EVENT_COMPOSITION_CANCEL}, &out);
            }
        }
        cui_str value = cui_otp_value(s);
        bool accepted = false;
        if (events[i].type == CUI_EVENT_KEY_DOWN && events[i].key == CUI_KEY_BACKSPACE) {
            size_t end = value.length;
            if (end) {
                end--;
                while (end && ((unsigned char)value.data[end] & 0xc0) == 0x80)
                    end--;
            }
            s->buffer[end] = 0;
            accepted = true;
        } else if (events[i].type == CUI_EVENT_TEXT &&
                   !(events[i].modifiers & (CUI_MOD_CTRL | CUI_MOD_META | CUI_MOD_ALT))) {
            const char *text = events[i].text_data ? events[i].text_data : events[i].text;
            size_t bytes = events[i].text_data ? events[i].text_length : strlen(events[i].text);
            char digit = bytes && ((unsigned char)text[0] < 128 || bytes >= 3)
                             ? cui_otp_private_digit(text)
                             : 0;
            if (digit && cui_otp_count(s) < s->length) {
                if (value.length + 1 >= s->capacity) {
                    out.error = CUI_EDIT_CAPACITY;
                    continue;
                }
                s->buffer[value.length] = digit;
                s->buffer[value.length + 1] = 0;
                accepted = true;
            }
        }
        if (accepted) {
            out.changed = true;
            out.change_count++;
            if (cui_otp_count(s) == s->length) {
                out.complete = true;
                out.complete_count++;
            }
            s->blink_epoch = o.now_seconds;
            cui_invalidate(ui);
        }
    }
    double interval = o.blink_interval > 0 ? o.blink_interval : .5;
    double elapsed = fmax(0, o.now_seconds - s->blink_epoch);
    double cycles = elapsed / interval;
    if (!isfinite(cycles)) {
        s->blink_epoch = o.now_seconds;
        cycles = 0;
    }
    double phase = floor(cycles);
    bool visible = focused && fmod(phase, 2) == 0;
    if (s->cursor_visible != visible)
        cui_invalidate(ui);
    s->cursor_visible = visible;
    if (focused && cui_otp_count(s) < s->length) {
        out.next_wake_seconds = s->blink_epoch + (phase + 1) * interval;
        if (!isfinite(out.next_wake_seconds) || out.next_wake_seconds <= o.now_seconds)
            out.next_wake_seconds = 0;
    }
    cui_update_semantic(ui, id, false, (float)cui_otp_count(s));
    cui_set_semantic_details(ui, id, (cui_semantic_details){.maximum = (float)s->length});
    return out;
}
