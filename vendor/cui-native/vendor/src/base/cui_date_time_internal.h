/* SPDX-License-Identifier: MIT */
#ifndef CUI_DATE_TIME_INTERNAL_H
#define CUI_DATE_TIME_INTERNAL_H
#include "cui/cui_date_picker_actions.h"
#include <math.h>
static bool cui_datetime_internal_absent(cui_date d) {
    return !d.year && !d.month && !d.day;
}
static bool cui_datetime_internal_value_valid(cui_date_picker_value v, bool complete) {
    if (v.shape != CUI_DATE_PICKER_SINGLE && v.shape != CUI_DATE_PICKER_RANGE)
        return false;
    bool start = cui_date_valid(v.dates.start), end = cui_date_valid(v.dates.end);
    if ((!start && !cui_datetime_internal_absent(v.dates.start)) ||
        (!end && !cui_datetime_internal_absent(v.dates.end)))
        return false;
    if (v.shape == CUI_DATE_PICKER_SINGLE)
        return !end && (!complete || start);
    return (!end || (start && cui_date_compare(v.dates.start, v.dates.end) <= 0)) &&
           (!complete || (start && end));
}
/* Bounded UTF-8 scalar validation, independent of Edit linkage. */
static bool cui_datetime_internal_label_valid(cui_str s) {
    if (!s.data || !s.length || s.length > CUI_DATE_PICKER_LABEL_LIMIT)
        return false;
    for (size_t i = 0; i < s.length;) {
        unsigned c = (unsigned char)s.data[i++], n = 0, minimum = 0;
        if (!c)
            return false;
        if (c < 128)
            continue;
        if (c >= 194 && c <= 223) {
            n = 1;
            c &= 31;
            minimum = 128;
        } else if (c >= 224 && c <= 239) {
            n = 2;
            c &= 15;
            minimum = 2048;
        } else if (c >= 240 && c <= 244) {
            n = 3;
            c &= 7;
            minimum = 65536;
        } else
            return false;
        if (n > s.length - i)
            return false;
        while (n--) {
            unsigned b = (unsigned char)s.data[i++];
            if ((b & 192) != 128)
                return false;
            c = (c << 6) | (b & 63);
        }
        if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            return false;
    }
    return true;
}
static bool cui_datetime_internal_active_clear(cui_date_picker_actions_options o) {
    return o.cleanable || o.delete_policy != CUI_DATE_PICKER_DELETE_OFF;
}
/* The existing part mapping is invertible modulo2^64. Verify decoded identities
 * through the authoritative public ID functions before treating them as reserved. */
