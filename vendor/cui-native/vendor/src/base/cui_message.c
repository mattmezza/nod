/* SPDX-License-Identifier: MIT */
#include "cui/cui_message.h"
#include "cui/cui_message_events.h"
#include "cui/cui_virtual.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static bool cui_message_private_slot_present(cui_message_slot s) {
    return s.present || s.text.length || s.render;
}
static bool cui_message_private_slot_valid(cui_message_slot s) {
    return isfinite(s.width) && s.width >= 0 && isfinite(s.height) && s.height >= 0 &&
           s.inset >= CUI_MESSAGE_INSET_DEFAULT && s.inset <= CUI_MESSAGE_INSET_OFF &&
           (!s.text.length || s.text.data);
}
cui_message_options cui_message_options_default(void) {
    return (cui_message_options){.width = 480, .stack_gap = 10, .row_gap = 8, .metadata_gap = 4};
}
bool cui_base_message(cui_message_options o, cui_message_plan *out) {
    if (!out || o.alignment < CUI_MESSAGE_START || o.alignment > CUI_MESSAGE_END ||
        !isfinite(o.width) || o.width <= 0 || !isfinite(o.stack_gap) || o.stack_gap < 0 ||
        !isfinite(o.row_gap) || o.row_gap < 0 || !isfinite(o.metadata_gap) || o.metadata_gap < 0 ||
        !isfinite(o.max_bubble_width) || o.max_bubble_width < 0 || (o.body_count && !o.body) ||
        o.body_count > 256 || !cui_message_private_slot_valid(o.avatar) ||
        !cui_message_private_slot_valid(o.header) || !cui_message_private_slot_valid(o.footer) ||
        (!o.name.data && o.name.length))
        return false;
    cui_message_plan p = {.avatar = cui_message_private_slot_present(o.avatar),
                          .header = cui_message_private_slot_present(o.header),
                          .footer = cui_message_private_slot_present(o.footer)};
    for (size_t i = 0; i < o.body_count; i++) {
        if (!cui_message_private_slot_valid(o.body[i].slot) ||
            o.body[i].bubble < CUI_BUBBLE_FILLED || o.body[i].bubble > CUI_BUBBLE_ERROR)
            return false;
        p.ghost |= o.body[i].typed_bubble && o.body[i].bubble == CUI_BUBBLE_GHOST;
    }
    p.header_inset = o.header.inset == CUI_MESSAGE_INSET_ON ||
                     (o.header.inset == CUI_MESSAGE_INSET_DEFAULT && !p.ghost);
    p.footer_inset = o.footer.inset == CUI_MESSAGE_INSET_ON ||
                     (o.footer.inset == CUI_MESSAGE_INSET_DEFAULT && !p.ghost);
    p.column_width = fmaxf(0, o.width - (p.avatar ? fmaxf(32, o.avatar.width) + o.row_gap : 0));
    if (o.max_bubble_width > 0)
        p.column_width = fminf(p.column_width, o.max_bubble_width);
    p.footer_offset = p.avatar ? fmaxf(32, o.avatar.width) + o.row_gap : 0;
    *out = p;
    return true;
}
#ifndef CUI_MESSAGE_PRIVATE_STATE_DEFINED
#define CUI_MESSAGE_PRIVATE_STATE_DEFINED
typedef struct {
    uint64_t key;
    double raw_height;
    cui_rect bounds;
    float render_gap;
    bool measured;
} cui_message_private_row;
struct cui_message_scroller_state {
    cui_virtual_list *list;
    uint64_t *keys;
    double *heights;
    cui_message_private_row *rows;
    size_t capacity, count, memory_bytes;
    uint64_t next_key;
    double estimate, scroll;
    cui_message_scroller_metrics metrics;
    bool following;
    cui_scroll_state scrollbar;
    cui_rect viewport;
    float jump_opacity, fade_opacity, jump_start, fade_start, jump_target, fade_target;
    double jump_epoch, fade_epoch;
    bool transition_initialized;
    size_t previous_begin, previous_end;
    float previous_width;
};
#endif
static double cui_message_private_total(cui_message_scroller_state *s) {
    return s->metrics.padding_top + cui_virtual_total(s->list) + s->metrics.padding_bottom;
}
static double cui_message_private_max(cui_message_scroller_state *s) {
    return fmax(0, cui_message_private_total(s) - s->metrics.viewport_height);
}
static void cui_message_private_clamp(cui_message_scroller_state *s) {
    double maximum = cui_message_private_max(s);
    s->scroll = s->following ? maximum : fmin(fmax(s->scroll, 0), maximum);
}
static cui_virtual_anchor cui_message_private_capture(cui_message_scroller_state *s) {
    cui_virtual_anchor anchor =
        cui_virtual_capture_anchor(s->list, fmax(0, s->scroll - s->metrics.padding_top));
    if (anchor.valid && s->scroll < s->metrics.padding_top)
        anchor.within = s->scroll - s->metrics.padding_top;
    return anchor;
}
static double cui_message_private_restore(cui_message_scroller_state *s,
                                          cui_virtual_anchor anchor) {
    size_t index;
    if (!s->count)
        return 0;
    if (!cui_virtual_index(s->list, anchor.key, &index))
        index = anchor.fallback_index < s->count ? anchor.fallback_index : s->count - 1;
    return s->metrics.padding_top + cui_virtual_offset(s->list, index) +
           fmin(anchor.within, cui_virtual_height(s->list, index));
}
static bool cui_message_private_rebuild(cui_message_scroller_state *s) {
    for (size_t i = 0; i < s->count; i++) {
        s->keys[i] = s->rows[i].key;
        s->heights[i] = s->rows[i].raw_height + (i + 1 < s->count ? s->metrics.row_gap : 0);
    }
    return cui_virtual_rebuild(s->list, s->keys, s->heights, s->count) == CUI_VIRTUAL_OK;
}
cui_message_scroller_state *cui_message_scroller_create(cui_message_scroller_config c) {
    if (!c.capacity || c.capacity > 1000000 || c.item_count > c.capacity ||
        !isfinite(c.estimated_height) || c.estimated_height <= 0 || c.estimated_height > 1000000)
        return NULL;
    cui_message_scroller_state *s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->capacity = c.capacity;
    s->estimate = c.estimated_height;
    s->following = true;
    s->metrics =
        (cui_message_scroller_metrics){.padding_top = 8, .padding_bottom = 8, .row_gap = 32};
    s->list = cui_virtual_create((cui_virtual_config){c.capacity, c.estimated_height + 32});
    s->keys = calloc(c.capacity, sizeof(*s->keys));
    s->heights = calloc(c.capacity, sizeof(*s->heights));
    s->rows = calloc(c.capacity, sizeof(*s->rows));
    if (!s->list || !s->keys || !s->heights || !s->rows) {
        cui_message_scroller_destroy(s);
        return NULL;
    }
    s->memory_bytes = sizeof(*s) +
                      c.capacity * (sizeof(*s->keys) + sizeof(*s->heights) + sizeof(*s->rows)) +
                      cui_virtual_memory_bytes(s->list);
    cui_message_scroller_reset(s, c.item_count);
    return s;
}
void cui_message_scroller_destroy(cui_message_scroller_state *s) {
    if (s) {
        cui_virtual_destroy(s->list);
        free(s->keys);
        free(s->heights);
        free(s->rows);
        free(s);
    }
}
size_t cui_message_scroller_count(const cui_message_scroller_state *s) {
    return s ? s->count : 0;
}
size_t cui_message_scroller_memory_bytes(const cui_message_scroller_state *s) {
    return s ? s->memory_bytes : 0;
}
bool cui_message_scroller_following(const cui_message_scroller_state *s) {
    return s && s->following;
}
bool cui_message_scroller_scrolled_up(const cui_message_scroller_state *s) {
    return s && s->metrics.viewport_height > 0 &&
           cui_message_private_total((cui_message_scroller_state *)s) >
               s->metrics.viewport_height &&
           !s->following &&
           s->scroll + 1e-6 < cui_message_private_max((cui_message_scroller_state *)s);
}
bool cui_message_scroller_reset(cui_message_scroller_state *s, size_t count) {
    if (!s || count > s->capacity || count > UINT64_MAX - s->next_key)
        return false;
    s->count = count;
    for (size_t i = 0; i < count; i++)
        s->rows[i] = (cui_message_private_row){.key = ++s->next_key, .raw_height = s->estimate};
    s->following = true;
    s->scroll = 0;
    s->previous_begin = s->previous_end = 0;
    cui_message_private_rebuild(s);
    cui_message_private_clamp(s);
    return true;
}
bool cui_message_scroller_splice(cui_message_scroller_state *s, size_t begin, size_t end,
                                 size_t count) {
    if (!s || begin > end || end > s->count || count > s->capacity - (s->count - (end - begin)) ||
        count > UINT64_MAX - s->next_key)
        return false;
    cui_virtual_anchor anchor = cui_message_private_capture(s);
    size_t next = s->count - (end - begin) + count;
    memmove(s->rows + begin + count, s->rows + end, (s->count - end) * sizeof(*s->rows));
    for (size_t i = 0; i < count; i++)
        s->rows[begin + i] =
            (cui_message_private_row){.key = ++s->next_key, .raw_height = s->estimate};
    s->count = next;
    s->previous_begin = s->previous_end = 0;
    if (s->count) {
        s->rows[s->count - 1].measured = false;
        s->rows[s->count - 1].bounds = (cui_rect){0};
        if (begin && begin - 1 < s->count) {
            s->rows[begin - 1].measured = false;
            s->rows[begin - 1].bounds = (cui_rect){0};
        }
    }
    cui_message_private_rebuild(s);
    if (!s->following && anchor.valid)
        s->scroll = cui_message_private_restore(s, anchor);
    cui_message_private_clamp(s);
    return true;
}
bool cui_message_scroller_append(cui_message_scroller_state *s, size_t count) {
    return s && cui_message_scroller_splice(s, s->count, s->count, count);
}
bool cui_message_scroller_prepend(cui_message_scroller_state *s, size_t count) {
    return cui_message_scroller_splice(s, 0, 0, count);
}
bool cui_message_scroller_remeasure(cui_message_scroller_state *s) {
    if (!s)
        return false;
    double total = cui_message_private_total(s), proportion = total ? s->scroll / total : 0;
    for (size_t i = 0; i < s->count; i++) {
        s->rows[i].raw_height = s->estimate;
        s->rows[i].measured = false;
        s->rows[i].bounds = (cui_rect){0};
    }
    cui_message_private_rebuild(s);
    if (!s->following)
        s->scroll = proportion * cui_message_private_total(s);
    cui_message_private_clamp(s);
    return true;
}
bool cui_message_scroller_remeasure_items(cui_message_scroller_state *s, size_t begin, size_t end) {
    if (!s || begin > end || end > s->count)
        return false;
    for (size_t i = begin; i < end; i++) {
        s->rows[i].measured = false;
        s->rows[i].bounds = (cui_rect){0};
    }
    return true;
}
bool cui_message_scroller_to_item(cui_message_scroller_state *s, size_t index) {
    if (!s || index >= s->count)
        return false;
    s->following = false;
    s->scroll = s->metrics.padding_top + cui_virtual_offset(s->list, index);
    cui_message_private_clamp(s);
    return true;
}
void cui_message_scroller_to_end(cui_message_scroller_state *s) {
    if (s) {
        s->following = true;
        cui_message_private_clamp(s);
    }
}
bool cui_message_scroller_scroll(cui_message_scroller_state *s, double offset) {
    if (!s || !isfinite(offset))
        return false;
    double maximum = cui_message_private_max(s);
    s->scroll = fmax(0, fmin(offset, maximum));
    s->following = s->scroll + 1e-6 >= maximum;
    return true;
}
bool cui_message_scroller_measure(cui_message_scroller_state *s, size_t index, double height) {
    if (!s || index >= s->count || !isfinite(height) || height < 0 || height > 1000000)
        return false;
    cui_virtual_anchor anchor = cui_message_private_capture(s);
    s->rows[index].raw_height = height;
    s->rows[index].measured = true;
    cui_virtual_set_height(s->list, index,
                           height + (index + 1 < s->count ? s->metrics.row_gap : 0));
    if (!s->following && anchor.valid)
        s->scroll = cui_message_private_restore(s, anchor);
    cui_message_private_clamp(s);
    return true;
}
bool cui_message_scroller_layout(cui_message_scroller_state *s, cui_message_scroller_metrics m,
                                 cui_message_scroller_view *out) {
    if (!s || !out || !isfinite(m.viewport_height) || m.viewport_height <= 0 ||
        !isfinite(m.padding_top) || m.padding_top < 0 || !isfinite(m.padding_bottom) ||
        m.padding_bottom < 0 || !isfinite(m.row_gap) || m.row_gap < 0 || m.padding_top > 1000000 ||
        m.padding_bottom > 1000000 || m.row_gap > 1000000)
        return false;
    bool changed = m.row_gap != s->metrics.row_gap || m.padding_top != s->metrics.padding_top;
    cui_virtual_anchor anchor = cui_message_private_capture(s);
    s->metrics = m;
    if (changed) {
        cui_message_private_rebuild(s);
        if (!s->following && anchor.valid)
            s->scroll = cui_message_private_restore(s, anchor);
    }
    cui_message_private_clamp(s);
    double scroll = fmax(0, s->scroll - m.padding_top);
    size_t begin = cui_virtual_lower_bound(s->list, fmax(0, scroll - 400)),
           end = cui_virtual_lower_bound(s->list, scroll + m.viewport_height + 400);
    if (end < s->count)
        end++;
    *out = (cui_message_scroller_view){begin,
                                       end,
                                       s->scroll,
                                       cui_message_private_total(s),
                                       m.padding_top + cui_virtual_offset(s->list, begin),
                                       s->following,
                                       cui_message_scroller_scrolled_up(s)};
    return true;
}
cui_message_scroller_options cui_message_scroller_options_default(void) {
    return (cui_message_scroller_options){.width = 480,
                                          .height = 320,
                                          .inset_left = 12,
                                          .inset_right = 12,
                                          .padding_top = 8,
                                          .padding_bottom = 8,
                                          .row_gap = 32,
                                          .bar_thickness = 10,
                                          .wheel_step = 48,
                                          .scrollbar = true,
                                          .jump_button = true,
                                          .jump_transition = .2,
                                          .jump_label = {"Jump to latest", 14},
                                          .name = {"Conversation", 12},
                                          .jump_width = 32,
                                          .jump_height = 32};
}
cui_id cui_message_scroller_jump_id(cui_id id) {
    return id ^ UINT64_C(0x45fa32b609871ec4);
}
bool cui_message_scroller_validate_options(cui_message_scroller_options o) {
    return isfinite(o.width) && o.width > 0 && isfinite(o.height) && o.height > 0 &&
           isfinite(o.inset_left) && o.inset_left >= 0 && isfinite(o.inset_right) &&
           o.inset_right >= 0 && isfinite(o.padding_top) && o.padding_top >= 0 &&
           isfinite(o.padding_bottom) && o.padding_bottom >= 0 && isfinite(o.row_gap) &&
           o.row_gap >= 0 && isfinite(o.bar_thickness) && o.bar_thickness > 0 &&
           isfinite(o.wheel_step) && o.wheel_step > 0 && isfinite(o.now_seconds) &&
           o.now_seconds >= 0 && isfinite(o.jump_transition) && o.jump_transition >= 0 &&
           isfinite(o.jump_width) && o.jump_width > 0 && isfinite(o.jump_height) &&
           o.jump_height > 0 && (!o.name.length || o.name.data) &&
           (!o.jump_label.length || o.jump_label.data) &&
           cui_message_private_slot_valid(o.jump_content);
}
cui_message_scroller_result cui_base_message_scroller(cui_context *ui, cui_id id,
                                                      cui_message_scroller_state *s,
                                                      cui_message_scroller_options o) {
    cui_message_scroller_result r = {0};
    if (!ui || !id || !s || !cui_message_scroller_validate_options(o))
        return r;
    r.valid = true;
    if (o.scrollbar_style) {
        if (isfinite(o.scrollbar_style->thickness) && o.scrollbar_style->thickness > 0)
            o.bar_thickness = o.scrollbar_style->thickness;
        if (o.scrollbar_style->visibility == CUI_SCROLLBAR_HIDDEN)
            o.scrollbar = false;
    }
    if (!o.scrollbar)
        s->scrollbar.dragging_y = false;
    for (size_t i = s->previous_begin; i < s->previous_end && i < s->count; i++) {
        double height = fmax(0, s->rows[i].bounds.h - s->rows[i].render_gap);
        if (s->rows[i].bounds.h > 0 && height != s->rows[i].raw_height) {
            cui_message_scroller_measure(s, i, height);
            r.changed = true;
        }
    }
    cui_message_scroller_metrics m = {o.height, o.padding_top, o.padding_bottom, o.row_gap};
    if (!cui_message_scroller_layout(s, m, &r.view))
        return r;
    double old = s->scroll;
    cui_scroll_options scroll = {.width = o.width,
                                 .height = o.height,
                                 .content_width = o.width,
                                 .content_height = (float)fmin(r.view.total, FLT_MAX),
                                 .bar_thickness = o.bar_thickness,
                                 .wheel_step = o.wheel_step,
                                 .scrollbar_style = o.scrollbar_style,
                                 .vertical = o.scrollbar};
    cui_scroll_geometry geometry = {
        .viewport = {o.width - (o.scrollbar ? fminf(o.bar_thickness, o.width * .5f) : 0), o.height},
        .maximum_y = (float)fmin(cui_message_private_max(s), FLT_MAX)};
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count; i++)
        if (cui_event_target(ui, i) == id) {
            cui_event e = events[i];
            if (e.type == CUI_EVENT_SCROLL && fabsf(e.dy) >= fabsf(e.dx) && isfinite(e.dy)) {
                double before = s->scroll;
                cui_message_scroller_scroll(s, s->scroll -
                                                   e.dy * (e.scroll_precise ? 1 : o.wheel_step));
                if (s->scroll != before)
                    cui_consume_event(ui, i);
            } else if (e.type == CUI_EVENT_KEY_DOWN && cui_focused(ui) == id) {
                double target = s->scroll;
                switch (e.key) {
                case CUI_KEY_HOME:
                    target = 0;
                    break;
                case CUI_KEY_END:
                    target = cui_message_private_max(s);
                    break;
                case CUI_KEY_UP:
                    target -= o.wheel_step;
                    break;
                case CUI_KEY_DOWN:
                    target += o.wheel_step;
                    break;
                case CUI_KEY_PAGEUP:
                    target -= o.height;
                    break;
                case CUI_KEY_PAGEDOWN:
                    target += o.height;
                    break;
                default:
                    continue;
                }
                cui_message_scroller_scroll(s, target);
            }
        }
    s->scrollbar.y = (float)fmin(s->scroll, FLT_MAX);
    if (o.scrollbar && cui_base_scrollbar(ui, id, &s->scrollbar, geometry, scroll, true)) {
        if (s->scrollbar.y >= geometry.maximum_y)
            cui_message_scroller_to_end(s);
        else
            cui_message_scroller_scroll(s, s->scrollbar.y);
    }
    cui_interact(ui, id, CUI_ROLE_LOG, o.name, false, false, 0);
    cui_set_tab_stop(ui, id, true);
    cui_set_semantic_details(ui, id,
                             (cui_semantic_details){.scrollable = true,
                                                    .orientation = CUI_ORIENTATION_VERTICAL,
                                                    .scroll_axis_lock = true,
                                                    .focusable = true,
                                                    .scroll_y = (float)fmin(s->scroll, FLT_MAX),
                                                    .scroll_max_y = geometry.maximum_y});
    cui_message_scroller_layout(s, m, &r.view);
    r.changed |= old != s->scroll;
    float jump = o.jump_button && r.view.scrolled_up ? 1 : 0,
          fade = o.bottom_fade && r.view.scrolled_up ? 1 : 0;
    float previous_jump = s->jump_opacity, previous_fade = s->fade_opacity;
    if (!s->transition_initialized) {
        s->jump_opacity = s->jump_start = s->jump_target = jump;
        s->fade_opacity = s->fade_start = s->fade_target = fade;
        s->jump_epoch = s->fade_epoch = o.now_seconds;
        s->transition_initialized = true;
    }
    double elapsed = fmax(0, o.now_seconds - s->jump_epoch);
    float phase =
        o.reduced_motion || !o.jump_transition ? 1 : (float)fmin(1, elapsed / o.jump_transition);
    phase = 1 - powf(1 - phase, 3);
    s->jump_opacity = s->jump_start + (s->jump_target - s->jump_start) * phase;
    elapsed = fmax(0, o.now_seconds - s->fade_epoch);
    phase = o.reduced_motion ? 1 : (float)fmin(1, elapsed / .2);
    phase = 1 - powf(1 - phase, 3);
    s->fade_opacity = s->fade_start + (s->fade_target - s->fade_start) * phase;
    if (jump != s->jump_target) {
        s->jump_start = s->jump_opacity;
        s->jump_target = jump;
        s->jump_epoch = o.now_seconds;
    }
    if (fade != s->fade_target) {
        s->fade_start = s->fade_opacity;
        s->fade_target = fade;
        s->fade_epoch = o.now_seconds;
    }
    if (o.reduced_motion || !o.jump_transition)
        s->jump_opacity = jump;
    if (o.reduced_motion)
        s->fade_opacity = fade;
    r.changed |= previous_jump != s->jump_opacity || previous_fade != s->fade_opacity;
    if (s->jump_opacity != jump || s->fade_opacity != fade)
        r.next_wake_seconds = o.now_seconds + 1.0 / 60;
    if (o.jump_button && s->jump_opacity > 0) {
        cui_id jump_id = cui_message_scroller_jump_id(id);
        cui_interaction jump_state =
            cui_interact(ui, jump_id, CUI_ROLE_BUTTON, o.jump_label, !r.view.scrolled_up, false, 0);
        cui_set_semantic_details(ui, jump_id, (cui_semantic_details){.controls = id, .parent = id});
        if (jump_state.activated) {
            cui_message_scroller_to_end(s);
            cui_message_scroller_layout(s, m, &r.view);
            r.jumped = r.changed = true;
            s->jump_start = s->jump_opacity;
            s->fade_start = s->fade_opacity;
            s->jump_target = s->fade_target = 0;
            s->jump_epoch = s->fade_epoch = o.now_seconds;
            if (o.reduced_motion || !o.jump_transition)
                s->jump_opacity = 0;
            if (o.reduced_motion)
                s->fade_opacity = 0;
            r.next_wake_seconds =
                s->jump_opacity > 0 || s->fade_opacity > 0 ? o.now_seconds + 1.0 / 60 : 0;
        }
    }
    r.jump_disabled = !r.view.scrolled_up;
    r.jump_opacity = s->jump_opacity;
    r.fade_opacity = s->fade_opacity;
    if (r.changed)
        cui_invalidate(ui);
    return r;
}

