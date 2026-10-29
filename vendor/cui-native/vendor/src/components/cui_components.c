/* SPDX-License-Identifier: MIT */
#include "cui/cui_components.h"
#include "../base/cui_design_metrics.h"
#include "cui/cui_button_styles.h"
#include "cui/cui_icon.h"
#include "cui_input_private.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool cui_components_design(const cui_theme *theme, cui_design_size size,
                                  cui_design_metrics *metrics) {
    if (size == CUI_DESIGN_INHERIT) {
        *metrics = (cui_design_metrics){0};
        return true;
    }
    return theme && cui_design_measure(size, theme->font_size / 16, metrics);
}
static cui_box cui_components_control(const cui_theme *t, bool disabled, bool selected) {
    return (cui_box){.height = t->control_height,
                     .padding = t->padding,
                     .gap = t->gap,
                     .radius = t->radius,
                     .row = true,
                     .background = selected && !disabled ? t->primary : t->surface,
                     .border = disabled ? t->muted : t->border,
                     .border_width = 1};
}
static void cui_components_focus(cui_context *ui, cui_id id, const cui_theme *t, cui_box *b) {
    if (cui_focused(ui) == id) {
        b->border = t->focus;
        b->border_width = 2;
    }
}
static bool cui_button_dimension(float value) {
    return isfinite(value) && value > 0 && value <= 4096;
}
static cui_str cui_button_label(cui_context *ui, cui_button_state *state, cui_str name, float font,
                                float width) {
    if (!state || !cui_button_dimension(width) || !name.length || !name.data ||
        cui_measure_text(ui, name, font).width <= width)
        return name;
    cui_str ellipsis = cui_string("…");
    state->label[0] = 0;
    float tail = cui_measure_text(ui, ellipsis, font).width;
    if (tail > width)
        return (cui_str){state->label, 0};
    size_t prefix = 0;
    while (prefix < name.length) {
        size_t next = cui_grapheme_next(name, prefix);
        if (next <= prefix || next > sizeof(state->label) - 4 ||
            cui_measure_text(ui, (cui_str){name.data, next}, font).width + tail > width)
            break;
        prefix = next;
    }
    memcpy(state->label, name.data, prefix);
    memcpy(state->label + prefix, ellipsis.data, ellipsis.length);
    state->label[prefix + ellipsis.length] = 0;
    return (cui_str){state->label, prefix + ellipsis.length};
}
typedef struct {
    size_t bytes;
    float font;
    cui_color color;
    cui_button_state *state;
    bool underline;
} cui_button_label_paint;
static bool cui_button_label_prepaint(const cui_prepaint_info *info, const void *data,
                                      cui_prepaint_writer *writer) {
    const cui_button_label_paint *p = data;
    cui_str name = {(const char *)data + sizeof *p, p->bytes};
    if (!name.length || info->bounds.w <= 0 || info->bounds.h <= 0) {
        if (name.length && p->state && !info->inert)
            p->state->label[0] = 0;
        return true;
    }
    float width = info->bounds.w;
    cui_size natural = cui_prepaint_measure(writer, name, p->font, (cui_text_style){0});
    if (!isfinite(natural.width) || !isfinite(natural.height) || natural.width < 0 ||
        natural.height < 0)
        return false;
    size_t prefix = name.length;
    float prefix_width = natural.width, tail_width = 0;
    cui_str tail = cui_string("…");
    /* The constrained row reserves two representable steps for fractional
     * child advances. Allow those plus subtraction rounding when deciding a
     * measured exact fit; the existing viewport still clips physical paint. */
    float fit_limit = width;
    for (unsigned i = 0; i < 4; ++i)
        fit_limit = nextafterf(fit_limit, INFINITY);
    bool truncated = natural.width > fit_limit;
    if (truncated) {
        if (p->state && !info->inert)
            p->state->label[0] = 0;
        tail_width = cui_prepaint_measure(writer, tail, p->font, (cui_text_style){0}).width;
        if (tail_width > width)
            return true;
        size_t cap = p->state ? sizeof p->state->label - 4 : name.length;
        size_t clusters = 0, offset = 0;
        while (offset < name.length) {
            size_t next = cui_grapheme_next(name, offset);
            if (next <= offset || next > cap)
                break;
            offset = next;
            ++clusters;
        }
        size_t low = 0, high = clusters;
        while (low < high) {
            size_t middle = low + (high - low + 1) / 2, end = 0;
            for (size_t i = 0; i < middle; ++i)
                end = cui_grapheme_next(name, end);
            float measured = cui_prepaint_measure(writer, (cui_str){name.data, end}, p->font,
                                                  (cui_text_style){0})
                                 .width;
            if (measured + tail_width <= width)
                low = middle;
            else
                high = middle - 1;
        }
        prefix = 0;
        for (size_t i = 0; i < low; ++i)
            prefix = cui_grapheme_next(name, prefix);
        prefix_width =
            cui_prepaint_measure(writer, (cui_str){name.data, prefix}, p->font, (cui_text_style){0})
                .width;
        if (p->state && !info->inert) {
            memcpy(p->state->label, name.data, prefix);
            memcpy(p->state->label + prefix, tail.data, tail.length);
            p->state->label[prefix + tail.length] = 0;
        }
    }
    float total = prefix_width + tail_width;
    float x = info->bounds.x + fmaxf(0, (width - total) * .5f);
    cui_color color = p->color;
    color.a = (uint8_t)(color.a * info->opacity);
    cui_draw_command command = {
        .type = CUI_DRAW_TEXT,
        .bounds = {x,
                   info->bounds.y +
                       (info->bounds.h - natural.height - (p->underline ? 1 : 0)) * .5f,
                   prefix_width, natural.height},
        .color = color,
        .font_size = p->font,
        .text = {name.data, prefix}};
    if (prefix && !cui_prepaint_emit(writer, &command, false))
        return false;
    if (truncated) {
        command.bounds.x += prefix_width;
        command.bounds.w = tail_width;
        command.text = tail;
        if (!cui_prepaint_emit(writer, &command, false))
            return false;
    }
    if (p->underline && total > 0) {
        command = (cui_draw_command){.type = CUI_DRAW_RECT,
                                     .bounds = {x, info->bounds.y + info->bounds.h - 1, total, 1},
                                     .color = color};
        if (!cui_prepaint_emit(writer, &command, false))
            return false;
    }
    return true;
}
static void cui_button_artwork_empty(cui_context *ui, cui_id id, float size) {
    cui_begin_box(
        ui, id,
        (cui_box){.width = size, .height = size, .fixed_width = true, .fixed_height = true});
    cui_end_box(ui);
}
static bool cui_button_artwork_overlap(const void *a, size_t an, const void *b, size_t bn) {
    if (!a || !b || !an || !bn)
        return false;
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return x <= y ? y - x < an : x - y < bn;
}
static bool cui_button_artwork_valid(const cui_button_artwork *a, bool loading) {
    if (!a)
        return true;
    if (a->result && (cui_button_artwork_overlap(a->result, sizeof(*a->result), a, sizeof(*a)) ||
                      cui_button_artwork_overlap(a->result, sizeof(*a->result), a->icon,
                                                 a->icon ? sizeof(*a->icon) : 0) ||
                      cui_button_artwork_overlap(a->result, sizeof(*a->result), a->progress,
                                                 a->progress ? sizeof(*a->progress) : 0)))
        return false;
    if (a->kind < CUI_BUTTON_ARTWORK_ICON || a->kind > CUI_BUTTON_ARTWORK_PROGRESS_CIRCLE ||
        (loading && a->kind != CUI_BUTTON_ARTWORK_ICON))
        return false;
    if (a->kind == CUI_BUTTON_ARTWORK_PROGRESS_CIRCLE) {
        cui_progress_options p = a->progress_options.progress;
        float scale = a->progress_options.raster_scale;
        if (a->progress && a->result &&
            (cui_button_artwork_overlap(a->result, sizeof(*a->result), a->progress->pixels,
                                        a->progress->capacity) ||
             cui_button_artwork_overlap(a->result, sizeof(*a->result), p.motion,
                                        p.motion ? sizeof(*p.motion) : 0)))
            return false;
        return a->progress && isfinite(a->fraction) && isfinite(scale) && scale >= 0 &&
               scale <= 4 && isfinite(p.now_seconds) && isfinite(p.duration) && p.duration >= 0 &&
               p.size >= CUI_CONTROL_MEDIUM && p.size <= CUI_CONTROL_XSMALL &&
               (!p.name.length || p.name.data);
    }
    if (a->kind == CUI_BUTTON_ARTWORK_ICON && !a->icon)
        return false;
    cui_icon_options o = a->icon_options;
    cui_icon_transform t = o.transform;
    return (!o.custom_opacity || isfinite(o.opacity)) &&
           (!o.custom_transform || (isfinite(t.a) && isfinite(t.b) && isfinite(t.c) &&
                                    isfinite(t.d) && isfinite(t.tx) && isfinite(t.ty)));
}
static void cui_button_artwork_paint(cui_context *ui, cui_id id, const cui_theme *theme,
                                     const cui_button_artwork *descriptor, float size,
                                     cui_color foreground, bool rotate) {
    cui_button_artwork a = *descriptor;
    if (!cui_frame_paint_enabled(ui)) {
        cui_button_artwork_empty(ui, id, size);
        return;
    }
    cui_begin_box(ui, id,
                  (cui_box){.width = size,
                            .height = size,
                            .fixed_width = true,
                            .fixed_height = true,
                            .align_x = CUI_ALIGN_CENTER,
                            .align_y = CUI_ALIGN_CENTER});
    cui_id child_id = id ^ UINT64_C(0x41d0e78ad86f5307);
    if (!child_id)
        child_id = ~id;
    cui_button_artwork_result result = {CUI_BUTTON_ARTWORK_OK, CUI_ICON_OK};
    if (a.kind == CUI_BUTTON_ARTWORK_PROGRESS_CIRCLE) {
        cui_progress_circle_options p = a.progress_options;
        p.diameter = size;
        p.progress.design_size = CUI_DESIGN_INHERIT;
        uint64_t revision = a.progress->revision;
        (void)cui_progress_circle(ui, child_id, theme, a.fraction, a.progress, p, NULL, NULL);
        if (a.progress->revision == revision) {
            result.status = CUI_BUTTON_ARTWORK_PROGRESS_FAILURE;
        }
    } else {
        cui_icon_options o = a.icon_options;
        o.size = CUI_ICON_CUSTOM;
        o.custom_size = size;
        o.design_size = CUI_DESIGN_INHERIT;
        o.custom_width = o.custom_height = false;
        if (!o.custom_color && !o.custom_inherited_color) {
            o.custom_color = true;
            o.color = foreground;
        }
        bool moving = rotate && !cui_reduced_motion(ui);
        if (moving) {
            double phase = fmod(cui_frame_time(ui), .8) / .8;
            double eased =
                phase < .5 ? 2 * phase * phase : 1 - (-2 * phase + 2) * (-2 * phase + 2) / 2;
            o.custom_transform = true;
            o.transform = cui_icon_rotation((float)(eased * 6.2831853071795864769));
        }
        result.icon_error = a.icon ? cui_icon(ui, child_id, theme, a.icon, o)
                                   : cui_icon_spinner(ui, child_id, theme, o);
        if (result.icon_error != CUI_ICON_OK)
            result.status = CUI_BUTTON_ARTWORK_ICON_FAILURE;
        else if (moving)
            cui_invalidate(ui);
    }
    cui_end_box(ui);
    if (a.result)
        *a.result = result;
}
static bool cui_button_shape_valid(cui_button_options);
static cui_button_style_paint cui_button_default_paint(cui_box b, const cui_theme *t,
                                                       cui_button_options o, cui_interaction state,
                                                       cui_color *caret) {
    bool disabled = o.disabled;
    bool text = o.variant == CUI_BUTTON_TEXT;
    cui_theme_button_kind kind =
        o.variant == CUI_BUTTON_SECONDARY ? CUI_THEME_BUTTON_SECONDARY
        : o.variant == CUI_BUTTON_DANGER  ? CUI_THEME_BUTTON_DANGER
        : o.variant == CUI_BUTTON_WARNING ? CUI_THEME_BUTTON_WARNING
        : o.variant == CUI_BUTTON_SUCCESS ? CUI_THEME_BUTTON_SUCCESS
        : o.variant == CUI_BUTTON_INFO    ? CUI_THEME_BUTTON_INFO
        : o.variant == CUI_BUTTON_OUTLINE || o.variant == CUI_BUTTON_DEFAULT
            ? CUI_THEME_BUTTON_DEFAULT
            : CUI_THEME_BUTTON_PRIMARY;
    cui_theme_button_colors palette = o.custom_colors ? o.colors : t->buttons[kind];
    if (o.variant == CUI_BUTTON_CUSTOM && !o.custom_colors)
        palette = (cui_theme_button_colors){.foreground = t->text, .outline_foreground = t->text};
    bool outlined = o.outline || o.variant == CUI_BUTTON_OUTLINE;
    /* Selected/open use their semantic style, independently of pointer state. */
    bool selected = o.selected || o.open;
    bool pressed = state.pressed && !selected;
    bool hovered = state.hovered && !pressed && !selected;
    bool active = pressed || selected;
    cui_color foreground = outlined ? palette.outline_foreground : palette.foreground;
    cui_color caret_color = foreground;
    if (!o.custom_colors) {
        if (o.variant == CUI_BUTTON_GHOST)
            caret_color = t->secondary_foreground;
        else if (o.variant == CUI_BUTTON_LINK)
            caret_color = t->link;
        else if (o.variant == CUI_BUTTON_TEXT) {
            caret_color = t->text;
            caret_color.a = (uint8_t)(caret_color.a * .9f);
        }
    }
    caret_color.a = (uint8_t)(caret_color.a * .75f);
    b.background = outlined ? (active          ? palette.outline_active
                               : state.hovered ? palette.outline_hover
                                               : palette.outline_normal)
                            : (active          ? palette.active
                               : state.hovered ? palette.hover
                                               : palette.normal);
    b.border = outlined ? palette.outline_border : palette.border;
    if (o.variant == CUI_BUTTON_GHOST && !o.custom_colors) {
        b.background = selected && !outlined ? t->buttons[CUI_THEME_BUTTON_SECONDARY].active
                       : active              ? t->buttons[CUI_THEME_BUTTON_DEFAULT].active
                       : hovered             ? t->accent
                                             : (cui_color){0};
        if (hovered && t->dark)
            b.background.a = (uint8_t)(b.background.a * .5f);
        foreground = hovered ? t->accent_foreground : t->secondary_foreground;
    } else if (o.variant == CUI_BUTTON_LINK && !o.custom_colors) {
        b.background = (cui_color){0};
        foreground = selected && outlined ? t->link
                     : active             ? t->link_active
                     : hovered            ? t->link_hover
                                          : t->link;
    } else if (text && !o.custom_colors) {
        b.background = (cui_color){0};
        foreground = t->text;
        foreground.a = (uint8_t)(foreground.a * (selected && outlined ? .9f
                                                 : active             ? .7f
                                                 : hovered            ? 1
                                                                      : .9f));
    }
    if (!outlined && (o.variant == CUI_BUTTON_GHOST || o.variant == CUI_BUTTON_LINK || text))
        b.border_width = 0;
    if (!o.custom_colors && (o.variant == CUI_BUTTON_GHOST || o.variant == CUI_BUTTON_LINK || text))
        b.border = (cui_color){0};
    if (disabled) {
        b.background = outlined ? palette.outline_normal : palette.normal;
        float opacity = outlined ? .5f : kind == CUI_THEME_BUTTON_SECONDARY ? 1.5f : .15f;
        b.background.a = (uint8_t)fminf(255, b.background.a * opacity);
        b.border.a = (uint8_t)fminf(255, b.border.a * (outlined ? .5f : opacity));
        if (o.variant == CUI_BUTTON_GHOST || o.variant == CUI_BUTTON_LINK || text)
            b.background.a = 0;
        if (o.variant == CUI_BUTTON_DEFAULT && !o.custom_colors && !outlined) {
            b.background = t->input_background;
            b.background.a /= 2;
            b.border = t->input;
            b.border.a /= 2;
        }
        foreground = t->muted;
        foreground.a /= 2;
    } else if (o.loading) {
        b.custom_opacity = true;
        b.opacity = .8f;
    }
    if (!o.hide_focus_ring && state.focus_visible) {
        b.border = t->focus;
        b.border_width = 2;
    }
    b.custom_corners = o.custom_corners;
    b.corner_radii = o.corner_radii;
    b.custom_border_edges = o.custom_border_edges;
    b.border_edges = o.border_edges;
    if (caret)
        *caret = caret_color;
    return (cui_button_style_paint){.root = b, .foreground = foreground};
}
static bool cui_button_patch_valid(cui_button_paint_patch p) {
    if (p.fields & ~63u)
        return false;
    if ((p.fields & CUI_BUTTON_PAINT_RADIUS) &&
        (!isfinite(p.paint.root.radius) || p.paint.root.radius < 0))
        return false;
    if ((p.fields & CUI_BUTTON_PAINT_OPACITY) &&
        (!isfinite(p.paint.root.opacity) || p.paint.root.opacity < 0 || p.paint.root.opacity > 1))
        return false;
    if (p.fields & CUI_BUTTON_PAINT_CORNERS) {
        float radii[] = {p.paint.root.corner_radii.top_left, p.paint.root.corner_radii.top_right,
                         p.paint.root.corner_radii.bottom_left,
                         p.paint.root.corner_radii.bottom_right};
        for (size_t i = 0; i < 4; i++)
            if (!isfinite(radii[i]) || radii[i] < 0)
                return false;
    }
    return true;
}
static void cui_button_apply_patch(cui_button_style_paint *p, cui_button_paint_patch patch) {
    if (patch.fields & CUI_BUTTON_PAINT_BACKGROUND)
        p->root.background = patch.paint.root.background;
    if (patch.fields & CUI_BUTTON_PAINT_BORDER)
        p->root.border = patch.paint.root.border;
    if (patch.fields & CUI_BUTTON_PAINT_FOREGROUND)
        p->foreground = patch.paint.foreground;
    if (patch.fields & CUI_BUTTON_PAINT_RADIUS)
        p->root.radius = patch.paint.root.radius;
    if (patch.fields & CUI_BUTTON_PAINT_CORNERS) {
        p->root.custom_corners = patch.paint.root.custom_corners;
        p->root.corner_radii = patch.paint.root.corner_radii;
    }
    if (patch.fields & CUI_BUTTON_PAINT_OPACITY) {
        p->root.custom_opacity = true;
        p->root.opacity = patch.paint.root.opacity;
    }
}
static cui_button_paint_patch cui_button_style_layer(cui_button_style_paint *p,
                                                     cui_button_style_info *info,
                                                     cui_button_style_phase phase,
                                                     cui_button_style_fn callback, void *user) {
    cui_button_paint_patch patch = {0};
    if (callback) {
        info->phase = phase;
        callback(user, info, p, &patch);
        if (!cui_button_patch_valid(patch))
            patch = (cui_button_paint_patch){0};
        cui_button_apply_patch(p, patch);
    }
    return patch;
}
static void cui_button_builtin_layer(cui_button_style_paint *p, cui_box geometry,
                                     const cui_theme *t, cui_button_options o,
                                     cui_interaction state) {
    cui_button_style_paint built = cui_button_default_paint(geometry, t, o, state, NULL);
    p->root.background = built.root.background;
    p->root.border = built.root.border;
    p->foreground = built.foreground;
}
static cui_button_style_paint cui_button_resolve_styles(cui_box geometry, cui_id id,
                                                        const cui_theme *t, cui_button_options o,
                                                        cui_interaction state,
                                                        cui_button_styles styles) {
    cui_button_style_info info = {.id = id,
                                  .hovered = state.hovered,
                                  .pressed = state.pressed,
                                  .focus_visible = state.focus_visible,
                                  .selected = o.selected,
                                  .open = o.open,
                                  .disabled = o.disabled,
                                  .loading = o.loading};
    cui_button_options normal = o;
    normal.selected = normal.open = normal.disabled = normal.loading = false;
    cui_interaction idle = {0};
    cui_button_style_paint p = cui_button_default_paint(geometry, t, normal, idle, NULL);
    cui_button_paint_patch instance =
        cui_button_style_layer(&p, &info, CUI_BUTTON_STYLE_INSTANCE, styles.instance, styles.user);
    if (!o.disabled && !o.loading && !o.selected && !o.open) {
        if (state.pressed || state.hovered) {
            cui_interaction pointer = {.pressed = state.pressed, .hovered = state.hovered};
            cui_button_builtin_layer(&p, geometry, t, normal, pointer);
            cui_button_style_layer(&p, &info,
                                   state.pressed ? CUI_BUTTON_STYLE_ACTIVE : CUI_BUTTON_STYLE_HOVER,
                                   state.pressed ? styles.active : styles.hovered, styles.user);
        }
    }
    if (o.selected || o.open) {
        cui_button_options selected = normal;
        selected.selected = true;
        cui_button_builtin_layer(&p, geometry, t, selected, idle);
        cui_button_style_layer(&p, &info, CUI_BUTTON_STYLE_SELECTED, styles.selected, styles.user);
        cui_button_apply_patch(&p, instance);
    }
    if (o.disabled) {
        cui_button_options disabled = normal;
        disabled.disabled = true;
        cui_button_builtin_layer(&p, geometry, t, disabled, idle);
        cui_button_style_layer(&p, &info, CUI_BUTTON_STYLE_DISABLED, styles.disabled, styles.user);
        cui_button_apply_patch(&p, instance);
    } else if (o.loading) {
        p.root.opacity = (p.root.custom_opacity ? p.root.opacity : 1) * .8f;
        p.root.custom_opacity = true;
    }
    if (state.focus_visible && !o.hide_focus_ring) {
        p.root.border = t->focus;
        p.root.border_width = 2;
        cui_button_style_layer(&p, &info, CUI_BUTTON_STYLE_FOCUS, styles.focused, styles.user);
    }
    return p;
}
static void cui_button_paint_state(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                                   cui_button_options o, cui_interaction state,
                                   const cui_button_styles *styles) {
    cui_design_metrics design;
    if (!ui || !t || !cui_frame_is_submitting(ui) ||
        !cui_components_design(t, o.design_size, &design) || !cui_button_shape_valid(o))
        return;
    bool disabled = o.disabled;
    cui_box b = cui_components_control(t, disabled, false);
    float density = isfinite(t->font_size) && t->font_size > 0 ? t->font_size / 16 : 1;
    float font = (o.size == CUI_CONTROL_XSMALL  ? 12
                  : o.size == CUI_CONTROL_LARGE ? 16
                                                : 14) *
                 density;
    float padding = (o.size == CUI_CONTROL_XSMALL  ? 4
                     : o.size == CUI_CONTROL_SMALL ? 8
                     : o.size == CUI_CONTROL_LARGE ? 12
                                                   : 10) *
                    density;
    float gap = (o.size == CUI_CONTROL_XSMALL || o.size == CUI_CONTROL_SMALL ? 4 : 8) * density;
    float icon = (o.size == CUI_CONTROL_XSMALL  ? 12
                  : o.size == CUI_CONTROL_SMALL ? 14
                  : o.size == CUI_CONTROL_LARGE ? 24
                                                : 16) *
                 density;
    b.height = (o.size == CUI_CONTROL_XSMALL  ? 20
                : o.size == CUI_CONTROL_SMALL ? 24
                                              : 32) *
               density;
    if (o.design_size != CUI_DESIGN_INHERIT) {
        font = design.font_size;
        padding = design.padding_x;
        b.height = design.control_height;
        gap = design.gap;
        icon = design.indicator_size;
    }
    float caret = icon;
    if (o.pixel_size > 0) {
        font = 16 * density;
        padding = o.pixel_size * .2f;
        gap = 8 * density;
        icon = o.pixel_size * .75f;
        caret = 16 * density;
        b.height = o.icon_only ? o.pixel_size : 0;
    }
    if (cui_button_dimension(o.height))
        b.height = o.height;
    if (cui_button_dimension(o.font_size))
        font = o.font_size;
    if (cui_button_dimension(o.icon_size))
        icon = o.icon_size;
    /* Raster icons occupy integral pixels; reserve the complete painted extent. */
    icon = ceilf(icon);
    caret = o.pixel_size > 0 ? ceilf(caret) : icon;
    if (o.compact && o.pixel_size == 0) {
        if (o.design_size != CUI_DESIGN_INHERIT)
            padding *= .8f;
        else
            padding = (o.size == CUI_CONTROL_XSMALL  ? 4
                       : o.size == CUI_CONTROL_SMALL ? 6
                                                     : 8) *
                      density;
    }
    bool text = o.variant == CUI_BUTTON_TEXT;
    if (o.variant == CUI_BUTTON_LINK || text)
        padding = 0;
    bool numeric_no_padding = o.pixel_size > 0 && (o.variant == CUI_BUTTON_LINK || text);
    if ((text || numeric_no_padding) && !cui_button_dimension(o.height))
        b.height = 0;
    b.padding = 0;
    b.gap = 0;
    b.width = isfinite(o.width) ? o.width : 0;
    b.fixed_width = b.width > 0;
    if (o.icon_only) {
        padding = 0;
        if (text && !b.height && !numeric_no_padding)
            b.height = icon;
        if (!b.width && !numeric_no_padding) {
            b.width = b.height;
            b.fixed_width = true;
        }
    }
    if (o.compact && !b.width && !o.icon_only && !o.content && !o.children && !o.leading_content &&
        !o.trailing_content) {
        float measured = 0;
        unsigned parts = 0;
        if (name.length) {
            measured += cui_measure_text(ui, name, font).width;
            parts++;
        }
        if (o.loading || o.icon.length || o.leading_artwork) {
            measured += icon;
            parts++;
        } else if (o.leading.length) {
            measured += cui_measure_text(ui, o.leading, font).width;
            parts++;
        }
        if (o.trailing_icon.length) {
            measured += icon;
            parts++;
        } else if (o.trailing.length) {
            measured += cui_measure_text(ui, o.trailing, font).width;
            parts++;
        }
        if (o.dropdown_caret) {
            measured += caret;
            parts++;
        }
        measured += padding * 2 + (parts ? parts - 1 : 0) * gap;
        if (isfinite(measured)) {
            b.width = fmaxf(b.height, measured);
            b.fixed_width = true;
        }
    }
    b.align_x = CUI_ALIGN_CENTER;
    b.align_y = CUI_ALIGN_CENTER;
    b.clip = b.width != 0 && !o.allow_content_overflow;
    if (o.rounding == CUI_BUTTON_ROUNDED_NONE)
        b.radius = 0;
    else if (o.rounding == CUI_BUTTON_ROUNDED_SMALL)
        b.radius = t->radius * .5f;
    else if (o.rounding == CUI_BUTTON_ROUNDED_LARGE)
        b.radius = t->radius * 2;
    else if (o.rounding == CUI_BUTTON_ROUNDED_CUSTOM && isfinite(o.radius) && o.radius >= 0)
        b.radius = o.radius;
    cui_color caret_color;
    cui_button_style_paint paint = cui_button_default_paint(b, t, o, state, &caret_color);
    if (styles && (styles->instance || styles->hovered || styles->active || styles->selected ||
                   styles->disabled || styles->focused))
        paint = cui_button_resolve_styles(b, id, t, o, state, *styles);
    b = paint.root;
    cui_color foreground = paint.foreground;
    cui_begin_box(ui, id, b);
    /* Clipping boxes retain natural child widths for scrolling. A percentage
     * child establishes the resolved viewport width without compression by the
     * clipping parent; its ordinary row can then shrink content for wrapping. */
    bool bounded_content = o.content && b.width != 0 && !o.allow_content_overflow;
    if (bounded_content)
        cui_begin_box(ui, 0,
                      (cui_box){.width = 1,
                                .width_percent = true,
                                .height = b.height,
                                .fixed_height = b.height > 0,
                                .row = true,
                                .align_x = CUI_ALIGN_CENTER,
                                .align_y = CUI_ALIGN_CENTER});
    cui_begin_box(ui, 0, (cui_box){.width = padding, .fixed_width = true, .height = 1});
    cui_end_box(ui);
    /* Leave two representable steps for summed fractional child advances. */
    cui_begin_box(
        ui, 0,
        (cui_box){.width = b.width ? -1 : 0,
                  .max_width = b.width > 0
                                   ? nextafterf(nextafterf(fmaxf(0, b.width - 2 * padding), 0), 0)
                                   : 0,
                  .row = true,
                  .height = b.height,
                  .fixed_height = b.height > 0,
                  .clip = !o.allow_content_overflow && !bounded_content,
                  .align_x = CUI_ALIGN_CENTER,
                  .align_y = CUI_ALIGN_CENTER,
                  .gap = gap});
    if (text && !b.height && !o.icon_only && !o.content) {
        float minimum = fmaxf(font * 1.25f, cui_measure_text(ui, name, font).height);
        cui_begin_box(
            ui, 0,
            (cui_box){.width = 0, .fixed_width = true, .height = minimum, .fixed_height = true});
        cui_end_box(ui);
    }
    cui_icon_options artwork = {
        .size = CUI_ICON_CUSTOM, .custom_size = icon, .custom_color = true, .color = foreground};
    if (o.leading_content)
        o.leading_content(ui, o.leading_user);
    else if (o.leading_artwork &&
             (!o.loading || o.leading_artwork->kind != CUI_BUTTON_ARTWORK_ICON))
        cui_button_artwork_paint(ui, id ^ UINT64_C(0x7185e28e9fb974db), t, o.leading_artwork, icon,
                                 foreground, o.leading_artwork->kind == CUI_BUTTON_ARTWORK_SPINNER);
    else if (o.loading && o.loading_artwork)
        cui_button_artwork_paint(ui, id ^ UINT64_C(0x7185e28e9fb974db), t, o.loading_artwork, icon,
                                 foreground, true);
    else if (o.loading && !cui_frame_paint_enabled(ui)) {
        /* Layout-only composition retains the icon slot without allocating or
         * scheduling invisible loading artwork. */
        cui_begin_box(
            ui, id ^ UINT64_C(0x7185e28e9fb974db),
            (cui_box){.width = icon, .height = icon, .fixed_width = true, .fixed_height = true});
        cui_end_box(ui);
    } else if (o.loading) {
        if (o.loading_icon.length) {
            if (!cui_reduced_motion(ui)) {
                double phase = fmod(cui_frame_time(ui), .8) / .8;
                double eased =
                    phase < .5 ? 2 * phase * phase : 1 - (-2 * phase + 2) * (-2 * phase + 2) / 2;
                artwork.custom_transform = true;
                artwork.transform = cui_icon_rotation((float)(eased * 6.2831853071795864769));
            }
            if (cui_icon_named(ui, id ^ UINT64_C(0x7185e28e9fb974db), t, o.loading_icon, artwork) ==
                    CUI_ICON_OK &&
                !cui_reduced_motion(ui))
                cui_invalidate(ui);
        } else
            (void)cui_icon_spinner(ui, id ^ UINT64_C(0x7185e28e9fb974db), t, artwork);
    } else if (o.icon.length)
        (void)cui_icon_named(ui, id ^ UINT64_C(0x7185e28e9fb974db), t, o.icon, artwork);
    else if (o.leading.length)
        cui_text_with_line_height(ui, o.leading, font, foreground, font * 1.25f);
    if (o.content)
        o.content(ui, o.content_user);
    else if (!o.icon_only) {
        bool constrained = b.width != 0 || cui_button_dimension(o.label_width);
        if (constrained && !o.allow_content_overflow) {
            bool explicit_width = !b.width && cui_button_dimension(o.label_width);
            cui_id label_id = id ^ UINT64_C(0x2a7251d89c831be7);
            if (!label_id)
                label_id = ~id;
            cui_begin_box(
                ui, label_id,
                (cui_box){.width = explicit_width ? o.label_width : -1,
                          .fixed_width = explicit_width,
                          .max_width = cui_button_dimension(o.label_width) ? o.label_width : 0,
                          .height = fmaxf(font * 1.25f, cui_measure_text(ui, name, font).height) +
                                    (o.variant == CUI_BUTTON_LINK ? 1 : 0),
                          .fixed_height = true,
                          .clip = true});
            cui_button_label_paint label = {name.length, font, foreground, o.state,
                                            o.variant == CUI_BUTTON_LINK};
            (void)cui_set_prepaint_parts(
                ui,
                (cui_prepaint_options){.callback = cui_button_label_prepaint,
                                       .payload = &label,
                                       .payload_bytes = sizeof label,
                                       .command_budget = 5},
                name.data, name.length);
            cui_end_box(ui);
        } else {
            cui_button_state passive_label;
            cui_button_state *label_state = o.state;
            if (label_state && !cui_frame_sequence(ui)) {
                /* This legacy branch copies text synchronously into the frame arena. */
                passive_label = *label_state;
                label_state = &passive_label;
            }
            cui_str visible = cui_button_label(ui, label_state, name, font, o.label_width);
            cui_begin_box(
                ui, 0,
                (cui_box){.width = cui_button_dimension(o.label_width) ? o.label_width : 0,
                          .fixed_width = cui_button_dimension(o.label_width),
                          .clip = cui_button_dimension(o.label_width),
                          .align_x = CUI_ALIGN_CENTER});
            if (cui_button_dimension(o.label_width))
                cui_text_line(ui, visible, font, foreground, font * 1.25f, (cui_text_style){0});
            else
                cui_text_with_line_height(ui, visible, font, foreground, font * 1.25f);
            if (o.variant == CUI_BUTTON_LINK && visible.length) {
                float underline = cui_measure_text(ui, visible, font).width;
                if (cui_button_dimension(o.label_width))
                    underline = fminf(underline, o.label_width);
                cui_begin_box(ui, 0,
                              (cui_box){.width = underline, .height = 1, .background = foreground});
                cui_end_box(ui);
            }
            cui_end_box(ui);
        }
    }
    if (o.children)
        o.children(ui, o.children_user);
    if (o.trailing_content)
        o.trailing_content(ui, o.trailing_user);
    else if (o.trailing_icon.length)
        (void)cui_icon_named(ui, id ^ UINT64_C(0x52a4e31), t, o.trailing_icon, artwork);
    else if (o.trailing.length)
        cui_text_with_line_height(ui, o.trailing, font, foreground, font * 1.25f);
    if (o.dropdown_caret) {
        if (b.width && (o.allow_content_overflow || o.content || o.icon_only ||
                        cui_button_dimension(o.label_width))) {
            cui_begin_box(ui, 0, (cui_box){.width = -1});
            cui_end_box(ui);
        }
        (void)cui_icon_named(ui, id ^ UINT64_C(0x7e51d), t, cui_string("chevron-down"),
                             (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                                .custom_size = caret,
                                                .custom_color = true,
                                                .color = caret_color});
    }
    cui_end_box(ui);
    cui_begin_box(ui, 0, (cui_box){.width = padding, .fixed_width = true, .height = 1});
    cui_end_box(ui);
    if (bounded_content)
        cui_end_box(ui);
    cui_end_box(ui);
}
void cui_button_paint(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                      cui_button_options o) {
    cui_interaction state = cui_interaction_state(ui, id);
    if (o.disabled || o.loading)
        state.hovered = state.pressed = false;
    cui_button_paint_state(ui, id, t, name, o, state, NULL);
}
static bool cui_button_shape_valid(cui_button_options o) {
    if (!cui_button_artwork_valid(o.leading_artwork, false) ||
        !cui_button_artwork_valid(o.loading_artwork, true) || !isfinite(o.pixel_size) ||
        o.pixel_size < 0 || o.pixel_size > 4096)
        return false;
    float radii[] = {o.corner_radii.top_left, o.corner_radii.top_right, o.corner_radii.bottom_left,
                     o.corner_radii.bottom_right};
    float edges[] = {o.border_edges.left, o.border_edges.right, o.border_edges.top,
                     o.border_edges.bottom};
    for (size_t i = 0; i < 4; i++)
        if ((o.custom_corners && (!isfinite(radii[i]) || radii[i] < 0)) ||
            (o.custom_border_edges && (!isfinite(edges[i]) || edges[i] < 0)))
            return false;
    return true;
}
static void cui_button_metadata(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                                cui_button_options o, cui_interaction interaction) {
    (void)t;
    (void)name;
    bool inert = o.disabled || o.loading;
    if (o.variant == CUI_BUTTON_LINK)
        (void)cui_set_semantic_role(ui, id, CUI_ROLE_LINK);
    cui_update_semantic(ui, id, o.toggle_semantics && o.toggled,
                        o.toggle_semantics && o.toggled ? 1 : 0);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.description = o.loading ? cui_string("Loading") : (cui_str){0},
                               .busy = o.loading,
                               .no_pointer_focus = o.prevent_pointer_focus,
                               .pointer_activatable = o.prevent_pointer_focus,
                               .blocks_pointer_activation = inert});
    if (o.custom_tab_index)
        cui_set_tab_index(ui, id, o.tab_index);
    cui_set_tab_stop(ui, id, !o.skip_tab);
    if (o.custom_cursor)
        (void)cui_set_cursor(ui, id, o.cursor);
    else if (!inert && (o.variant == CUI_BUTTON_LINK || o.variant == CUI_BUTTON_TEXT))
        (void)cui_set_cursor(ui, id, CUI_CURSOR_POINTER);
    if (o.state) {
        if (o.state->id != id)
            *o.state = (cui_button_state){.id = id};
        bool hovered = !inert && interaction.hovered;
        if (!inert && o.on_hover &&
            ((o.state->initialized && o.state->hovered != hovered) ||
             (!o.state->initialized && hovered)))
            o.on_hover(o.action_user, hovered);
        o.state->hovered = hovered;
        o.state->initialized = true;
        o.state->next_wake_seconds = 0;
    }
}
static void cui_button_tooltip(cui_context *ui, cui_id id, const cui_theme *t,
                               cui_button_options o) {
    bool inert = o.disabled || o.loading;
    if (o.state && o.tooltip.length) {
        cui_hover_options hover = o.tooltip_options;
        hover.trigger = id;
        hover.dt = cui_frame_delta(ui);
        hover.disabled = inert;
        size_t count = 0;
        const cui_event *events = cui_frame_events(ui, &count);
        for (size_t i = 0; i < count; i++)
            if (events[i].type == CUI_EVENT_CANCEL && cui_event_target(ui, i) == id)
                o.state->tooltip = (cui_hover_state){.suppressed = true};
        cui_tooltip(ui, id ^ UINT64_C(0x83a46d), t, &o.state->tooltip, o.tooltip, hover);
        double now = cui_frame_time(ui);
        if (o.state->tooltip.pending && isfinite(now) && now <= 1e12)
            o.state->next_wake_seconds = now + 1.0 / 60;
    } else if (o.state)
        o.state->tooltip = (cui_hover_state){0};
}
static cui_button_click_event cui_button_click_snapshot(cui_context *ui, cui_id id, size_t index,
                                                        const cui_event *event,
                                                        const cui_activation_snapshot *evidence) {
    cui_button_click_event result = {.target = id,
                                     .frame_sequence = cui_frame_sequence(ui),
                                     .event_index = index,
                                     .event_type = event->type,
                                     .modifiers = event->modifiers,
                                     .repeat = event->repeat};
    if (event->type == CUI_EVENT_POINTER_UP) {
        result.origin = CUI_BUTTON_CLICK_POINTER;
        result.has_position = true;
        result.x = event->x;
        result.y = event->y;
        result.pointer_type = event->pointer_type;
        result.pointer_button = event->pointer_button;
        result.pointer_id = event->pointer_id;
        result.pointer_device = event->pointer_device;
        result.click_count = event->click_count ? event->click_count : 1;
    } else if (event->type == CUI_EVENT_ACCESSIBILITY_ACTION)
        result.origin = CUI_BUTTON_CLICK_ACCESSIBILITY;
    else {
        result.origin = CUI_BUTTON_CLICK_KEYBOARD;
        result.key = event->key;
    }
    if (evidence) {
        result.has_evidence = true;
        result.evidence = *evidence;
    } else
        result.has_evidence = cui_event_activation_snapshot(ui, index, &result.evidence);
    return result;
}
static bool cui_button_submit(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                              cui_button_options o, const cui_button_styles *styles) {
    if (!ui || !t || !cui_button_shape_valid(o))
        return false;
    /* Freeze descriptor values before accepted callbacks; resource handles remain borrowed. */
    cui_button_artwork leading, loading;
    if (o.leading_artwork) {
        leading = *o.leading_artwork;
        o.leading_artwork = &leading;
    }
    if (o.loading_artwork) {
        loading = *o.loading_artwork;
        o.loading_artwork = &loading;
    }
    cui_design_metrics design;
    if (!ui || !t || !cui_components_design(t, o.design_size, &design) ||
        !cui_button_shape_valid(o))
        return false;
    if (!cui_frame_is_submitting(ui))
        return false;
    if (!cui_frame_sequence(ui)) {
        /* Suppressed ancestry composes visuals without touching caller lifecycle state.
         * The borrowed state survives through deferred label paint, whose inert guard
         * preserves the diagnostic prefix cap without writing it. */
        cui_button_paint_state(ui, id, t, name, o, (cui_interaction){0}, styles);
        return false;
    }
    bool inert = o.disabled || o.loading;
    cui_str accessible = o.accessibility_label.length ? o.accessibility_label : name;
    cui_interaction interaction = cui_interact(
        ui, id, o.variant == CUI_BUTTON_LINK ? CUI_ROLE_LINK : CUI_ROLE_BUTTON, accessible, inert,
        o.toggle_semantics && o.toggled, o.toggle_semantics && o.toggled ? 1 : 0);
    cui_button_metadata(ui, id, t, name, o, interaction);
    bool activated = false;
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t e = 0; e < count; e++)
        if (cui_event_accepted_activation(ui, e) == id) {
            activated = true;
            cui_button_click_event event = cui_button_click_snapshot(ui, id, e, &events[e], NULL);
            if (o.on_click)
                o.on_click(o.action_user);
            if (o.on_click_event)
                o.on_click_event(o.action_user, &event);
        }
    cui_button_paint_state(ui, id, t, name, o, interaction, styles);
    cui_button_tooltip(ui, id, t, o);
    return activated;
}
bool cui_button_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                   cui_button_options o) {
    return cui_button_submit(ui, id, t, name, o, NULL);
}
bool cui_button_styled_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                          cui_button_options o, cui_button_styles styles) {
    return cui_button_submit(ui, id, t, name, o, &styles);
}
void cui_button_styled_paint(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                             cui_button_options o, cui_button_styles styles) {
    cui_interaction state = cui_interaction_state(ui, id);
    if (o.disabled || o.loading)
        state.hovered = state.pressed = false;
    cui_button_paint_state(ui, id, t, name, o, state, &styles);
}
typedef struct {
    const cui_button_group_styled_item *child;
    cui_button_click_event event;
    bool prepared;
} cui_button_group_event_forward;
static void cui_button_group_prepare_click(void *user, cui_context *ui,
                                           const cui_activation_snapshot *snapshot) {
    cui_button_group_event_forward *forward = user;
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    forward->prepared = snapshot->event_index < count;
    if (forward->prepared) {
        forward->event = cui_button_click_snapshot(ui, forward->child->id, snapshot->event_index,
                                                   &events[snapshot->event_index], snapshot);
    }
}
static void cui_button_group_click_event(void *user, cui_context *ui, size_t index) {
    (void)ui;
    cui_button_group_event_forward *forward = user;
    if (!forward->prepared || forward->event.event_index != index)
        return;
    forward->child->options.on_click_event(forward->child->options.action_user, &forward->event);
    forward->prepared = false;
}
static bool cui_button_group_style_valid(cui_button_options o) {
    cui_str strings[] = {o.leading,       o.trailing,     o.icon,
                         o.trailing_icon, o.loading_icon, o.accessibility_label,
                         o.tooltip};
    for (size_t i = 0; i < sizeof strings / sizeof strings[0]; i++)
        if (strings[i].length && !strings[i].data)
            return false;
    return o.variant >= CUI_BUTTON_PRIMARY && o.variant <= CUI_BUTTON_CUSTOM &&
           o.size >= CUI_CONTROL_MEDIUM && o.size <= CUI_CONTROL_XSMALL && isfinite(o.width) &&
           isfinite(o.height) && o.height >= 0 && isfinite(o.font_size) && o.font_size >= 0 &&
           isfinite(o.icon_size) && o.icon_size >= 0 && isfinite(o.label_width) &&
           o.label_width >= 0 && o.rounding >= CUI_BUTTON_ROUNDED_THEME &&
           o.rounding <= CUI_BUTTON_ROUNDED_CUSTOM &&
           (o.rounding != CUI_BUTTON_ROUNDED_CUSTOM || (isfinite(o.radius) && o.radius >= 0)) &&
           cui_button_shape_valid(o);
}
bool cui_button_group_ex(cui_context *ui, cui_id id, const cui_theme *theme,
                         const cui_button_group_styled_item *items, bool *selected, size_t count,
                         cui_button_group_style_workspace *w,
                         cui_button_group_style_options options, cui_button_group_result *out) {
    if (out)
        *out = (cui_button_group_result){0};
    if (!ui || !id || !theme || !w || !cui_frame_is_submitting(ui) || w->behavior.active ||
        w->composing || count > CUI_BUTTON_GROUP_MAX_ITEMS || (count && (!items || !selected)) ||
        !isfinite(theme->font_size) || theme->font_size <= 0 || !isfinite(theme->radius) ||
        theme->radius < 0)
        return false;
    cui_theme snapshot = *theme;
    const cui_theme *t = &snapshot;
    cui_design_metrics metrics;
    if (!cui_components_design(t, options.design_size, &metrics) ||
        (options.override_size &&
         (options.size < CUI_CONTROL_MEDIUM || options.size > CUI_CONTROL_XSMALL)) ||
        (options.override_variant &&
         (options.variant < CUI_BUTTON_PRIMARY || options.variant > CUI_BUTTON_CUSTOM)))
        return false;
    float box[] = {options.box.width, options.box.height, options.box.padding};
    for (size_t i = 0; i < sizeof box / sizeof box[0]; i++)
        if (!isfinite(box[i]) || (i == 2 && box[i] < 0))
            return false;
    uintptr_t a = (uintptr_t)w, b = (uintptr_t)selected;
    if (count && b < a + sizeof *w && b + count * sizeof *selected > a)
        return false;
    if (count)
        memmove(w->items, items, count * sizeof *items);
    cui_button_group_event_forward forwarding[CUI_BUTTON_GROUP_MAX_ITEMS];
    for (size_t i = 0; i < count; i++) {
        cui_button_group_styled_item *child = &w->items[i];
        cui_button_options *o = &child->options;
        if (child->label.length && !child->label.data)
            return false;
        if (options.override_size)
            o->size = options.size;
        if (options.design_size != CUI_DESIGN_INHERIT)
            o->design_size = options.design_size;
        if (options.override_variant)
            o->variant = options.variant;
        o->compact |= options.compact;
        o->outline |= options.outline;
        o->disabled |= options.behavior.disabled;
        o->selected = o->toggled = selected[i];
        o->toggle_semantics = true;
        if (!cui_button_group_style_valid(*o) ||
            !cui_components_design(t, o->design_size, &metrics))
            return false;
        forwarding[i] = (cui_button_group_event_forward){.child = child};
        w->behavior.items[i] = (cui_button_group_item){
            .id = child->id,
            .label = o->accessibility_label.length ? o->accessibility_label : child->label,
            .disabled = o->disabled,
            .loading = o->loading,
            .skip_tab = o->skip_tab,
            .prevent_pointer_focus = o->prevent_pointer_focus,
            .custom_cursor = o->custom_cursor,
            .cursor = o->cursor,
            .role = o->variant == CUI_BUTTON_LINK ? CUI_ROLE_LINK : CUI_ROLE_BUTTON,
            .on_click = o->on_click,
            .action_user = o->action_user,
            .on_click_event = o->on_click_event ? cui_button_group_click_event : NULL,
            .event_user = &forwarding[i],
            .prepare_click_event = o->on_click_event ? cui_button_group_prepare_click : NULL};
    }
    w->composing = true;
    bool passive = !cui_frame_sequence(ui) || options.box.inert || options.box.layout_only;
    /* Begin semantics before opening the matching group layout box, as Button does. */
    if (!passive && !cui_base_button_group_begin(ui, id, w->behavior.items, selected, count,
                                                 &w->behavior, options.behavior)) {
        if (w->behavior.active)
            (void)cui_base_button_group_end(ui, &w->behavior);
        w->composing = false;
        return false;
    }
    cui_box box_options = options.box;
    box_options.row = !options.behavior.vertical;
    box_options.gap = 0;
    cui_begin_box(ui, id, box_options);
    for (size_t i = 0; i < count; i++) {
        if (passive) {
            w->behavior.interactions[i] = (cui_interaction){0};
            continue;
        }
        cui_interaction interaction = cui_base_button_group_item(ui, &w->behavior, i);
        cui_button_metadata(ui, w->items[i].id, t, w->items[i].label, w->items[i].options,
                            interaction);
    }
    cui_button_group_result result = passive ? (cui_button_group_result){.valid = true}
                                             : cui_base_button_group_end(ui, &w->behavior);
    /* Freeze final controlled paint state before invoking any content callbacks. */
    if (!passive)
        for (size_t i = 0; i < count; i++)
            w->items[i].options.selected = w->items[i].options.toggled = selected[i];
    if (result.valid)
        for (size_t i = 0; i < count; i++) {
            cui_button_group_styled_item child = w->items[i];
            cui_button_options o = child.options;
            if (passive && o.state) {
                w->passive_states[i] = *o.state;
                o.state = &w->passive_states[i];
            }
            if (count > 1) {
                float radius = o.rounding == CUI_BUTTON_ROUNDED_NONE     ? 0
                               : o.rounding == CUI_BUTTON_ROUNDED_SMALL  ? t->radius * .5f
                               : o.rounding == CUI_BUTTON_ROUNDED_LARGE  ? t->radius * 2
                               : o.rounding == CUI_BUTTON_ROUNDED_CUSTOM ? o.radius
                                                                         : t->radius;
                bool first = i == 0, last = i + 1 == count, vertical = options.behavior.vertical;
                o.custom_corners = true;
                o.corner_radii =
                    (cui_corner_radii){.top_left = first ? radius : 0,
                                       .top_right = (vertical ? first : last) ? radius : 0,
                                       .bottom_left = (vertical ? last : first) ? radius : 0,
                                       .bottom_right = last ? radius : 0};
                float width = w->behavior.interactions[i].focus_visible && !o.hide_focus_ring ? 2
                              : o.variant == CUI_BUTTON_GHOST || o.variant == CUI_BUTTON_LINK ||
                                      o.variant == CUI_BUTTON_TEXT
                                  ? 0
                                  : 1;
                o.custom_border_edges = true;
                o.border_edges = (cui_border_edges){.left = (vertical || first) ? width : 0,
                                                    .top = (!vertical || first) ? width : 0,
                                                    .right = width,
                                                    .bottom = width};
            }
            cui_button_paint_state(ui, child.id, t, child.label, o, w->behavior.interactions[i],
                                   NULL);
            if (!passive)
                cui_button_tooltip(ui, child.id, t, o);
        }
    cui_end_box(ui);
    result.valid &= cui_last_error(ui) == CUI_OK;
    w->composing = false;
    if (out)
        *out = result;
    return result.valid;
}
bool cui_button(cui_context *ui, cui_id id, const cui_theme *t, cui_str name, bool disabled) {
    return cui_button_ex(ui, id, t, name, (cui_button_options){.disabled = disabled});
}
static const unsigned char cui_components_check_pixels[16 * 16 * 4] = {
    255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255,
    0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255,
    255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255,
    255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,
    255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255,
    0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255,
    255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255,
    255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,
    255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255,
    0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255,
    255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255,
    255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,
    255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 80,  255, 255, 255,
    80,  255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255,
    255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255,
    255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 96,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255,
    0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255,
    255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255,
    255, 255, 96,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 96,  255, 255, 255, 0,
    255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255,
    0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255,
    255, 0,   255, 255, 255, 48,  255, 255, 255, 239, 255, 255, 255, 255, 255, 255, 255, 128, 255,
    255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,
    255, 255, 255, 207, 255, 255, 255, 207, 255, 255, 255, 16,  255, 255, 255, 0,   255, 255, 255,
    0,   255, 255, 255, 0,   255, 255, 255, 48,  255, 255, 255, 239, 255, 255, 255, 255, 255, 255,
    255, 159, 255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255,
    255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 207, 255, 255, 255, 255, 255, 255, 255, 207,
    255, 255, 255, 16,  255, 255, 255, 0,   255, 255, 255, 48,  255, 255, 255, 239, 255, 255, 255,
    255, 255, 255, 255, 159, 255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255,
    255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 16,  255,
    255, 255, 207, 255, 255, 255, 255, 255, 255, 255, 207, 255, 255, 255, 32,  255, 255, 255, 239,
    255, 255, 255, 255, 255, 255, 255, 159, 255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255,
    0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255,
    255, 0,   255, 255, 255, 0,   255, 255, 255, 16,  255, 255, 255, 207, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 207, 255, 255, 255, 16,  255, 255, 255, 0,
    255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255,
    0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255,
    255, 16,  255, 255, 255, 207, 255, 255, 255, 255, 255, 255, 255, 207, 255, 255, 255, 16,  255,
    255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,
    255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255,
    0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 16,  255, 255, 255, 128, 255, 255,
    255, 16,  255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255,
    255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,
    255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255,
    0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255,
    255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255,
    255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,
    255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255,
    0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255,
    255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255,
    255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,
    255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255,
    0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,   255, 255, 255, 0,
};
void cui_checkmark(cui_context *ui, float size, cui_color color) {
    if (!ui || !isfinite(size) || size <= 0)
        return;
    cui_paint_bitmap(ui, 0,
                     (cui_bitmap_paint){.bitmap = {cui_components_check_pixels, 16, 16, 64, 1},
                                        .opacity = 1,
                                        .custom_tint = true,
                                        .tint = color},
                     (cui_box){.width = size, .height = size});
}
static cui_color cui_checkbox_mix(cui_color from, cui_color to, float amount) {
    return (cui_color){
        (uint8_t)(from.r + (to.r - from.r) * amount), (uint8_t)(from.g + (to.g - from.g) * amount),
        (uint8_t)(from.b + (to.b - from.b) * amount), (uint8_t)(from.a + (to.a - from.a) * amount)};
}
bool cui_checkbox_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_str label,
                     cui_check_state *value, cui_checkbox_options options) {
    cui_design_metrics design;
    if (!ui || !t || !value || *value < CUI_CHECK_UNCHECKED || *value > CUI_CHECK_MIXED ||
        (options.custom_role &&
         (options.role < CUI_ROLE_NONE || options.role > CUI_ROLE_RADIOGROUP)) ||
        !cui_components_design(t, options.design_size, &design))
        return false;
    cui_str name = options.accessibility_label.length ? options.accessibility_label : label;
    bool extended =
        options.source_behavior || options.on_change || options.custom_role || options.tab_index;
    bool changed;
    if (extended) {
        changed = cui_base_check_ex(ui, id, name, value,
                                    (cui_check_options){.disabled = options.disabled,
                                                        .required = options.required,
                                                        .invalid = options.invalid,
                                                        .skip_tab = options.skip_tab,
                                                        .no_pointer_focus = options.source_behavior,
                                                        .controlled = options.source_behavior,
                                                        .tab_index = options.tab_index,
                                                        .custom_role = options.custom_role,
                                                        .role = options.role,
                                                        .on_change = options.on_change,
                                                        .change_user = options.change_user});
    } else {
        changed = cui_base_check(ui, id, name, value, options.disabled);
        cui_set_semantic_details(
            ui, id,
            (cui_semantic_details){.required = options.required, .invalid = options.invalid});
        cui_set_tab_stop(ui, id, !options.skip_tab);
    }
    float density = t->font_size / 16;
    float size = (options.size == CUI_CONTROL_XSMALL  ? 12
                  : options.size == CUI_CONTROL_SMALL ? 14
                  : options.size == CUI_CONTROL_LARGE ? 18
                                                      : 16) *
                 density;
    float font = (options.size == CUI_CONTROL_XSMALL  ? 12
                  : options.size == CUI_CONTROL_LARGE ? 16
                                                      : 14) *
                 density;
    if (options.design_size != CUI_DESIGN_INHERIT) {
        size = design.indicator_size;
        font = design.font_size;
    }
    float checked = *value == CUI_CHECK_CHECKED ? 1 : 0;
    float mixed = *value == CUI_CHECK_MIXED ? 1 : 0;
    if (options.state) {
        cui_checkbox_state *state = options.state;
        if (state->id != id) {
            *state = (cui_checkbox_state){.id = id};
        }
        bool reduced = options.reduced_motion || cui_reduced_motion(ui);
        cui_disclosure_motion_step_ex(&state->checked, checked != 0, cui_frame_delta(ui),
                                      t->spring_control, reduced);
        cui_disclosure_motion_step_ex(&state->mixed, mixed != 0, cui_frame_delta(ui),
                                      t->spring_control, reduced);
        /* Extreme finite spring settings can exceed float storage after integration. */
        if (!isfinite(state->checked.progress) || !isfinite(state->checked.velocity)) {
            state->checked.progress = checked;
            state->checked.velocity = 0;
        }
        if (!isfinite(state->mixed.progress) || !isfinite(state->mixed.velocity)) {
            state->mixed.progress = mixed;
            state->mixed.velocity = 0;
        }
        bool active = state->checked.progress != checked || state->checked.velocity != 0 ||
                      state->mixed.progress != mixed || state->mixed.velocity != 0;
        double now = cui_frame_time(ui);
        state->next_wake_seconds = active && isfinite(now) && now <= 1e12 ? now + 1.0 / 60 : 0;
        if (state->next_wake_seconds)
            cui_invalidate(ui);
        checked = fminf(1, fmaxf(0, state->checked.progress));
        mixed = fminf(1, fmaxf(0, state->mixed.progress));
    }
    bool selected = *value != CUI_CHECK_UNCHECKED;
    cui_color border = options.invalid ? t->danger : selected ? t->primary : t->input;
    cui_color background = selected ? t->primary : t->input_background;
    cui_color foreground = t->on_primary;
    if (options.state) {
        float visible = fminf(1, checked + mixed);
        background = cui_checkbox_mix(t->input_background, t->primary, visible);
        if (!options.invalid)
            border = cui_checkbox_mix(t->input, t->primary, visible);
    }
    if (options.disabled) {
        border.a /= 2;
        if (selected)
            background.a /= 2;
        foreground.a /= 2;
    }
    bool bounded = isfinite(options.width) && options.width > 0;
    float width = bounded ? fmaxf(size + 8 * density, options.width) : 0;
    cui_box outer = {.width = width,
                     .fixed_width = bounded,
                     .row = true,
                     .gap = 8 * density,
                     .radius = t->radius * .5f};
    if (!options.hide_focus_ring && cui_focused(ui) == id) {
        outer.border = t->focus;
        outer.border_width = 2;
    }
    cui_checkbox_paint paint = {.root = outer,
                                .indicator = {.width = size,
                                              .height = size,
                                              .background = background,
                                              .border = border,
                                              .border_width = 1,
                                              .radius = fminf(t->radius, 4 * density),
                                              .align_x = CUI_ALIGN_CENTER,
                                              .align_y = CUI_ALIGN_CENTER},
                                .foreground = foreground,
                                .label = options.disabled ? t->muted : t->text,
                                .label_font_size = font,
                                .label_line_height = options.source_behavior ? font * 1.25f : 0};
    cui_checkbox_style_fn layers[] = {options.styles.instance,
                                      *value == CUI_CHECK_CHECKED ? options.styles.checked
                                      : *value == CUI_CHECK_MIXED ? options.styles.mixed
                                                                  : NULL,
                                      options.disabled ? options.styles.disabled : NULL};
    for (size_t i = 0; i < sizeof layers / sizeof layers[0]; ++i)
        if (layers[i])
            layers[i](ui, options.styles.user, *value, options.disabled, &paint);
    foreground = paint.foreground;
    bounded = paint.root.fixed_width && isfinite(paint.root.width) && paint.root.width > 0;
    width = bounded ? paint.root.width : width;
    float indicator_width = fmaxf(0, paint.indicator.width);
    cui_begin_box(ui, id, paint.root);
    cui_begin_box(ui, 0, (cui_box){.width = indicator_width, .fixed_width = true, .gap = 0});
    if (label.length || options.content) {
        cui_begin_box(ui, 0, (cui_box){.height = size * .125f});
        cui_end_box(ui);
    }
    cui_begin_box(ui, 0, paint.indicator);
    size = fmaxf(0, fminf(paint.indicator.width, paint.indicator.height));
    if (options.indicator) {
        options.indicator(ui, options.indicator_user,
                          (cui_checkbox_indicator_info){.value = *value,
                                                        .width = paint.indicator.width,
                                                        .height = paint.indicator.height,
                                                        .checked_progress = checked,
                                                        .mixed_progress = mixed,
                                                        .foreground = foreground,
                                                        .disabled = options.disabled});
    } else if (options.state) {
        float glyph = fmaxf(0, size - 4 * density);
        if (checked > 0) {
            cui_color color = foreground;
            color.a = (uint8_t)(color.a * checked);
            cui_begin_absolute_box(ui, 0, (cui_box){0},
                                   (cui_rect){(paint.indicator.width - glyph) / 2,
                                              (paint.indicator.height - glyph) / 2, glyph, glyph});
            cui_checkmark(ui, glyph, color);
            cui_end_box(ui);
        }
        if (mixed > 0) {
            cui_color color = foreground;
            color.a = (uint8_t)(color.a * mixed);
            float bar = fmaxf(0, size - 6 * density);
            cui_begin_absolute_box(ui, 0, (cui_box){.background = color, .radius = density},
                                   (cui_rect){(paint.indicator.width - bar) / 2,
                                              (paint.indicator.height - 2 * density) / 2, bar,
                                              2 * density});
            cui_end_box(ui);
        }
    } else if (*value == CUI_CHECK_CHECKED) {
        cui_checkmark(ui, fmaxf(0, size - 4 * density), foreground);
    } else if (*value == CUI_CHECK_MIXED) {
        cui_begin_box(ui, 0,
                      (cui_box){.width = fmaxf(0, size - 6 * density),
                                .height = 2 * density,
                                .radius = density,
                                .background = foreground});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    cui_end_box(ui);
    if (label.length || options.content) {
        cui_begin_box(
            ui, 0,
            (cui_box){.width = bounded ? fmaxf(0, width - indicator_width - paint.root.gap) : 0,
                      .fixed_width = bounded,
                      .clip = options.source_behavior && bounded,
                      .gap = 4 * density});
        if (label.length) {
            cui_text_ex(ui, label, paint.label_font_size, paint.label, paint.label_line_height,
                        paint.label_style);
        }
        if (options.content)
            options.content(ui, options.content_user);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    if (options.state && options.tooltip.length) {
        cui_hover_options hover = options.tooltip_options;
        hover.trigger = id;
        hover.dt = cui_frame_delta(ui);
        hover.disabled = options.disabled;
        size_t count = 0;
        const cui_event *events = cui_frame_events(ui, &count);
        for (size_t i = 0; i < count; i++)
            if (events[i].type == CUI_EVENT_CANCEL && cui_event_target(ui, i) == id)
                options.state->tooltip = (cui_hover_state){.suppressed = true};
        cui_tooltip(ui, id ^ UINT64_C(0x43c19b7), t, &options.state->tooltip, options.tooltip,
                    hover);
        if (options.state->tooltip.pending) {
            double now = cui_frame_time(ui);
            if (isfinite(now) && now <= 1e12)
                options.state->next_wake_seconds = now + 1.0 / 60;
        }
    } else if (options.state) {
        options.state->tooltip = (cui_hover_state){0};
    }
    return changed;
}
bool cui_checkbox_source(cui_context *ui, cui_id id, const cui_theme *theme, cui_str label,
                         cui_check_state *value, cui_checkbox_options options) {
    options.source_behavior = true;
    return cui_checkbox_ex(ui, id, theme, label, value, options);
}
bool cui_checkbox(cui_context *ui, cui_id id, const cui_theme *t, cui_str name, bool *value,
                  bool disabled) {
    if (!value)
        return false;
    cui_check_state state = *value ? CUI_CHECK_CHECKED : CUI_CHECK_UNCHECKED;
    bool changed =
        cui_checkbox_ex(ui, id, t, name, &state, (cui_checkbox_options){.disabled = disabled});
    *value = state == CUI_CHECK_CHECKED;
    return changed;
}
static cui_id cui_components_child_id(cui_id parent, uint64_t child) {
    uint64_t x = parent + child * UINT64_C(0x9e3779b97f4a7c15);
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x ^= x >> 31;
    return x ? x : 1;
}
cui_switch_options cui_switch_options_default(void) {
    return (cui_switch_options){0};
}
cui_id cui_switch_track_id(cui_id id) {
    return cui_components_child_id(id, 5001);
}
cui_id cui_switch_thumb_id(cui_id id) {
    return cui_components_child_id(id, 5002);
}
static void cui_components_spring_step(cui_switch_state *s, double target,
                                       cui_spring_settings policy, double now, bool reduced) {
    s->next_wake_seconds = 0;
    if (!s->initialized || reduced || !policy.travel || policy.response == 0) {
        s->position = s->target = target;
        s->velocity = 0;
        s->updated_at = now;
        s->initialized = true;
        return;
    }
    if (s->position == target && s->velocity == 0) {
        s->target = target;
        return;
    }
    double dt = fmax(0, now - s->updated_at), w = 6.283185307179586 / policy.response,
           z = policy.damping;
    double y = s->position - s->target, v = s->velocity, position = y, velocity = v;
    if (z <= 1 && z * w * dt > 700) {
        position = velocity = 0;
    } else if (z < 1) {
        double wd = w * sqrt(1 - z * z), phase = fmod(dt, 6.283185307179586 / wd) * wd;
        double decay = exp(-z * w * dt), c = cos(phase), sn = sin(phase);
        position = decay * (y * c + (v + z * w * y) / wd * sn);
        velocity = decay * (v * c - (z * w * v + w * w * y) / wd * sn);
    } else if (z == 1) {
        double decay = exp(-w * dt), b = v + w * y;
        position = decay * (y + b * dt);
        velocity = decay * (v - w * b * dt);
    } else {
        double root = sqrt(z * z - 1), r1 = -w / (z + root), r2 = -w * (z + root);
        double a = (v - r2 * y) / (r1 - r2), b = y - a, e1 = exp(r1 * dt), e2 = exp(r2 * dt);
        position = a * e1 + b * e2;
        velocity = r1 * a * e1 + r2 * b * e2;
    }
    s->position = s->target + position;
    s->velocity = velocity;
    s->target = target;
    s->updated_at = fmax(s->updated_at, now);
    if (!isfinite(s->position) || !isfinite(s->velocity) ||
        (fabs(s->position - target) <= policy.epsilon && fabs(s->velocity) <= policy.epsilon)) {
        s->position = target;
        s->velocity = 0;
    } else
        s->next_wake_seconds = now + 1. / 60;
}
static void cui_components_switch_label(cui_context *ui, const cui_theme *t, cui_str label,
                                        cui_switch_options o, float font) {
    cui_begin_box(ui, 0, (cui_box){.width = o.width ? -1 : 0});
    if (o.content)
        o.content(ui, o.content_user);
    else if (label.length) {
        float line_height = o.size == CUI_CONTROL_SMALL || o.size == CUI_CONTROL_XSMALL ? 16
                            : o.size == CUI_CONTROL_LARGE                               ? 24
                                                                                        : 20;
        if (o.design_size != CUI_DESIGN_INHERIT) {
            cui_design_metrics design;
            if (cui_components_design(t, o.design_size, &design))
                line_height = design.line_height;
        }
        cui_text_with_line_height(ui, label, font, o.disabled ? t->muted : t->text, line_height);
    }
    cui_end_box(ui);
}

static bool cui_components_switch_valid(cui_context *ui, cui_id id, const cui_theme *t,
                                        cui_str label, cui_switch_options o,
                                        cui_design_metrics *design) {
    if (!ui || !id || !t || !cui_components_design(t, o.design_size, design) ||
        o.size < CUI_CONTROL_MEDIUM || o.size > CUI_CONTROL_XSMALL || !isfinite(o.width) ||
        !isfinite(o.now_seconds) || o.now_seconds < 0 || !isfinite(t->font_size) ||
        t->font_size <= 0 || !isfinite(t->radius) || t->radius < 0 ||
        !isfinite(t->spring_move.response) || t->spring_move.response < 0 ||
        !isfinite(t->spring_move.damping) || t->spring_move.damping < 0 ||
        !isfinite(t->spring_move.epsilon) || t->spring_move.epsilon <= 0 ||
        (label.length && !label.data) ||
        (o.accessibility_label.length && !o.accessibility_label.data) ||
        (o.tooltip.length && !o.tooltip.data))
        return false;
    return true;
}
static void cui_components_switch_paint(cui_context *ui, cui_id id, const cui_theme *t,
                                        cui_str label, const bool *value, cui_switch_state *state,
                                        cui_switch_options o, cui_design_metrics design,
                                        bool lifecycle) {
    bool small = o.size == CUI_CONTROL_SMALL || o.size == CUI_CONTROL_XSMALL;
    float width = small ? 28 : o.size == CUI_CONTROL_LARGE ? 44 : 36;
    float height = small ? 16 : o.size == CUI_CONTROL_LARGE ? 24 : 20;
    float font = (o.size == CUI_CONTROL_XSMALL  ? 12
                  : o.size == CUI_CONTROL_LARGE ? 16
                                                : 14) *
                 t->font_size / 16;
    if (o.design_size != CUI_DESIGN_INHERIT) {
        height = design.indicator_size + 4;
        width = height * 1.8f;
        font = design.font_size;
    }
    float thumb = height - 4, target = *value ? width - thumb - 4 : 0;
    float radius = t->radius >= 4 ? height : t->radius;
    if (state)
        cui_components_spring_step(state, target, t->spring_move, o.now_seconds, o.reduced_motion);
    float position = state ? (float)state->position : target;
    bool label_present = label.length || o.content;
    cui_begin_box(ui, id,
                  (cui_box){.width = o.width,
                            .row = true,
                            .gap = label_present ? 8 : 0,
                            .align_y = o.design_size == CUI_DESIGN_INHERIT ? CUI_ALIGN_START
                                                                           : CUI_ALIGN_CENTER});
    if (o.label_left && label_present)
        cui_components_switch_label(ui, t, label, o, font);
    cui_color background = *value ? (o.custom_color ? o.color : t->primary) : t->switch_background;
    if (o.disabled)
        background.a /= 2;
    bool focused = !o.disabled && !o.hide_focus_ring && cui_focused(ui) == id;
    cui_id track = cui_switch_track_id(id);
    cui_begin_box(ui, track,
                  (cui_box){.width = width,
                            .height = height,
                            .padding = 2,
                            .radius = radius,
                            .background = background,
                            .border = focused ? t->focus : (cui_color){0},
                            .border_width = 1,
                            .clip = true,
                            .offset_x = position,
                            .align_y = CUI_ALIGN_CENTER});
    cui_begin_box(
        ui, cui_switch_thumb_id(id),
        (cui_box){
            .width = thumb, .height = thumb, .radius = radius, .background = t->switch_thumb});
    cui_end_box(ui);
    cui_end_box(ui);
    if (!o.label_left && label_present)
        cui_components_switch_label(ui, t, label, o, font);
    cui_end_box(ui);
    if (focused) {
        cui_rect bounds = cui_bounds(ui, track);
        cui_color color = t->focus;
        color.a /= 2;
        if (bounds.w > 0) {
            cui_begin_portal(ui, cui_components_child_id(id, 5003),
                             (cui_box){.radius = radius + 3, .border = color, .border_width = 3},
                             (cui_rect){bounds.x - 3, bounds.y - 3, bounds.w + 6, bounds.h + 6}, 1);
            cui_end_box(ui);
        }
    }
    if (lifecycle && state && o.tooltip.length) {
        cui_hover_options tooltip = o.tooltip_options;
        tooltip.trigger = id;
        tooltip.disabled |= o.disabled;
        cui_tooltip(ui, cui_components_child_id(id, 5004), t, &state->tooltip, o.tooltip, tooltip);
    }
    if (lifecycle && state && state->next_wake_seconds)
        cui_invalidate(ui);
}
bool cui_switch_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_str label, bool *value,
                   cui_switch_state *state, cui_switch_options o) {
    cui_design_metrics design;
    if (!ui || !id || !t || !value || !cui_components_design(t, o.design_size, &design) ||
        o.size < CUI_CONTROL_MEDIUM || o.size > CUI_CONTROL_XSMALL || !isfinite(o.width) ||
        !isfinite(o.now_seconds) || o.now_seconds < 0 || !isfinite(t->font_size) ||
        t->font_size <= 0 || !isfinite(t->radius) || t->radius < 0 ||
        !isfinite(t->spring_move.response) || t->spring_move.response < 0 ||
        !isfinite(t->spring_move.damping) || t->spring_move.damping < 0 ||
        !isfinite(t->spring_move.epsilon) || t->spring_move.epsilon <= 0 ||
        (label.length && !label.data) ||
        (o.accessibility_label.length && !o.accessibility_label.data) ||
        (o.tooltip.length && !o.tooltip.data))
        return false;
    cui_str name = o.accessibility_label.length ? o.accessibility_label : label;
    bool requested = *value;
    bool changed = cui_base_switch(ui, id, name, &requested, o.disabled);
    if (changed) {
        if (o.on_change)
            o.on_change(o.change_user, requested);
        else
            *value = requested;
    }
    cui_update_semantic(ui, id, *value, *value ? 1 : 0);
    cui_set_tab_stop(ui, id, !o.skip_tab);
    cui_components_switch_paint(ui, id, t, label, value, state, o, design, true);
    return changed;
}
bool cui_switch(cui_context *ui, cui_id id, const cui_theme *t, cui_str name, bool *value,
                bool disabled) {
    return cui_switch_ex(ui, id, t, name, value, NULL, (cui_switch_options){.disabled = disabled});
}

cui_toggle_options cui_toggle_options_default(void) {
    return (cui_toggle_options){0};
}
static bool cui_components_toggle_valid(cui_str label, cui_toggle_options o) {
    float radii[] = {o.corner_radii.top_left, o.corner_radii.top_right, o.corner_radii.bottom_left,
                     o.corner_radii.bottom_right};
    float edges[] = {o.border_edges.left, o.border_edges.right, o.border_edges.top,
                     o.border_edges.bottom};
    for (size_t i = 0; i < 4; i++) {
        if (o.custom_corners && (!isfinite(radii[i]) || radii[i] < 0))
            return false;
        if (o.custom_border_edges && (!isfinite(edges[i]) || edges[i] < 0))
            return false;
    }
    return o.size >= CUI_CONTROL_MEDIUM && o.size <= CUI_CONTROL_XSMALL &&
           o.design_size >= CUI_DESIGN_INHERIT && o.design_size <= CUI_DESIGN_9XL &&
           o.variant >= CUI_TOGGLE_GHOST && o.variant <= CUI_TOGGLE_OUTLINE && isfinite(o.width) &&
           (!label.length || label.data) &&
           (!o.accessibility_label.length || o.accessibility_label.data) &&
           (!o.tooltip.length || o.tooltip.data);
}

void cui_toggle_paint(cui_context *ui, cui_id id, const cui_theme *t, cui_str label, bool value,
                      cui_toggle_options o) {
    cui_design_metrics design;
    if (!ui || !id || !t || !cui_components_design(t, o.design_size, &design) ||
        !cui_components_toggle_valid(label, o) || !isfinite(t->font_size) || t->font_size <= 0 ||
        !isfinite(t->radius) || t->radius < 0)
        return;
    bool accent = value || (!o.disabled && cui_hovered(ui, id));
    float side = o.size == CUI_CONTROL_XSMALL  ? 20
                 : o.size == CUI_CONTROL_SMALL ? 24
                 : o.size == CUI_CONTROL_LARGE ? 36
                                               : 32;
    float padding = o.size == CUI_CONTROL_XSMALL  ? 2
                    : o.size == CUI_CONTROL_SMALL ? 4
                    : o.size == CUI_CONTROL_LARGE ? 12
                                                  : 8;
    float font = (o.size == CUI_CONTROL_XSMALL  ? 12
                  : o.size == CUI_CONTROL_LARGE ? 16
                                                : 14) *
                 t->font_size / 16;
    if (o.design_size != CUI_DESIGN_INHERIT) {
        side = design.control_height;
        padding = design.padding_x;
        font = design.font_size;
    }
    cui_box box = {.width = o.width,
                   .height = side,
                   .radius = t->radius,
                   .background = accent                            ? t->accent
                                 : o.variant == CUI_TOGGLE_OUTLINE ? t->background
                                                                   : (cui_color){0},
                   .border = t->border,
                   .border_width = o.variant == CUI_TOGGLE_OUTLINE ? 1 : 0,
                   .align_x = CUI_ALIGN_CENTER,
                   .align_y = CUI_ALIGN_CENTER,
                   .custom_corners = o.custom_corners,
                   .corner_radii = o.corner_radii,
                   .custom_border_edges = o.custom_border_edges && o.variant == CUI_TOGGLE_OUTLINE,
                   .border_edges = o.border_edges};
    if (o.focus_ring && !o.disabled)
        cui_components_focus(ui, id, t, &box);
    cui_begin_box(ui, id, box);
    /* A zero-height minimum spacer preserves min-width without constraining arbitrary children. */
    if (o.width == 0) {
        cui_begin_box(ui, 0, (cui_box){.width = side, .height = 0, .fixed_height = true});
        cui_end_box(ui);
    }
    cui_begin_box(ui, 0, (cui_box){.row = true, .height = -1, .align_y = CUI_ALIGN_CENTER});
    cui_begin_box(ui, 0, (cui_box){.width = padding, .height = 0, .fixed_height = true});
    cui_end_box(ui);
    if (o.content)
        o.content(ui, o.content_user);
    else if (label.length)
        cui_text_with_line_height(ui, label, font, accent ? t->accent_foreground : t->text,
                                  o.design_size == CUI_DESIGN_INHERIT ? font * 1.25f
                                                                      : design.line_height);
    cui_begin_box(ui, 0, (cui_box){.width = padding, .height = 0, .fixed_height = true});
    cui_end_box(ui);
    cui_end_box(ui);
    cui_end_box(ui);
}
bool cui_toggle_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_str label, bool *value,
                   cui_hover_state *tooltip_state, cui_toggle_options o) {
    cui_design_metrics design;
    if (!ui || !id || !t || !value || !cui_components_design(t, o.design_size, &design) ||
        !cui_components_toggle_valid(label, o) || !isfinite(t->font_size) || t->font_size <= 0 ||
        !isfinite(t->radius) || t->radius < 0)
        return false;
    cui_str name = o.accessibility_label.length ? o.accessibility_label
                   : o.tooltip.length           ? o.tooltip
                                                : label;
    bool requested = *value;
    bool changed = cui_base_toggle(ui, id, name, &requested, o.disabled);
    if (changed) {
        if (o.on_change)
            o.on_change(o.change_user, requested);
        else
            *value = requested;
    }
    cui_update_semantic(ui, id, *value, *value ? 1 : 0);
    cui_set_tab_stop(ui, id, !o.skip_tab);
    cui_toggle_paint(ui, id, t, label, *value, o);
    if (tooltip_state && o.tooltip.length) {
        cui_hover_options tooltip = o.tooltip_options;
        tooltip.trigger = id;
        tooltip.disabled |= o.disabled;
        cui_tooltip(ui, cui_components_child_id(id, 5100), t, tooltip_state, o.tooltip, tooltip);
    }
    return changed;
}
bool cui_toggle(cui_context *ui, cui_id id, const cui_theme *t, cui_str label, bool *value,
                bool disabled) {
    return cui_toggle_ex(ui, id, t, label, value, NULL, (cui_toggle_options){.disabled = disabled});
}
typedef struct {
    cui_toggle_group_style_options *options;
    size_t count, index;
    const bool *values;
} cui_components_toggle_request;
static void cui_components_toggle_group_request(void *user, bool requested) {
    cui_components_toggle_request *r = user;
    for (size_t i = 0; i < r->count; i++)
        r->options->requested_values[i] = r->values[i];
    r->options->requested_values[r->index] = requested;
    r->options->on_change(r->options->change_user, r->options->requested_values, r->count);
}
bool cui_toggle_group_ex(cui_context *ui, cui_id id, const cui_theme *t,
                         const cui_toggle_item *items, bool *values, size_t count,
                         cui_toggle_group_style_options o) {
    cui_design_metrics design;
    if (!ui || !id || !t || !cui_components_design(t, o.design_size, &design) ||
        (count && (!items || !values)) || o.size < CUI_CONTROL_MEDIUM ||
        o.size > CUI_CONTROL_XSMALL || o.variant < CUI_TOGGLE_GHOST ||
        o.variant > CUI_TOGGLE_OUTLINE || !isfinite(o.gap) || o.gap < 0 ||
        (o.accessibility_label.length && !o.accessibility_label.data) ||
        (o.on_change &&
         (!o.requested_values || o.requested_capacity < count || o.requested_values == values)))
        return false;
    if (!isfinite(t->font_size) || t->font_size <= 0 || !isfinite(t->radius) || t->radius < 0)
        return false;
    if (o.on_change) {
        uintptr_t a = (uintptr_t)values, b = (uintptr_t)o.requested_values;
        if ((a > b ? a - b : b - a) < count * sizeof(bool))
            return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (!items[i].id || items[i].id == id ||
            !cui_components_toggle_valid(items[i].label, items[i].options))
            return false;
        for (size_t j = 0; j < i; j++)
            if (items[i].id == items[j].id)
                return false;
    }
    if (o.on_change)
        for (size_t i = 0; i < count; i++)
            o.requested_values[i] = values[i];
    cui_interact(ui, id, CUI_ROLE_TOOLBAR, o.accessibility_label, o.disabled, false, 0);
    cui_set_semantic_details(ui, id,
                             (cui_semantic_details){.orientation = CUI_ORIENTATION_HORIZONTAL});
    cui_begin_box(ui, id,
                  (cui_box){.row = true,
                            .gap = o.custom_gap  ? o.gap
                                   : o.segmented ? 0
                                                 : 8});
    bool changed = false;
    for (size_t i = 0; i < count; i++) {
        cui_toggle_options child = items[i].options;
        child.size = o.size;
        if (o.design_size != CUI_DESIGN_INHERIT)
            child.design_size = o.design_size;
        child.variant = o.variant;
        child.disabled |= o.disabled;
        child.on_change = NULL;
        child.change_user = NULL;
        if (o.segmented) {
            child.custom_corners = true;
            child.corner_radii = (cui_corner_radii){.top_left = i == 0 ? t->radius : 0,
                                                    .bottom_left = i == 0 ? t->radius : 0,
                                                    .top_right = i + 1 == count ? t->radius : 0,
                                                    .bottom_right = i + 1 == count ? t->radius : 0};
            child.custom_border_edges = true;
            float border = o.variant == CUI_TOGGLE_OUTLINE ? 1 : 0;
            child.border_edges = (cui_border_edges){
                .left = i == 0 ? border : 0, .right = border, .top = border, .bottom = border};
        }
        cui_components_toggle_request request = {&o, count, i, values};
        if (o.on_change) {
            child.on_change = cui_components_toggle_group_request;
            child.change_user = &request;
        }
        changed |= cui_toggle_ex(ui, items[i].id, t, items[i].label, &values[i],
                                 items[i].tooltip_state, child);
    }
    cui_end_box(ui);
    if (changed && o.on_change) {
        /* A controlled callback may replace siblings that were already painted.
           Publish the owner's final state now and repaint the completed group. */
        for (size_t i = 0; i < count; i++)
            cui_update_semantic(ui, items[i].id, values[i], values[i] ? 1 : 0);
        cui_invalidate(ui);
    }
    return changed;
}

