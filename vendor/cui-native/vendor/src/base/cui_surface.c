/* SPDX-License-Identifier: MIT */
#include "cui/cui_surface.h"
#include <float.h>
#include <math.h>
static bool cui_popover_float(double value) {
    return isfinite(value) && value >= -FLT_MAX && value <= FLT_MAX;
}
static bool cui_popover_rect(cui_rect r, bool positive) {
    return isfinite(r.x) && isfinite(r.y) && isfinite(r.w) && isfinite(r.h) &&
           (positive ? r.w > 0 && r.h > 0 : r.w >= 0 && r.h >= 0) &&
           cui_popover_float((double)r.x + r.w) && cui_popover_float((double)r.y + r.h);
}
static bool cui_popover_anchor_valid(cui_popover_anchor a) {
    return a >= CUI_POPOVER_TOP_LEFT && a <= CUI_POPOVER_RIGHT_CENTER;
}
static double cui_popover_clamp(double value, double minimum, double maximum) {
    return fmax(minimum, fmin(maximum, value));
}
bool cui_popover_position_resolve(cui_rect trigger, cui_size popup, cui_rect viewport, float margin,
                                  cui_popover_position p, cui_rect *out) {
    if (!out || p.mode != CUI_POPOVER_POSITION_ANCHOR || !cui_popover_anchor_valid(p.anchor) ||
        !cui_popover_rect(trigger, false) || !cui_popover_rect(viewport, true) ||
        !isfinite(popup.width) || !isfinite(popup.height) || popup.width <= 0 ||
        popup.height <= 0 || !isfinite(margin) || margin < 0 || !isfinite(p.reference_rem) ||
        p.reference_rem < 0 || (p.custom_offset && !isfinite(p.offset)))
        return false;
    double rem = p.reference_rem == 0 ? 16 : p.reference_rem;
    double gap = (p.custom_offset ? p.offset : rem * .25) + (p.arrow ? rem * .375 : 0);
    double x = trigger.x, y = trigger.y;
    if (p.anchor <= CUI_POPOVER_BOTTOM_RIGHT) {
        unsigned align = (unsigned)p.anchor % 3;
        x += align == 0   ? 0
             : align == 1 ? ((double)trigger.w - popup.width) / 2
                          : (double)trigger.w - popup.width;
        y += p.anchor <= CUI_POPOVER_TOP_RIGHT ? (double)trigger.h + gap
                                               : -(double)popup.height - gap;
    } else {
        x += p.anchor == CUI_POPOVER_LEFT_CENTER ? (double)trigger.w + gap
                                                 : -(double)popup.width - gap;
        y += ((double)trigger.h - popup.height) / 2;
    }
    double min_x = (double)viewport.x + margin, min_y = (double)viewport.y + margin;
    double max_x = fmax(min_x, (double)viewport.x + viewport.w - margin);
    double max_y = fmax(min_y, (double)viewport.y + viewport.h - margin);
    if (!cui_popover_float(gap) || !cui_popover_float(x) || !cui_popover_float(y) ||
        !cui_popover_float(x + popup.width) || !cui_popover_float(y + popup.height) ||
        !cui_popover_float(min_x) || !cui_popover_float(min_y) || !cui_popover_float(max_x) ||
        !cui_popover_float(max_y))
        return false;
    x = fmax(min_x, fmin(x, max_x - popup.width));
    y = fmax(min_y, fmin(y, max_y - popup.height));
    cui_rect next = {(float)x, (float)y, popup.width, popup.height};
    if (!cui_popover_rect(next, true))
        return false;
    *out = next;
    return true;
}
bool cui_popover_arrow_resolve(cui_rect surface, cui_rect trigger, cui_popover_anchor anchor,
                               float depth, float radius, cui_popover_arrow_geometry *out) {
    if (!out || !cui_popover_anchor_valid(anchor) || !cui_popover_rect(surface, true) ||
        !cui_popover_rect(trigger, false) || !isfinite(depth) || depth < 0 || !isfinite(radius) ||
        radius < 0)
        return false;
    bool horizontal = anchor >= CUI_POPOVER_LEFT_CENTER;
    double start = horizontal ? surface.y : surface.x;
    double span = horizontal ? surface.h : surface.w;
    double target;
    if (horizontal)
        target = (double)trigger.y + trigger.h / 2.0;
    else {
        unsigned align = (unsigned)anchor % 3;
        target = (double)trigger.x + (align == 0 ? 0 : align == 1 ? trigger.w / 2.0 : trigger.w);
    }
    double half = fmin(depth, span * .5);
    double inset = fmin((double)radius + half, span * .5);
    double center = cui_popover_clamp(target, start + inset, start + span - inset);
    double base = horizontal ? surface.x : surface.y;
    bool far = anchor >= CUI_POPOVER_BOTTOM_LEFT && anchor <= CUI_POPOVER_BOTTOM_RIGHT;
    far |= anchor == CUI_POPOVER_RIGHT_CENTER;
    if (far)
        base += horizontal ? surface.w : surface.h;
    double tip = base + (far ? depth : -(double)depth);
    double seam = fmin(1, half);
    double coords[6] = {center - half, base, center, tip, center + half, base};
    double join[4] = {center - half + seam, base - 1, 2 * (half - seam), 2};
    if (horizontal) {
        for (unsigned i = 0; i < 6; i += 2) {
            double swap = coords[i];
            coords[i] = coords[i + 1];
            coords[i + 1] = swap;
        }
        double swap = join[0];
        join[0] = join[1];
        join[1] = swap;
        swap = join[2];
        join[2] = join[3];
        join[3] = swap;
    }
    for (unsigned i = 0; i < 6; ++i)
        if (!cui_popover_float(coords[i]))
            return false;
    for (unsigned i = 0; i < 4; ++i)
        if (!cui_popover_float(join[i]))
            return false;
    cui_popover_arrow_geometry next = {
        .points = {{(float)coords[0], (float)coords[1]},
                   {(float)coords[2], (float)coords[3]},
                   {(float)coords[4], (float)coords[5]}},
        .join = {(float)join[0], (float)join[1], (float)join[2], (float)join[3]},
        .side = horizontal ? (far ? CUI_POPUP_LEFT : CUI_POPUP_RIGHT)
                           : (far ? CUI_POPUP_ABOVE : CUI_POPUP_BELOW)};
    if (!cui_popover_rect(next.join, false))
        return false;
    *out = next;
    return true;
}
bool cui_popover_refinement_valid(cui_popover_appearance appearance, cui_popover_refinement r) {
    if (appearance < CUI_POPOVER_APPEARANCE_LEGACY || appearance > CUI_POPOVER_APPEARANCE_NONE ||
        (r.flags & ~63u) || r.shadow_count > CUI_BOX_SHADOW_MAX)
        return false;
    if (((r.flags & CUI_POPOVER_REFINE_RADIUS) &&
         (!isfinite(r.radius) || r.radius < 0 || r.radius > 4096)) ||
        ((r.flags & CUI_POPOVER_REFINE_PADDING) &&
         (!isfinite(r.padding) || r.padding < 0 || r.padding > 4096)) ||
        ((r.flags & CUI_POPOVER_REFINE_BORDER) &&
         (!isfinite(r.border_width) || r.border_width < 0 || r.border_width > 4096)))
        return false;
    if (r.flags & CUI_POPOVER_REFINE_SHADOWS) {
        float radius = r.flags & CUI_POPOVER_REFINE_RADIUS ? r.radius : 0;
        cui_corner_radii radii = {radius, radius, radius, radius};
        for (size_t i = 0; i < r.shadow_count; ++i) {
            cui_rect bounds;
            if ((r.shadows[i].kind != CUI_BOX_SHADOW_DROP &&
                 r.shadows[i].kind != CUI_BOX_SHADOW_INSET) ||
                !cui_drop_shadow_bounds((cui_drop_shadow_paint){.caster = {0, 0, 1, 1},
                                                                .radii = radii,
                                                                .layer = r.shadows[i].layer},
                                        &bounds))
                return false;
        }
    }
    return true;
}
static bool cui_surface_popover_valid(const cui_surface_state *s, cui_surface_options o) {
    return s->kind != CUI_SURFACE_POPOVER ||
           cui_popover_refinement_valid(o.popover_appearance, o.popover_refinement);
}
static float cui_surface_positive(float value, float fallback) {
    return isfinite(value) && value > 0 ? value : fallback;
}
void cui_surface_init(cui_surface_state *s, cui_id id, cui_surface_kind kind) {
    if (!s)
        return;
    *s = (cui_surface_state){.kind = kind >= CUI_SURFACE_DIALOG && kind <= CUI_SURFACE_POPOVER
                                         ? kind
                                         : CUI_SURFACE_DIALOG};
    cui_overlay_init(&s->overlay, id, s->kind != CUI_SURFACE_POPOVER);
}
void cui_surface_open(cui_context *ui, cui_surface_state *s, cui_id trigger) {
    if (!s)
        return;
    s->sheet_motion_from = s->sheet_closing ? s->sheet_progress : 0;
    s->sheet_progress = s->sheet_motion_from;
    s->sheet_motion_start = cui_frame_time(ui);
    s->sheet_closing = false;
    s->sheet_close_result = CUI_SURFACE_NONE;
    s->result = CUI_SURFACE_NONE;
    s->pending_close = CUI_SURFACE_NONE;
    cui_overlay_open(ui, &s->overlay, trigger);
}
static void cui_surface_finish_close(cui_context *ui, cui_surface_state *s,
                                     cui_surface_result result) {
    if (!ui || !s || !s->overlay.open)
        return;
    s->result = result;
    s->sheet_closing = false;
    s->sheet_progress = 0;
    s->resizing = false;
    cui_dismiss_focus_scope(ui, s->overlay.id, s->overlay.trigger);
    cui_overlay_close(ui, &s->overlay);
    if (s->on_close)
        s->on_close(s->close_user, result);
}
static void cui_surface_block_input(cui_context *ui) {
    size_t count;
    cui_frame_events(ui, &count);
    for (size_t i = 0; i < count; i++)
        cui_consume_event(ui, i);
}
void cui_surface_close(cui_context *ui, cui_surface_state *s, cui_surface_result result) {
    if (!ui || !s || !s->overlay.open || s->sheet_closing)
        return;
    if (s->kind == CUI_SURFACE_SHEET && s->sheet_motion_enabled && s->sheet_progress > 0) {
        s->sheet_closing = true;
        s->sheet_close_result = result;
        s->sheet_motion_from = s->sheet_progress;
        s->sheet_motion_start = cui_frame_time(ui);
        s->resizing = false;
        cui_surface_block_input(ui);
        cui_invalidate(ui);
    } else
        cui_surface_finish_close(ui, s, result);
}
void cui_surface_request_close(cui_context *ui, cui_surface_state *s, cui_surface_result result) {
    if (!ui || !s || !s->overlay.open || s->sheet_closing)
        return;
    s->pending_close = result >= CUI_SURFACE_ACCEPTED && result <= CUI_SURFACE_CLOSED
                           ? result
                           : CUI_SURFACE_CLOSED;
    cui_invalidate(ui);
}
cui_surface_result cui_surface_take_result(cui_surface_state *s) {
    if (!s)
        return CUI_SURFACE_NONE;
    cui_surface_result result = s->result;
    s->result = CUI_SURFACE_NONE;
    return result;
}
cui_id cui_surface_part_id(cui_id id, unsigned part) {
    return id ^ (UINT64_C(0x7e361f4b2da50813) + part * UINT64_C(0x9e3779b97f4a7c15));
}
cui_rect cui_surface_geometry(cui_surface_kind kind, cui_surface_options o) {
    cui_rect v = o.viewport;
    if (!isfinite(v.x) || !isfinite(v.y) || !isfinite(v.w) || !isfinite(v.h) || v.w <= 0 ||
        v.h <= 0)
        return (cui_rect){0};
    float margin = isfinite(o.margin) && o.margin >= 0 ? o.margin : 0;
    margin = fminf(margin, fminf(v.w, v.h) * .49f);
    cui_rect area = {v.x + margin, v.y + margin, v.w - 2 * margin, v.h - 2 * margin};
    float width =
        fminf(area.w, cui_surface_positive(o.width, kind == CUI_SURFACE_SHEET ? 350 : 420));
    float height = fminf(area.h, cui_surface_positive(o.height, 260));
    if (kind == CUI_SURFACE_POPOVER)
        return cui_popup_place(o.anchor, (cui_size){width, height}, area, o.side, 6).bounds;
    if (kind == CUI_SURFACE_SHEET) {
        if (o.edge == CUI_SHEET_TOP || o.edge == CUI_SHEET_BOTTOM)
            return (cui_rect){area.x, o.edge == CUI_SHEET_TOP ? area.y : area.y + area.h - height,
                              area.w, height};
        return (cui_rect){o.edge == CUI_SHEET_LEFT ? area.x : area.x + area.w - width, area.y,
                          width, area.h};
    }
    return (cui_rect){area.x + (area.w - width) / 2, area.y + (area.h - height) / 2, width, height};
}
bool cui_popover_surface_geometry(cui_surface_options o, cui_popover_position p, cui_rect *out) {
    if (!out)
        return false;
    if (p.mode == CUI_POPOVER_POSITION_LEGACY) {
        cui_rect next = cui_surface_geometry(CUI_SURFACE_POPOVER, o);
        if (!cui_popover_rect(next, true))
            return false;
        *out = next;
        return true;
    }
    if (!isfinite(o.width) || !isfinite(o.height))
        return false;
    return cui_popover_position_resolve(
        o.anchor, (cui_size){o.width > 0 ? o.width : 420, o.height > 0 ? o.height : 260},
        o.viewport, o.margin, p, out);
}
static bool cui_surface_vertical_sheet(cui_surface_options o) {
    return o.edge == CUI_SHEET_TOP || o.edge == CUI_SHEET_BOTTOM;
}
static void cui_surface_sheet_limits(cui_surface_options o, float *minimum, float *maximum) {
    cui_surface_options full = o;
    full.width = o.viewport.w;
    full.height = o.viewport.h;
    cui_rect area = cui_surface_geometry(CUI_SURFACE_SHEET, full);
    float extent = cui_surface_vertical_sheet(o) ? area.h : area.w;
    *maximum = fminf(extent, cui_surface_positive(o.sheet_maximum, extent));
    *minimum = fminf(*maximum, cui_surface_positive(o.sheet_minimum, 120));
}
cui_rect cui_surface_current_geometry(const cui_surface_state *s, cui_surface_options o) {
    if (!s)
        return (cui_rect){0};
    if (s->kind == CUI_SURFACE_SHEET && isfinite(s->sheet_size) && s->sheet_size > 0) {
        float minimum, maximum;
        cui_surface_sheet_limits(o, &minimum, &maximum);
        float size = fminf(maximum, fmaxf(minimum, s->sheet_size));
        if (cui_surface_vertical_sheet(o))
            o.height = size;
        else
            o.width = size;
    }
    cui_rect b = cui_surface_geometry(s->kind, o);
    if (s->kind == CUI_SURFACE_SHEET && s->sheet_motion_enabled) {
        float hidden = 1 - fminf(1, fmaxf(0, s->sheet_progress));
        if (o.edge == CUI_SHEET_LEFT)
            b.x -= (b.x + b.w - o.viewport.x) * hidden;
        else if (o.edge == CUI_SHEET_RIGHT)
            b.x += (o.viewport.x + o.viewport.w - b.x) * hidden;
        else if (o.edge == CUI_SHEET_TOP)
            b.y -= (b.y + b.h - o.viewport.y) * hidden;
        else
            b.y += (o.viewport.y + o.viewport.h - b.y) * hidden;
    }
    return b;
}
cui_rect cui_surface_resize_bounds(const cui_surface_state *s, cui_surface_options o) {
    if (!s || s->kind != CUI_SURFACE_SHEET || o.no_sheet_resize || s->sheet_closing ||
        (s->sheet_motion_enabled && s->sheet_progress < 1))
        return (cui_rect){0};
    cui_rect b = cui_surface_current_geometry(s, o);
    if (cui_surface_vertical_sheet(o)) {
        float size = fminf(6, b.h);
        if (o.edge == CUI_SHEET_TOP)
            b.y += b.h - size;
        b.h = size;
    } else {
        float size = fminf(6, b.w);
        if (o.edge == CUI_SHEET_LEFT)
            b.x += b.w - size;
        b.w = size;
    }
    return b;
}
static void cui_surface_resize_event(cui_context *ui, cui_surface_state *s, cui_surface_options o,
                                     cui_event e, size_t i) {
    if (e.type == CUI_EVENT_CANCEL || o.no_sheet_resize || s->kind != CUI_SURFACE_SHEET)
        s->resizing = false;
    if (o.no_sheet_resize || s->kind != CUI_SURFACE_SHEET ||
        cui_event_target(ui, i) != cui_surface_part_id(s->overlay.id, 5))
        return;
    bool vertical = cui_surface_vertical_sheet(o);
    float pointer = vertical ? e.y : e.x;
    float minimum, maximum;
    cui_surface_sheet_limits(o, &minimum, &maximum);
    cui_rect b = cui_surface_current_geometry(s, o);
    float size = vertical ? b.h : b.w;
    float direction = o.edge == CUI_SHEET_RIGHT || o.edge == CUI_SHEET_BOTTOM ? -1 : 1;
    bool change = false;
    if (e.type == CUI_EVENT_POINTER_DOWN && e.pointer_button == CUI_POINTER_PRIMARY &&
        isfinite(pointer)) {
        s->resizing = true;
        s->resize_origin = pointer;
        s->resize_size = size;
    } else if (s->resizing && isfinite(pointer) &&
               (e.type == CUI_EVENT_POINTER_MOVE ||
                (e.type == CUI_EVENT_POINTER_UP && e.pointer_button == CUI_POINTER_PRIMARY))) {
        size = s->resize_size + (pointer - s->resize_origin) * direction;
        change = true;
        if (e.type == CUI_EVENT_POINTER_UP)
            s->resizing = false;
    } else if (e.type == CUI_EVENT_KEY_DOWN) {
        if (e.key == CUI_KEY_HOME)
            size = minimum;
        else if (e.key == CUI_KEY_END)
            size = maximum;
        else if ((!vertical && e.key == CUI_KEY_LEFT) || (vertical && e.key == CUI_KEY_UP))
            size -= 10 * direction;
        else if ((!vertical && e.key == CUI_KEY_RIGHT) || (vertical && e.key == CUI_KEY_DOWN))
            size += 10 * direction;
        else
            return;
        change = true;
    }
    if (change) {
        s->sheet_size = fminf(maximum, fmaxf(minimum, size));
        cui_invalidate(ui);
    }
}
bool cui_base_surface_update(cui_context *ui, cui_surface_state *s, cui_surface_options o) {
    if (!ui || !s || !cui_surface_popover_valid(s, o))
        return false;
    s->sheet_animation_seconds =
        isfinite(o.sheet_animation_seconds) && o.sheet_animation_seconds > 0
            ? fminf(o.sheet_animation_seconds, 10)
            : 0;
    s->sheet_motion_enabled =
        s->kind == CUI_SURFACE_SHEET && s->sheet_animation_seconds > 0 && !o.reduced_motion;
    if (!s->overlay.open)
        return false;
    s->on_close = o.on_close;
    s->close_user = o.close_user;
    if (s->sheet_motion_enabled) {
        double elapsed = fmax(0, cui_frame_time(ui) - s->sheet_motion_start);
        float phase = (float)fmin(1, elapsed / s->sheet_animation_seconds);
        float eased = phase * phase * (3 - 2 * phase);
        float target = s->sheet_closing ? 0 : 1;
        s->sheet_progress = s->sheet_motion_from + (target - s->sheet_motion_from) * eased;
        if (phase < 1)
            cui_invalidate(ui);
        else if (s->sheet_closing) {
            cui_surface_finish_close(ui, s, s->sheet_close_result);
            return true;
        }
    } else {
        s->sheet_progress = 1;
        if (s->sheet_closing) {
            cui_surface_finish_close(ui, s, s->sheet_close_result);
            return true;
        }
    }
    if (s->sheet_closing) {
        cui_surface_block_input(ui);
        return false;
    }
    if (s->pending_close != CUI_SURFACE_NONE) {
        cui_surface_result reason = s->pending_close;
        s->pending_close = CUI_SURFACE_NONE;
        if (!o.decide || o.decide(o.decision_user, reason)) {
            cui_surface_close(ui, s, reason);
            return !s->overlay.open;
        }
    }
    s->overlay.modal = s->kind != CUI_SURFACE_POPOVER || o.modal_popover;
    if (o.no_sheet_resize || s->kind != CUI_SURFACE_SHEET)
        s->resizing = false;
    cui_rect previous = cui_bounds(ui, s->overlay.id);
    if (previous.w > 0 && previous.h > 0)
        cui_overlay_set_bounds(&s->overlay, previous);
    cui_rect viewport = s->body_viewport;
    cui_rect content = s->body_content;
    float previous_maximum = s->body_maximum;
    s->body_maximum = fmaxf(0, content.h - viewport.h);
    s->body_scroll =
        isfinite(s->body_scroll) ? fminf(s->body_maximum, fmaxf(0, s->body_scroll)) : 0;
    if (previous_maximum != s->body_maximum)
        cui_invalidate(ui);
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count && s->overlay.open && !s->sheet_closing; ++i) {
        if (cui_event_consumed(ui, i))
            continue;
        cui_event e = events[i];
        if (!s->sheet_motion_enabled || s->sheet_progress >= 1)
            cui_surface_resize_event(ui, s, o, e, i);
        if (e.type == CUI_EVENT_SCROLL && isfinite(e.dy) &&
            cui_event_target(ui, i) == cui_surface_part_id(s->overlay.id, 6)) {
            s->body_scroll = fminf(s->body_maximum,
                                   fmaxf(0, s->body_scroll - e.dy * (e.scroll_precise ? 1 : 48)));
            cui_invalidate(ui);
        }
        cui_id activation = cui_event_activation(ui, i);
        cui_surface_result reason = CUI_SURFACE_NONE;
        if (s->kind == CUI_SURFACE_ALERT && activation == cui_surface_part_id(s->overlay.id, 2))
            reason = CUI_SURFACE_ACCEPTED;
        else if (s->kind == CUI_SURFACE_ALERT &&
                 activation == cui_surface_part_id(s->overlay.id, 3))
            reason = CUI_SURFACE_CANCELLED;
        else if (!o.hide_close && (s->kind != CUI_SURFACE_ALERT || o.show_alert_close) &&
                 activation == cui_surface_part_id(s->overlay.id, 1))
            reason = CUI_SURFACE_CLOSED;
        else if (!o.no_escape_dismiss && e.type == CUI_EVENT_KEY_DOWN && e.key == CUI_KEY_ESCAPE &&
                 !e.repeat && cui_event_target(ui, i) == s->overlay.id)
            reason = CUI_SURFACE_CANCELLED;
        else if (!o.no_outside_dismiss &&
                 (s->kind != CUI_SURFACE_ALERT || o.alert_outside_dismiss) &&
                 e.type == CUI_EVENT_POINTER_DOWN && e.pointer_button == CUI_POINTER_PRIMARY &&
                 s->overlay.has_bounds && cui_pointer_scope_allowed(ui, s->overlay.id) &&
                 (!s->overlay.modal || cui_event_target(ui, i) == s->overlay.id)) {
            cui_rect b = s->overlay.bounds;
            if (e.x < b.x || e.y < b.y || e.x >= b.x + b.w || e.y >= b.y + b.h)
                reason = CUI_SURFACE_CANCELLED;
        }
        if (reason != CUI_SURFACE_NONE && (!o.decide || o.decide(o.decision_user, reason)))
            cui_surface_close(ui, s, reason);
    }
    return !s->overlay.open;
}
bool cui_base_surface_begin(cui_context *ui, cui_surface_state *s, cui_str title,
                            cui_str description, cui_surface_options o) {
    if (!ui || !s || !cui_surface_popover_valid(s, o))
        return false;
    cui_base_surface_update(ui, s, o);
    if (!s->overlay.open || !cui_overlay_begin(ui, &s->overlay))
        return false;
    cui_interact(ui, s->overlay.id,
                 s->kind == CUI_SURFACE_ALERT ? CUI_ROLE_ALERTDIALOG : CUI_ROLE_DIALOG, title,
                 false, false, 0);
    cui_set_semantic_details(
        ui, s->overlay.id,
        (cui_semantic_details){.description = description,
                               .labelled_by = cui_surface_part_id(s->overlay.id, 4)});
    cui_interact(ui, cui_surface_part_id(s->overlay.id, 4), CUI_ROLE_LABEL, title, false, false, 0);
    return true;
}
void cui_base_surface_end(cui_context *ui) {
    cui_overlay_end(ui);
}

