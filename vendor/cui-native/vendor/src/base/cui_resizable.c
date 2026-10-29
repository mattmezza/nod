/* SPDX-License-Identifier: MIT */
#include "cui/cui_resizable.h"
#include <float.h>
#include <math.h>
#include <string.h>
bool cui_resizable_panel_valid(cui_resizable_panel p) {
    return p.id && (!p.accessibility_label.length || p.accessibility_label.data) &&
           (!p.has_initial_size || (isfinite(p.initial_size) && p.initial_size >= 0)) &&
           (!p.custom_range || (isfinite(p.minimum) && isfinite(p.maximum) && p.minimum >= 0 &&
                                p.maximum >= p.minimum));
}
cui_resizable_panel cui_resizable_panel_default(cui_id id) {
    return (cui_resizable_panel){.id = id};
}
bool cui_resizable_state_init(cui_resizable_state *s, cui_resizable_slot *slots, size_t capacity) {
    if (!s || !slots || !capacity || capacity > CUI_RESIZABLE_PANEL_LIMIT)
        return false;
    memset(slots, 0, capacity * sizeof(*slots));
    *s = (cui_resizable_state){.slots = slots, .capacity = capacity, .fit_constraints = true};
    return true;
}
static bool cui_resizable_valid(const cui_resizable_state *s) {
    return s && s->slots && s->capacity && s->capacity <= CUI_RESIZABLE_PANEL_LIMIT &&
           s->count <= s->capacity;
}
static float cui_resizable_clamp(float value, float lo, float hi) {
    return fmaxf(lo, fminf(hi, value));
}
static void cui_resizable_emit(cui_resizable_state *s) {
    if (s->on_resize)
        s->on_resize(s->resize_user, s);
}
static cui_resizable_error cui_resizable_fit(cui_resizable_state *s, bool proportion) {
    double sum = 0, minimum = 0, maximum = 0;
    for (size_t i = 0; i < s->count; i++)
        if (!s->slots[i].hidden) {
            sum += s->slots[i].size;
            minimum += s->slots[i].minimum;
            maximum += s->slots[i].maximum;
        }
    bool any_visible = false;
    for (size_t i = 0; i < s->count; i++)
        any_visible |= !s->slots[i].hidden;
    s->fit_constraints =
        !any_visible || (minimum <= s->extent + .001 && maximum >= s->extent - .001);
    if (proportion && sum > 0)
        for (size_t i = 0; i < s->count; i++)
            if (!s->slots[i].hidden)
                s->slots[i].size = (float)(s->slots[i].size / sum * s->extent);
    for (size_t i = 0; i < s->count; i++)
        s->slots[i].size =
            cui_resizable_clamp(s->slots[i].size, s->slots[i].minimum, s->slots[i].maximum);
    /* Equal-share water filling, bounded by the number of saturating slots. */
    for (size_t pass = 0; pass <= s->count; pass++) {
        sum = 0;
        size_t eligible = 0;
        for (size_t i = 0; i < s->count; i++)
            if (!s->slots[i].hidden)
                sum += s->slots[i].size;
        double delta = s->extent - sum;
        if (fabs(delta) < .001)
            break;
        for (size_t i = 0; i < s->count; i++)
            if (!s->slots[i].hidden && (delta > 0 ? s->slots[i].size < s->slots[i].maximum
                                                  : s->slots[i].size > s->slots[i].minimum))
                eligible++;
        if (!eligible)
            break;
        for (size_t i = 0; i < s->count; i++)
            if (!s->slots[i].hidden && (delta > 0 ? s->slots[i].size < s->slots[i].maximum
                                                  : s->slots[i].size > s->slots[i].minimum))
                s->slots[i].size = cui_resizable_clamp((float)(s->slots[i].size + delta / eligible),
                                                       s->slots[i].minimum, s->slots[i].maximum);
    }
    return s->fit_constraints ? CUI_RESIZABLE_OK : CUI_RESIZABLE_INFEASIBLE;
}
static cui_resizable_slot cui_resizable_new_slot(cui_resizable_panel p) {
    float lo = p.custom_range ? p.minimum : 100, hi = p.custom_range ? p.maximum : FLT_MAX;
    return (cui_resizable_slot){
        .id = p.id,
        .minimum = lo,
        .maximum = hi,
        .hidden = p.hidden,
        .size = cui_resizable_clamp(p.has_initial_size ? p.initial_size : lo, lo, hi)};
}
cui_resizable_error cui_resizable_sync(cui_resizable_state *s, const cui_resizable_panel *panels,
                                       size_t count, cui_resizable_axis axis, float extent) {
    if (!cui_resizable_valid(s) || (count && !panels) || !isfinite(extent) || extent < 0 ||
        axis < CUI_RESIZABLE_HORIZONTAL || axis > CUI_RESIZABLE_VERTICAL)
        return CUI_RESIZABLE_INVALID;
    if (count > s->capacity)
        return CUI_RESIZABLE_CAPACITY;
    for (size_t i = 0; i < count; i++) {
        if (!cui_resizable_panel_valid(panels[i]))
            return CUI_RESIZABLE_INVALID;
        for (size_t j = 0; j < i; j++)
            if (panels[j].id == panels[i].id)
                return CUI_RESIZABLE_INVALID;
    }
    /* Mark old slots that must survive; replacement then avoids cubic ID scans. */
    for (size_t j = 0; j < s->count; j++) {
        s->slots[j].scratch = 0;
        for (size_t i = 0; i < count; i++)
            if (s->slots[j].id == panels[i].id) {
                s->slots[j].scratch = 1;
                break;
            }
    }
    bool changed = count != s->count || axis != s->axis;
    bool first = s->count == 0;
    for (size_t i = 0; i < count; i++) {
        size_t found = i;
        while (found < s->count && s->slots[found].id != panels[i].id)
            found++;
        if (found == s->count) {
            if (s->count < s->capacity)
                s->count++;
            else {
                found = i;
                while (found < s->count && s->slots[found].scratch != 0)
                    found++;
            }
            s->slots[found] = cui_resizable_new_slot(panels[i]);
            changed = true;
        }
        if (found != i) {
            cui_resizable_slot temp = s->slots[i];
            s->slots[i] = s->slots[found];
            s->slots[found] = temp;
            changed = true;
        }
        cui_resizable_slot *slot = &s->slots[i];
        float lo = panels[i].custom_range ? panels[i].minimum : 100;
        float hi = panels[i].custom_range ? panels[i].maximum : FLT_MAX;
        changed |= slot->hidden != panels[i].hidden || slot->minimum != lo || slot->maximum != hi;
        slot->minimum = lo;
        slot->maximum = hi;
        slot->hidden = panels[i].hidden;
    }
    if (changed) {
        s->active_handle = 0;
        s->dragging = false;
    }
    s->count = count;
    s->axis = axis;
    bool extent_changed = s->extent != extent;
    s->extent = extent;
    if (first) {
        double fixed = 0;
        size_t flexible = 0;
        for (size_t i = 0; i < count; i++)
            if (!panels[i].hidden) {
                if (panels[i].has_initial_size)
                    fixed += s->slots[i].size;
                else
                    flexible++;
            }
        if (flexible)
            for (size_t i = 0; i < count; i++)
                if (!panels[i].hidden && !panels[i].has_initial_size)
                    s->slots[i].size = (float)fmax(0, (extent - fixed) / flexible);
    }
    cui_resizable_error error = cui_resizable_fit(s, !first && extent_changed);
    for (size_t i = 0; i < count; i++)
        s->slots[i].initialized = true;
    return error;
}
static size_t cui_resizable_next(const cui_resizable_state *s, size_t index) {
    do {
        index++;
    } while (index < s->count && s->slots[index].hidden);
    return index;
}
static bool cui_resizable_resize_at(cui_resizable_state *s, size_t index, float requested) {
    if (!cui_resizable_valid(s) || index >= s->count || s->slots[index].hidden ||
        !isfinite(requested))
        return false;
    size_t next = cui_resizable_next(s, index);
    if (next == s->count)
        return false;
    for (size_t i = 0; i < s->count; i++)
        s->slots[i].scratch = s->slots[i].size;
    double delta = requested - s->slots[index].size;
    if (delta > 0) {
        double available = 0;
        for (size_t i = next; i < s->count; i++)
            if (!s->slots[i].hidden)
                available += fmaxf(0, s->slots[i].size - s->slots[i].minimum);
        delta = fmin(delta, fmin(available, s->slots[index].maximum - s->slots[index].size));
        s->slots[index].size += (float)delta;
        for (size_t i = next; i < s->count && delta > 0; i++)
            if (!s->slots[i].hidden) {
                float take = (float)fmin(delta, fmaxf(0, s->slots[i].size - s->slots[i].minimum));
                s->slots[i].size -= take;
                delta -= take;
            }
    } else if (delta < 0) {
        double available = 0, capacity = 0;
        for (size_t i = 0; i <= index; i++)
            if (!s->slots[i].hidden)
                available += fmaxf(0, s->slots[i].size - s->slots[i].minimum);
        for (size_t i = next; i < s->count; i++)
            if (!s->slots[i].hidden)
                capacity += fmaxf(0, s->slots[i].maximum - s->slots[i].size);
        double transfer = fmin(-delta, fmin(available, capacity)), remainder = transfer;
        for (size_t i = index + 1; i > 0 && remainder > 0;) {
            i--;
            if (s->slots[i].hidden)
                continue;
            float take = (float)fmin(remainder, fmaxf(0, s->slots[i].size - s->slots[i].minimum));
            s->slots[i].size -= take;
            remainder -= take;
        }
        for (size_t i = next; i < s->count && transfer > 0; i++)
            if (!s->slots[i].hidden) {
                float add = (float)fmin(transfer, fmaxf(0, s->slots[i].maximum - s->slots[i].size));
                s->slots[i].size += add;
                transfer -= add;
            }
    }
    bool changed = false;
    for (size_t i = 0; i < s->count; i++)
        changed |= s->slots[i].size != s->slots[i].scratch;
    return changed;
}
bool cui_resizable_resize_panel(cui_resizable_state *s, size_t index, float size) {
    if (!cui_resizable_valid(s) || index >= s->count || !isfinite(size) || size < 0 ||
        s->slots[index].hidden)
        return false;
    bool changed = false;
    if (cui_resizable_next(s, index) < s->count)
        changed = cui_resizable_resize_at(s, index, size);
    else {
        size_t previous = index;
        while (previous > 0) {
            previous--;
            if (!s->slots[previous].hidden) {
                changed = cui_resizable_resize_at(
                    s, previous, s->slots[previous].size + s->slots[index].size - size);
                break;
            }
        }
    }
    s->dragging = false;
    s->active_handle = 0;
    cui_resizable_emit(s);
    return changed;
}
cui_resizable_error cui_resizable_insert(cui_resizable_state *s, size_t index,
                                         cui_resizable_panel panel) {
    if (!cui_resizable_valid(s) || index > s->count || !cui_resizable_panel_valid(panel))
        return CUI_RESIZABLE_INVALID;
    if (s->count == s->capacity)
        return CUI_RESIZABLE_CAPACITY;
    for (size_t i = 0; i < s->count; i++)
        if (s->slots[i].id == panel.id)
            return CUI_RESIZABLE_INVALID;
    cui_resizable_slot new_slot = cui_resizable_new_slot(panel);
    float size = panel.hidden ? 0 : new_slot.size;
    double available = fmax(1, s->extent - size);
    for (size_t i = 0; i < s->count; i++)
        if (!s->slots[i].hidden)
            s->slots[i].size = (float)(s->slots[i].size / fmax(1, s->extent) * available);
    memmove(s->slots + index + 1, s->slots + index, (s->count - index) * sizeof(*s->slots));
    s->slots[index] = new_slot;
    s->count++;
    s->active_handle = 0;
    s->dragging = false;
    return cui_resizable_fit(s, false);
}
bool cui_resizable_remove(cui_resizable_state *s, size_t index) {
    if (!cui_resizable_valid(s) || index >= s->count)
        return false;
    memmove(s->slots + index, s->slots + index + 1, (s->count - index - 1) * sizeof(*s->slots));
    s->count--;
    s->active_handle = 0;
    s->dragging = false;
    cui_resizable_fit(s, true);
    return true;
}
bool cui_resizable_reset(cui_resizable_state *s, size_t index) {
    if (!cui_resizable_valid(s) || index >= s->count)
        return false;
    cui_resizable_slot *slot = &s->slots[index];

    slot->phase = CUI_RESIZE_IDLE;
    slot->indicator_initialized = false;
    slot->indicator_length = slot->indicator_opacity = slot->start_length = slot->start_opacity =
        slot->target_length = slot->target_opacity = slot->transition_start = 0;
    s->active_handle = 0;
    s->dragging = false;
    return true;
}
void cui_resizable_clear(cui_resizable_state *s) {
    if (cui_resizable_valid(s)) {
        s->count = 0;
        s->active_handle = 0;
        s->dragging = false;
    }
}
bool cui_resizable_save(const cui_resizable_state *s, float *output, size_t capacity) {
    if (!cui_resizable_valid(s) || !output || capacity < s->count)
        return false;
    for (size_t i = 0; i < s->count; i++)
        output[i] = s->slots[i].size;
    return true;
}
cui_resizable_error cui_resizable_restore(cui_resizable_state *s, const float *sizes,
                                          size_t count) {
    if (!cui_resizable_valid(s) || count != s->count || (count && !sizes))
        return CUI_RESIZABLE_INVALID;
    for (size_t i = 0; i < count; i++)
        if (!isfinite(sizes[i]) || sizes[i] < 0)
            return CUI_RESIZABLE_INVALID;
    for (size_t i = 0; i < count; i++)
        s->slots[i].size = sizes[i];
    cui_resizable_error error = cui_resizable_fit(s, true);
    s->active_handle = 0;
    s->dragging = false;
    cui_resizable_emit(s);
    return error;
}
bool cui_resize_handle_rectangles(cui_resizable_axis axis, cui_resize_edge edge, cui_size container,
                                  cui_rect *band, cui_rect *line) {
    if (!band || !line || axis < CUI_RESIZABLE_HORIZONTAL || axis > CUI_RESIZABLE_VERTICAL ||
        edge < CUI_RESIZE_STRADDLE || edge > CUI_RESIZE_TRAILING || !isfinite(container.width) ||
        !isfinite(container.height) || container.width < 0 || container.height < 0)
        return false;
    float extent = axis == CUI_RESIZABLE_HORIZONTAL ? container.width : container.height;
    float cross = axis == CUI_RESIZABLE_HORIZONTAL ? container.height : container.width;
    float size = edge == CUI_RESIZE_STRADDLE ? 9 : 5;
    float start = edge == CUI_RESIZE_STRADDLE ? -4 : edge == CUI_RESIZE_LEADING ? 0 : extent - 5;
    float seam = edge == CUI_RESIZE_TRAILING ? extent - 1 : 0;
    *band = axis == CUI_RESIZABLE_HORIZONTAL ? (cui_rect){start, 0, size, cross}
                                             : (cui_rect){0, start, cross, size};
    *line = axis == CUI_RESIZABLE_HORIZONTAL ? (cui_rect){seam, 0, 1, cross}
                                             : (cui_rect){0, seam, cross, 1};
    return true;
}
cui_id cui_resizable_handle_id(cui_id group, cui_id panel) {
    uint64_t x = group ^ (panel + UINT64_C(0x9e3779b97f4a7c15));
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x ^= x >> 31;
    return x ? x : 1;
}
cui_resize_result cui_base_resize_handle(cui_context *ui, cui_id id, cui_resizable_state *s,
                                         size_t index, bool disabled, float step) {
    cui_resize_result result = {0};
    if (!ui || !id || !cui_resizable_valid(s) || index >= s->count || s->slots[index].hidden ||
        cui_resizable_next(s, index) >= s->count || !isfinite(step) || step < 0)
        return result;
    cui_resizable_slot *slot = &s->slots[index];
    size_t before_count = 0;
    cui_semantics(ui, &before_count);
    cui_interact(ui, id, CUI_ROLE_SEPARATOR, cui_string("Resize panels"), disabled, false,
                 slot->size);
    size_t after_count = 0;
    const cui_semantic *submitted = cui_semantics(ui, &after_count);
    if (after_count != before_count + 1 || submitted[after_count - 1].id != id) {
        if (s->active_handle == id) {
            s->active_handle = 0;
            s->dragging = false;
        }
        slot->phase = CUI_RESIZE_IDLE;
        return result;
    }
    disabled |= !cui_pointer_scope_allowed(ui, id);
    double grow_available = 0;
    for (size_t j = cui_resizable_next(s, index); j < s->count; j++)
        if (!s->slots[j].hidden)
            grow_available += fmaxf(0, s->slots[j].size - s->slots[j].minimum);
    float reachable_maximum = (float)fmin(slot->maximum, slot->size + grow_available);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){
            .focusable = true,
            .orientation = s->axis == CUI_RESIZABLE_HORIZONTAL ? CUI_ORIENTATION_VERTICAL
                                                               : CUI_ORIENTATION_HORIZONTAL,
            .cursor = s->axis == CUI_RESIZABLE_HORIZONTAL ? CUI_CURSOR_RESIZE_HORIZONTAL
                                                          : CUI_CURSOR_RESIZE_VERTICAL,
            .controls = slot->id,
            .minimum = slot->minimum,
            .maximum = reachable_maximum});
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    bool active = s->active_handle == id;
    if (disabled && active) {
        s->active_handle = 0;
        s->dragging = false;
        active = false;
    }
    for (size_t i = 0; i < count; i++) {
        cui_event e = events[i];
        if (disabled)
            continue;
        if (e.type == CUI_EVENT_POINTER_DOWN && e.pointer_button == CUI_POINTER_PRIMARY &&
            cui_event_target(ui, i) == id) {
            s->active_handle = id;
            s->active_index = index;
            s->dragging = false;
            active = true;
        } else if (active && e.type == CUI_EVENT_POINTER_MOVE && cui_event_target(ui, i) == id) {
            float at = s->axis == CUI_RESIZABLE_HORIZONTAL ? e.x : e.y;
            s->dragging = true;
            float origin = s->axis == CUI_RESIZABLE_HORIZONTAL ? s->bounds.x : s->bounds.y;
            for (size_t j = 0; j < s->count; j++)
                if (!s->slots[j].hidden && s->slots[j].bounds.w > 0 && s->slots[j].bounds.h > 0) {
                    origin = s->axis == CUI_RESIZABLE_HORIZONTAL ? s->slots[j].bounds.x
                                                                 : s->slots[j].bounds.y;
                    break;
                }
            for (size_t j = 0; j < index; j++)
                if (!s->slots[j].hidden)
                    origin += s->slots[j].size;
            result.changed |= cui_resizable_resize_at(s, index, at - origin);
        } else if (active && e.type == CUI_EVENT_POINTER_UP &&
                   e.pointer_button == CUI_POINTER_PRIMARY && cui_event_target(ui, i) == id) {
            s->active_handle = 0;
            s->dragging = false;
            active = false;
            result.completed = true;
            cui_resizable_emit(s);
        } else if (active && (e.type == CUI_EVENT_CANCEL ||
                              (e.type == CUI_EVENT_KEY_DOWN && e.key == CUI_KEY_ESCAPE))) {
            s->active_handle = 0;
            s->dragging = false;
            active = false;
        } else if (!active && e.type == CUI_EVENT_KEY_DOWN && cui_event_target(ui, i) == id) {
            float delta = 0;
            bool resize = false;
            float target = slot->size;
            if (e.key == CUI_KEY_HOME) {
                double available = 0;
                for (size_t j = 0; j <= index; j++)
                    if (!s->slots[j].hidden)
                        available += fmaxf(0, s->slots[j].size - s->slots[j].minimum);
                target = (float)(slot->size - available);
                resize = true;
            } else if (e.key == CUI_KEY_END) {
                target = FLT_MAX;
                resize = true;
            } else if (s->axis == CUI_RESIZABLE_HORIZONTAL &&
                       (e.key == CUI_KEY_LEFT || e.key == CUI_KEY_RIGHT)) {
                delta = e.key == CUI_KEY_LEFT ? -1 : 1;
                resize = true;
            } else if (s->axis == CUI_RESIZABLE_VERTICAL &&
                       (e.key == CUI_KEY_UP || e.key == CUI_KEY_DOWN)) {
                delta = e.key == CUI_KEY_UP ? -1 : 1;
                resize = true;
            }
            if (resize) {
                if (delta)
                    target += delta * (step ? step : 10) * (e.modifiers & CUI_MOD_SHIFT ? 10 : 1);
                result.changed |= cui_resizable_resize_at(s, index, target);
                result.completed = true;
                cui_resizable_emit(s);
            }
        }
    }
    if (active && !cui_pressed(ui, id)) {
        s->active_handle = 0;
        s->dragging = false;
        active = false;
    }
    result.phase = active
                       ? (s->dragging ? CUI_RESIZE_DRAGGING : CUI_RESIZE_PRESSED)
                       : (!disabled && cui_hovered(ui, id) ? CUI_RESIZE_HOVERED : CUI_RESIZE_IDLE);
    slot->phase = result.phase;
    cui_update_semantic(ui, id, false, slot->size);
    return result;
}
