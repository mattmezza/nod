/* SPDX-License-Identifier: MIT */
#include "cui/cui_kbd.h"
#include <math.h>
bool cui_kbd_ex(cui_context *ui, cui_id id, const cui_theme *theme, cui_kbd_stroke stroke,
                cui_kbd_options options) {
    char text[CUI_KBD_TEXT_CAPACITY];
    if (!ui || !id || !theme || !cui_frame_is_submitting(ui) || !isfinite(theme->font_size) ||
        theme->font_size <= 0 || theme->font_size > 512 || !isfinite(theme->radius) ||
        theme->radius < 0 || options.accessible_name.length > 1024 ||
        !cui_text_line_source_valid((cui_text_line_source){
            .paragraph = options.accessible_name, .byte_end = options.accessible_name.length}) ||
        cui_kbd_format_stroke(stroke, options.mac_symbols, text, sizeof text) == SIZE_MAX)
        return false;
    for (size_t i = 0; i < options.accessible_name.length; ++i) {
        unsigned char c = (unsigned char)options.accessible_name.data[i];
        if (c < 32 || c == 127 ||
            (i + 2 < options.accessible_name.length && c == 0xe2 &&
             (unsigned char)options.accessible_name.data[i + 1] == 0x80 &&
             (unsigned char)options.accessible_name.data[i + 2] == 0xa8))
            return false;
    }
    cui_str formatted = cui_string(text);
    float scale = theme->font_size / 16;
    float font = options.plain ? theme->font_size : 12 * scale;
    if (!(font > 0) || !isfinite(font))
        return false;
    float border = !options.plain && options.outline ? 1 : 0;
    cui_size measured = cui_measure_text(ui, formatted, font);
    if (!isfinite(measured.width) || measured.width < 0 || !isfinite(measured.height) ||
        measured.height < 0)
        return false;
    cui_box box = {0};
    if (!options.plain)
        box = (cui_box){.width = fmaxf(20 * scale, measured.width + 8 * scale + 2 * border),
                        .height = 16 * scale + 2 * border,
                        .fixed_width = true,
                        .fixed_height = true,
                        .align_x = CUI_ALIGN_CENTER,
                        .align_y = CUI_ALIGN_CENTER,
                        .background = options.outline ? theme->background : theme->muted_background,
                        .border = theme->border,
                        .border_width = border,
                        .radius = theme->radius * .5f};
    cui_begin_box(ui, id, box);
    cui_interact(ui, id, CUI_ROLE_LABEL,
                 options.accessible_name.length ? options.accessible_name : formatted, false, false,
                 0);
    cui_set_tab_stop(ui, id, false);
    cui_text_with_line_height(ui, formatted, font, options.plain ? theme->text : theme->muted,
                              options.plain ? 0 : font);
    cui_end_box(ui);
    return true;
}
