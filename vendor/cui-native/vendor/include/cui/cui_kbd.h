/* SPDX-License-Identifier: MIT */
#ifndef CUI_KBD_H
#define CUI_KBD_H
#include "cui_content.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { CUI_KBD_KEY_MAX_BYTES = 128, CUI_KBD_TEXT_CAPACITY = 160 };
/* One stroke, not a sequence. Modifiers are CTRL/ALT/SHIFT/META bits. The host
 * maps parser/platform aliases; key names use frozen lower-case spellings. */
typedef struct {
    unsigned modifiers;
    cui_str key;
} cui_kbd_stroke;
/* Returns required bytes excluding NUL, or SIZE_MAX for invalid input. Nonempty
 * key must be strict single-line UTF-8 <=128 bytes, without ASCII controls.
 * Invalid input leaves output untouched. NULL output queries required bytes.
 * Partial output ends on a complete scalar and is NUL terminated when capacity>0.
 * Common special names and first ASCII letter uppercase match frozen source;
 * non-ASCII case conversion is not implemented. Required count is bounded147
 * bytes, so arithmetic cannot overflow; writes never exceed capacity. Legacy
 * cui_kbd_format is unchanged. Output may overlap the input key buffer. */
size_t cui_kbd_format_stroke(cui_kbd_stroke, bool mac_symbols, char *output, size_t capacity);
typedef struct {
    bool plain, outline, mac_symbols; /* plain ignores outline; zero is normal pill. */
    /* Optional strict single-line UTF-8, max1024 bytes; no NUL/ASCII controls. */
    cui_str accessible_name;
} cui_kbd_options;
/* Decorative, noninteractive shortcut label; no bindings or activation. All
 * invalid arguments fail before submission. One copied label semantic defaults
 * to formatted text. Source-shaped pill uses theme font_size/16 rem scaling:
 * X4/Y2 padding, min width20, text12, half radius; outline adds 1-unit border.
 * Theme font must be finite(0,512], radius finite>=0.
 * Plain uses theme text/font without pill geometry. Inputs are copied into Core
 * frame storage on submission; no retained state or resources. Compose several
 * calls in a normal parent row for grouped hints; no sequence/binding lookup. */
bool cui_kbd_ex(cui_context *, cui_id, const cui_theme *, cui_kbd_stroke, cui_kbd_options);
#ifdef __cplusplus
}
#endif
#endif