/* Internal read-only snapshot; no layout, allocation or model mutation. */
void cui_message_private_event_snapshot(const cui_message_scroller_state *state, double *offset,
                                        bool *following) {
    *offset = state->scroll;
    *following = state->following;
}

cui_message_scroller_event_result
cui_base_message_scroller_events(cui_context *ui, cui_id id, cui_message_scroller_state *state,
                                 cui_message_scroller_options options,
                                 cui_message_scroller_handler handler) {
    cui_message_scroller_event_result result = {0};
    if (!ui || !id || !state || !cui_frame_is_submitting(ui))
        return result;
    double previous, value;
    bool previous_tail, tail;
    cui_message_private_event_snapshot(state, &previous, &previous_tail);
    uint64_t frame = cui_frame_sequence(ui);
    result.control_result = cui_base_message_scroller(ui, id, state, options);
    cui_message_private_event_snapshot(state, &value, &tail);
    if (!result.control_result.valid || !isfinite(previous) || !isfinite(value))
        return result;
    result.valid = true;
    result.offset_changed = previous != value;
    result.tail_changed = previous_tail != tail;
    cui_message_offset_change offset_change = {.previous = previous, .value = value};
    cui_message_tail_change tail_change = {.previous = previous_tail, .value = tail};
    if (frame) {
        (void)cui_component_event_init(&offset_change.event, CUI_COMPONENT_EVENT_CHANGE, id,
                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
        offset_change.event.flags |= CUI_COMPONENT_EVENT_HAS_FRAME;
        offset_change.event.frame_sequence = frame;
        tail_change.event = offset_change.event;
        if (result.offset_changed && handler.on_offset_change) {
            result.offset_change_count = 1;
            handler.on_offset_change(handler.user, &offset_change);
        }
        if (result.tail_changed && handler.on_tail_change) {
            result.tail_change_count = 1;
            handler.on_tail_change(handler.user, &tail_change);
        }
    }
    return result;
}
