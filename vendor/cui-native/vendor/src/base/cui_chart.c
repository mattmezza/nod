/* SPDX-License-Identifier: MIT */
#include "cui/cui_chart.h"
#include <float.h>
#include <math.h>
#include <string.h>
#define CUI_CHART_MAX_ITEMS 4096
#define CUI_CHART_PI 3.14159265358979323846
static cui_color cui_chart_palette(const cui_chart_options *o, size_t i) {
    cui_color c = o->data[i].color;
    if (c.a)
        return c;
    c = o->series[i % 8];
    return c.a ? c : (cui_color){64, 120, 220, 255};
}
static bool cui_chart_valid_str(cui_str s) {
    return !s.length || s.data;
}
static double cui_chart_sankey_scaled(const cui_chart_options *o, double value) {
    return o->sankey_value_scale == CUI_CHART_SANKEY_SQRT ? sqrt(value) : value;
}
static bool cui_chart_pie_radii(const cui_chart_options *o, size_t i, double *inner,
                                double *outer) {
    double base_outer = o->pie_outer_radius > 0 ? o->pie_outer_radius : o->height * .4;
    cui_chart_pie_arc arc = {0};
    bool use_arc =
        o->data[i].value > 0 && (o->pie_inner_arc_radius_fn || o->pie_outer_arc_radius_fn);
    if (use_arc) {
        long double total = 0, before = 0;
        for (size_t j = 0; j < o->count; j++) {
            double value = o->data[j].value;
            if (!isfinite(value) || value < 0)
                return false;
            total += value;
            if (j < i)
                before += value;
        }
        if (total <= 0 || total > DBL_MAX)
            return false;
        arc.datum = &o->data[i];
        arc.index = i;
        arc.value = o->data[i].value;
        arc.start_angle = 2 * CUI_CHART_PI * (double)(before / total);
        arc.end_angle = 2 * CUI_CHART_PI * (double)((before + arc.value) / total);
        arc.pad_angle = o->pad_angle;
    }
    *inner = o->pie_inner_arc_radius_fn
                 ? (use_arc ? o->pie_inner_arc_radius_fn(&arc, o->inner_radius, o->pie_radius_user)
                            : o->inner_radius)
             : o->pie_inner_radius_fn
                 ? o->pie_inner_radius_fn(&o->data[i], i, o->inner_radius, o->pie_radius_user)
                 : o->inner_radius;
    *outer = o->pie_outer_arc_radius_fn
                 ? (use_arc ? o->pie_outer_arc_radius_fn(&arc, base_outer, o->pie_radius_user)
                            : base_outer)
             : o->pie_outer_radius_fn
                 ? o->pie_outer_radius_fn(&o->data[i], i, base_outer, o->pie_radius_user)
                 : base_outer;
    return isfinite(*inner) && isfinite(*outer) && *inner >= 0 && *outer >= *inner &&
           *outer <= 8192;
}
static bool cui_chart_sankey_margins_valid(const cui_chart_options *o) {
    cui_chart_sankey_margins m = o->sankey_margins;
    double width = o->node_width > 0 ? o->node_width : 10;
    return isfinite(m.left) && isfinite(m.right) && isfinite(m.top) && isfinite(m.bottom) &&
           m.left >= 0 && m.right >= 0 && m.top >= 0 && m.bottom >= 0 &&
           (double)m.left + m.right + width + 16 < o->width &&
           (double)m.top + m.bottom + 16 < o->height;
}
static bool cui_chart_layout_valid(const cui_chart_options *o) {
    return o && o->radar_tooltip_title_mode >= CUI_CHART_RADAR_TOOLTIP_TITLE_LEGACY &&
           o->radar_tooltip_title_mode <= CUI_CHART_RADAR_TOOLTIP_TITLE_SOURCE_LABEL &&
           (o->radar_tooltip_title_mode == CUI_CHART_RADAR_TOOLTIP_TITLE_LEGACY ||
            o->kind == CUI_CHART_RADAR) &&
           o->layout_mode >= CUI_CHART_LAYOUT_LEGACY &&
           o->layout_mode <= CUI_CHART_LAYOUT_SOURCE_POINT &&
           (o->layout_mode != CUI_CHART_LAYOUT_SOURCE_POINT || o->kind == CUI_CHART_LINE ||
            o->kind == CUI_CHART_AREA);
}
cui_plot_result cui_chart_point_axis_layout(const cui_chart_options *o, size_t *slots,
                                            float *gutter) {
    if (!slots || !gutter || !cui_chart_layout_valid(o) ||
        (o->kind != CUI_CHART_LINE && o->kind != CUI_CHART_AREA) || !o->count ||
        o->count > CUI_CHART_MAX_ITEMS || o->point_count > 1000000 ||
        (o->layout_mode == CUI_CHART_LAYOUT_LEGACY && o->point_count && o->point_count < o->count))
        return CUI_PLOT_INVALID;
    size_t count = o->point_count > o->count ? o->point_count : o->count;
    float gap = o->hide_x_axis ? 0 : o->layout_mode == CUI_CHART_LAYOUT_SOURCE_POINT ? 18 : 20;
    *slots = count;
    *gutter = gap;
    return CUI_PLOT_OK;
}
static float cui_chart_point_gutter(const cui_chart_options *o) {
    size_t slots;
    float gutter;
    if (cui_chart_point_axis_layout(o, &slots, &gutter) == CUI_PLOT_OK)
        return gutter;
    /* Preserve raw legacy geometry, including configurations render rejects. */
    return o->hide_x_axis ? 0 : 20;
}
static size_t cui_chart_point_slots(const cui_chart_options *o) {
    size_t slots;
    float gutter;
    if (cui_chart_point_axis_layout(o, &slots, &gutter) == CUI_PLOT_OK)
        return slots;
    return o->point_count ? o->point_count : o->count;
}
static cui_plot_result cui_chart_validate(const cui_chart_state *s, const cui_chart_options *o) {
    if (!s || !cui_chart_layout_valid(o) || !s->pixels || !o->data || !o->count ||
        o->count > CUI_CHART_MAX_ITEMS || o->kind < CUI_CHART_LINE || o->kind > CUI_CHART_SANKEY ||
        !o->width || !o->height || o->curve < CUI_CHART_CURVE_NATURAL ||
        o->curve > CUI_CHART_CURVE_STEP_AFTER || o->extra_series_count > 7 ||
        (o->extra_series_count && !o->extra_series) ||
        (o->extra_series_count && o->kind != CUI_CHART_LINE && o->kind != CUI_CHART_AREA &&
         o->kind != CUI_CHART_RADAR) ||
        o->width > 4096 || o->height > 4096 || o->width > SIZE_MAX / 4 / o->height ||
        ((o->kind == CUI_CHART_PIE || o->kind == CUI_CHART_RADAR || o->kind == CUI_CHART_SANKEY) &&
         o->count > 256) ||
        !isfinite(o->inner_radius) || o->inner_radius < 0 || !isfinite(o->pie_outer_radius) ||
        o->pie_outer_radius < 0 || !isfinite(o->pie_label_gap) || o->pie_label_gap < 0 ||
        !isfinite(o->candle_body_ratio) || o->candle_body_ratio < 0 || o->candle_body_ratio > 1 ||
        !isfinite(o->pad_angle) || o->pad_angle < 0 || o->pad_angle > 2 * CUI_CHART_PI ||
        !isfinite(o->bar_ratio) || o->bar_ratio < 0 || o->bar_ratio > 1 || o->tick_count > 1000 ||
        o->bar_alignment < CUI_CHART_BAR_BOTTOM || o->bar_alignment > CUI_CHART_BAR_RIGHT ||
        !isfinite(o->bar_padding_inner) || !isfinite(o->bar_padding_outer) ||
        !isfinite(o->max_bar_width) || !isfinite(o->min_bar_length) || o->bar_padding_inner < 0 ||
        o->bar_padding_inner > 1 || o->bar_padding_outer < 0 || o->bar_padding_outer > 1 ||
        o->max_bar_width < 0 || o->min_bar_length < 0 || !isfinite(o->radar_max_value) ||
        !isfinite(o->bar_corner_radius) || o->bar_corner_radius < 0 ||
        !isfinite(o->reveal_progress) || o->reveal_progress < 0 || o->reveal_progress > 1 ||
        !isfinite(o->appear_duration) || o->appear_duration < 0 || !isfinite(o->hover_duration) ||
        o->hover_duration < 0 || (o->bar_gradient_start.a == 0) != (o->bar_gradient_end.a == 0) ||
        o->radar_max_value < 0 || !isfinite(o->radar_outer_radius) || o->radar_outer_radius < 0 ||
        o->x_tick_count > 1000 || o->grid_columns > 1000 || o->reference_count > 16 ||
        o->sankey_iterations > 100 || o->sankey_value_scale < CUI_CHART_SANKEY_LINEAR ||
        o->sankey_value_scale > CUI_CHART_SANKEY_SQRT || !isfinite(o->sankey_min_link_width) ||
        o->sankey_min_link_width < 0 || !isfinite(o->sankey_link_opacity) ||
        o->sankey_link_opacity < 0 || o->sankey_link_opacity > 1 ||
        !isfinite(o->sankey_node_corner_radius) || o->sankey_node_corner_radius < 0 ||
        !isfinite(o->sankey_label_gap) || o->sankey_label_gap < 0 ||
        !isfinite(o->sankey_font_size) || o->sankey_font_size < 0 ||
        !isfinite(o->radar_label_gap) || o->radar_label_gap < 0 ||
        (o->reference_count && !o->reference_values) ||
        (o->point_count &&
         ((o->layout_mode == CUI_CHART_LAYOUT_LEGACY && o->point_count < o->count) ||
          o->point_count > 1000000)) ||
        !isfinite(o->logical_width) || !isfinite(o->logical_height) || o->logical_width < 0 ||
        o->logical_height < 0 || !isfinite(o->value_axis_gutter) || o->value_axis_gutter < 0 ||
        o->value_axis_gutter >= o->width || !isfinite(o->y_padding_top) ||
        !isfinite(o->bar_category_gutter) || !isfinite(o->bar_value_end_gutter) ||
        o->bar_category_gutter < 0 || o->bar_value_end_gutter < 0 ||
        (o->custom_horizontal_bar_gutters &&
         (double)(o->hide_x_axis ? 0 : o->bar_category_gutter) + o->bar_value_end_gutter >=
             o->width) ||
        !isfinite(o->y_padding_bottom) || o->y_padding_top < 0 || o->y_padding_bottom < 0 ||
        (o->custom_y_padding && (o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA) &&
         (double)o->y_padding_top + o->y_padding_bottom >= o->height - cui_chart_point_gutter(o)) ||
        (o->has_domain &&
         (!isfinite(o->domain_min) || !isfinite(o->domain_max) || o->domain_min >= o->domain_max)))
        return CUI_PLOT_INVALID;
    if (s->byte_capacity < (size_t)o->width * o->height * 4)
        return CUI_PLOT_CAPACITY;
    if (o->kind == CUI_CHART_SANKEY && !cui_chart_sankey_margins_valid(o))
        return CUI_PLOT_INVALID;
    for (size_t i = 0; i < 4; i++)
        if (!isfinite(o->bar_corner_radii[i]) || o->bar_corner_radii[i] < 0)
            return CUI_PLOT_INVALID;
    for (size_t i = 0; i < o->count; i++) {
        const cui_chart_datum *d = &o->data[i];
        if (!cui_chart_valid_str(d->label) || !isfinite(d->value))
            return CUI_PLOT_INVALID;
        if (o->kind == CUI_CHART_CANDLESTICK &&
            (!isfinite(d->open) || !isfinite(d->high) || !isfinite(d->low) || !isfinite(d->close) ||
             d->low > d->high || d->open < d->low || d->open > d->high || d->close < d->low ||
             d->close > d->high))
            return CUI_PLOT_INVALID;
        if ((o->kind == CUI_CHART_PIE || o->kind == CUI_CHART_SANKEY) && d->value < 0)
            return CUI_PLOT_INVALID;
        if (o->kind == CUI_CHART_PIE) {
            double inner, outer;
            if (!cui_chart_pie_radii(o, i, &inner, &outer))
                return CUI_PLOT_INVALID;
        }
    }
    for (size_t series = 0; series < o->extra_series_count; series++) {
        const cui_chart_series *extra = &o->extra_series[series];
        if (!extra->values || extra->count != o->count || !cui_chart_valid_str(extra->name) ||
            (extra->custom_curve &&
             (extra->curve < CUI_CHART_CURVE_NATURAL || extra->curve > CUI_CHART_CURVE_STEP_AFTER)))
            return CUI_PLOT_INVALID;
        for (size_t i = 0; i < o->count; i++)
            if (!isfinite(extra->values[i]))
                return CUI_PLOT_INVALID;
    }
    for (size_t i = 0; i < o->reference_count; i++)
        if (!isfinite(o->reference_values[i]))
            return CUI_PLOT_INVALID;
    if (o->kind == CUI_CHART_SANKEY) {
        if (o->link_count > 1024 || (o->link_count && !o->links))
            return CUI_PLOT_INVALID;
        for (size_t i = 0; i < o->link_count; i++)
            if (o->links[i].source >= o->count || o->links[i].target >= o->count ||
                o->links[i].source == o->links[i].target || !isfinite(o->links[i].value) ||
                o->links[i].value < 0)
                return CUI_PLOT_INVALID;
    }
    if (o->kind == CUI_CHART_PIE) {
        long double total = 0;
        for (size_t i = 0; i < o->count; i++)
            total += o->data[i].value;
        if (total > DBL_MAX)
            return CUI_PLOT_OVERFLOW;
    }
    return CUI_PLOT_OK;
}
static void cui_chart_pixel(cui_chart_state *s, const cui_chart_options *o, int x, int y,
                            cui_color c) {
    if (x < 0 || y < 0 || (unsigned)x >= o->width || (unsigned)y >= o->height || !c.a ||
        (s->mark_clip_active && (x + .5 < s->mark_clip_left || x + .5 > s->mark_clip_x)))
        return;
    unsigned char *p = s->pixels + ((size_t)y * o->width + (unsigned)x) * 4;
    unsigned a = c.a, inv = 255 - a, back = p[3];
    unsigned out = a + (back * inv + 127) / 255;
    if (!out)
        return;
    p[0] = (unsigned char)((c.r * a + (p[0] * back * inv + 127) / 255 + out / 2) / out);
    p[1] = (unsigned char)((c.g * a + (p[1] * back * inv + 127) / 255 + out / 2) / out);
    p[2] = (unsigned char)((c.b * a + (p[2] * back * inv + 127) / 255 + out / 2) / out);
    p[3] = (unsigned char)out;
}
static void cui_chart_rect(cui_chart_state *s, const cui_chart_options *o, int x0, int y0, int x1,
                           int y1, cui_color c) {
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > (int)o->width)
        x1 = (int)o->width;
    if (y1 > (int)o->height)
        y1 = (int)o->height;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            cui_chart_pixel(s, o, x, y, c);
}
static void cui_chart_area_rect(cui_chart_state *s, const cui_chart_options *o, int x0, int y0,
                                int x1, int y1, cui_color fallback, size_t series) {
    if (!o->area_radar_fill_pixel) {
        cui_chart_rect(s, o, x0, y0, x1, y1, fallback);
        return;
    }
    x0 = x0 < 0 ? 0 : x0;
    y0 = y0 < 0 ? 0 : y0;
    x1 = x1 > (int)o->width ? (int)o->width : x1;
    y1 = y1 > (int)o->height ? (int)o->height : y1;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            cui_chart_pixel(s, o, x, y,
                            o->area_radar_fill_pixel(series, x + .5f, y + .5f, fallback,
                                                     o->area_radar_fill_user));
}
static void cui_chart_rounded_rect(cui_chart_state *s, const cui_chart_options *o,
                                   cui_plot_rect bounds, double requested_radius, cui_color color) {
    double radius = fmin(requested_radius, fmin((bounds.x_max - bounds.x_min) / 2,
                                                (bounds.y_max - bounds.y_min) / 2));
    int x0 = (int)fmax(0, floor(bounds.x_min)), x1 = (int)fmin(o->width, ceil(bounds.x_max));
    int y0 = (int)fmax(0, floor(bounds.y_min)), y1 = (int)fmin(o->height, ceil(bounds.y_max));
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            double px = x + .5, py = y + .5;
            if (px < bounds.x_min || px >= bounds.x_max || py < bounds.y_min || py >= bounds.y_max)
                continue;
            double cx = fmax(bounds.x_min + radius, fmin(bounds.x_max - radius, px));
            double cy = fmax(bounds.y_min + radius, fmin(bounds.y_max - radius, py));
            if (radius > 0 && (px - cx) * (px - cx) + (py - cy) * (py - cy) > radius * radius)
                continue;
            cui_chart_pixel(s, o, x, y, color);
        }
}
static void cui_chart_line(cui_chart_state *s, const cui_chart_options *o, double ax, double ay,
                           double bx, double by, cui_color c) {
    double dx = bx - ax, dy = by - ay, steps = fmax(fabs(dx), fabs(dy));
    if (!isfinite(steps) || steps > 20000)
        return;
    if (steps < 1) {
        cui_chart_pixel(s, o, (int)lround(ax), (int)lround(ay), c);
        return;
    }
    for (int i = 0; i <= (int)ceil(steps); i++) {
        double t = i / ceil(steps);
        cui_chart_pixel(s, o, (int)lround(ax + dx * t), (int)lround(ay + dy * t), c);
    }
}
static void cui_chart_stroke(cui_chart_state *s, const cui_chart_options *o, double ax, double ay,
                             double bx, double by, cui_color color) {
    double width = isfinite(o->line_width) && o->line_width > 0 ? fmin(o->line_width, 16) : 2;
    double radius = width / 2, dx = bx - ax, dy = by - ay, length2 = dx * dx + dy * dy;
    if (!isfinite(ax) || !isfinite(ay) || !isfinite(bx) || !isfinite(by) || !isfinite(length2))
        return;
    int x0 = (int)fmax(0, fmin(o->width, floor(fmin(ax, bx) - radius - .5)));
    int x1 = (int)fmax(0, fmin(o->width, ceil(fmax(ax, bx) + radius + .5)));
    int y0 = (int)fmax(0, fmin(o->height, floor(fmin(ay, by) - radius - .5)));
    int y1 = (int)fmax(0, fmin(o->height, ceil(fmax(ay, by) + radius + .5)));
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            double px = x + .5, py = y + .5;
            double t = length2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / length2 : 0;
            t = fmax(0, fmin(1, t));
            double distance = hypot(px - ax - t * dx, py - ay - t * dy);
            double coverage = fmax(0, fmin(1, radius + .5 - distance));
            cui_color covered = color;
            covered.a = (unsigned char)lround(color.a * coverage);
            if (covered.a)
                cui_chart_pixel(s, o, x, y, covered);
        }
}
static void cui_chart_guide(cui_chart_state *s, const cui_chart_options *o, double ax, double ay,
                            double bx, double by, cui_color c, bool solid) {
    if (solid) {
        cui_chart_line(s, o, ax, ay, bx, by, c);
        return;
    }
    double distance = hypot(bx - ax, by - ay);
    if (!isfinite(distance) || distance <= 0)
        return;
    for (double start = 0; start < distance; start += 6) {
        double end = fmin(distance, start + 4);
        cui_chart_line(s, o, ax + (bx - ax) * start / distance, ay + (by - ay) * start / distance,
                       ax + (bx - ax) * end / distance, ay + (by - ay) * end / distance, c);
    }
}
static double cui_chart_plot_left(const cui_chart_options *o) {
    if (o->kind == CUI_CHART_CANDLESTICK)
        return 0;
    if (o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA)
        return o->show_y_axis && !o->y_axis_inside
                   ? (o->value_axis_gutter > 0 ? o->value_axis_gutter : 32)
                   : 0;
    if (o->kind == CUI_CHART_BAR &&
        (o->bar_alignment == CUI_CHART_BAR_LEFT || o->bar_alignment == CUI_CHART_BAR_RIGHT)) {
        double category = o->hide_x_axis                     ? 0
                          : o->custom_horizontal_bar_gutters ? o->bar_category_gutter
                                                             : 24;
        double value_end = o->custom_horizontal_bar_gutters ? o->bar_value_end_gutter : 12;
        return o->bar_alignment == CUI_CHART_BAR_LEFT ? category : value_end;
    }
    if (o->kind == CUI_CHART_BAR)
        return o->show_y_axis && !o->y_axis_inside
                   ? (o->value_axis_gutter > 0 ? o->value_axis_gutter : 32)
                   : 0;
    return o->show_y_axis && !o->y_axis_inside
               ? (o->value_axis_gutter > 0 ? o->value_axis_gutter : 50)
               : 24;
}
static double cui_chart_plot_right(const cui_chart_options *o, double left) {
    if (o->kind == CUI_CHART_CANDLESTICK)
        return o->width;
    if (o->kind == CUI_CHART_BAR && o->bar_alignment == CUI_CHART_BAR_RIGHT)
        return o->width - (o->hide_x_axis                     ? 0
                           : o->custom_horizontal_bar_gutters ? o->bar_category_gutter
                                                              : 24);
    if (o->kind == CUI_CHART_BAR && o->bar_alignment == CUI_CHART_BAR_LEFT)
        return o->width - (o->custom_horizontal_bar_gutters ? o->bar_value_end_gutter : 12);
    if (o->kind == CUI_CHART_BAR)
        return o->width;
    return o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA
               ? o->width
               : (o->width > 36 ? o->width - 12 : left);
}
static double cui_chart_plot_bottom(const cui_chart_options *o) {
    if (o->kind == CUI_CHART_CANDLESTICK)
        return o->height - (o->hide_x_axis ? 0 : 20);
    if (o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA)
        return o->height - cui_chart_point_gutter(o) -
               (o->custom_y_padding ? o->y_padding_bottom : 0);
    if (o->kind == CUI_CHART_BAR) {
        if (o->bar_alignment == CUI_CHART_BAR_LEFT || o->bar_alignment == CUI_CHART_BAR_RIGHT)
            return o->height - (o->show_y_axis && !o->y_axis_inside ? 32 : 0);
        if (o->bar_alignment == CUI_CHART_BAR_TOP)
            return o->height - (o->show_bar_values ? 12 : 10);
        return o->height - (o->hide_x_axis ? 0 : 20);
    }
    return o->height - 20;
}
static double cui_chart_plot_top(const cui_chart_options *o) {
    if (o->kind == CUI_CHART_BAR) {
        if (o->bar_alignment == CUI_CHART_BAR_LEFT || o->bar_alignment == CUI_CHART_BAR_RIGHT)
            return 0;
        if (o->bar_alignment == CUI_CHART_BAR_TOP)
            return o->hide_x_axis ? 0 : 20;
        return o->show_bar_values ? 12 : 10;
    }
    return 12;
}
static double cui_chart_x_at(const cui_chart_options *o, size_t i, double left, double right) {
    size_t n = cui_chart_point_slots(o);
    return n > 1 ? left + (right - left) * i / (n - 1) : (left + right) / 2;
}
static double cui_chart_map_y(double v, double lo, double hi, double top, double bottom) {
    cui_plot_scale scale = {CUI_PLOT_LINEAR, lo, hi, bottom, top, true};
    double mapped = bottom;
    (void)cui_plot_map(&scale, v, &mapped);
    return mapped;
}
static void cui_chart_domain(const cui_chart_options *o, double *lo, double *hi) {
    if (o->has_domain) {
        *lo = o->domain_min;
        *hi = o->domain_max;
        return;
    }
    *lo = o->kind == CUI_CHART_CANDLESTICK ? o->data[0].low : 0;
    *hi = o->kind == CUI_CHART_CANDLESTICK ? o->data[0].high : 0;
    for (size_t i = 0; i < o->count; i++) {
        double a = o->kind == CUI_CHART_CANDLESTICK ? o->data[i].low : o->data[i].value;
        double b = o->kind == CUI_CHART_CANDLESTICK ? o->data[i].high : o->data[i].value;
        if (a < *lo)
            *lo = a;
        if (b > *hi)
            *hi = b;
    }
    for (size_t series = 0; series < o->extra_series_count; series++)
        for (size_t i = 0; i < o->count; i++) {
            double value = o->extra_series[series].values[i];
            if (value < *lo)
                *lo = value;
            if (value > *hi)
                *hi = value;
        }
    if (*lo == *hi) {
        if (*hi == 0)
            *hi = 1;
        else {
            double delta = fmax(1, fabs(*lo) * .01);
            *lo = fmax(-DBL_MAX, *lo - delta);
            *hi = fmin(DBL_MAX, *hi + delta);
        }
    }
}
bool cui_chart_value_domain(const cui_chart_options *o, double *minimum, double *maximum) {
    if (!o || !minimum || !maximum || !o->data || !o->count || o->count > CUI_CHART_MAX_ITEMS ||
        o->extra_series_count > 7 || (o->extra_series_count && !o->extra_series))
        return false;
    for (size_t i = 0; i < o->count; i++)
        if (!isfinite(o->data[i].value) ||
            (o->kind == CUI_CHART_CANDLESTICK &&
             (!isfinite(o->data[i].low) || !isfinite(o->data[i].high) ||
              o->data[i].low > o->data[i].high)))
            return false;
    for (size_t series = 0; series < o->extra_series_count; series++) {
        const cui_chart_series *extra = &o->extra_series[series];
        if (!extra->values || extra->count != o->count)
            return false;
        for (size_t i = 0; i < o->count; i++)
            if (!isfinite(extra->values[i]))
                return false;
    }
    cui_chart_domain(o, minimum, maximum);
    return isfinite(*minimum) && isfinite(*maximum) && *minimum < *maximum;
}
static cui_plot_result cui_chart_bar_bounds_domain(const cui_chart_options *o, size_t index,
                                                   double lo, double hi, cui_plot_rect *output) {
    double left = cui_chart_plot_left(o), right = cui_chart_plot_right(o, left);
    double top = cui_chart_plot_top(o), bottom = cui_chart_plot_bottom(o);
    if (right <= left || bottom <= top)
        return CUI_PLOT_EMPTY;
    bool horizontal =
        o->bar_alignment == CUI_CHART_BAR_LEFT || o->bar_alignment == CUI_CHART_BAR_RIGHT;
    size_t slots = o->point_count ? o->point_count : o->count;
    double start = horizontal ? top : left, extent = horizontal ? bottom - top : right - left;
    double inner = o->custom_bar_padding ? o->bar_padding_inner : .4;
    double outer = o->custom_bar_padding ? o->bar_padding_outer : .2;
    double avg = extent / slots, outer_width = avg * outer;
    double width = avg * (o->bar_ratio ? o->bar_ratio : 1 - inner);
    width = fmin(width, o->max_bar_width > 0 ? o->max_bar_width : 30);
    double display = (extent - 2 * outer_width) / slots;
    double step = slots > 1 ? display * (1 + inner / (slots - 1)) : extent;
    double band_start =
        slots > 1 ? start + index * step + outer_width : start + (extent - width) / 2;
    cui_plot_scale scale = {CUI_PLOT_LINEAR, lo, hi, 0, 0, true};
    if (horizontal) {
        scale.range_start = o->bar_alignment == CUI_CHART_BAR_LEFT ? left : right;
        scale.range_end = o->bar_alignment == CUI_CHART_BAR_LEFT ? right : left;
    } else {
        scale.range_start = o->bar_alignment == CUI_CHART_BAR_TOP ? top : bottom;
        scale.range_end = o->bar_alignment == CUI_CHART_BAR_TOP ? bottom : top;
    }
    double at, zero;
    if (cui_plot_map(&scale, o->data[index].value, &at) != CUI_PLOT_OK ||
        cui_plot_map(&scale, 0, &zero) != CUI_PLOT_OK)
        return CUI_PLOT_OVERFLOW;
    if (fabs(at - zero) < o->min_bar_length) {
        bool negative = o->data[index].value < 0;
        bool toward_origin =
            o->bar_alignment == CUI_CHART_BAR_BOTTOM || o->bar_alignment == CUI_CHART_BAR_RIGHT;
        at = zero + (toward_origin != negative ? -o->min_bar_length : o->min_bar_length);
    }
    cui_plot_rect r;
    if (horizontal)
        r = (cui_plot_rect){fmax(left, fmin(at, zero)), fmax(top, band_start),
                            fmin(right, fmax(at, zero)), fmin(bottom, band_start + width)};
    else
        r = (cui_plot_rect){fmax(left, band_start), fmax(top, fmin(at, zero)),
                            fmin(right, band_start + width), fmin(bottom, fmax(at, zero))};
    if (r.x_min >= r.x_max || r.y_min >= r.y_max)
        return CUI_PLOT_EMPTY;
    *output = r;
    return CUI_PLOT_OK;
}
cui_plot_result cui_chart_bar_bounds(const cui_chart_options *o, size_t index, cui_plot_rect *out) {
    if (!cui_chart_layout_valid(o) || !out || o->kind != CUI_CHART_BAR || !o->data || !o->count ||
        o->count > CUI_CHART_MAX_ITEMS || index >= o->count ||
        o->bar_alignment < CUI_CHART_BAR_BOTTOM || o->bar_alignment > CUI_CHART_BAR_RIGHT ||
        (o->point_count &&
         ((o->layout_mode == CUI_CHART_LAYOUT_LEGACY && o->point_count < o->count) ||
          o->point_count > 1000000)) ||
        !isfinite(o->bar_padding_inner) || !isfinite(o->bar_padding_outer) ||
        !isfinite(o->max_bar_width) || !isfinite(o->min_bar_length) || o->bar_padding_inner < 0 ||
        o->bar_padding_inner > 1 || o->bar_padding_outer < 0 || o->bar_padding_outer > 1 ||
        o->max_bar_width < 0 || o->min_bar_length < 0 || !isfinite(o->value_axis_gutter) ||
        o->value_axis_gutter < 0 || o->value_axis_gutter >= o->width ||
        !isfinite(o->bar_category_gutter) || !isfinite(o->bar_value_end_gutter) ||
        o->bar_category_gutter < 0 || o->bar_value_end_gutter < 0 ||
        (o->custom_horizontal_bar_gutters &&
         (double)(o->hide_x_axis ? 0 : o->bar_category_gutter) + o->bar_value_end_gutter >=
             o->width))
        return CUI_PLOT_INVALID;
    double lo, hi;
    if (!cui_chart_value_domain(o, &lo, &hi))
        return CUI_PLOT_INVALID;
    return cui_chart_bar_bounds_domain(o, index, lo, hi, out);
}
cui_plot_result cui_chart_bar_band_center(const cui_chart_options *o, size_t index, float *center) {
    if (!cui_chart_layout_valid(o) || !center || o->kind != CUI_CHART_BAR || !o->data ||
        !o->count || o->count > CUI_CHART_MAX_ITEMS || index >= o->count || !o->width ||
        !o->height || o->width > 4096 || o->height > 4096 ||
        o->bar_alignment < CUI_CHART_BAR_BOTTOM || o->bar_alignment > CUI_CHART_BAR_RIGHT ||
        (o->point_count &&
         ((o->layout_mode == CUI_CHART_LAYOUT_LEGACY && o->point_count < o->count) ||
          o->point_count > 1000000)) ||
        !isfinite(o->bar_padding_inner) || !isfinite(o->bar_padding_outer) ||
        o->bar_padding_inner < 0 || o->bar_padding_inner > 1 || o->bar_padding_outer < 0 ||
        o->bar_padding_outer > 1 || !isfinite(o->bar_ratio) || o->bar_ratio < 0 ||
        o->bar_ratio > 1 || !isfinite(o->max_bar_width) || o->max_bar_width < 0 ||
        !isfinite(o->value_axis_gutter) || o->value_axis_gutter < 0 ||
        o->value_axis_gutter >= o->width || !isfinite(o->bar_category_gutter) ||
        !isfinite(o->bar_value_end_gutter) || o->bar_category_gutter < 0 ||
        o->bar_value_end_gutter < 0 ||
        (o->custom_horizontal_bar_gutters &&
         (double)(o->hide_x_axis ? 0 : o->bar_category_gutter) + o->bar_value_end_gutter >=
             o->width))
        return CUI_PLOT_INVALID;
    bool horizontal =
        o->bar_alignment == CUI_CHART_BAR_LEFT || o->bar_alignment == CUI_CHART_BAR_RIGHT;
    double left = cui_chart_plot_left(o), right = cui_chart_plot_right(o, left);
    double top = cui_chart_plot_top(o), bottom = cui_chart_plot_bottom(o);
    double start = horizontal ? top : left;
    double extent = horizontal ? bottom - top : right - left;
    if (extent <= 0)
        return CUI_PLOT_EMPTY;
    size_t slots = o->point_count ? o->point_count : o->count;
    double inner = o->custom_bar_padding ? o->bar_padding_inner : .4;
    double outer = o->custom_bar_padding ? o->bar_padding_outer : .2;
    double average = extent / slots;
    double outer_width = average * outer;
    double band_width = average * (o->bar_ratio ? o->bar_ratio : 1 - inner);
    band_width = fmin(band_width, o->max_bar_width > 0 ? o->max_bar_width : 30);
    double display = (extent - 2 * outer_width) / slots;
    double step = slots > 1 ? display * (1 + inner / (slots - 1)) : extent;
    double band_start =
        slots > 1 ? start + index * step + outer_width : start + (extent - band_width) / 2;
    double result = band_start + band_width / 2;
    if (!isfinite(result) || result < -4096 || result > 8192)
        return CUI_PLOT_OVERFLOW;
    *center = (float)result;
    return CUI_PLOT_OK;
}
cui_plot_result cui_chart_candle_bounds(const cui_chart_options *o, size_t index,
                                        cui_chart_candle_geometry *out) {
    if (!cui_chart_layout_valid(o) || !out || o->kind != CUI_CHART_CANDLESTICK || !o->data ||
        !o->count || o->count > CUI_CHART_MAX_ITEMS || index >= o->count || !o->width ||
        !o->height || o->width > 4096 || o->height > 4096 ||
        (o->point_count &&
         ((o->layout_mode == CUI_CHART_LAYOUT_LEGACY && o->point_count < o->count) ||
          o->point_count > 1000000)) ||
        (o->has_domain && (!isfinite(o->domain_min) || !isfinite(o->domain_max) ||
                           o->domain_min >= o->domain_max)) ||
        !isfinite(o->max_bar_width) || o->max_bar_width < 0 || !isfinite(o->candle_body_ratio) ||
        o->candle_body_ratio < 0 || o->candle_body_ratio > 1 || !isfinite(o->value_axis_gutter) ||
        o->value_axis_gutter < 0 || o->value_axis_gutter >= o->width)
        return CUI_PLOT_INVALID;
    for (size_t i = 0; i < o->count; i++) {
        const cui_chart_datum *d = &o->data[i];
        if (!isfinite(d->open) || !isfinite(d->high) || !isfinite(d->low) || !isfinite(d->close) ||
            d->low > d->high || d->open < d->low || d->open > d->high || d->close < d->low ||
            d->close > d->high)
            return CUI_PLOT_INVALID;
    }
    double lo, hi;
    cui_chart_domain(o, &lo, &hi);
    if (!isfinite(lo) || !isfinite(hi) || lo >= hi)
        return CUI_PLOT_OVERFLOW;
    double left = cui_chart_plot_left(o), right = cui_chart_plot_right(o, left);
    double top = 10, bottom = cui_chart_plot_bottom(o);
    if (right <= left || bottom <= top)
        return CUI_PLOT_EMPTY;
    size_t slots = o->point_count ? o->point_count : o->count;
    double extent = right - left, average = extent / slots;
    double outer = average * .2;
    double display = (extent - 2 * outer) / slots;
    double step = slots > 1 ? display * (1 + .4 / (slots - 1)) : extent;
    double band = fmin(o->max_bar_width > 0 ? o->max_bar_width : 30, average * .6);
    double center = slots > 1 ? left + outer + index * step + band / 2 : (left + right) / 2;
    double width = band * (o->candle_body_ratio ? o->candle_body_ratio : .8);
    const cui_chart_datum *d = &o->data[index];
    double y_low = cui_chart_map_y(d->low, lo, hi, top, bottom);
    double y_high = cui_chart_map_y(d->high, lo, hi, top, bottom);
    double y_open = cui_chart_map_y(d->open, lo, hi, top, bottom);
    double y_close = cui_chart_map_y(d->close, lo, hi, top, bottom);
    cui_chart_candle_geometry result = {.wick = {center, y_high, center, y_low},
                                        .body = {center - width / 2, fmin(y_open, y_close),
                                                 center + width / 2, fmax(y_open, y_close)},
                                        .bullish = d->close > d->open};
    *out = result;
    return CUI_PLOT_OK;
}
static double cui_chart_series_value(const cui_chart_options *o, size_t series, size_t point) {
    return series ? o->extra_series[series - 1].values[point] : o->data[point].value;
}
static void cui_chart_series_point(const cui_chart_options *o, size_t series, size_t point,
                                   double left, double right, double lo, double hi, double top,
                                   double bottom, double *x, double *y) {
    *x = cui_chart_x_at(o, point, left, right);
    *y = cui_chart_map_y(cui_chart_series_value(o, series, point), lo, hi, top, bottom);
    if (*y < top)
        *y = top;
    if (*y > bottom)
        *y = bottom;
}
cui_plot_result cui_chart_point_coordinates(const cui_chart_options *o, size_t index, size_t series,
                                            float *x, float *y) {
    if (!cui_chart_layout_valid(o) || !x || !y || !o->data ||
        (o->kind != CUI_CHART_LINE && o->kind != CUI_CHART_AREA) || !o->count ||
        o->count > CUI_CHART_MAX_ITEMS || index >= o->count || series > o->extra_series_count ||
        !o->width || !o->height || o->width > 4096 || o->height > 4096 ||
        !isfinite(o->value_axis_gutter) || o->value_axis_gutter < 0 ||
        o->value_axis_gutter >= o->width || !isfinite(o->y_padding_top) ||
        !isfinite(o->y_padding_bottom) || o->y_padding_top < 0 || o->y_padding_bottom < 0 ||
        (o->point_count &&
         ((o->layout_mode == CUI_CHART_LAYOUT_LEGACY && o->point_count < o->count) ||
          o->point_count > 1000000)))
        return CUI_PLOT_INVALID;
    double left = cui_chart_plot_left(o), right = cui_chart_plot_right(o, left);
    double top = o->custom_y_padding ? o->y_padding_top : 10;
    double bottom = cui_chart_plot_bottom(o);
    if (right <= left || bottom <= top)
        return CUI_PLOT_EMPTY;
    double lo, hi;
    if (!cui_chart_value_domain(o, &lo, &hi))
        return CUI_PLOT_INVALID;
    double px, py;
    cui_chart_series_point(o, series, index, left, right, lo, hi, top, bottom, &px, &py);
    if (!isfinite(px) || !isfinite(py))
        return CUI_PLOT_OVERFLOW;
    *x = (float)px;
    *y = (float)py;
    return CUI_PLOT_OK;
}
static void cui_chart_draw_series(cui_chart_state *s, const cui_chart_options *o, size_t series,
                                  double left, double right, double top, double bottom, double lo,
                                  double hi, double baseline) {
    cui_color color = series ? o->extra_series[series - 1].color : o->series[0];
    if (!color.a)
        color = o->series[series].a ? o->series[series] : cui_chart_palette(o, 0);
    cui_color fill = color;
    fill.a = (unsigned char)(fill.a / 3);
    cui_color explicit_fill = series ? o->extra_series[series - 1].fill : o->primary_fill;
    if (explicit_fill.a)
        fill = explicit_fill;
    cui_chart_curve curve = series && o->extra_series[series - 1].custom_curve
                                ? o->extra_series[series - 1].curve
                                : o->curve;
    double first_x, first_y;
    cui_chart_series_point(o, series, 0, left, right, lo, hi, top, bottom, &first_x, &first_y);
    if (o->kind == CUI_CHART_AREA)
        cui_chart_area_rect(s, o, (int)floor(first_x), (int)floor(fmin(first_y, baseline)),
                            (int)ceil(first_x + 1), (int)ceil(fmax(first_y, baseline)), fill,
                            series);
    for (size_t i = 1; i < o->count; i++) {
        double x0, y0, x1, y1;
        cui_chart_series_point(o, series, i - 1, left, right, lo, hi, top, bottom, &x0, &y0);
        cui_chart_series_point(o, series, i, left, right, lo, hi, top, bottom, &x1, &y1);
        if (curve == CUI_CHART_CURVE_STEP_AFTER) {
            cui_chart_stroke(s, o, x0, y0, x1, y0, color);
            if (i + 1 < o->count)
                cui_chart_stroke(s, o, x1, y0, x1, y1, color);
            if (o->kind == CUI_CHART_AREA)
                cui_chart_area_rect(s, o, (int)ceil(x0), (int)floor(fmin(y0, baseline)),
                                    (int)ceil(x1), (int)ceil(fmax(y0, baseline)), fill, series);
        } else if (curve == CUI_CHART_CURVE_LINEAR) {
            cui_chart_stroke(s, o, x0, y0, x1, y1, color);
            if (o->kind == CUI_CHART_AREA)
                for (int x = (int)ceil(x0); x <= (int)floor(x1); x++) {
                    double y = y0 + (y1 - y0) * (x - x0) / (x1 - x0);
                    cui_chart_area_rect(s, o, x, (int)floor(fmin(y, baseline)), x + 1,
                                        (int)ceil(fmax(y, baseline)), fill, series);
                }
        } else {
            double prev_y = y0, prev_x = x0, p0y = y0, p3y = y1;
            if (i > 1) {
                double unused;
                cui_chart_series_point(o, series, i - 2, left, right, lo, hi, top, bottom, &unused,
                                       &p0y);
            }
            if (i + 1 < o->count) {
                double unused;
                cui_chart_series_point(o, series, i + 1, left, right, lo, hi, top, bottom, &unused,
                                       &p3y);
            }
            double c1 = y0 + (y1 - p0y) / 6, c2 = y1 - (p3y - y0) / 6;
            int steps = (int)fmax(4, ceil(x1 - x0) * 2);
            for (int step = 1; step <= steps; step++) {
                double t = (double)step / steps, u = 1 - t;
                double x = x0 + (x1 - x0) * t;
                double y =
                    u * u * u * y0 + 3 * u * u * t * c1 + 3 * u * t * t * c2 + t * t * t * y1;
                if (y < top)
                    y = top;
                if (y > bottom)
                    y = bottom;
                cui_chart_stroke(s, o, prev_x, prev_y, x, y, color);
                if (o->kind == CUI_CHART_AREA)
                    cui_chart_area_rect(s, o, (int)floor(x), (int)floor(fmin(y, baseline)),
                                        (int)ceil(x + 1), (int)ceil(fmax(y, baseline)), fill,
                                        series);
                prev_x = x;
                prev_y = y;
            }
        }
    }
    for (size_t i = 0; i < o->count; i++) {
        double x, y;
        cui_chart_series_point(o, series, i, left, right, lo, hi, top, bottom, &x, &y);
        cui_chart_rect(s, o, (int)lround(x) - 1, (int)lround(y) - 1, (int)lround(x) + 2,
                       (int)lround(y) + 2, color);
    }
}
static cui_color cui_chart_blend_color(cui_color a, cui_color b, double t) {
    t = fmax(0, fmin(1, t));
    return (cui_color){
        (unsigned char)lround(a.r + (b.r - a.r) * t), (unsigned char)lround(a.g + (b.g - a.g) * t),
        (unsigned char)lround(a.b + (b.b - a.b) * t), (unsigned char)lround(a.a + (b.a - a.a) * t)};
}
static void cui_chart_draw_bar(cui_chart_state *s, const cui_chart_options *o, size_t index,
                               cui_plot_rect bounds, cui_color solid) {
    double emphasis = 1;
    if (!o->noninteractive && s->emphasis_focus > 0 && o->count > 1) {
        float center = 0, first = 0, second = 0;
        if (cui_chart_bar_band_center(o, index, &center) == CUI_PLOT_OK &&
            cui_chart_bar_band_center(o, 0, &first) == CUI_PLOT_OK &&
            cui_chart_bar_band_center(o, 1, &second) == CUI_PLOT_OK) {
            double step = fmax(.0001, fabs(second - first));
            double distance = fmin(1, fabs(center - s->emphasis_center) / step);
            emphasis = 1 - .45 * fmin(1, s->emphasis_focus) * distance;
        }
    }
    bool horizontal =
        o->bar_alignment == CUI_CHART_BAR_LEFT || o->bar_alignment == CUI_CHART_BAR_RIGHT;
    bool positive = o->data[index].value >= 0;
    bool forward = (o->bar_alignment == CUI_CHART_BAR_BOTTOM ||
                    o->bar_alignment == CUI_CHART_BAR_RIGHT) == positive;
    double start = horizontal ? (forward ? bounds.x_min : bounds.x_max)
                              : (forward ? bounds.y_max : bounds.y_min);
    double end = horizontal ? (forward ? bounds.x_max : bounds.x_min)
                            : (forward ? bounds.y_min : bounds.y_max);
    double progress = o->reveal_enabled && !o->reduced_motion ? o->reveal_progress : 1;
    double reveal_end = start + (end - start) * progress;
    if (horizontal) {
        bounds.x_min = fmin(start, reveal_end);
        bounds.x_max = fmax(start, reveal_end);
    } else {
        bounds.y_min = fmin(start, reveal_end);
        bounds.y_max = fmax(start, reveal_end);
    }
    double max_radius = fmin((bounds.x_max - bounds.x_min) / 2, (bounds.y_max - bounds.y_min) / 2);
    int x0 = (int)fmax(0, floor(bounds.x_min)), x1 = (int)fmin(o->width, ceil(bounds.x_max));
    int y0 = (int)fmax(0, floor(bounds.y_min)), y1 = (int)fmin(o->height, ceil(bounds.y_max));
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            double px = x + .5, py = y + .5;
            if (px < bounds.x_min || px >= bounds.x_max || py < bounds.y_min || py >= bounds.y_max)
                continue;
            size_t corner = py < (bounds.y_min + bounds.y_max) / 2
                                ? (px < (bounds.x_min + bounds.x_max) / 2 ? 0 : 1)
                                : (px < (bounds.x_min + bounds.x_max) / 2 ? 3 : 2);
            double radius = fmin(max_radius, o->custom_bar_corners ? o->bar_corner_radii[corner]
                                                                   : o->bar_corner_radius);
            if (radius > 0) {
                double cx = fmax(bounds.x_min + radius, fmin(bounds.x_max - radius, px));
                double cy = fmax(bounds.y_min + radius, fmin(bounds.y_max - radius, py));
                if ((px - cx) * (px - cx) + (py - cy) * (py - cy) > radius * radius)
                    continue;
            }
            cui_color color = solid;
            if (o->bar_gradient_start.a && o->bar_gradient_end.a) {
                double coordinate = horizontal ? px : py;
                double t = end != start ? (coordinate - start) / (end - start) : 0;
                color = cui_chart_blend_color(o->bar_gradient_start, o->bar_gradient_end, t);
            }
            if (o->bar_fill_pixel)
                color = o->bar_fill_pixel(&o->data[index], index, bounds,
                                          (cui_plot_rect){0, 0, o->width, o->height}, (float)px,
                                          (float)py, o->bar_fill_user);
            color.a = (unsigned char)lround(color.a * emphasis);
            cui_chart_pixel(s, o, x, y, color);
        }
}
static void cui_chart_cartesian(cui_chart_state *s, const cui_chart_options *o) {
    double left = cui_chart_plot_left(o), right = cui_chart_plot_right(o, left),
           top = cui_chart_plot_top(o), bottom = cui_chart_plot_bottom(o);
    if (o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA) {
        top = o->custom_y_padding ? o->y_padding_top : 10;
        bottom = cui_chart_plot_bottom(o);
    } else if (o->kind == CUI_CHART_CANDLESTICK)
        top = 10;
    if (right <= left || bottom <= top)
        return;
    bool point_chart = o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA;
    double grid_top = point_chart ? 0 : top;
    double grid_bottom = point_chart ? o->height - cui_chart_point_gutter(o) : bottom;
    double lo, hi;
    cui_chart_domain(o, &lo, &hi);
    bool horizontal = o->kind == CUI_CHART_BAR && (o->bar_alignment == CUI_CHART_BAR_LEFT ||
                                                   o->bar_alignment == CUI_CHART_BAR_RIGHT);
    cui_color grid = o->grid.a ? o->grid : (cui_color){120, 120, 120, 80};
    if (!o->hide_grid) {
        unsigned n = o->tick_count ? o->tick_count : 5;
        if (n < 2)
            n = 2;
        for (unsigned i = 0; i + 1 < n; i++) {
            if (horizontal) {
                double x = o->bar_alignment == CUI_CHART_BAR_LEFT
                               ? right - (right - left) * i / (n - 1)
                               : left + (right - left) * i / (n - 1);
                cui_chart_guide(s, o, x, top, x, bottom, grid, o->grid_solid);
            } else {
                double y = point_chart ? grid_top + (grid_bottom - grid_top) * i / (n - 1)
                           : o->kind == CUI_CHART_CANDLESTICK && !o->tick_count ? bottom * i / 4
                           : o->kind == CUI_CHART_BAR && o->bar_alignment == CUI_CHART_BAR_TOP
                               ? bottom - (bottom - top) * i / (n - 1)
                               : top + (bottom - top) * i / (n - 1);
                cui_chart_guide(s, o, left, y, right, y, grid, o->grid_solid);
            }
        }
        for (unsigned i = 0; i < o->grid_columns; i++) {
            double x = left + (right - left) * i / o->grid_columns;
            cui_chart_guide(s, o, x, grid_top, x, grid_bottom, grid, o->grid_solid);
        }
    }
    if (!o->hide_axes) {
        if (!o->hide_x_axis) {
            if (o->kind == CUI_CHART_BAR) {
                cui_plot_scale scale = {CUI_PLOT_LINEAR, lo, hi, 0, 0, true};
                if (horizontal) {
                    scale.range_start = o->bar_alignment == CUI_CHART_BAR_LEFT ? left : right;
                    scale.range_end = o->bar_alignment == CUI_CHART_BAR_LEFT ? right : left;
                } else {
                    scale.range_start = o->bar_alignment == CUI_CHART_BAR_TOP ? top : bottom;
                    scale.range_end = o->bar_alignment == CUI_CHART_BAR_TOP ? bottom : top;
                }
                double zero;
                if (cui_plot_map(&scale, 0, &zero) == CUI_PLOT_OK) {
                    if (horizontal)
                        cui_chart_line(s, o, zero, top, zero, bottom, grid);
                    else
                        cui_chart_line(s, o, left, zero, right, zero, grid);
                }
            } else
                cui_chart_line(s, o, left, grid_bottom, right, grid_bottom, grid);
        }
    }
    for (size_t i = 0; i < o->reference_count; i++) {
        double value = o->reference_values[i];
        if (value >= lo && value <= hi) {
            double y = cui_chart_map_y(value, lo, hi, top, bottom);
            cui_chart_guide(s, o, left, y, right, y, o->foreground.a ? o->foreground : grid, false);
        }
    }
    double baseline = cui_chart_map_y(0, lo, hi, top, bottom);
    if (baseline < top)
        baseline = top;
    if (baseline > bottom)
        baseline = bottom;
    if (o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA) {
        s->mark_clip_active = o->reveal_enabled && !o->reduced_motion && o->reveal_progress < 1;
        s->mark_clip_left = left;
        s->mark_clip_x = left + (right - left) * o->reveal_progress;
        for (size_t series = 0; series <= o->extra_series_count; series++)
            cui_chart_draw_series(s, o, series, left, right, top, bottom, lo, hi, baseline);
        s->mark_clip_active = false;
        return;
    }
    s->mark_clip_active = o->kind == CUI_CHART_CANDLESTICK && o->reveal_enabled &&
                          !o->reduced_motion && o->reveal_progress < 1;
    s->mark_clip_left = 0;
    s->mark_clip_x = o->width * o->reveal_progress;
    for (size_t i = 0; i < o->count; i++) {
        const cui_chart_datum *d = &o->data[i];
        cui_color c = cui_chart_palette(o, i);
        double y = cui_chart_map_y(d->value, lo, hi, top, bottom);
        if (y < top)
            y = top;
        if (y > bottom)
            y = bottom;
        if (o->kind == CUI_CHART_BAR) {
            cui_plot_rect bounds;
            if (cui_chart_bar_bounds_domain(o, i, lo, hi, &bounds) == CUI_PLOT_OK)
                cui_chart_draw_bar(s, o, i, bounds, c);
        } else if (o->kind == CUI_CHART_CANDLESTICK) {
            cui_chart_candle_geometry candle;
            if (cui_chart_candle_bounds(o, i, &candle) != CUI_PLOT_OK)
                continue;
            c = candle.bullish ? (o->bullish.a ? o->bullish : c) : (o->bearish.a ? o->bearish : c);
            cui_chart_line(s, o, candle.wick.x_min, candle.wick.y_min, candle.wick.x_max,
                           candle.wick.y_max, c);
            cui_chart_rect(s, o, (int)floor(candle.body.x_min), (int)floor(candle.body.y_min),
                           (int)ceil(candle.body.x_max), (int)ceil(candle.body.y_max + 1), c);
        }
    }
    s->mark_clip_active = false;
}
cui_plot_result cui_chart_pie_labels(const cui_chart_options *o, cui_chart_pie_label *out,
                                     size_t capacity) {
    if (!o || o->kind != CUI_CHART_PIE || !o->data || !out || !o->count || o->count > 256 ||
        !o->width || !o->height || o->width > 4096 || o->height > 4096 ||
        !isfinite(o->pie_outer_radius) || o->pie_outer_radius < 0 || !isfinite(o->pie_label_gap) ||
        o->pie_label_gap < 0)
        return CUI_PLOT_INVALID;
    if (capacity < o->count)
        return CUI_PLOT_CAPACITY;
    long double total = 0;
    for (size_t i = 0; i < o->count; i++) {
        if (!isfinite(o->data[i].value) || o->data[i].value < 0 ||
            !cui_chart_valid_str(o->data[i].label))
            return CUI_PLOT_INVALID;
        double inner, outer;
        if (!cui_chart_pie_radii(o, i, &inner, &outer))
            return CUI_PLOT_INVALID;
        total += o->data[i].value;
    }
    if (total <= 0 || total > DBL_MAX)
        return CUI_PLOT_EMPTY;
    cui_chart_pie_label labels[256];
    memset(labels, 0, sizeof labels);
    double radius = o->pie_outer_radius > 0 ? o->pie_outer_radius : o->height * .4;
    double gap = o->pie_label_gap > 0 ? o->pie_label_gap : 15;
    double cx = o->width / 2.0, cy = o->height / 2.0, cumulative = 0;
    for (size_t i = 0; i < o->count; i++) {
        double sweep = 2 * CUI_CHART_PI * (double)(o->data[i].value / total);
        double midpoint = cumulative + sweep / 2;
        cumulative += sweep;
        if (sweep < CUI_CHART_PI / 360 || !o->data[i].label.length)
            continue;
        cui_chart_pie_label *label = &labels[i];
        label->text = o->data[i].label;
        label->visible = true;
        label->right = sin(midpoint) >= 0;
        double slice_inner, slice_outer;
        (void)cui_chart_pie_radii(o, i, &slice_inner, &slice_outer);
        label->edge_x = (float)(cx + sin(midpoint) * slice_outer);
        label->edge_y = (float)(cy - cos(midpoint) * slice_outer);
        label->anchor_x = (float)(cx + sin(midpoint) * (radius + gap));
        label->anchor_y = (float)(cy - cos(midpoint) * (radius + gap));
    }
    for (int side = 0; side < 2; side++) {
        size_t indices[256], count = 0;
        for (size_t i = 0; i < o->count; i++)
            if (labels[i].visible && labels[i].right == (side == 1))
                indices[count++] = i;
        for (size_t i = 1; i < count; i++) {
            size_t item = indices[i], j = i;
            while (j && labels[indices[j - 1]].anchor_y > labels[item].anchor_y) {
                indices[j] = indices[j - 1];
                j--;
            }
            indices[j] = item;
        }
        double spacing = fmin(12, o->height / (double)(count + 1));
        double previous = -DBL_MAX;
        for (size_t i = 0; i < count; i++) {
            cui_chart_pie_label *label = &labels[indices[i]];
            double y = fmax(6, fmin(o->height - 6, label->anchor_y));
            if (i && y < previous + spacing)
                y = previous + spacing;
            label->anchor_y = (float)y;
            previous = y;
        }
        double next = o->height - 6;
        for (size_t i = count; i > 0; i--) {
            cui_chart_pie_label *label = &labels[indices[i - 1]];
            if (label->anchor_y > next)
                label->anchor_y = (float)next;
            next = label->anchor_y - spacing;
            label->text_x = label->anchor_x + (label->right ? 4 : -68);
            label->text_y = label->anchor_y - 6;
        }
    }
    memcpy(out, labels, o->count * sizeof(*out));
    return CUI_PLOT_OK;
}
static void cui_chart_pie(cui_chart_state *s, const cui_chart_options *o) {
    long double sum = 0;
    for (size_t i = 0; i < o->count; i++)
        sum += o->data[i].value;
    if (sum <= 0)
        return;
    double cx = o->width / 2.0, cy = o->height / 2.0;
    double r = o->pie_outer_radius > 0 ? o->pie_outer_radius : o->height * .4;
    if (r <= 0)
        return;
    double edges[257] = {0}, inners[256], outers[256];
    for (size_t i = 0; i < o->count; i++) {
        (void)cui_chart_pie_radii(o, i, &inners[i], &outers[i]);
        double lift = !o->noninteractive
                          ? fmin(1, fmax(0, s->pie_lift[i])) * fmin(1, fmax(0, s->emphasis_focus))
                          : 0;
        r = fmax(r, outers[i] + 6 * lift);
    }
    double progress = o->reveal_enabled && !o->reduced_motion ? o->reveal_progress : 1;
    for (size_t i = 0; i < o->count; i++)
        edges[i + 1] = edges[i] + 2 * CUI_CHART_PI * progress * (double)(o->data[i].value / sum);
    edges[o->count] = 2 * CUI_CHART_PI * progress;
    int y0 = (int)fmax(0, floor(cy - r)), y1 = (int)fmin(o->height - 1, ceil(cy + r));
    int x0 = (int)fmax(0, floor(cx - r)), x1 = (int)fmin(o->width - 1, ceil(cx + r));
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            double dx = x + .5 - cx, dy = y + .5 - cy, rr = dx * dx + dy * dy;
            if (rr > r * r)
                continue;
            double angle = atan2(dy, dx) + CUI_CHART_PI / 2;
            if (angle < 0)
                angle += 2 * CUI_CHART_PI;
            size_t low = 0, high = o->count;
            while (low < high) {
                size_t mid = low + (high - low) / 2;
                if (angle >= edges[mid + 1])
                    low = mid + 1;
                else
                    high = mid;
            }
            if (low == o->count || angle < edges[low] + o->pad_angle / 2 ||
                angle >= edges[low + 1] - o->pad_angle / 2)
                continue;
            double focus = !o->noninteractive ? fmin(1, fmax(0, s->emphasis_focus)) : 0;
            double lift = fmin(1, fmax(0, s->pie_lift[low])) * focus;
            double outer = outers[low] + 6 * lift;
            if (rr < inners[low] * inners[low] || rr > outer * outer)
                continue;
            cui_color color = cui_chart_palette(o, low);
            color.a = (unsigned char)lround(color.a * (1 - .35 * focus * (1 - lift)));
            cui_chart_pixel(s, o, x, y, color);
        }
    if (o->pie_show_labels) {
        cui_chart_pie_label labels[256];
        if (cui_chart_pie_labels(o, labels, 256) == CUI_PLOT_OK) {
            double opacity = fmax(0, fmin(1, (progress - .7) / .3));
            cui_color line = o->pie_label_line_color.a
                                 ? o->pie_label_line_color
                                 : (o->grid.a ? o->grid : (cui_color){120, 120, 120, 255});
            for (size_t i = 0; i < o->count; i++)
                if (labels[i].visible) {
                    double focus = !o->noninteractive ? fmin(1, fmax(0, s->emphasis_focus)) : 0;
                    double lift = fmin(1, fmax(0, s->pie_lift[i])) * focus;
                    double label_radius =
                        o->pie_outer_radius > 0 ? o->pie_outer_radius : o->height * .4;
                    label_radius += o->pie_label_gap > 0 ? o->pie_label_gap : 15;
                    double edge_radius = fmin(outers[i] + 6 * lift, label_radius);
                    double dx = labels[i].edge_x - cx, dy = labels[i].edge_y - cy;
                    double previous_radius = hypot(dx, dy);
                    if (previous_radius > 0) {
                        labels[i].edge_x = (float)(cx + dx * edge_radius / previous_radius);
                        labels[i].edge_y = (float)(cy + dy * edge_radius / previous_radius);
                    }
                    cui_color slice_line = o->pie_label_line_color_fn
                                               ? o->pie_label_line_color_fn(
                                                     &o->data[i], i, o->pie_label_line_color_user)
                                               : line;
                    slice_line.a = (unsigned char)lround(slice_line.a * opacity);
                    cui_chart_line(s, o, labels[i].edge_x, labels[i].edge_y, labels[i].anchor_x,
                                   labels[i].anchor_y, slice_line);
                    cui_chart_line(s, o, labels[i].anchor_x, labels[i].anchor_y,
                                   labels[i].anchor_x + (labels[i].right ? 4 : -4),
                                   labels[i].anchor_y, slice_line);
                }
        }
    }
}
static void cui_chart_triangle(cui_chart_state *s, const cui_chart_options *o, double ax, double ay,
                               double bx, double by, double cx, double cy, cui_color color,
                               size_t series) {
    double left = fmax(0, floor(fmin(ax, fmin(bx, cx))));
    double right = fmin(o->width, ceil(fmax(ax, fmax(bx, cx))));
    double top = fmax(0, floor(fmin(ay, fmin(by, cy))));
    double bottom = fmin(o->height, ceil(fmax(ay, fmax(by, cy))));
    double area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (fabs(area) < 1e-9)
        return;
    for (int y = (int)top; y < (int)bottom; y++)
        for (int x = (int)left; x < (int)right; x++) {
            double px = x + .5, py = y + .5;
            double a = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
            double b = (cx - bx) * (py - by) - (cy - by) * (px - bx);
            double c = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
            if ((area > 0 && a >= 0 && b >= 0 && c >= 0) ||
                (area < 0 && a <= 0 && b <= 0 && c <= 0)) {
                cui_color fill = o->area_radar_fill_pixel
                                     ? o->area_radar_fill_pixel(series, x + .5f, y + .5f, color,
                                                                o->area_radar_fill_user)
                                     : color;
                cui_chart_pixel(s, o, x, y, fill);
            }
        }
}
static double cui_chart_radar_radius(const cui_chart_options *o) {
    return o->radar_outer_radius > 0 ? o->radar_outer_radius : o->height * .4;
}
cui_plot_result cui_chart_radar_vertex(const cui_chart_options *o, size_t dimension, size_t series,
                                       float *x, float *y) {
    if (!o || !x || !y || o->kind != CUI_CHART_RADAR || !o->data || !o->count || o->count > 256 ||
        dimension >= o->count || series > o->extra_series_count || o->extra_series_count > 7 ||
        (o->extra_series_count && !o->extra_series) || !o->width || !o->height || o->width > 4096 ||
        o->height > 4096 || !isfinite(o->radar_outer_radius) || o->radar_outer_radius < 0 ||
        !isfinite(o->radar_max_value) || o->radar_max_value < 0)
        return CUI_PLOT_INVALID;
    double max = o->radar_max_value;
    bool pinned_max = max > 0;
    for (size_t s = 0; s < o->extra_series_count; s++)
        if (!o->extra_series[s].values || o->extra_series[s].count < o->count)
            return CUI_PLOT_INVALID;
    for (size_t s = 0; s <= o->extra_series_count; s++)
        for (size_t i = 0; i < o->count; i++) {
            double value = cui_chart_series_value(o, s, i);
            if (!isfinite(value))
                return CUI_PLOT_INVALID;
            if (!pinned_max && value > max)
                max = value;
        }
    double radius = cui_chart_radar_radius(o);
    if (max <= 0 || radius <= 0)
        return CUI_PLOT_EMPTY;
    double value = cui_chart_series_value(o, series, dimension);
    double progress = o->reveal_enabled && !o->reduced_motion ? o->reveal_progress : 1;
    if (!isfinite(progress) || progress < 0 || progress > 1)
        return CUI_PLOT_INVALID;
    double r = fmin(8192, radius * fmax(0, value) / max * progress);
    double angle = -CUI_CHART_PI / 2 + 2 * CUI_CHART_PI * dimension / o->count;
    double px = o->width / 2.0 + r * cos(angle);
    double py = o->height / 2.0 + r * sin(angle);
    if (!isfinite(px) || !isfinite(py))
        return CUI_PLOT_OVERFLOW;
    *x = (float)px;
    *y = (float)py;
    return CUI_PLOT_OK;
}
static void cui_chart_radar(cui_chart_state *s, const cui_chart_options *o) {
    if (o->count < 3)
        return;
    double cx = o->width / 2.0, cy = o->height / 2.0, radius = cui_chart_radar_radius(o),
           max = o->radar_max_value;
    if (max == 0)
        for (size_t series = 0; series <= o->extra_series_count; series++)
            for (size_t i = 0; i < o->count; i++) {
                double value = cui_chart_series_value(o, series, i);
                if (value > max)
                    max = value;
            }
    if (max <= 0 || radius <= 0)
        return;
    cui_color grid = o->grid.a ? o->grid : (cui_color){120, 120, 120, 80};
    unsigned levels = o->tick_count ? o->tick_count : 4;
    if (!o->hide_grid) {
        for (unsigned level = 1; level <= levels; level++) {
            double r = radius * level / levels;
            for (size_t i = 0; i < o->count; i++) {
                double a = -CUI_CHART_PI / 2 + 2 * CUI_CHART_PI * i / o->count;
                double b = -CUI_CHART_PI / 2 + 2 * CUI_CHART_PI * ((i + 1) % o->count) / o->count;
                cui_chart_line(s, o, cx + r * cos(a), cy + r * sin(a), cx + r * cos(b),
                               cy + r * sin(b), grid);
            }
        }
        for (size_t i = 0; i < o->count; i++) {
            double a = -CUI_CHART_PI / 2 + 2 * CUI_CHART_PI * i / o->count;
            cui_chart_line(s, o, cx, cy, cx + radius * cos(a), cy + radius * sin(a), grid);
        }
    }
    if (o->reveal_enabled && !o->reduced_motion && o->reveal_progress <= 0)
        return;
    for (size_t series = 0; series <= o->extra_series_count; series++) {
        cui_color color = series ? o->extra_series[series - 1].color : o->series[0];
        if (!color.a)
            color = o->series[series].a ? o->series[series] : cui_chart_palette(o, 0);
        cui_color fill = color;
        fill.a = (unsigned char)(color.a * 3 / 10);
        cui_color explicit_fill = series ? o->extra_series[series - 1].fill : o->primary_fill;
        if (explicit_fill.a)
            fill = explicit_fill;
        double xs[256], ys[256];
        for (size_t i = 0; i < o->count; i++) {
            double a = -CUI_CHART_PI / 2 + 2 * CUI_CHART_PI * i / o->count;
            double ratio = fmax(0, cui_chart_series_value(o, series, i)) / max;
            double progress = o->reveal_enabled && !o->reduced_motion ? o->reveal_progress : 1;
            double r = fmin(8192, radius * ratio * progress);
            xs[i] = cx + r * cos(a);
            ys[i] = cy + r * sin(a);
        }
        for (size_t i = 0; i < o->count; i++) {
            size_t j = (i + 1) % o->count;
            cui_chart_triangle(s, o, cx, cy, xs[i], ys[i], xs[j], ys[j], fill, series);
        }
        for (size_t i = 0; i < o->count; i++) {
            size_t j = (i + 1) % o->count;
            cui_chart_line(s, o, xs[i], ys[i], xs[j], ys[j], color);
            if (o->show_dots)
                cui_chart_rect(s, o, (int)lround(xs[i]) - 3, (int)lround(ys[i]) - 3,
                               (int)lround(xs[i]) + 4, (int)lround(ys[i]) + 4, color);
        }
    }
}
/* Chart preserves its public defaults/raw throughput; shared Base owns all
 * topology, relaxation and link-breadth math. */
