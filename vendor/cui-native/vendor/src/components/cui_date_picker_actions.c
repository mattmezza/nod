/* SPDX-License-Identifier: MIT */
#include "cui/cui_date_picker_actions.h"
#include "cui/cui_components.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
void cui_picker_actions_paint(cui_context *, cui_id, const cui_theme *, cui_calendar_state *,
                              cui_date_selection *, cui_calendar_options,
                              cui_calendar_presentation_options);
cui_date_picker_actions_result cui_date_picker_actions(cui_context *ui, cui_id trigger,
                                                       const cui_theme *t, cui_str name,
                                                       cui_date_picker_actions_state *s,
                                                       cui_date_picker_value *v,
                                                       cui_date_picker_actions_options o) {
    if (!t || !s)
        return (cui_date_picker_actions_result){0};
    if (!isfinite(t->font_size) || t->font_size <= 0 || !isfinite(t->padding) || t->padding < 0 ||
        !isfinite(t->gap) || t->gap < 0 || !isfinite(t->radius) || t->radius < 0)
        return (cui_date_picker_actions_result){0};
    float cell = o.picker.calendar.cell_size >= 20 ? fminf(o.picker.calendar.cell_size, 128) : 32;
    unsigned months = o.picker.calendar.months < 1   ? 1
                      : o.picker.calendar.months > 3 ? 3
                                                     : o.picker.calendar.months;
    float width = (cell * 7 + 12) * months + t->padding * 2 + t->gap * (months - 1);
    float row = 32 * t->font_size / 16;
    float height = cell * 7.7f + 14 + t->padding * 2 + o.preset_count * (row + t->gap);
    if (o.preset_count && (!isfinite(width) || !isfinite(height) || width > o.picker.viewport.w ||
                           height > o.picker.viewport.h))
        return (cui_date_picker_actions_result){0};
    bool was_open = s->picker.overlay.open;
    cui_id root = cui_date_picker_calendar_id(trigger);
    cui_date_picker_actions_result r =
        cui_base_date_picker_actions(ui, trigger, root, name, s, v, o);
    if (!r.valid)
        return r;
    char text[200], end[96];
    cui_str label;
    if (cui_date_valid(v->dates.start)) {
        cui_date_format_locale(v->dates.start, o.picker.calendar.locale, text, sizeof text);
        if (v->shape == CUI_DATE_PICKER_RANGE && cui_date_valid(v->dates.end)) {
            cui_date_format_locale(v->dates.end, o.picker.calendar.locale, end, sizeof end);
            size_t n = strlen(text);
            snprintf(text + n, sizeof text - n, " / %s", end);
        }
        label = cui_string(text);
    } else
        label = o.picker.placeholder.length ? o.picker.placeholder : cui_string("Select date");
    cui_button_paint(ui, trigger, t, label,
                     (cui_button_options){.variant = CUI_BUTTON_OUTLINE,
                                          .width = o.picker.width > 0 ? o.picker.width : 240,
                                          .disabled = o.picker.disabled,
                                          .icon = cui_string("calendar")});
    if (o.cleanable && cui_date_valid(v->dates.start))
        cui_button_paint(
            ui, o.clear_id, t, o.clear_label,
            (cui_button_options){.variant = CUI_BUTTON_OUTLINE, .disabled = o.picker.disabled});
    if (!s->picker.overlay.open)
        return r;
    cui_popup_result placement = cui_popup_place(cui_bounds(ui, trigger), (cui_size){width, height},
                                                 o.picker.viewport, CUI_POPUP_BELOW, 4);
    cui_overlay_set_bounds(&s->picker.overlay, placement.bounds);
    if (!cui_overlay_begin(ui, &s->picker.overlay))
        return r;
    cui_begin_portal(ui, s->picker.overlay.id,
                     (cui_box){.background = t->surface,
                               .border = t->border,
                               .border_width = 1,
                               .radius = t->radius,
                               .align_x = CUI_ALIGN_CENTER,
                               .align_y = CUI_ALIGN_CENTER,
                               .clip = true},
                     placement.bounds, 100);
    cui_calendar_options child = o.picker.calendar;
    child.range = s->calendar_value.shape == CUI_DATE_PICKER_RANGE;
    child.disabled = child.disabled || o.picker.disabled;
    cui_calendar_submit_ex(ui, root, &s->picker.calendar, s->calendar_value.dates, child,
                           o.calendar.presentation);
    cui_picker_actions_paint(ui, root, t, &s->picker.calendar, &s->calendar_value.dates, child,
                             o.calendar.presentation);
    cui_date_picker_actions_submit_presets(ui, s, o);
    for (size_t i = 0; i < o.preset_count; i++)
        cui_button_paint(ui, o.presets[i].id, t, o.presets[i].label,
                         (cui_button_options){.variant = CUI_BUTTON_GHOST,
                                              .width = width - 2 * t->padding,
                                              .disabled = o.picker.disabled});
    if (!was_open) {
        cui_id focus = s->picker.calendar.view == CUI_CALENDAR_YEARS
                           ? (cui_calendar_year_enabled(s->picker.calendar.year_cursor, child)
                                  ? cui_calendar_year_id(root, s->picker.calendar.year_cursor)
                                  : cui_calendar_navigation_id(root, 5))
                       : s->picker.calendar.view == CUI_CALENDAR_MONTHS
                           ? cui_calendar_month_id(root, s->picker.calendar.month_cursor)
                           : cui_calendar_day_id(root, s->picker.calendar.cursor);
        cui_focus(ui, focus);
    }
    cui_end_box(ui);
    cui_overlay_end(ui);
    return r;
}