void cui_base_surface_submit_resize(cui_context *ui, const cui_surface_state *s,
                                    cui_surface_options o) {
    if (!ui || !s || !s->overlay.open)
        return;
    if (s->kind == CUI_SURFACE_SHEET && !o.no_sheet_resize) {
        cui_id resize = cui_surface_part_id(s->overlay.id, 5);
        bool vertical = cui_surface_vertical_sheet(o);
        float minimum, maximum;
        cui_surface_sheet_limits(o, &minimum, &maximum);
        cui_rect b = cui_surface_current_geometry(s, o);
        cui_interact(ui, resize, CUI_ROLE_SLIDER,
                     vertical ? cui_string("Sheet height") : cui_string("Sheet width"),
                     maximum <= minimum, false, vertical ? b.h : b.w);
        cui_set_semantic_details(
            ui, resize,
            (cui_semantic_details){.parent = s->overlay.id,
                                   .controls = s->overlay.id,
                                   .minimum = minimum,
                                   .maximum = maximum,
                                   .orientation = vertical ? CUI_ORIENTATION_VERTICAL
                                                           : CUI_ORIENTATION_HORIZONTAL});
    }
}

void cui_base_surface_submit_body(cui_context *ui, cui_surface_state *s) {
    if (!ui || !s || !s->overlay.open)
        return;
    cui_id body = cui_surface_part_id(s->overlay.id, 6);
    cui_track_bounds(ui, body, &s->body_viewport);
    cui_track_bounds(ui, cui_surface_part_id(s->overlay.id, 7), &s->body_content);
    cui_interact(ui, body, CUI_ROLE_GROUP, cui_string("Surface content"), false, false, 0);
    cui_set_semantic_details(ui, body,
                             (cui_semantic_details){.parent = s->overlay.id,
                                                    .scrollable = true,
                                                    .scroll_y = s->body_scroll,
                                                    .scroll_max_y = s->body_maximum});
}