static uint64_t cui_datetime_internal_part(cui_id id, cui_id root) {
    uint64_t step = UINT64_C(0x9e3779b97f4a7c15), inverse = 1;
    for (unsigned i = 0; i < 6; i++)
        inverse *= 2 - step * inverse;
    return ((id ^ root) - UINT64_C(0x19f617380ad34759)) * inverse;
}
static bool cui_datetime_internal_view_target(cui_id id, cui_id root,
                                              const cui_date_picker_actions_state *s,
                                              cui_date_picker_actions_options o) {
    if (id == s->picker.overlay.id)
        return true; /* Actual caller-owned scope binding. */
    if (id == cui_calendar_navigation_id(root, 5))
        return true;
    unsigned months = o.picker.calendar.months < 1   ? 1
                      : o.picker.calendar.months > 3 ? 3
                                                     : o.picker.calendar.months;
    if (id == cui_calendar_month_toggle_id(root))
        return o.calendar.presentation.month_toggle && months == 1;
    for (unsigned p = 1; p <= 4; p++)
        if (id == cui_calendar_navigation_id(root, p))
            return s->picker.calendar.view == CUI_CALENDAR_DAYS ||
                   (s->picker.calendar.view == CUI_CALENDAR_YEARS && (p == 1 || p == 4));
    uint64_t encoded = cui_datetime_internal_part(id, root);
    if (s->picker.calendar.view == CUI_CALENDAR_YEARS)
        return encoded > UINT64_C(200000000) && encoded <= UINT64_C(200009999) &&
               encoded - UINT64_C(200000000) >= s->picker.calendar.year_page &&
               encoded - UINT64_C(200000000) < (uint64_t)s->picker.calendar.year_page + 20 &&
               id == cui_calendar_year_id(root, (unsigned)(encoded - UINT64_C(200000000)));
    if (s->picker.calendar.view == CUI_CALENDAR_MONTHS)
        return encoded > UINT64_C(300000000) && encoded <= UINT64_C(300000012) &&
               id == cui_calendar_month_id(root, (unsigned)(encoded - UINT64_C(300000000)));
    for (unsigned panel = 0; panel < months; panel++)
        for (unsigned cell = 0; cell < 42; cell++) {
            cui_calendar_cell_info info;
            if (!cui_calendar_cell_resolve(root, s->picker.calendar.month,
                                           cui_calendar_first_weekday(o.picker.calendar), panel,
                                           cell, &info))
                continue;
            if ((o.calendar.presentation.neighboring_days || !info.neighboring) && id == info.id)
                return true;
        }
    return false;
}
static bool cui_datetime_internal_collision(cui_id id, cui_id trigger, cui_id root,
                                            const cui_date_picker_actions_state *s,
                                            cui_date_picker_actions_options o) {
    if (!id || id == trigger || id == root || id == s->picker.overlay.id)
        return true;
    for (unsigned p = 1; p <= 5; p++)
        if (id == cui_calendar_navigation_id(root, p))
            return true;
    if (id == cui_calendar_month_toggle_id(root))
        return true;
    uint64_t encoded = cui_datetime_internal_part(id, root);
    if (encoded >= 100 && encoded < UINT64_C(100000000)) {
        uint64_t ymd = encoded - 100;
        cui_date date = {(int)(ymd / 10000), (int)(ymd / 100 % 100), (int)(ymd % 100)};
        if (cui_date_valid(date) && id == cui_calendar_day_id(root, date))
            return true;
    }
    if (encoded > UINT64_C(200000000) && encoded <= UINT64_C(200009999) &&
        id == cui_calendar_year_id(root, (unsigned)(encoded - UINT64_C(200000000))))
        return true;
    if (encoded > UINT64_C(300000000) && encoded <= UINT64_C(300000012) &&
        id == cui_calendar_month_id(root, (unsigned)(encoded - UINT64_C(300000000))))
        return true;
    cui_date month = s->picker.calendar.month;
    unsigned months = o.picker.calendar.months < 1   ? 1
                      : o.picker.calendar.months > 3 ? 3
                                                     : o.picker.calendar.months;
    for (unsigned panel = 0; panel < months; panel++)
        for (unsigned cell = 0; cell < 42; cell++) {
            cui_calendar_cell_info info;
            if (cui_calendar_cell_resolve(root, month,
                                          cui_calendar_first_weekday(o.picker.calendar), panel,
                                          cell, &info) &&
                (id == info.id || id == cui_calendar_day_id(root, info.date)))
                return true;
        }
    return false;
}
static bool cui_datetime_internal_preflight(cui_id trigger, cui_id root,
                                            const cui_date_picker_actions_state *s,
                                            cui_date_picker_value v,
                                            cui_date_picker_actions_options o) {
    if (!trigger || !root || !s || !s->picker.overlay.id || trigger == root ||
        trigger == s->picker.overlay.id || root == s->picker.overlay.id ||
        !cui_datetime_internal_value_valid(v, false) ||
        !cui_datetime_internal_value_valid(s->calendar_value, false) ||
        o.preset_count > CUI_DATE_PICKER_PRESET_LIMIT || (o.preset_count && !o.presets) ||
        o.preset_policy < CUI_DATE_PICKER_PRESET_SOURCE ||
        o.preset_policy > CUI_DATE_PICKER_PRESET_MATCH_SHAPE_ENABLED ||
        o.delete_policy < CUI_DATE_PICKER_DELETE_OFF ||
        o.delete_policy > CUI_DATE_PICKER_DELETE_SCOPE)
        return false;
    cui_date min = o.picker.calendar.minimum, max = o.picker.calendar.maximum;
    cui_rect r = o.picker.viewport;
    if (!isfinite(r.x) || !isfinite(r.y) || !isfinite(r.w) || !isfinite(r.h) || r.w <= 0 ||
        r.h <= 0 || !isfinite(o.picker.width) || o.picker.width < 0 ||
        !isfinite(o.picker.calendar.cell_size) || o.picker.calendar.cell_size < 0 ||
        (!cui_datetime_internal_absent(min) && !cui_date_valid(min)) ||
        (!cui_datetime_internal_absent(max) && !cui_date_valid(max)) ||
        (cui_date_valid(min) && cui_date_valid(max) && cui_date_compare(min, max) > 0))
        return false;
    if (cui_datetime_internal_active_clear(o) &&
        (cui_datetime_internal_collision(o.clear_id, trigger, root, s, o) ||
         !cui_datetime_internal_label_valid(o.clear_label)))
        return false;
    for (size_t i = 0; i < o.preset_count; i++) {
        const cui_date_picker_preset *p = o.presets + i;
        if (!cui_datetime_internal_label_valid(p->label) ||
            !cui_datetime_internal_value_valid(p->value, true) ||
            cui_datetime_internal_collision(p->id, trigger, root, s, o) ||
            (cui_datetime_internal_active_clear(o) && p->id == o.clear_id))
            return false;
        for (size_t j = 0; j < i; j++)
            if (p->id == o.presets[j].id)
                return false;
    }
    return true;
}
#endif
