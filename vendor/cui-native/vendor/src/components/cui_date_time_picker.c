/* SPDX-License-Identifier: MIT */
#include "cui/cui_date_time_picker.h"
#include "cui/cui_components.h"
#include "cui_picker_paint_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
cui_date_time_picker_result cui_date_time_picker(cui_context *ui, cui_id trigger,
                                                 const cui_theme *t, cui_str name,
                                                 cui_date_time_picker_state *s,
                                                 cui_date_time_value *v,
                                                 cui_date_time_picker_options o) {
    if (!t || !s)
        return (cui_date_time_picker_result){0};
    if (!isfinite(t->font_size) || t->font_size <= 0 || !isfinite(t->padding) || t->padding < 0 ||
        !isfinite(t->gap) || t->gap < 0 || !isfinite(t->radius) || t->radius < 0)
        return (cui_date_time_picker_result){0};
    float cell = o.picker.calendar.cell_size >= 20 ? fminf(o.picker.calendar.cell_size, 128) : 32;
    unsigned months = o.picker.calendar.months < 1   ? 1
                      : o.picker.calendar.months > 3 ? 3
                                                     : o.picker.calendar.months;
    float width = (cell * 7 + 12) * months + t->padding * 2 + t->gap * (months - 1);
    float row = 32 * t->font_size / 16;
    float segments = 2 + (o.precision == CUI_DATE_TIME_SECOND) + o.twelve_hour;
    float time_width = segments * (2 * t->font_size + 4) + (segments - 1) * 4 +
                       (o.precision == CUI_DATE_TIME_SECOND ? 2 : 1) * t->font_size +
                       2 * t->padding;
    width = fmaxf(width, time_width + 2 * t->padding);
    float height = cell * 7.7f + 14 + t->padding * 2 + o.preset_count * (row + t->gap) +
                   t->font_size * 2.5f + 2 * t->padding + 3 * t->gap;
    if ((!isfinite(width) || !isfinite(height) || width > o.picker.viewport.w ||
         height > o.picker.viewport.h))
        return (cui_date_time_picker_result){0};
    bool was_open = s->actions.picker.overlay.open;
    cui_id root = cui_date_picker_calendar_id(trigger);
    cui_date_time_picker_result r = cui_base_date_time_picker(
        ui, trigger, root, cui_date_time_picker_time_id(trigger), name, s, v, o);
    if (!r.valid)
        return r;
    char text[240], date[200], time_text[16];
    cui_str label;
    if (cui_date_valid(v->date)) {
        cui_date_format_locale(v->date, o.picker.calendar.locale, date, sizeof date);
        if (o.twelve_hour) {
            unsigned hour = v->time.hour % 12 ? v->time.hour % 12 : 12;
            if (o.precision == CUI_DATE_TIME_SECOND)
                snprintf(time_text, sizeof time_text, "%02u:%02u:%02u %s", hour, v->time.minute,
                         v->time.second, v->time.hour >= 12 ? "PM" : "AM");
            else
                snprintf(time_text, sizeof time_text, "%02u:%02u %s", hour, v->time.minute,
                         v->time.hour >= 12 ? "PM" : "AM");
        } else
            cui_time_format(v->time, o.precision == CUI_DATE_TIME_SECOND, time_text,
                            sizeof time_text);
        snprintf(text, sizeof text, "%s %s", date, time_text);
        label = cui_string(text);
    } else
        label = o.picker.placeholder.length ? o.picker.placeholder : cui_string("Select date/time");
    cui_button_paint(ui, trigger, t, label,
                     (cui_button_options){.variant = CUI_BUTTON_OUTLINE,
                                          .width = o.picker.width > 0 ? o.picker.width : 240,
                                          .disabled = o.picker.disabled,
                                          .icon = cui_string("calendar")});
    if (o.cleanable && cui_date_valid(v->date))
        cui_button_paint(
            ui, o.clear_id, t, o.clear_label,
            (cui_button_options){.variant = CUI_BUTTON_OUTLINE, .disabled = o.picker.disabled});
    if (!s->actions.picker.overlay.open)
        return r;
    cui_popup_result placement = cui_popup_place(cui_bounds(ui, trigger), (cui_size){width, height},
                                                 o.picker.viewport, CUI_POPUP_BELOW, 4);
    cui_overlay_set_bounds(&s->actions.picker.overlay, placement.bounds);
    if (!cui_overlay_begin(ui, &s->actions.picker.overlay))
        return r;
    cui_begin_portal(ui, s->actions.picker.overlay.id,
                     (cui_box){.background = t->surface,
                               .border = t->border,
                               .border_width = 1,
                               .radius = t->radius,
                               .align_x = CUI_ALIGN_CENTER,
                               .align_y = CUI_ALIGN_CENTER,
                               .clip = true},
                     placement.bounds, 100);
    cui_calendar_options child = o.picker.calendar;
    child.range = false;
    child.disabled = child.disabled || o.picker.disabled;
    cui_calendar_submit_ex(ui, root, &s->actions.picker.calendar, s->actions.calendar_value.dates,
                           child, o.calendar.presentation);
    cui_picker_actions_paint(ui, root, t, &s->actions.picker.calendar,
                             &s->actions.calendar_value.dates, child, o.calendar.presentation);
    cui_text(ui, o.time_label.length ? o.time_label : cui_string("Time"), t->font_size, t->muted);
    cui_date_time_picker_submit_time(ui, cui_date_time_picker_time_id(trigger), s, v, o);
    cui_picker_time_paint(ui, cui_date_time_picker_time_id(trigger), t, &v->time,
                          (cui_time_options){.seconds = o.precision == CUI_DATE_TIME_SECOND,
                                             .twelve_hour = o.twelve_hour,
                                             .disabled = o.picker.disabled});
    cui_date_time_picker_submit_presets(ui, s, o);
    for (size_t i = 0; i < o.preset_count; i++)
        cui_button_paint(ui, o.presets[i].id, t, o.presets[i].label,
                         (cui_button_options){.variant = CUI_BUTTON_GHOST,
                                              .width = width - 2 * t->padding,
                                              .disabled = o.picker.disabled});
    if (!was_open) {
        cui_id focus =
            s->actions.picker.calendar.view == CUI_CALENDAR_YEARS
                ? (cui_calendar_year_enabled(s->actions.picker.calendar.year_cursor, child)
                       ? cui_calendar_year_id(root, s->actions.picker.calendar.year_cursor)
                       : cui_calendar_navigation_id(root, 5))
            : s->actions.picker.calendar.view == CUI_CALENDAR_MONTHS
                ? cui_calendar_month_id(root, s->actions.picker.calendar.month_cursor)
                : cui_calendar_day_id(root, s->actions.picker.calendar.cursor);
        cui_focus(ui, focus);
    }
    cui_end_box(ui);
    cui_overlay_end(ui);
    return r;
}
