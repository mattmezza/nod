/* SPDX-License-Identifier: MIT */
#include "cui/cui_date_time_picker.h"
#include "cui_date_time_internal.h"
#include "cui_picker_composition_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    cui_date date;
    bool changed, confirmed;
} cui_datetime_acceptance;
static cui_picker_accept_flow cui_datetime_accept(void *context, size_t event_index,
                                                  cui_date_selection accepted) {
    (void)event_index;
    cui_datetime_acceptance *a = context;
    if (!cui_date_compare(a->date, accepted.start)) {
        a->confirmed = true;
        return CUI_PICKER_ACCEPT_STOP;
    }
    a->date = accepted.start;
    a->changed = true;
    return CUI_PICKER_ACCEPT_CONTINUE;
}
static bool cui_datetime_time_same(cui_time a, cui_time b) {
    return a.hour == b.hour && a.minute == b.minute && a.second == b.second;
}
static bool cui_datetime_same(cui_date_time_value a, cui_date_time_value b) {
    return !cui_date_compare(a.date, b.date) && cui_datetime_time_same(a.time, b.time);
}
static bool cui_datetime_value_valid(cui_date_time_value v) {
    return (cui_datetime_internal_absent(v.date) || cui_date_valid(v.date)) &&
           cui_time_valid(v.time);
}
static cui_time cui_datetime_truncate(cui_time t, cui_date_time_precision precision) {
    if (precision == CUI_DATE_TIME_MINUTE)
        t.second = 0;
    return t;
}
static cui_date_picker_actions_options cui_datetime_options(cui_date_time_picker_options o,
                                                            cui_date_picker_preset *p) {
    for (size_t i = 0; i < o.preset_count && i < CUI_DATE_PICKER_PRESET_LIMIT; i++)
        p[i] = (cui_date_picker_preset){.id = o.presets[i].id,
                                        .label = o.presets[i].label,
                                        .value = {.dates = {.start = o.presets[i].date}}};
    return (cui_date_picker_actions_options){.picker = o.picker,
                                             .calendar = o.calendar,
                                             .presets = p,
                                             .preset_count = o.preset_count,
                                             .clear_id = o.clear_id,
                                             .clear_label = o.clear_label,
                                             .cleanable = o.cleanable,
                                             .delete_policy = o.delete_policy,
                                             .preset_policy = o.preset_policy};
}
static bool cui_datetime_preflight(cui_id trigger, cui_id calendar, cui_id time,
                                   const cui_date_time_picker_state *s, cui_date_time_value v,
                                   cui_date_time_picker_options o) {
    if (!s || !cui_datetime_value_valid(v) || !cui_time_valid(o.default_time) ||
        o.precision < CUI_DATE_TIME_MINUTE || o.precision > CUI_DATE_TIME_SECOND ||
        o.picker.calendar.range || o.preset_count > CUI_DATE_PICKER_PRESET_LIMIT ||
        (o.preset_count && !o.presets) ||
        (o.time_label.length && !cui_datetime_internal_label_valid(o.time_label)))
        return false;
    if (o.calendar.behavior.range_transition < CUI_CALENDAR_RANGE_LEGACY ||
        o.calendar.behavior.range_transition > CUI_CALENDAR_RANGE_RESTART_EARLIER ||
        o.calendar.behavior.year_destination < CUI_CALENDAR_YEAR_OPEN_MONTHS ||
        o.calendar.behavior.year_destination > CUI_CALENDAR_YEAR_OPEN_DAYS ||
        o.calendar.behavior.recenter < CUI_CALENDAR_RECENTER_LEGACY ||
        o.calendar.behavior.recenter > CUI_CALENDAR_RECENTER_SELECTION_START ||
        o.calendar.behavior.range_validation < CUI_CALENDAR_VALIDATE_ACTIVATED_DATE ||
        o.calendar.behavior.range_validation > CUI_CALENDAR_VALIDATE_RANGE_ENDPOINTS)
        return false;
    for (size_t i = 0; i < o.preset_count; i++)
        if (o.presets[i].has_time && !cui_time_valid(o.presets[i].time))
            return false;
    cui_date_picker_preset presets[CUI_DATE_PICKER_PRESET_LIMIT];
    cui_date_picker_actions_options a = cui_datetime_options(o, presets);
    if (!cui_datetime_internal_preflight(trigger, calendar, &s->actions,
                                         (cui_date_picker_value){.dates = {.start = v.date}}, a))
        return false;
    cui_id ids[5] = {time, cui_time_segment_id(time, 0), cui_time_segment_id(time, 1),
                     cui_time_segment_id(time, 2), cui_time_segment_id(time, 3)};
    for (unsigned i = 0; i < 5; i++) {
        if (cui_datetime_internal_collision(ids[i], trigger, calendar, &s->actions, a))
            return false;
        if ((o.cleanable || o.delete_policy != CUI_DATE_PICKER_DELETE_OFF) && ids[i] == o.clear_id)
            return false;
        for (unsigned j = 0; j < i; j++)
            if (ids[i] == ids[j])
                return false;
        for (size_t j = 0; j < o.preset_count; j++)
            if (ids[i] == o.presets[j].id)
                return false;
    }
    return true;
}
static bool cui_datetime_enabled(cui_date d, cui_calendar_options o) {
    o.disabled = false;
    return !cui_date_valid(d) || cui_calendar_date_enabled(d, o);
}
static void cui_datetime_sync_date(cui_date_time_picker_state *s, cui_date d,
                                   cui_calendar_options o) {
    s->actions.observed_value = (cui_date_picker_value){.dates = {.start = d}};
    s->actions.synchronized = true;
    if (!cui_datetime_enabled(d, o))
        return;
    s->actions.calendar_value = s->actions.observed_value;
    s->actions.picker.calendar.selecting_end = false;
    if (cui_date_valid(d))
        s->actions.picker.calendar.month = (cui_date){d.year, d.month, 1};
}
void cui_date_time_picker_init(cui_date_time_picker_state *s, cui_id popup, cui_date initial) {
    if (!s)
        return;
    *s = (cui_date_time_picker_state){0};
    cui_date_picker_actions_init(&s->actions, popup, initial);
}
bool cui_date_time_picker_assign(cui_date_time_picker_state *s, cui_date_time_value *v,
                                 cui_date_time_value next) {
    if (!s || !s->actions.picker.overlay.id || !v || !cui_datetime_value_valid(next))
        return false;
    *v = next;
    s->assignment_pending = true;
    return true;
}
cui_id cui_date_time_picker_time_id(cui_id trigger) {
    return trigger ? trigger ^ UINT64_C(0x8276a9df821994af) : 0;
}
static bool cui_datetime_time_target(cui_id target, cui_id time, cui_date_time_picker_options o) {
    for (unsigned i = 0; i < 4; i++)
        if ((i != 2 || o.precision == CUI_DATE_TIME_SECOND) && (i != 3 || o.twelve_hour) &&
            target == cui_time_segment_id(time, i))
            return true;
    return false;
}
static cui_time_options cui_datetime_time_options(cui_date_time_picker_options o) {
    return (cui_time_options){.seconds = o.precision == CUI_DATE_TIME_SECOND,
                              .twelve_hour = o.twelve_hour,
                              .disabled = o.picker.disabled};
}
void cui_date_time_picker_submit_time(cui_context *ui, cui_id time, cui_date_time_picker_state *s,
                                      const cui_date_time_value *v,
                                      cui_date_time_picker_options o) {
    if (!s)
        return;
    s->time_submitted_frame = 0;
    if (!ui || !v || s->actions.prepared_frame != cui_frame_sequence(ui) ||
        !s->actions.picker.overlay.open || time != s->time_id || cui_last_error(ui) != CUI_OK ||
        !cui_datetime_preflight(s->actions.prepared_trigger, s->actions.prepared_root, time, s, *v,
                                o))
        return;
    cui_time t = v->time;
    cui_picker_time_submit(ui, time, o.time_label.length ? o.time_label : cui_string("Time"),
                           &s->time_field, &t, cui_datetime_time_options(o));
    if (cui_last_error(ui) == CUI_OK)
        s->time_submitted_frame = cui_frame_sequence(ui);
}
void cui_date_time_picker_submit_presets(cui_context *ui, cui_date_time_picker_state *s,
                                         cui_date_time_picker_options o) {
    if (!s)
        return;
    s->actions.submitted_frame = 0;
    uint64_t frame = cui_frame_sequence(ui);
    if (!ui || !frame || frame != s->actions.prepared_frame || !s->actions.picker.overlay.open ||
        cui_last_error(ui) != CUI_OK ||
        !cui_datetime_preflight(s->actions.prepared_trigger, s->actions.prepared_root, s->time_id,
                                s, s->observed, o))
        return;
    for (size_t i = 0; i < o.preset_count; i++) {
        cui_interact(ui, o.presets[i].id, CUI_ROLE_BUTTON, o.presets[i].label, o.picker.disabled,
                     false, 0);
        cui_set_tab_stop(ui, o.presets[i].id, false);
    }
    if (cui_last_error(ui) != CUI_OK)
        return;
    cui_id targets[CUI_DATE_PICKER_TARGET_LIMIT];
    size_t n = 0, count;
    const cui_semantic *sem = cui_semantics(ui, &count);
    cui_date_picker_preset presets[CUI_DATE_PICKER_PRESET_LIMIT];
    cui_date_picker_actions_options a = cui_datetime_options(o, presets);
    for (size_t i = 0; i < count; i++) {
        if (sem[i].disabled || sem[i].details.no_focus)
            continue;
        bool accepted = false;
        if (sem[i].role == CUI_ROLE_BUTTON) {
            accepted = cui_datetime_internal_view_target(sem[i].id, s->actions.prepared_root,
                                                         &s->actions, a);
            for (size_t j = 0; j < o.preset_count; j++)
                if (sem[i].id == o.presets[j].id)
                    accepted = true;
        }
        if (sem[i].role == CUI_ROLE_TEXTBOX && s->time_submitted_frame == frame &&
            cui_datetime_time_target(sem[i].id, s->time_id, o))
            accepted = true;
        if (!accepted)
            continue;
        if (n == CUI_DATE_PICKER_TARGET_LIMIT)
            return;
        targets[n++] = sem[i].id;
    }
    memcpy(s->actions.submitted_targets, targets, n * sizeof *targets);
    s->actions.submitted_target_count = n;
    s->actions.submitted_count = o.preset_count;
    for (size_t i = 0; i < o.preset_count; i++)
        s->actions.submitted_presets[i] = o.presets[i].id;
    s->actions.submitted_root = s->actions.prepared_root;
    s->actions.submitted_calendar = s->actions.picker.calendar;
    s->actions.submitted_open = true;
    s->actions.submitted_frame = frame;
}
cui_date_time_picker_result cui_base_date_time_picker(cui_context *ui, cui_id trigger,
                                                      cui_id calendar, cui_id time, cui_str name,
                                                      cui_date_time_picker_state *s,
                                                      cui_date_time_value *v,
                                                      cui_date_time_picker_options o) {
    cui_date_time_picker_result r = {0};
    uint64_t frame = cui_frame_sequence(ui);
    if (!ui || !frame || !v || cui_last_error(ui) != CUI_OK ||
        !cui_datetime_preflight(trigger, calendar, time, s, *v, o) ||
        s->actions.prepared_frame == frame)
        return r;
    r.valid = true;
    cui_date_time_value before = *v;
    cui_date_time_picker_state prior = *s;
    bool prior_popup = s->actions.picker.overlay.open && prior.actions.submitted_open &&
                       prior.actions.submitted_root == calendar &&
                       prior.actions.submitted_frame + 1 == frame;
    bool prior_clear = prior.actions.submitted_clear && prior.actions.clear_frame + 1 == frame &&
                       prior.actions.submitted_clear_id == o.clear_id;
    bool external = s->assignment_pending || (s->configured && !cui_datetime_same(s->observed, *v));
    cui_time def = cui_datetime_truncate(o.default_time, o.precision);
    bool reset = !s->configured || s->applied_precision != o.precision ||
                 s->applied_twelve_hour != o.twelve_hour ||
                 !cui_datetime_time_same(s->applied_default, def);
    if (s->configured && !cui_datetime_time_same(s->applied_default, def))
        v->time = def;
    if (!s->configured || external) {
        if (external)
            *v = before;
        if (!cui_date_valid(v->date))
            v->time = def;
        cui_datetime_sync_date(s, v->date, o.picker.calendar);
        reset = true;
    }
    v->time = cui_datetime_truncate(v->time, o.precision);
    if (reset)
        s->time_field = (cui_time_state){0};
    if (external) {
        s->actions.picker.overlay.open = false;
        prior_popup = prior_clear = false;
        cui_invalidate(ui);
    }
    s->configured = true;
    s->assignment_pending = false;
    s->applied_default = def;
    s->applied_precision = o.precision;
    s->applied_twelve_hour = o.twelve_hour;
    s->time_id = time;
    s->time_submitted_frame = 0;
    s->actions.prepared_frame = frame;
    s->actions.prepared_root = calendar;
    s->actions.prepared_trigger = trigger;
    s->actions.submitted_frame = 0;
    s->actions.clear_frame = 0;
    s->actions.submitted_open = false;
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t e = 0; e < count; e++)
        if (cui_event_activation(ui, e) == trigger ||
            (events[e].type == CUI_EVENT_KEY_DOWN && events[e].key == CUI_KEY_ESCAPE))
            prior_popup = false;
    bool open_after = false;
    cui_datetime_acceptance acceptance = {.date = v->date};
    r.calendar = cui_picker_datetime_reduce(ui, trigger, calendar, &s->actions.picker,
                                            &s->actions.calendar_value.dates, o.picker, o.calendar,
                                            &open_after, cui_datetime_accept, &acceptance);
    if (!open_after)
        prior_popup = false;
    if (r.calendar.activated) {
        v->date = acceptance.date;
        r.source_change = acceptance.changed;
        r.confirmed = acceptance.confirmed;
        if (r.confirmed)
            cui_overlay_close(ui, &s->actions.picker.overlay);
    } else if (r.calendar.changed)
        v->date = s->actions.calendar_value.dates.start;
    if (s->actions.picker.overlay.open) {
        cui_time_options t = cui_datetime_time_options(o);
        t.readonly =
            !prior_popup || prior.time_id != time || prior.time_submitted_frame + 1 != frame;
        r.time_edited = cui_picker_time_reduce(ui, time, o.time_label, &s->time_field, &v->time, t);
        if (r.time_edited && cui_date_valid(v->date))
            r.source_change = true;
    }
    cui_date_picker_preset converted[CUI_DATE_PICKER_PRESET_LIMIT];
    cui_date_picker_actions_options a = cui_datetime_options(o, converted);
    if (!o.picker.disabled)
        for (size_t e = 0; e < count; e++) {
            if (cui_event_consumed(ui, e))
                continue;
            cui_id id = cui_event_activation(ui, e), target = cui_event_target(ui, e);
            bool previous = false, current = false, was_preset = false;
            for (size_t j = 0;
                 j < prior.actions.submitted_target_count && j < CUI_DATE_PICKER_TARGET_LIMIT; j++)
                if (target == prior.actions.submitted_targets[j])
                    previous = true;
            for (size_t j = 0;
                 j < prior.actions.submitted_count && j < CUI_DATE_PICKER_PRESET_LIMIT; j++)
                if (target == prior.actions.submitted_presets[j])
                    was_preset = true;
            for (size_t j = 0; j < o.preset_count; j++)
                if (target == o.presets[j].id)
                    current = true;
            bool scope =
                prior_popup && previous &&
                (current || cui_datetime_internal_view_target(target, calendar, &s->actions, a) ||
                 (prior.time_id == time && prior.time_submitted_frame + 1 == frame &&
                  cui_datetime_time_target(target, time, o)));
            bool del =
                events[e].type == CUI_EVENT_KEY_DOWN && events[e].key == CUI_KEY_DELETE &&
                !events[e].modifiers && !events[e].repeat &&
                o.delete_policy != CUI_DATE_PICKER_DELETE_OFF &&
                (target == trigger || (o.delete_policy == CUI_DATE_PICKER_DELETE_SCOPE && scope));
            bool clear = del || (o.cleanable && prior_clear && id == o.clear_id);
            const cui_date_time_preset *p = NULL;
            if (prior_popup && previous && was_preset)
                for (size_t j = 0; j < o.preset_count; j++)
                    if (id == o.presets[j].id)
                        p = o.presets + j;
            if (!clear && !p)
                continue;
            if (p && o.preset_policy == CUI_DATE_PICKER_PRESET_MATCH_SHAPE_ENABLED &&
                !cui_datetime_enabled(p->date, o.picker.calendar))
                continue;
            if (clear)
                *v = (cui_date_time_value){.time = def};
            else {
                v->date = p->date;
                if (p->has_time)
                    v->time = cui_datetime_truncate(p->time, o.precision);
            }
            cui_datetime_sync_date(s, v->date, o.picker.calendar);
            if (clear || p->has_time)
                s->time_field = (cui_time_state){0};
            s->actions.picker.overlay.open = false;
            if (p)
                cui_focus(ui, trigger);
            cui_consume_event(ui, e);
            cui_invalidate(ui);
            r.action_activated = true;
            r.source_change = true;
            break;
        }
    s->observed = *v;
    s->actions.observed_value = (cui_date_picker_value){.dates = {.start = v->date}};
    r.value_changed = !cui_datetime_same(before, *v);
    cui_interact(ui, trigger, CUI_ROLE_BUTTON, name, o.picker.disabled, false, 0);
    cui_set_semantic_details(ui, trigger,
                             (cui_semantic_details){.expanded = s->actions.picker.overlay.open,
                                                    .controls = s->actions.picker.overlay.id});
    if (o.cleanable && cui_date_valid(v->date)) {
        cui_interact(ui, o.clear_id, CUI_ROLE_BUTTON, o.clear_label, o.picker.disabled, false, 0);
        if (cui_last_error(ui) == CUI_OK) {
            s->actions.submitted_clear = true;
            s->actions.submitted_clear_id = o.clear_id;
            s->actions.clear_frame = frame;
        }
    } else
        s->actions.submitted_clear = false;
    return r;
}
