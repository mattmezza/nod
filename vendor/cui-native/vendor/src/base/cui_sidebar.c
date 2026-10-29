/* SPDX-License-Identifier: MIT */
#include "cui/cui_sidebar.h"
#include <float.h>
#include <math.h>
#include <string.h>
cui_id cui_sidebar_part_id(cui_id id, unsigned part) {
    uint64_t h = id ^ (UINT64_C(0x629752dd7d0e5f25) * (part + 1u));
    h ^= h >> 30;
    h *= UINT64_C(0xbf58476d1ce4e5b9);
    h ^= h >> 27;
    h *= UINT64_C(0x94d049bb133111eb);
    h ^= h >> 31;
    return h ? h : 1;
}
bool cui_sidebar_state_init(cui_sidebar_state *s, cui_sidebar_cache *cache, size_t cap) {
    if (!s || !cache || !cap || cap > CUI_SIDEBAR_ENTRY_LIMIT)
        return false;
    memset(s, 0, sizeof(*s));
    memset(cache, 0, cap * sizeof(*cache));
    s->cache = cache;
    s->capacity = cap;
    return true;
}
cui_sidebar_options cui_sidebar_default(void) {
    return (cui_sidebar_options){.duration_seconds = .2f};
}
static float cui_sidebar_ease(float x) {
    return x < .5f ? 4 * x * x * x : 1 - powf(-2 * x + 2, 3) * .5f;
}
static float cui_sidebar_sample(const cui_sidebar_state *s, double now, float duration) {
    if (!s->animating || duration <= 0)
        return s->target_width;
    float t = (float)((now - s->started) / duration);
    t = fmaxf(0, fminf(1, t));
    return s->from_width + (s->target_width - s->from_width) * cui_sidebar_ease(t);
}
bool cui_sidebar_prepare(cui_sidebar_state *s, const cui_sidebar_entry *entries, size_t count,
                         cui_sidebar_options o, float available) {
    if (!s || !s->cache || !s->capacity || s->capacity > CUI_SIDEBAR_ENTRY_LIMIT ||
        count > s->capacity || s->count > s->capacity || (count && !entries) ||
        o.collapse < CUI_SIDEBAR_ICON || o.collapse > CUI_SIDEBAR_NONE ||
        o.side < CUI_SIDEBAR_LEFT || o.side > CUI_SIDEBAR_RIGHT || !isfinite(o.width) ||
        !isfinite(o.height) || o.height < 0 || !isfinite(o.viewport_height) ||
        o.viewport_height < 0 || !isfinite(available) || available < 0 ||
        !isfinite(o.now_seconds) || o.now_seconds < 0 || !isfinite(o.duration_seconds) ||
        o.duration_seconds < 0 || (o.width_percent && (o.width < 0 || o.width > 1)))
        return false;
    double estimated_total = 24;
    for (size_t i = 0; i < count; i++) {
        estimated_total += entries[i].estimated_height;
        if (estimated_total > FLT_MAX)
            return false;
        if (!entries[i].id || !entries[i].render || !isfinite(entries[i].estimated_height) ||
            entries[i].estimated_height <= 0)
            return false;
        for (size_t j = 0; j < i; j++)
            if (entries[j].id == entries[i].id)
                return false;
    }
    bool pixel = o.width >= 0 && !o.width_percent;
    float expanded = o.width_percent ? available * o.width
                     : o.width < 0   ? available
                     : o.width > 0   ? o.width
                                     : 255;
    bool collapsed = o.collapsed && o.collapse != CUI_SIDEBAR_NONE;
    bool icon = collapsed && o.collapse == CUI_SIDEBAR_ICON;
    bool offcanvas = collapsed && o.collapse == CUI_SIDEBAR_OFFCANVAS;
    float child = icon ? 48 : expanded, target = offcanvas ? 0 : child;
    double now = fmax(o.now_seconds, s->last_time);
    float sampled = cui_sidebar_sample(s, now, o.duration_seconds);
    bool change = !s->initialized || s->icon_collapsed != icon || s->target_child_width != child;
    if (s->initialized && s->target_width != target) {
        s->from_width = sampled;
        s->target_width = target;
        s->started = now;
        s->animating =
            pixel && o.collapse != CUI_SIDEBAR_NONE && !o.reduced_motion && o.duration_seconds > 0;
    }
    if (!s->initialized) {
        s->from_width = s->target_width = target;
        s->render_child = !offcanvas;
    }
    if (!offcanvas)
        s->render_child = true;
    if (!pixel || o.collapse == CUI_SIDEBAR_NONE || o.reduced_motion || o.duration_seconds == 0)
        s->animating = false;
    s->width = cui_sidebar_sample(s, now, o.duration_seconds);
    if (s->animating && now - s->started >= o.duration_seconds - FLT_EPSILON) {
        s->animating = false;
        s->width = target;
    }
    if (offcanvas && !s->animating)
        s->render_child = false;
    if (s->count != count)
        s->scroll.y = 0;
    for (size_t i = s->count; i < count; i++)
        s->cache[i] = (cui_sidebar_cache){0};
    if (count > s->count)
        s->count = count;
    for (size_t i = 0; i < s->count; i++) {
        s->cache[i].retained = false;
        for (size_t j = 0; j < count; j++)
            if (s->cache[i].id == entries[j].id)
                s->cache[i].retained = true;
    }
    for (size_t i = 0; i < count; i++) {
        size_t found = i;
        while (found < s->count && s->cache[found].id != entries[i].id)
            found++;
        if (found < s->count) {
            cui_sidebar_cache tmp = s->cache[i];
            s->cache[i] = s->cache[found];
            s->cache[found] = tmp;
        } else {
            size_t unused = i;
            while (unused < s->count && s->cache[unused].retained)
                unused++;
            if (unused < s->count) {
                cui_sidebar_cache tmp = s->cache[i];
                s->cache[i] = s->cache[unused];
                s->cache[unused] = tmp;
            }
            s->cache[i] =
                (cui_sidebar_cache){.id = entries[i].id, .height = entries[i].estimated_height};
        }
        cui_sidebar_cache *c = &s->cache[i];
        if (change || c->revision != entries[i].revision) {
            c->height = entries[i].estimated_height;
            c->measured = false;
            c->bounds = (cui_rect){0};
        } else if (c->bounds.h > 0) {
            c->height = c->bounds.h;
            c->measured = true;
        }
        c->revision = entries[i].revision;
    }
    s->count = count;
    s->last_time = now;
    s->target_child_width = child;
    s->icon_collapsed = icon;
    s->initialized = true;
    return true;
}
float cui_sidebar_content_height(const cui_sidebar_state *s) {
    if (!s || !s->cache || s->count > s->capacity)
        return 0;
    float sum = s->count ? 24 : 0;
    for (size_t i = 0; i < s->count; i++)
        sum += s->cache[i].height;
    return sum;
}
static bool cui_sidebar_string_valid(cui_str s) {
    return !s.length || s.data;
}
static bool cui_sidebar_open(const cui_sidebar_menu_item *item) {
    return item->expanded ? *item->expanded : item->state && item->state->open;
}
static void cui_sidebar_set_open(const cui_sidebar_menu_item *item, bool open) {
    if (!item->expanded && item->state)
        item->state->open = open;
    if (item->on_expand)
        item->on_expand(open, item->user);
}
static cui_sidebar_activation cui_sidebar_event_cause(cui_context *ui, cui_id item, unsigned level,
                                                      size_t index, cui_event_type type) {
    cui_sidebar_activation event;
    memset(&event, 0, sizeof(event));
    event.item = item;
    event.level = level;
    cui_activation_snapshot snapshot;
    if (!cui_event_activation_snapshot(ui, index, &snapshot) ||
        !cui_component_event_from_activation(&event.event, CUI_COMPONENT_EVENT_ACTIVATE,
                                             &snapshot)) {
        cui_component_event_source source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN;
        if (type == CUI_EVENT_POINTER_UP)
            source = CUI_COMPONENT_EVENT_SOURCE_POINTER;
        else if (type == CUI_EVENT_KEY_DOWN || type == CUI_EVENT_KEY_UP)
            source = CUI_COMPONENT_EVENT_SOURCE_KEYBOARD;
        else if (type == CUI_EVENT_ACCESSIBILITY_ACTION)
            source = CUI_COMPONENT_EVENT_SOURCE_ACCESSIBILITY;
        (void)cui_component_event_init(&event.event, CUI_COMPONENT_EVENT_ACTIVATE, item, source);
        (void)cui_component_event_set_frame_index(&event.event, cui_frame_sequence(ui), index);
    }
    event.event.target = item;
    return event;
}
static void cui_sidebar_expand_observed(const cui_sidebar_menu_item *item, bool before, bool next,
                                        cui_sidebar_item_handler handler,
                                        cui_sidebar_item_event_result *observed,
                                        const cui_sidebar_activation *cause) {
    bool controlled = item->expanded != NULL;
    cui_sidebar_set_open(item, next);
    if (!observed)
        return;
    cui_sidebar_expansion_fn callback =
        controlled ? handler.on_expand_request : handler.on_expand_change;
    bool value = controlled ? next : item->state && item->state->open;
    if (!callback || (!controlled && value == before))
        return;
    cui_sidebar_expansion_event event = {cause->event, cause->item, cause->level, before, value};
    event.event.kind = controlled ? CUI_COMPONENT_EVENT_REQUEST : CUI_COMPONENT_EVENT_CHANGE;
    callback(handler.user, &event);
    size_t *count = controlled ? &observed->request_count : &observed->change_count;
    if (*count == SIZE_MAX)
        observed->count_overflow = true;
    else
        (*count)++;
}
static cui_sidebar_item_result cui_sidebar_item_common(cui_context *ui,
                                                       const cui_sidebar_menu_item *item,
                                                       bool collapsed, unsigned level,
                                                       cui_sidebar_item_handler handler,
                                                       cui_sidebar_item_event_result *observed) {
    cui_sidebar_item_result r = {0};
    if (!ui || !item || !item->id || !cui_sidebar_string_valid(item->label) ||
        !cui_sidebar_string_valid(item->accessibility_label) ||
        (item->child_count && (!item->children || !item->state)) || level > CUI_SIDEBAR_DEPTH_LIMIT)
        return r;
    if (observed)
        observed->valid = true;
    if (item->child_count && item->state &&
        (!item->state->initialized || item->state->id != item->id)) {
        item->state->id = item->id;
        item->state->open = item->default_open;
        item->state->initialized = true;
    }
    r.open = item->child_count && cui_sidebar_open(item);
    cui_str name = item->accessibility_label.length ? item->accessibility_label : item->label;
    cui_interaction row =
        cui_interact(ui, item->id, CUI_ROLE_TREEITEM, name, item->disabled, item->active, 0);
    cui_set_semantic_details(
        ui, item->id,
        (cui_semantic_details){.selected = item->active,
                               .expanded = item->child_count && !collapsed && r.open,
                               .level = level,
                               .controls =
                                   item->child_count ? cui_sidebar_part_id(item->id, 1) : 0});
    cui_interaction caret = {0};
    cui_id caret_id = cui_sidebar_part_id(item->id, 0);
    if (item->child_count && !collapsed) {
        caret = cui_interact(ui, caret_id, CUI_ROLE_BUTTON, cui_string("Toggle submenu"),
                             item->disable_caret, false, 0);
    }
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count; i++) {
        cui_id activation = cui_event_activation(ui, i);
        if (row.activated && activation == item->id) {
            cui_sidebar_activation notification;
            bool notify = observed && handler.on_activate;
            bool expansion_observer =
                observed && (handler.on_expand_request || handler.on_expand_change);
            if (notify || expansion_observer)
                notification = cui_sidebar_event_cause(ui, item->id, level, i, events[i].type);
            r.activated = true;
            if (item->child_count && (item->click_to_open || item->click_to_toggle)) {
                bool before = cui_sidebar_open(item);
                bool next = item->click_to_open ? true : !before;
                if (next != before) {
                    r.expansion_changed = true;
                    cui_sidebar_expand_observed(item, before, next, handler, observed,
                                                expansion_observer ? &notification : NULL);
                }
            }
            if (item->on_activate)
                item->on_activate(item->user);
            if (notify) {
                handler.on_activate(handler.user, &notification);
                if (observed->activation_count == SIZE_MAX)
                    observed->count_overflow = true;
                else
                    observed->activation_count++;
            }
        } else if (caret.activated && activation == caret_id) {
            cui_sidebar_activation cause;
            bool expansion_observer =
                observed && (handler.on_expand_request || handler.on_expand_change);
            if (expansion_observer)
                cause = cui_sidebar_event_cause(ui, item->id, level, i, events[i].type);
            bool before = cui_sidebar_open(item);
            r.expansion_changed = true;
            cui_sidebar_expand_observed(item, before, !before, handler, observed,
                                        expansion_observer ? &cause : NULL);
        }
    }
    r.open = item->child_count && cui_sidebar_open(item);
    cui_update_semantic(ui, item->id, item->active, 0);
    cui_set_semantic_details(
        ui, item->id,
        (cui_semantic_details){.selected = item->active,
                               .expanded = item->child_count && !collapsed && r.open,
                               .level = level,
                               .controls =
                                   item->child_count ? cui_sidebar_part_id(item->id, 1) : 0});
    if (item->child_count && !collapsed)
        cui_set_semantic_details(
            ui, caret_id,
            (cui_semantic_details){.expanded = r.open,
                                   .controls = cui_sidebar_part_id(item->id, 1)});
    if (item->expanded)
        r.open = item->child_count && *item->expanded;
    if (r.expansion_changed || r.activated)
        cui_invalidate(ui);
    return r;
}
cui_sidebar_item_result cui_base_sidebar_item(cui_context *ui, const cui_sidebar_menu_item *item,
                                              bool collapsed, unsigned level) {
    return cui_sidebar_item_common(ui, item, collapsed, level, (cui_sidebar_item_handler){0}, NULL);
}
cui_sidebar_item_event_result cui_base_sidebar_item_events(cui_context *ui,
                                                           const cui_sidebar_menu_item *item,
                                                           bool collapsed, unsigned level,
                                                           cui_sidebar_item_handler handler) {
    cui_sidebar_item_event_result result;
    memset(&result, 0, sizeof(result));
    result.control = cui_sidebar_item_common(ui, item, collapsed, level, handler, &result);
    return result;
}
static bool cui_sidebar_toggle_common(cui_context *ui, cui_id id, cui_sidebar_toggle_options o,
                                      cui_sidebar_collapse_handler handler,
                                      cui_sidebar_toggle_event_result *observed) {
    if (!ui || !id || o.side < CUI_SIDEBAR_LEFT || o.side > CUI_SIDEBAR_RIGHT ||
        !cui_sidebar_string_valid(o.accessibility_label))
        return false;
    if (observed)
        observed->valid = true;
    cui_interaction state =
        cui_interact(ui, id, CUI_ROLE_BUTTON, o.accessibility_label, o.disabled, false, 0);
    cui_set_semantic_details(ui, id, (cui_semantic_details){.expanded = !o.collapsed});
    if (state.activated) {
        size_t count = 0;
        const cui_event *events = cui_frame_events(ui, &count);
        bool collapsed = o.collapsed;
        for (size_t i = 0; i < count; i++)
            if (cui_event_activation(ui, i) == id) {
                cui_sidebar_collapse_request request;
                bool notify = observed && handler.on_request;
                if (notify) {
                    cui_sidebar_activation cause =
                        cui_sidebar_event_cause(ui, id, 0, i, events[i].type);
                    request.event = cause.event;
                    request.event.kind = CUI_COMPONENT_EVENT_REQUEST;
                    request.previous = collapsed;
                    request.value = !collapsed;
                }
                collapsed = !collapsed;
                if (o.on_change)
                    o.on_change(collapsed, o.user);
                if (notify) {
                    handler.on_request(handler.user, &request);
                    if (observed->request_count == SIZE_MAX)
                        observed->count_overflow = true;
                    else
                        observed->request_count++;
                }
            }
        cui_invalidate(ui);
    }
    return state.activated;
}
bool cui_base_sidebar_toggle(cui_context *ui, cui_id id, cui_sidebar_toggle_options o) {
    return cui_sidebar_toggle_common(ui, id, o, (cui_sidebar_collapse_handler){0}, NULL);
}
cui_sidebar_toggle_event_result
cui_base_sidebar_toggle_events(cui_context *ui, cui_id id, cui_sidebar_toggle_options o,
                               cui_sidebar_collapse_handler handler) {
    cui_sidebar_toggle_event_result result;
    memset(&result, 0, sizeof(result));
    result.activated = cui_sidebar_toggle_common(ui, id, o, handler, &result);
    return result;
}
