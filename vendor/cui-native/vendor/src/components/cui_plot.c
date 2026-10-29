/* SPDX-License-Identifier: MIT */
#include "cui/cui_plot_view.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

cui_plot_result cui_plot_viewport_reduce(cui_context *ui, uint64_t id, cui_plot_viewport *v,
                                         cui_plot_viewport_options o) {
    if (!ui || !id || !v || !v->initialized)
        return CUI_PLOT_INVALID;
    if (!o.enabled || o.disabled) {
        v->dragging = false;
        return CUI_PLOT_OK;
    }
    /* A transient nonmodal scope activates Core's original-hit query. The
     * surface ID itself is sufficient; no extra retained semantic is created. */
    if (!cui_begin_focus_scope(ui, id, false))
        return CUI_PLOT_INVALID;
    if (!cui_pointer_scope_allowed(ui, id))
        v->dragging = false;
    cui_rect bounds = cui_bounds(ui, id);
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    cui_plot_result result = CUI_PLOT_OK;
    for (size_t i = 0; i < count; ++i) {
        const cui_event *e = &events[i];
        if (cui_event_consumed(ui, i))
            continue;
        bool target = cui_event_target(ui, i) == id;
        double dx = 0, dy = 0, factor = 1;
        bool transform = false, claimed = false;
        if (e->type == CUI_EVENT_CANCEL && (target || v->dragging)) {
            v->dragging = false;
            claimed = true;
        } else if (target && e->type == CUI_EVENT_POINTER_DOWN &&
                   e->pointer_type == CUI_POINTER_MOUSE &&
                   e->pointer_button == CUI_POINTER_PRIMARY && (e->modifiers & CUI_MOD_SHIFT) &&
                   (o.pan_x || o.pan_y)) {
            v->dragging = true;
            v->pointer_x = e->x;
            v->pointer_y = e->y;
            claimed = true;
        } else if (v->dragging && target && e->type == CUI_EVENT_POINTER_MOVE) {
            dx = (double)e->x - v->pointer_x;
            dy = (double)e->y - v->pointer_y;
            v->pointer_x = e->x;
            v->pointer_y = e->y;
            transform = claimed = true;
        } else if (v->dragging && e->type == CUI_EVENT_POINTER_UP) {
            v->dragging = false;
            (void)cui_cancel_event_activation(ui, i);
            claimed = true;
        } else if (target && e->type == CUI_EVENT_GESTURE) {
            v->dragging = false;
            if (e->scroll_phase != CUI_SCROLL_PHASE_ENDED &&
                e->scroll_phase != CUI_SCROLL_PHASE_CANCELLED) {
                dx = e->dx;
                dy = e->dy;
                factor = e->gesture_scale;
                transform = true;
            }
            claimed = true;
        } else if (e->type == CUI_EVENT_SCROLL && (e->modifiers & CUI_MOD_CTRL) &&
                   (o.zoom_x || o.zoom_y) && cui_event_hit_within_scope(ui, i, id)) {
            double delta = e->dy * (e->scroll_precise ? 1.0 : 32.0);
            factor = exp(fmax(-20, fmin(20, delta * .002)));
            transform = claimed = true;
        }
        if (transform) {
            cui_plot_result next = cui_plot_viewport_transform(
                v, o, dx, dy, factor, (cui_plot_point){e->x - bounds.x, e->y - bounds.y});
            if (next != CUI_PLOT_OK)
                result = next;
            else
                cui_invalidate(ui);
        }
        if (claimed)
            (void)cui_consume_event(ui, i);
    }
    cui_end_focus_scope(ui);
    return result;
}
bool cui_plot_viewport_bind(cui_context *ui, uint64_t id, const cui_plot_viewport *v,
                            cui_plot_viewport_options o) {
    if (!ui || !id || !v || !v->initialized)
        return false;
    size_t count;
    const cui_semantic *items = cui_semantics(ui, &count);
    for (size_t i = 0; i < count; ++i)
        if (items[i].id == id) {
            cui_semantic_details details = items[i].details;
            bool enabled = o.enabled && !o.disabled && !items[i].disabled;
            if (enabled)
                details.pointer_activatable = true;
            details.gesture_pan = enabled && (o.pan_x || o.pan_y);
            details.gesture_zoom = enabled && (o.zoom_x || o.zoom_y);
            cui_set_semantic_details(ui, id, details);
            if (enabled)
                return cui_set_cursor(ui, id, v->dragging ? CUI_CURSOR_GRABBING : CUI_CURSOR_GRAB);
            return true;
        }
    return false;
}

