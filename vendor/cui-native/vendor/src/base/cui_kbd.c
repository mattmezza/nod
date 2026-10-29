/* SPDX-License-Identifier: MIT */
#include "cui/cui_kbd.h"
#include <string.h>
size_t cui_kbd_format_stroke(cui_kbd_stroke stroke, bool mac, char *output, size_t capacity) {
    if ((stroke.modifiers & ~(CUI_MOD_CTRL | CUI_MOD_ALT | CUI_MOD_SHIFT | CUI_MOD_META)) ||
        !stroke.key.data || !stroke.key.length || stroke.key.length > CUI_KBD_KEY_MAX_BYTES ||
        !cui_text_line_source_valid(
            (cui_text_line_source){.paragraph = stroke.key, .byte_end = stroke.key.length}))
        return SIZE_MAX;
    for (size_t i = 0; i < stroke.key.length; ++i) {
        unsigned char c = (unsigned char)stroke.key.data[i];
        if (c < 32 || c == 127 ||
            (i + 2 < stroke.key.length && c == 0xe2 &&
             (unsigned char)stroke.key.data[i + 1] == 0x80 &&
             (unsigned char)stroke.key.data[i + 2] == 0xa8))
            return SIZE_MAX;
    }
    static const struct {
        const char *key, *normal, *mac;
    } names[] = {{"ctrl", "Ctrl", "⌃"},
                 {"alt", "Alt", "⌥"},
                 {"shift", "Shift", "⇧"},
                 {"cmd", "Win", "⌘"},
                 {"space", "Space", "Space"},
                 {"backspace", "Backspace", "⌫"},
                 {"delete", "Delete", "⌫"},
                 {"escape", "Esc", "⎋"},
                 {"enter", "Enter", "⏎"},
                 {"pagedown", "Page Down", "Page Down"},
                 {"pageup", "Page Up", "Page Up"},
                 {"left", "Left", "←"},
                 {"right", "Right", "→"},
                 {"up", "Up", "↑"},
                 {"down", "Down", "↓"}};
    char key[CUI_KBD_KEY_MAX_BYTES + 1];
    memcpy(key, stroke.key.data, stroke.key.length);
    key[stroke.key.length] = 0;
    cui_str label = {key, stroke.key.length};
    if (key[0] >= 'a' && key[0] <= 'z')
        key[0] -= 'a' - 'A';
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i)
        if (stroke.key.length == strlen(names[i].key) &&
            !memcmp(stroke.key.data, names[i].key, stroke.key.length)) {
            label = cui_string(mac ? names[i].mac : names[i].normal);
            break;
        }
    return cui_kbd_format(stroke.modifiers, label, mac, output, capacity);
}
