/* SPDX-License-Identifier: MIT */
#include "cui/cui_virtual_events.h"
#include "cui/cui_virtual_scroll.h"
#include <limits.h>
#include <math.h>
cui_virtual_widget_result cui_virtual_list_draw(cui_context *ui, cui_id id, const cui_theme *t,
                                                const cui_virtual_list *v,
                                                cui_virtual_widget_state *s,
                                                cui_virtual_widget_options o,
                                                cui_virtual_render_item_fn render, void *user) {
    cui_virtual_widget_result out = {.error = CUI_VIRTUAL_INVALID};
    cui_scroll_options scroll;
    if (t)
        o.scroll = cui_scroll_resolve(t, o.scroll);
    if (!t || !render || !cui_virtual_widget_scroll_options(v, o, &scroll))
        return out;
    if (!ui || !id || !s)
        return out;
    if (o.measure && cui_virtual_count(v)) {
        size_t index =
            o.measure_index < cui_virtual_count(v) ? o.measure_index : cui_virtual_count(v) - 1;
        uint64_t key;
        cui_size measured = {0};
        if (!cui_virtual_key(v, index, &key) ||
            !o.measure(o.measure_user, index, key, o.horizontal ? 0 : s->last_viewport.width,
                       &measured) ||
            !isfinite(measured.width) || measured.width < 0 || !isfinite(measured.height) ||
            measured.height < 0)
            return out;
        if (o.horizontal && o.scroll.content_height == 0)
            o.scroll.content_height = measured.height;
        if (!o.horizontal && o.scroll.content_width == 0)
            o.scroll.content_width = measured.width;
        if (!cui_virtual_widget_scroll_options(v, o, &scroll))
            return out;
    }
    o.scroll = cui_scroll_resolve(t, o.scroll);
    scroll = cui_scroll_resolve(t, scroll);
    out = cui_base_virtual_list(ui, id, v, s, o);
    if (out.error != CUI_VIRTUAL_OK)
        return out;
    cui_scroll_geometry g = cui_scroll_layout(scroll);
    float old_x = s->scroll.x, old_y = s->scroll.y;
    float width = scroll.width > 0 ? scroll.width : 320;
    float height = scroll.height > 0 ? scroll.height : 240;
    float cross = o.horizontal ? fmaxf(g.viewport.height, scroll.content_height)
                               : fmaxf(g.viewport.width, scroll.content_width);
    cui_id viewport = id ^ UINT64_C(0x1f7e83240b695da3);
    cui_id window = id ^ UINT64_C(0x692adbfd1074e385);
    cui_begin_box(ui, id, (cui_box){.width = width, .height = height, .background = t->surface});
    cui_begin_box(ui, 0, (cui_box){.width = -1, .height = g.viewport.height, .row = true});
    cui_begin_box(ui, viewport,
                  (cui_box){.width = g.viewport.width,
                            .height = g.viewport.height,
                            .clip = true,
                            .offset_x = o.horizontal ? 0 : -s->scroll.x,
                            .offset_y = o.horizontal ? -s->scroll.y : 0});
    cui_begin_box(ui, window,
                  (cui_box){.width = o.horizontal ? g.viewport.width : cross,
                            .clip = true,
                            .height = o.horizontal ? cross : g.viewport.height});
    size_t count = cui_virtual_count(v);
    for (size_t i = out.view.begin; i < out.view.end; ++i) {
        uint64_t key;
        if (!cui_virtual_key(v, i, &key))
            break;
        cui_id item = cui_virtual_item_id(id, key);
        float extent = (float)cui_virtual_height(v, i);
        /* Subtract in double before converting to renderer coordinates. */
        float offset = (float)(cui_virtual_offset(v, i) - out.view.scroll);
        cui_size size = o.horizontal ? (cui_size){extent, cross} : (cui_size){cross, extent};
        cui_begin_absolute_box(ui, item, (cui_box){.clip = true},
                               (cui_rect){o.horizontal ? offset : 0, o.horizontal ? 0 : offset,
                                          size.width, size.height});
        if (!o.no_item_semantics) {
            cui_interact(ui, item, CUI_ROLE_LISTITEM, (cui_str){0}, scroll.disabled, false, 0);
            cui_set_semantic_details(
                ui, item,
                (cui_semantic_details){.parent = id,
                                       .pos_in_set = i >= UINT_MAX ? UINT_MAX : (unsigned)i + 1,
                                       .set_size = count > UINT_MAX ? UINT_MAX : (unsigned)count});
        }
        render(ui, t, user, i, key, item, size);
        cui_end_box(ui);
    }
    cui_end_box(ui); /* window */
    cui_end_box(ui); /* viewport */
    bool deferred = cui_base_scrollable_finish(ui, id, &s->scroll, scroll);
    if (deferred) {
        cui_set_box_scroll_offset(ui, viewport, o.horizontal ? old_x - s->scroll.x : -s->scroll.x,
                                  o.horizontal ? -s->scroll.y : old_y - s->scroll.y);
        out.scroll_changed = true;
        out.window_pending = o.horizontal ? old_x != s->scroll.x : old_y != s->scroll.y;
        if (out.window_pending)
            cui_invalidate(ui);
    }
    size_t semantic_count;
    const cui_semantic *sem = cui_semantics(ui, &semantic_count);
    for (size_t i = 0; i < semantic_count; ++i)
        if (sem[i].id == id) {
            cui_semantic_details details = sem[i].details;
            details.focusable = !scroll.disabled;
            cui_set_semantic_details(ui, id, details);
            break;
        }
    if (scroll.vertical && !scroll.hide_scrollbars)
        cui_scrollbar(ui, id, t, &s->scroll, g, scroll, true);
    cui_end_box(ui); /* body */
    if (scroll.horizontal && !scroll.hide_scrollbars)
        cui_scrollbar(ui, id, t, &s->scroll, g, scroll, false);
    cui_end_box(ui); /* root */
    return out;
}

cui_virtual_event_result
cui_virtual_list_draw_events(cui_context *ui, cui_id id, const cui_theme *theme,
                             const cui_virtual_list *model, cui_virtual_widget_state *state,
                             cui_virtual_widget_options options, cui_virtual_render_item_fn render,
                             void *user, cui_scroll_handler handler) {
    cui_virtual_event_result result = {.control_result = {.error = CUI_VIRTUAL_INVALID}};
    if (!ui || !id || !model || !state || !theme || !render || !cui_frame_is_submitting(ui))
        return result;
    cui_scroll_offset previous = {state->scroll.x, state->scroll.y};
    uint64_t frame = cui_frame_sequence(ui);
    result.control_result =
        cui_virtual_list_draw(ui, id, theme, model, state, options, render, user);
    if (result.control_result.error != CUI_VIRTUAL_OK || !isfinite(state->scroll.x) ||
        !isfinite(state->scroll.y))
        return result;
    cui_scroll_change change = {0};
    change.has_previous = isfinite(previous.x) && isfinite(previous.y);
    if (change.has_previous)
        change.previous = previous;
    change.value = (cui_scroll_offset){state->scroll.x, state->scroll.y};
    result.valid = true;
    result.changed =
        !change.has_previous || previous.x != change.value.x || previous.y != change.value.y;
    if (result.changed && frame && handler.on_change) {
        (void)cui_component_event_init(&change.event, CUI_COMPONENT_EVENT_CHANGE, id,
                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
        change.event.flags |= CUI_COMPONENT_EVENT_HAS_FRAME;
        change.event.frame_sequence = frame;
        result.change_count = 1;
        handler.on_change(handler.user, &change);
    }
    return result;
}