cui_plot_axis_options cui_plot_axis_defaults(float width, float height) {
    return (cui_plot_axis_options){.width = width, .height = height, .x_line = true};
}
cui_plot_axis_options cui_plot_axis_source_defaults(float width, float height) {
    cui_plot_axis_options options = cui_plot_axis_defaults(width, height);
    options.source_offsets = true;
    options.explicit_colors = true;
    return options;
}
float cui_plot_axis_gutter(float size) {
    return isfinite(size) && size > 0 ? size + 8 : 0;
}
static bool axis_valid(const cui_plot_axis_options *o) {
    if (!o || !isfinite(o->width) || !isfinite(o->height) || o->width <= 0 || o->height <= 0 ||
        o->width > 16384 || o->height > 16384 || !isfinite(o->x) || !isfinite(o->y) ||
        o->x_count > CUI_PLOT_AXIS_LIMIT || o->y_count > CUI_PLOT_AXIS_LIMIT ||
        (o->x_count && !o->x_labels) || (o->y_count && !o->y_labels) ||
        o->x_side > CUI_PLOT_AXIS_START || o->y_side > CUI_PLOT_AXIS_START ||
        o->x_side < CUI_PLOT_AXIS_END || o->y_side < CUI_PLOT_AXIS_END)
        return false;
    for (size_t k = 0; k < 2; ++k) {
        const cui_plot_axis_text *labels = k ? o->y_labels : o->x_labels;
        size_t count = k ? o->y_count : o->x_count;
        for (size_t i = 0; i < count; ++i)
            if ((!labels[i].text.data && labels[i].text.length) || !isfinite(labels[i].tick) ||
                fabsf(labels[i].tick) > 16384 || !isfinite(labels[i].font_size) ||
                labels[i].font_size < 0 || labels[i].font_size > 1024 ||
                labels[i].align < CUI_ALIGN_START || labels[i].align > CUI_ALIGN_END)
                return false;
    }
    return fabsf(o->x) <= 16384 && fabsf(o->y) <= 16384;
}
cui_plot_result cui_plot_axis_layout(cui_context *ui, const cui_plot_axis_options *o,
                                     cui_plot_axis_label_geometry *out, size_t capacity,
                                     size_t *required) {
    if (!ui || !required || (!out && capacity) || !axis_valid(o))
        return CUI_PLOT_INVALID;
    cui_plot_axis_label_geometry geometry[CUI_PLOT_AXIS_LIMIT * 2];
    size_t used = 0;
    for (size_t k = 0; k < 2; ++k) {
        if (!(k ? o->has_y : o->has_x))
            continue;
        const cui_plot_axis_text *labels = k ? o->y_labels : o->x_labels;
        size_t count = k ? o->y_count : o->x_count;
        for (size_t i = 0; i < count; ++i) {
            float size = labels[i].font_size ? labels[i].font_size : 10;
            cui_size measured = cui_measure_text(ui, labels[i].text, size);
            if (!isfinite(measured.width) || !isfinite(measured.height) || measured.width < 0 ||
                measured.height < 0 || measured.width > 16384 || measured.height > 16384)
                return CUI_PLOT_INVALID;
            float x = k ? o->y + (o->y_side == CUI_PLOT_AXIS_START ? -2 : 2) : labels[i].tick;
            float y = k ? labels[i].tick - (o->source_offsets ? 5 : size / 2)
                        : o->x + (o->x_side == CUI_PLOT_AXIS_START
                                      ? (o->source_offsets ? -14 : -(size + 4))
                                      : 6);
            if (labels[i].align == CUI_ALIGN_CENTER)
                x -= measured.width / 2;
            if (labels[i].align == CUI_ALIGN_END)
                x -= measured.width;
            geometry[used++] =
                (cui_plot_axis_label_geometry){{x, y, measured.width, measured.height}, i, !k};
        }
    }
    *required = used;
    if (capacity < used)
        return CUI_PLOT_CAPACITY;
    if (used)
        memcpy(out, geometry, used * sizeof *out);
    return CUI_PLOT_OK;
}
static cui_color resolved(cui_color color, cui_color fallback) {
    return color.a ? color : fallback;
}
static void axis_paint(cui_context *ui, cui_id id, const cui_theme *theme,
                       const cui_plot_axis_options *o, const cui_plot_axis_label_geometry *geometry,
                       size_t count) {
    cui_color stroke = o->explicit_colors ? o->stroke : resolved(o->stroke, theme->border);
    if (o->has_x && o->x_line) {
        cui_begin_absolute_box(ui, id ^ UINT64_C(0xa1), (cui_box){.background = stroke},
                               (cui_rect){0, o->x - .5f, o->width, 1});
        cui_end_box(ui);
    }
    if (o->has_y && o->y_line) {
        cui_begin_absolute_box(ui, id ^ UINT64_C(0xa2), (cui_box){.background = stroke},
                               (cui_rect){o->y - .5f, 0, 1, o->height});
        cui_end_box(ui);
    }
    for (size_t i = 0; i < count; ++i) {
        const cui_plot_axis_text *label = geometry[i].horizontal ? o->x_labels + geometry[i].index
                                                                 : o->y_labels + geometry[i].index;
        cui_begin_absolute_box(ui, id ^ UINT64_C(0x789ae632a981c000) ^ (i + 1), (cui_box){0},
                               geometry[i].bounds);
        cui_color color = o->explicit_colors ? label->color : resolved(label->color, theme->muted);
        if (o->source_offsets)
            cui_text_line(ui, label->text, label->font_size ? label->font_size : 10, color,
                          geometry[i].bounds.h, (cui_text_style){0});
        else
            cui_text(ui, label->text, label->font_size ? label->font_size : 10, color);
        cui_end_box(ui);
    }
}
cui_plot_result cui_plot_axis(cui_context *ui, cui_id id, const cui_theme *theme,
                              const cui_plot_axis_options *o) {
    if (!theme || !id)
        return CUI_PLOT_INVALID;
    cui_plot_axis_label_geometry geometry[512];
    size_t count;
    cui_plot_result result = cui_plot_axis_layout(ui, o, geometry, 512, &count);
    if (result != CUI_PLOT_OK)
        return result;
    cui_begin_box(
        ui, id,
        (cui_box){
            .width = o->width, .height = o->height, .fixed_width = true, .fixed_height = true});
    axis_paint(ui, id, theme, o, geometry, count);
    cui_end_box(ui);
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_axis_ticks(const cui_plot_scale *scale, size_t desired,
                                    cui_plot_numeric_tick *out, size_t capacity, size_t *required) {
    if (!required || (!out && capacity) || desired > CUI_PLOT_AXIS_LIMIT)
        return CUI_PLOT_INVALID;
    double ticks[1024];
    size_t count;
    cui_plot_result result = cui_plot_ticks(scale, desired, ticks, 1024, &count);
    if (result != CUI_PLOT_OK)
        return result;
    if (count > CUI_PLOT_AXIS_LIMIT) {
        *required = count;
        return CUI_PLOT_CAPACITY;
    }
    double pixels[256];
    for (size_t i = 0; i < count; ++i) {
        result = cui_plot_map(scale, ticks[i], pixels + i);
        if (result != CUI_PLOT_OK)
            return result;
        if (fabs(pixels[i]) > 16384)
            return CUI_PLOT_INVALID;
    }
    *required = count;
    if (capacity < count)
        return CUI_PLOT_CAPACITY;
    for (size_t i = 0; i < count; ++i) {
        snprintf(out[i].text, sizeof out[i].text, "%.17g", ticks[i]);
        out[i].value = ticks[i];
        out[i].label = (cui_plot_axis_text){
            .text = cui_string(out[i].text), .tick = (float)pixels[i], .align = CUI_ALIGN_CENTER};
    }
    return CUI_PLOT_OK;
}

typedef cui_plot_path_segment plot_segment;
static bool point_valid(cui_plot_point p) {
    return isfinite(p.x) && isfinite(p.y) && fabs(p.x) <= 16384 && fabs(p.y) <= 16384;
}
static cui_plot_result segments(const cui_plot_shape *shape, plot_segment *lines, size_t *count) {
    if (!shape || shape->count > 4096 || (shape->count && !shape->commands) ||
        !isfinite(shape->stroke_width) || shape->stroke_width < 0 || shape->stroke_width > 1024)
        return CUI_PLOT_INVALID;
    size_t used = 0, begin = 0;
    cui_plot_point previous = {0}, start = {0};
    bool active = false;
    for (size_t i = 0; i < shape->count; ++i) {
        const cui_plot_path_command *c = shape->commands + i;
        if (c->kind < CUI_PLOT_PATH_MOVE || c->kind > CUI_PLOT_PATH_CLOSE)
            return CUI_PLOT_INVALID;
        if (c->kind != CUI_PLOT_PATH_CLOSE && !point_valid(c->point))
            return CUI_PLOT_INVALID;
        if (c->kind == CUI_PLOT_PATH_MOVE) {
            previous = start = c->point;
            active = true;
            begin = used;
            continue;
        }
        if (!active)
            return CUI_PLOT_INVALID;
        if (c->kind == CUI_PLOT_PATH_CLOSE) {
            if (used == 4096)
                return CUI_PLOT_CAPACITY;
            lines[used++] = (plot_segment){{previous, start}, true};
            for (size_t j = begin; j < used; ++j)
                lines[j].closed = true;
            previous = start;
            active = false;
            continue;
        }
        if (c->kind == CUI_PLOT_PATH_CUBIC) {
            if (!point_valid(c->control1) || !point_valid(c->control2))
                return CUI_PLOT_INVALID;
            cui_plot_point from = previous;
            for (size_t j = 1; j <= 16; ++j) {
                if (used == 4096)
                    return CUI_PLOT_CAPACITY;
                double t = (double)j / 16, u = 1 - t;
                cui_plot_point next = {u * u * u * from.x + 3 * u * u * t * c->control1.x +
                                           3 * u * t * t * c->control2.x + t * t * t * c->point.x,
                                       u * u * u * from.y + 3 * u * u * t * c->control1.y +
                                           3 * u * t * t * c->control2.y + t * t * t * c->point.y};
                lines[used++] = (plot_segment){{previous, next}, false};
                previous = next;
            }
        } else {
            if (used == 4096)
                return CUI_PLOT_CAPACITY;
            lines[used++] = (plot_segment){{previous, c->point}, false};
            previous = c->point;
        }
    }
    *count = used;
    return CUI_PLOT_OK;
}
static void blend(unsigned char *p, cui_color c) {
    double a = c.a / 255., old = p[3] / 255., alpha = a + old * (1 - a);
    if (alpha == 0) {
        memset(p, 0, 4);
        return;
    }
    p[0] = (unsigned char)lround((c.r * a + p[0] * old * (1 - a)) / alpha);
    p[1] = (unsigned char)lround((c.g * a + p[1] * old * (1 - a)) / alpha);
    p[2] = (unsigned char)lround((c.b * a + p[2] * old * (1 - a)) / alpha);
    p[3] = (unsigned char)lround(alpha * 255);
}
static bool background_frame_valid(cui_plot_rect frame) {
    return point_valid((cui_plot_point){frame.x_min, frame.y_min}) &&
           point_valid((cui_plot_point){frame.x_max, frame.y_max}) && frame.x_min < frame.x_max &&
           frame.y_min < frame.y_max;
}
static bool slash_parameters(const cui_plot_background *bg, double *width, double *period) {
    if (!isfinite(bg->pattern_width) || !isfinite(bg->pattern_interval) || bg->pattern_width < 0 ||
        bg->pattern_interval < 0 || bg->pattern_width > 257 || bg->pattern_interval >= 257)
        return false;
    float w = (float)bg->pattern_width * 255.f;
    float gap = (float)bg->pattern_interval * 255.f;
    if (gap >= 65535.f)
        return false;
    uint64_t packed = (uint64_t)w * 65535 + (uint64_t)gap;
    if (packed > UINT32_MAX)
        return false;
    float encoded = (float)packed;
    float decoded_width = encoded / 65535.f / 255.f;
    float decoded_gap = fmodf(encoded, 65535.f) / 255.f;
    float height = decoded_width + decoded_gap;
    float p = height * sinf(3.1415926f / 4);
    if (!(p > 0) || !isfinite(p))
        return false;
    *width = p * (decoded_width / height) / 2;
    *period = p;
    return true;
}
static bool background_valid(const cui_plot_background *background) {
    if (!background || background->kind < CUI_PLOT_BACKGROUND_SOLID ||
        background->kind > CUI_PLOT_BACKGROUND_CHECKERBOARD ||
        background->color_space < CUI_PLOT_COLOR_SRGB ||
        background->color_space > CUI_PLOT_COLOR_OKLAB)
        return false;
    if (background->kind == CUI_PLOT_BACKGROUND_SLASH) {
        double width, period;
        return slash_parameters(background, &width, &period);
    }
    if (background->kind == CUI_PLOT_BACKGROUND_CHECKERBOARD)
        return isfinite(background->pattern_size) && background->pattern_size >= 1. / 255 &&
               background->pattern_size <= 16384;
    return background->kind == CUI_PLOT_BACKGROUND_SOLID ||
           (isfinite(background->angle) && isfinite(background->from.position) &&
            isfinite(background->to.position) && background->from.position >= 0 &&
            background->from.position <= 1 && background->to.position >= 0 &&
            background->to.position <= 1);
}
cui_plot_background cui_plot_solid_background(cui_color color) {
    return (cui_plot_background){.solid = color};
}
cui_plot_background cui_plot_linear_background(double angle, cui_plot_color_stop from,
                                               cui_plot_color_stop to) {
    return (cui_plot_background){
        .kind = CUI_PLOT_BACKGROUND_LINEAR, .angle = angle, .from = from, .to = to};
}
cui_plot_background cui_plot_linear_background_source(double angle, cui_plot_color_stop from,
                                                      cui_plot_color_stop to,
                                                      cui_plot_color_space space) {
    cui_plot_background bg = cui_plot_linear_background(angle, from, to);
    bg.color_space = space;
    bg.source_projection = true;
    return bg;
}
cui_plot_background cui_plot_pattern_slash(cui_color color, double width, double interval) {
    return (cui_plot_background){.kind = CUI_PLOT_BACKGROUND_SLASH,
                                 .solid = color,
                                 .pattern_width = width,
                                 .pattern_interval = interval};
}
cui_plot_background cui_plot_checkerboard(cui_color color, double size) {
    return (cui_plot_background){.kind = CUI_PLOT_BACKGROUND_CHECKERBOARD,
                                 .solid = color,
                                 .pattern_size = (double)(float)size};
}
/* Standard Oklab transforms (same published matrices used by the pinned shader).
 * CUI colors are encoded sRGB bytes, so decode before entering linear RGB. */
static double plot_srgb_decode(double value) {
    return value <= .04045 ? value / 12.92 : pow((value + .055) / 1.055, 2.4);
}
static unsigned char plot_srgb_encode(double value) {
    value = fmax(0, fmin(1, value));
    double encoded = value <= .0031308 ? value * 12.92 : 1.055 * pow(value, 1. / 2.4) - .055;
    return (unsigned char)lround(fmax(0, fmin(1, encoded)) * 255);
}
static void plot_rgb_lab(double r, double g, double b, double out[3]) {
    double l = cbrt(.4122214708 * r + .5363325363 * g + .0514459929 * b);
    double m = cbrt(.2119034982 * r + .6806995451 * g + .1073969566 * b);
    double s = cbrt(.0883024619 * r + .2817188376 * g + .6299787005 * b);
    out[0] = .2104542553 * l + .7936177850 * m - .0040720468 * s;
    out[1] = 1.9779984951 * l - 2.4285922050 * m + .4505937099 * s;
    out[2] = .0259040371 * l + .7827717662 * m - .8086757660 * s;
}
static void plot_color_lab(cui_color color, double out[3]) {
    plot_rgb_lab(plot_srgb_decode(color.r / 255.), plot_srgb_decode(color.g / 255.),
                 plot_srgb_decode(color.b / 255.), out);
}
static void plot_lab_rgb(const double a[3], const double b[3], double t, double out[3]) {
    double L = a[0] * (1 - t) + b[0] * t, A = a[1] * (1 - t) + b[1] * t;
    double B = a[2] * (1 - t) + b[2] * t;
    double l = L + .3963377774 * A + .2158037573 * B;
    double m = L - .1055613458 * A - .0638541728 * B;
    double s = L - .0894841775 * A - 1.2914855480 * B;
    l *= l * l;
    m *= m * m;
    s *= s * s;
    out[0] = 4.0767416621 * l - 3.3077115913 * m + .2309699292 * s;
    out[1] = -1.2684380046 * l + 2.6097574011 * m - .3413193965 * s;
    out[2] = -.0041960863 * l - .7034186147 * m + 1.7076147010 * s;
}
static cui_color plot_lab_color(const double a[3], const double b[3], double t,
                                unsigned char alpha) {
    double rgb[3];
    plot_lab_rgb(a, b, t, rgb);
    return (cui_color){plot_srgb_encode(rgb[0]), plot_srgb_encode(rgb[1]), plot_srgb_encode(rgb[2]),
                       alpha};
}

typedef struct {
    const cui_plot_background *background;
    double center_x, center_y, x, y, pattern_width, pattern_period;
    double lab_from[3], lab_to[3];
} plot_background_sampler;
static plot_background_sampler background_sampler(const cui_plot_background *background,
                                                  cui_plot_rect frame) {
    plot_background_sampler sample = {.background = background};
    sample.center_x = frame.x_min;
    sample.center_y = frame.y_min;
    if (!background || background->kind == CUI_PLOT_BACKGROUND_SOLID ||
        background->kind == CUI_PLOT_BACKGROUND_CHECKERBOARD)
        return sample;
    if (background->kind == CUI_PLOT_BACKGROUND_SLASH) {
        (void)slash_parameters(background, &sample.pattern_width, &sample.pattern_period);
        return sample;
    }
    if (background->color_space == CUI_PLOT_COLOR_OKLAB) {
        plot_color_lab(background->from.color, sample.lab_from);
        plot_color_lab(background->to.color, sample.lab_to);
    }
    double angle = fmod(background->angle, 360) * (3.14159265358979323846 / 180);
    double dx = sin(angle), dy = -cos(angle);
    double width = frame.x_max - frame.x_min, height = frame.y_max - frame.y_min;
    double length;
    if (background->source_projection) {
        if (width > height)
            dy *= height / width;
        else
            dx *= width / height;
        length = hypot(dx, dy) * (fabs(dx) > fabs(dy) ? width : height);
    } else
        length = fabs(width * dx) + fabs(height * dy);
    sample.x = dx / length;
    sample.y = dy / length;
    sample.center_x = frame.x_min / 2 + frame.x_max / 2;
    sample.center_y = frame.y_min / 2 + frame.y_max / 2;
    return sample;
}
static cui_color background_color(plot_background_sampler sample, cui_plot_point point) {
    const cui_plot_background *background = sample.background;
    if (background->kind == CUI_PLOT_BACKGROUND_SOLID)
        return background->solid;
    if (background->kind == CUI_PLOT_BACKGROUND_SLASH ||
        background->kind == CUI_PLOT_BACKGROUND_CHECKERBOARD) {
        double x = point.x - sample.center_x, y = point.y - sample.center_y, coverage;
        if (background->kind == CUI_PLOT_BACKGROUND_CHECKERBOARD)
            coverage = fmax(0, fmin(1, fmod(floor(x / background->pattern_size) +
                                                floor(y / background->pattern_size),
                                            2)));
        else {
            double rotated = cosf(3.1415926f / 4) * x + sinf(3.1415926f / 4) * y;
            double phase = fmod(rotated, sample.pattern_period);
            double distance = fmin(phase, sample.pattern_period - phase) - sample.pattern_width;
            coverage = fmax(0, fmin(1, .5 - distance));
        }
        cui_color color = background->solid;
        color.a = (unsigned char)lround(color.a * coverage);
        return color;
    }
    double position =
        .5 + (point.x - sample.center_x) * sample.x + (point.y - sample.center_y) * sample.y;
    double interval = background->to.position - background->from.position;
    double t = interval == 0 ? (position >= background->to.position ? 1 : 0)
                             : (position - background->from.position) / interval;
    t = fmax(0, fmin(1, t));
    cui_color a = background->from.color, b = background->to.color;
    if (background->color_space == CUI_PLOT_COLOR_OKLAB) {
        if (t == 0)
            return a;
        if (t == 1)
            return b;
        return plot_lab_color(sample.lab_from, sample.lab_to, t,
                              (unsigned char)lround(a.a * (1 - t) + b.a * t));
    }
    return (cui_color){(unsigned char)lround(a.r * (1 - t) + b.r * t),
                       (unsigned char)lround(a.g * (1 - t) + b.g * t),
                       (unsigned char)lround(a.b * (1 - t) + b.b * t),
                       (unsigned char)lround(a.a * (1 - t) + b.a * t)};
}
cui_plot_result cui_plot_background_sample(const cui_plot_background *background,
                                           cui_plot_rect frame, cui_plot_point position,
                                           cui_color *out) {
    if (!out || !background_valid(background) || !background_frame_valid(frame) ||
        !point_valid(position))
        return CUI_PLOT_INVALID;
    *out = background_color(background_sampler(background, frame), position);
    return CUI_PLOT_OK;
}
static void raster_hits(const plot_segment *lines, size_t used, cui_plot_point origin, double px,
                        double py, double stroke_width, bool *inside, bool *stroked) {
    *inside = false;
    *stroked = false;
    for (size_t j = 0; j < used; ++j) {
        cui_plot_point a = {lines[j].line.start.x + origin.x, lines[j].line.start.y + origin.y};
        cui_plot_point b = {lines[j].line.end.x + origin.x, lines[j].line.end.y + origin.y};
        if (lines[j].closed && ((a.y > py) != (b.y > py)) &&
            px < (b.x - a.x) * (py - a.y) / (b.y - a.y) + a.x)
            *inside = !*inside;
        if (stroke_width > 0) {
            double dx = b.x - a.x, dy = b.y - a.y, length = dx * dx + dy * dy;
            double t = length ? fmax(0, fmin(1, ((px - a.x) * dx + (py - a.y) * dy) / length)) : 0;
            if (hypot(px - a.x - t * dx, py - a.y - t * dy) <= stroke_width / 2)
                *stroked = true;
        }
    }
}
static void raster_segments(unsigned char *pixels, unsigned width, unsigned height, size_t stride,
                            const plot_segment *lines, size_t used, cui_plot_point origin,
                            cui_color fill, cui_color stroke, float stroke_width,
                            const cui_plot_background *fill_background,
                            const cui_plot_background *stroke_background,
                            cui_plot_rect background_frame) {
    plot_background_sampler fill_sample = background_sampler(fill_background, background_frame);
    plot_background_sampler stroke_sample = background_sampler(stroke_background, background_frame);
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
            double px = x + .5, py = y + .5;
            bool inside, stroked;
            raster_hits(lines, used, origin, px, py,
                        (stroke_background || stroke.a) ? stroke_width : 0, &inside, &stroked);
            if (!inside && !stroked)
                continue;
            cui_plot_point relative = {px - origin.x, py - origin.y};
            unsigned char *p = pixels + y * stride + x * 4;
            if (inside) {
                cui_color color = fill_background ? background_color(fill_sample, relative) : fill;
                if (color.a)
                    blend(p, color);
            }
            if (stroked) {
                cui_color color =
                    stroke_background ? background_color(stroke_sample, relative) : stroke;
                if (color.a)
                    blend(p, color);
            }
        }
}
cui_plot_result cui_plot_raster(unsigned char *pixels, unsigned width, unsigned height,
                                size_t stride, size_t capacity_bytes, const cui_plot_shape *shapes,
                                size_t count, cui_color clear) {
    if (!pixels || !width || !height || width > 1024 || height > 1024 ||
        stride < (size_t)width * 4 || stride > SIZE_MAX / height || count > 64 ||
        (count && !shapes))
        return CUI_PLOT_INVALID;
    if (capacity_bytes < stride * height)
        return CUI_PLOT_CAPACITY;
    plot_segment lines[4096];
    size_t used, total = 0;
    for (size_t i = 0; i < count; ++i) {
        cui_plot_result result = segments(shapes + i, lines, &used);
        if (result != CUI_PLOT_OK)
            return result;
        total += used;
    }
    if (total && (size_t)width * height > 64000000 / total)
        return CUI_PLOT_CAPACITY;
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
            unsigned char *p = pixels + y * stride + x * 4;
            p[0] = clear.r;
            p[1] = clear.g;
            p[2] = clear.b;
            p[3] = clear.a;
        }
    for (size_t i = 0; i < count; ++i) {
        (void)segments(shapes + i, lines, &used);
        raster_segments(pixels, width, height, stride, lines, used, (cui_plot_point){0},
                        shapes[i].fill, shapes[i].stroke, shapes[i].stroke_width, NULL, NULL,
                        (cui_plot_rect){0});
    }
    return CUI_PLOT_OK;
}
static cui_plot_result element_appearance(double now, bool reduced, cui_id id,
                                          cui_plot_element_state *next,
                                          const cui_plot_element_options *o) {
    next->appearance = (cui_plot_appear_value){.time = 1, .progress = 1};
    if (!o->has_appearance) {
        next->has_generation = next->has_frame = false;
        next->scope = NULL;
        next->appear_clock = (cui_plot_transition_clock){0};
        return CUI_PLOT_OK;
    }
    if (next->has_frame && next->last_frame != o->frame_serial &&
        (next->last_frame == UINT64_MAX || next->last_frame + 1 != o->frame_serial))
        *next = (cui_plot_element_state){.owner = id};
    next->last_frame = o->frame_serial;
    next->has_frame = true;
    uint64_t scope_identity = o->appear_scope ? o->appear_scope->identity : 0;
    if (!next->has_generation || next->generation != o->generation ||
        next->scope != o->appear_scope || next->scope_identity != scope_identity)
        next->appear_clock = (cui_plot_transition_clock){0};
    next->generation = o->generation;
    next->has_generation = true;
    next->scope = o->appear_scope;
    next->scope_identity = scope_identity;
    bool complete = false;
    if (o->appear_scope) {
        cui_plot_result result =
            cui_plot_appear_registry_complete(o->appear_scope, id, o->generation, &complete);
        if (result != CUI_PLOT_OK)
            return result;
        if (o->appear_scope->last_frame != o->frame_serial)
            return CUI_PLOT_INVALID;
    }
    if (complete)
        next->appearance = (cui_plot_appear_value){.time = 1, .progress = 1};
    else {
        cui_plot_result result =
            cui_plot_appear_step(&next->appear_clock, now, o->appear, reduced, &next->appearance);
        if (result != CUI_PLOT_OK)
            return result;
    }
    if (o->appear_scope && next->appearance.time == 1 && !complete) {
        size_t index = 0;
        while (index < o->appear_scope->count && o->appear_scope->entries[index].identity != id)
            ++index;
        if (index == o->appear_scope->count && index == o->appear_scope->capacity)
            return CUI_PLOT_CAPACITY;
    }
    return CUI_PLOT_OK;
}
static cui_plot_result element_prepare(double now, bool reduced, cui_id id,
                                       const cui_plot_view_options *view,
                                       cui_plot_element_state *next,
                                       const cui_plot_element_options *o) {
    if (!o || !next || (o->custom_motion && !cui_plot_motion_valid(&o->motion)) ||
        !point_valid(o->pointer) ||
        !point_valid((cui_plot_point){o->bounds.x_min, o->bounds.y_min}) ||
        !point_valid((cui_plot_point){o->bounds.x_max, o->bounds.y_max}) ||
        o->bounds.x_max - o->bounds.x_min != view->bitmap.width ||
        o->bounds.y_max - o->bounds.y_min != view->bitmap.height || o->tooltip_layer < 0)
        return CUI_PLOT_INVALID;
    cui_plot_element_options resolved_options;
    if (o->custom_motion) {
        resolved_options = *o;
        resolved_options.enter = cui_plot_motion_transition_adapter(&resolved_options.motion.enter);
        resolved_options.exit = cui_plot_motion_transition_adapter(&resolved_options.motion.exit);
        resolved_options.appear =
            cui_plot_motion_transition_adapter(&resolved_options.motion.appear);
        o = &resolved_options;
    }
    if (next->owner != id)
        *next = (cui_plot_element_state){.owner = id};
    cui_plot_result result = element_appearance(now, reduced, id, next, o);
    if (result != CUI_PLOT_OK)
        return result;
    if (o->on_appear)
        o->on_appear(id, &next->appearance, o->user);
    if (o->custom_motion) {
        /* The adapter points into this call's local options. Persist only values;
         * later stagger sampling takes an explicit copied policy instead. */
        next->appearance.easing = NULL;
        next->appearance.user = NULL;
    }
    next->request_frame = next->appearance.request_frame;
    if (!view->interactive || view->disabled) {
        next->cursor = (cui_plot_cursor_memory){0};
        next->hover = (cui_plot_hover_memory){0};
        next->transition = (cui_plot_transition_clock){0};
        return CUI_PLOT_OK;
    }
    if (!o->touch) {
        cui_plot_cursor_update update;
        result = cui_plot_cursor_mouse(&next->cursor, o->bounds, o->pointer, o->hovered, &update);
        if (result != CUI_PLOT_OK)
            return result;
    }
    cui_plot_point relative = {0};
    bool resolved = false;
    result =
        cui_plot_cursor_resolve(&next->cursor, o->bounds, o->pointer,
                                o->appearing || next->appearance.appearing, &relative, &resolved);
    if (result != CUI_PLOT_OK)
        return result;
    cui_plot_tooltip_state live = {0};
    bool has_live = false;
    if (resolved && o->resolve) {
        result = o->resolve(relative, o->bounds, &live, o->user);
        if (result != CUI_PLOT_OK && result != CUI_PLOT_EMPTY)
            return result;
        has_live = result == CUI_PLOT_OK;
    }
    cui_plot_motion_value motion;
    result = cui_plot_transition_step(&next->transition, has_live ? 1 : 0, now,
                                      has_live ? o->enter : o->exit, reduced, &motion);
    if (result != CUI_PLOT_OK)
        return result;
    next->request_frame |= motion.request_frame;
    result =
        cui_plot_hover_track(&next->hover, has_live ? &live : NULL, has_live ? &relative : NULL,
                             o->custom_motion ? fmax(0, fmin(1, motion.value)) : motion.value);
    return result;
}
typedef struct {
    cui_plot_view_options view;
    cui_plot_element_options element;
    cui_plot_element_state *state;
    cui_theme theme;
    bool reduced;
} plot_prepaint_payload;
static bool plot_prepaint(const cui_prepaint_info *info, const void *data,
                          cui_prepaint_writer *writer) {
    const plot_prepaint_payload *p = data;
    cui_plot_element_options o = p->element;
    o.bounds =
        (cui_plot_rect){info->bounds.x, info->bounds.y, (double)info->bounds.x + info->bounds.w,
                        (double)info->bounds.y + info->bounds.h};
    o.pointer = info->pointer_valid ? (cui_plot_point){info->pointer_x, info->pointer_y}
                                    : (cui_plot_point){info->bounds.x, info->bounds.y};
    o.hovered = info->pointer_valid && info->pointer_visible;
    cui_plot_element_state next = *p->state;
    if (element_prepare(info->frame_time, p->reduced, info->owner, &p->view, &next, &o) !=
        CUI_PLOT_OK)
        return false;
    cui_draw_command image = {.type = CUI_DRAW_IMAGE,
                              .bounds = info->bounds,
                              .bitmap = {.bitmap = p->view.bitmap, .opacity = 1}};
    if (!cui_prepaint_emit(writer, &image, false))
        return false;
    const cui_plot_hover_memory *hover = next.hover.has_state ? &next.hover : NULL;
    if (o.paint_marks && !o.paint_marks(info, writer, &p->theme, hover, o.user))
        return false;
    if (hover && o.paint_tooltip && info->modal_allowed && !info->inert &&
        !o.paint_tooltip(info, writer, &p->theme, hover, o.user))
        return false;
    next.current_bounds = info->bounds;
    *p->state = next;
    if (o.has_appearance && o.appear_scope && next.appearance.time == 1)
        (void)cui_plot_appear_registry_record(o.appear_scope, info->owner, o.generation, 1);
    if (next.request_frame)
        cui_prepaint_request_frame(writer);
    return true;
}
static bool plot_current_frame(const cui_plot_element_options *o) {
    return o && (o->interaction_mode == CUI_PLOT_INTERACTION_CURRENT_FRAME ||
                 (o->interaction_mode == CUI_PLOT_INTERACTION_AUTO &&
                  (o->paint_marks || o->paint_tooltip) && !o->marks && !o->children &&
                  !o->tooltip && o->bounds.x_min == 0 && o->bounds.y_min == 0 &&
                  o->bounds.x_max == 0 && o->bounds.y_max == 0));
}
static cui_plot_result plot_view_impl(cui_context *ui, cui_id id, const cui_theme *theme,
                                      const cui_plot_view_options *o, cui_plot_element_state *state,
                                      const cui_plot_element_options *element) {
    if (!ui || !id || !theme || !o ||
        (element && element->custom_motion && !cui_plot_motion_valid(&element->motion)) ||
        (!o->name.data && o->name.length) || !cui_bitmap_valid(o->bitmap) ||
        o->bitmap.width > 16384 || o->bitmap.height > 16384)
        return CUI_PLOT_INVALID;
    bool current = plot_current_frame(element);
    if (element &&
        (element->interaction_mode < CUI_PLOT_INTERACTION_AUTO ||
         element->interaction_mode > CUI_PLOT_INTERACTION_LEGACY ||
         element->paint_command_budget > 4096 ||
         ((element->paint_marks || element->paint_tooltip) && !element->paint_command_budget) ||
         (current && (element->marks || element->children || element->tooltip || element->touch ||
                      element->tooltip_layer < 0)) ||
         (!current && (element->paint_marks || element->paint_tooltip))))
        return CUI_PLOT_INVALID;
    cui_plot_axis_label_geometry geometry[512];
    size_t labels = 0;
    if (o->axis) {
        if (o->axis->width != o->bitmap.width || o->axis->height != o->bitmap.height)
            return CUI_PLOT_INVALID;
        cui_plot_result result = cui_plot_axis_layout(ui, o->axis, geometry, 512, &labels);
        if (result != CUI_PLOT_OK)
            return result;
    }
    cui_plot_element_state next = {0};
    if (element) {
        if (!state)
            return CUI_PLOT_INVALID;
        next = *state;
        if (current) {
            cui_plot_element_options checked = *element;
            checked.bounds = (cui_plot_rect){0, 0, o->bitmap.width, o->bitmap.height};
            checked.pointer = (cui_plot_point){0};
            checked.hovered = false;
            checked.resolve = NULL;
            checked.on_appear = NULL;
            cui_plot_view_options passive = *o;
            passive.interactive = false;
            cui_plot_result result = element_prepare(cui_frame_time(ui), cui_reduced_motion(ui), id,
                                                     &passive, &next, &checked);
            if (result != CUI_PLOT_OK)
                return result;
        } else {
            cui_plot_result result =
                element_prepare(cui_frame_time(ui), cui_reduced_motion(ui), id, o, &next, element);
            if (result != CUI_PLOT_OK)
                return result;
        }
    }
    cui_rect bounds = cui_bounds(ui, id);
    if (current)
        bounds = state->current_bounds;
    else if (element)
        bounds = (cui_rect){(float)element->bounds.x_min, (float)element->bounds.y_min,
                            (float)o->bitmap.width, (float)o->bitmap.height};
    cui_begin_box(ui, id,
                  (cui_box){.width = (float)o->bitmap.width,
                            .height = (float)o->bitmap.height,
                            .fixed_width = true,
                            .fixed_height = true});
    (void)cui_interact(ui, id, CUI_ROLE_IMAGE, o->name, o->disabled, false, 0);
    cui_set_semantic_details(
        ui, id, (cui_semantic_details){.pointer_activatable = o->interactive, .no_focus = true});
    bool painted;
    if (current) {
        plot_prepaint_payload payload = {.view = *o,
                                         .element = *element,
                                         .state = state,
                                         .theme = *theme,
                                         .reduced = cui_reduced_motion(ui)};
        painted = cui_set_prepaint(
            ui, (cui_prepaint_options){.callback = plot_prepaint,
                                       .payload = &payload,
                                       .payload_bytes = sizeof payload,
                                       .command_budget = element->paint_command_budget + 5,
                                       .hover_hitbox = o->interactive && !o->disabled,
                                       .deferred_layer = element->tooltip_layer});
    } else
        painted = cui_paint_bitmap(ui, id ^ UINT64_C(0xe18a5902),
                                   (cui_bitmap_paint){.bitmap = o->bitmap, .opacity = 1},
                                   (cui_box){.width = (float)o->bitmap.width,
                                             .height = (float)o->bitmap.height,
                                             .fixed_width = true,
                                             .fixed_height = true});
    if (o->axis)
        axis_paint(ui, id, theme, o->axis, geometry, labels);
    if (painted && element && !current) {
        *state = next;
        if (element->has_appearance && element->appear_scope && state->appearance.time == 1)
            (void)cui_plot_appear_registry_record(element->appear_scope, id, element->generation,
                                                  1);
        const cui_plot_hover_memory *hover = state->hover.has_state ? &state->hover : NULL;
        if (element->marks)
            element->marks(ui, id, theme, hover, element->bounds, element->user);
        if (element->children)
            element->children(ui, id, theme, hover, element->bounds, element->user);
    }
    cui_end_box(ui);
    if (painted) {
        size_t count;
        const cui_event *events = cui_frame_events(ui, &count);
        for (size_t i = 0; i < count; ++i)
            if (o->interactive && !o->disabled && events[i].type == CUI_EVENT_POINTER_UP &&
                cui_event_activation(ui, i) == id) {
                if (o->on_activate)
                    o->on_activate(ui, id,
                                   (cui_plot_point){events[i].x - bounds.x, events[i].y - bounds.y},
                                   o->user);
                (void)cui_consume_event(ui, i);
            }
    }
    if (painted && element && !current) {
        if (next.request_frame)
            cui_invalidate(ui);
        if (state->hover.has_state && element->tooltip) {
            cui_begin_portal(ui, id ^ UINT64_C(0xad79e25c10000400),
                             (cui_box){.inert = true,
                                       .opacity = (float)state->hover.progress,
                                       .custom_opacity = true},
                             bounds, element->tooltip_layer ? element->tooltip_layer : 1);
            element->tooltip(ui, id, theme, &state->hover, element->bounds, element->user);
            cui_end_box(ui);
        }
    }
    return painted ? CUI_PLOT_OK : CUI_PLOT_CAPACITY;
}
cui_plot_result cui_plot_view(cui_context *ui, cui_id id, const cui_theme *theme,
                              const cui_plot_view_options *o) {
    return plot_view_impl(ui, id, theme, o, NULL, NULL);
}
cui_plot_result cui_plot_element(cui_context *ui, cui_id id, const cui_theme *theme,
                                 const cui_plot_view_options *view, cui_plot_element_state *state,
                                 const cui_plot_element_options *element) {
    if (!state || !element)
        return CUI_PLOT_INVALID;
    return plot_view_impl(ui, id, theme, view, state, element);
}