/* Geometry is invariant under uniform value scaling. Keep raw throughput below,
 * but avoid relying on long double having a wider exponent range for layer sums
 * and value-weighted pixel coordinates in the shared solver. */
static double cui_chart_sankey_normalizer(const cui_chart_options *o) {
    if (o->sankey_value_scale != CUI_CHART_SANKEY_LINEAR)
        return 1;
    double maximum = 0;
    for (size_t i = 0; i < o->link_count; ++i)
        maximum = fmax(maximum, o->links[i].value);
    return maximum > DBL_MAX / (1024.0 * 4096.0) ? maximum : 1;
}
cui_plot_result cui_chart_sankey_layout(const cui_chart_options *o, cui_chart_sankey_node *out,
                                        size_t capacity) {
    if (!o || o->kind != CUI_CHART_SANKEY || !o->data || !o->count || o->count > 256 ||
        o->link_count > 1024 || (o->link_count && !o->links) || !out ||
        o->sankey_align < CUI_CHART_SANKEY_JUSTIFY || o->sankey_align > CUI_CHART_SANKEY_CENTER ||
        !o->width || !o->height || o->width > 4096 || o->height > 4096 ||
        !isfinite(o->node_width) || !isfinite(o->node_padding) || o->node_width < 0 ||
        o->node_padding < 0 || o->sankey_iterations > 100 || !isfinite(o->sankey_min_link_width) ||
        o->sankey_value_scale < CUI_CHART_SANKEY_LINEAR ||
        o->sankey_value_scale > CUI_CHART_SANKEY_SQRT || o->sankey_min_link_width < 0)
        return CUI_PLOT_INVALID;
    if (!cui_chart_sankey_margins_valid(o))
        return CUI_PLOT_INVALID;
    if (capacity < o->count)
        return CUI_PLOT_CAPACITY;

    double normalizer = cui_chart_sankey_normalizer(o);
    cui_plot_sankey_link input[1024];
    long double incoming[256] = {0}, outgoing[256] = {0};
    for (size_t i = 0; i < o->link_count; ++i) {
        const cui_chart_link *link = o->links + i;
        if (link->source >= o->count || link->target >= o->count || link->source == link->target ||
            !isfinite(link->value) || link->value < 0)
            return CUI_PLOT_INVALID;
        input[i] = (cui_plot_sankey_link){link->source, link->target, link->value / normalizer};
        incoming[link->target] += link->value;
        outgoing[link->source] += link->value;
    }
    for (size_t i = 0; i < o->count; ++i)
        if (!cui_chart_valid_str(o->data[i].label) || !isfinite(o->data[i].value) ||
            !isfinite(incoming[i]) || !isfinite(outgoing[i]) || incoming[i] > DBL_MAX ||
            outgoing[i] > DBL_MAX)
            return CUI_PLOT_OVERFLOW;
    cui_plot_sankey_node nodes[256];
    cui_plot_sankey_link_layout links[1024];
    cui_plot_sankey_graph graph = {nodes, links, 0, 0, 256, 1024};
    cui_plot_sankey_options options = cui_plot_sankey_defaults();
    options.extent =
        (cui_plot_rect){o->sankey_margins.left, o->sankey_margins.top,
                        o->width - o->sankey_margins.right, o->height - o->sankey_margins.bottom};
    options.node_width = o->node_width ? o->node_width : 10;
    options.node_padding =
        fmin(o->node_padding ? o->node_padding : 16, options.extent.y_max - options.extent.y_min);
    options.iterations = o->sankey_iterations ? o->sankey_iterations : 6;
    options.align = (cui_plot_sankey_align)o->sankey_align;
    options.value_scale = (cui_plot_sankey_value_scale)o->sankey_value_scale;
    options.legacy_value_alignment = true;
    cui_plot_result result =
        cui_plot_sankey_layout(&graph, o->count, input, o->link_count, &options, NULL);
    if (result != CUI_PLOT_OK)
        return result;
    cui_chart_sankey_node output[256];
    for (size_t i = 0; i < o->count; ++i)
        output[i] = (cui_chart_sankey_node){.depth = nodes[i].depth,
                                            .height = nodes[i].height,
                                            .layer = nodes[i].layer,
                                            .throughput = (double)fmaxl(incoming[i], outgoing[i]),
                                            .x0 = (float)nodes[i].x0,
                                            .x1 = (float)nodes[i].x1,
                                            .y0 = (float)nodes[i].y0,
                                            .y1 = (float)nodes[i].y1};
    memcpy(out, output, o->count * sizeof(*out));
    return CUI_PLOT_OK;
}
cui_plot_result cui_chart_sankey_link_bounds(const cui_chart_options *o,
                                             const cui_chart_sankey_node *nodes,
                                             cui_chart_sankey_link_geometry *out, size_t capacity) {
    if (!o || o->kind != CUI_CHART_SANKEY || !o->data || !nodes || !out || !o->count ||
        o->count > 256 || o->link_count > 1024 || (o->link_count && !o->links))
        return CUI_PLOT_INVALID;
    if (capacity < o->link_count)
        return CUI_PLOT_CAPACITY;
    double normalizer = cui_chart_sankey_normalizer(o);
    cui_plot_sankey_node base_nodes[256] = {{0}};
    cui_plot_sankey_link_layout base_links[1024];
    for (size_t i = 0; i < o->count; ++i) {
        if (!isfinite(nodes[i].y0) || !isfinite(nodes[i].y1) || nodes[i].y0 > nodes[i].y1)
            return CUI_PLOT_INVALID;
        base_nodes[i].y0 = nodes[i].y0;
        base_nodes[i].y1 = nodes[i].y1;
    }
    for (size_t i = 0; i < o->link_count; ++i) {
        const cui_chart_link *link = o->links + i;
        if (link->source >= o->count || link->target >= o->count || !isfinite(link->value) ||
            link->value < 0)
            return CUI_PLOT_INVALID;
        base_links[i] = (cui_plot_sankey_link_layout){
            .index = i,
            .source = link->source,
            .target = link->target,
            .value = cui_chart_sankey_scaled(o, link->value) / normalizer};
    }
    cui_plot_sankey_graph graph = {base_nodes, base_links, o->count, o->link_count, 256, 1024};
    cui_plot_result result = cui_plot_sankey_link_breadths(&graph);
    if (result != CUI_PLOT_OK)
        return result;
    cui_chart_sankey_link_geometry geometry[1024];
    for (size_t i = 0; i < o->link_count; ++i)
        geometry[i] = (cui_chart_sankey_link_geometry){
            (float)base_links[i].y0, (float)base_links[i].y1, (float)base_links[i].source_width,
            (float)base_links[i].target_width};
    if (o->link_count)
        memcpy(out, geometry, o->link_count * sizeof(*out));
    return CUI_PLOT_OK;
}
static void cui_chart_sankey(cui_chart_state *s, const cui_chart_options *o,
                             const cui_chart_sankey_node *nodes,
                             const cui_chart_sankey_link_geometry *geometry) {
    for (size_t i = 0; i < o->link_count; i++) {
        const cui_chart_link *link = &o->links[i];
        size_t source = link->source, target = link->target;
        double start_y = geometry[i].source_y, end_y = geometry[i].target_y;
        double min_width = o->sankey_min_link_width > 0 ? o->sankey_min_link_width : 1;
        double start_w = fmax(geometry[i].source_width, min_width);
        double end_w = fmax(geometry[i].target_width, min_width);
        double x0 = nodes[source].x1, x1 = nodes[target].x0;
        if (x1 <= x0)
            continue;
        cui_color source_color = cui_chart_palette(o, source);
        cui_color target_color = cui_chart_palette(o, target);
        double opacity = o->custom_sankey_link_opacity ? o->sankey_link_opacity : .3;
        if (!o->noninteractive && s->emphasis_focus > 0 && s->emphasis_visual_index < o->count &&
            source != s->emphasis_visual_index && target != s->emphasis_visual_index)
            opacity *= 1 - .7 * fmin(1, s->emphasis_focus);
        for (int x = (int)ceil(x0); x < (int)x1; x++) {
            double t = (x - x0) / (x1 - x0), smooth = t * t * (3 - 2 * t),
                   center = (1 - smooth) * start_y + smooth * end_y;
            double half = ((1 - t) * start_w + t * end_w) / 2;
            cui_color color =
                link->color.a ? link->color : cui_chart_blend_color(source_color, target_color, t);
            color.a = (unsigned char)lround(color.a * opacity);
            cui_chart_rect(s, o, x, (int)floor(center - half), x + 1, (int)ceil(center + half),
                           color);
        }
    }
    for (size_t i = 0; i < o->count; i++)
        cui_chart_rounded_rect(s, o,
                               (cui_plot_rect){nodes[i].x0, nodes[i].y0, nodes[i].x1,
                                               fmax(nodes[i].y1, nodes[i].y0 + 1)},
                               o->sankey_node_corner_radius, cui_chart_palette(o, i));
}
static uint64_t cui_chart_mix(uint64_t h, uint64_t v) {
    return (h ^ v) * UINT64_C(1099511628211);
}
static uint64_t cui_chart_bits(double v) {
    uint64_t u;
    memcpy(&u, &v, sizeof u);
    return u;
}
static uint64_t cui_chart_cache_key(const cui_chart_options *o, const cui_chart_state *s) {
    if (!o->data_revision)
        return 0;
    uint64_t h = UINT64_C(1469598103934665603);
    h = cui_chart_mix(h, o->data_revision);
    h = cui_chart_mix(h, (uintptr_t)s->pixels);
    h = cui_chart_mix(h, (uintptr_t)o->data);
    h = cui_chart_mix(h, (uintptr_t)o->links);
    h = cui_chart_mix(h, (uintptr_t)o->extra_series);
    h = cui_chart_mix(h, o->count);
    h = cui_chart_mix(h, o->link_count);
    h = cui_chart_mix(h, o->extra_series_count);
    h = cui_chart_mix(h, o->kind);
    h = cui_chart_mix(h, o->width);
    h = cui_chart_mix(h, o->height);
    h = cui_chart_mix(h, o->layout_mode == CUI_CHART_LAYOUT_SOURCE_POINT ? cui_chart_point_slots(o)
                                                                         : o->point_count);
    if (o->layout_mode == CUI_CHART_LAYOUT_SOURCE_POINT) {
        h = cui_chart_mix(h, o->layout_mode);
        h = cui_chart_mix(h, cui_chart_bits(cui_chart_point_gutter(o)));
    }
    h = cui_chart_mix(h, o->tick_count);
    h = cui_chart_mix(h, o->grid_columns);
    h = cui_chart_mix(h, o->curve);
    h = cui_chart_mix(h, cui_chart_bits(o->line_width));
    h = cui_chart_mix(h, o->bar_alignment);
    h = cui_chart_mix(h, o->custom_bar_padding);
    h = cui_chart_mix(h, o->show_dots);
    h = cui_chart_mix(h, cui_chart_bits(o->value_axis_gutter));
    h = cui_chart_mix(h, o->custom_horizontal_bar_gutters);
    h = cui_chart_mix(h, cui_chart_bits(o->bar_category_gutter));
    h = cui_chart_mix(h, cui_chart_bits(o->bar_value_end_gutter));
    h = cui_chart_mix(h, o->custom_y_padding);
    h = cui_chart_mix(h, cui_chart_bits(o->y_padding_top));
    h = cui_chart_mix(h, cui_chart_bits(o->y_padding_bottom));
    h = cui_chart_mix(h, o->sankey_align);
    h = cui_chart_mix(h, o->sankey_value_scale);
    h = cui_chart_mix(h, o->sankey_iterations);
    h = cui_chart_mix(h, o->custom_sankey_link_opacity);
    h = cui_chart_mix(h, cui_chart_bits(o->sankey_link_opacity));
    h = cui_chart_mix(h, cui_chart_bits(o->sankey_node_corner_radius));
    h = cui_chart_mix(h, cui_chart_bits(o->sankey_min_link_width));
    h = cui_chart_mix(h, o->hide_grid);
    h = cui_chart_mix(h, o->hide_axes);
    h = cui_chart_mix(h, o->custom_x_tick_count);
    h = cui_chart_mix(h, o->hide_x_axis);
    h = cui_chart_mix(h, o->show_y_axis);
    h = cui_chart_mix(h, o->show_bar_values);
    h = cui_chart_mix(h, o->y_axis_inside);
    h = cui_chart_mix(h, o->grid_solid);
    h = cui_chart_mix(h, o->has_domain);
    h = cui_chart_mix(h, cui_chart_bits(o->domain_min));
    h = cui_chart_mix(h, cui_chart_bits(o->domain_max));
    h = cui_chart_mix(h, cui_chart_bits(o->inner_radius));
    h = cui_chart_mix(h, cui_chart_bits(o->pie_outer_radius));
    const unsigned char *inner_callback = (const unsigned char *)&o->pie_inner_radius_fn;
    const unsigned char *outer_callback = (const unsigned char *)&o->pie_outer_radius_fn;
    for (size_t i = 0; i < sizeof(o->pie_inner_radius_fn); i++)
        h = cui_chart_mix(h, inner_callback[i]);
    for (size_t i = 0; i < sizeof(o->pie_outer_radius_fn); i++)
        h = cui_chart_mix(h, outer_callback[i]);
    const unsigned char *inner_arc_callback = (const unsigned char *)&o->pie_inner_arc_radius_fn;
    const unsigned char *outer_arc_callback = (const unsigned char *)&o->pie_outer_arc_radius_fn;
    for (size_t i = 0; i < sizeof(o->pie_inner_arc_radius_fn); i++)
        h = cui_chart_mix(h, inner_arc_callback[i]);
    for (size_t i = 0; i < sizeof(o->pie_outer_arc_radius_fn); i++)
        h = cui_chart_mix(h, outer_arc_callback[i]);
    h = cui_chart_mix(h, (uintptr_t)o->pie_radius_user);
    h = cui_chart_mix(h, o->pie_show_labels);
    h = cui_chart_mix(h, cui_chart_bits(o->pie_label_gap));
    const unsigned char *pie_label_colors = (const unsigned char *)&o->pie_label_color;
    for (size_t i = 0; i < 2 * sizeof(cui_color); i++)
        h = cui_chart_mix(h, pie_label_colors[i]);
    const unsigned char *pie_line_callback = (const unsigned char *)&o->pie_label_line_color_fn;
    for (size_t i = 0; i < sizeof(o->pie_label_line_color_fn); i++)
        h = cui_chart_mix(h, pie_line_callback[i]);
    h = cui_chart_mix(h, (uintptr_t)o->pie_label_line_color_user);
    h = cui_chart_mix(h, cui_chart_bits(o->pad_angle));
    h = cui_chart_mix(h, cui_chart_bits(o->bar_ratio));
    h = cui_chart_mix(h, cui_chart_bits(o->candle_body_ratio));
    h = cui_chart_mix(h, cui_chart_bits(o->bar_corner_radius));
    h = cui_chart_mix(h, o->custom_bar_corners);
    for (size_t i = 0; i < 4; i++)
        h = cui_chart_mix(h, cui_chart_bits(o->bar_corner_radii[i]));
    h = cui_chart_mix(h, o->reveal_enabled);
    h = cui_chart_mix(h, o->appear_key);
    h = cui_chart_mix(h, o->disable_appear);
    h = cui_chart_mix(h, cui_chart_bits(o->appear_duration));
    h = cui_chart_mix(h, o->reduced_motion);
    h = cui_chart_mix(h, cui_chart_bits(o->reveal_progress));
    const unsigned char *gradient = (const unsigned char *)&o->bar_gradient_start;
    for (size_t i = 0; i < 2 * sizeof(cui_color); i++)
        h = cui_chart_mix(h, gradient[i]);
    const unsigned char *fill_callback = (const unsigned char *)&o->bar_fill_pixel;
    for (size_t i = 0; i < sizeof(o->bar_fill_pixel); i++)
        h = cui_chart_mix(h, fill_callback[i]);
    h = cui_chart_mix(h, (uintptr_t)o->bar_fill_user);
    h = cui_chart_mix(h, cui_chart_bits(o->bar_padding_inner));
    h = cui_chart_mix(h, cui_chart_bits(o->bar_padding_outer));
    h = cui_chart_mix(h, cui_chart_bits(o->max_bar_width));
    h = cui_chart_mix(h, cui_chart_bits(o->min_bar_length));
    h = cui_chart_mix(h, cui_chart_bits(o->radar_max_value));
    h = cui_chart_mix(h, cui_chart_bits(o->radar_outer_radius));
    h = cui_chart_mix(h, cui_chart_bits(o->node_width));
    h = cui_chart_mix(h, cui_chart_bits(o->node_padding));
    h = cui_chart_mix(h, cui_chart_bits(o->sankey_margins.left));
    h = cui_chart_mix(h, cui_chart_bits(o->sankey_margins.right));
    h = cui_chart_mix(h, cui_chart_bits(o->sankey_margins.top));
    h = cui_chart_mix(h, cui_chart_bits(o->sankey_margins.bottom));
    h = cui_chart_mix(h, o->reference_count);
    for (size_t i = 0; i < o->reference_count; i++)
        h = cui_chart_mix(h, cui_chart_bits(o->reference_values[i]));
    const cui_color *colors = &o->background;
    for (size_t i = 0; i < sizeof(cui_color) * 13; i++)
        h = cui_chart_mix(h, ((const unsigned char *)colors)[i]);
    const unsigned char *primary_fill = (const unsigned char *)&o->primary_fill;
    for (size_t i = 0; i < sizeof(o->primary_fill); i++)
        h = cui_chart_mix(h, primary_fill[i]);
    const unsigned char *region_fill = (const unsigned char *)&o->area_radar_fill_pixel;
    for (size_t i = 0; i < sizeof(o->area_radar_fill_pixel); i++)
        h = cui_chart_mix(h, region_fill[i]);
    h = cui_chart_mix(h, (uintptr_t)o->area_radar_fill_user);
    if (!o->noninteractive &&
        (o->kind == CUI_CHART_BAR || o->kind == CUI_CHART_PIE || o->kind == CUI_CHART_SANKEY)) {
        h = cui_chart_mix(h, s->hovered);
        h = cui_chart_mix(h, s->emphasis_visual_index);
        h = cui_chart_mix(h, cui_chart_bits(s->emphasis_focus));
        h = cui_chart_mix(h, cui_chart_bits(s->emphasis_center));
        if (o->kind == CUI_CHART_PIE)
            for (size_t i = 0; i < o->count; i++)
                h = cui_chart_mix(h, cui_chart_bits(s->pie_lift[i]));
    }
    return h ? h : 1;
}
/* Compare explicit animation and styled rendering in the same frame, including
 * caller-owned data when normal cross-frame caching is disabled. */