bool cui_radio_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_str name, int *value,
                  int option, cui_radio_options options) {
    cui_design_metrics design;
    if (!ui || !t || !value || !cui_components_design(t, options.design_size, &design))
        return false;
    cui_str accessible = options.accessibility_label.length ? options.accessibility_label : name;
    bool changed = cui_base_radio(ui, id, accessible, value, option, options.disabled);
    cui_set_semantic_details(
        ui, id, (cui_semantic_details){.required = options.required, .invalid = options.invalid});
    cui_set_tab_stop(ui, id, !options.skip_tab);
    float density = t->font_size / 16;
    float size = (options.size == CUI_CONTROL_XSMALL  ? 12
                  : options.size == CUI_CONTROL_SMALL ? 14
                  : options.size == CUI_CONTROL_LARGE ? 18
                                                      : 16) *
                 density;
    float font = (options.size == CUI_CONTROL_XSMALL  ? 12
                  : options.size == CUI_CONTROL_LARGE ? 16
                                                      : 14) *
                 density;
    if (options.design_size != CUI_DESIGN_INHERIT) {
        size = design.indicator_size;
        font = design.font_size;
    }
    bool selected = *value == option;
    cui_color ink = options.invalid ? t->danger : selected ? t->primary : t->input;
    cui_color dot = t->on_primary;
    cui_color background = selected ? t->primary : t->input_background;
    if (options.disabled) {
        ink.a /= 2;
        dot.a /= 2;
        if (selected)
            background.a /= 2;
    }
    cui_box outer = {.row = true, .gap = 8 * density, .radius = t->radius * .5f};
    if (!options.hide_focus_ring && cui_focused(ui) == id) {
        outer.border = t->focus;
        outer.border_width = 2;
    }
    cui_begin_box(ui, id, outer);
    cui_begin_box(ui, 0, (cui_box){.width = size});
    if (name.length || options.content) {
        cui_begin_box(ui, 0, (cui_box){.height = size * .125f});
        cui_end_box(ui);
    }
    cui_begin_box(ui, 0,
                  (cui_box){.width = size,
                            .height = size,
                            .radius = size / 2,
                            .border_width = 1,
                            .border = ink,
                            .background = background,
                            .align_x = CUI_ALIGN_CENTER,
                            .align_y = CUI_ALIGN_CENTER});
    if (selected) {
        cui_checkmark(ui, size - 4 * density, dot);
    }
    cui_end_box(ui);
    cui_end_box(ui);
    if (name.length || options.content) {
        cui_begin_box(ui, 0, (cui_box){.gap = 4 * density});
        if (name.length)
            cui_text(ui, name, font, options.disabled ? t->muted : t->text);
        if (options.content)
            options.content(ui, options.content_user);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return changed;
}
bool cui_radio(cui_context *ui, cui_id id, const cui_theme *t, cui_str name, int *value, int option,
               bool disabled) {
    return cui_radio_ex(ui, id, t, name, value, option, (cui_radio_options){.disabled = disabled});
}
void cui_label(cui_context *ui, const cui_theme *t, cui_str label) {
    cui_text(ui, label, t->font_size, t->text);
}
void cui_badge(cui_context *ui, cui_id id, const cui_theme *t, cui_str label) {
    cui_begin_box(ui, id, (cui_box){.padding = 4, .radius = t->radius, .background = t->primary});
    cui_text(ui, label, t->font_size * .8f, t->on_primary);
    cui_end_box(ui);
}
cui_badge_options cui_badge_defaults(void) {
    return (cui_badge_options){.maximum = 99, .size = CUI_CONTROL_MEDIUM};
}
bool cui_badge_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_badge_options o,
                  cui_button_content_fn content, void *user) {
    if (!ui || !id || !t || !cui_design_valid(o.design_size) || o.variant < CUI_BADGE_COUNT ||
        o.variant > CUI_BADGE_ICON || o.size < CUI_CONTROL_MEDIUM || o.size > CUI_CONTROL_XSMALL ||
        (o.name.length && !o.name.data) || (o.icon.length && !o.icon.data))
        return false;
    cui_design_metrics dm = {0};
    if (o.design_size && !cui_design_resolve(o.design_size, t->font_size / 16, &dm))
        return false;
    float badge_font = o.design_size ? dm.font_size * .75f : 10;
    char number[3 * sizeof(size_t) + 3];
    if (o.count > o.maximum)
        snprintf(number, sizeof(number), "%zu+", o.maximum);
    else
        snprintf(number, sizeof(number), "%zu", o.count);
    bool visible = o.variant != CUI_BADGE_COUNT || o.count > 0;
    cui_begin_box(ui, id, (cui_box){0});
    if (content)
        content(ui, user);
    if (visible) {
        float extent = o.size == CUI_CONTROL_LARGE ? 24 : o.size == CUI_CONTROL_MEDIUM ? 16 : 10;
        if (o.design_size)
            extent = dm.indicator_size;
        float x = o.variant != CUI_BADGE_COUNT ? 0
                                               : (o.size == CUI_CONTROL_LARGE    ? 1
                                                  : o.size == CUI_CONTROL_MEDIUM ? 3
                                                                                 : 4) *
                                                     strlen(number);
        float y = o.variant != CUI_BADGE_COUNT   ? 0
                  : o.size == CUI_CONTROL_LARGE  ? 2
                  : o.size == CUI_CONTROL_MEDIUM ? -3
                                                 : -4;
        cui_id overlay = id ^ UINT64_C(0x35d9258f62be9a0c);
        cui_id indicator = id ^ UINT64_C(0x2780a7c4f39251db);
        cui_begin_absolute_box(
            ui, overlay,
            (cui_box){.align_x = CUI_ALIGN_END,
                      .align_y = o.variant == CUI_BADGE_ICON ? CUI_ALIGN_END : CUI_ALIGN_START},
            (cui_rect){x, y, -1, -1});
        cui_box mark = {.background = o.custom_color ? o.color : t->danger,
                        .radius = 1024,
                        .align_x = CUI_ALIGN_CENTER,
                        .align_y = CUI_ALIGN_CENTER};
        if (o.variant == CUI_BADGE_COUNT) {
            mark.height = o.design_size ? dm.line_height * .75f + dm.padding_y : 14;
            mark.width =
                fmaxf(mark.height, cui_measure_text(ui, cui_string(number), badge_font).width +
                                       (o.design_size ? dm.padding_x : 4));
        } else {
            mark.width = mark.height = o.variant == CUI_BADGE_DOT
                                           ? (o.design_size ? dm.indicator_size * .375f : 6)
                                           : extent;
            if (o.variant == CUI_BADGE_ICON) {
                mark.border = t->background;
                mark.border_width = 1;
            }
        }
        cui_begin_box(ui, indicator, mark);
        cui_str name = o.name.length                  ? o.name
                       : o.variant == CUI_BADGE_COUNT ? cui_string(number)
                       : o.variant == CUI_BADGE_DOT   ? cui_string("Notification")
                                                      : cui_string("Status");
        cui_interact(ui, indicator, CUI_ROLE_STATUS, name, false, false, (float)o.count);
        cui_set_semantic_details(ui, indicator,
                                 (cui_semantic_details){.parent = id, .no_focus = true});
        cui_set_tab_stop(ui, indicator, false);
        if (o.variant == CUI_BADGE_COUNT)
            cui_text_with_line_height(ui, cui_string(number), badge_font,
                                      (cui_color){255, 255, 255, 255}, badge_font);
        else if (o.variant == CUI_BADGE_ICON)
            cui_icon_named(ui, indicator ^ UINT64_C(0x2c38f42991a75e63), t, o.icon,
                           (cui_icon_options){.custom_size = extent - 2,
                                              .size = CUI_ICON_CUSTOM,
                                              .color = {255, 255, 255, 255},
                                              .custom_color = true});
        cui_end_box(ui);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return true;
}
cui_progress_options cui_progress_defaults(void) {
    return (cui_progress_options){.source_motion = true, .duration = .18f};
}
bool cui_progress_ex(cui_context *ui, cui_id id, const cui_theme *t, float fraction, float width,
                     cui_progress_options o) {
    if (!ui || !id || !t || !cui_design_valid(o.design_size) || o.size < CUI_CONTROL_MEDIUM ||
        o.size > CUI_CONTROL_XSMALL || (o.name.length && !o.name.data))
        return false;
    cui_design_metrics dm = {0};
    if (o.design_size && !cui_design_resolve(o.design_size, t->font_size / 16, &dm))
        return false;
    bool reduced = o.reduced_motion || cui_reduced_motion(ui);
    cui_progress_geometry geometry =
        cui_progress_measure(fraction, o.indeterminate, o.source_motion, o.now_seconds, reduced);
    bool moving = geometry.animating;
    if (!o.indeterminate && o.motion) {
        cui_progress_motion_result transition;
        if (!cui_progress_animate(o.motion, geometry.fraction, o.now_seconds,
                                  o.duration == 0 ? .18f : o.duration, reduced, &transition))
            return false;
        geometry.length = transition.value;
        moving = transition.active;
    }
    if (width <= 0 || !isfinite(width))
        width = 160;
    float height = isfinite(o.height) && o.height > 0 ? o.height
                   : o.size == CUI_CONTROL_XSMALL     ? 4
                   : o.size == CUI_CONTROL_SMALL      ? 6
                   : o.size == CUI_CONTROL_LARGE      ? 10
                                                      : 8;
    if (o.design_size && !(isfinite(o.height) && o.height > 0))
        height = dm.control_height * .25f;
    cui_color color = o.custom_color ? o.color : t->primary;
    cui_color track = color;
    track.a = (uint8_t)(track.a * .2f);
    float radius = t->radius == 0 ? 0 : height * .5f;
    cui_begin_box(ui, id,
                  (cui_box){.width = width,
                            .height = height,
                            .radius = radius,
                            .custom_corners = o.custom_corners,
                            .corner_radii = o.corners,
                            .background = track,
                            .row = true,
                            .clip = true});
    cui_base_progress(ui, id, o.name, geometry.fraction, o.indeterminate);
    float offset = width * geometry.start, fill_width = width * geometry.length;
    if (offset > 0) {
        cui_begin_box(ui, id ^ UINT64_C(0x1829d4cd19342f11),
                      (cui_box){.width = offset, .height = height});
        cui_end_box(ui);
    }
    if (fill_width > 0) {
        cui_begin_box(ui, id ^ UINT64_C(0x7129d4cd19342f11),
                      (cui_box){.width = fill_width,
                                .height = height,
                                .radius = radius,
                                .custom_corners = o.custom_corners,
                                .corner_radii = o.corners,
                                .background = color});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    if (moving)
        cui_invalidate(ui);
    return moving;
}
void cui_progress(cui_context *ui, cui_id id, const cui_theme *t, float fraction, float width) {
    cui_progress_ex(ui, id, t, fraction, width, (cui_progress_options){0});
}
bool cui_slider(cui_context *ui, cui_id id, const cui_theme *t, cui_str name, float *value,
                float min, float max, float step, bool disabled) {
    cui_box b = cui_components_control(t, disabled, false);
    b.width = 260;
    b.row = false;
    b.height = 0;
    cui_components_focus(ui, id, t, &b);
    cui_begin_box(ui, id, b);
    bool changed = cui_base_slider(ui, id, name, value, min, max, step, disabled);
    cui_text(ui, name, t->font_size, disabled ? t->muted : t->text);
    float ratio = value && isfinite(*value) && max > min ? (*value - min) / (max - min) : 0;
    cui_color track = t->primary;
    track.a = (uint8_t)lroundf(track.a * .2f);
    cui_begin_box(ui, id ^ UINT64_C(0x39c1a2170384edaa),
                  (cui_box){.width = 240,
                            .height = 8,
                            .fixed_height = true,
                            .clip = true,
                            .background = track,
                            .radius = t->radius == 0 ? 0 : 4});
    float fill = fmaxf(0, fminf(1, ratio)) * 240;
    if (fill > 0) {
        cui_begin_box(ui, 0,
                      (cui_box){.width = fill,
                                .height = 8,
                                .fixed_height = true,
                                .background = t->primary,
                                .radius = t->radius == 0 ? 0 : 4});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    cui_end_box(ui);
    return changed;
}
void cui_separator(cui_context *ui, cui_id id, const cui_theme *t, float width) {
    cui_begin_box(
        ui, id, (cui_box){.width = width != 0 ? width : 160, .height = 1, .background = t->border});
    cui_end_box(ui);
}

cui_separator_options cui_separator_defaults(void) {
    return (cui_separator_options){.length = 160};
}
bool cui_separator_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_separator_options o) {
    if (!ui || !id || !t || !isfinite(o.length) || o.length < 0 || o.length > 4096 ||
        (o.label.length && !o.label.data) || !isfinite(t->font_size) || t->font_size <= 0 ||
        t->font_size > 4096)
        return false;
    float length = o.length ? o.length : 160;
    float font = t->font_size * .75f, line_height = t->font_size;
    float label_width = o.has_label ? cui_measure_text(ui, o.label, font).width + 16 : 0;
    float label_height = o.has_label ? line_height + 8 : 0;
    if (!isfinite(label_width) || !isfinite(label_height))
        return false;
    float width = o.vertical ? fmaxf(1, label_width) : length;
    float height = o.vertical ? length : fmaxf(1, label_height);
    cui_color color = o.custom_color ? o.color : t->border;
    cui_begin_box(
        ui, id,
        (cui_box){.width = width, .height = height, .fixed_width = true, .fixed_height = true});
    float increment = o.dashed ? 6 : length;
    for (float offset = 0; offset < length; offset += increment) {
        float segment = o.dashed ? fminf(4, length - offset) : length;
        cui_rect rect = o.vertical ? (cui_rect){(width - 1) / 2, offset, 1, segment}
                                   : (cui_rect){offset, (height - 1) / 2, segment, 1};
        cui_begin_absolute_box(ui, 0, (cui_box){.background = color}, rect);
        cui_end_box(ui);
    }
    if (o.has_label) {
        cui_begin_absolute_box(ui, 0, (cui_box){.background = t->background},
                               (cui_rect){(width - label_width) / 2, (height - label_height) / 2,
                                          label_width, label_height});
        cui_begin_absolute_box(ui, 0, (cui_box){0},
                               (cui_rect){8, 4, label_width - 16, line_height});
        cui_text_line(ui, o.label, font, t->muted, line_height, (cui_text_style){0});
        cui_end_box(ui);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return true;
}

bool cui_input_measure(cui_input_options options, float root_font_size,
                       cui_input_metrics *metrics) {
    if (!metrics || options.size < CUI_INPUT_MEDIUM || options.size > CUI_INPUT_CUSTOM ||
        !isfinite(root_font_size) || root_font_size <= 0 || root_font_size > 4096 ||
        !isfinite(options.density) || options.density < 0 || options.density > 256 ||
        !isfinite(options.height) || options.height < 0 || options.height > 1000000 ||
        !isfinite(options.padding_x) || options.padding_x < 0 || options.padding_x > 1000000 ||
        !isfinite(options.padding_y) || options.padding_y < 0 || options.padding_y > 1000000)
        return false;
    float d = options.density > 0 ? options.density : root_font_size / 16;
    float h = 32, font = 14, px = 10, py = 8, gap = 6;
    switch (options.design_size == CUI_DESIGN_INHERIT ? options.size : CUI_INPUT_MEDIUM) {
    case CUI_INPUT_XSMALL:
        h = 20;
        font = 12;
        px = 4;
        py = 0;
        break;
    case CUI_INPUT_SMALL:
        h = 24;
        px = 8;
        py = 2;
        gap = 4;
        break;
    case CUI_INPUT_LARGE:
        h = 44;
        font = 16;
        px = 12;
        py = 10;
        gap = 8;
        break;
    case CUI_INPUT_CUSTOM:
        if (!isfinite(options.custom_size) || options.custom_size <= 0 ||
            options.custom_size > 4096)
            return false;
        h = options.custom_size / d;
        font = options.custom_size * .875f / d;
        px = 8;
        py = 2;
        break;
    default:
        break;
    }
    float line_height = 20 * d;
    if (options.design_size != CUI_DESIGN_INHERIT) {
        cui_design_metrics design;
        if (!cui_design_measure(options.design_size, d, &design))
            return false;
        h = design.control_height / d;
        font = design.font_size / d;
        px = design.padding_x / d;
        py = design.padding_y / d;
        gap = design.gap / d;
        line_height = design.line_height;
    }
    *metrics =
        (cui_input_metrics){options.custom_height || options.height > 0 ? options.height : h * d,
                            font * d,
                            line_height,
                            options.custom_padding ? options.padding_x : px * d,
                            options.custom_padding ? options.padding_y : py * d,
                            gap * d};
    return true;
}
cui_id cui_input_viewport_id(cui_id id) {
    return id ^ UINT64_C(0xb1642b97583fbd63);
}
static float cui_input_alignment(float available, float text, cui_align align) {
    float extra = fmaxf(0, available - text);
    return align == CUI_ALIGN_CENTER ? extra * .5f : align == CUI_ALIGN_END ? extra : 0;
}
/* Optional visual session is owned by a UI resource, shared by reduction and
 * paint in this frame. Collection refreshes it even when a host changes its
 * font registrations without changing the editor bytes. */
typedef struct {
    cui_text_geometry geometry;
    char *text;
    size_t length;
    float size;
    bool attempted, valid;
    const cui_edit *model;
    uint64_t revision;
    size_t caret_byte;
    cui_text_affinity affinity;
    bool affinity_valid;
} cui_input_visual_cache;
static void cui_input_visual_clear(void *payload) {
    cui_input_visual_cache *cache = payload;
    cui_release_text_geometry(&cache->geometry);
    free(cache->text);
    cache->geometry = (cui_text_geometry){0};
    cache->text = NULL;
    cache->attempted = cache->valid = false;
    cache->length = 0;
    cache->size = 0;
}
static cui_input_visual_cache *cui_input_visual_resource(cui_context *ui, cui_id id) {
    if (!cui_has_text_geometry(ui))
        return NULL;
    return cui_resource_get(ui, id, UINT64_C(0x290fa458736bc921), sizeof(cui_input_visual_cache),
                            cui_input_visual_clear, cui_input_visual_clear);
}
static void cui_input_visual_bind(cui_input_visual_cache *cache, const cui_edit *edit) {
    if (!cache)
        return;
    uint64_t revision = cui_edit_get_state(edit).revision;
    if (cache->model != edit || cache->revision != revision)
        cache->affinity_valid = false;
    cache->model = edit;
    cache->revision = revision;
}
static bool cui_input_visual_acquire_impl(cui_input_visual_cache *cache, cui_context *ui,
                                          cui_str text, float size, bool staged_mask) {
    if (!cache || text.length == SIZE_MAX)
        return false;
    if (cache->attempted && cache->size == size && cache->length == text.length &&
        (!text.length || !memcmp(cache->text, text.data, text.length)))
        return cache->valid;
    if (!staged_mask && text.length > 65535)
        return false;
    cui_input_visual_clear(cache);
    cache->text = malloc(text.length + 1);
    if (!cache->text)
        return false;
    if (text.length)
        memcpy(cache->text, text.data, text.length);
    cache->text[text.length] = 0;
    cache->length = text.length;
    cache->size = size;
    cache->attempted = true;
    cache->valid =
        cui_get_text_geometry(ui, (cui_str){cache->text, text.length}, size, (cui_text_style){0},
                              &cache->geometry) == CUI_TEXT_LAYOUT_OK;
    return cache->valid;
}
static bool cui_input_visual_acquire(cui_input_visual_cache *cache, cui_context *ui, cui_str text,
                                     float size) {
    return cui_input_visual_acquire_impl(cache, ui, text, size, false);
}
typedef struct {
    cui_input_hit_geometry measured;
    cui_input_visual_cache *cache;
} cui_input_cached_hit;
static size_t cui_input_cached_hit_test(void *payload, cui_str text, float x, float y) {
    cui_input_cached_hit *hit = payload;
    if (hit->cache && hit->cache->model)
        cui_input_visual_bind(hit->cache, hit->cache->model);
    if (cui_input_visual_acquire(hit->cache, hit->measured.ui, text, hit->measured.font_size)) {
        cui_input_visual_hit_geometry visual = {hit->measured, &hit->cache->geometry,
                                                &hit->cache->affinity};
        size_t byte = cui_base_input_visual_hit_test(&visual, text, x, y);
        if (byte != SIZE_MAX) {
            hit->cache->caret_byte = byte;
            hit->cache->affinity_valid = true;
            return byte;
        }
    }
    return cui_base_input_measured_hit_test(&hit->measured, text, x, y);
}
static bool cui_input_visual_caret(const cui_text_geometry *geometry, size_t byte,
                                   cui_text_affinity preferred, float *x) {
    cui_text_caret carets[2];
    size_t count;
    if (geometry->ops->carets(geometry->session, byte, carets, 2, &count) != CUI_TEXT_LAYOUT_OK ||
        !count || count > 2)
        return false;
    size_t chosen = 0;
    for (size_t i = 0; i < count; ++i) {
        if (carets[i].byte_offset != byte || !isfinite(carets[i].x) || !isfinite(carets[i].y) ||
            !isfinite(carets[i].height) || carets[i].height < 0 ||
            (unsigned)carets[i].affinity > CUI_TEXT_DOWNSTREAM)
            return false;
        if (carets[i].affinity == preferred)
            chosen = i;
    }
    *x = carets[chosen].x;
    return true;
}
static bool cui_input_visual_selection(const cui_text_geometry *geometry, size_t start, size_t end,
                                       cui_text_selection_rect **rects, size_t *count) {
    size_t needed;
    if (geometry->ops->selection(geometry->session, start, end, NULL, 0, &needed) !=
            CUI_TEXT_LAYOUT_OK ||
        needed > 4096)
        return false;
    cui_text_selection_rect *result = needed ? malloc(needed * sizeof *result) : NULL;
    if (needed && !result)
        return false;
    size_t written;
    if (geometry->ops->selection(geometry->session, start, end, result, needed, &written) !=
            CUI_TEXT_LAYOUT_OK ||
        written != needed) {
        free(result);
        return false;
    }
    for (size_t i = 0; i < needed; ++i) {
        cui_rect bounds = result[i].bounds;
        if (!isfinite(bounds.x) || !isfinite(bounds.y) || !isfinite(bounds.w) ||
            !isfinite(bounds.h) || bounds.w < 0 || bounds.h < 0 || result[i].byte_start < start ||
            result[i].byte_end > end || result[i].byte_start > result[i].byte_end) {
            free(result);
            return false;
        }
    }
    *rects = result;
    *count = needed;
    return true;
}
typedef struct {
    char *bytes;
    size_t capacity, length;
    bool owned, ready;
    cui_input_visual_cache visual;
    uint64_t font_revision;
} cui_input_mask_slot;
typedef struct {
    cui_input_mask_slot slots[3];
    size_t committed, visible, stage;
    bool valid, pending_provisional;
    cui_context *ui;
    float font_size, bound_font_size;
    uintptr_t model;
    uint64_t revision, font_revision;
    cui_input_mask_storage *storage;
    cui_input_mask_storage storage_snapshot;
    cui_input_mask_allocator allocator;
} cui_input_mask_cache;
static void cui_input_mask_clear(void *payload) {
    cui_input_mask_cache *m = payload;
    for (size_t i = 0; i < 3; i++) {
        cui_input_visual_clear(&m->slots[i].visual);
        if (m->slots[i].owned) {
            if (m->allocator.release)
                m->allocator.release(m->allocator.user, m->slots[i].bytes);
            else
                free(m->slots[i].bytes);
        }
    }
    *m = (cui_input_mask_cache){0};
}
static cui_input_mask_cache *cui_input_mask_resource(cui_context *ui, cui_id id) {
    return cui_resource_get(ui, id, UINT64_C(0x731d52aae92865b1), sizeof(cui_input_mask_cache),
                            NULL, cui_input_mask_clear);
}
static size_t cui_input_mask_stage(cui_input_mask_cache *m) {
    for (size_t i = 0; i < 3; i++)
        if (!m->valid || (i != m->visible && i != m->committed))
            return i;
    return 0;
}
static cui_edit_result cui_input_mask_prepare(void *payload, cui_str candidate, bool provisional) {
    cui_input_mask_cache *m = payload;
    size_t bytes;
    cui_edit_result error = cui_edit_mask_offset(candidate, candidate.length, &bytes);
    if (error != CUI_EDIT_OK)
        return error;
    if (bytes == SIZE_MAX)
        return CUI_EDIT_CAPACITY;
    m->stage = cui_input_mask_stage(m);
    cui_input_mask_slot *slot = &m->slots[m->stage];
    if (m->storage && bytes >= slot->capacity)
        return CUI_EDIT_CAPACITY;
    if (bytes >= slot->capacity) {
        size_t needed = bytes + 1;
        char *grown = m->allocator.allocate ? m->allocator.allocate(m->allocator.user, needed)
                                            : malloc(needed);
        if (!grown)
            return CUI_EDIT_CAPACITY;
        cui_input_visual_clear(&slot->visual);
        if (slot->owned) {
            if (m->allocator.release)
                m->allocator.release(m->allocator.user, slot->bytes);
            else
                free(slot->bytes);
        }
        slot->bytes = grown;
        slot->capacity = needed;
        slot->owned = true;
        slot->ready = false;
    }
    cui_str display;
    error = cui_edit_mask_text(candidate, slot->bytes, slot->capacity, &display);
    if (error != CUI_EDIT_OK)
        return error;
    uint64_t font_revision = cui_text_metrics_revision(m->ui);
    if (slot->font_revision != font_revision)
        cui_input_visual_clear(&slot->visual);
    if (cui_has_text_geometry(m->ui) &&
        !cui_input_visual_acquire_impl(&slot->visual, m->ui, display, m->font_size, true))
        return CUI_EDIT_PRESENTATION;
    if (!cui_has_text_geometry(m->ui)) {
        float width = cui_measure_text(m->ui, display, m->font_size).width;
        if (!isfinite(width) || width < 0)
            return CUI_EDIT_PRESENTATION;
    }
    slot->length = display.length;
    slot->ready = true;
    slot->font_revision = font_revision;
    m->pending_provisional = provisional;
    return CUI_EDIT_OK;
}
static void cui_input_mask_complete(void *payload, bool prepared, bool applied) {
    cui_input_mask_cache *m = payload;
    if (!prepared || !applied)
        return;
    m->visible = m->stage;
    if (!m->pending_provisional)
        m->committed = m->stage;
    m->valid = true;
}
static cui_edit_presentation cui_input_mask_presentation(cui_input_mask_cache *m) {
    cui_edit_presentation p = {
        .prepare = cui_input_mask_prepare, .user = m, .user_bytes = sizeof *m};
    if (m->storage) {
        p.caller_descriptor = (cui_edit_workspace){m->storage, sizeof *m->storage};
        p.caller_workspace_count = 3;
        for (size_t i = 0; i < 3; i++)
            p.caller_workspaces[i] = (cui_edit_workspace){m->storage_snapshot.buffers[i],
                                                          m->storage_snapshot.capacities[i]};
    }
    return p;
}
static cui_edit_result cui_input_mask_bind(cui_input_mask_cache *m, cui_context *ui, cui_edit *edit,
                                           cui_edit_composition *composition, cui_input_options o,
                                           float size, bool reset) {
    if (!m || (!!o.mask_allocator.allocate != !!o.mask_allocator.release))
        return CUI_EDIT_INVALID_ARGUMENT;
    bool rebind = reset || m->model != (uintptr_t)edit || m->storage != o.mask_storage ||
                  m->allocator.user != o.mask_allocator.user ||
                  m->allocator.allocate != o.mask_allocator.allocate ||
                  m->allocator.release != o.mask_allocator.release;
    if (o.mask_storage)
        for (size_t i = 0; i < 3; i++)
            rebind |= m->storage_snapshot.buffers[i] != o.mask_storage->buffers[i] ||
                      m->storage_snapshot.capacities[i] != o.mask_storage->capacities[i];
    if (rebind) {
        cui_input_mask_clear(m);
        m->model = (uintptr_t)edit;
        m->storage = o.mask_storage;
        m->allocator = o.mask_allocator;
        if (o.mask_storage) {
            m->storage_snapshot = *o.mask_storage;
            size_t minimum = SIZE_MAX;
            for (size_t i = 0; i < 3; i++)
                if (o.mask_storage->capacities[i] < minimum)
                    minimum = o.mask_storage->capacities[i];
            for (size_t i = 0; i < 3; i++) {
                m->slots[i].bytes = o.mask_storage->buffers[i];
                m->slots[i].capacity = minimum;
            }
        }
    }
    m->ui = ui;
    m->font_size = size;
    uint64_t revision = cui_edit_get_state(edit).revision,
             font_revision = cui_text_metrics_revision(ui);
    if (m->valid && m->revision == revision && m->font_revision == font_revision &&
        m->bound_font_size == size && (composition && composition->active))
        return CUI_EDIT_OK;
    if (m->valid && m->revision == revision && m->font_revision == font_revision &&
        m->bound_font_size == size) {
        if (!composition || !composition->active)
            m->visible = m->committed;
        return CUI_EDIT_OK;
    }
    size_t previous_visible = m->visible;
    bool retain_preedit = m->valid && composition && composition->active;
    cui_edit_request request = {.kind = CUI_EDIT_REQUEST_PRESENT,
                                .presentation = cui_input_mask_presentation(m)};
    cui_edit_checked_result result =
        cui_edit_apply_checked(edit, request, (cui_edit_acceptance){0});
    cui_input_mask_complete(m, result.presentation_prepared, result.presentation_applied);
    if (result.error != CUI_EDIT_OK)
        return result.error;
    if (composition && composition->active) {
        if (retain_preedit)
            m->visible = previous_visible;
        request.composition = composition;
        result = cui_edit_apply_checked(edit, request, (cui_edit_acceptance){0});
        cui_input_mask_complete(m, result.presentation_prepared, result.presentation_applied);
        if (result.error != CUI_EDIT_OK)
            return result.error;
    }
    m->revision = revision;
    m->font_revision = font_revision;
    m->bound_font_size = size;
    return CUI_EDIT_OK;
}
typedef struct {
    cui_input_cached_hit hit;
    cui_input_mask_cache *mask;
} cui_input_mask_hit;
static size_t cui_input_mask_hit_test(void *payload, cui_str actual, float x, float y) {
    cui_input_mask_hit *h = payload;
    if (!h->mask->valid)
        return SIZE_MAX;
    cui_input_mask_slot *slot = &h->mask->slots[h->mask->committed];
    cui_str display = {slot->bytes, slot->length};
    h->hit.cache = slot->visual.valid ? &slot->visual : NULL;
    size_t masked = cui_input_cached_hit_test(&h->hit, display, x, y), source = 0;
    if (masked == SIZE_MAX || cui_edit_unmask_offset(actual, masked, &source) != CUI_EDIT_OK)
        return SIZE_MAX;
    return source;
}
void cui_input_paint(cui_context *ui, cui_id id, const cui_theme *t, cui_str name, cui_edit *edit,
                     cui_input_state *state, cui_input_options options) {
    (void)name;
    cui_input_metrics metrics;
    if (!ui || !id || !t || !edit || !state ||
        !cui_input_measure(options, t->font_size, &metrics) || !isfinite(options.width) ||
        options.text_align < CUI_ALIGN_START || options.text_align > CUI_ALIGN_END)
        return;
    bool multiline = cui_edit_get_state(edit).multiline;
    bool focused = cui_focused(ui) == id && !options.disabled;
    float border = options.no_appearance || options.no_border ? 0 : 1;
    cui_color background = t->input_background, foreground = options.disabled ? t->muted : t->text;
    if (options.disabled) {
        background = t->input;
        background.a = (uint8_t)lroundf(background.a * .2f);
        foreground.a = (uint8_t)lroundf(foreground.a * .5f);
    }
    cui_color frame_border = options.invalid                       ? t->danger
                             : focused && !options.hide_focus_ring ? t->focus
                                                                   : t->input;
    float width = options.width ? options.width : 280;
    cui_begin_box(ui, id,
                  (cui_box){.width = width,
                            .height = multiline ? 0 : metrics.height,
                            .fixed_height = !multiline,
                            .row = !multiline,
                            .align_y = CUI_ALIGN_CENTER,
                            .clip = true,
                            .radius = options.no_appearance ? 0 : t->radius,
                            .background = options.no_appearance ? (cui_color){0} : background,
                            .border = frame_border,
                            .border_width = border});
    cui_str text = cui_edit_composition_view(edit, state->behavior.composition);
    cui_edit_state selection = cui_edit_composition_selection(edit, state->behavior.composition);
    cui_input_mask_cache *mask = options.masked ? cui_input_mask_resource(ui, id) : NULL;
    cui_str actual = text;
    bool mask_ready = !options.masked;
    if (options.masked) {
        if (mask && mask->valid) {
            if (!state->behavior.composition || !state->behavior.composition->active)
                mask->visible = mask->committed;
            cui_input_mask_slot *slot = &mask->slots[mask->visible];
            text = (cui_str){slot->bytes, slot->length};
            size_t expected;
            mask_ready = cui_edit_mask_offset(actual, actual.length, &expected) == CUI_EDIT_OK &&
                         expected == slot->length;
            if (!mask_ready ||
                cui_edit_mask_offset(actual, selection.anchor, &selection.anchor) != CUI_EDIT_OK ||
                cui_edit_mask_offset(actual, selection.cursor, &selection.cursor) != CUI_EDIT_OK ||
                selection.anchor > text.length || selection.cursor > text.length) {
                text = (cui_str){0};
                selection.anchor = selection.cursor = 0;
                mask_ready = false;
            }
        } else {
            text = (cui_str){0};
            selection.anchor = selection.cursor = 0;
        }
    }
    if (multiline) {
        cui_text_with_line_height(ui, text.length ? text : cui_string(" "), metrics.font_size,
                                  foreground, metrics.line_height);
        cui_end_box(ui);
        return;
    }
    float inset = metrics.padding_x + border;
    cui_id viewport_id = cui_input_viewport_id(id);
    float viewport_width = width > 0 ? fmaxf(0, width - 2 * inset) : cui_bounds(ui, viewport_id).w;
    float text_width = 0, caret = 0;
    cui_input_visual_cache *cache =
        options.masked
            ? (mask && mask->valid && mask_ready ? &mask->slots[mask->visible].visual : NULL)
            : cui_input_visual_resource(ui, id);
    cui_input_visual_bind(cache, edit);
    cui_text_selection_rect *visual_selection = NULL, *visual_composition = NULL;
    size_t visual_count = 0, composition_count = 0;
    size_t first = selection.anchor < selection.cursor ? selection.anchor : selection.cursor;
    size_t last = selection.anchor > selection.cursor ? selection.anchor : selection.cursor;
    bool visual = cui_input_visual_acquire(cache, ui, text, metrics.font_size);
    if (visual)
        visual =
            cui_input_visual_caret(&cache->geometry, selection.cursor,
                                   cache->affinity_valid && cache->caret_byte == selection.cursor
                                       ? cache->affinity
                                       : CUI_TEXT_DOWNSTREAM,
                                   &caret) &&
            cui_input_visual_selection(&cache->geometry, first, last, &visual_selection,
                                       &visual_count);
    if (visual && state->behavior.composition && state->behavior.composition->active) {
        cui_edit_composition *composition = state->behavior.composition;
        size_t marked_start = composition->start,
               marked_end = composition->start + composition->length;
        if (options.masked) {
            (void)cui_edit_mask_offset(actual, marked_start, &marked_start);
            (void)cui_edit_mask_offset(actual, marked_end, &marked_end);
        }
        visual = cui_input_visual_selection(&cache->geometry, marked_start, marked_end,
                                            &visual_composition, &composition_count);
    }
    if (visual)
        text_width = cache->geometry.width;
    else {
        free(visual_selection);
        visual_selection = NULL;
        visual_count = 0;
        free(visual_composition);
        visual_composition = NULL;
        composition_count = 0;
        text_width = cui_measure_text(ui, text, metrics.font_size).width;
        caret =
            cui_measure_text(ui, (cui_str){text.data, selection.cursor}, metrics.font_size).width;
    }

    float total = text_width + (focused ? 1 : 0);
    if (!isfinite(state->scroll_x) || state->scroll_x < 0)
        state->scroll_x = 0;
    if (focused && viewport_width > 0) {
        if (caret < state->scroll_x)
            state->scroll_x = caret;
        if (caret + 1 > state->scroll_x + viewport_width)
            state->scroll_x = caret + 1 - viewport_width;
    }
    state->scroll_x = fminf(state->scroll_x, fmaxf(0, total - viewport_width));
    /* A clipped Clay row does not compress growing children. A percentage row
     * takes the resolved frame width independently of its long text, then its
     * unclipped flex layout constrains the inner viewport between the insets.
     * Keep the outer clip and fixed-width composition unchanged. */
    if (width < 0)
        cui_begin_box(ui, 0,
                      (cui_box){.width = 1,
                                .width_percent = true,
                                .row = true,
                                .height = metrics.line_height,
                                .fixed_height = true});
    cui_begin_box(ui, 0, (cui_box){.width = inset});
    cui_end_box(ui);
    cui_begin_box(ui, viewport_id,
                  (cui_box){.width = width > 0 ? viewport_width : -1,
                            .fixed_width = width > 0,
                            .height = metrics.line_height,
                            .fixed_height = true,
                            .clip = true,
                            .align_x = options.text_align,
                            .offset_x = -state->scroll_x});
    /* Paint-only text wrapper: neighboring owner IDs must not alias another
     * field's tracked viewport. No routing or geometry refers to this box. */
    cui_begin_box(ui, 0,
                  (cui_box){.width = fmaxf(text_width, 1),
                            .fixed_width = true,
                            .height = metrics.line_height,
                            .fixed_height = true});
    if (!visual && focused && mask_ready && state->behavior.composition &&
        state->behavior.composition->active) {
        cui_edit_composition *c = state->behavior.composition;
        size_t marked_start = c->start, marked_end = c->start + c->length;
        if (options.masked) {
            (void)cui_edit_mask_offset(actual, marked_start, &marked_start);
            (void)cui_edit_mask_offset(actual, marked_end, &marked_end);
        }
        float left =
            cui_measure_text(ui, (cui_str){text.data, marked_start}, metrics.font_size).width;
        float right =
            cui_measure_text(ui, (cui_str){text.data, marked_end}, metrics.font_size).width;
        cui_set_underlay(ui, (cui_rect){left, metrics.line_height - 1, right - left, 1}, t->caret,
                         (cui_corner_radii){0});
    }
    if (!visual && focused && first != last) {
        float left = cui_measure_text(ui, (cui_str){text.data, first}, metrics.font_size).width;
        float right = cui_measure_text(ui, (cui_str){text.data, last}, metrics.font_size).width;
        cui_set_underlay(ui, (cui_rect){left, 0, right - left, metrics.line_height}, t->selection,
                         (cui_corner_radii){0});
    }
    if (visual && focused) {
        for (size_t i = 0; i < visual_count; ++i) {
            cui_rect bounds = visual_selection[i].bounds;
            bounds.y = 0;
            bounds.h = metrics.line_height;
            cui_begin_absolute_box(ui, 0, (cui_box){.background = t->selection}, bounds);
            cui_end_box(ui);
        }
        for (size_t i = 0; i < composition_count; ++i) {
            cui_rect bounds = visual_composition[i].bounds;
            bounds.y = metrics.line_height - 1;
            bounds.h = 1;
            cui_begin_absolute_box(ui, 0, (cui_box){.background = t->caret}, bounds);
            cui_end_box(ui);
        }
    }
    free(visual_selection);
    free(visual_composition);
    if (text.length) {
        cui_text_line(ui, text, metrics.font_size, foreground, metrics.line_height,
                      (cui_text_style){0});
    } else if (options.placeholder.length) {
        cui_color placeholder = t->muted;
        if (options.disabled)
            placeholder.a = (uint8_t)lroundf(placeholder.a * .5f);
        /* Align the placeholder by its own width while the empty text box
         * continues to anchor the caret and hit testing. */
        float placeholder_width =
            cui_measure_text(ui, options.placeholder, metrics.font_size).width;
        float placeholder_x =
            cui_input_alignment(viewport_width, placeholder_width, options.text_align) -
            cui_input_alignment(viewport_width, fmaxf(text_width, 1), options.text_align);
        cui_begin_absolute_box(
            ui, 0, (cui_box){0},
            (cui_rect){placeholder_x, 0, placeholder_width, metrics.line_height});
        cui_text_line(ui, options.placeholder, metrics.font_size, placeholder, metrics.line_height,
                      (cui_text_style){0});
        cui_end_box(ui);
    }
    if (focused) {
        cui_rect viewport = cui_bounds(ui, viewport_id);
        cui_set_text_input_rect(
            ui, id,
            (cui_rect){viewport.x + caret - state->scroll_x, viewport.y, 1, metrics.line_height});
        float height = metrics.line_height * .85f;
        cui_begin_absolute_box(ui, id ^ UINT64_C(0x847d1eb65930ac28),
                               (cui_box){.background = t->caret},
                               (cui_rect){caret, (metrics.line_height - height) * .5f, 1, height});
        cui_end_box(ui);
        (void)cui_set_text_input_geometry_rect(ui, id, id ^ UINT64_C(0x847d1eb65930ac28),
                                               metrics.line_height);
    }
    cui_end_box(ui);
    cui_end_box(ui);
    cui_begin_box(ui, 0, (cui_box){.width = inset});
    cui_end_box(ui);
    if (width < 0)
        cui_end_box(ui);
    cui_end_box(ui);
}
static cui_input_result
cui_input_common(cui_context *ui, cui_id id, const cui_theme *t, cui_str name, cui_edit *edit,
                 cui_input_state *state, cui_input_options options,
                 cui_component_text_handler handler, cui_input_event_result *events, bool paint,
                 const cui_base_input_options *overrides, cui_input_behavior_state *behavior_owner,
                 cui_rect viewport, bool synthetic_hit, cui_input_options *final_options) {
    cui_input_behavior_state *behavior = behavior_owner ? behavior_owner
                                         : state        ? &state->behavior
                                                        : NULL;
    if (final_options)
        *final_options = options;
    cui_input_metrics metrics;
    if (!ui || !t || !edit || !state || !cui_input_measure(options, t->font_size, &metrics) ||
        !isfinite(options.width) || options.text_align < CUI_ALIGN_START ||
        options.text_align > CUI_ALIGN_END ||
        (options.placeholder.length && !options.placeholder.data))
        return (cui_input_result){.error = CUI_EDIT_INVALID_ARGUMENT};
    if (options.content_type < CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED ||
        options.content_type > CUI_TEXT_INPUT_PURPOSE_NEW_PASSWORD ||
        ((options.masked || options.content_type != CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED) &&
         (cui_edit_get_state(edit).multiline || cui_edit_get_content(edit).token_count)))
        return (cui_input_result){.error = CUI_EDIT_INVALID_ARGUMENT};
    if (overrides && options.masked &&
        (overrides->presentation.prepare || overrides->presentation_complete)) {
        (void)cui_interact(ui, id, CUI_ROLE_TEXTBOX, name, true, false, 0);
        cui_set_semantic_details(
            ui, id,
            (cui_semantic_details){.secret = true,
                                   .password =
                                       options.content_type != CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED,
                                   .invalid = true,
                                   .readonly = true});
        if (final_options)
            final_options->invalid = true;
        return (cui_input_result){.error = CUI_EDIT_INVALID_ARGUMENT};
    }
    cui_edit_checked_result configured =
        cui_edit_apply_checked(edit,
                               (cui_edit_request){.kind = CUI_EDIT_REQUEST_PRESENT,
                                                  .format = options.format,
                                                  .format_workspace = options.format_workspace},
                               (cui_edit_acceptance){0});
    if (configured.error != CUI_EDIT_OK)
        return (cui_input_result){.error = configured.error};
    bool pattern_placeholder = options.format.pattern.length && !options.placeholder.length;
    if (pattern_placeholder) {
        cui_str placeholder;
        cui_edit_result error =
            cui_edit_format_placeholder(options.format, options.format_workspace.data,
                                        options.format_workspace.bytes, &placeholder);
        if (error != CUI_EDIT_OK)
            return (cui_input_result){.error = error};
    }
    cui_input_mask_cache *mask = NULL;
    if (options.masked) {
        cui_input_visual_cache *plain = cui_input_visual_resource(ui, id);
        if (plain)
            cui_input_visual_clear(plain);
        mask = cui_input_mask_resource(ui, id);
        behavior->composition =
            cui_base_composition(ui, id, behavior->composition, &behavior->composition_owned);
        cui_edit_result prepared =
            cui_input_mask_bind(mask, ui, edit, behavior->composition, options, metrics.font_size,
                                !behavior->focus_initialized);
        if (prepared != CUI_EDIT_OK) {
            (void)cui_interact(ui, id, CUI_ROLE_TEXTBOX, name, true, false, 0);
            cui_set_semantic_details(
                ui, id,
                (cui_semantic_details){.secret = true,
                                       .password = options.content_type !=
                                                   CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED,
                                       .invalid = true,
                                       .readonly = true});
            if (paint)
                cui_input_paint(ui, id, t, name, edit, state, options);
            return (cui_input_result){.error = prepared};
        }
    }
    cui_id viewport_id = cui_input_viewport_id(id);
    cui_input_hit_geometry hit = {ui, overrides ? viewport : cui_bounds(ui, viewport_id),
                                  metrics.font_size, state->scroll_x};
    bool multiline = cui_edit_get_state(edit).multiline;
    cui_input_cached_hit cached_hit = {
        hit, (multiline || options.masked) ? NULL : cui_input_visual_resource(ui, id)};
    cui_input_visual_bind(cached_hit.cache, edit);
    cui_str committed = cui_edit_get_text(edit);
    if (options.masked) {
        cui_input_mask_slot *slot = &mask->slots[mask->committed];
        committed = (cui_str){slot->bytes, slot->length};
        cached_hit.cache = slot->visual.valid ? &slot->visual : NULL;
    }
    float text_width = cui_input_visual_acquire(cached_hit.cache, ui, committed, metrics.font_size)
                           ? cached_hit.cache->geometry.width
                           : cui_measure_text(ui, committed, metrics.font_size).width;
    cached_hit.measured.viewport.x +=
        cui_input_alignment(hit.viewport.w, text_width, options.text_align);
    behavior->composition =
        cui_base_composition(ui, id, behavior->composition, &behavior->composition_owned);
    if (behavior->composition && behavior->composition_owned)
        behavior->composition->grow_view = true;
    cui_input_mask_hit masked_hit = {cached_hit, mask};
    cui_base_input_options base_options = {
        .disabled = options.disabled,
        .readonly = options.readonly,
        .required = options.required,
        .invalid = options.invalid,
        .clipboard = options.clipboard,
        .validate = options.validate,
        .validate_user = options.validate_user,
        .normalization = options.normalization,
        .format = options.format,
        .format_workspace = options.format_workspace,
        .matcher = options.matcher,
        .masked = options.masked,
        .content_type = options.content_type,
        .presentation = mask ? cui_input_mask_presentation(mask) : (cui_edit_presentation){0},
        .presentation_complete = mask ? cui_input_mask_complete : NULL,
        .presentation_user = mask,
        .delegate_up_down = options.delegate_up_down,
        .hit_test = multiline        ? NULL
                    : options.masked ? cui_input_mask_hit_test
                                     : cui_input_cached_hit_test,
        .hit_test_user = options.masked ? (void *)&masked_hit : (void *)&cached_hit};
    if (overrides) {
        cui_base_input_options supplied = *overrides;
        if (!supplied.hit_test || synthetic_hit) {
            supplied.hit_test = base_options.hit_test;
            supplied.hit_test_user = base_options.hit_test_user;
        }
        if (mask) {
            supplied.presentation = base_options.presentation;
            supplied.presentation_complete = base_options.presentation_complete;
            supplied.presentation_user = base_options.presentation_user;
        }
        base_options = supplied;
    }
    cui_input_result result;
    if (events) {
        *events = cui_base_input_events(ui, id, name, edit, behavior, base_options, handler);
        result = events->control;
    } else {
        result = cui_base_input_ex(ui, id, name, edit, behavior, base_options);
    }
    if (mask) {
        mask->revision = cui_edit_get_state(edit).revision;
        if (!behavior->composition || !behavior->composition->active)
            mask->visible = mask->committed;
    }
    if (pattern_placeholder) {
        cui_str placeholder;
        (void)cui_edit_format_placeholder(options.format, options.format_workspace.data,
                                          options.format_workspace.bytes, &placeholder);
        options.placeholder = placeholder;
    }
    options.invalid = options.invalid || result.error != CUI_EDIT_OK;
    if (final_options)
        *final_options = options;
    if (paint)
        cui_input_paint(ui, id, t, name, edit, state, options);
    return result;
}

cui_input_result cui_input_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                              cui_edit *edit, cui_input_state *state, cui_input_options options) {
    return cui_input_common(ui, id, t, name, edit, state, options, (cui_component_text_handler){0},
                            NULL, true, NULL, NULL, (cui_rect){0}, false, NULL);
}

cui_input_event_result cui_input_events(cui_context *ui, cui_id id, const cui_theme *t,
                                        cui_str name, cui_edit *edit, cui_input_state *state,
                                        cui_input_options options,
                                        cui_component_text_handler handler) {
    cui_input_event_result result = {0};
    result.control = cui_input_common(ui, id, t, name, edit, state, options, handler, &result, true,
                                      NULL, NULL, (cui_rect){0}, false, NULL);
    return result;
}

cui_input_event_result
cui_input_private_reduce(cui_context *ui, cui_id id, const cui_theme *theme, cui_str name,
                         cui_edit *edit, cui_input_state *state, cui_input_behavior_state *behavior,
                         cui_input_options *options, cui_base_input_options overrides,
                         cui_rect viewport, bool synthetic_hit,
                         cui_component_text_handler handler) {
    cui_input_event_result result = {0};
    result.control = cui_input_common(ui, id, theme, name, edit, state, *options, handler,
                                      handler.callback ? &result : NULL, false, &overrides,
                                      behavior, viewport, synthetic_hit, options);
    return result;
}

cui_input_result cui_input(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                           cui_edit *edit, bool disabled) {
    if (!ui || !id || !t || !edit)
        return (cui_input_result){.error = CUI_EDIT_INVALID_ARGUMENT};
    cui_input_state *state =
        cui_resource_get(ui, id, UINT64_C(0x5a2a3f8d00e3af41), sizeof(*state), NULL, NULL);
    if (!state)
        return (cui_input_result){.error = CUI_EDIT_CAPACITY};
    return cui_input_ex(ui, id, t, name, edit, state, (cui_input_options){.disabled = disabled});
}

cui_progress_circle_options cui_progress_circle_defaults(void) {
    return (cui_progress_circle_options){.progress = cui_progress_defaults(), .raster_scale = 2};
}
bool cui_progress_circle(cui_context *ui, cui_id id, const cui_theme *theme, float fraction,
                         cui_progress_circle_state *state, cui_progress_circle_options options,
                         cui_button_content_fn content, void *user) {
    cui_progress_options p = options.progress;
    if (!ui || !id || !theme || !cui_design_valid(p.design_size) || !state ||
        p.size < CUI_CONTROL_MEDIUM || p.size > CUI_CONTROL_XSMALL ||
        (p.name.length && !p.name.data))
        return false;
    cui_design_metrics dm = {0};
    if (p.design_size && !cui_design_resolve(p.design_size, theme->font_size / 16, &dm))
        return false;
    bool reduced = p.reduced_motion || cui_reduced_motion(ui);
    cui_progress_geometry progress =
        cui_progress_measure(fraction, p.indeterminate, true, p.now_seconds, reduced);
    bool moving = progress.animating;
    cui_progress_motion staged = {0};
    if (!p.indeterminate && p.motion) {
        staged = *p.motion;
        cui_progress_motion_result transition;
        if (!cui_progress_animate(&staged, progress.fraction, p.now_seconds,
                                  p.duration == 0 ? .18f : p.duration, reduced, &transition))
            return false;
        progress.length = transition.value;
        moving = transition.active;
    }
    float diameter = options.diameter == 0 ? (p.size == CUI_CONTROL_XSMALL  ? 8
                                              : p.size == CUI_CONTROL_SMALL ? 12
                                              : p.size == CUI_CONTROL_LARGE ? 20
                                                                            : 16)
                                           : options.diameter;
    if (p.design_size && options.diameter == 0)
        diameter = dm.control_height;
    cui_progress_circle_geometry geometry;
    cui_bitmap bitmap;
    if (!cui_progress_circle_measure(progress, diameter, &geometry) ||
        !cui_progress_circle_raster(geometry, p.custom_color ? p.color : theme->primary,
                                    options.raster_scale == 0 ? 2 : options.raster_scale,
                                    state->pixels, state->capacity, &bitmap))
        return false;
    if (!p.indeterminate && p.motion)
        *p.motion = staged;
    if (++state->revision == 0)
        ++state->revision;
    bitmap.revision = state->revision;
    cui_begin_box(ui, id,
                  (cui_box){.width = diameter,
                            .height = diameter,
                            .fixed_height = true,
                            .align_x = CUI_ALIGN_CENTER,
                            .align_y = CUI_ALIGN_CENTER});
    cui_base_progress(ui, id, p.name, fraction, p.indeterminate);
    cui_set_bitmap_underlay(ui, (cui_underlay_layout){.grow_width = true, .grow_height = true},
                            (cui_bitmap_paint){.bitmap = bitmap, .opacity = 1},
                            (cui_corner_radii){0});
    if (content)
        content(ui, user);
    cui_end_box(ui);
    if (moving)
        cui_invalidate(ui);
    return moving;
}

/* Additive event facades. Legacy entry points above retain their exact reducers. */
typedef struct {
    cui_button_options original;
    cui_component_activation_handler handler;
    cui_component_action_result *result;
} cui_control_events_button_forward;
static void cui_control_events_button_click(void *user) {
    cui_control_events_button_forward *forward = user;
    forward->original.on_click(forward->original.action_user);
}
static void cui_control_events_button_hover(void *user, bool hovered) {
    cui_control_events_button_forward *forward = user;
    forward->original.on_hover(forward->original.action_user, hovered);
}
static void cui_control_events_button_typed(void *user, const cui_button_click_event *click) {
    cui_control_events_button_forward *forward = user;
    cui_component_event_metadata event;
    bool evidence =
        click->has_evidence &&
        cui_component_event_from_activation(&event, CUI_COMPONENT_EVENT_ACTIVATE, &click->evidence);
    cui_control_events_private_increment(&forward->result->activation_count,
                                         &forward->result->count_overflow);
    if (forward->original.on_click_event)
        forward->original.on_click_event(forward->original.action_user, click);
    if (evidence && forward->handler.callback)
        forward->handler.callback(forward->handler.user, &event);
}
cui_component_action_result cui_button_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                              cui_str name, cui_button_options options,
                                              cui_component_activation_handler handler) {
    cui_component_action_result result = {0};
    cui_design_metrics design;
    if (!ui || !id || !theme || (name.length && !name.data) ||
        !cui_components_design(theme, options.design_size, &design) ||
        !cui_button_shape_valid(options))
        return result;
    result.valid = true;
    result.submitted = cui_frame_is_submitting(ui) && cui_frame_sequence(ui) != 0;
    cui_control_events_button_forward forward = {options, handler, &result};
    options.on_click = options.on_click ? cui_control_events_button_click : NULL;
    options.on_hover = options.on_hover ? cui_control_events_button_hover : NULL;
    options.on_click_event = cui_control_events_button_typed;
    options.action_user = &forward;
    (void)cui_button_ex(ui, id, theme, name, options);
    return result;
}
cui_component_action_result cui_checkbox_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                                cui_str name, cui_check_state value,
                                                cui_checkbox_options options,
                                                cui_component_check_handler handler) {
    cui_component_action_result result = {0};
    cui_design_metrics design;
    if (!ui || !id || !theme || (name.length && !name.data) || value < CUI_CHECK_UNCHECKED ||
        value > CUI_CHECK_MIXED || options.on_change || options.change_user ||
        (options.custom_role &&
         (options.role < CUI_ROLE_NONE || options.role > CUI_ROLE_RADIOGROUP)) ||
        !cui_components_design(theme, options.design_size, &design))
        return result;
    result.valid = true;
    result.submitted = cui_frame_is_submitting(ui) && cui_frame_sequence(ui) != 0;
    if (!cui_frame_is_submitting(ui))
        return result;
    cui_control_events_private_check forward = {value, handler, &result};
    options.on_change = cui_control_events_private_check_request;
    options.change_user = &forward;
    (void)cui_checkbox_source(ui, id, theme, name, &value, options);
    return result;
}

