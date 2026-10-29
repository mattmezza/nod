/* SPDX-License-Identifier: MIT */
#include "cui/cui_status.h"
#include "cui/cui_content.h"
#include <math.h>
cui_tag_options cui_tag_options_default(void) {
    return (cui_tag_options){.variant = CUI_TAG_SECONDARY, .size = CUI_STATUS_MEDIUM};
}
cui_marker_options cui_marker_options_default(void) {
    return (cui_marker_options){.width = 480, .duration = 2, .shimmer_spread = .3f};
}
bool cui_base_marker(cui_marker_options o, const cui_marker_child *children, size_t count,
                     cui_marker_plan *out) {
    if (!out || (count && !children) || count > CUI_MARKER_MAX_CHILDREN ||
        o.variant < CUI_MARKER_PLAIN || o.variant > CUI_MARKER_BORDER ||
        o.alignment < CUI_MARKER_ALIGN_DEFAULT || o.alignment > CUI_MARKER_ALIGN_END ||
        o.loading_style < CUI_MARKER_SPINNER || o.loading_style > CUI_MARKER_SHIMMER ||
        !isfinite(o.width) || o.width <= 0 || !isfinite(o.now_seconds) || o.now_seconds < 0 ||
        !isfinite(o.duration) || o.duration < 0 || !isfinite(o.shimmer_spread) ||
        o.shimmer_spread < 0)
        return false;
    bool icon = false, content = false;
    for (size_t i = 0; i < count; i++) {
        if (children[i].kind < CUI_MARKER_ICON || children[i].kind > CUI_MARKER_DIRECT ||
            !isfinite(children[i].width) || children[i].width < 0 ||
            (children[i].text.length && !children[i].text.data))
            return false;
        icon |= children[i].kind == CUI_MARKER_ICON;
        content |= children[i].kind == CUI_MARKER_TEXT || children[i].kind == CUI_MARKER_CONTENT;
    }
    double duration = fmax(o.duration, .001), raw = o.now_seconds / duration;
    if (!isfinite(raw))
        return false;
    float phase = (float)(o.once ? fmin(raw, 1) : fmod(raw, 1));
    bool animated = o.loading && !o.reduced_motion &&
                    (o.loading_style == CUI_MARKER_SPINNER ? !icon : content) &&
                    !(o.once && raw >= 1);
    cui_marker_plan p = {
        .alignment = o.alignment == CUI_MARKER_ALIGN_DEFAULT
                         ? (o.variant == CUI_MARKER_SEPARATOR ? CUI_MARKER_ALIGN_CENTER
                                                              : CUI_MARKER_ALIGN_START)
                         : o.alignment,
        .spinner = o.loading && o.loading_style == CUI_MARKER_SPINNER && !icon,
        .animated = animated,
        .phase = o.reverse ? 1 - phase : phase,
        .rich_opacity = o.loading && !o.reduced_motion && o.loading_style == CUI_MARKER_SHIMMER
                            ? .6f + .4f * (float)(.5 + .5 * cos(phase * 6.283185307179586))
                            : 1,
        .next_wake_seconds = animated ? o.now_seconds + 1.0 / 60 : 0};
    *out = p;
    return true;
}
cui_shimmer_options cui_shimmer_options_default(void) {
    return (cui_shimmer_options){.duration = 2, .spread = .3f, .role = CUI_ROLE_LABEL};
}
bool cui_base_shimmer(cui_shimmer_options o, cui_shimmer_plan *out) {
    if (!out || !isfinite(o.now_seconds) || o.now_seconds < 0 || !isfinite(o.duration) ||
        o.duration < 0 || !isfinite(o.spread) || o.spread < 0 || !isfinite(o.font_size) ||
        o.font_size < 0 || !isfinite(o.max_width) || o.max_width < 0 || o.role < CUI_ROLE_NONE ||
        o.role > CUI_ROLE_LOG)
        return false;
    double elapsed = o.now_seconds / fmax(.001, o.duration);
    if (!isfinite(elapsed))
        return false;
    float phase = (float)(o.once ? fmin(1, elapsed) : fmod(elapsed, 1));
    bool animated = !o.reduced_motion && !(o.once && elapsed >= 1);
    *out = (cui_shimmer_plan){.phase = o.reverse ? 1 - phase : phase,
                              .animated = animated,
                              .next_wake_seconds = animated ? o.now_seconds + 1.0 / 60 : 0};
    return true;
}
static bool cui_status_notification_id_equal(cui_notification_id a, cui_notification_id b) {
    return a.type == b.type && a.key == b.key;
}
static bool cui_status_notification_time(double now) {
    return isfinite(now) && now >= 0;
}
bool cui_notification_settings_valid(cui_notification_settings s) {
    return s.placement >= CUI_NOTIFICATION_TOP_LEFT &&
           s.placement <= CUI_NOTIFICATION_RIGHT_CENTER && s.delivery >= CUI_NOTIFICATION_IN_APP &&
           s.delivery <= CUI_NOTIFICATION_BOTH && isfinite(s.width) && s.width > 0 &&
           isfinite(s.margin_left) && s.margin_left >= 0 && isfinite(s.margin_right) &&
           s.margin_right >= 0 && isfinite(s.margin_top) && s.margin_top >= 0 &&
           isfinite(s.margin_bottom) && s.margin_bottom >= 0 &&
           s.max_visible <= CUI_NOTIFICATION_MAX_ITEMS &&
           s.shadow_policy >= CUI_NOTIFICATION_SHADOW_LEGACY &&
           s.shadow_policy <= CUI_NOTIFICATION_SHADOW_SOURCE;
}
static bool cui_status_notification_valid(cui_notification n) {
    return (n.id.type || n.id.key) && n.kind >= CUI_NOTIFICATION_UNTYPED &&
           n.kind <= CUI_NOTIFICATION_ERROR && n.placement >= CUI_NOTIFICATION_TOP_LEFT &&
           n.placement <= CUI_NOTIFICATION_RIGHT_CENTER && n.delivery >= CUI_NOTIFICATION_IN_APP &&
           n.delivery <= CUI_NOTIFICATION_BOTH && isfinite(n.height) && n.height >= 0 &&
           (!n.title.length || n.title.data) && (!n.message.length || n.message.data) &&
           (!n.icon.length || n.icon.data) && (!n.action_label.length || n.action_label.data);
}
cui_notification cui_notification_default(void) {
    return (cui_notification){.autohide = true};
}
cui_notification_settings cui_notification_settings_default(void) {
    return (cui_notification_settings){.placement = CUI_NOTIFICATION_TOP_RIGHT,
                                       .width = 382,
                                       .margin_left = 16,
                                       .margin_right = 16,
                                       .margin_top = 48,
                                       .margin_bottom = 16,
                                       .max_visible = 10};
}
bool cui_notification_init(cui_notification_state *s, cui_notification_entry *entries,
                           size_t capacity, cui_notification_settings settings,
                           cui_notification_host host) {
    if (!s || !entries || !capacity || capacity > CUI_NOTIFICATION_MAX_ITEMS ||
        !cui_notification_settings_valid(settings))
        return false;
    *s = (cui_notification_state){
        .entries = entries, .capacity = capacity, .settings = settings, .host = host};
    return true;
}
static bool cui_status_notification_retract(cui_notification_state *s, cui_notification_id id) {
    for (size_t i = 0; i < s->system_count; i++)
        if (cui_status_notification_id_equal(s->system_registry[i].id, id)) {
            for (size_t j = i + 1; j < s->system_count; j++)
                s->system_registry[j - 1] = s->system_registry[j];
            s->system_count--;
            if (s->host.retract)
                s->host.retract(s->host.user, id);
            return true;
        }
    return false;
}
static void cui_status_notification_register(cui_notification_state *s, cui_notification n) {
    for (size_t i = 0; i < s->system_count; i++)
        if (cui_status_notification_id_equal(s->system_registry[i].id, n.id)) {
            for (size_t j = i + 1; j < s->system_count; j++)
                s->system_registry[j - 1] = s->system_registry[j];
            s->system_count--;
            break;
        }
    if (s->system_count == 100) {
        for (size_t i = 1; i < 100; i++)
            s->system_registry[i - 1] = s->system_registry[i];
        s->system_count--;
    }
    s->system_registry[s->system_count++] = n;
}
bool cui_notification_push(cui_notification_state *s, cui_notification n, double now) {
    if (!s || !s->entries || !cui_status_notification_time(now) ||
        !cui_status_notification_valid(n) || !cui_notification_settings_valid(s->settings))
        return false;
    size_t existing = s->count;
    for (size_t i = 0; i < s->count; i++)
        if (cui_status_notification_id_equal(s->entries[i].notification.id, n.id)) {
            existing = i;
            break;
        }

    cui_notification_delivery delivery = n.custom_delivery ? n.delivery : s->settings.delivery;
    if (delivery != CUI_NOTIFICATION_SYSTEM && existing == s->count && s->count == s->capacity)
        return false;
    bool system = delivery != CUI_NOTIFICATION_IN_APP;
    if (system && (!s->host.send || !s->host.send(s->host.user, &n)))
        return false;
    if (system)
        cui_status_notification_register(s, n);
    if (delivery == CUI_NOTIFICATION_SYSTEM)
        return true;
    if (existing < s->count) {
        for (size_t i = existing + 1; i < s->count; i++)
            s->entries[i - 1] = s->entries[i];
        s->count--;
    }
    s->entries[s->count++] =
        (cui_notification_entry){.notification = n,
                                 .phase = CUI_NOTIFICATION_STARTING,
                                 .last_advance = now,
                                 .remaining = n.autohide ? 5 : -1,
                                 .system_posted = system,
                                 .in_app = delivery != CUI_NOTIFICATION_SYSTEM};
    return true;
}
bool cui_notification_dismiss(cui_notification_state *s, cui_notification_id id, double now) {
    if (!s || !cui_status_notification_time(now) || !cui_notification_settings_valid(s->settings))
        return false;
    bool retracted = cui_status_notification_retract(s, id);
    for (size_t i = 0; i < s->count; i++)
        if (cui_status_notification_id_equal(s->entries[i].notification.id, id)) {
            cui_notification_entry *e = &s->entries[i];
            if (e->phase == CUI_NOTIFICATION_ENDING)
                return retracted;
            e->phase = CUI_NOTIFICATION_ENDING;
            e->elapsed = 0;
            e->last_advance = now;
            e->explicit_close = true;
            e->system_posted = false;
            return true;
        }
    return retracted;
}
size_t cui_notification_dismiss_type(cui_notification_state *s, uint64_t type, double now) {
    if (!s || !cui_status_notification_time(now) || !cui_notification_settings_valid(s->settings))
        return 0;
    size_t count = 0;
    for (size_t i = 0; i < s->system_count;)
        if (s->system_registry[i].id.type == type)
            count += cui_notification_dismiss(s, s->system_registry[i].id, now);
        else
            i++;
    for (size_t i = 0; i < s->count; i++)
        if (s->entries[i].notification.id.type == type)
            count += cui_notification_dismiss(s, s->entries[i].notification.id, now);
    return count;
}
size_t cui_notification_clear(cui_notification_state *s, double now) {
    if (!s || !cui_status_notification_time(now) || !cui_notification_settings_valid(s->settings))
        return 0;
    size_t count = 0;
    while (s->system_count)
        count += cui_notification_dismiss(s, s->system_registry[0].id, now);
    for (size_t i = 0; i < s->count; i++)
        count += cui_notification_dismiss(s, s->entries[i].notification.id, now);
    return count;
}
cui_notification_advance cui_notification_update(cui_notification_state *s, double now,
                                                 bool paused) {
    cui_notification_advance r = {0};
    if (!s || !cui_status_notification_time(now) || !cui_notification_settings_valid(s->settings))
        return r;
    for (size_t i = 0; i < s->count; i++) {
        cui_notification_entry *e = &s->entries[i];
        double delta = fmax(0, now - e->last_advance);
        e->last_advance = fmax(now, e->last_advance);
        if (!e->in_app)
            continue;
        switch (e->phase) {
        case CUI_NOTIFICATION_STARTING:
            e->elapsed += delta;
            if (e->elapsed + 1e-9 >= .4) {
                e->phase = CUI_NOTIFICATION_PRESENT;
                e->elapsed = 0;
                r.presented++;
                r.changed = true;
            }
            break;
        case CUI_NOTIFICATION_PRESENT:
            if (!paused && e->remaining >= 0) {
                e->remaining = fmax(0, e->remaining - delta);
                if (e->remaining == 0) {
                    e->phase = CUI_NOTIFICATION_ENDING;
                    e->elapsed = 0;
                    r.ending++;
                    r.changed = true;
                }
            }
            break;
        case CUI_NOTIFICATION_ENDING:
            e->elapsed += delta;
            break;
        }
    }
    for (size_t i = 0; i < s->count;) {
        cui_notification_entry *e = &s->entries[i];
        if (e->phase == CUI_NOTIFICATION_ENDING && (!e->in_app || e->elapsed + 1e-9 >= .2)) {
            cui_notification n = e->notification;
            for (size_t j = i + 1; j < s->count; j++)
                s->entries[j - 1] = s->entries[j];
            s->count--;
            r.removed++;
            r.changed = true;
            if (n.on_close)
                n.on_close(n.user, n.id);
        } else {
            i++;
        }
    }
    for (size_t i = 0; i < s->count; i++) {
        cui_notification_entry *e = &s->entries[i];
        if (!e->in_app)
            continue;
        double delay = 0;
        if (e->phase != CUI_NOTIFICATION_PRESENT)
            delay = 1.0 / 60;
        else if (!paused && e->remaining >= 0)
            delay = fmin(.05, e->remaining);
        if (delay > 0 && (!r.next_wake_seconds || now + delay < r.next_wake_seconds))
            r.next_wake_seconds = now + delay;
    }
    return r;
}
bool cui_notification_visible(const cui_notification_state *s, size_t index) {
    if (!s || index >= s->count || !s->entries[index].in_app)
        return false;
    if (s->entries[index].phase == CUI_NOTIFICATION_ENDING)
        return true;
    size_t newer = 0;
    for (size_t i = index + 1; i < s->count; i++)
        newer += s->entries[i].in_app && s->entries[i].phase != CUI_NOTIFICATION_ENDING;
    return newer < s->settings.max_visible;
}
bool cui_notification_stack_geometry(const float *heights, size_t count, size_t index, bool bottom,
                                     cui_notification_geometry *out) {
    if (!heights || !out || !count || count > CUI_NOTIFICATION_MAX_ITEMS || index >= count)
        return false;
    float expanded = 14 * (count - 1), collapsed = 0, newer = 0;
    size_t rank = count - 1 - index;
    for (size_t i = 0; i < count; i++) {
        if (!isfinite(heights[i]) || heights[i] < 0)
            return false;
        expanded += heights[i];
        collapsed = fmaxf(collapsed, heights[i] + 14 * (count - 1 - i));
        if (i > index)
            newer += heights[i];
    }
    if (!isfinite(expanded) || !isfinite(collapsed) || !isfinite(newer))
        return false;
    *out = (cui_notification_geometry){
        collapsed, expanded, bottom ? collapsed - heights[index] - 14 * rank : 14 * rank,
        bottom ? expanded - newer - 14 * rank - heights[index] : newer + 14 * rank, rank};
    return true;
}
bool cui_notification_system_click(cui_notification_state *s, cui_notification_id id, double now) {
    if (!s || !cui_status_notification_time(now) || !cui_notification_settings_valid(s->settings))
        return false;
    for (size_t i = 0; i < s->system_count; i++)
        if (cui_status_notification_id_equal(s->system_registry[i].id, id)) {
            cui_notification n = s->system_registry[i];
            if (s->host.activate)
                s->host.activate(s->host.user, id);
            cui_notification_dismiss(s, id, now);
            if (n.on_click)
                n.on_click(n.user, id);
            return true;
        }
    return false;
}

