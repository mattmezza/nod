/* SPDX-License-Identifier: MIT */
#include "../base/cui_command_events_private.h"
#include "cui/cui_components.h"
#include "cui/cui_content.h"
#include "cui/cui_icon.h"
#include <math.h>
static uint64_t cui_command_hash(cui_str q) {
    uint64_t h = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < q.length; ++i) {
        h ^= (unsigned char)q.data[i];
        h *= UINT64_C(1099511628211);
    }
    return h;
}
cui_id cui_command_input_id(cui_id id) {
    return id ^ UINT64_C(0x8f43ed519a5610fb);
}
cui_id cui_command_list_id(cui_id id) {
    return id ^ UINT64_C(0x46c36d9947ea5a61);
}
cui_id cui_command_row_id(cui_id id, cui_command_index i) {
    return id ^ UINT64_C(0xc8e5cc5217169bed) ^
           ((uint64_t)i.section * UINT64_C(0x9e3779b97f4a7c15)) ^
           ((uint64_t)i.row * UINT64_C(0xbf58476d1ce4e5b9));
}
void cui_command_focus(cui_context *ui, cui_id id, cui_command_options o) {
    cui_focus(ui, o.searchable ? cui_command_input_id(id) : id);
}
static void cui_command_icon(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                             float size, cui_color color) {
    (void)cui_icon_named(
        ui, id, t, name,
        (cui_icon_options){
            .size = CUI_ICON_CUSTOM, .custom_size = size, .custom_color = true, .color = color});
}
static void cui_command_scroll_selection(cui_command_state *s, float viewport, float total) {
    if (s->pending_scroll) {
        if (s->selected < s->row_count) {
            cui_command_row *r = &s->rows[s->selected];
            if (r->offset < s->scroll.y)
                s->scroll.y = r->offset;
            else if (r->offset + r->height > s->scroll.y + viewport)
                s->scroll.y = r->offset + r->height - viewport;
        } else
            s->scroll.y = 0;
        s->pending_scroll = false;
    }
    s->scroll.y = fminf(fmaxf(s->scroll.y, 0), fmaxf(0, total - viewport));
}
static bool cui_command_geometry(cui_context *ui, cui_command_state *s, float content, float font,
                                 float density, float *out) {
    float total = 4 * density;
    for (size_t i = 0; i < s->row_count; ++i) {
        cui_command_row *r = &s->rows[i];
        float h = r->kind == CUI_COMMAND_ROW_SEPARATOR ? 9 * density
                  : r->kind == CUI_COMMAND_ROW_HEADING ? 28 * density
                                                       : 32 * density;
        if (r->item && r->item->measure)
            h = r->item->measure(content, font, r->item->content_user);
        else if (r->item) {
            float measured = cui_measure_text(ui, r->item->label, font).height;
            h = fmaxf(20 * density, measured) + 12 * density;
        }
        if (!isfinite(h) || h <= 0 || h > 1000000)
            return false;
        r->offset = total;
        r->height = h;
        total += h;
        if (!isfinite(total))
            return false;
    }
    total += 4 * density;
    *out = total;
    return true;
}
static cui_component_event_metadata cui_command_styled_events_frame(cui_context *ui, cui_id id) {
    cui_component_event_metadata event = {0};
    (void)cui_component_event_init(&event, CUI_COMPONENT_EVENT_CHANGE, id,
                                   CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
    event.frame_sequence = cui_frame_sequence(ui);
    if (event.frame_sequence)
        event.flags |= CUI_COMPONENT_EVENT_HAS_FRAME;
    return event;
}
static void cui_command_styled_events_cause(cui_command_events_private_observer *observer,
                                            cui_component_event_metadata cause) {
    if (observer) {
        observer->cause = cause;
        observer->has_cause = true;
        observer->suppress = !(cause.flags & CUI_COMPONENT_EVENT_HAS_FRAME);
    }
}
static cui_component_event_metadata cui_command_styled_events_row(cui_context *ui, cui_id id,
                                                                  cui_id row) {
    cui_component_event_metadata cause = cui_command_styled_events_frame(ui, id);
    size_t count = 0, matches = 0;
    (void)cui_frame_events(ui, &count);
    cui_activation_snapshot snapshot = {0};
    for (size_t i = 0; i < count; ++i)
        if (cui_event_accepted_activation(ui, i) == row) {
            if (++matches == 1)
                (void)cui_event_activation_snapshot(ui, i, &snapshot);
        }
    if (matches == 1 &&
        cui_component_event_from_activation(&cause, CUI_COMPONENT_EVENT_ACTIVATE, &snapshot))
        cause.target = id;
    return cause;
}
static bool cui_command_styled_events_common(cui_context *ui, cui_id id, const cui_theme *t,
                                             cui_command_state *s, const cui_command_entry *entries,
                                             size_t n, cui_command_options o,
                                             cui_command_events_private_observer *observer) {

    if (!ui || !id || !t || !s || !s->query || !isfinite(o.width) || !isfinite(o.max_height) ||
        o.max_height <= 0 || o.width == 0)
        return false;
    cui_str q = cui_edit_get_text(s->query);
    uint64_t hash = cui_command_hash(q);
    bool changed = s->initialized && hash != s->query_hash;
    if (!cui_command_events_private_rebuild(s, entries, n, q, changed, o, observer))
        return false;
    s->query_hash = hash;
    float density = t->font_size / 16;
    if (!isfinite(density) || density <= 0)
        return false;
    float width = o.width > 0 ? o.width : cui_bounds(ui, id).w;
    if (width <= 0)
        width = 380 * density;
    float inset = 4 * density, font = 14 * density,
          content = fmaxf(0, width - 2 * inset - (o.bordered ? 2 : 0) - 10);
    float total;
    if (!cui_command_geometry(ui, s, content, font, density, &total))
        return false;
    float viewport = s->row_count ? fminf(total, o.max_height)
                     : s->loading ? 8 * density
                                  : 76 * density;
    if (viewport > o.max_height)
        viewport = o.max_height;
    cui_command_scroll_selection(s, viewport, total);
    if (!cui_begin_focus_scope(ui, id, false))
        return false;
    (void)cui_interact(ui, id, CUI_ROLE_GROUP, o.label, o.disabled, false, 0);
    cui_set_semantic_details(
        ui, id, (cui_semantic_details){.focusable = !o.searchable, .busy = s->loading});
    cui_begin_box(ui, id,
                  (cui_box){.width = o.width,
                            .padding = o.bordered ? 1 : 0,
                            .inert = o.disabled,
                            .clip = true,
                            .background = t->popover,
                            .border = t->border,
                            .border_width = o.bordered ? 1 : 0,
                            .radius = o.bordered ? t->radius_lg : 0});
    if (o.header)
        o.header(ui, t, s, o.header_user);
    if (o.searchable) {
        cui_begin_box(ui, id ^ UINT64_C(0x701c84),
                      (cui_box){.width = -1,
                                .height = 33 * density,
                                .padding = 0,
                                .row = true,
                                .align_y = CUI_ALIGN_CENTER,
                                .gap = 0,
                                .border = t->border,
                                .custom_border_edges = true,
                                .border_edges = {.bottom = 1}});
        cui_begin_box(ui, id ^ UINT64_C(0x701c88), (cui_box){.width = 12 * density, .height = 1});
        cui_end_box(ui);
        cui_begin_box(
            ui, id ^ UINT64_C(0x701c8a),
            (cui_box){.width = -1, .row = true, .align_y = CUI_ALIGN_CENTER, .gap = 8 * density});
        cui_command_icon(ui, id ^ UINT64_C(0x701c85), t, cui_string("search"), 16 * density,
                         t->muted);
        cui_input_state input = {.behavior = s->input, .scroll_x = s->input_scroll_x};
        cui_input_result result = cui_input_ex(
            ui, cui_command_input_id(id), t, o.label, s->query, &input,
            (cui_input_options){.width = -1,
                                .placeholder = o.placeholder.length
                                                   ? o.placeholder
                                                   : cui_string("Type a command or search..."),
                                .disabled = o.disabled,
                                .clipboard = o.clipboard,
                                .no_appearance = true,
                                .delegate_up_down = true,
                                .custom_padding = true,
                                .padding_x = 0,
                                .padding_y = 0,
                                .hide_focus_ring = true});
        s->input = input.behavior;
        s->input_scroll_x = input.scroll_x;
        if (s->loading)
            (void)cui_icon_spinner(
                ui, id ^ UINT64_C(0x701c86), t,
                (cui_icon_options){.size = CUI_ICON_CUSTOM, .custom_size = 16 * density});
        cui_end_box(ui);
        cui_begin_box(ui, id ^ UINT64_C(0x701c89), (cui_box){.width = 12 * density, .height = 1});
        cui_end_box(ui);
        cui_end_box(ui);
        if (result.changed) {
            cui_command_styled_events_cause(observer, cui_command_styled_events_frame(ui, id));
            q = cui_edit_get_text(s->query);
            if (!cui_command_events_private_rebuild(s, entries, n, q, true, o, observer)) {
                cui_end_box(ui);
                cui_end_focus_scope(ui);
                return false;
            }
            s->query_hash = cui_command_hash(q);
            cui_invalidate(ui);
            if (!cui_command_geometry(ui, s, content, font, density, &total)) {
                cui_end_box(ui);
                cui_end_focus_scope(ui);
                return false;
            }
            viewport = fminf(s->row_count ? total
                             : s->loading ? 8 * density
                                          : 76 * density,
                             o.max_height);
            cui_command_scroll_selection(s, viewport, total);
        }
    }
    cui_id list = cui_command_list_id(id);
    cui_scroll_options scroll = {.width = fmaxf(1, width - (o.bordered ? 2 : 0)),
                                 .height = viewport,
                                 .content_height = s->row_count ? total : viewport,
                                 .vertical = true,
                                 .disabled = o.disabled};
    if (cui_scrollable_begin(ui, list, t, &s->scroll, scroll)) {
        (void)cui_set_semantic_role(ui, list, CUI_ROLE_LISTBOX);
        if (!s->row_count && !s->loading) {
            cui_begin_box(ui, id ^ UINT64_C(0x701c87),
                          (cui_box){.width = -1,
                                    .height = viewport,
                                    .align_x = CUI_ALIGN_CENTER,
                                    .align_y = CUI_ALIGN_CENTER,
                                    .padding = inset});
            if (o.empty)
                o.empty(ui, t, s, o.empty_user);
            else
                cui_text(ui,
                         o.empty_message.length ? o.empty_message : cui_string("No results found."),
                         font, t->muted);
            cui_end_box(ui);
        }
        cui_id hovered = 0;
        size_t position = 0;
        for (size_t i = 0; i < s->row_count; ++i) {
            cui_command_row *r = &s->rows[i];
            if (r->item)
                ++position;
            if (r->offset + r->height < s->scroll.y || r->offset > s->scroll.y + viewport)
                continue;
            cui_id row = r->item ? cui_command_row_id(id, r->index)
                                 : id ^ UINT64_C(0x90cc548000000000) ^ (uint64_t)i;
            bool selected = s->selected == i && r->item && !r->item->disabled;
            cui_interaction interaction = {0};
            cui_component_event_metadata row_cause = {0};
            cui_command_index interaction_index = r->index;
            if (r->item) {
                interaction = cui_interact(ui, row, CUI_ROLE_OPTION, r->item->label,
                                           o.disabled || r->item->disabled, selected, 0);
                cui_set_semantic_details(ui, row,
                                         (cui_semantic_details){.pointer_activatable = true,
                                                                .parent = list,
                                                                .pos_in_set = position,
                                                                .set_size = s->matched_count});
                cui_set_tab_stop(ui, row, false);
                if (observer && interaction.activated)
                    row_cause = cui_command_styled_events_row(ui, id, row);
                if (interaction.hovered && !o.disabled && !r->item->disabled) {
                    hovered = row;
                    if (s->hovered_row != row) {
                        cui_command_styled_events_cause(observer,
                                                        cui_command_styled_events_frame(ui, id));
                        (void)cui_command_events_private_select(s, r->index, false, o, observer);
                    }
                }
                selected = s->selected == i && !r->item->disabled;
            }
            if (r->kind == CUI_COMMAND_ROW_SEPARATOR) {
                cui_begin_absolute_box(ui, row, (cui_box){.align_y = CUI_ALIGN_CENTER},
                                       (cui_rect){inset, r->offset, content, r->height});
                cui_divider(ui, row ^ UINT64_C(0x79d7a00c), t, content, false);
                cui_end_box(ui);
                continue;
            }
            cui_begin_absolute_box(
                ui, row,
                (cui_box){.row = true,
                          .align_y = CUI_ALIGN_CENTER,
                          .gap = 0,
                          .padding = 6 * density,
                          .radius = o.bordered ? fmaxf(0, t->radius_lg - inset - 1) : t->radius,
                          .background = selected ? t->accent : (cui_color){0}},
                (cui_rect){inset, r->offset, content, r->height});
            cui_begin_box(ui, row ^ UINT64_C(0x79d7a008),
                          (cui_box){.width = 2 * density, .height = 1});
            cui_end_box(ui);
            cui_begin_box(
                ui, row ^ UINT64_C(0x79d7a00a),
                (cui_box){
                    .width = -1, .row = true, .gap = 8 * density, .align_y = CUI_ALIGN_CENTER});
            cui_color color = selected                       ? t->accent_foreground
                              : r->item && r->item->disabled ? t->muted
                                                             : t->popover_foreground;
            if (r->kind == CUI_COMMAND_ROW_SEPARATOR)
                cui_divider(ui, row ^ UINT64_C(0x79d7a00c), t, content, false);
            else if (r->kind == CUI_COMMAND_ROW_HEADING)
                cui_text_with_line_height(ui, r->heading, 12 * density, t->muted, 16 * density);
            else if (r->item->content)
                r->item->content(ui, t, s, r->item->content_user);
            else {
                if (r->item->icon.length)
                    cui_command_icon(ui, row ^ 2, t, r->item->icon, 16 * density,
                                     selected ? t->accent_foreground : t->muted);
                cui_begin_box(ui, row ^ 3,
                              (cui_box){.width = -1,
                                        .height = r->height - 12 * density,
                                        .align_y = CUI_ALIGN_CENTER,
                                        .clip = true});
                cui_text_with_line_height(ui, r->item->label, font, color, 20 * density);
                cui_end_box(ui);
            }
            if (r->item) {
                cui_str binding = !r->item->content && r->item->action && r->item->binding
                                      ? r->item->binding(r->item->action_user)
                                      : (cui_str){0};
                if (binding.length)
                    cui_kbd(ui, row ^ 4, t, binding);
                else if (r->item->checked)
                    cui_command_icon(ui, row ^ 5, t, cui_string("check"), 12 * density, color);
            }
            cui_end_box(ui);
            cui_begin_box(ui, row ^ UINT64_C(0x79d7a009),
                          (cui_box){.width = 2 * density, .height = 1});
            cui_end_box(ui);
            cui_end_box(ui);
            if (interaction.activated && !o.disabled) {
                cui_command_styled_events_cause(observer, row_cause);
                (void)cui_command_events_private_select(s, r->index, false, o, observer);
                cui_command_index confirmed;
                if (observer && cui_command_selected(s, &confirmed) &&
                    !cui_command_index_equal(interaction_index, confirmed)) {
                    observer->cause.flags &= ~CUI_COMPONENT_EVENT_HAS_ACTIVATION;
                    memset(&observer->cause.activation, 0, sizeof observer->cause.activation);
                }
                (void)cui_command_events_private_confirm(s, o, observer);
                cui_command_focus(ui, id, o);
            }
        }
        s->hovered_row = hovered;
        cui_scrollable_end_ex(ui, list, t, &s->scroll, scroll);
        cui_set_semantic_details(
            ui, list,
            (cui_semantic_details){.scrollable = !o.disabled,
                                   .orientation = CUI_ORIENTATION_VERTICAL,
                                   .busy = s->loading,
                                   .scroll_y = s->scroll.y,
                                   .scroll_max_y = fmaxf(0, total - viewport)});
    }
    if (o.footer)
        o.footer(ui, t, s, o.footer_user);
    size_t event_count;
    const cui_event *events = cui_frame_events(ui, &event_count);
    if (!o.disabled && cui_focus_within(ui, id))
        for (size_t i = 0; i < event_count; ++i) {
            const cui_event *e = &events[i];
            if (cui_event_consumed(ui, i) || e->type != CUI_EVENT_KEY_DOWN || e->modifiers)
                continue;
            cui_component_event_metadata cause = cui_command_styled_events_frame(ui, id);
            if (cause.flags & CUI_COMPONENT_EVENT_HAS_FRAME) {
                (void)cui_component_event_set_frame_index(&cause, cause.frame_sequence, i);
                cause.source = CUI_COMPONENT_EVENT_SOURCE_KEYBOARD;
                cause.flags |= CUI_COMPONENT_EVENT_HAS_SOURCE;
            }
            cui_command_styled_events_cause(observer, cause);
            if (e->key == CUI_KEY_UP || e->key == CUI_KEY_DOWN) {
                (void)cui_command_events_private_move(s, e->key == CUI_KEY_DOWN, o, observer);
                (void)cui_consume_event(ui, i);
                cui_invalidate(ui);
            } else if (e->key == CUI_KEY_ENTER) {
                (void)cui_command_events_private_confirm(s, o, observer);
                (void)cui_consume_event(ui, i);
            } else if (e->key == CUI_KEY_ESCAPE) {
                q = cui_edit_get_text(s->query);
                if (o.searchable && q.length) {
                    (void)cui_edit_select(s->query, 0, q.length);
                    (void)cui_edit_insert(s->query, cui_string(""));
                    (void)cui_command_events_private_rebuild(
                        s, entries, n, cui_edit_get_text(s->query), true, o, observer);
                    s->query_hash = cui_command_hash(cui_edit_get_text(s->query));
                    (void)cui_consume_event(ui, i);
                    cui_invalidate(ui);
                } else if (o.on_cancel || (observer && observer->handler.on_cancel)) {
                    cui_command_cancel_request request = {cause};
                    request.event.kind = CUI_COMPONENT_EVENT_REQUEST;
                    if (o.on_cancel)
                        o.on_cancel(o.user);
                    if (observer && !observer->suppress && observer->handler.on_cancel) {
                        if (observer->result.cancel_request_count == SIZE_MAX)
                            observer->result.count_overflow = true;
                        else
                            observer->result.cancel_request_count++;
                        observer->handler.on_cancel(observer->handler.user, &request);
                    }
                }
            }
        }
    cui_end_box(ui);
    cui_end_focus_scope(ui);
    return true;
}

bool cui_command(cui_context *ui, cui_id id, const cui_theme *theme, cui_command_state *state,
                 const cui_command_entry *entries, size_t count, cui_command_options options) {
    return cui_command_styled_events_common(ui, id, theme, state, entries, count, options, NULL);
}
cui_command_event_result cui_command_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                            cui_command_state *state,
                                            const cui_command_entry *entries, size_t count,
                                            cui_command_options options,
                                            cui_command_event_handler handler) {
    cui_command_events_private_observer observer = {.target = id, .handler = handler};
    observer.result.control =
        cui_command_styled_events_common(ui, id, theme, state, entries, count, options, &observer);
    return observer.result;
}
