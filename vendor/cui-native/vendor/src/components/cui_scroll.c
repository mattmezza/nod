/* SPDX-License-Identifier: MIT */
#include "cui/cui_scroll.h"
#include "cui/cui_scroll_events.h"
#include <math.h>
static float cui_scroll_component_positive(float value, float fallback) {
    return isfinite(value) && value > 0 ? value : fallback;
}
static cui_id cui_scroll_content_id(cui_id id) {
    return id ^ UINT64_C(0x2ebf107b4533ddf1);
}
static bool cui_scroll_auto_axis(cui_scroll_options o, bool vertical) {
    float size = vertical ? o.content_height : o.content_width;
    return o.auto_measure_content && (vertical ? o.vertical : o.horizontal) &&
           (!isfinite(size) || size <= 0);
}
static cui_scroll_options cui_scroll_measured(cui_context *ui, cui_id id, cui_scroll_options o) {
    if (o.auto_measure_content) {
        cui_rect measured = cui_bounds(ui, cui_scroll_content_id(id));
        if (cui_scroll_auto_axis(o, false))
            o.content_width = measured.w;
        if (cui_scroll_auto_axis(o, true))
            o.content_height = measured.h;
    }
    return o;
}
bool cui_scrollable_begin(cui_context *ui, cui_id id, const cui_theme *t, cui_scroll_state *s,
                          cui_scroll_options o) {
    if (!ui || !id || !t || !s)
        return false;
    o = cui_scroll_resolve(t, o);
    cui_scroll_options measured = cui_scroll_measured(ui, id, o);
    cui_scroll_geometry g = cui_scroll_layout(measured);
    cui_base_scrollable(ui, id, s, measured);
    cui_begin_box(ui, id,
                  (cui_box){.width = cui_scroll_component_positive(o.width, 320),
                            .height = cui_scroll_component_positive(o.height, 240),
                            .background = t->surface});
    cui_begin_box(ui, 0, (cui_box){.width = -1, .height = g.viewport.height, .row = true});
    cui_begin_box(ui, id ^ UINT64_C(0x75b21b4d3cba0f89),
                  (cui_box){.width = g.viewport.width,
                            .height = g.viewport.height,
                            .clip = true,
                            .offset_x = -s->x,
                            .offset_y = -s->y});
    cui_begin_box(
        ui, o.auto_measure_content ? cui_scroll_content_id(id) : 0,
        (cui_box){
            .width = cui_scroll_auto_axis(o, false)
                         ? 0
                         : fmaxf(g.viewport.width,
                                 cui_scroll_component_positive(o.content_width, g.viewport.width)),
            .height = cui_scroll_auto_axis(o, true)
                          ? 0
                          : fmaxf(g.viewport.height, cui_scroll_component_positive(
                                                         o.content_height, g.viewport.height)),
            .row = o.content_row,
            .gap = isfinite(o.gap) && o.gap > 0 ? o.gap : 0});
    return true;
}
void cui_scrollbar(cui_context *ui, cui_id id, const cui_theme *t, const cui_scroll_state *s,
                   cui_scroll_geometry g, cui_scroll_options o, bool vertical) {
    if (!ui || !id || !t || !s)
        return;
    o = cui_scroll_resolve(t, o);
    if (o.hide_scrollbars)
        return;
    float width = cui_scroll_component_positive(o.width, 320);
    float height = cui_scroll_component_positive(o.height, 240);
    float bar = vertical ? width - g.viewport.width : height - g.viewport.height;
    float length = vertical ? g.viewport.height : g.viewport.width;
    float value = vertical ? s->y : s->x;
    float maximum = vertical ? g.maximum_y : g.maximum_x;
    cui_rect thumb = cui_scrollbar_thumb_ex(
        (cui_rect){0, 0, vertical ? bar : length, vertical ? length : bar}, value, length,
        length + maximum, vertical, o.scrollbar_style ? o.scrollbar_style->min_thumb : 0);
    float start = vertical ? thumb.y : thumb.x;
    cui_id bar_id = cui_scrollbar_id(id, vertical);
    const cui_scrollbar_style *style = o.scrollbar_style;
    float opacity = cui_scrollbar_opacity(ui, id, bar_id, style, value,
                                          vertical ? s->dragging_y : s->dragging_x);
    cui_color track = style && style->custom_track_color ? style->track_color : t->border;
    cui_color fill = style && style->custom_thumb_color ? style->thumb_color
                     : o.disabled || maximum <= 0       ? t->border
                     : cui_focused(ui) == bar_id        ? t->focus
                                                        : t->muted;
    if (style && style->no_track)
        track.a = 0;
    cui_begin_box(ui, bar_id,
                  (cui_box){.width = vertical ? bar : length,
                            .height = vertical ? length : bar,
                            .row = !vertical,
                            .background = track,
                            .radius = style && isfinite(style->track_radius)
                                          ? fmaxf(0, style->track_radius)
                                          : 0,
                            .custom_opacity = true,
                            .opacity = opacity});
    if (start > 0) {
        cui_begin_box(ui, 0,
                      (cui_box){.width = vertical ? bar : start, .height = vertical ? start : bar});
        cui_end_box(ui);
    }
    cui_begin_box(ui, 0,
                  (cui_box){.width = thumb.w,
                            .height = thumb.h,
                            .background = fill,
                            .radius = style && isfinite(style->thumb_radius)
                                          ? fmaxf(0, style->thumb_radius)
                                          : 0});
    cui_end_box(ui);
    cui_end_box(ui);
}
void cui_scrollable_end(cui_context *ui, cui_id id, const cui_theme *t, const cui_scroll_state *s,
                        cui_scroll_options o) {
    o = cui_scroll_measured(ui, id, cui_scroll_resolve(t, o));
    cui_scroll_geometry g = cui_scroll_layout(o);
    cui_end_box(ui); /* content */
    cui_end_box(ui); /* viewport */
    if (o.vertical && !o.hide_scrollbars)
        cui_scrollbar(ui, id, t, s, g, o, true);
    cui_end_box(ui); /* body row */
    if (o.horizontal && !o.hide_scrollbars)
        cui_scrollbar(ui, id, t, s, g, o, false);
    cui_end_box(ui); /* root */
}