uint64_t cui_chart_frame_key(const cui_chart_options *o, const cui_chart_state *s) {
    if (!o || !s || !o->data || o->count > CUI_CHART_MAX_ITEMS || o->extra_series_count > 7 ||
        (o->extra_series_count && !o->extra_series) || o->link_count > 1024 ||
        (o->link_count && !o->links))
        return 0;
    cui_chart_options normalized = *o;
    if (!normalized.data_revision)
        normalized.data_revision = 1;
    uint64_t h = cui_chart_cache_key(&normalized, s);
    for (size_t i = 0; i < o->count; i++) {
        const cui_chart_datum *d = &o->data[i];
        h = cui_chart_mix(h, (uintptr_t)d->label.data);
        h = cui_chart_mix(h, d->label.length);
        h = cui_chart_mix(h, cui_chart_bits(d->value));
        h = cui_chart_mix(h, cui_chart_bits(d->open));
        h = cui_chart_mix(h, cui_chart_bits(d->high));
        h = cui_chart_mix(h, cui_chart_bits(d->low));
        h = cui_chart_mix(h, cui_chart_bits(d->close));
        h = cui_chart_mix(h, d->color.r | ((uint64_t)d->color.g << 8) |
                                 ((uint64_t)d->color.b << 16) | ((uint64_t)d->color.a << 24));
    }
    for (size_t i = 0; i < o->extra_series_count; i++) {
        const cui_chart_series *series = &o->extra_series[i];
        if (!series->values || series->count != o->count)
            return 0;
        for (size_t j = 0; j < o->count; j++)
            h = cui_chart_mix(h, cui_chart_bits(series->values[j]));
    }
    for (size_t i = 0; i < o->link_count; i++) {
        const cui_chart_link *link = &o->links[i];
        h = cui_chart_mix(h, link->source);
        h = cui_chart_mix(h, link->target);
        h = cui_chart_mix(h, cui_chart_bits(link->value));
        h = cui_chart_mix(h, link->color.r | ((uint64_t)link->color.g << 8) |
                                 ((uint64_t)link->color.b << 16) | ((uint64_t)link->color.a << 24));
    }
    return h ? h : 1;
}
cui_plot_result cui_chart_render(cui_chart_state *s, const cui_chart_options *o) {
    cui_plot_result status = cui_chart_validate(s, o);
    if (status != CUI_PLOT_OK)
        return status;
    cui_chart_sankey_node nodes[256];
    cui_chart_sankey_link_geometry link_geometry[1024];
    if (o->kind == CUI_CHART_SANKEY) {
        status = cui_chart_sankey_layout(o, nodes, 256);
        if (status != CUI_PLOT_OK)
            return status;
        status = cui_chart_sankey_link_bounds(o, nodes, link_geometry, 1024);
        if (status != CUI_PLOT_OK)
            return status;
    }
    if (s->rendered_data != o->data || s->rendered_count != o->count ||
        s->rendered_kind != o->kind) {
        s->hovered = SIZE_MAX;
        s->hovered_value = 0;
        s->hover_visual_started = false;
        s->hover_progress = 0;
        s->emphasis_focus = 0;
        s->emphasis_center = 0;
        s->emphasis_velocity = 0;
        s->emphasis_started = false;
        s->emphasis_visual_index = SIZE_MAX;
        s->tooltip_cursor_valid = false;
        s->appear_hover_pending = false;
        s->tooltip_phase = s->tooltip_focus = 0;
        s->tooltip_from = 0;
        s->tooltip_active = s->tooltip_started = false;
        s->tooltip_visual_index = SIZE_MAX;
        s->tooltip_visual_value = 0;
        memset(s->pie_lift, 0, sizeof s->pie_lift);
        memset(s->pie_lift_velocity, 0, sizeof s->pie_lift_velocity);
    }
    uint64_t key = cui_chart_cache_key(o, s);
    s->animated_at_valid = false;
    if (key && key == s->cache_key)
        return CUI_PLOT_OK;
    for (size_t i = 0; i < (size_t)o->width * o->height; i++) {
        s->pixels[4 * i] = o->background.r;
        s->pixels[4 * i + 1] = o->background.g;
        s->pixels[4 * i + 2] = o->background.b;
        s->pixels[4 * i + 3] = o->background.a;
    }
    if (o->kind == CUI_CHART_PIE)
        cui_chart_pie(s, o);
    else if (o->kind <= CUI_CHART_BAR || o->kind == CUI_CHART_CANDLESTICK)
        cui_chart_cartesian(s, o);
    else if (o->kind == CUI_CHART_RADAR)
        cui_chart_radar(s, o);
    else if (o->kind == CUI_CHART_SANKEY) {
        s->mark_clip_active = o->reveal_enabled && !o->reduced_motion && o->reveal_progress < 1;
        s->mark_clip_left = 0;
        s->mark_clip_x = o->width * o->reveal_progress;
        cui_chart_sankey(s, o, nodes, link_geometry);
        s->mark_clip_active = false;
    }
    s->rendered_data = o->data;
    s->rendered_count = o->count;
    s->rendered_kind = o->kind;
    s->revision++;
    s->cache_key = key;
    s->rendered_reveal_progress = o->reveal_enabled && !o->reduced_motion ? o->reveal_progress : 1;
    return CUI_PLOT_OK;
}
static size_t cui_chart_band_nearest(const cui_chart_options *o, double position, double start,
                                     double extent) {
    size_t slots = o->point_count ? o->point_count : o->count;
    if (slots <= 1)
        return 0;
    double inner = o->custom_bar_padding ? o->bar_padding_inner : .4;
    double outer = o->custom_bar_padding ? o->bar_padding_outer : .2;
    double avg = extent / slots, outer_width = avg * outer;
    double display = (extent - 2 * outer_width) / slots;
    double step = display * (1 + inner / (slots - 1));
    if (step <= 0)
        return SIZE_MAX;
    double candidate = round((position - start - outer_width) / step);
    if (candidate < 0)
        candidate = 0;
    if (candidate >= slots)
        candidate = slots - 1;
    return (size_t)candidate;
}
cui_chart_hit cui_chart_hit_test(const cui_chart_options *o, float x, float y) {
    cui_chart_hit hit = {0};
    if (!cui_chart_layout_valid(o) || !o->data || !o->count || o->count > CUI_CHART_MAX_ITEMS ||
        o->point_count > 1000000 || o->noninteractive || !isfinite(x) || !isfinite(y) || x < 0 ||
        y < 0 || x >= (o->logical_width > 0 ? o->logical_width : o->width) ||
        y >= (o->logical_height > 0 ? o->logical_height : o->height))
        return hit;
    if (o->logical_width > 0)
        x = (float)(x * o->width / o->logical_width);
    if (o->logical_height > 0)
        y = (float)(y * o->height / o->logical_height);
    size_t index = 0;
    if (o->kind == CUI_CHART_PIE) {
        double cx = o->width / 2.0, cy = o->height / 2.0;
        double dx = x - cx, dy = y - cy;
        double sum = 0;
        for (size_t i = 0; i < o->count; i++)
            sum += o->data[i].value;
        if (sum <= 0)
            return hit;
        double a = atan2(dy, dx) + CUI_CHART_PI / 2;
        if (a < 0)
            a += 2 * CUI_CHART_PI;
        double cumulative = 0;
        for (index = 0; index < o->count; index++) {
            cumulative += 2 * CUI_CHART_PI * (o->data[index].value / sum);
            /* Source Arc::contains selects the original angular interval,
             * including the padded painted gap at its edges. */
            if (a < cumulative)
                break;
        }
        if (index == o->count)
            return hit;
        double slice_inner, slice_outer;
        double radius = hypot(dx, dy);
        if (!cui_chart_pie_radii(o, index, &slice_inner, &slice_outer) || radius < slice_inner ||
            radius > slice_outer)
            return hit;
    } else if (o->kind == CUI_CHART_RADAR) {
        double dx = x - o->width / 2.0, dy = y - o->height / 2.0;
        if (!isfinite(o->radar_outer_radius) || o->radar_outer_radius < 0 ||
            !isfinite(o->radar_label_gap) || o->radar_label_gap < 0)
            return hit;
        double radius =
            cui_chart_radar_radius(o) + (o->radar_label_gap > 0 ? o->radar_label_gap : 10);
        if (dx * dx + dy * dy > radius * radius || radius <= 0)
            return hit;
        double angle = atan2(dy, dx) + CUI_CHART_PI / 2;
        if (angle < 0)
            angle += 2 * CUI_CHART_PI;
        index = (size_t)llround(angle * o->count / (2 * CUI_CHART_PI)) % o->count;
    } else if (o->kind == CUI_CHART_SANKEY) {
        cui_chart_sankey_node nodes[256];
        if (cui_chart_sankey_layout(o, nodes, 256) != CUI_PLOT_OK)
            return hit;
        for (index = 0; index < o->count; index++)
            if (x >= nodes[index].x0 && x <= nodes[index].x1 && y >= nodes[index].y0 &&
                y <= fmax(nodes[index].y1, nodes[index].y0 + 1))
                break;
        if (index == o->count)
            return hit;
        hit.valid = true;
        hit.hovered = index;
        hit.label = o->data[index].label;
        hit.value = nodes[index].throughput;
        return hit;
    } else {
        double left = cui_chart_plot_left(o), right = cui_chart_plot_right(o, left);
        bool point_chart = o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA;
        double top = o->kind == CUI_CHART_CANDLESTICK || point_chart ? 0 : cui_chart_plot_top(o);
        double bottom =
            point_chart ? o->height - cui_chart_point_gutter(o) : cui_chart_plot_bottom(o);
        if (x < left || x > right || y < top || y > bottom || right <= left)
            return hit;
        size_t n = point_chart      ? cui_chart_point_slots(o)
                   : o->point_count ? o->point_count
                                    : o->count;
        if (o->kind == CUI_CHART_BAR &&
            (o->bar_alignment == CUI_CHART_BAR_LEFT || o->bar_alignment == CUI_CHART_BAR_RIGHT)) {
            index = cui_chart_band_nearest(o, y, top, bottom - top);
            if (index >= o->count)
                return hit;
        } else if (o->kind == CUI_CHART_BAR || o->kind == CUI_CHART_CANDLESTICK) {
            index = cui_chart_band_nearest(o, x, left, right - left);
            if (index >= o->count)
                return hit;
        } else {
            double pos = (x - left) / (right - left) * (n > 1 ? n - 1 : 0);
            index = (size_t)llround(pos);
            if (index >= o->count)
                return hit;
        }
    }
    hit.valid = true;
    hit.hovered = index;
    hit.label = o->data[index].label;
    hit.value = o->kind == CUI_CHART_CANDLESTICK ? o->data[index].close : o->data[index].value;
    return hit;
}

cui_chart_hit cui_chart_update_hover(cui_chart_state *state, const cui_chart_options *options,
                                     float x, float y) {
    cui_chart_hit hit = {0};
    if (!state)
        return hit;
    hit = cui_chart_hit_test(options, x, y);
    state->hovered = hit.valid ? hit.hovered : SIZE_MAX;
    state->hovered_value = hit.valid ? hit.value : 0;
    if (!hit.valid)
        state->appear_hover_pending = false;
    if (hit.valid) {
        state->tooltip_cursor_x = x;
        state->tooltip_cursor_y = y;
        state->tooltip_cursor_valid = true;
    } else if (!options || options->noninteractive)
        state->tooltip_cursor_valid = false;
    /* Retain the visual index/cursor on ordinary exit for the styled fade. */
    return hit;
}
