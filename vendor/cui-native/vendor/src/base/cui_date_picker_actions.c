/* SPDX-License-Identifier: MIT */
#include "cui/cui_date_picker_actions.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
cui_calendar_result cui_picker_actions_reduce(cui_context *, cui_id, cui_id,
                                              cui_date_picker_state *, cui_date_selection *,
                                              cui_date_picker_options,
                                              cui_date_picker_calendar_options, bool *);
static bool cui_date_actions_absent(cui_date d) {
    return !d.year && !d.month && !d.day;
}
static bool cui_date_actions_same(cui_date_picker_value a, cui_date_picker_value b) {
    return a.shape == b.shape && !cui_date_compare(a.dates.start, b.dates.start) &&
           !cui_date_compare(a.dates.end, b.dates.end);
}
static bool cui_date_actions_value_valid(cui_date_picker_value v, bool complete) {
    if (v.shape != CUI_DATE_PICKER_SINGLE && v.shape != CUI_DATE_PICKER_RANGE)
        return false;
    bool start = cui_date_valid(v.dates.start), end = cui_date_valid(v.dates.end);
    if ((!start && !cui_date_actions_absent(v.dates.start)) ||
        (!end && !cui_date_actions_absent(v.dates.end)))
        return false;
    if (v.shape == CUI_DATE_PICKER_SINGLE)
        return !end && (!complete || start);
    return (!end || (start && cui_date_compare(v.dates.start, v.dates.end) <= 0)) &&
           (!complete || (start && end));
}
/* Bounded UTF-8 scalar validation, independent of Edit linkage. */
static bool cui_date_actions_label_valid(cui_str s) {
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
static bool cui_date_actions_active_clear(cui_date_picker_actions_options o) {
    return o.cleanable || o.delete_policy != CUI_DATE_PICKER_DELETE_OFF;
}
/* The existing part mapping is invertible modulo2^64. Verify decoded identities
 * through the authoritative public ID functions before treating them as reserved. */
static uint64_t cui_date_actions_part(cui_id id, cui_id root) {
    uint64_t step = UINT64_C(0x9e3779b97f4a7c15), inverse = 1;
    for (unsigned i = 0; i < 6; i++)
        inverse *= 2 - step * inverse;
    return ((id ^ root) - UINT64_C(0x19f617380ad34759)) * inverse;
}
static bool cui_date_actions_view_target(cui_id id, cui_id root,
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
    uint64_t encoded = cui_date_actions_part(id, root);
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
static bool cui_date_actions_collision(cui_id id, cui_id trigger, cui_id root,
                                       const cui_date_picker_actions_state *s,
                                       cui_date_picker_actions_options o) {
    if (!id || id == trigger || id == root || id == s->picker.overlay.id)
        return true;
    for (unsigned p = 1; p <= 5; p++)
        if (id == cui_calendar_navigation_id(root, p))
            return true;
    if (id == cui_calendar_month_toggle_id(root))
        return true;
    uint64_t encoded = cui_date_actions_part(id, root);
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
static bool cui_date_actions_preflight(cui_id trigger, cui_id root,
                                       const cui_date_picker_actions_state *s,
                                       cui_date_picker_value v, cui_date_picker_actions_options o) {
    if (!trigger || !root || !s || !s->picker.overlay.id || trigger == root ||
        trigger == s->picker.overlay.id || root == s->picker.overlay.id ||
        !cui_date_actions_value_valid(v, false) ||
        !cui_date_actions_value_valid(s->calendar_value, false) ||
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
        (!cui_date_actions_absent(min) && !cui_date_valid(min)) ||
        (!cui_date_actions_absent(max) && !cui_date_valid(max)) ||
        (cui_date_valid(min) && cui_date_valid(max) && cui_date_compare(min, max) > 0))
        return false;
    if (cui_date_actions_active_clear(o) &&
        (cui_date_actions_collision(o.clear_id, trigger, root, s, o) ||
         !cui_date_actions_label_valid(o.clear_label)))
        return false;
    for (size_t i = 0; i < o.preset_count; i++) {
        const cui_date_picker_preset *p = o.presets + i;
        if (!cui_date_actions_label_valid(p->label) ||
            !cui_date_actions_value_valid(p->value, true) ||
            cui_date_actions_collision(p->id, trigger, root, s, o) ||
            (cui_date_actions_active_clear(o) && p->id == o.clear_id))
            return false;
        for (size_t j = 0; j < i; j++)
            if (p->id == o.presets[j].id)
                return false;
    }
    return true;
}
void cui_date_picker_actions_init(cui_date_picker_actions_state *s, cui_id popup,
                                  cui_date initial) {
    if (!s)
        return;
    *s = (cui_date_picker_actions_state){0};
    cui_date_picker_init(&s->picker, popup, initial);
}
static bool cui_date_actions_enabled(cui_date_picker_value v, cui_calendar_options o) {
    o.disabled = false; /* Child configuration disables day input, not assigned values. */
    return (!cui_date_valid(v.dates.start) || cui_calendar_date_enabled(v.dates.start, o)) &&
           (!cui_date_valid(v.dates.end) || cui_calendar_date_enabled(v.dates.end, o));
}
static void cui_date_actions_synchronize(cui_date_picker_actions_state *s, cui_date_picker_value v,
                                         cui_calendar_options o) {
    s->observed_value = v;
    s->synchronized = true;
    if (!cui_date_actions_enabled(v, o))
        return;
    s->calendar_value = v;
    s->picker.calendar.selecting_end = v.shape == CUI_DATE_PICKER_RANGE &&
                                       cui_date_valid(v.dates.start) &&
                                       !cui_date_valid(v.dates.end);
    if (cui_date_valid(v.dates.start))
        s->picker.calendar.month = (cui_date){v.dates.start.year, v.dates.start.month, 1};
}
static void cui_date_actions_submit_trigger(cui_context *ui, cui_id trigger, cui_str name,
                                            cui_date_picker_actions_state *s,
                                            cui_date_picker_value v,
                                            cui_date_picker_actions_options o) {
    char text[200], end[96];
    if (o.picker.calendar.locale && cui_date_valid(v.dates.start)) {
        cui_date_format_locale(v.dates.start, o.picker.calendar.locale, text, sizeof text);
        if (v.shape == CUI_DATE_PICKER_RANGE && cui_date_valid(v.dates.end)) {
            cui_date_format_locale(v.dates.end, o.picker.calendar.locale, end, sizeof end);
            size_t n = strlen(text);
            snprintf(text + n, sizeof text - n, " / %s", end);
        }
        name = cui_string(text);
    }
    cui_interact(ui, trigger, CUI_ROLE_BUTTON, name, o.picker.disabled, false, 0);
    cui_set_semantic_details(ui, trigger,
                             (cui_semantic_details){.expanded = s->picker.overlay.open,
                                                    .controls = s->picker.overlay.id});
    if (o.cleanable && cui_date_valid(v.dates.start))
        cui_interact(ui, o.clear_id, CUI_ROLE_BUTTON, o.clear_label, o.picker.disabled, false, 0);
}
void cui_date_picker_actions_submit_presets(cui_context *ui, cui_date_picker_actions_state *s,
                                            cui_date_picker_actions_options o) {
    uint64_t frame = cui_frame_sequence(ui);
    if (s)
        s->submitted_frame = 0;
    if (!ui || !s || !frame || s->prepared_frame != frame || !s->picker.overlay.open ||
        cui_last_error(ui) != CUI_OK || o.preset_count > CUI_DATE_PICKER_PRESET_LIMIT ||
        (o.preset_count && !o.presets) ||
        !cui_date_actions_preflight(s->prepared_trigger, s->prepared_root, s, s->observed_value, o))
        return;
    for (size_t i = 0; i < o.preset_count; i++) {
        cui_interact(ui, o.presets[i].id, CUI_ROLE_BUTTON, o.presets[i].label, o.picker.disabled,
                     false, 0);
        cui_set_tab_stop(ui, o.presets[i].id, false); /* Frozen preset buttons are not Tab stops. */
    }
    if (cui_last_error(ui) != CUI_OK)
        return;
    cui_id targets[CUI_DATE_PICKER_TARGET_LIMIT];
    size_t target_count = 0;
    size_t semantic_count;
    const cui_semantic *sem = cui_semantics(ui, &semantic_count);
    for (size_t n = 0; n < semantic_count; n++) {
        if (sem[n].role != CUI_ROLE_BUTTON || sem[n].disabled || sem[n].details.no_focus)
            continue;
        bool preset = false;
        for (size_t i = 0; i < o.preset_count; i++)
            if (sem[n].id == o.presets[i].id)
                preset = true;
        if (!preset && !cui_date_actions_view_target(sem[n].id, s->prepared_root, s, o))
            continue;
        if (target_count == CUI_DATE_PICKER_TARGET_LIMIT)
            return;
        targets[target_count++] = sem[n].id;
    }
    memcpy(s->submitted_targets, targets, target_count * sizeof *targets);
    s->submitted_target_count = target_count;
    s->submitted_root = s->prepared_root;
    s->submitted_calendar = s->picker.calendar;
    s->submitted_open = true;
    s->submitted_count = o.preset_count;
    for (size_t i = 0; i < o.preset_count; i++)
        s->submitted_presets[i] = o.presets[i].id;
    s->submitted_months = o.picker.calendar.months;
    s->submitted_first_weekday = cui_calendar_first_weekday(o.picker.calendar);
    s->submitted_frame = frame;
}
cui_date_picker_actions_result cui_base_date_picker_actions(cui_context *ui, cui_id trigger,
                                                            cui_id root, cui_str name,
                                                            cui_date_picker_actions_state *s,
                                                            cui_date_picker_value *v,
                                                            cui_date_picker_actions_options o) {
    cui_date_picker_actions_result result = {0};
    if (!ui || !cui_frame_sequence(ui) || cui_last_error(ui) != CUI_OK || !v ||
        !cui_date_actions_preflight(trigger, root, s, *v, o) ||
        s->prepared_frame == cui_frame_sequence(ui))
        return result;
    result.valid = true;
    cui_date_picker_value before = *v;
    bool was_open = s->picker.overlay.open;
    bool prior_popup = was_open && s->submitted_open && s->submitted_root == root &&
                       s->submitted_frame + 1 == cui_frame_sequence(ui);
    bool prior_clear = s->submitted_clear && s->clear_frame + 1 == cui_frame_sequence(ui) &&
                       s->submitted_clear_id == o.clear_id;
    cui_date_picker_actions_state prior = *s;
    prior.picker.calendar = s->submitted_calendar;
    if (!s->synchronized || !cui_date_actions_same(s->observed_value, *v))
        cui_date_actions_synchronize(s, *v, o.picker.calendar);
    s->prepared_frame = cui_frame_sequence(ui);
    s->prepared_root = root;
    s->prepared_trigger = trigger;
    s->submitted_frame = 0;
    s->submitted_open = false;
    s->clear_frame = 0;
    cui_date_picker_options child = o.picker;
    child.calendar.range = s->calendar_value.shape == CUI_DATE_PICKER_RANGE;
    bool open_after_trigger = false;
    result.calendar =
        cui_picker_actions_reduce(ui, trigger, root, &s->picker, &s->calendar_value.dates, child,
                                  o.calendar, &open_after_trigger);
    if (!open_after_trigger)
        prior_popup = false;
    bool complete = s->calendar_value.shape == CUI_DATE_PICKER_SINGLE ||
                    cui_date_valid(s->calendar_value.dates.end);
    if (result.calendar.changed || (result.calendar.activated && complete))
        *v = s->calendar_value;
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t e = 0; e < count; e++)
        if (cui_event_activation(ui, e) == trigger ||
            (events[e].type == CUI_EVENT_KEY_DOWN && events[e].key == CUI_KEY_ESCAPE))
            prior_popup = false;
    if (!o.picker.disabled)
        for (size_t e = 0; e < count; e++) {
            if (cui_event_consumed(ui, e))
                continue;
            cui_id id = cui_event_activation(ui, e), target = cui_event_target(ui, e);
            bool prior_preset = false;
            for (size_t i = 0; i < prior.submitted_count && i < CUI_DATE_PICKER_PRESET_LIMIT; i++)
                if (target == prior.submitted_presets[i])
                    prior_preset = true;
            bool submitted_target = false, current_preset = false;
            for (size_t i = 0; i < prior.submitted_target_count && i < CUI_DATE_PICKER_TARGET_LIMIT;
                 i++)
                if (target == prior.submitted_targets[i])
                    submitted_target = true;
            for (size_t i = 0; i < o.preset_count; i++)
                if (target == o.presets[i].id)
                    current_preset = true;
            bool popup_target =
                prior_popup && submitted_target &&
                (current_preset || cui_date_actions_view_target(target, root, s, o));
            bool deletion =
                events[e].type == CUI_EVENT_KEY_DOWN && events[e].key == CUI_KEY_DELETE &&
                !events[e].modifiers && !events[e].repeat &&
                o.delete_policy != CUI_DATE_PICKER_DELETE_OFF &&
                (target == trigger ||
                 (o.delete_policy == CUI_DATE_PICKER_DELETE_SCOPE && was_open && popup_target));
            bool clear = deletion || (o.cleanable && prior_clear && id == o.clear_id);
            const cui_date_picker_preset *preset = NULL;
            if (prior_popup && prior_preset && submitted_target)
                for (size_t i = 0; i < o.preset_count; i++)
                    if (id == o.presets[i].id) {
                        preset = o.presets + i;
                        break;
                    }
            if (!clear && !preset)
                continue;
            if (preset && ((o.preset_policy != CUI_DATE_PICKER_PRESET_SOURCE &&
                            preset->value.shape != v->shape) ||
                           (o.preset_policy == CUI_DATE_PICKER_PRESET_MATCH_SHAPE_ENABLED &&
                            !cui_date_actions_enabled(preset->value, o.picker.calendar))))
                continue;
            if (clear) {
                *v = (cui_date_picker_value){.shape = v->shape};
                cui_date_actions_synchronize(s, *v, o.picker.calendar);
            } else {
                *v = preset->value;
                cui_date_actions_synchronize(s, *v, o.picker.calendar);
                cui_focus(ui, trigger);
            }
            /* Private withdrawal: clear must not invoke shared focus-restoring close. */
            s->picker.overlay.open = false;
            cui_invalidate(ui);
            cui_consume_event(ui, e);
            result.action_activated = true;
            break;
        }
    s->observed_value = *v;
    result.changed = !cui_date_actions_same(before, *v);
    cui_date_actions_submit_trigger(ui, trigger, name, s, *v, o);
    if (cui_last_error(ui) == CUI_OK && o.cleanable && cui_date_valid(v->dates.start)) {
        s->submitted_clear = true;
        s->submitted_clear_id = o.clear_id;
        s->clear_frame = cui_frame_sequence(ui);
    } else
        s->submitted_clear = false;
    return result;
}