void cui_scrollable_end_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_scroll_state *s,
                           cui_scroll_options o) {
    if (!ui || !id || !t || !s)
        return;
    o = cui_scroll_measured(ui, id, cui_scroll_resolve(t, o));
    cui_base_scrollable_finish(ui, id, s, o);
    cui_set_box_scroll_offset(ui, id ^ UINT64_C(0x75b21b4d3cba0f89), -s->x, -s->y);
    cui_scrollable_end(ui, id, t, s, o);
}
void cui_scrollbar_overlay_paint(cui_context *ui, cui_id id, const cui_theme *theme, float x,
                                 float y, cui_rect viewport, cui_size content,
                                 cui_scroll_options o) {
    if (!ui || !id || !theme || !isfinite(x) || !isfinite(y) || !isfinite(viewport.x) ||
        !isfinite(viewport.y) || !isfinite(viewport.w) || !isfinite(viewport.h) ||
        viewport.w <= 0 || viewport.h <= 0 || !isfinite(content.width) ||
        !isfinite(content.height) || content.width < 0 || content.height < 0)
        return;
    o = cui_scroll_resolve(theme, o);
    if (o.hide_scrollbars || !o.scrollbar_style ||
        (o.scrollbar_style->visibility != CUI_SCROLLBAR_VISIBLE &&
         o.scrollbar_style->visibility != CUI_SCROLLBAR_AUTO_HIDE))
        return;
    cui_scroll_state *state =
        cui_resource_get(ui, id, UINT64_C(0x30914be927c650a8), sizeof(*state), NULL, NULL);
    if (!state)
        return;
    float bar = fminf(cui_scroll_component_positive(o.bar_thickness, 10),
                      fminf(viewport.w, viewport.h) * .5f);
    cui_scroll_geometry g = {.viewport = {viewport.w, viewport.h},
                             .maximum_x = o.horizontal ? fmaxf(0, content.width - viewport.w) : 0,
                             .maximum_y = o.vertical ? fmaxf(0, content.height - viewport.h) : 0};
    state->x = fminf(fmaxf(0, x), g.maximum_x);
    state->y = fminf(fmaxf(0, y), g.maximum_y);
    o.width = viewport.w + bar;
    o.height = viewport.h + bar;
    if (o.vertical && g.maximum_y > 0) {
        cui_begin_absolute_box(
            ui, 0, (cui_box){0},
            (cui_rect){viewport.x + viewport.w - bar, viewport.y, bar, viewport.h});
        cui_scrollbar(ui, id, theme, state, g, o, true);
        cui_end_box(ui);
    }
    if (o.horizontal && g.maximum_x > 0) {
        cui_begin_absolute_box(
            ui, 0, (cui_box){0},
            (cui_rect){viewport.x, viewport.y + viewport.h - bar, viewport.w, bar});
        cui_scrollbar(ui, id, theme, state, g, o, false);
        cui_end_box(ui);
    }
}
bool cui_scrollbar_overlay(cui_context *ui, cui_id id, const cui_theme *theme, float *x, float *y,
                           cui_rect viewport, cui_size content, cui_scroll_options o) {
    o = cui_scroll_resolve(theme, o);
    bool changed = cui_base_scrollbar_overlay(ui, id, x, y, viewport, content, o);
    if (x && y)
        cui_scrollbar_overlay_paint(ui, id, theme, *x, *y, viewport, content, o);
    return changed;
}