cui_component_action_result cui_switch_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                              cui_str label, bool value, cui_switch_state *state,
                                              cui_switch_options options,
                                              cui_component_boolean_handler handler) {
    cui_component_action_result result = {0};
    cui_design_metrics design;
    if (options.on_change || options.change_user ||
        !cui_components_switch_valid(ui, id, theme, label, options, &design))
        return result;
    result.valid = true;
    if (!cui_frame_is_submitting(ui))
        return result;
    cui_str name = options.accessibility_label.length ? options.accessibility_label : label;
    result = cui_base_switch_events(
        ui, id, name, value,
        (cui_boolean_event_options){.disabled = options.disabled, .skip_tab = options.skip_tab},
        handler);
    cui_update_semantic(ui, id, value, value ? 1 : 0);
    cui_switch_state passive;
    if (!result.submitted && state) {
        passive = *state;
        state = &passive;
    }
    cui_components_switch_paint(ui, id, theme, label, &value, state, options, design,
                                result.submitted);
    return result;
}
cui_component_action_result cui_toggle_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                              cui_str label, bool value,
                                              cui_hover_state *tooltip_state,
                                              cui_toggle_options options,
                                              cui_component_boolean_handler handler) {
    cui_component_action_result result = {0};
    cui_design_metrics design;
    if (options.on_change || options.change_user || !ui || !id || !theme ||
        !cui_components_design(theme, options.design_size, &design) ||
        !cui_components_toggle_valid(label, options) || !isfinite(theme->font_size) ||
        theme->font_size <= 0 || !isfinite(theme->radius) || theme->radius < 0)
        return result;
    result.valid = true;
    if (!cui_frame_is_submitting(ui))
        return result;
    cui_str name = options.accessibility_label.length ? options.accessibility_label
                   : options.tooltip.length           ? options.tooltip
                                                      : label;
    result = cui_base_toggle_events(
        ui, id, name, value,
        (cui_boolean_event_options){.disabled = options.disabled, .skip_tab = options.skip_tab},
        handler);
    cui_update_semantic(ui, id, value, value ? 1 : 0);
    cui_toggle_paint(ui, id, theme, label, value, options);
    if (result.submitted && tooltip_state && options.tooltip.length) {
        cui_hover_options tooltip = options.tooltip_options;
        tooltip.trigger = id;
        tooltip.disabled |= options.disabled;
        cui_tooltip(ui, cui_components_child_id(id, 5100), theme, tooltip_state, options.tooltip,
                    tooltip);
    }
    return result;
}
