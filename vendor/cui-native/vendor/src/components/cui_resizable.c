/* SPDX-License-Identifier: MIT */
#include "cui/cui_resizable.h"
#include <math.h>
static double cui_resizable_easing(double t) {
    /* Frozen default motion curve cubic-bezier(.2,0,0,1), invert x by bisection. */
    double lo = 0, hi = 1;
    for (unsigned i = 0; i < 32; i++) {
        double u = (lo + hi) / 2, v = 1 - u, x = 3 * v * v * u * .2 + u * u * u;
        if (x < t)
            lo = u;
        else
            hi = u;
    }
    double u = (lo + hi) / 2;
    return 3 * (1 - u) * u * u + u * u * u;
}
static bool cui_resizable_indicator(cui_resizable_slot *slot, cui_resizable_options o) {
    static const double lengths[] = {0, 20, 28, 44}, opacities[] = {0, .35, .6, .9};
    double length = lengths[slot->phase], opacity = opacities[slot->phase];
    double duration = o.reduced_motion ? 0 : o.duration_seconds;
    if (!slot->indicator_initialized) {
        slot->indicator_length = slot->target_length = length;
        slot->indicator_opacity = slot->target_opacity = opacity;
        slot->transition_start = o.now_seconds;
        slot->indicator_initialized = true;
        return false;
    }
    double elapsed = fmax(0, o.now_seconds - slot->transition_start);
    double t = duration > 0 ? fmin(1, elapsed / duration) : 1;
    double easing = cui_resizable_easing(t);
    slot->indicator_length =
        slot->start_length + (slot->target_length - slot->start_length) * easing;
    slot->indicator_opacity =
        slot->start_opacity + (slot->target_opacity - slot->start_opacity) * easing;
    if (slot->target_length != length || slot->target_opacity != opacity) {
        slot->start_length = slot->indicator_length;
        slot->start_opacity = slot->indicator_opacity;
        slot->target_length = length;
        slot->target_opacity = opacity;
        slot->transition_start = o.now_seconds;
        t = duration > 0 ? 0 : 1;
    }
    if (t >= 1) {
        slot->indicator_length = length;
        slot->indicator_opacity = opacity;
        return false;
    }
    return true;
}
void cui_resize_handle_appearance(cui_context *ui, const cui_theme *theme, cui_resizable_axis axis,
                                  cui_resize_edge edge, double length, double opacity) {
    if (!ui || !theme || axis < CUI_RESIZABLE_HORIZONTAL || axis > CUI_RESIZABLE_VERTICAL ||
        edge < CUI_RESIZE_STRADDLE || edge > CUI_RESIZE_TRAILING || !isfinite(length) ||
        length < 0 || !isfinite(opacity) || opacity < 0 || opacity > 1)
        return;
    float seam = edge == CUI_RESIZE_LEADING ? 0 : 4;
    cui_rect line =
        axis == CUI_RESIZABLE_HORIZONTAL ? (cui_rect){seam, 0, 1, -1} : (cui_rect){0, seam, -1, 1};
    cui_begin_absolute_box(ui, 0, (cui_box){.background = theme->border}, line);
    cui_end_box(ui);
    if (length <= .5)
        return;
    cui_color color = theme->muted;
    color.a = (unsigned char)lround(color.a * opacity);
    cui_rect wrapper = axis == CUI_RESIZABLE_HORIZONTAL ? (cui_rect){seam - 1, 0, 3, -1}
                                                        : (cui_rect){0, seam - 1, -1, 3};
    cui_begin_absolute_box(ui, 0,
                           (cui_box){.row = axis == CUI_RESIZABLE_VERTICAL,
                                     .align_x = CUI_ALIGN_CENTER,
                                     .align_y = CUI_ALIGN_CENTER},
                           wrapper);
    cui_begin_box(ui, 0,
                  (cui_box){.width = axis == CUI_RESIZABLE_HORIZONTAL ? 3 : (float)length,
                            .height = axis == CUI_RESIZABLE_HORIZONTAL ? (float)length : 3,
                            .background = color,
                            .radius = (float)length});
    cui_end_box(ui);
    cui_end_box(ui);
}
cui_resizable_options cui_resizable_options_default(void) {
    return (cui_resizable_options){.duration_seconds = .12, .key_step = 10};
}
cui_resizable_error cui_resizable(cui_context *ui, cui_id id, const cui_theme *theme,
                                  cui_resizable_state *state, const cui_resizable_panel *panels,
                                  size_t count, cui_resizable_options o) {
    if (!ui || !id || !theme || !state || !isfinite(o.extent) || o.extent < 0 ||
        !isfinite(o.cross_size) || o.cross_size < 0 || !isfinite(o.now_seconds) ||
        o.now_seconds < 0 || !isfinite(o.duration_seconds) || o.duration_seconds < 0 ||
        !isfinite(o.key_step) || o.key_step < 0)
        return CUI_RESIZABLE_INVALID;
    if (!state->slots || !state->capacity || state->capacity > CUI_RESIZABLE_PANEL_LIMIT ||
        state->count > state->capacity || (count && !panels) || o.axis < CUI_RESIZABLE_HORIZONTAL ||
        o.axis > CUI_RESIZABLE_VERTICAL)
        return CUI_RESIZABLE_INVALID;
    if (count > state->capacity)
        return CUI_RESIZABLE_CAPACITY;
    for (size_t i = 0; i < count; i++) {
        if (panels[i].id == id || !cui_resizable_panel_valid(panels[i]))
            return CUI_RESIZABLE_INVALID;
        for (size_t j = 0; j < i; j++)
            if (panels[i].id == panels[j].id)
                return CUI_RESIZABLE_INVALID;
    }
    for (size_t i = 0; i < count; i++) {
        if (panels[i].hidden)
            continue;
        size_t next = i + 1;
        while (next < count && panels[next].hidden)
            next++;
        if (next == count)
            continue;
        cui_id handle = cui_resizable_handle_id(id, panels[i].id);
        if (handle == id)
            return CUI_RESIZABLE_INVALID;
        for (size_t j = 0; j < count; j++) {
            if (handle == panels[j].id)
                return CUI_RESIZABLE_INVALID;
            if (j < i && !panels[j].hidden && handle == cui_resizable_handle_id(id, panels[j].id))
                return CUI_RESIZABLE_INVALID;
        }
    }
    float padding = isfinite(o.box.padding) && o.box.padding > 0 ? o.box.padding : 0;
    float declared = o.axis == CUI_RESIZABLE_HORIZONTAL ? o.box.width : o.box.height;
    bool fixed_zero = o.axis == CUI_RESIZABLE_HORIZONTAL ? o.box.fixed_width : o.box.fixed_height;
    float measured = o.axis == CUI_RESIZABLE_HORIZONTAL ? state->bounds.w : state->bounds.h;
    float outer_extent = o.extent > 0 ? o.extent : declared > 0 ? declared : measured;
    bool first_measure = outer_extent <= 0 && !fixed_zero;
    float extent = fmaxf(0, outer_extent - padding * 2);
    if (first_measure) {
        for (size_t i = 0; i < count; i++)
            if (!panels[i].hidden)
                extent += panels[i].has_initial_size ? panels[i].initial_size
                          : panels[i].custom_range   ? panels[i].minimum
                                                     : 100;
    } else if (state->awaiting_measure && state->slots && state->count == count) {
        for (size_t i = 0; i < count; i++) {
            float actual = o.axis == CUI_RESIZABLE_HORIZONTAL ? state->slots[i].bounds.w
                                                              : state->slots[i].bounds.h;
            if (state->slots[i].id == panels[i].id && actual > 0)
                state->slots[i].size = actual;
        }
    }
    cui_resizable_error error = cui_resizable_sync(state, panels, count, o.axis, extent);
    if (error != CUI_RESIZABLE_OK && error != CUI_RESIZABLE_INFEASIBLE)
        return error;
    state->awaiting_measure = first_measure;
    cui_interact(ui, id, CUI_ROLE_GROUP, cui_string("Resizable panels"), false, false, 0);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.orientation = o.axis == CUI_RESIZABLE_HORIZONTAL
                                                  ? CUI_ORIENTATION_HORIZONTAL
                                                  : CUI_ORIENTATION_VERTICAL});
    cui_box group = o.box;
    group.row = o.axis == CUI_RESIZABLE_HORIZONTAL;
    group.gap = 0;
    if (o.axis == CUI_RESIZABLE_HORIZONTAL) {
        group.width = o.extent > 0 ? o.extent : group.width || group.fixed_width ? group.width : -1;
        group.height = o.cross_size > 0                     ? o.cross_size
                       : group.height || group.fixed_height ? group.height
                                                            : -1;
    } else {
        group.height = o.extent > 0                         ? o.extent
                       : group.height || group.fixed_height ? group.height
                                                            : -1;
        group.width = o.cross_size > 0                   ? o.cross_size
                      : group.width || group.fixed_width ? group.width
                                                         : -1;
    }
    cui_begin_box(ui, id, group);
    cui_track_bounds(ui, id, &state->bounds);
    /* Resolve all input before painting panels so callbacks see/paint current sizes. */
    for (size_t i = 0; i < count; i++) {
        if (state->slots[i].hidden)
            continue;
        size_t next = i + 1;
        while (next < count && state->slots[next].hidden)
            next++;
        if (next < count)
            cui_base_resize_handle(
                ui, cui_resizable_handle_id(id, panels[i].id), state, i,
                o.disabled || panels[next].box.inert || panels[next].box.layout_only, o.key_step);
    }

    size_t preceding = count;
    for (size_t i = 0; i < count; i++) {
        cui_resizable_slot *slot = &state->slots[i];
        cui_track_bounds(ui, slot->id, &slot->bounds);
        if (slot->hidden)
            continue;
        cui_box box = panels[i].box;
        float size = first_measure && !panels[i].has_initial_size ? -1 : slot->size;
        if (o.axis == CUI_RESIZABLE_HORIZONTAL) {
            box.width = size;
            box.fixed_width = size == 0;
            box.max_width = slot->maximum;
            if (!box.height && !box.fixed_height)
                box.height = -1;
        } else {
            box.height = size;
            box.fixed_height = size == 0;
            box.max_height = slot->maximum;
            if (!box.width && !box.fixed_width)
                box.width = -1;
        }
        cui_interact(ui, slot->id, CUI_ROLE_GROUP, panels[i].accessibility_label, false, false, 0);
        cui_begin_box(ui, slot->id, box);
        cui_begin_box(ui, 0,
                      (cui_box){.width = o.axis == CUI_RESIZABLE_HORIZONTAL ? slot->minimum : 0,
                                .height = o.axis == CUI_RESIZABLE_VERTICAL ? slot->minimum : 0,
                                .fixed_width = o.axis == CUI_RESIZABLE_VERTICAL,
                                .fixed_height = o.axis == CUI_RESIZABLE_HORIZONTAL});
        cui_end_box(ui);
        if (panels[i].content) {
            cui_begin_absolute_box(ui, 0, (cui_box){.row = box.row, .gap = box.gap},
                                   (cui_rect){0, 0, -1, -1});
            panels[i].content(ui, panels[i].content_user);
            cui_end_box(ui);
        }
        if (preceding < count) {
            cui_resizable_slot *handle_slot = &state->slots[preceding];
            cui_id handle = cui_resizable_handle_id(id, handle_slot->id);
            float padding = isfinite(box.padding) && box.padding > 0 ? box.padding : 0;
            float cross = -1;
            cui_rect band = o.axis == CUI_RESIZABLE_HORIZONTAL
                                ? (cui_rect){-4 - padding, -padding, 9, cross}
                                : (cui_rect){-padding, -4 - padding, cross, 9};
            cui_begin_absolute_box(ui, handle, (cui_box){0}, band);
            if (cui_resizable_indicator(handle_slot, o))
                cui_invalidate(ui);
            bool custom =
                o.appearance && o.appearance(ui, handle, o.axis, handle_slot->phase, band,
                                             handle_slot->indicator_length,
                                             handle_slot->indicator_opacity, o.appearance_user);
            if (!custom)
                cui_resize_handle_appearance(ui, theme, o.axis, CUI_RESIZE_STRADDLE,
                                             handle_slot->indicator_length,
                                             handle_slot->indicator_opacity);
            cui_end_box(ui);
        }
        cui_end_box(ui);
        preceding = i;
    }
    cui_end_box(ui);
    return error;
}