bool cui_scrollable_begin_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                 cui_scroll_state *state, cui_scroll_options options,
                                 cui_scroll_event_scope *scope) {
    if (!ui || !id || !theme || !state || !scope || scope->private_active ||
        !cui_frame_is_submitting(ui))
        return false;
    cui_scroll_event_scope admitted;
    memset(&admitted, 0, sizeof(admitted));
    admitted.private_context = ui;
    admitted.private_state = state;
    admitted.private_owner = id;
    admitted.private_frame = cui_frame_sequence(ui);
    admitted.private_entry = (cui_scroll_offset){state->x, state->y};
    admitted.private_active = admitted.private_styled = true;
    if (!cui_scrollable_begin(ui, id, theme, state, options))
        return false;
    *scope = admitted;
    return true;
}

cui_scroll_event_result cui_scrollable_end_events(cui_context *ui, cui_id id,
                                                  const cui_theme *theme, cui_scroll_state *state,
                                                  cui_scroll_options options,
                                                  cui_scroll_event_scope *scope,
                                                  cui_scroll_handler handler) {
    cui_scroll_event_result result;
    memset(&result, 0, sizeof(result));
    if (!ui || !id || !theme || !state || !scope || !scope->private_active ||
        !scope->private_styled || scope->private_context != ui || scope->private_owner != id ||
        scope->private_state != state || !cui_frame_is_submitting(ui) ||
        scope->private_frame != cui_frame_sequence(ui))
        return result;
    cui_scrollable_end_ex(ui, id, theme, state, options);
    /* Same aggregate policy as Base, after Styled translation/paint/closure.
     * Do not call the Base event finalizer here: it would reduce finish twice. */
    cui_scroll_change change;
    memset(&change, 0, sizeof(change));
    change.has_previous = isfinite(scope->private_entry.x) && isfinite(scope->private_entry.y);
    if (change.has_previous)
        change.previous = scope->private_entry;
    change.value = (cui_scroll_offset){state->x, state->y};
    uint64_t frame = scope->private_frame;
    scope->private_active = false;
    if (!isfinite(change.value.x) || !isfinite(change.value.y))
        return result;
    result.valid = true;
    result.changed = !change.has_previous || change.previous.x != change.value.x ||
                     change.previous.y != change.value.y;
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
