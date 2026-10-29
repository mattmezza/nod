/* SPDX-License-Identifier: MIT */
#include "cui/cui_picker.h"
#include "cui/cui_calendar_behavior.h"
#include "cui/cui_calendar_presentation.h"
#include "cui/cui_components.h"
#include "cui/cui_date_picker_calendar.h"
#include "cui/cui_icon.h"
#include "cui_picker_paint_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static cui_id cui_picker_style_id(cui_id id, unsigned index) {
    return id ^ (UINT64_C(0x713d9902fd4b5a97) + UINT64_C(0x9e3779b97f4a7c15) * index);
}
static unsigned cui_picker_style_months(cui_calendar_options o) {
    return o.months < 1 ? 1 : o.months > 3 ? 3 : o.months;
}
static float cui_picker_cell(cui_calendar_options o) {
    return isfinite(o.cell_size) && o.cell_size >= 20 ? fminf(o.cell_size, 128) : 32;
}
static bool cui_picker_style_selected(cui_date d, cui_date_selection s, bool range) {
    if (!cui_date_valid(s.start))
        return false;
    if (!range || !cui_date_valid(s.end))
        return !cui_date_compare(d, s.start);
    return cui_date_compare(d, s.start) >= 0 && cui_date_compare(d, s.end) <= 0;
}
static void cui_picker_paint_cell(cui_context *ui, cui_id id, const cui_theme *t, cui_str label,
                                  float size, bool disabled, bool selected, bool today,
                                  bool neighboring) {
    cui_begin_box(ui, id,
                  (cui_box){.width = size,
                            .height = size,
                            .align_x = CUI_ALIGN_CENTER,
                            .align_y = CUI_ALIGN_CENTER,
                            .radius = t->radius,
                            .background = selected ? t->primary
                                          : today  ? t->accent
                                                   : t->surface,
                            .border = cui_focused(ui) == id ? t->focus : t->border,
                            .border_width = cui_focused(ui) == id ? 2 : 0});
    cui_text(ui, label, fminf(t->font_size, size * .625f),
             disabled      ? t->muted
             : selected    ? t->on_primary
             : today       ? t->accent_foreground
             : neighboring ? t->muted
                           : t->text);
    cui_end_box(ui);
}
static void cui_picker_paint_navigation(cui_context *ui, cui_id id, const cui_theme *t, float size,
                                        bool disabled, bool previous, bool year) {
    cui_begin_box(ui, id,
                  (cui_box){.width = size,
                            .height = size,
                            .row = true,
                            .align_x = CUI_ALIGN_CENTER,
                            .align_y = CUI_ALIGN_CENTER,
                            .radius = t->radius,
                            .background = t->surface,
                            .border = cui_focused(ui) == id ? t->focus : t->border,
                            .border_width = cui_focused(ui) == id ? 2 : 0});
    for (unsigned i = 0; i < (year ? 2u : 1u); i++)
        cui_icon_named(ui, cui_picker_style_id(id, i), t,
                       cui_string(previous ? "chevron-left" : "chevron-right"),
                       (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                          .custom_size = size * (year ? .3f : .5f),
                                          .custom_color = true,
                                          .color = disabled ? t->muted : t->text});
    cui_end_box(ui);
}
static bool cui_picker_style_year_page(int page, cui_calendar_options o) {
    if (page < 1 || page > 9999)
        return false;
    for (unsigned year = (unsigned)page; year < (unsigned)page + 20 && year <= 9999; year++)
        if (cui_calendar_year_enabled(year, o))
            return true;
    return false;
}
static void cui_picker_paint_calendar_headers(cui_context *ui, cui_id id, const cui_theme *t,
                                              const cui_calendar_state *s, cui_calendar_options o,
                                              float cell) {
    const cui_date_locale *locale = o.locale;
    const char *month = locale ? locale->months[s->month.month - 1] : NULL;
    if (!month)
        month = cui_date_locale_builtin(CUI_DATE_LOCALE_EN_US)->months[s->month.month - 1];
    char year[16];
    snprintf(year, sizeof year, "%04d", s->month.year);
    for (unsigned part = 0; part < 2; part++) {
        cui_id header = part ? cui_calendar_navigation_id(id, 5) : cui_calendar_month_toggle_id(id);
        bool active = s->view == (part ? CUI_CALENDAR_YEARS : CUI_CALENDAR_MONTHS);
        cui_begin_box(ui, header,
                      (cui_box){.width = -1,
                                .height = cell,
                                .align_x = CUI_ALIGN_CENTER,
                                .align_y = CUI_ALIGN_CENTER,
                                .radius = t->radius,
                                .background = active ? t->accent : t->surface,
                                .border = t->focus,
                                .border_width = cui_focused(ui) == header ? 2 : 0});
        cui_text(ui, cui_string(part ? year : month), fminf(t->font_size, cell * .4f),
                 o.disabled ? t->muted
                 : active   ? t->accent_foreground
                            : t->text);
        cui_end_box(ui);
    }
}
static void cui_picker_paint_calendar(cui_context *ui, cui_id id, const cui_theme *t,
                                      cui_calendar_state *s, cui_date_selection *selection,
                                      cui_calendar_options o,
                                      cui_calendar_presentation_options presentation) {
    if (!t || !s || !selection)
        return;
    float cell = cui_picker_cell(o), gap = 2, width = cell * 7 + gap * 6;
    unsigned months = cui_picker_style_months(o);
    bool month_toggle = presentation.month_toggle && months == 1;
    const char *weekday[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    cui_begin_box(ui, id,
                  (cui_box){.row = true,
                            .gap = t->gap,
                            .padding = t->padding,
                            .radius = t->radius,
                            .background = t->surface});
    if (s->view != CUI_CALENDAR_DAYS) {
        bool years = s->view == CUI_CALENDAR_YEARS;
        cui_begin_box(ui, cui_picker_style_id(id, 800), (cui_box){.width = width, .gap = gap});
        cui_begin_box(
            ui, 0,
            (cui_box){.row = true, .width = -1, .height = cell, .align_y = CUI_ALIGN_CENTER});
        if (years)
            cui_picker_paint_navigation(
                ui, cui_calendar_navigation_id(id, 1), t, cell,
                o.disabled || !cui_picker_style_year_page((int)s->year_page - 20, o), true, true);
        if (month_toggle)
            cui_picker_paint_calendar_headers(ui, id, t, s, o, cell);
        else {
            char heading[40];
            unsigned last = s->year_page + 19;
            if (last > 9999)
                last = 9999;
            if (years)
                snprintf(heading, sizeof heading, "%u–%u", s->year_page, last);
            else
                snprintf(heading, sizeof heading, "%04d", s->month.year);
            cui_begin_box(ui, cui_calendar_navigation_id(id, 5),
                          (cui_box){.width = -1,
                                    .height = cell,
                                    .align_x = CUI_ALIGN_CENTER,
                                    .align_y = CUI_ALIGN_CENTER});
            cui_text(ui, cui_string(heading), t->font_size, t->text);
            cui_end_box(ui);
        }
        if (years)
            cui_picker_paint_navigation(
                ui, cui_calendar_navigation_id(id, 4), t, cell,
                o.disabled || !cui_picker_style_year_page((int)s->year_page + 20, o), false, true);
        cui_end_box(ui);
        unsigned first = years ? s->year_page : 1, total = years ? 20 : 12;
        for (unsigned row = 0; row < total / 4; row++) {
            cui_begin_box(ui, 0, (cui_box){.row = true, .gap = gap});
            for (unsigned col = 0; col < 4; col++) {
                unsigned item = first + row * 4 + col;
                cui_id child =
                    years ? cui_calendar_year_id(id, item) : cui_calendar_month_id(id, item);
                char number[16];
                snprintf(number, sizeof number, "%u", item);
                const cui_date_locale *locale =
                    o.locale ? o.locale : cui_date_locale_builtin(CUI_DATE_LOCALE_EN_US);
                const char *label = years ? number : locale->months[item - 1];
                if (!label)
                    label = cui_date_locale_builtin(CUI_DATE_LOCALE_EN_US)->months[item - 1];
                bool enabled = years ? cui_calendar_year_enabled(item, o)
                                     : cui_calendar_month_enabled((unsigned)s->month.year, item, o);
                cui_begin_box(ui, child,
                              (cui_box){.width = (width - gap * 3) / 4,
                                        .height = cell * 1.2f,
                                        .align_x = CUI_ALIGN_CENTER,
                                        .align_y = CUI_ALIGN_CENTER,
                                        .radius = t->radius,
                                        .background = t->surface,
                                        .border = cui_focused(ui) == child ? t->focus : t->border,
                                        .border_width = cui_focused(ui) == child ? 2 : 0});
                if (!years || item <= 9999)
                    cui_text(ui, cui_string(label), fminf(t->font_size, cell * .4f),
                             enabled ? t->text : t->muted);
                cui_end_box(ui);
            }
            cui_end_box(ui);
        }
        cui_end_box(ui);
        cui_end_box(ui);
        return;
    }
    for (unsigned m = 0; m < months; ++m) {
        cui_date month = cui_date_add_months(s->month, (int)m), grid[42];
        if (!cui_date_valid(month))
            continue;
        cui_calendar_grid(month, cui_calendar_first_weekday(o), grid);
        cui_begin_box(ui, cui_picker_style_id(id, 10 + m), (cui_box){.width = width, .gap = gap});
        cui_begin_box(
            ui, cui_picker_style_id(id, 20 + m),
            (cui_box){
                .row = true, .gap = gap, .width = -1, .height = cell, .align_y = CUI_ALIGN_CENTER});
        if (m == 0) {
            cui_picker_paint_navigation(ui, cui_calendar_navigation_id(id, 1), t, cell, o.disabled,
                                        true, true);
            cui_picker_paint_navigation(ui, cui_calendar_navigation_id(id, 2), t, cell, o.disabled,
                                        true, false);
        }
        if (month_toggle)
            cui_picker_paint_calendar_headers(ui, id, t, s, o, cell);
        else {
            char title[24];
            snprintf(title, sizeof(title), "%04d-%02d", month.year, month.month);
            cui_begin_box(
                ui, m == 0 ? cui_calendar_navigation_id(id, 5) : cui_picker_style_id(id, 40 + m),
                (cui_box){.width = -1,
                          .height = cell,
                          .align_x = CUI_ALIGN_CENTER,
                          .align_y = CUI_ALIGN_CENTER});
            if (o.locale) {
                const char *month_label = o.locale->months[month.month - 1];
                if (!month_label)
                    month_label =
                        cui_date_locale_builtin(CUI_DATE_LOCALE_EN_US)->months[month.month - 1];
                cui_text(ui, cui_string(month_label), fminf(t->font_size, cell * .4f), t->text);
                snprintf(title, sizeof title, "%04d", month.year);
                cui_text(ui, cui_string(title), fminf(t->font_size, cell * .4f), t->text);
            } else
                cui_text(ui, cui_string(title), fminf(t->font_size, cell * .625f), t->text);
            cui_end_box(ui);
        }
        if (m == 0) {
            cui_picker_paint_navigation(ui, cui_calendar_navigation_id(id, 3), t, cell, o.disabled,
                                        false, false);
            cui_picker_paint_navigation(ui, cui_calendar_navigation_id(id, 4), t, cell, o.disabled,
                                        false, true);
        }
        cui_end_box(ui);
        cui_begin_box(ui, cui_picker_style_id(id, 30 + m), (cui_box){.row = true, .gap = gap});
        for (unsigned w = 0; w < 7; ++w) {
            cui_begin_box(ui, cui_picker_style_id(id, 100 + m * 7 + w),
                          (cui_box){.width = cell,
                                    .height = cell * .7f,
                                    .align_x = CUI_ALIGN_CENTER,
                                    .align_y = CUI_ALIGN_CENTER});
            cui_text(
                ui,
                cui_string(o.locale && o.locale->weekdays[(cui_calendar_first_weekday(o) + w) % 7]
                               ? o.locale->weekdays[(cui_calendar_first_weekday(o) + w) % 7]
                               : weekday[(cui_calendar_first_weekday(o) + w) % 7]),
                t->font_size * .8f, t->muted);
            cui_end_box(ui);
        }
        cui_end_box(ui);
        for (unsigned row = 0; row < 6; ++row) {
            cui_begin_box(ui, cui_picker_style_id(id, 200 + m * 6 + row),
                          (cui_box){.row = true, .gap = gap});
            for (unsigned col = 0; col < 7; ++col) {
                unsigned index = row * 7 + col;
                cui_calendar_cell_info info = {0};
                bool visible =
                    presentation.neighboring_days
                        ? cui_calendar_cell_resolve(id, s->month, cui_calendar_first_weekday(o), m,
                                                    index, &info)
                        : cui_date_valid(grid[index]) && grid[index].month == month.month &&
                              grid[index].year == month.year;
                if (visible) {
                    char label[16];
                    snprintf(label, sizeof(label), "%d", grid[index].day);
                    cui_picker_paint_cell(
                        ui,
                        presentation.neighboring_days ? info.id
                                                      : cui_calendar_day_id(id, grid[index]),
                        t, cui_string(label), cell, !cui_calendar_date_enabled(grid[index], o),
                        cui_picker_style_selected(grid[index], *selection, o.range),
                        cui_date_valid(presentation.today) &&
                            !cui_date_compare(grid[index], presentation.today),
                        presentation.neighboring_days && info.neighboring);
                } else {
                    cui_begin_box(ui, cui_picker_style_id(id, 400 + m * 42 + index),
                                  (cui_box){.width = cell, .height = cell});
                    cui_end_box(ui);
                }
            }
            cui_end_box(ui);
        }
        cui_end_box(ui);
    }
    cui_end_box(ui);
}
cui_calendar_result cui_calendar(cui_context *ui, cui_id id, const cui_theme *t,
                                 cui_calendar_state *s, cui_date_selection *selection,
                                 cui_calendar_options o) {
    if (!t)
        return (cui_calendar_result){0};
    cui_calendar_result result = cui_base_calendar(ui, id, s, selection, o);
    cui_picker_paint_calendar(ui, id, t, s, selection, o, (cui_calendar_presentation_options){0});
    return result;
}
cui_calendar_result cui_calendar_ex(cui_context *ui, cui_id id, const cui_theme *t,
                                    cui_calendar_state *s, cui_date_selection *selection,
                                    cui_calendar_options o,
                                    cui_calendar_presentation_options presentation) {
    if (!t)
        return (cui_calendar_result){0};
    cui_calendar_result result = cui_base_calendar_ex(ui, id, s, selection, o, presentation);
    cui_picker_paint_calendar(ui, id, t, s, selection, o, presentation);
    return result;
}
cui_calendar_result cui_calendar_with_behavior(cui_context *ui, cui_id id, const cui_theme *t,
                                               cui_calendar_state *s, cui_date_selection *selection,
                                               cui_calendar_options o,
                                               cui_calendar_presentation_options presentation,
                                               cui_calendar_behavior_options behavior) {
    if (!t)
        return (cui_calendar_result){0};
    cui_calendar_result result =
        cui_base_calendar_with_behavior(ui, id, s, selection, o, presentation, behavior);
    cui_picker_paint_calendar(ui, id, t, s, selection, o, presentation);
    return result;
}
/* Private action facade paint seam; never reduces or submits semantics. */
void cui_picker_actions_paint(cui_context *ui, cui_id id, const cui_theme *t, cui_calendar_state *s,
                              cui_date_selection *value, cui_calendar_options o,
                              cui_calendar_presentation_options presentation) {
    cui_picker_paint_calendar(ui, id, t, s, value, o, presentation);
}
static bool cui_picker_date_picker_styled(cui_context *ui, cui_id id, const cui_theme *t,
                                          cui_str name, cui_date_picker_state *s,
                                          cui_date_selection *selection, cui_date_picker_options o,
                                          cui_date_picker_calendar_options extension) {
    if (!ui || !t || !s || !selection)
        return false;
    bool was_open = s->overlay.open;
    cui_calendar_result result = cui_base_date_picker_ex(ui, id, cui_picker_style_id(id, 900), name,
                                                         s, selection, o, extension);
    char value[200], end_value[96];
    cui_str label;
    if (cui_date_valid(selection->start)) {
        cui_date_format_locale(selection->start, o.calendar.locale, value, sizeof value);
        if (o.calendar.range && cui_date_valid(selection->end)) {
            cui_date_format_locale(selection->end, o.calendar.locale, end_value, sizeof end_value);
            size_t used = strlen(value);
            snprintf(value + used, sizeof value - used, " / %s", end_value);
        }
        label = cui_string(value);
    } else
        label = o.placeholder.length ? o.placeholder : cui_string("Select date");
    cui_button_paint(ui, id, t, label,
                     (cui_button_options){.variant = CUI_BUTTON_OUTLINE,
                                          .width = isfinite(o.width) && o.width > 0 ? o.width : 240,
                                          .disabled = o.disabled,
                                          .icon = cui_string("calendar")});
    if (!s->overlay.open)
        return result.changed;
    if (!isfinite(o.viewport.x) || !isfinite(o.viewport.y) || !isfinite(o.viewport.w) ||
        !isfinite(o.viewport.h) || o.viewport.w <= 0 || o.viewport.h <= 0) {
        cui_overlay_close(ui, &s->overlay);
        return false;
    }
    float cell = cui_picker_cell(o.calendar),
          width = (cell * 7 + 12) * cui_picker_style_months(o.calendar) + t->padding * 2 +
                  t->gap * (cui_picker_style_months(o.calendar) - 1);
    float height = cell * 7.7f + 14 + t->padding * 2;
    cui_popup_result placement = cui_popup_place(cui_bounds(ui, id), (cui_size){width, height},
                                                 o.viewport, CUI_POPUP_BELOW, 4);
    cui_overlay_set_bounds(&s->overlay, placement.bounds);
    if (!cui_overlay_begin(ui, &s->overlay))
        return false;
    cui_begin_portal(ui, s->overlay.id,
                     (cui_box){.background = t->surface,
                               .border = t->border,
                               .border_width = 1,
                               .radius = t->radius,
                               .align_x = CUI_ALIGN_CENTER,
                               .align_y = CUI_ALIGN_CENTER,
                               .clip = true},
                     placement.bounds, 100);
    cui_calendar_options calendar = o.calendar;
    calendar.disabled = calendar.disabled || o.disabled;
    cui_calendar_submit_ex(ui, cui_picker_style_id(id, 900), &s->calendar, *selection, calendar,
                           extension.presentation);
    cui_picker_paint_calendar(ui, cui_picker_style_id(id, 900), t, &s->calendar, selection,
                              calendar, extension.presentation);
    if (!was_open) {
        cui_id calendar_id = cui_picker_style_id(id, 900);
        cui_id focus = s->calendar.view == CUI_CALENDAR_YEARS
                           ? (cui_calendar_year_enabled(s->calendar.year_cursor, calendar)
                                  ? cui_calendar_year_id(calendar_id, s->calendar.year_cursor)
                                  : cui_calendar_navigation_id(calendar_id, 5))
                       : s->calendar.view == CUI_CALENDAR_MONTHS
                           ? cui_calendar_month_id(calendar_id, s->calendar.month_cursor)
                           : cui_calendar_day_id(calendar_id, s->calendar.cursor);
        cui_focus(ui, focus);
    }
    cui_end_box(ui);
    cui_overlay_end(ui);
    cui_set_semantic_details(
        ui, id, (cui_semantic_details){.expanded = s->overlay.open, .controls = s->overlay.id});
    return result.changed;
}
bool cui_date_picker_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                        cui_date_picker_state *s, cui_date_selection *selection,
                        cui_date_picker_options o, cui_date_picker_calendar_options extension) {
    return cui_picker_date_picker_styled(ui, id, t, name, s, selection, o, extension);
}
bool cui_date_picker(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                     cui_date_picker_state *s, cui_date_selection *selection,
                     cui_date_picker_options o) {
    return cui_picker_date_picker_styled(ui, id, t, name, s, selection, o,
                                         (cui_date_picker_calendar_options){0});
}
void cui_picker_time_paint(cui_context *ui, cui_id id, const cui_theme *t, const cui_time *time,
                           cui_time_options options) {
    if (!ui || !t || !time || !cui_time_valid(*time))
        return;
    cui_begin_box(ui, id,
                  (cui_box){.row = true,
                            .gap = 4,
                            .padding = t->padding,
                            .radius = t->radius,
                            .background = t->surface,
                            .border = t->border,
                            .border_width = 1});
    for (unsigned i = 0; i < 4; ++i) {
        if ((i == 2 && !options.seconds) || (i == 3 && !options.twelve_hour))
            continue;
        char text[4];
        unsigned value =
            i == 0   ? (options.twelve_hour ? (time->hour % 12 ? time->hour % 12 : 12) : time->hour)
            : i == 1 ? time->minute
                     : time->second;
        if (i == 3)
            snprintf(text, sizeof(text), "%s", time->hour >= 12 ? "PM" : "AM");
        else
            snprintf(text, sizeof(text), "%02u", value);
        cui_id segment = cui_time_segment_id(id, i);
        cui_begin_box(
            ui, segment,
            (cui_box){.width = t->font_size * 2,
                      .padding = 2,
                      .radius = t->radius,
                      .background = cui_focused(ui) == segment ? t->primary : t->surface});
        cui_text(ui, cui_string(text), t->font_size,
                 options.disabled             ? t->muted
                 : cui_focused(ui) == segment ? t->on_primary
                                              : t->text);
        cui_end_box(ui);
        if (i == 0 || (i == 1 && options.seconds))
            cui_text(ui, cui_string(":"), t->font_size, t->muted);
    }
    cui_end_box(ui);
}
bool cui_time_field(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                    cui_time_state *state, cui_time *time, cui_time_options options) {
    if (!t || !state || !time || !cui_time_valid(*time))
        return false;
    bool changed = cui_base_time_field(ui, id, name, state, time, options);
    cui_picker_time_paint(ui, id, t, time, options);
    return changed;
}
bool cui_color_picker(cui_context *ui, cui_id id, const cui_theme *t, cui_color_state *state,
                      cui_color *color, cui_color_picker_options options) {
    if (!t || !state || !color || (!options.palette && options.palette_count) ||
        options.palette_count > 64)
        return false;
    bool changed = cui_base_color_picker(ui, id, state, color, options);
    float width =
        isfinite(options.width) && options.width >= 120 ? fminf(options.width, 1000) : 240;
    cui_begin_box(
        ui, id,
        (cui_box){
            .gap = t->gap, .padding = t->padding, .radius = t->radius, .background = t->surface});
    cui_begin_box(ui, cui_color_part_id(id, 2), (cui_box){.width = width, .height = width * .6f});
    for (unsigned y = 0; y < 8; ++y) {
        cui_begin_box(ui, cui_picker_style_id(id, 1000 + y),
                      (cui_box){.row = true, .height = width * .6f / 8});
        for (unsigned x = 0; x < 12; ++x) {
            cui_hsv hsv = {state->hsv.hue, x / 11.f, 1 - y / 7.f, 1};
            bool marker = (unsigned)lroundf(state->hsv.saturation * 11) == x &&
                          (unsigned)lroundf((1 - state->hsv.value) * 7) == y;
            cui_begin_box(ui, cui_picker_style_id(id, 1100 + y * 12 + x),
                          (cui_box){.width = width / 12,
                                    .height = width * .6f / 8,
                                    .background = cui_color_from_hsv(hsv),
                                    .border = marker ? t->focus : (cui_color){0},
                                    .border_width = marker ? 2 : 0});
            cui_end_box(ui);
        }
        cui_end_box(ui);
    }
    cui_end_box(ui);
    cui_begin_box(ui, cui_color_part_id(id, 1),
                  (cui_box){.row = true, .width = width, .height = 20});
    for (unsigned x = 0; x < 24; ++x) {
        cui_begin_box(ui, cui_picker_style_id(id, 1300 + x),
                      (cui_box){.width = width / 24,
                                .height = 20,
                                .background = cui_color_from_hsv((cui_hsv){x * 15.f, 1, 1, 1})});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    if (options.alpha) {
        cui_begin_box(
            ui, cui_color_part_id(id, 3),
            (cui_box){.row = true, .width = width, .height = 20, .background = t->border});
        for (unsigned x = 0; x < 16; ++x) {
            cui_color c = *color;
            c.a = (uint8_t)(x * 17);
            cui_begin_box(ui, cui_picker_style_id(id, 1400 + x),
                          (cui_box){.width = width / 16, .height = 20, .background = c});
            cui_end_box(ui);
        }
        cui_end_box(ui);
    }
    const char *names[] = {"R", "G", "B", "A"};
    unsigned channels[] = {color->r, color->g, color->b, color->a};
    for (unsigned i = 0; i < (options.alpha ? 4u : 3u); ++i) {
        char label[24];
        snprintf(label, sizeof(label), "%s %u", names[i], channels[i]);
        cui_id part = cui_color_part_id(id, 20 + i);
        cui_begin_box(ui, part,
                      (cui_box){.width = width,
                                .height = 40,
                                .gap = 4,
                                .radius = t->radius,
                                .border = cui_focused(ui) == part ? t->focus : t->border,
                                .border_width = 1});
        cui_text(ui, cui_string(label), t->font_size, options.disabled ? t->muted : t->text);
        if (channels[i]) {
            cui_begin_box(ui, cui_picker_style_id(id, 1500 + i),
                          (cui_box){.width = width * channels[i] / 255.f,
                                    .height = 4,
                                    .background = t->primary});
            cui_end_box(ui);
        }
        cui_end_box(ui);
    }
    char hex[10];
    cui_color_format_hex(*color, options.alpha, hex, sizeof(hex));
    cui_begin_box(ui, cui_picker_style_id(id, 1600), (cui_box){.row = true, .gap = t->gap});
    cui_begin_box(ui, cui_picker_style_id(id, 1601),
                  (cui_box){.width = 24,
                            .height = 24,
                            .background = *color,
                            .border = t->border,
                            .border_width = 1});
    cui_end_box(ui);
    cui_text(ui, cui_string(hex), t->font_size, t->text);
    cui_end_box(ui);
    if (options.palette_count) {
        cui_begin_box(ui, cui_picker_style_id(id, 1700), (cui_box){.row = true, .gap = 4});
        for (size_t i = 0; i < options.palette_count; ++i) {
            cui_begin_box(ui, cui_color_part_id(id, 100 + (unsigned)i),
                          (cui_box){.width = 24,
                                    .height = 24,
                                    .background = options.palette[i],
                                    .border = t->border,
                                    .border_width = 1,
                                    .radius = t->radius});
            cui_end_box(ui);
        }
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return changed;
}
