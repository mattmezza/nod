/* SPDX-License-Identifier: MIT */
#include "cui/cui_message.h"
#include "cui/cui_icon.h"
#include "cui/cui_message_events.h"
#include "cui/cui_virtual.h"
#include <float.h>
#include <math.h>
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
static cui_id cui_message_component_child(cui_id id, size_t i) {
    uint64_t x = id + UINT64_C(0x9e3779b97f4a7c15) * (i + 1);
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}
static void cui_message_component_space(cui_context *ui, cui_id id, float width, float height) {
    cui_begin_box(
        ui, id,
        (cui_box){.width = width, .height = height, .fixed_width = true, .fixed_height = true});
    cui_end_box(ui);
}
static void cui_message_component_render_slot(cui_context *ui, cui_id id, cui_message_slot slot,
                                              cui_message_style style, float inset, float gap) {
    float width = slot.width > 0 ? slot.width
                  : slot.render
                      ? style.width
                      : cui_measure_text(ui, slot.text, style.font_size).width + inset * 2;
    width = fminf(style.width, width);
    cui_begin_box(ui, id,
                  (cui_box){.row = true,
                            .width = width,
                            .height = slot.height,
                            .gap = 0,
                            .align_y = CUI_ALIGN_CENTER});
    if (inset)
        cui_message_component_space(ui, cui_message_component_child(id, 0), inset, 0);
    cui_begin_box(
        ui, cui_message_component_child(id, 1),
        (cui_box){.row = true,
                  .width = fmaxf(0, width - 2 * inset),
                  .gap = gap,
                  .align_y = CUI_ALIGN_CENTER,
                  .align_x = style.alignment == CUI_MESSAGE_END ? CUI_ALIGN_END : CUI_ALIGN_START});
    style.width = fmaxf(0, width - 2 * inset);
    if (slot.render)
        slot.render(ui, cui_message_component_child(id, 2), slot.user, style);
    else if (slot.text.length)
        cui_text(ui, slot.text, style.font_size, style.color);
    cui_end_box(ui);
    if (inset)
        cui_message_component_space(ui, cui_message_component_child(id, 3), inset, 0);
    cui_end_box(ui);
}
static void cui_message_component_avatar(cui_context *ui, cui_id id, const cui_theme *t,
                                         cui_message_slot slot, cui_message_alignment alignment) {
    float width = fmaxf(32, slot.width);
    cui_begin_box(ui, id,
                  (cui_box){.width = width,
                            .height = slot.height,
                            .row = true,
                            .align_x = CUI_ALIGN_CENTER,
                            .align_y = CUI_ALIGN_CENTER,
                            .clip = true,
                            .radius = 100000,
                            .background = t->surface});
    cui_message_style style = {width, t->font_size, t->text, alignment};
    if (slot.render)
        slot.render(ui, cui_message_component_child(id, 0), slot.user, style);
    else if (slot.text.length)
        cui_text(ui, slot.text, t->font_size, t->text);
    cui_end_box(ui);
}
bool cui_message(cui_context *ui, cui_id id, const cui_theme *t, cui_message_options o) {
    cui_message_plan p;
    if (!ui || !id || !t || !cui_base_message(o, &p))
        return false;
    cui_align align = o.alignment == CUI_MESSAGE_END ? CUI_ALIGN_END : CUI_ALIGN_START;
    cui_begin_box(ui, id, (cui_box){.width = o.width, .gap = o.stack_gap, .align_x = align});
    if (o.role != CUI_ROLE_NONE) {
        cui_interact(ui, id, o.role, o.name, false, false, 0);
        cui_set_tab_stop(ui, id, false);
    }
    cui_begin_box(ui, cui_message_component_child(id, 0),
                  (cui_box){.width = o.width,
                            .row = true,
                            .gap = o.row_gap,
                            .align_y = CUI_ALIGN_END,
                            .align_x = align});
    if (p.avatar && o.alignment == CUI_MESSAGE_START)
        cui_message_component_avatar(ui, cui_message_component_child(id, 1), t, o.avatar,
                                     o.alignment);
    cui_begin_box(ui, cui_message_component_child(id, 2),
                  (cui_box){.width = p.column_width, .gap = o.stack_gap, .align_x = align});
    cui_message_style metadata = {p.column_width, t->font_size * (12.0f / 14), t->muted,
                                  o.alignment};
    if (p.header)
        cui_message_component_render_slot(ui, cui_message_component_child(id, 3), o.header,
                                          metadata, p.header_inset ? 12 : 0, o.metadata_gap);
    cui_begin_box(ui, cui_message_component_child(id, 4),
                  (cui_box){.width = p.column_width, .gap = o.stack_gap, .align_x = align});
    for (size_t i = 0; i < o.body_count; i++) {
        const cui_message_body *body = &o.body[i];
        cui_id child = cui_message_component_child(id, i + 20);
        cui_message_style style = {p.column_width, t->font_size, t->text, o.alignment};
        if (body->typed_bubble) {
            float width = body->slot.width > 0 ? fminf(body->slot.width, p.column_width)
                          : body->slot.render
                              ? p.column_width
                              : fminf(p.column_width,
                                      cui_measure_text(ui, body->slot.text, t->font_size).width +
                                          (body->bubble == CUI_BUBBLE_GHOST ? 0 : 2 * t->padding));
            cui_begin_bubble(ui, child, t, body->bubble, width);
            style.width = fmaxf(0, width - (body->bubble == CUI_BUBBLE_GHOST ? 0 : 2 * t->padding));
            style.color = body->bubble == CUI_BUBBLE_FILLED ? t->on_primary : t->text;
            if (body->slot.render)
                body->slot.render(ui, cui_message_component_child(child, 0), body->slot.user,
                                  style);
            else if (body->slot.text.length)
                cui_text(ui, body->slot.text, style.font_size, style.color);
            cui_end_bubble(ui);
        } else
            cui_message_component_render_slot(ui, child, body->slot, style, 0, 0);
    }
    cui_end_box(ui);
    cui_end_box(ui);
    if (p.avatar && o.alignment == CUI_MESSAGE_END)
        cui_message_component_avatar(ui, cui_message_component_child(id, 1), t, o.avatar,
                                     o.alignment);
    cui_end_box(ui);
    if (p.footer) {
        cui_begin_box(ui, cui_message_component_child(id, 5),
                      (cui_box){.width = o.width, .row = true, .align_x = align});
        if (p.avatar && o.alignment == CUI_MESSAGE_START)
            cui_message_component_space(ui, cui_message_component_child(id, 6), p.footer_offset, 0);
        cui_message_component_render_slot(ui, cui_message_component_child(id, 7), o.footer,
                                          metadata, p.footer_inset ? 12 : 0, o.metadata_gap);
        if (p.avatar && o.alignment == CUI_MESSAGE_END)
            cui_message_component_space(ui, cui_message_component_child(id, 6), p.footer_offset, 0);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return true;
}
void cui_begin_message_group(cui_context *ui, cui_id id, float width, float gap) {
    if (ui)
        cui_begin_box(ui, id, (cui_box){.width = width, .gap = gap});
}
void cui_end_message_group(cui_context *ui) {
    if (ui)
        cui_end_box(ui);
}
typedef struct {
    cui_message_slot slot;
    cui_message_style style;
    const cui_theme *theme;
    cui_id id;
} cui_message_component_jump;
static void cui_message_component_jump_content(cui_context *ui, void *user) {
    cui_message_component_jump *j = user;
    if (j->slot.render)
        j->slot.render(ui, j->id, j->slot.user, j->style);
    else if (j->slot.text.length)
        cui_text(ui, j->slot.text, j->style.font_size, j->style.color);
    else
        cui_icon_named(ui, j->id, j->theme, cui_string("arrow-down"),
                       (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                          .custom_size = j->style.font_size,
                                          .custom_color = true,
                                          .color = j->style.color});
}
cui_message_scroller_result cui_message_scroller(cui_context *ui, cui_id id, const cui_theme *t,
                                                 cui_message_scroller_state *s,
                                                 cui_message_row_render_fn render, void *user,
                                                 cui_message_scroller_options o) {
    cui_message_scroller_result r = {0};
    if (!ui || !t || !s || !render || !cui_message_scroller_validate_options(o))
        return r;
    cui_scrollbar_style scrollbar_skin = o.scrollbar_style ? *o.scrollbar_style : t->scrollbar;
    scrollbar_skin.reduced_motion |= o.reduced_motion;
    o.scrollbar_style = &scrollbar_skin;
    if (isfinite(o.scrollbar_style->thickness) && o.scrollbar_style->thickness > 0)
        o.bar_thickness = o.scrollbar_style->thickness;
    if (o.scrollbar_style->visibility == CUI_SCROLLBAR_HIDDEN)
        o.scrollbar = false;
    else if (o.scrollbar_style->visibility == CUI_SCROLLBAR_VISIBLE ||
             o.scrollbar_style->visibility == CUI_SCROLLBAR_AUTO_HIDE)
        o.scrollbar = true;
    if (s->previous_width > 0 && s->previous_width != o.width)
        cui_message_scroller_remeasure(s);
    s->previous_width = o.width;
    r = cui_base_message_scroller(ui, id, s, o);
    if (r.view.end > s->count || !isfinite(o.width) || o.width <= 0 || !isfinite(o.height) ||
        o.height <= 0)
        return r;
    float bar = o.scrollbar ? fminf(o.bar_thickness, o.width * .5f) : 0, width = o.width - bar;
    cui_begin_box(ui, id,
                  (cui_box){.width = o.width,
                            .height = o.height,
                            .fixed_width = true,
                            .fixed_height = true,
                            .row = true,
                            .clip = true});
    cui_begin_box(ui, cui_message_component_child(id, 100),
                  (cui_box){.width = width,
                            .height = o.height,
                            .clip = true,
                            .offset_y = (float)(r.view.before - r.view.scroll)});
    for (size_t i = r.view.begin; i < r.view.end; i++) {
        cui_id row = cui_message_component_child(id, (size_t)s->rows[i].key + 200);
        cui_begin_box(ui, row, (cui_box){.width = width});
        cui_track_bounds(ui, row, &s->rows[i].bounds);
        cui_begin_box(ui, cui_message_component_child(row, 0),
                      (cui_box){.row = true, .width = width});
        cui_message_component_space(ui, cui_message_component_child(row, 1), o.inset_left, 0);
        float available = fmaxf(0, width - o.inset_left - o.inset_right);
        cui_begin_box(ui, cui_message_component_child(row, 2), (cui_box){.width = available});
        render(ui, cui_message_component_child(row, 3), i, user, available);
        cui_end_box(ui);
        cui_message_component_space(ui, cui_message_component_child(row, 4), o.inset_right, 0);
        cui_end_box(ui);
        s->rows[i].render_gap = i + 1 < s->count ? o.row_gap : 0;
        if (s->rows[i].render_gap)
            cui_message_component_space(ui, cui_message_component_child(row, 5), 0,
                                        s->rows[i].render_gap);
        cui_end_box(ui);
    }
    s->previous_begin = r.view.begin;
    s->previous_end = r.view.end;
    cui_end_box(ui);
    cui_scroll_options scroll = {.width = o.width,
                                 .height = o.height,
                                 .vertical = o.scrollbar,
                                 .bar_thickness = o.bar_thickness,
                                 .content_height = (float)fmin(r.view.total, FLT_MAX),
                                 .wheel_step = o.wheel_step};
    scroll.scrollbar_style = o.scrollbar_style;
    cui_scroll_geometry geometry = {.viewport = {width, o.height},
                                    .maximum_y = (float)fmax(0, r.view.total - o.height)};
    s->scrollbar.y = (float)s->scroll;
    if (o.scrollbar)
        cui_scrollbar(ui, id, t, &s->scrollbar, geometry, scroll, true);
    cui_track_bounds(ui, id, &s->viewport);
    if (s->viewport.w > 0 && s->viewport.h > 0) {
        if (o.bottom_fade && s->fade_opacity > 0) {
            float height = fminf(48, o.height);
            for (size_t strip = 0; strip < 32; strip++) {
                float y = height * (float)strip / 32, next = height * (float)(strip + 1) / 32;
                cui_color color = o.fade_color;
                color.a = (uint8_t)(color.a * s->fade_opacity * ((float)strip + .5f) / 32);
                cui_begin_portal(
                    ui, cui_message_component_child(id, strip + 500),
                    (cui_box){
                        .width = width, .height = next - y, .background = color, .inert = true},
                    (cui_rect){s->viewport.x, s->viewport.y + o.height - height + y, width,
                               next - y},
                    2);
                cui_end_box(ui);
            }
        }
        if (o.jump_button && s->jump_opacity > 0) {
            float jump_width = fminf(o.jump_width, width),
                  jump_height = fminf(o.jump_height, o.height), bottom = 8 + 8 * s->jump_opacity;
            cui_begin_portal(ui, cui_message_component_child(id, 600),
                             (cui_box){.width = jump_width, .height = jump_height},
                             (cui_rect){s->viewport.x + (width - jump_width) / 2,
                                        s->viewport.y + o.height - bottom - jump_height, jump_width,
                                        jump_height},
                             3);
            cui_theme theme = *t;
            theme.control_height = jump_height;
            theme.radius = 100000;
            theme.surface = t->background;
            theme.border.a = (uint8_t)(theme.border.a * s->jump_opacity);
            theme.text.a = (uint8_t)(theme.text.a * s->jump_opacity);
            theme.background.a = (uint8_t)(theme.background.a * s->jump_opacity);
            cui_message_component_jump jump = {
                .slot = o.jump_content,
                .theme = &theme,
                .style = {jump_width, t->font_size, theme.text, CUI_MESSAGE_START},
                .id = cui_message_component_child(id, 601)};
            cui_begin_box(ui, cui_message_scroller_jump_id(id),
                          (cui_box){.width = jump_width,
                                    .height = jump_height,
                                    .fixed_width = true,
                                    .fixed_height = true,
                                    .radius = 100000,
                                    .background = theme.background,
                                    .border = cui_focused(ui) == cui_message_scroller_jump_id(id)
                                                  ? theme.focus
                                                  : theme.border,
                                    .border_width = 1,
                                    .align_x = CUI_ALIGN_CENTER,
                                    .align_y = CUI_ALIGN_CENTER});
            cui_message_component_jump_content(ui, &jump);
            cui_end_box(ui);
            cui_end_box(ui);
        }
    }
    cui_end_box(ui);
    return r;
}

/* Internal read-only snapshot; no layout, allocation or model mutation. */
void cui_message_private_event_snapshot(const cui_message_scroller_state *, double *, bool *);

cui_message_scroller_event_result
cui_message_scroller_events(cui_context *ui, cui_id id, const cui_theme *theme,
                            cui_message_scroller_state *state, cui_message_row_render_fn render,
                            void *user, cui_message_scroller_options options,
                            cui_message_scroller_handler handler) {
    cui_message_scroller_event_result result = {0};
    if (!ui || !id || !state || !theme || !render || !cui_frame_is_submitting(ui))
        return result;
    double previous, value;
    bool previous_tail, tail;
    cui_message_private_event_snapshot(state, &previous, &previous_tail);
    uint64_t frame = cui_frame_sequence(ui);
    result.control_result = cui_message_scroller(ui, id, theme, state, render, user, options);
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