cui_plot_result cui_plot_grid_ex(cui_context *ui, cui_id id, const cui_theme *theme,
                                 const cui_plot_grid_options *o, cui_color stroke,
                                 bool explicit_color) {
    if (!ui || !id || !theme)
        return CUI_PLOT_INVALID;
    cui_plot_rect quads[4096];
    size_t count;
    cui_plot_result result = cui_plot_grid_layout(o, quads, 4096, &count);
    if (result != CUI_PLOT_OK)
        return result;
    cui_begin_box(ui, id,
                  (cui_box){.width = (float)o->width,
                            .height = (float)o->height,
                            .fixed_width = true,
                            .fixed_height = true});
    if (!explicit_color)
        stroke = resolved(stroke, theme->border);
    for (size_t i = 0; i < count; ++i) {
        cui_plot_rect q = quads[i];
        cui_begin_absolute_box(ui, id ^ UINT64_C(0x891ac3714f982000) ^ (i + 1),
                               (cui_box){.background = stroke},
                               (cui_rect){(float)q.x_min, (float)q.y_min,
                                          (float)(q.x_max - q.x_min), (float)(q.y_max - q.y_min)});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return CUI_PLOT_OK;
}

cui_plot_dot_options cui_plot_dot_defaults(void) {
    return (cui_plot_dot_options){.size = 4, .use_theme_fill = true};
}
static bool mark_extent(cui_size extent) {
    return isfinite(extent.width) && isfinite(extent.height) && extent.width > 0 &&
           extent.height > 0 && extent.width <= 16384 && extent.height <= 16384;
}
static bool dot_options(cui_plot_dot_options o) {
    return isfinite(o.size) && o.size >= 0 && o.size <= 1024;
}
static void dot_paint(cui_context *ui, cui_id id, const cui_theme *theme,
                      cui_plot_dot_geometry geometry, cui_plot_dot_options o) {
    cui_color fill = o.use_theme_fill ? theme->primary : o.fill;
    cui_color border = o.custom_stroke ? o.stroke : fill;
    cui_begin_absolute_box(ui, id,
                           (cui_box){.background = fill,
                                     .border = border,
                                     .border_width = 1,
                                     .radius = (float)geometry.radius},
                           (cui_rect){(float)geometry.frame.x_min, (float)geometry.frame.y_min,
                                      (float)o.size, (float)o.size});
    cui_end_box(ui);
}
cui_plot_result cui_plot_dots(cui_context *ui, cui_id id, const cui_theme *theme, cui_size extent,
                              const cui_plot_sample *samples, size_t count,
                              cui_plot_dot_options o) {
    if (!ui || !id || !theme || !mark_extent(extent) || !dot_options(o) || count > 4096 ||
        (count && !samples))
        return CUI_PLOT_INVALID;
    cui_plot_dot_geometry geometry;
    for (size_t i = 0; i < count; ++i)
        if (samples[i].defined) {
            if (!point_valid((cui_plot_point){samples[i].x, samples[i].y1}))
                return CUI_PLOT_INVALID;
            cui_plot_result result = cui_plot_dot_layout(
                (cui_plot_point){samples[i].x, samples[i].y1}, o.size, &geometry);
            if (result != CUI_PLOT_OK)
                return result;
        }
    cui_begin_box(ui, id,
                  (cui_box){.width = extent.width,
                            .height = extent.height,
                            .fixed_width = true,
                            .fixed_height = true});
    for (size_t i = 0; i < count; ++i)
        if (samples[i].defined) {
            (void)cui_plot_dot_layout((cui_plot_point){samples[i].x, samples[i].y1}, o.size,
                                      &geometry);
            dot_paint(ui, id ^ UINT64_C(0x721dd761552a0000) ^ (i + 1), theme, geometry, o);
        }
    cui_end_box(ui);
    return CUI_PLOT_OK;
}
static cui_plot_point radial_dot(cui_plot_radial_sample sample, cui_size extent) {
    double angle = fmod(sample.angle, 2 * acos(-1.));
    return (cui_plot_point){extent.width / 2 + sample.radius1 * sin(angle),
                            extent.height / 2 - sample.radius1 * cos(angle)};
}
cui_plot_result cui_plot_radial_dots(cui_context *ui, cui_id id, const cui_theme *theme,
                                     cui_size extent, const cui_plot_radial_sample *samples,
                                     size_t count, cui_plot_dot_options o) {
    if (!ui || !id || !theme || !mark_extent(extent) || !dot_options(o) || count > 4096 ||
        (count && !samples))
        return CUI_PLOT_INVALID;
    cui_plot_dot_geometry geometry;
    for (size_t i = 0; i < count; ++i)
        if (samples[i].defined) {
            if (!isfinite(samples[i].angle) || !isfinite(samples[i].radius1) ||
                samples[i].radius1 < 0)
                return CUI_PLOT_INVALID;
            cui_plot_point point = radial_dot(samples[i], extent);
            if (!point_valid(point))
                return CUI_PLOT_INVALID;
            cui_plot_result result = cui_plot_dot_layout(point, o.size, &geometry);
            if (result != CUI_PLOT_OK)
                return result;
        }
    cui_begin_box(ui, id,
                  (cui_box){.width = extent.width,
                            .height = extent.height,
                            .fixed_width = true,
                            .fixed_height = true});
    for (size_t i = 0; i < count; ++i)
        if (samples[i].defined) {
            (void)cui_plot_dot_layout(radial_dot(samples[i], extent), o.size, &geometry);
            dot_paint(ui, id ^ UINT64_C(0x7397d895aac70000) ^ (i + 1), theme, geometry, o);
        }
    cui_end_box(ui);
    return CUI_PLOT_OK;
}
cui_plot_bar_options cui_plot_bar_defaults(void) {
    return (cui_plot_bar_options){.alignment = CUI_PLOT_BAR_BOTTOM, .use_theme_fill = true};
}
cui_plot_result cui_plot_bars(cui_context *ui, cui_id id, const cui_theme *theme, cui_size extent,
                              const cui_plot_bar_sample *samples, size_t count,
                              const cui_plot_bar_options *o) {
    if (!ui || !id || !theme || !o || !mark_extent(extent) || count > 4096 || (count && !samples) ||
        !isfinite(o->band_width) || o->band_width < 0 || o->band_width > 16384 ||
        o->alignment < CUI_PLOT_BAR_BOTTOM || o->alignment > CUI_PLOT_BAR_RIGHT)
        return CUI_PLOT_INVALID;
    float corners[] = {o->corners.top_left, o->corners.top_right, o->corners.bottom_left,
                       o->corners.bottom_right};
    for (size_t i = 0; i < 4; ++i)
        if (!isfinite(corners[i]) || corners[i] < 0 || corners[i] > 1024)
            return CUI_PLOT_INVALID;
    cui_plot_bar_geometry geometry;
    for (size_t i = 0; i < count; ++i)
        if (samples[i].has_cross && samples[i].has_value) {
            if (!isfinite(samples[i].cross) || !isfinite(samples[i].baseline) ||
                !isfinite(samples[i].value) || fabs(samples[i].cross) > 16384 ||
                fabs(samples[i].baseline) > 16384 || fabs(samples[i].value) > 16384)
                return CUI_PLOT_INVALID;
            cui_plot_result result =
                cui_plot_bar_layout(o->alignment, samples[i].cross, o->band_width,
                                    samples[i].baseline, samples[i].value, &geometry);
            if (result != CUI_PLOT_OK)
                return result;
            if (fabs(geometry.frame.x_max) > 16384 || fabs(geometry.frame.y_max) > 16384)
                return CUI_PLOT_INVALID;
        }
    cui_begin_box(ui, id,
                  (cui_box){.width = extent.width,
                            .height = extent.height,
                            .fixed_width = true,
                            .fixed_height = true});
    for (size_t i = 0; i < count; ++i)
        if (samples[i].has_cross && samples[i].has_value) {
            (void)cui_plot_bar_layout(o->alignment, samples[i].cross, o->band_width,
                                      samples[i].baseline, samples[i].value, &geometry);
            cui_color fill = o->use_theme_fill ? theme->primary : o->fill;
            if (o->fill_for)
                fill = o->fill_for(samples + i, i, geometry.frame, o->alignment, o->user);
            cui_begin_absolute_box(
                ui, id ^ UINT64_C(0x61498ec3a7980000) ^ (i + 1),
                (cui_box){.background = fill, .corner_radii = o->corners, .custom_corners = true},
                (cui_rect){(float)geometry.frame.x_min, (float)geometry.frame.y_min,
                           (float)(geometry.frame.x_max - geometry.frame.x_min),
                           (float)(geometry.frame.y_max - geometry.frame.y_min)});
            cui_end_box(ui);
        }
    cui_end_box(ui);
    return CUI_PLOT_OK;
}

static bool plot_label_valid(const cui_plot_label *label) {
    return (!label->text.length || label->text.data) && label->text.length <= 4096 &&
           isfinite(label->origin.x) && isfinite(label->origin.y) &&
           fabs(label->origin.x) <= 16384 && fabs(label->origin.y) <= 16384 &&
           isfinite(label->font_size) && label->font_size >= 0 && label->font_size <= 1024 &&
           label->align >= CUI_ALIGN_START && label->align <= CUI_ALIGN_END;
}
static bool plot_label_measure(cui_context *ui, cui_str text, float size, cui_size *out) {
    *out = text.length ? cui_measure_text(ui, text, size) : (cui_size){0, size};
    return isfinite(out->width) && isfinite(out->height) && out->width >= 0 && out->height >= 0 &&
           out->width <= 16777216 && out->height <= 16777216;
}
cui_plot_result cui_plot_labels_layout(cui_context *ui, const cui_plot_label *labels, size_t count,
                                       cui_rect *out, size_t capacity, size_t *required) {
    if (!ui || !required || (count && !labels) || count > 256 || (!out && capacity))
        return CUI_PLOT_INVALID;
    cui_rect geometry[256];
    for (size_t i = 0; i < count; ++i) {
        cui_size size;
        if (!plot_label_valid(labels + i) ||
            !plot_label_measure(ui, labels[i].text, labels[i].font_size ? labels[i].font_size : 10,
                                &size))
            return CUI_PLOT_INVALID;
        float x = labels[i].origin.x;
        if (labels[i].align == CUI_ALIGN_CENTER)
            x -= size.width / 2;
        else if (labels[i].align == CUI_ALIGN_END)
            x -= size.width;
        geometry[i] = (cui_rect){x, labels[i].origin.y, size.width, size.height};
    }
    *required = count;
    if (capacity < count)
        return CUI_PLOT_CAPACITY;
    if (count)
        memcpy(out, geometry, count * sizeof *out);
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_labels(cui_context *ui, cui_id id, cui_size extent,
                                const cui_plot_label *labels, size_t count) {
    if (!id || !isfinite(extent.width) || !isfinite(extent.height) || extent.width <= 0 ||
        extent.height <= 0 || extent.width > 16384 || extent.height > 16384)
        return CUI_PLOT_INVALID;
    cui_rect geometry[256];
    size_t required;
    cui_plot_result result = cui_plot_labels_layout(ui, labels, count, geometry, 256, &required);
    if (result != CUI_PLOT_OK)
        return result;
    cui_begin_box(ui, id,
                  (cui_box){.width = extent.width,
                            .height = extent.height,
                            .fixed_width = true,
                            .fixed_height = true});
    for (size_t i = 0; i < count; ++i) {
        cui_begin_absolute_box(ui, id ^ UINT64_C(0x52773d2976650000) ^ (i + 1), (cui_box){0},
                               geometry[i]);
        cui_text_line(ui, labels[i].text, labels[i].font_size ? labels[i].font_size : 10,
                      labels[i].color, geometry[i].h, (cui_text_style){0});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return CUI_PLOT_OK;
}
/* Decode scalar boundaries rather than slicing continuation bytes. */
static bool plot_utf8_cuts(cui_str text, size_t *cuts, size_t *count) {
    *count = 0;
    for (size_t i = 0; i < text.length;) {
        unsigned char c = (unsigned char)text.data[i];
        size_t n;
        uint32_t value;
        if (c < 0x80) {
            n = 1;
            value = c;
        } else if (c >= 0xc2 && c <= 0xdf) {
            n = 2;
            value = c & 31;
        } else if (c >= 0xe0 && c <= 0xef) {
            n = 3;
            value = c & 15;
        } else if (c >= 0xf0 && c <= 0xf4) {
            n = 4;
            value = c & 7;
        } else
            return false;
        if (n > text.length - i)
            return false;
        for (size_t j = 1; j < n; ++j) {
            unsigned char d = (unsigned char)text.data[i + j];
            if ((d & 0xc0) != 0x80)
                return false;
            value = (value << 6) | (d & 63);
        }
        if ((n == 3 && value < 0x800) || (n == 4 && value < 0x10000) ||
            (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff)
            return false;
        i += n;
        if (i < text.length)
            cuts[(*count)++] = i;
    }
    return true;
}
cui_plot_result cui_plot_truncate_text(cui_context *ui, cui_str text, float size, float budget,
                                       char *out, size_t capacity, size_t *required) {
    if (!ui || !required || (!out && capacity) || (!text.data && text.length) ||
        text.length > 4096 || !isfinite(size) || size < 0 || size > 1024 || !isfinite(budget))
        return CUI_PLOT_INVALID;
    size_t cuts[4096], count;
    if (!plot_utf8_cuts(text, cuts, &count))
        return CUI_PLOT_INVALID;
    if (!size)
        size = 10;
    char buffer[4100];
    size_t length = text.length;
    cui_size measured;
    if (budget > 0) {
        if (!plot_label_measure(ui, text, size, &measured))
            return CUI_PLOT_INVALID;
        if (measured.width > budget) {
            size_t lo = 0, hi = count, best = 0;
            while (lo < hi) {
                size_t mid = lo + (hi - lo) / 2, prefix = cuts[mid];
                memcpy(buffer, text.data, prefix);
                memcpy(buffer + prefix, "\xe2\x80\xa6", 3);
                if (!plot_label_measure(ui, (cui_str){buffer, prefix + 3}, size, &measured))
                    return CUI_PLOT_INVALID;
                if (measured.width <= budget) {
                    best = prefix;
                    lo = mid + 1;
                } else
                    hi = mid;
            }
            if (best)
                memcpy(buffer, text.data, best);
            memcpy(buffer + best, "\xe2\x80\xa6", 3);
            length = best + 3;
        } else if (length)
            memcpy(buffer, text.data, length);
    } else if (length)
        memcpy(buffer, text.data, length);
    buffer[length] = 0;
    *required = length + 1;
    if (capacity < length + 1)
        return CUI_PLOT_CAPACITY;
    memcpy(out, buffer, length + 1);
    return CUI_PLOT_OK;
}

cui_plot_result cui_plot_grid(cui_context *ui, cui_id id, const cui_theme *theme,
                              const cui_plot_grid_options *options, cui_color stroke) {
    return cui_plot_grid_ex(ui, id, theme, options, stroke, false);
}

static bool cached_path_valid(cui_plot_cached_path path) {
    if (path.count > 4096 || (path.count && !path.segments) || !point_valid(path.origin))
        return false;
    for (size_t i = 0; i < path.count; ++i) {
        cui_plot_line line = path.segments[i].line;
        if (!point_valid(line.start) || !point_valid(line.end) ||
            !point_valid(
                (cui_plot_point){line.start.x + path.origin.x, line.start.y + path.origin.y}) ||
            !point_valid((cui_plot_point){line.end.x + path.origin.x, line.end.y + path.origin.y}))
            return false;
    }
    return true;
}
cui_plot_result cui_plot_path_cache_init(cui_plot_path_cache *cache, cui_plot_path_segment *storage,
                                         size_t capacity) {
    if (!cache || capacity > 4096 || (capacity && !storage))
        return CUI_PLOT_INVALID;
    *cache = (cui_plot_path_cache){.storage = storage, .capacity = capacity};
    return CUI_PLOT_OK;
}
bool cui_plot_path_cache_is_warm(const cui_plot_path_cache *cache) {
    return cache && cache->warm;
}
cui_plot_result cui_plot_path_cache_get(cui_plot_path_cache *cache, uint64_t key,
                                        cui_plot_point origin, cui_plot_path_build_fn build,
                                        void *user, cui_plot_cached_path *out) {
    if (!cache || !out || cache->capacity > 4096 || cache->count > cache->capacity ||
        (cache->capacity && !cache->storage) || !point_valid(origin))
        return CUI_PLOT_INVALID;
    if (cache->warm && cache->key == key) {
        cui_plot_cached_path path = {cache->storage, cache->count, origin};
        if (!cached_path_valid(path))
            return CUI_PLOT_INVALID;
        *out = path;
        return cache->count ? CUI_PLOT_OK : CUI_PLOT_EMPTY;
    }
    if (!build)
        return CUI_PLOT_INVALID;
    cui_plot_shape shape = {0};
    cui_plot_result result = build(user, &shape);
    if (result != CUI_PLOT_OK && result != CUI_PLOT_EMPTY)
        return result;
    plot_segment temporary[4096];
    size_t count = 0;
    if (result == CUI_PLOT_OK) {
        shape.stroke_width = 0;
        result = segments(&shape, temporary, &count);
        if (result != CUI_PLOT_OK)
            return result;
    }
    if (count > cache->capacity)
        return CUI_PLOT_CAPACITY;
    if (!cached_path_valid((cui_plot_cached_path){temporary, count, origin}))
        return CUI_PLOT_INVALID;
    if (count)
        memcpy(cache->storage, temporary, count * sizeof *temporary);
    cache->count = count;
    cache->key = key;
    cache->warm = true;
    *out = (cui_plot_cached_path){cache->storage, count, origin};
    return count ? CUI_PLOT_OK : CUI_PLOT_EMPTY;
}
cui_plot_result cui_plot_raster_cached(unsigned char *pixels, unsigned width, unsigned height,
                                       size_t stride, size_t capacity_bytes,
                                       const cui_plot_cached_shape *shapes, size_t count,
                                       cui_color clear) {
    if (!pixels || !width || !height || width > 1024 || height > 1024 ||
        stride < (size_t)width * 4 || stride > SIZE_MAX / height || count > 64 ||
        (count && !shapes))
        return CUI_PLOT_INVALID;
    if (capacity_bytes < stride * height)
        return CUI_PLOT_CAPACITY;
    size_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!cached_path_valid(shapes[i].path) || !isfinite(shapes[i].stroke_width) ||
            shapes[i].stroke_width < 0 || shapes[i].stroke_width > 1024)
            return CUI_PLOT_INVALID;
        total += shapes[i].path.count;
    }
    if (total && (size_t)width * height > 64000000 / total)
        return CUI_PLOT_CAPACITY;
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
            unsigned char *p = pixels + y * stride + x * 4;
            p[0] = clear.r;
            p[1] = clear.g;
            p[2] = clear.b;
            p[3] = clear.a;
        }
    for (size_t i = 0; i < count; ++i)
        raster_segments(pixels, width, height, stride, shapes[i].path.segments,
                        shapes[i].path.count, shapes[i].path.origin, shapes[i].fill,
                        shapes[i].stroke, shapes[i].stroke_width, NULL, NULL, (cui_plot_rect){0});
    return CUI_PLOT_OK;
}

cui_plot_result cui_plot_raster_backgrounds(unsigned char *pixels, unsigned width, unsigned height,
                                            size_t stride, size_t capacity_bytes,
                                            const cui_plot_background_shape *shapes, size_t count,
                                            cui_color clear) {
    if (!pixels || !width || !height || width > 1024 || height > 1024 ||
        stride < (size_t)width * 4 || stride > SIZE_MAX / height || count > 64 ||
        (count && !shapes))
        return CUI_PLOT_INVALID;
    if (capacity_bytes < stride * height)
        return CUI_PLOT_CAPACITY;
    size_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!cached_path_valid(shapes[i].path) ||
            !background_frame_valid(shapes[i].background_frame) ||
            !background_valid(&shapes[i].fill) || !background_valid(&shapes[i].stroke) ||
            !isfinite(shapes[i].stroke_width) || shapes[i].stroke_width < 0 ||
            shapes[i].stroke_width > 1024)
            return CUI_PLOT_INVALID;
        total += shapes[i].path.count;
    }
    if (total && (size_t)width * height > 64000000 / total)
        return CUI_PLOT_CAPACITY;
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
            unsigned char *p = pixels + y * stride + x * 4;
            p[0] = clear.r;
            p[1] = clear.g;
            p[2] = clear.b;
            p[3] = clear.a;
        }
    for (size_t i = 0; i < count; ++i)
        raster_segments(pixels, width, height, stride, shapes[i].path.segments,
                        shapes[i].path.count, shapes[i].path.origin, (cui_color){0}, (cui_color){0},
                        shapes[i].stroke_width, &shapes[i].fill, &shapes[i].stroke,
                        shapes[i].background_frame);
    return CUI_PLOT_OK;
}

