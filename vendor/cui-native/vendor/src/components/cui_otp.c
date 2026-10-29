/* SPDX-License-Identifier: MIT */
#include "cui/cui_otp.h"
#include "cui/cui_otp_events.h"
#include <math.h>
static cui_otp_result cui_otp_common(cui_context *ui, cui_id id, const cui_theme *t,
                                     cui_otp_state *s, cui_otp_options o,
                                     const cui_otp_event_handler *handler, size_t *change_count) {
    if (!t || o.design_size < CUI_DESIGN_INHERIT || o.design_size > CUI_DESIGN_9XL)
        return (cui_otp_result){.error = CUI_EDIT_INVALID_ARGUMENT};
    cui_design_metrics design = {0};
    if (o.design_size != CUI_DESIGN_INHERIT) {
        if (!cui_design_measure(o.design_size, t->font_size / 16, &design))
            return (cui_otp_result){.error = CUI_EDIT_INVALID_ARGUMENT};
        if (o.cell_size == 0)
            o.cell_size = design.control_height;
        if (o.font_size == 0)
            o.font_size = design.font_size;
        if (o.gap == 0)
            o.gap = design.gap;
        if (o.group_gap == 0)
            o.group_gap = design.gap * 3;
    }
    cui_otp_result out;
    if (handler) {
        cui_otp_event_result result = cui_base_otp_events(ui, id, s, o, *handler);
        out = result.control;
        *change_count = result.change_count;
    } else
        out = cui_base_otp(ui, id, s, o);
    if (out.error == CUI_EDIT_INVALID_ARGUMENT)
        return out;
    float size = o.cell_size > 0                                       ? o.cell_size
                 : o.size == CUI_OTP_LARGE                             ? 44
                 : o.size == CUI_OTP_SMALL || o.size == CUI_OTP_XSMALL ? 24
                                                                       : 32;
    float font = o.font_size > 0 ? o.font_size : o.cell_size > 0 ? size * .5f : t->font_size;
    size_t groups = cui_otp_groups(s->length, o.groups);
    size_t per_group = s->length ? (s->length + groups - 1) / groups : 1;
    cui_str value = cui_otp_value(s);
    size_t offset = 0, cell = 0, filled = cui_otp_count(s);
    cui_otp_composition *composition = s->composition;
    bool composing = composition && composition->active;
    size_t preedit_length = 0, preedit_offset = 0;
    if (composing && composition->length) {
        unsigned char first = (unsigned char)composition->buffer[0];
        bool digit = first >= '0' && first <= '9';
        bool fullwidth = composition->length >= 3 && first == 0xef &&
                         (unsigned char)composition->buffer[1] == 0xbc &&
                         (unsigned char)composition->buffer[2] >= 0x90 &&
                         (unsigned char)composition->buffer[2] <= 0x99;
        preedit_length = digit ? 1 : fullwidth ? 3 : 0;
    }
    size_t cursor_count = preedit_length && composition->cursor ? 1 : 0;
    size_t cursor = filled + cursor_count;
    cursor = s->length ? (cursor < s->length ? cursor : s->length - 1) : 0;
    cui_begin_box(ui, id,
                  (cui_box){.row = true,
                            .gap = o.group_gap > 0 ? o.group_gap : 20,
                            .align_y = CUI_ALIGN_CENTER});
    for (size_t group = 0; group < groups; group++) {
        cui_begin_box(
            ui, id ^ (UINT64_C(0x2351ad67c85ae802) + group),
            (cui_box){.row = true, .gap = o.gap > 0 ? o.gap : 4, .align_y = CUI_ALIGN_CENTER});
        for (size_t j = 0; j < per_group && cell < s->length; j++, cell++) {
            bool focus = s->focused && cell == cursor && !o.hide_focus_ring;
            cui_begin_box(ui, cui_otp_cell_id(id, cell),
                          (cui_box){.width = size,
                                    .height = size,
                                    .row = true,
                                    .align_x = CUI_ALIGN_CENTER,
                                    .align_y = CUI_ALIGN_CENTER,
                                    .radius = t->radius,
                                    .border_width = focus ? 2 : 1,
                                    .border = focus ? t->focus : t->border,
                                    .background = o.disabled ? t->background : t->surface});
            if (offset < value.length) {
                size_t end = offset + 1;
                while (end < value.length && ((unsigned char)value.data[end] & 0xc0) == 0x80)
                    end++;
                cui_text(ui,
                         s->masked ? cui_string("*") : (cui_str){value.data + offset, end - offset},
                         font, o.disabled ? t->muted : t->text);
                offset = end;
            } else if (composing && preedit_offset < preedit_length) {
                size_t end = preedit_offset + 1;
                while (end < preedit_length &&
                       ((unsigned char)composition->buffer[end] & 0xc0) == 0x80)
                    end++;
                cui_text(ui,
                         s->masked ? cui_string("*")
                                   : (cui_str){composition->buffer + preedit_offset,
                                               end - preedit_offset},
                         font, t->text);
                size_t first = composition->anchor < composition->cursor ? composition->anchor
                                                                         : composition->cursor;
                size_t last = composition->anchor > composition->cursor ? composition->anchor
                                                                        : composition->cursor;
                cui_set_underlay(ui, (cui_rect){2, size - 3, size - 4, 1}, t->caret,
                                 (cui_corner_radii){0});
                if (preedit_offset < last && end > first)
                    cui_set_underlay(ui, (cui_rect){2, 2, size - 4, size - 4}, t->selection,
                                     (cui_corner_radii){0});
                preedit_offset = end;
            } else if (s->focused && cell == cursor && s->cursor_visible) {
                cui_begin_box(
                    ui, 0,
                    (cui_box){.width = 3,
                              .height = fminf(o.design_size == CUI_DESIGN_INHERIT ? 16 : font,
                                              size * .5f),
                              .background = t->text});
                cui_end_box(ui);
            }
            cui_end_box(ui);
        }
        cui_end_box(ui);
    }
    cui_end_box(ui);
    if (s->focused && !o.disabled && s->length) {
        cui_rect cell_bounds = cui_bounds(ui, cui_otp_cell_id(id, cursor));
        cui_set_text_input_rect(ui, id,
                                (cui_rect){cell_bounds.x + cell_bounds.w * .5f,
                                           cell_bounds.y + cell_bounds.h * .25f, 1,
                                           cell_bounds.h * .5f});
    }
    return out;
}
cui_otp_result cui_otp(cui_context *ui, cui_id id, const cui_theme *theme, cui_otp_state *state,
                       cui_otp_options options) {
    return cui_otp_common(ui, id, theme, state, options, NULL, NULL);
}
cui_otp_event_result cui_otp_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                    cui_otp_state *state, cui_otp_options options,
                                    cui_otp_event_handler handler) {
    cui_otp_event_result result = {0};
    result.control = cui_otp_common(ui, id, theme, state, options, &handler, &result.change_count);
    return result;
}
