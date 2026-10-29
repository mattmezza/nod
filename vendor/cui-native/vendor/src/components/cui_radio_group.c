/* SPDX-License-Identifier: MIT */
#include "cui/cui_radio_group.h"
#include <math.h>
static cui_radio_group_result
cui_radio_group_render(cui_context *ui, cui_id id, const cui_theme *t,
                       const cui_radio_group_item *items, size_t count, cui_id *selected,
                       cui_radio_group_state *state, cui_radio_group_options o,
                       cui_selection_handler handler, cui_selection_event_result *observed) {
    cui_radio_group_result r = {0};
    if (!t)
        return r;
    float density = t->font_size / 16;
    float size = (o.size == CUI_CONTROL_XSMALL  ? 12
                  : o.size == CUI_CONTROL_SMALL ? 14
                  : o.size == CUI_CONTROL_LARGE ? 18
                                                : 16) *
                 density;
    float font = (o.size == CUI_CONTROL_XSMALL  ? 12
                  : o.size == CUI_CONTROL_LARGE ? 16
                                                : 14) *
                 density;
    if (o.design_size != CUI_DESIGN_INHERIT) {
        cui_design_metrics design;
        if (!cui_design_measure(o.design_size, density, &design))
            return r;
        size = design.indicator_size;
        font = design.font_size;
    }
    if (observed) {
        cui_radio_group_event_result result =
            cui_base_radio_group_events(ui, id, items, count, selected, state, o, handler);
        *observed = result.events;
        r = result.legacy;
    } else
        r = cui_base_radio_group(ui, id, items, count, selected, state, o);
    if (!r.valid)
        return r;
    cui_begin_box(ui, id, (cui_box){.row = !o.vertical, .gap = o.gap > 0 ? o.gap : 12 * density});
    for (size_t i = 0; i < count; i++) {
        bool disabled = o.disabled || items[i].disabled, checked = *selected == items[i].id;
        cui_color ink = o.invalid ? t->danger : checked ? t->primary : t->input;
        cui_color background = checked ? t->primary : t->input_background, dot = t->on_primary;
        if (disabled) {
            ink.a /= 2;
            dot.a /= 2;
            if (checked)
                background.a /= 2;
        }
        cui_box row = {.row = true, .gap = 8 * density, .radius = t->radius * .5f};
        if (!o.hide_focus_ring && cui_focused(ui) == items[i].id) {
            row.border = t->focus;
            row.border_width = 2;
        }
        cui_begin_box(ui, items[i].id, row);
        cui_begin_box(ui, 0, (cui_box){.width = size});
        if (items[i].label.length) {
            cui_begin_box(ui, 0, (cui_box){.height = size * .125f});
            cui_end_box(ui);
        }
        cui_begin_box(ui, 0,
                      (cui_box){.width = size,
                                .height = size,
                                .radius = size / 2,
                                .border_width = 1,
                                .border = ink,
                                .background = background,
                                .align_x = CUI_ALIGN_CENTER,
                                .align_y = CUI_ALIGN_CENTER});
        if (checked)
            cui_checkmark(ui, size - 4 * density, dot);
        cui_end_box(ui);
        cui_end_box(ui);
        if (items[i].label.length)
            cui_text(ui, items[i].label, font, disabled ? t->muted : t->text);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return r;
}

cui_radio_group_result cui_radio_group(cui_context *ui, cui_id id, const cui_theme *theme,
                                       const cui_radio_group_item *items, size_t count,
                                       cui_id *selected, cui_radio_group_state *state,
                                       cui_radio_group_options options) {
    return cui_radio_group_render(ui, id, theme, items, count, selected, state, options,
                                  (cui_selection_handler){0}, NULL);
}
cui_radio_group_event_result cui_radio_group_events(cui_context *ui, cui_id id,
                                                    const cui_theme *theme,
                                                    const cui_radio_group_item *items, size_t count,
                                                    cui_id *selected, cui_radio_group_state *state,
                                                    cui_radio_group_options options,
                                                    cui_selection_handler handler) {
    cui_radio_group_event_result result;
    memset(&result, 0, sizeof(result));
    result.legacy = cui_radio_group_render(ui, id, theme, items, count, selected, state, options,
                                           handler, &result.events);
    return result;
}