static bool hsla_valid(cui_plot_hsla c) {
    return isfinite(c.h) && isfinite(c.s) && isfinite(c.l) && isfinite(c.a) && c.h >= 0 &&
           c.h <= 1 && c.s >= 0 && c.s <= 1 && c.l >= 0 && c.l <= 1 && c.a >= 0 && c.a <= 1;
}
static cui_plot_background hsla_geometry(const cui_plot_hsla_background *bg) {
    return (cui_plot_background){.kind = bg->kind,
                                 .color_space = bg->color_space,
                                 .angle = bg->angle,
                                 .from = {.position = bg->from.position},
                                 .to = {.position = bg->to.position},
                                 .source_projection = true,
                                 .pattern_width = bg->pattern_width,
                                 .pattern_interval = bg->pattern_interval,
                                 .pattern_size = bg->pattern_size};
}
static bool hsla_background_valid(const cui_plot_hsla_background *bg) {
    if (!bg)
        return false;
    cui_plot_background geometry = hsla_geometry(bg);
    return background_valid(&geometry) &&
           (bg->kind == CUI_PLOT_BACKGROUND_LINEAR
                ? hsla_valid(bg->from.color) && hsla_valid(bg->to.color)
                : hsla_valid(bg->solid));
}
static cui_plot_linear_rgba hsla_linear(cui_plot_hsla c) {
    double h = c.h * 6., chroma = (1 - fabs(2. * c.l - 1)) * c.s;
    double x = chroma * (1 - fabs(fmod(h, 2) - 1)), m = c.l - chroma / 2;
    cui_plot_linear_rgba rgb = {(float)m, (float)m, (float)m, c.a};
    if (h < 1) {
        rgb.r += (float)chroma;
        rgb.g += (float)x;
    } else if (h < 2) {
        rgb.r += (float)x;
        rgb.g += (float)chroma;
    } else if (h < 3) {
        rgb.g += (float)chroma;
        rgb.b += (float)x;
    } else if (h < 4) {
        rgb.g += (float)x;
        rgb.b += (float)chroma;
    } else if (h < 5) {
        rgb.r += (float)x;
        rgb.b += (float)chroma;
    } else {
        rgb.r += (float)chroma;
        rgb.b += (float)x;
    }
    return rgb;
}
static double shader_srgb_decode(double value) {
    return value < .04045 ? value / 12.92 : pow((value + .055) / 1.055, 2.4);
}
static double shader_srgb_encode(double value) {
    return value < .0031308 ? value * 12.92 : 1.055 * pow(value, 1. / 2.4) - .055;
}
typedef struct {
    const cui_plot_hsla_background *background;
    plot_background_sampler geometry;
    cui_plot_linear_rgba solid;
    double from[3], to[3];
} plot_hsla_sampler;
static plot_hsla_sampler hsla_sampler(const cui_plot_hsla_background *bg, cui_plot_rect frame) {
    cui_plot_background geometry = hsla_geometry(bg);
    /* Color preparation is handled below; avoid meaningless byte-Lab work. */
    geometry.color_space = CUI_PLOT_COLOR_SRGB;
    plot_hsla_sampler sample = {.background = bg,
                                .geometry = background_sampler(&geometry, frame),
                                .solid = hsla_linear(bg->solid)};
    sample.geometry.background = NULL;
    if (bg->kind == CUI_PLOT_BACKGROUND_LINEAR) {
        cui_plot_linear_rgba a = hsla_linear(bg->from.color), b = hsla_linear(bg->to.color);
        if (bg->color_space == CUI_PLOT_COLOR_OKLAB) {
            plot_rgb_lab(a.r, a.g, a.b, sample.from);
            plot_rgb_lab(b.r, b.g, b.b, sample.to);
        } else {
            sample.from[0] = shader_srgb_encode(a.r);
            sample.from[1] = shader_srgb_encode(a.g);
            sample.from[2] = shader_srgb_encode(a.b);
            sample.to[0] = shader_srgb_encode(b.r);
            sample.to[1] = shader_srgb_encode(b.g);
            sample.to[2] = shader_srgb_encode(b.b);
        }
    }
    return sample;
}
static cui_plot_linear_rgba hsla_fragment(plot_hsla_sampler sample, cui_plot_point p,
                                          double coverage) {
    const cui_plot_hsla_background *bg = sample.background;
    plot_background_sampler g = sample.geometry;
    cui_plot_linear_rgba color = sample.solid;
    if (bg->kind == CUI_PLOT_BACKGROUND_LINEAR) {
        double position = .5 + (p.x - g.center_x) * g.x + (p.y - g.center_y) * g.y;
        double interval = bg->to.position - bg->from.position;
        double t = interval == 0 ? (position >= bg->to.position ? 1 : 0)
                                 : (position - bg->from.position) / interval;
        t = fmax(0, fmin(1, t));
        double rgb[3];
        if (bg->color_space == CUI_PLOT_COLOR_OKLAB)
            plot_lab_rgb(sample.from, sample.to, t, rgb);
        else
            for (size_t i = 0; i < 3; i++)
                rgb[i] = shader_srgb_decode(sample.from[i] * (1 - t) + sample.to[i] * t);
        color = (cui_plot_linear_rgba){(float)rgb[0], (float)rgb[1], (float)rgb[2],
                                       (float)(bg->from.color.a * (1 - t) + bg->to.color.a * t)};
    } else if (bg->kind == CUI_PLOT_BACKGROUND_SLASH ||
               bg->kind == CUI_PLOT_BACKGROUND_CHECKERBOARD) {
        double x = p.x - g.center_x, y = p.y - g.center_y;
        if (bg->kind == CUI_PLOT_BACKGROUND_CHECKERBOARD)
            coverage *= fmax(
                0, fmin(1, fmod(floor(x / bg->pattern_size) + floor(y / bg->pattern_size), 2)));
        else {
            double phase =
                fmod(cosf(3.1415926f / 4) * x + sinf(3.1415926f / 4) * y, g.pattern_period);
            double distance = fmin(phase, g.pattern_period - phase) - g.pattern_width;
            coverage *= fmax(0, fmin(1, .5 - distance));
        }
    }
    color.a *= (float)coverage;
    color.r *= color.a;
    color.g *= color.a;
    color.b *= color.a;
    return color;
}
cui_plot_result cui_plot_hsla_background_sample(const cui_plot_hsla_background *bg,
                                                cui_plot_rect frame, cui_plot_point position,
                                                double coverage, cui_plot_linear_rgba *out) {
    if (!out || !hsla_background_valid(bg) || !background_frame_valid(frame) ||
        !point_valid(position) || !isfinite(coverage) || coverage < 0 || coverage > 1)
        return CUI_PLOT_INVALID;
    *out = hsla_fragment(hsla_sampler(bg, frame), position, coverage);
    return CUI_PLOT_OK;
}
static void linear_over(cui_plot_linear_rgba *below, cui_plot_linear_rgba above) {
    float remainder = 1 - above.a;
    below->r = above.r + below->r * remainder;
    below->g = above.g + below->g * remainder;
    below->b = above.b + below->b * remainder;
    below->a = above.a + below->a * remainder;
}
cui_plot_result cui_plot_raster_hsla(cui_plot_linear_rgba *pixels, unsigned width, unsigned height,
                                     size_t stride, size_t capacity,
                                     const cui_plot_hsla_shape *shapes, size_t count,
                                     cui_plot_linear_rgba clear) {
    if (!pixels || !width || !height || width > 1024 || height > 1024 || stride < width ||
        stride > SIZE_MAX / sizeof *pixels / height || count > 64 || (count && !shapes) ||
        !isfinite(clear.r) || !isfinite(clear.g) || !isfinite(clear.b) || !isfinite(clear.a) ||
        clear.a < 0 || clear.a > 1 || clear.r < 0 || clear.r > clear.a || clear.g < 0 ||
        clear.g > clear.a || clear.b < 0 || clear.b > clear.a)
        return CUI_PLOT_INVALID;
    if (capacity < stride * height)
        return CUI_PLOT_CAPACITY;
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        if (!cached_path_valid(shapes[i].path) ||
            !background_frame_valid(shapes[i].background_frame) ||
            !hsla_background_valid(&shapes[i].fill) || !hsla_background_valid(&shapes[i].stroke) ||
            !isfinite(shapes[i].stroke_width) || shapes[i].stroke_width < 0 ||
            shapes[i].stroke_width > 1024)
            return CUI_PLOT_INVALID;
        total += shapes[i].path.count;
    }
    if (total && (size_t)width * height > 64000000 / total)
        return CUI_PLOT_CAPACITY;
    for (unsigned y = 0; y < height; y++)
        for (unsigned x = 0; x < width; x++)
            pixels[y * stride + x] = clear;
    for (size_t i = 0; i < count; i++) {
        const cui_plot_hsla_shape *shape = shapes + i;
        plot_hsla_sampler fill = hsla_sampler(&shape->fill, shape->background_frame);
        plot_hsla_sampler stroke = hsla_sampler(&shape->stroke, shape->background_frame);
        for (unsigned y = 0; y < height; y++)
            for (unsigned x = 0; x < width; x++) {
                bool inside, stroked;
                raster_hits(shape->path.segments, shape->path.count, shape->path.origin, x + .5,
                            y + .5, shape->stroke_width, &inside, &stroked);
                cui_plot_point p = {x + .5 - shape->path.origin.x, y + .5 - shape->path.origin.y};
                if (inside)
                    linear_over(pixels + y * stride + x, hsla_fragment(fill, p, 1));
                if (stroked)
                    linear_over(pixels + y * stride + x, hsla_fragment(stroke, p, 1));
            }
    }
    return CUI_PLOT_OK;
}
