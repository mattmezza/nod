/* SPDX-License-Identifier: MIT */
#include "cui/cui_content.h"
#include "cui/cui_hover_events.h"
#include <math.h>
#include <string.h>
static size_t cui_content_scalar(const char *text, size_t length) {
    if (!length)
        return 0;
    const unsigned char *s = (const unsigned char *)text;
    size_t n = s[0] < 0x80                    ? 1
               : s[0] >= 0xc2 && s[0] <= 0xdf ? 2
               : s[0] >= 0xe0 && s[0] <= 0xef ? 3
               : s[0] >= 0xf0 && s[0] <= 0xf4 ? 4
                                              : 0;
    if (!n || n > length)
        return 0;
    for (size_t i = 1; i < n; ++i)
        if ((s[i] & 0xc0) != 0x80)
            return 0;
    if ((n == 3 && ((s[0] == 0xe0 && s[1] < 0xa0) || (s[0] == 0xed && s[1] >= 0xa0))) ||
        (n == 4 && ((s[0] == 0xf0 && s[1] < 0x90) || (s[0] == 0xf4 && s[1] >= 0x90))))
        return 0;
    return n;
}
size_t cui_avatar_initials(cui_str name, char output[9]) {
    if (!output)
        return 0;
    size_t first = SIZE_MAX, last = SIZE_MAX;
    bool word = false;
    if (name.data)
        for (size_t i = 0; i < name.length;) {
            unsigned char c = (unsigned char)name.data[i];
            if (c == ' ' || (c >= 9 && c <= 13)) {
                word = false;
                ++i;
                continue;
            }
            size_t n = cui_content_scalar(name.data + i, name.length - i);
            if (!n) {
                memcpy(output, "?", 2);
                return 1;
            }
            if (!word) {
                if (first == SIZE_MAX)
                    first = i;
                last = i;
                word = true;
            }
            i += n;
        }
    if (first == SIZE_MAX) {
        memcpy(output, "?", 2);
        return 1;
    }
    size_t total = cui_content_scalar(name.data + first, name.length - first);
    memcpy(output, name.data + first, total);
    if (last != first) {
        size_t n = cui_content_scalar(name.data + last, name.length - last);
        memcpy(output + total, name.data + last, n);
        total += n;
    }
    for (size_t i = 0; i < total; ++i)
        if (output[i] >= 'a' && output[i] <= 'z')
            output[i] -= 'a' - 'A';
    output[total] = 0;
    return total;
}
size_t cui_kbd_format(unsigned modifiers, cui_str key, bool mac, char *out, size_t capacity) {
    const char *normal[] = {"Ctrl", "Alt", "Shift", "Win"};
    const char *symbols[] = {"⌃", "⌥", "⇧", "⌘"};
    const unsigned bits[] = {CUI_MOD_CTRL, CUI_MOD_ALT, CUI_MOD_SHIFT, CUI_MOD_META};
    size_t length = 0;
    for (size_t i = 0; i < 4; ++i)
        if (modifiers & bits[i]) {
            const char *s = mac ? symbols[i] : normal[i];
            size_t n = strlen(s);
            for (size_t j = 0; j < n; ++j) {
                if (out && length + 1 < capacity)
                    out[length] = s[j];
                ++length;
            }
            if (!mac) {
                if (out && length + 1 < capacity)
                    out[length] = '+';
                ++length;
            }
        }
    if (key.data)
        for (size_t i = 0; i < key.length; ++i) {
            if (out && length + 1 < capacity)
                out[length] = key.data[i];
            ++length;
        }
    if (out && capacity) {
        size_t copied = length < capacity ? length : capacity - 1;
        out[copied] = 0;
        /* Byte limits must never expose half a UTF-8 modifier/key scalar. */
        for (size_t i = 0; i < copied;) {
            size_t scalar = cui_content_scalar(out + i, copied - i);
            if (!scalar) {
                out[i] = 0;
                break;
            }
            i += scalar;
        }
    }
    return length;
}
float cui_loading_phase(double seconds, float period) {
    if (!isfinite(seconds) || seconds < 0 || !isfinite(period) || period <= 0)
        return 0;
    return (float)(fmod(seconds, period) / period);
}
bool cui_hover_update(cui_hover_state *s, bool trigger, bool content, bool focus, bool escape,
                      bool disabled, float dt, float opening, float closing) {
    if (!s)
        return false;
    bool old = s->open;
    if (disabled || escape) {
        *s = (cui_hover_state){.suppressed = escape};
        return old;
    }
    if (s->suppressed) {
        if (trigger || focus || content)
            return false;
        s->suppressed = false;
    }
    bool target = trigger || focus || (s->open && content);
    if (target == s->open) {
        s->pending = false;
        s->elapsed = 0;
        s->target = target;
        return false;
    }
    if (!s->pending || s->target != target) {
        s->pending = true;
        s->target = target;
        s->elapsed = 0;
    }
    if (isfinite(dt) && dt > 0)
        s->elapsed += dt;
    float delay = target ? opening : closing;
    if (!isfinite(delay) || delay < 0)
        delay = 0;
    if (s->elapsed >= delay) {
        s->open = target;
        s->pending = false;
        s->elapsed = 0;
    }
    return old != s->open;
}

cui_hover_event_result cui_hover_update_events(cui_id target, cui_hover_state *state, bool trigger,
                                               bool content, bool focus, bool escape, bool disabled,
                                               float dt, float opening, float closing,
                                               cui_hover_handler handler) {
    cui_hover_event_result result;
    memset(&result, 0, sizeof(result));
    if (!target || !state)
        return result;
    bool previous = state->open;
    result.valid = true;
    result.control_result =
        cui_hover_update(state, trigger, content, focus, escape, disabled, dt, opening, closing);
    result.changed = previous != state->open;
    if (result.changed && handler.on_change) {
        cui_hover_lifecycle_event event;
        memset(&event, 0, sizeof(event));
        (void)cui_component_event_init(&event.event, CUI_COMPONENT_EVENT_LIFECYCLE, target,
                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
        event.previous_open = previous;
        event.open = state->open;
        result.change_count = 1;
        handler.on_change(handler.user, &event);
    }
    return result;
}
