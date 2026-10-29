/* SPDX-License-Identifier: MIT */
#include "cui/cui_carousel.h"
#include <float.h>
#include <math.h>
#include <string.h>
static float cui_carousel_clamp(float v, float low, float high) {
    return fmaxf(low, fminf(high, v));
}
static void cui_carousel_stop(cui_carousel_state *s) {
    bool scrolling = s->scrolling;
    if (scrolling)
        s->scroll_deferred = true;
    if (!scrolling && !s->scroll_deferred)
        s->scroll_seen = false;
    s->wheel_owned = false;
    s->dragging = s->axis_locked = s->scrolling = s->suppress_click = s->scroll_contact = false;
    s->gesture_delta = 0;
}
bool cui_carousel_state_init(cui_carousel_state *s, cui_carousel_geometry *items, size_t capacity,
                             size_t count) {
    if (!s || !items || !capacity || capacity > CUI_CAROUSEL_ITEM_LIMIT || count > capacity)
        return false;
    memset(s, 0, sizeof(*s));
    memset(items, 0, capacity * sizeof(*items));
    s->items = items;
    s->capacity = capacity;
    s->count = count;
    s->selected = count ? 0 : CUI_CAROUSEL_NO_SELECTION;
    s->axis = CUI_ORIENTATION_HORIZONTAL;
    s->response = .28f;
    s->damping = .85f;
    return true;
}
bool cui_carousel_set_count(cui_carousel_state *s, size_t count) {
    if (!s || !s->items || count > s->capacity)
        return false;
    if (s->count == count)
        return true;
    s->count = count;
    s->selected = count ? (s->selected == CUI_CAROUSEL_NO_SELECTION ? 0
                           : s->selected < count                    ? s->selected
                                                                    : count - 1)
                        : CUI_CAROUSEL_NO_SELECTION;
    s->geometry_ready = s->runtime_looping = false;
    s->offset = s->target = s->velocity = 0;
    cui_carousel_stop(s);
    return true;
}
bool cui_carousel_set_axis(cui_carousel_state *s, cui_orientation axis) {
    if (!s || (axis != CUI_ORIENTATION_HORIZONTAL && axis != CUI_ORIENTATION_VERTICAL))
        return false;
    if (axis != s->axis) {
        s->axis = axis;
        s->geometry_ready = s->runtime_looping = false;
        s->offset = s->target = s->velocity = 0;
        cui_carousel_stop(s);
    }
    return true;
}
void cui_carousel_set_looping(cui_carousel_state *s, bool looping) {
    if (!s || s->looping == looping)
        return;
    s->looping = looping;
    s->runtime_looping = looping && s->geometry_ready && s->count > 1 && s->cycle >= s->viewport;
    cui_carousel_stop(s);
    s->offset = s->target = cui_carousel_snap(s, s->selected);
    s->velocity = 0;
}
float cui_carousel_snap(const cui_carousel_state *s, size_t index) {
    if (!s || !s->geometry_ready || index >= s->count)
        return 0;
    float target = s->items[0].start - s->items[index].start;
    return s->runtime_looping ? target : cui_carousel_clamp(target, -s->maximum, 0);
}
bool cui_carousel_set_geometry(cui_carousel_state *s, const cui_carousel_geometry *items,
                               size_t count, float viewport) {
    if (!s || !s->items || count != s->count || count > s->capacity || (count && !items) ||
        !isfinite(viewport) || viewport <= 0)
        return false;
    double end = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!items[i].id || !isfinite(items[i].start) || !isfinite(items[i].extent) ||
            items[i].extent <= 0 || items[i].start < 0 || (i && items[i].start < end))
            return false;
        for (size_t j = 0; j < i; ++j)
            if (items[i].id == items[j].id)
                return false;
        end = (double)items[i].start + items[i].extent;
        if (end > FLT_MAX)
            return false;
    }
    float gap = count > 1 ? items[1].start - items[0].start - items[0].extent : 0;
    double cycle_value = count ? end - items[0].start + gap : 0;
    if (cycle_value > FLT_MAX)
        return false;
    float cycle = (float)cycle_value;
    bool changed = !s->geometry_ready || s->viewport != viewport || s->cycle != cycle;
    for (size_t i = 0; i < count; ++i)
        changed |= s->items[i].id != items[i].id || s->items[i].start != items[i].start ||
                   s->items[i].extent != items[i].extent;
    if (count)
        memmove(s->items, items, count * sizeof(*items));
    s->viewport = viewport;
    s->cycle = cycle;
    s->maximum = count ? fmaxf(0, (float)end - items[0].start - viewport) : 0;
    s->geometry_ready = true;
    s->runtime_looping = s->looping && count > 1 && cycle >= viewport;
    if (changed) {
        cui_carousel_stop(s);
        s->offset = s->target = cui_carousel_snap(s, s->selected);
        s->velocity = 0;
    }
    return true;
}
size_t cui_carousel_navigation(const cui_carousel_state *s, bool next) {
    if (!s || s->selected >= s->count || s->count < 2)
        return CUI_CAROUSEL_NO_SELECTION;
    size_t current = s->selected;
    for (size_t n = 0; n < s->count - 1; ++n) {
        if ((next && current + 1 == s->count) || (!next && current == 0)) {
            if (!s->runtime_looping && (s->geometry_ready || !s->looping))
                return CUI_CAROUSEL_NO_SELECTION;
            current = next ? 0 : s->count - 1;
        } else
            current = next ? current + 1 : current - 1;
        if (!s->geometry_ready || s->runtime_looping ||
            cui_carousel_snap(s, current) != cui_carousel_snap(s, s->selected))
            return current;
    }
    return CUI_CAROUSEL_NO_SELECTION;
}
static bool cui_carousel_select_internal(cui_carousel_state *s, size_t index, bool emit,
                                         bool gesture) {
    if (!s)
        return false;
    if (s->runtime_looping && s->selected < s->count) {
        float canonical = cui_carousel_snap(s, s->selected);
        float shift = canonical - s->target;
        if (fabsf(shift) >= s->cycle * .5f) {
            s->offset += shift;
            s->gesture_start += shift;
            s->target = canonical;
        }
    }
    cui_carousel_stop(s);
    if (index >= s->count)
        return false;
    size_t previous = s->selected;
    float target = cui_carousel_snap(s, index);
    if (s->runtime_looping && previous < s->count) {
        /* First/last transitions use the adjacent cycle in the requested direction. */
        if (previous == 0 && index + 1 == s->count)
            target += s->cycle;
        else if (previous + 1 == s->count && index == 0)
            target -= s->cycle;
    }
    if (gesture && s->runtime_looping)
        target += roundf((s->offset - target) / s->cycle) * s->cycle;
    s->selected = index;
    s->target = target;
    if (previous == index)
        return false;
    if (emit && s->on_change)
        s->on_change(index, s->change_user);
    return true;
}
void cui_carousel_set_selected(cui_carousel_state *s, size_t index) {
    if (!s)
        return;
    if (!s->count) {
        s->selected = CUI_CAROUSEL_NO_SELECTION;
        cui_carousel_stop(s);
        return;
    }
    cui_carousel_select_internal(s, index < s->count ? index : s->count - 1, false, false);
}
bool cui_carousel_select(cui_carousel_state *s, size_t index) {
    return cui_carousel_select_internal(s, index, true, false);
}
bool cui_carousel_step(cui_carousel_state *s, bool next) {
    return cui_carousel_select(s, cui_carousel_navigation(s, next));
}
size_t cui_carousel_nearest(const cui_carousel_state *s, float offset) {
    if (!s || !s->geometry_ready || !s->count || !isfinite(offset))
        return CUI_CAROUSEL_NO_SELECTION;
    size_t result = 0;
    float distance = FLT_MAX;
    for (size_t i = 0; i < s->count; ++i) {
        float target = cui_carousel_snap(s, i);
        float d = fabsf(offset - target);
        if (s->runtime_looping)
            d = fminf(d,
                      fminf(fabsf(offset - target - s->cycle), fabsf(offset - target + s->cycle)));
        if (d < distance) {
            distance = d;
            result = i;
        }
    }
    return result;
}
bool cui_carousel_begin_drag(cui_carousel_state *s, float x, float y) {
    if (!s || s->count < 2 || !s->geometry_ready || !isfinite(x) || !isfinite(y))
        return false;
    cui_carousel_stop(s);
    s->dragging = true;
    s->pointer_x = x;
    s->pointer_y = y;
    s->gesture_start = s->offset;
    s->gesture_index = s->selected;
    s->velocity = 0;
    return true;
}
bool cui_carousel_update_drag(cui_carousel_state *s, float x, float y) {
    if (!s || !s->dragging || !isfinite(x) || !isfinite(y))
        return false;
    float primary = s->axis == CUI_ORIENTATION_HORIZONTAL ? x - s->pointer_x : y - s->pointer_y;
    float cross = s->axis == CUI_ORIENTATION_HORIZONTAL ? y - s->pointer_y : x - s->pointer_x;
    if (!isfinite(primary) || !isfinite(cross))
        return false;
    if (!s->axis_locked) {
        if (fmaxf(fabsf(primary), fabsf(cross)) <= 2)
            return false;
        if (fabsf(cross) > fabsf(primary)) {
            s->dragging = false;
            s->suppress_click = true;
            return false;
        }
        s->axis_locked = true;
    }
    s->gesture_delta = primary;
    s->offset = s->gesture_start + primary;
    if (s->runtime_looping) {
        float normalized = (float)fmod((double)s->offset + s->cycle, s->cycle);
        if (normalized < 0)
            normalized += s->cycle;
        normalized -= s->cycle;
        s->gesture_start += normalized - s->offset;
        s->offset = normalized;
    } else
        s->offset = cui_carousel_clamp(s->offset, -s->maximum, 0);
    return true;
}
bool cui_carousel_finish_drag(cui_carousel_state *s) {
    if (!s)
        return false;
    if (!s->dragging) {
        s->suppress_click = false;
        return false;
    }
    size_t index = cui_carousel_nearest(s, s->offset);
    if (s->runtime_looping && fabsf(s->gesture_delta) >= fmaxf(1, s->viewport * .25f)) {
        if (s->gesture_index == 0 && s->gesture_delta > 0)
            index = s->count - 1;
        else if (s->gesture_index + 1 == s->count && s->gesture_delta < 0)
            index = 0;
    }
    return cui_carousel_select_internal(s, index, true, true);
}
void cui_carousel_cancel(cui_carousel_state *s) {
    if (!s)
        return;
    if (s->dragging || s->scrolling)
        s->offset = s->gesture_start;
    cui_carousel_stop(s);
    s->scroll_deferred = s->wheel_owned = false;
    s->scroll_axis = CUI_ORIENTATION_UNSPECIFIED;
    s->target = cui_carousel_snap(s, s->selected);
}
bool cui_carousel_wheel(cui_carousel_state *s, float delta, double now) {
    if (!s || !isfinite(delta) || !isfinite(now) || delta == 0)
        return false;
    bool burst = s->scroll_seen && now >= s->last_scroll && now - s->last_scroll < .028;
    if (burst) {
        s->last_scroll = now;
        return s->wheel_owned;
    }
    s->wheel_owned = cui_carousel_step(s, delta < 0);
    s->last_scroll = now;
    s->scroll_seen = true;
    return s->wheel_owned;
}
static bool cui_carousel_scroll_impl(cui_carousel_state *s, float dx, float dy,
                                     cui_scroll_phase phase, double now, bool contact) {
    if (!s || !isfinite(dx) || !isfinite(dy) || !isfinite(now) || phase < CUI_SCROLL_PHASE_NONE ||
        phase > CUI_SCROLL_PHASE_CANCELLED || s->count < 2 || !s->geometry_ready)
        return false;
    if (!contact && s->scroll_seen && now - s->last_scroll >= .028) {
        if (s->scrolling) {
            s->dragging = true;
            cui_carousel_finish_drag(s);
        }
        s->scroll_deferred = false;
        s->scroll_axis = CUI_ORIENTATION_UNSPECIFIED;
    }
    if (phase == CUI_SCROLL_PHASE_STARTED) {
        if (s->scrolling) {
            s->dragging = true;
            cui_carousel_finish_drag(s);
        }
        s->scroll_deferred = false;
        s->scroll_axis = CUI_ORIENTATION_UNSPECIFIED;
    }
    s->last_scroll = now;
    s->scroll_seen = true;
    if (phase == CUI_SCROLL_PHASE_ENDED || phase == CUI_SCROLL_PHASE_CANCELLED) {
        bool owned = s->scrolling;
        if (owned) {
            if (phase == CUI_SCROLL_PHASE_CANCELLED)
                cui_carousel_cancel(s);
            else {
                s->dragging = true;
                cui_carousel_finish_drag(s);
            }
        }
        s->scroll_contact = false;
        s->scroll_deferred = false;
        s->scroll_axis = CUI_ORIENTATION_UNSPECIFIED;
        return owned;
    }
    s->scroll_contact = contact;
    if (s->scroll_deferred)
        return false;
    if (s->scroll_axis == CUI_ORIENTATION_UNSPECIFIED && (dx != 0 || dy != 0))
        s->scroll_axis =
            fabsf(dx) > fabsf(dy) ? CUI_ORIENTATION_HORIZONTAL : CUI_ORIENTATION_VERTICAL;
    if (s->scroll_axis != s->axis)
        return false;
    float delta = s->axis == CUI_ORIENTATION_HORIZONTAL ? dx : dy;
    if (!s->scrolling) {
        if (!s->runtime_looping &&
            ((delta < 0 && s->offset <= -s->maximum) || (delta > 0 && s->offset >= 0))) {
            s->scroll_deferred = true;
            return false;
        }
        cui_carousel_stop(s);
        s->scrolling = true;
        s->scroll_contact = contact;
        s->scroll_seen = true;
        s->gesture_start = s->offset;
        s->gesture_index = s->selected;
        s->velocity = 0;
    }
    s->gesture_delta = (float)fmax(-FLT_MAX, fmin(FLT_MAX, (double)s->gesture_delta + delta));
    double next_offset = (double)s->offset + delta;
    s->offset = (float)fmax(-FLT_MAX, fmin(FLT_MAX, next_offset));
    if (!s->runtime_looping)
        s->offset = cui_carousel_clamp(s->offset, -s->maximum, 0);
    else {
        float normalized = (float)fmod((double)s->offset + s->cycle, s->cycle);
        if (normalized < 0)
            normalized += s->cycle;
        normalized -= s->cycle;
        s->gesture_start += normalized - s->offset;
        s->offset = normalized;
    }
    return true;
}
bool cui_carousel_scroll(cui_carousel_state *s, float dx, float dy, cui_scroll_phase phase,
                         double now) {
    return cui_carousel_scroll_impl(s, dx, dy, phase, now, false);
}
bool cui_carousel_touch_scroll(cui_carousel_state *s, float dx, float dy, cui_scroll_phase phase,
                               double now) {
    return cui_carousel_scroll_impl(s, dx, dy, phase, now, true);
}
bool cui_carousel_tick(cui_carousel_state *s, double now, bool reduced) {
    if (!s || !isfinite(now))
        return false;
    double dt = s->clock_ready ? fmax(0, now - s->last_time) : 0;
    s->last_time = s->clock_ready ? fmax(s->last_time, now) : now;
    s->clock_ready = true;
    if (!s->scroll_contact && s->scrolling && now - s->last_scroll >= .028) {
        s->dragging = true;
        cui_carousel_finish_drag(s);
        s->scroll_axis = CUI_ORIENTATION_UNSPECIFIED;
    }
    if (!s->scroll_contact && s->scroll_deferred && now - s->last_scroll >= .028)
        s->scroll_deferred = false;
    if (s->dragging)
        return false;
    if (s->scrolling)
        return true;
    if (reduced) {
        s->offset = s->target;
        s->velocity = 0;
    } else {
        /* Exact underdamped spring integration; response is one oscillation period. */
        double response = isfinite(s->response) && s->response > 0 ? s->response : .28;
        double damping = isfinite(s->damping) && s->damping >= 0 ? s->damping : .85;
        double omega = 6.283185307179586 / response;
        double displacement = s->offset - s->target, velocity = s->velocity;
        if (damping < 1) {
            double wd = omega * sqrt(1 - damping * damping);
            double a = displacement, b = (velocity + damping * omega * displacement) / wd;
            double decay = exp(-damping * omega * dt), co = cos(wd * dt), si = sin(wd * dt);
            displacement = decay * (a * co + b * si);
            velocity = decay * ((-a * wd * si + b * wd * co) - damping * omega * (a * co + b * si));
        } else if (damping == 1) {
            double b = velocity + omega * displacement, decay = exp(-omega * dt);
            velocity = decay * (b - omega * (displacement + b * dt));
            displacement = decay * (displacement + b * dt);
        } else {
            double factor = damping + sqrt(damping * damping - 1);
            double r1 = -omega / factor, r2 = -omega * factor;
            double a = (velocity - r2 * displacement) / (r1 - r2), b = displacement - a;
            velocity = a * r1 * exp(r1 * dt) + b * r2 * exp(r2 * dt);
            displacement = a * exp(r1 * dt) + b * exp(r2 * dt);
        }
        s->offset = (float)fmax(-FLT_MAX, fmin(FLT_MAX, s->target + displacement));
        s->velocity = (float)fmax(-FLT_MAX, fmin(FLT_MAX, velocity));
        if (fabsf(s->offset - s->target) <= .5f && fabsf(s->velocity) <= .5f) {
            s->offset = s->target;
            s->velocity = 0;
        }
    }
    if (s->runtime_looping && s->offset == s->target) {
        s->offset = s->target = cui_carousel_snap(s, s->selected);
    }
    return s->offset != s->target || s->velocity != 0;
}
float cui_carousel_item_position(const cui_carousel_state *s, size_t i) {
    if (!s || i >= s->count || !s->geometry_ready)
        return 0;
    float position = s->items[i].start + s->offset;
    if (s->runtime_looping) {
        float center = s->viewport / 2 - s->offset;
        float item_center = s->items[i].start + s->items[i].extent / 2;
        position += roundf((center - item_center) / s->cycle) * s->cycle;
    }
    return position;
}