/* Loading primitives deliberately expose scheduling instead of a perpetual
 * invalidation loop. The host owns the clock and the next frame deadline. */
cui_spinner_options cui_spinner_options_default(void) {
    return (cui_spinner_options){.size = 16, .duration = .8f};
}
cui_skeleton_options cui_skeleton_options_default(void) {
    return (cui_skeleton_options){.width = 160, .height = 16, .duration = 2};
}
bool cui_loading_motion_sample(cui_loading_motion *state, double now, float period, bool reduced,
                               bool skeleton, cui_loading_ease_fn ease, void *user,
                               cui_loading_motion_result *out) {
    if (!state || !out || !isfinite(now) || now < 0 || now > 1e12 || !isfinite(period) ||
        period <= 0 || period > 60 ||
        (state->ready &&
         (!isfinite(state->started) || !isfinite(state->last_time) || state->started < 0 ||
          state->last_time < state->started || state->last_time > 1e12)))
        return false;
    cui_loading_motion next = *state;
    if (!next.ready)
        next = (cui_loading_motion){true, now, now};
    now = fmax(now, next.last_time);
    next.last_time = now;
    float phase = reduced ? 0 : (float)(fmod(now - next.started, period) / period);
    float input = skeleton ? (phase < .5f ? phase * 2 : (1 - phase) * 2) : phase;
    float eased = reduced       ? 0
                  : ease        ? ease(input, user)
                  : input < .5f ? 2 * input * input
                                : 1 - 2 * (1 - input) * (1 - input);
    if (!isfinite(eased) || eased < 0 || eased > 1)
        return false;
    *out = (cui_loading_motion_result){.phase = eased,
                                       .opacity = skeleton ? 1 - eased * .5f : 1,
                                       .active = !reduced,
                                       .next_wake_seconds = reduced ? 0 : now + 1.0 / 60};
    *state = next;
    return true;
}
