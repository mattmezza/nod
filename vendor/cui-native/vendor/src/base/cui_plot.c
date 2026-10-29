/* SPDX-License-Identifier: MIT */
#include "cui/cui_plot.h"
#include <float.h>
#include <math.h>
#include <string.h>
#define CUI_PLOT_MAX_TICKS 1024
static long double cui_plot_transform(cui_plot_scale_kind kind, double value) {
    return kind == CUI_PLOT_LOG10 ? log10l((long double)value) : (long double)value;
}
static cui_plot_result cui_plot_number(long double value, double *out) {
    if (!isfinite(value) || value > DBL_MAX || value < -DBL_MAX)
        return CUI_PLOT_OVERFLOW;
    double result = (double)value;
    if (!isfinite(result))
        return CUI_PLOT_OVERFLOW;
    *out = result;
    return CUI_PLOT_OK;
}
static cui_plot_result cui_plot_untransform(cui_plot_scale_kind kind, long double value,
                                            double *out) {
    if (kind == CUI_PLOT_LOG10) {
        long double result = powl(10, value);
        if (result <= 0 || (double)result == 0)
            return CUI_PLOT_OVERFLOW;
        return cui_plot_number(result, out);
    }
    return cui_plot_number(value, out);
}
cui_plot_viewport_options cui_plot_viewport_defaults(void) {
    return (cui_plot_viewport_options){
        .enabled = true, .pan_x = true, .pan_y = true, .zoom_x = true, .zoom_y = true};
}
cui_plot_result cui_plot_viewport_init(cui_plot_viewport *v, cui_plot_scale x, cui_plot_scale y) {
    if (!v || cui_plot_validate(&x) != CUI_PLOT_OK || cui_plot_validate(&y) != CUI_PLOT_OK)
        return CUI_PLOT_INVALID;
    *v = (cui_plot_viewport){.x = x, .y = y, .initial_x = x, .initial_y = y, .initialized = true};
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_viewport_reset(cui_plot_viewport *v) {
    if (!v || !v->initialized)
        return CUI_PLOT_INVALID;
    cui_plot_scale x = v->initial_x, y = v->initial_y;
    x.range_start = v->x.range_start;
    x.range_end = v->x.range_end;
    y.range_start = v->y.range_start;
    y.range_end = v->y.range_end;
    return cui_plot_viewport_init(v, x, y);
}
static cui_plot_result viewport_axis(cui_plot_scale *s, cui_plot_viewport_limit limit, double delta,
                                     double factor, double anchor) {
    if (!isfinite(limit.min_span) || !isfinite(limit.max_span) || limit.min_span < 0 ||
        limit.max_span < 0 || (limit.max_span && limit.max_span < limit.min_span) ||
        (limit.bounded &&
         (!isfinite(limit.minimum) || !isfinite(limit.maximum) || limit.minimum >= limit.maximum ||
          (long double)limit.min_span > (long double)limit.maximum - limit.minimum)))
        return CUI_PLOT_INVALID;
    long double lo = cui_plot_transform(s->kind, s->domain_min);
    long double hi = cui_plot_transform(s->kind, s->domain_max), span = hi - lo;
    long double range = (long double)s->range_end - s->range_start;
    long double fraction = ((long double)anchor - s->range_start) / range;
    long double center = lo - (long double)delta / range * span + fraction * span;
    long double next_span = span / factor;
    if (limit.min_span)
        next_span = fmaxl(next_span, limit.min_span);
    if (limit.max_span)
        next_span = fminl(next_span, limit.max_span);
    if (limit.bounded)
        next_span = fminl(next_span, (long double)limit.maximum - limit.minimum);
    lo = center - fraction * next_span;
    hi = lo + next_span;
    if (limit.bounded) {
        if (lo < limit.minimum) {
            hi += limit.minimum - lo;
            lo = limit.minimum;
        }
        if (hi > limit.maximum) {
            lo -= hi - limit.maximum;
            hi = limit.maximum;
        }
    }
    cui_plot_result result = cui_plot_untransform(s->kind, lo, &s->domain_min);
    if (result == CUI_PLOT_OK)
        result = cui_plot_untransform(s->kind, hi, &s->domain_max);
    if (result == CUI_PLOT_OK)
        result = cui_plot_validate(s);
    return result;
}
cui_plot_result cui_plot_viewport_transform(cui_plot_viewport *v, cui_plot_viewport_options o,
                                            double dx, double dy, double factor,
                                            cui_plot_point anchor) {
    if (!v || !v->initialized || !isfinite(dx) || !isfinite(dy) || !isfinite(factor) ||
        factor <= 0 || !isfinite(anchor.x) || !isfinite(anchor.y) ||
        cui_plot_validate(&v->x) != CUI_PLOT_OK || cui_plot_validate(&v->y) != CUI_PLOT_OK)
        return CUI_PLOT_INVALID;
    if (!o.enabled || o.disabled)
        return CUI_PLOT_OK;
    cui_plot_viewport next = *v;
    cui_plot_result result =
        viewport_axis(&next.x, o.x_limit, o.pan_x ? dx : 0, o.zoom_x ? factor : 1, anchor.x);
    if (result == CUI_PLOT_OK)
        result =
            viewport_axis(&next.y, o.y_limit, o.pan_y ? dy : 0, o.zoom_y ? factor : 1, anchor.y);
    if (result == CUI_PLOT_OK)
        *v = next;
    return result;
}
static long double cui_plot_fraction(long double value, long double a, long double b) {
    long double span = b - a;
    if (!isfinite(span) || !isfinite(value - a))
        return (value / 2 - a / 2) / (b / 2 - a / 2);
    return (value - a) / span;
}
static long double cui_plot_lerp(long double a, long double b, long double t) {
    if (t == 0)
        return a;
    if (t == 1)
        return b;
    return (1 - t) * a + t * b;
}
static long double cui_plot_clamp_fraction(long double t, bool clamp) {
    return clamp ? fmaxl(0, fminl(1, t)) : t;
}
cui_plot_result cui_plot_validate(const cui_plot_scale *s) {
    if (!s || (s->kind != CUI_PLOT_LINEAR && s->kind != CUI_PLOT_LOG10) ||
        !isfinite(s->domain_min) || !isfinite(s->domain_max) || !isfinite(s->range_start) ||
        !isfinite(s->range_end) || s->domain_min > s->domain_max ||
        (s->kind == CUI_PLOT_LOG10 && s->domain_min <= 0))
        return CUI_PLOT_INVALID;
    if (s->domain_min == s->domain_max || s->range_start == s->range_end ||
        cui_plot_transform(s->kind, s->domain_min) == cui_plot_transform(s->kind, s->domain_max))
        return CUI_PLOT_DEGENERATE;
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_map(const cui_plot_scale *s, double value, double *pixel) {
    cui_plot_result status = cui_plot_validate(s);
    if (status != CUI_PLOT_OK)
        return status;
    if (!pixel || !isfinite(value) || (s->kind == CUI_PLOT_LOG10 && value <= 0))
        return CUI_PLOT_INVALID;
    long double t = cui_plot_fraction(cui_plot_transform(s->kind, value),
                                      cui_plot_transform(s->kind, s->domain_min),
                                      cui_plot_transform(s->kind, s->domain_max));
    t = cui_plot_clamp_fraction(t, s->clamp);
    return cui_plot_number(cui_plot_lerp(s->range_start, s->range_end, t), pixel);
}
cui_plot_result cui_plot_inverse(const cui_plot_scale *s, double pixel, double *value) {
    cui_plot_result status = cui_plot_validate(s);
    if (status != CUI_PLOT_OK)
        return status;
    if (!value || !isfinite(pixel))
        return CUI_PLOT_INVALID;
    long double t =
        cui_plot_clamp_fraction(cui_plot_fraction(pixel, s->range_start, s->range_end), s->clamp);
    if (t == 0) {
        *value = s->domain_min;
        return CUI_PLOT_OK;
    }
    if (t == 1) {
        *value = s->domain_max;
        return CUI_PLOT_OK;
    }
    return cui_plot_untransform(s->kind,
                                cui_plot_lerp(cui_plot_transform(s->kind, s->domain_min),
                                              cui_plot_transform(s->kind, s->domain_max), t),
                                value);
}
static long double cui_plot_tick_step(const cui_plot_scale *s, size_t desired) {
    long double a = cui_plot_transform(s->kind, s->domain_min);
    long double b = cui_plot_transform(s->kind, s->domain_max);
    long double raw = (b - a) / (desired - 1);
    if (!isfinite(raw))
        raw = (b / (desired - 1) - a / (desired - 1));
    if (raw <= 0)
        raw = DBL_TRUE_MIN;
    long double power = powl(10, floorl(log10l(raw)));
    if (power == 0)
        power = DBL_TRUE_MIN;
    long double normalized = raw / power;
    /* log10l/powl and the span division can straddle an exact nice-step
       threshold by a few arithmetic ulps. Do not double a nominal unit step
       merely because one libm returns log10l(1000) just above 3. */
    long double rounding = 1 + 8 * LDBL_EPSILON;
    long double step = power * (normalized <= rounding       ? 1
                                : normalized <= 2 * rounding ? 2
                                : normalized <= 5 * rounding ? 5
                                                             : 10);
    if (s->kind == CUI_PLOT_LINEAR) {
        double magnitude = fmax(fabs(s->domain_min), fabs(s->domain_max));
        double ulp = magnitude - nextafter(magnitude, 0);
        step = fmaxl(step, fmaxl(ulp, DBL_TRUE_MIN));
        step = fminl(step, DBL_MAX);
    }
    return step;
}
cui_plot_result cui_plot_ticks(const cui_plot_scale *s, size_t desired, double *ticks,
                               size_t capacity, size_t *count) {
    if (!count)
        return CUI_PLOT_INVALID;
    *count = 0;
    cui_plot_result status = cui_plot_validate(s);
    if (status != CUI_PLOT_OK)
        return status;
    if (desired < 2 || desired > 1000 || (!ticks && capacity))
        return CUI_PLOT_INVALID;
    long double a = cui_plot_transform(s->kind, s->domain_min);
    long double b = cui_plot_transform(s->kind, s->domain_max);
    long double step = cui_plot_tick_step(s, desired);
    /* Inspect the adjacent grid index on each side: a transformed endpoint
       can round across its exact index. Inclusion is decided below in the
       original double domain, never by a tolerant out-of-domain comparison. */
    long double first = ceill(a / step) - 1;
    long double last = floorl(b / step) + 1;
    double values[CUI_PLOT_MAX_TICKS];
    size_t n = 0;
    for (size_t i = 0; i < CUI_PLOT_MAX_TICKS; ++i) {
        long double index = first + i;
        if (index > last)
            break;
        long double tick = index * step;
        double value;
        status = cui_plot_untransform(s->kind, tick, &value);
        if (status != CUI_PLOT_OK)
            continue;
        if (value < s->domain_min || value > s->domain_max || (n && value <= values[n - 1]))
            continue;
        values[n++] = value == 0 ? 0 : value;
    }
    *count = n;
    if (capacity < n)
        return CUI_PLOT_CAPACITY;
    if (n)
        memcpy(ticks, values, n * sizeof(*ticks));
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_nice(cui_plot_scale *s, size_t desired) {
    cui_plot_result status = cui_plot_validate(s);
    if (status != CUI_PLOT_OK)
        return status;
    if (desired < 2 || desired > 1000)
        return CUI_PLOT_INVALID;
    long double step = cui_plot_tick_step(s, desired);
    long double lo = floorl(cui_plot_transform(s->kind, s->domain_min) / step) * step;
    long double hi = ceill(cui_plot_transform(s->kind, s->domain_max) / step) * step;
    cui_plot_scale next = *s;
    if (cui_plot_untransform(s->kind, lo, &next.domain_min) != CUI_PLOT_OK)
        next.domain_min = s->kind == CUI_PLOT_LOG10 ? DBL_TRUE_MIN : -DBL_MAX;
    if (cui_plot_untransform(s->kind, hi, &next.domain_max) != CUI_PLOT_OK)
        next.domain_max = DBL_MAX;
    next.domain_min = fmin(next.domain_min, s->domain_min);
    next.domain_max = fmax(next.domain_max, s->domain_max);
    status = cui_plot_validate(&next);
    if (status == CUI_PLOT_OK)
        *s = next;
    return status;
}
static cui_plot_result cui_plot_set_transformed(cui_plot_scale *s, long double a, long double b) {
    cui_plot_scale next = *s;
    cui_plot_result status = cui_plot_untransform(s->kind, a, &next.domain_min);
    if (status != CUI_PLOT_OK)
        return status;
    status = cui_plot_untransform(s->kind, b, &next.domain_max);
    if (status != CUI_PLOT_OK)
        return status;
    status = cui_plot_validate(&next);
    if (status == CUI_PLOT_OK)
        *s = next;
    return status;
}
cui_plot_result cui_plot_pan(cui_plot_scale *s, double delta_pixels) {
    cui_plot_result status = cui_plot_validate(s);
    if (status != CUI_PLOT_OK)
        return status;
    if (!isfinite(delta_pixels))
        return CUI_PLOT_INVALID;
    if (delta_pixels == 0)
        return CUI_PLOT_OK;
    long double a = cui_plot_transform(s->kind, s->domain_min);
    long double b = cui_plot_transform(s->kind, s->domain_max);
    long double span = (long double)s->range_end - s->range_start;
    long double ratio = isfinite(span)
                            ? delta_pixels / span
                            : ((long double)delta_pixels / 2) /
                                  ((long double)s->range_end / 2 - (long double)s->range_start / 2);
    long double shift = -ratio * b + ratio * a;
    return cui_plot_set_transformed(s, a + shift, b + shift);
}
cui_plot_result cui_plot_zoom(cui_plot_scale *s, double factor, double anchor) {
    cui_plot_result status = cui_plot_validate(s);
    if (status != CUI_PLOT_OK)
        return status;
    if (!isfinite(factor) || factor <= 0 || !isfinite(anchor))
        return CUI_PLOT_INVALID;
    if (factor == 1)
        return CUI_PLOT_OK;
    long double a = cui_plot_transform(s->kind, s->domain_min);
    long double b = cui_plot_transform(s->kind, s->domain_max);
    long double t = cui_plot_fraction(anchor, s->range_start, s->range_end);
    long double fixed = cui_plot_lerp(a, b, t);
    long double da = a - fixed, db = b - fixed;
    long double offset_a = isfinite(da) ? da / factor : a / factor - fixed / factor;
    long double offset_b = isfinite(db) ? db / factor : b / factor - fixed / factor;
    return cui_plot_set_transformed(s, fixed + offset_a, fixed + offset_b);
}
static bool cui_plot_rect_valid(cui_plot_rect r) {
    return isfinite(r.x_min) && isfinite(r.y_min) && isfinite(r.x_max) && isfinite(r.y_max) &&
           r.x_min < r.x_max && r.y_min < r.y_max;
}
static unsigned cui_plot_outcode(cui_plot_point p, cui_plot_rect r) {
    return (p.x < r.x_min   ? 1u
            : p.x > r.x_max ? 2u
                            : 0u) |
           (p.y < r.y_min   ? 4u
            : p.y > r.y_max ? 8u
                            : 0u);
}
cui_plot_result cui_plot_clip_line(cui_plot_line line, cui_plot_rect r, cui_plot_line *out) {
    if (!out || !cui_plot_rect_valid(r) || !isfinite(line.start.x) || !isfinite(line.start.y) ||
        !isfinite(line.end.x) || !isfinite(line.end.y))
        return CUI_PLOT_INVALID;
    for (unsigned iteration = 0; iteration < 16; ++iteration) {
        unsigned a = cui_plot_outcode(line.start, r), b = cui_plot_outcode(line.end, r);
        if (!(a | b)) {
            *out = line;
            return CUI_PLOT_OK;
        }
        if (a & b)
            return CUI_PLOT_EMPTY;
        unsigned code = a ? a : b;
        cui_plot_point p;
        if (code & (4u | 8u)) {
            p.y = code & 8u ? r.y_max : r.y_min;
            long double t = cui_plot_fraction(p.y, line.start.y, line.end.y);
            cui_plot_result status =
                cui_plot_number(cui_plot_lerp(line.start.x, line.end.x, t), &p.x);
            if (status != CUI_PLOT_OK)
                return status;
        } else {
            p.x = code & 2u ? r.x_max : r.x_min;
            long double t = cui_plot_fraction(p.x, line.start.x, line.end.x);
            cui_plot_result status =
                cui_plot_number(cui_plot_lerp(line.start.y, line.end.y, t), &p.y);
            if (status != CUI_PLOT_OK)
                return status;
        }
        if (a)
            line.start = p;
        else
            line.end = p;
    }
    return CUI_PLOT_OVERFLOW;
}
cui_plot_result cui_plot_project_line(const cui_plot_scale *x, const cui_plot_scale *y,
                                      cui_plot_point a, cui_plot_point b, cui_plot_rect r,
                                      cui_plot_line *out) {
    cui_plot_line line;
    cui_plot_result status;
    if ((status = cui_plot_map(x, a.x, &line.start.x)) != CUI_PLOT_OK ||
        (status = cui_plot_map(y, a.y, &line.start.y)) != CUI_PLOT_OK ||
        (status = cui_plot_map(x, b.x, &line.end.x)) != CUI_PLOT_OK ||
        (status = cui_plot_map(y, b.y, &line.end.y)) != CUI_PLOT_OK)
        return status;
    return cui_plot_clip_line(line, r, out);
}
cui_plot_result cui_plot_project_bar(const cui_plot_scale *x, const cui_plot_scale *y,
                                     double center, double width, double value, double baseline,
                                     cui_plot_rect viewport, cui_plot_rect *out) {
    if (!out || !cui_plot_rect_valid(viewport) || !isfinite(width) || width <= 0)
        return CUI_PLOT_INVALID;
    double px, py, base;
    cui_plot_result status;
    if ((status = cui_plot_map(x, center, &px)) != CUI_PLOT_OK ||
        (status = cui_plot_map(y, value, &py)) != CUI_PLOT_OK ||
        (status = cui_plot_map(y, baseline, &base)) != CUI_PLOT_OK)
        return status;
    cui_plot_rect r = {fmax(viewport.x_min, px - width / 2), fmax(viewport.y_min, fmin(py, base)),
                       fmin(viewport.x_max, px + width / 2), fmin(viewport.y_max, fmax(py, base))};
    if (r.x_min >= r.x_max || r.y_min >= r.y_max)
        return CUI_PLOT_EMPTY;
    *out = r;
    return CUI_PLOT_OK;
}

static cui_plot_result cui_plot_categories(const uint64_t *keys, size_t count) {
    if (count > CUI_PLOT_CATEGORY_LIMIT)
        return CUI_PLOT_CAPACITY;
    return count && !keys ? CUI_PLOT_INVALID : CUI_PLOT_OK;
}
static cui_plot_result cui_plot_band_geometry_for(size_t count, cui_plot_band_options o,
                                                  cui_plot_band_geometry *out) {
    if (!isfinite(o.range_start) || !isfinite(o.range_end) || !isfinite(o.padding_inner) ||
        o.padding_inner < 0 || o.padding_inner > 1 || !isfinite(o.padding_outer) ||
        o.padding_outer < 0 ||
        (o.has_max_band_width && (!isfinite(o.max_band_width) || o.max_band_width < 0)))
        return CUI_PLOT_INVALID;
    size_t bands = count > o.band_count ? count : o.band_count;
    if (bands > CUI_PLOT_CATEGORY_LIMIT)
        return CUI_PLOT_CAPACITY;
    if (bands && o.padding_outer > (double)bands / 2)
        return CUI_PLOT_INVALID;
    long double low = fmin(o.range_start, o.range_end);
    long double high = fmax(o.range_start, o.range_end);
    long double span = high - low;
    long double average = bands ? span / bands : 0;
    long double width = average * (1 - o.padding_inner);
    if (o.has_max_band_width)
        width = fminl(width, o.max_band_width);
    long double step = bands <= 1 ? span
                                  : average * (1 - 2 * (long double)o.padding_outer / bands) *
                                        (1 + (long double)o.padding_inner / (bands - 1));
    long double start =
        bands == 1 ? cui_plot_lerp(low, high, .5L) - width / 2 : low + average * o.padding_outer;
    cui_plot_band_geometry result = {.band_count = bands};
    cui_plot_result status = cui_plot_number(start, &result.start);
    if (status == CUI_PLOT_OK)
        status = cui_plot_number(step, &result.step);
    if (status == CUI_PLOT_OK)
        status = cui_plot_number(width, &result.band_width);
    if (status == CUI_PLOT_OK)
        *out = result;
    return status;
}
cui_plot_result cui_plot_band_layout(const cui_plot_band_scale *s, cui_plot_band_geometry *out) {
    if (!s || !out || s->count > s->capacity)
        return CUI_PLOT_INVALID;
    cui_plot_result status = cui_plot_categories(s->domain, s->count);
    return status == CUI_PLOT_OK ? cui_plot_band_geometry_for(s->count, s->options, out) : status;
}
cui_plot_result cui_plot_band_init(cui_plot_band_scale *s, uint64_t *storage, size_t capacity,
                                   const uint64_t *domain, size_t count,
                                   cui_plot_band_options options, size_t *required) {
    if (!s || !required || (capacity && !storage))
        return CUI_PLOT_INVALID;
    cui_plot_result status = cui_plot_categories(domain, count);
    if (status != CUI_PLOT_OK)
        return status;
    uint64_t distinct[CUI_PLOT_CATEGORY_LIMIT];
    size_t unique = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t at = 0;
        while (at < unique && distinct[at] != domain[i])
            ++at;
        if (at == unique)
            distinct[unique++] = domain[i];
    }
    cui_plot_band_geometry geometry;
    status = cui_plot_band_geometry_for(unique, options, &geometry);
    if (status != CUI_PLOT_OK)
        return status;
    *required = unique;
    if (capacity < unique)
        return CUI_PLOT_CAPACITY;
    if (unique)
        memmove(storage, distinct, unique * sizeof *storage);
    *s = (cui_plot_band_scale){storage, unique, capacity, options};
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_band_at(const cui_plot_band_scale *s, size_t index, double *pixel) {
    if (!pixel)
        return CUI_PLOT_INVALID;
    cui_plot_band_geometry geometry;
    cui_plot_result status = cui_plot_band_layout(s, &geometry);
    if (status != CUI_PLOT_OK)
        return status;
    if (index >= s->count)
        return CUI_PLOT_EMPTY;
    return cui_plot_number((long double)geometry.start + index * (long double)geometry.step, pixel);
}
cui_plot_result cui_plot_band_map(const cui_plot_band_scale *s, uint64_t key, double *pixel) {
    if (!pixel)
        return CUI_PLOT_INVALID;
    cui_plot_band_geometry geometry;
    cui_plot_result status = cui_plot_band_layout(s, &geometry);
    if (status != CUI_PLOT_OK)
        return status;
    for (size_t i = 0; i < s->count; ++i)
        if (s->domain[i] == key)
            return cui_plot_band_at(s, i, pixel);
    return CUI_PLOT_EMPTY;
}
static size_t cui_plot_nearest_category(long double index, size_t count) {
    if (!count || index <= 0)
        return 0;
    if (!isfinite(index) || index >= (long double)(count - 1))
        return count - 1;
    return (size_t)floorl(index + .5L);
}
cui_plot_result cui_plot_band_nearest(const cui_plot_band_scale *s, double pixel, size_t *index) {
    if (!index || !isfinite(pixel))
        return CUI_PLOT_INVALID;
    cui_plot_band_geometry geometry;
    cui_plot_result status = cui_plot_band_layout(s, &geometry);
    if (status != CUI_PLOT_OK)
        return status;
    size_t result = 0;
    if (geometry.band_count > 1) {
        if (geometry.step == 0)
            result = pixel > geometry.start ? geometry.band_count - 1 : 0;
        else {
            long double normalized = ((long double)pixel - geometry.start) / geometry.step;
            result = cui_plot_nearest_category(normalized, geometry.band_count);
        }
    }
    *index = result;
    return CUI_PLOT_OK;
}
static cui_plot_result cui_plot_point_valid(const cui_plot_point_scale *s) {
    if (!s || !isfinite(s->range_start) || !isfinite(s->range_end))
        return CUI_PLOT_INVALID;
    return cui_plot_categories(s->domain, s->count);
}
cui_plot_result cui_plot_point_init(cui_plot_point_scale *s, const uint64_t *domain, size_t count,
                                    double start, double end) {
    if (!s)
        return CUI_PLOT_INVALID;
    cui_plot_point_scale candidate = {domain, count, start, end};
    cui_plot_result status = cui_plot_point_valid(&candidate);
    if (status == CUI_PLOT_OK)
        *s = candidate;
    return status;
}
cui_plot_result cui_plot_point_at(const cui_plot_point_scale *s, size_t index, double *pixel) {
    if (!pixel)
        return CUI_PLOT_INVALID;
    cui_plot_result status = cui_plot_point_valid(s);
    if (status != CUI_PLOT_OK)
        return status;
    if (index >= s->count)
        return CUI_PLOT_EMPTY;
    long double fraction = s->count == 1 ? .5L : (long double)index / (s->count - 1);
    return cui_plot_number(cui_plot_lerp(s->range_start, s->range_end, fraction), pixel);
}
cui_plot_result cui_plot_point_map(const cui_plot_point_scale *s, uint64_t key, double *pixel) {
    if (!pixel)
        return CUI_PLOT_INVALID;
    cui_plot_result status = cui_plot_point_valid(s);
    if (status != CUI_PLOT_OK)
        return status;
    for (size_t i = 0; i < s->count; ++i)
        if (s->domain[i] == key)
            return cui_plot_point_at(s, i, pixel);
    return CUI_PLOT_EMPTY;
}
cui_plot_result cui_plot_point_nearest(const cui_plot_point_scale *s, double pixel, size_t *index) {
    if (!index || !isfinite(pixel))
        return CUI_PLOT_INVALID;
    cui_plot_result status = cui_plot_point_valid(s);
    if (status != CUI_PLOT_OK)
        return status;
    size_t result = 0;
    if (s->count > 1 && s->range_start != s->range_end) {
        long double fraction = cui_plot_fraction(pixel, s->range_start, s->range_end);
        result = cui_plot_nearest_category(fraction * (s->count - 1), s->count);
    }
    *index = result;
    return CUI_PLOT_OK;
}
static cui_plot_result cui_plot_ordinal_valid(const cui_plot_ordinal_scale *s) {
    if (!s || !s->element_size || (s->range_count && !s->range) || (s->has_unknown && !s->unknown))
        return CUI_PLOT_INVALID;
    cui_plot_result status = cui_plot_categories(s->domain, s->count);
    if (status != CUI_PLOT_OK)
        return status;
    if (s->range_count > CUI_PLOT_CATEGORY_LIMIT || s->element_size > CUI_PLOT_ORDINAL_BYTE_LIMIT ||
        s->range_count > CUI_PLOT_ORDINAL_BYTE_LIMIT / s->element_size)
        return CUI_PLOT_CAPACITY;
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_ordinal_init(cui_plot_ordinal_scale *s, const uint64_t *domain,
                                      size_t count, const void *range, size_t range_count,
                                      size_t element_size, const void *unknown) {
    if (!s)
        return CUI_PLOT_INVALID;
    cui_plot_ordinal_scale candidate = {domain,      count,        range,          unknown,
                                        range_count, element_size, unknown != NULL};
    cui_plot_result status = cui_plot_ordinal_valid(&candidate);
    if (status == CUI_PLOT_OK)
        *s = candidate;
    return status;
}
cui_plot_result cui_plot_ordinal_map(const cui_plot_ordinal_scale *s, uint64_t key, void *output,
                                     size_t capacity) {
    cui_plot_result status = cui_plot_ordinal_valid(s);
    if (status != CUI_PLOT_OK)
        return status;
    size_t at = 0;
    while (at < s->count && s->domain[at] != key)
        ++at;
    const void *value = NULL;
    if (at < s->count) {
        if (!s->range_count)
            return CUI_PLOT_EMPTY;
        value = (const unsigned char *)s->range + (at % s->range_count) * s->element_size;
    } else if (s->has_unknown)
        value = s->unknown;
    else
        return CUI_PLOT_EMPTY;
    if (capacity < s->element_size)
        return CUI_PLOT_CAPACITY;
    if (!output)
        return CUI_PLOT_INVALID;
    memmove(output, value, s->element_size);
    return CUI_PLOT_OK;
}

static cui_plot_result cui_plot_path_emit(cui_plot_path_command *out, size_t *used,
                                          cui_plot_path_command command) {
    if (out)
        out[*used] = command;
    ++*used;
    return CUI_PLOT_OK;
}
static cui_plot_result cui_plot_path_run(const cui_plot_sample *samples, const size_t *indices,
                                         size_t count, cui_plot_curve curve, bool area,
                                         cui_plot_path_command *out, size_t *used) {
    cui_plot_path_command command = {0};
    command.kind = CUI_PLOT_PATH_MOVE;
    command.point = (cui_plot_point){samples[indices[0]].x, samples[indices[0]].y1};
    cui_plot_path_emit(out, used, command);
    for (size_t i = 0; i + 1 < count; ++i) {
        cui_plot_sample p1 = samples[indices[i]], p2 = samples[indices[i + 1]];
        command = (cui_plot_path_command){0};
        command.kind = CUI_PLOT_PATH_LINE;
        command.point = (cui_plot_point){p2.x, p2.y1};
        if (curve == CUI_PLOT_CURVE_NATURAL) {
            cui_plot_sample p0 = samples[indices[i ? i - 1 : 0]];
            cui_plot_sample p3 = samples[indices[i + 2 < count ? i + 2 : count - 1]];
            command.kind = CUI_PLOT_PATH_CUBIC;
            /* Divide operands before subtracting: portable even when long
             * double has the same range as double (e.g. wasm32). */
            cui_plot_result result =
                cui_plot_number((long double)p1.x + (p2.x / 6.L - p0.x / 6.L), &command.control1.x);
            if (result == CUI_PLOT_OK)
                result = cui_plot_number((long double)p1.y1 + (p2.y1 / 6.L - p0.y1 / 6.L),
                                         &command.control1.y);
            if (result == CUI_PLOT_OK)
                result = cui_plot_number((long double)p2.x - (p3.x / 6.L - p1.x / 6.L),
                                         &command.control2.x);
            if (result == CUI_PLOT_OK)
                result = cui_plot_number((long double)p2.y1 - (p3.y1 / 6.L - p1.y1 / 6.L),
                                         &command.control2.y);
            if (result != CUI_PLOT_OK)
                return result;
        } else if (curve == CUI_PLOT_CURVE_STEP_AFTER) {
            command.point.y = p1.y1;
            cui_plot_path_emit(out, used, command);
            if (i + 2 == count)
                continue;
            command.point.y = p2.y1;
        }
        cui_plot_path_emit(out, used, command);
    }
    if (area) {
        for (size_t i = count; i > 0; --i) {
            cui_plot_sample sample = samples[indices[i - 1]];
            command = (cui_plot_path_command){0};
            command.kind = CUI_PLOT_PATH_LINE;
            command.point = (cui_plot_point){sample.x, sample.y0};
            cui_plot_path_emit(out, used, command);
        }
        command = (cui_plot_path_command){0};
        command.kind = CUI_PLOT_PATH_CLOSE;
        cui_plot_path_emit(out, used, command);
    }
    return CUI_PLOT_OK;
}
static cui_plot_result cui_plot_path_pass(const cui_plot_sample *samples, size_t count,
                                          cui_plot_curve curve, cui_plot_gap_policy gaps, bool area,
                                          cui_plot_path_command *out, size_t *used) {
    size_t indices[CUI_PLOT_CATEGORY_LIMIT], run = 0;
    *used = 0;
    for (size_t i = 0; i <= count; ++i) {
        if (i < count && samples[i].defined) {
            indices[run++] = i;
        } else if (run && (i == count || gaps == CUI_PLOT_GAPS_SPLIT)) {
            cui_plot_result result =
                cui_plot_path_run(samples, indices, run, curve, area, out, used);
            if (result != CUI_PLOT_OK)
                return result;
            run = 0;
        }
    }
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_path(const cui_plot_sample *samples, size_t count, cui_plot_curve curve,
                              cui_plot_gap_policy gaps, bool area, cui_plot_path_command *out,
                              size_t capacity, size_t *required) {
    if (!required || count > CUI_PLOT_CATEGORY_LIMIT || (count && !samples) || (!out && capacity) ||
        curve < CUI_PLOT_CURVE_LINEAR || curve > CUI_PLOT_CURVE_STEP_AFTER ||
        gaps < CUI_PLOT_GAPS_SKIP || gaps > CUI_PLOT_GAPS_SPLIT)
        return CUI_PLOT_INVALID;
    for (size_t i = 0; i < count; ++i)
        if (samples[i].defined && (!isfinite(samples[i].x) || !isfinite(samples[i].y1) ||
                                   (area && !isfinite(samples[i].y0))))
            return CUI_PLOT_INVALID;
    size_t needed;
    cui_plot_result result = cui_plot_path_pass(samples, count, curve, gaps, area, NULL, &needed);
    if (result != CUI_PLOT_OK)
        return result;
    *required = needed;
    if (capacity < needed)
        return CUI_PLOT_CAPACITY;
    return cui_plot_path_pass(samples, count, curve, gaps, area, out, &needed);
}
cui_plot_result cui_plot_stack(const cui_plot_value *values, size_t series_count,
                               size_t point_count, cui_plot_stack_point *out, size_t capacity,
                               size_t *required) {
    if (!required || series_count > CUI_PLOT_CATEGORY_LIMIT ||
        point_count > CUI_PLOT_CATEGORY_LIMIT ||
        (point_count && series_count > CUI_PLOT_CATEGORY_LIMIT / point_count) || (!out && capacity))
        return CUI_PLOT_INVALID;
    size_t cells = series_count * point_count;
    if (cells && !values)
        return CUI_PLOT_INVALID;
    /* Preflight all columns before writing any output. */
    for (size_t j = 0; j < point_count; ++j) {
        double baseline = 0;
        for (size_t i = 0; i < series_count; ++i) {
            cui_plot_value value = values[i * point_count + j];
            if (value.defined && !isfinite(value.value))
                return CUI_PLOT_INVALID;
            if (cui_plot_number((long double)baseline + (value.defined ? value.value : 0),
                                &baseline) != CUI_PLOT_OK)
                return CUI_PLOT_OVERFLOW;
        }
    }
    *required = cells;
    if (capacity < cells)
        return CUI_PLOT_CAPACITY;
    for (size_t j = 0; j < point_count; ++j) {
        double baseline = 0;
        for (size_t i = 0; i < series_count; ++i) {
            size_t index = i * point_count + j;
            out[index].y0 = baseline;
            cui_plot_number((long double)baseline +
                                (values[index].defined ? values[index].value : 0),
                            &baseline);
            out[index].y1 = baseline;
        }
    }
    return CUI_PLOT_OK;
}

cui_plot_result cui_plot_pie(const cui_plot_value *values, size_t count, double start_angle,
                             double end_angle, double pad_angle, cui_plot_arc *out, size_t capacity,
                             size_t *required) {
    if (!required || count > CUI_PLOT_CATEGORY_LIMIT || (count && !values) || (!out && capacity) ||
        !isfinite(start_angle) || !isfinite(end_angle) || !isfinite(pad_angle) || pad_angle < 0)
        return CUI_PLOT_INVALID;
    double largest = 0;
    size_t needed = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!values[i].defined)
            continue;
        if (!isfinite(values[i].value))
            return CUI_PLOT_INVALID;
        if (values[i].value > 0) {
            ++needed;
            largest = fmax(largest, values[i].value);
        }
    }
    /* Normalize before summation so large finite values do not overflow on
     * targets whose long double has only double's exponent range. */
    long double total = 0;
    for (size_t i = 0; i < count; ++i)
        if (values[i].defined && values[i].value > 0)
            total += (long double)values[i].value / largest;
    *required = needed;
    if (capacity < needed)
        return CUI_PLOT_CAPACITY;
    long double cumulative = 0;
    double angle = start_angle;
    size_t emitted = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!values[i].defined || values[i].value <= 0)
            continue;
        cumulative += (long double)values[i].value / largest;
        double next = (double)cui_plot_lerp(start_angle, end_angle, cumulative / total);
        /* Force the final endpoint to avoid a visible seam from sum rounding. */
        if (emitted + 1 == needed)
            next = end_angle;
        out[emitted++] = (cui_plot_arc){i, values[i].value, angle, next, pad_angle};
        angle = next;
    }
    return CUI_PLOT_OK;
}
static bool cui_plot_arc_valid(const cui_plot_arc *arc, double inner, double outer) {
    return arc && isfinite(arc->start_angle) && isfinite(arc->end_angle) &&
           isfinite(arc->pad_angle) && arc->pad_angle >= 0 && isfinite(inner) && isfinite(outer) &&
           inner >= 0 && outer >= inner;
}
cui_plot_result cui_plot_arc_centroid(const cui_plot_arc *arc, double inner, double outer,
                                      cui_plot_point *out) {
    if (!out || !cui_plot_arc_valid(arc, inner, outer))
        return CUI_PLOT_INVALID;
    long double angle = cui_plot_lerp(arc->start_angle, arc->end_angle, .5L);
    long double radius = cui_plot_lerp(inner, outer, .5L);
    const long double half_pi = acosl(-1.L) / 2;
    cui_plot_point point;
    cui_plot_result result = cui_plot_number(radius * cosl(angle - half_pi), &point.x);
    if (result == CUI_PLOT_OK)
        result = cui_plot_number(radius * sinl(angle - half_pi), &point.y);
    if (result == CUI_PLOT_OK)
        *out = point;
    return result;
}
cui_plot_result cui_plot_arc_contains(const cui_plot_arc *arc, double inner, double outer,
                                      cui_plot_point position, bool *out) {
    if (!out || !cui_plot_arc_valid(arc, inner, outer) || !isfinite(position.x) ||
        !isfinite(position.y))
        return CUI_PLOT_INVALID;
    long double radius = hypotl(position.x, position.y);
    const long double pi = acosl(-1.L);
    long double angle = fmodl(atan2l(position.y, position.x) + pi / 2, 2 * pi);
    if (angle < 0)
        angle += 2 * pi;
    *out =
        radius >= inner && radius <= outer && angle >= arc->start_angle && angle < arc->end_angle;
    return CUI_PLOT_OK;
}

typedef struct {
    long double outer0, outer1, inner0, inner1;
    bool empty, full;
} cui_plot_arc_angles;
static cui_plot_result cui_plot_arc_angles_get(const cui_plot_arc *arc, double inner, double outer,
                                               cui_plot_arc_policy policy,
                                               cui_plot_arc_angles *angles) {
    if ((policy != CUI_PLOT_ARC_SEAMLESS && policy != CUI_PLOT_ARC_SOURCE_PADDING) ||
        !cui_plot_arc_valid(arc, inner, outer))
        return CUI_PLOT_INVALID;
    const long double pi = acosl(-1.L), tau = 2 * pi;
    long double span = (long double)arc->end_angle - arc->start_angle;
    if (span > tau + 1e-12L)
        return CUI_PLOT_INVALID;
    *angles = (cui_plot_arc_angles){0};
    if (outer == 0 || span <= 0 ||
        (policy == CUI_PLOT_ARC_SOURCE_PADDING && (outer < 1e-12 || span < 1e-12L))) {
        angles->empty = true;
        return CUI_PLOT_OK;
    }
    angles->full = policy == CUI_PLOT_ARC_SEAMLESS && fabsl(span - tau) <= 1e-12L;
    if (angles->full)
        span = tau;
    long double pad = angles->full ? 0 : arc->pad_angle;
    if (policy == CUI_PLOT_ARC_SOURCE_PADDING && span >= (double)pi)
        pad = .0001L;
    if (policy == CUI_PLOT_ARC_SOURCE_PADDING && inner <= 1e-12)
        inner = 0;
    long double inner_pad =
        inner > 0 && pad > 0 ? fminl((outer / (long double)inner) * pad, span * .8L) : pad;
    /* Reduce before adding offsets to preserve geometry for large angles. */
    long double start = fmodl(arc->start_angle, tau) - pi / 2;
    angles->outer0 = start + pad / 2;
    angles->outer1 = start + span - pad / 2;
    angles->inner0 = start + inner_pad / 2;
    angles->inner1 = start + span - inner_pad / 2;
    angles->empty = angles->outer1 <= angles->outer0;
    return CUI_PLOT_OK;
}
static cui_plot_point cui_plot_polar(double radius, long double angle) {
    return (cui_plot_point){(double)(radius * cosl(angle)), (double)(radius * sinl(angle))};
}
static void cui_plot_circular_command(cui_plot_path_command *commands, size_t *used, double radius,
                                      long double a0, long double a1, bool clockwise) {
    cui_plot_path_command command = {0};
    command.kind = CUI_PLOT_PATH_ARC;
    command.point = cui_plot_polar(radius, a1);
    command.radius = radius;
    command.large_arc = fabsl(a1 - a0) > acosl(-1.L);
    command.clockwise = clockwise;
    commands[(*used)++] = command;
}
cui_plot_result cui_plot_arc_path_ex(const cui_plot_arc *arc, double inner, double outer,
                                     cui_plot_arc_policy policy, cui_plot_path_command *out,
                                     size_t capacity, size_t *required) {
    if (!required || (!out && capacity))
        return CUI_PLOT_INVALID;
    cui_plot_arc_angles angles;
    cui_plot_result result = cui_plot_arc_angles_get(arc, inner, outer, policy, &angles);
    if (result != CUI_PLOT_OK)
        return result;
    if (policy == CUI_PLOT_ARC_SOURCE_PADDING && inner <= 1e-12)
        inner = 0;
    cui_plot_path_command commands[8] = {{0}};
    size_t used = 0;
    if (!angles.empty) {
        commands[used].kind = CUI_PLOT_PATH_MOVE;
        commands[used++].point = cui_plot_polar(outer, angles.outer0);
        if (angles.full) {
            long double middle = (angles.outer0 + angles.outer1) / 2;
            cui_plot_circular_command(commands, &used, outer, angles.outer0, middle, true);
            commands[used - 1].large_arc = false;
            cui_plot_circular_command(commands, &used, outer, middle, angles.outer1, true);
            commands[used - 1].large_arc = false;
        } else
            cui_plot_circular_command(commands, &used, outer, angles.outer0, angles.outer1, true);
        commands[used].kind = CUI_PLOT_PATH_LINE;
        commands[used++].point =
            inner > 0 ? cui_plot_polar(inner, angles.inner1) : (cui_plot_point){0, 0};
        if (inner > 0) {
            if (angles.full) {
                long double middle = (angles.inner0 + angles.inner1) / 2;
                cui_plot_circular_command(commands, &used, inner, angles.inner1, middle, false);
                commands[used - 1].large_arc = false;
                cui_plot_circular_command(commands, &used, inner, middle, angles.inner0, false);
                commands[used - 1].large_arc = false;
            } else
                cui_plot_circular_command(commands, &used, inner, angles.inner1, angles.inner0,
                                          false);
        }
        commands[used++].kind = CUI_PLOT_PATH_CLOSE;
    }
    *required = used;
    if (capacity < used)
        return CUI_PLOT_CAPACITY;
    if (used)
        memcpy(out, commands, used * sizeof *out);
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_arc_flatten_ex(const cui_plot_arc *arc, double inner, double outer,
                                        size_t segments, cui_plot_arc_policy policy,
                                        cui_plot_path_command *out, size_t capacity,
                                        size_t *required) {
    if (!required || (!out && capacity) || segments < 4 || segments > 1024)
        return CUI_PLOT_INVALID;
    cui_plot_arc_angles angles;
    cui_plot_result result = cui_plot_arc_angles_get(arc, inner, outer, policy, &angles);
    if (result != CUI_PLOT_OK)
        return result;
    if (policy == CUI_PLOT_ARC_SOURCE_PADDING && inner <= 1e-12)
        inner = 0;
    const long double tau = 2 * acosl(-1.L);
    size_t outer_steps =
        angles.empty ? 0 : (size_t)ceill((angles.outer1 - angles.outer0) * segments / tau);
    size_t inner_steps = angles.empty || inner == 0
                             ? 0
                             : (size_t)ceill((angles.inner1 - angles.inner0) * segments / tau);
    if (!angles.empty && !outer_steps)
        outer_steps = 1;
    if (!angles.empty && inner > 0 && !inner_steps)
        inner_steps = 1;
    size_t needed = angles.empty ? 0 : outer_steps + inner_steps + 3;
    *required = needed;
    if (capacity < needed)
        return CUI_PLOT_CAPACITY;
    if (angles.empty)
        return CUI_PLOT_OK;
    size_t used = 0;
    for (size_t i = 0; i <= outer_steps; ++i) {
        cui_plot_path_command command = {0};
        command.kind = i ? CUI_PLOT_PATH_LINE : CUI_PLOT_PATH_MOVE;
        command.point = cui_plot_polar(
            outer, cui_plot_lerp(angles.outer0, angles.outer1, (long double)i / outer_steps));
        out[used++] = command;
    }
    for (size_t i = 0; i <= inner_steps; ++i) {
        cui_plot_path_command command = {0};
        command.kind = CUI_PLOT_PATH_LINE;
        command.point = inner > 0
                            ? cui_plot_polar(inner, cui_plot_lerp(angles.inner1, angles.inner0,
                                                                  (long double)i / inner_steps))
                            : (cui_plot_point){0, 0};
        out[used++] = command;
    }
    out[used] = (cui_plot_path_command){0};
    out[used].kind = CUI_PLOT_PATH_CLOSE;
    return CUI_PLOT_OK;
}
static cui_plot_result cui_plot_radial_pass(const cui_plot_radial_sample *samples, size_t count,
                                            cui_plot_gap_policy gaps, bool closed, bool fill,
                                            bool area, cui_plot_path_command *out, size_t *used) {
    size_t indices[CUI_PLOT_CATEGORY_LIMIT], run = 0;
    *used = 0;
    for (size_t i = 0; i <= count; ++i) {
        if (i < count && samples[i].defined) {
            indices[run++] = i;
            continue;
        }
        if (!run || (i < count && gaps == CUI_PLOT_GAPS_SKIP))
            continue;
        if (!fill || area || run >= 3) {
            for (size_t j = 0; j < run; ++j) {
                cui_plot_radial_sample sample = samples[indices[j]];
                cui_plot_path_command command = {0};
                command.kind = j ? CUI_PLOT_PATH_LINE : CUI_PLOT_PATH_MOVE;
                command.point = cui_plot_polar(
                    sample.radius1, fmodl(sample.angle, 2 * acosl(-1.L)) - acosl(-1.L) / 2);
                cui_plot_path_emit(out, used, command);
            }
            if (area)
                for (size_t j = run; j > 0; --j) {
                    cui_plot_radial_sample sample = samples[indices[j - 1]];
                    cui_plot_path_command command = {0};
                    command.kind = CUI_PLOT_PATH_LINE;
                    command.point = cui_plot_polar(
                        sample.radius0, fmodl(sample.angle, 2 * acosl(-1.L)) - acosl(-1.L) / 2);
                    cui_plot_path_emit(out, used, command);
                }
            if (area || ((closed || fill) && run >= 3)) {
                cui_plot_path_command command = {0};
                command.kind = CUI_PLOT_PATH_CLOSE;
                cui_plot_path_emit(out, used, command);
            }
        }
        run = 0;
    }
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_radial_path(const cui_plot_radial_sample *samples, size_t count,
                                     cui_plot_gap_policy gaps, bool closed, bool fill, bool area,
                                     cui_plot_path_command *out, size_t capacity,
                                     size_t *required) {
    if (!required || (!out && capacity) || count > CUI_PLOT_CATEGORY_LIMIT || (count && !samples) ||
        gaps < CUI_PLOT_GAPS_SKIP || gaps > CUI_PLOT_GAPS_SPLIT)
        return CUI_PLOT_INVALID;
    for (size_t i = 0; i < count; ++i)
        if (samples[i].defined &&
            (!isfinite(samples[i].angle) || !isfinite(samples[i].radius1) ||
             samples[i].radius1 < 0 ||
             (area && (!isfinite(samples[i].radius0) || samples[i].radius0 < 0))))
            return CUI_PLOT_INVALID;
    size_t needed;
    cui_plot_radial_pass(samples, count, gaps, closed, fill, area, NULL, &needed);
    *required = needed;
    if (capacity < needed)
        return CUI_PLOT_CAPACITY;
    return cui_plot_radial_pass(samples, count, gaps, closed, fill, area, out, &needed);
}

cui_plot_result cui_plot_bar_layout(cui_plot_bar_alignment alignment, double cross, double width,
                                    double base, double value, cui_plot_bar_geometry *out) {
    if (!out || alignment < CUI_PLOT_BAR_BOTTOM || alignment > CUI_PLOT_BAR_RIGHT ||
        !isfinite(cross) || !isfinite(width) || width < 0 || !isfinite(base) || !isfinite(value))
        return CUI_PLOT_INVALID;
    cui_plot_bar_geometry geometry = {0};
    double band_end, center;
    cui_plot_result result = cui_plot_number((long double)cross + width, &band_end);
    if (result == CUI_PLOT_OK)
        result = cui_plot_number((long double)cross + width / 2, &center);
    if (result != CUI_PLOT_OK)
        return result;
    double low = fmin(base, value), high = fmax(base, value);
    if (alignment == CUI_PLOT_BAR_BOTTOM || alignment == CUI_PLOT_BAR_TOP) {
        geometry.frame = (cui_plot_rect){cross, low, band_end, high};
        geometry.label_origin.x = center;
        bool below = alignment == CUI_PLOT_BAR_BOTTOM ? value > base : value >= base;
        result = cui_plot_number((long double)value + (below ? 2 : -12), &geometry.label_origin.y);
        geometry.gradient_angle = alignment == CUI_PLOT_BAR_BOTTOM ? 0 : 180;
    } else {
        geometry.frame = (cui_plot_rect){low, cross, high, band_end};
        bool right = alignment == CUI_PLOT_BAR_LEFT ? value >= base : value > base;
        result = cui_plot_number((long double)value + (right ? 2 : -2), &geometry.label_origin.x);
        if (result == CUI_PLOT_OK)
            result = cui_plot_number((long double)center - 5, &geometry.label_origin.y);
        geometry.gradient_angle = alignment == CUI_PLOT_BAR_LEFT ? 90 : 270;
    }
    if (result == CUI_PLOT_OK)
        *out = geometry;
    return result;
}
static bool cui_plot_grid_valid(const cui_plot_grid_options *o) {
    if (!o || !isfinite(o->width) || !isfinite(o->height) || o->width <= 0 || o->height <= 0 ||
        o->width > 16384 || o->height > 16384 || o->x_count > 256 || o->y_count > 256 ||
        o->dash_count > 64 || (o->x_count && !o->x) || (o->y_count && !o->y) ||
        (o->dash_count && !o->dashes))
        return false;
    for (size_t i = 0; i < o->x_count; ++i)
        if (!isfinite(o->x[i]) || fabs(o->x[i]) > 16384)
            return false;
    for (size_t i = 0; i < o->y_count; ++i)
        if (!isfinite(o->y[i]) || fabs(o->y[i]) > 16384)
            return false;
    for (size_t i = 0; i < o->dash_count; ++i)
        if (!isfinite(o->dashes[i]))
            return false;
    return true;
}
static cui_plot_result cui_plot_grid_quad(cui_plot_rect *out, size_t *used, bool vertical,
                                          double cross, double start, double end) {
    if (*used == 4096) {
        ++*used;
        return CUI_PLOT_CAPACITY;
    }
    if (out)
        out[*used] = vertical ? (cui_plot_rect){cross - .5, start, cross + .5, end}
                              : (cui_plot_rect){start, cross - .5, end, cross + .5};
    ++*used;
    return CUI_PLOT_OK;
}
static cui_plot_result cui_plot_grid_pass(const cui_plot_grid_options *o, cui_plot_rect *out,
                                          size_t *used) {
    *used = 0;
    bool positive = false, painted = false;
    size_t pattern = o->dash_count * (o->dash_count % 2 ? 2 : 1);
    for (size_t i = 0; i < pattern; ++i)
        if (o->dashes[i % o->dash_count] > 0) {
            positive = true;
            if (i % 2 == 0)
                painted = true;
        }
    for (size_t axis = 0; axis < 2; ++axis) {
        size_t count = axis ? o->y_count : o->x_count;
        const double *positions = axis ? o->y : o->x;
        double length = axis ? o->width : o->height;
        for (size_t i = 0; i < count; ++i) {
            if (!positive) {
                cui_plot_result result =
                    cui_plot_grid_quad(out, used, !axis, positions[i], 0, length);
                if (result != CUI_PLOT_OK)
                    return result;
                continue;
            }
            if (!painted)
                continue;
            double position = 0;
            size_t index = 0;
            while (position < length) {
                double dash = fmax(0, o->dashes[index % o->dash_count]);
                double next = (double)fminl((long double)position + dash, length);
                if (dash > 0 && next <= position)
                    return CUI_PLOT_DEGENERATE;
                if (index % 2 == 0 && next > position) {
                    cui_plot_result result =
                        cui_plot_grid_quad(out, used, !axis, positions[i], position, next);
                    if (result != CUI_PLOT_OK)
                        return result;
                }
                position = next;
                index = (index + 1) % pattern;
            }
        }
    }
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_grid_layout(const cui_plot_grid_options *o, cui_plot_rect *out,
                                     size_t capacity, size_t *required) {
    if (!required || (!out && capacity) || !cui_plot_grid_valid(o))
        return CUI_PLOT_INVALID;
    size_t used;
    cui_plot_result result = cui_plot_grid_pass(o, NULL, &used);
    if (result != CUI_PLOT_OK && result != CUI_PLOT_CAPACITY)
        return result;
    *required = used;
    if (result == CUI_PLOT_CAPACITY || capacity < used)
        return CUI_PLOT_CAPACITY;
    return cui_plot_grid_pass(o, out, &used);
}

cui_plot_result cui_plot_area_paths(const cui_plot_area_sample *samples, size_t count,
                                    cui_plot_curve curve, bool has_baseline, double baseline,
                                    cui_plot_area_buffers *buffers) {
    if (!buffers || count > CUI_PLOT_CATEGORY_LIMIT || (count && !samples) ||
        (!buffers->fill && buffers->fill_capacity) ||
        (!buffers->stroke && buffers->stroke_capacity) || curve < CUI_PLOT_CURVE_LINEAR ||
        curve > CUI_PLOT_CURVE_STEP_AFTER || (has_baseline && !isfinite(baseline)))
        return CUI_PLOT_INVALID;
    cui_plot_sample points[CUI_PLOT_CATEGORY_LIMIT];
    size_t indices[CUI_PLOT_CATEGORY_LIMIT], valid = 0;
    for (size_t i = 0; i < count; ++i) {
        if ((samples[i].has_x && !isfinite(samples[i].x)) ||
            (samples[i].has_x && samples[i].has_y && !isfinite(samples[i].y)))
            return CUI_PLOT_INVALID;
        points[i] =
            (cui_plot_sample){samples[i].x, 0, samples[i].y, samples[i].has_x && samples[i].has_y};
        if (points[i].defined)
            indices[valid++] = i;
    }
    size_t stroke_count = 0;
    if (valid) {
        cui_plot_result result =
            cui_plot_path_run(points, indices, valid, curve, false, NULL, &stroke_count);
        if (result != CUI_PLOT_OK)
            return result;
    }
    bool close = valid > 1 && has_baseline && samples[0].has_x && samples[count - 1].has_x;
    size_t fill_count = stroke_count + (close ? 3 : 0);
    buffers->fill_count = fill_count;
    buffers->stroke_count = stroke_count;
    if (buffers->fill_capacity < fill_count || buffers->stroke_capacity < stroke_count)
        return CUI_PLOT_CAPACITY;
    if (valid) {
        size_t used = 0;
        (void)cui_plot_path_run(points, indices, valid, curve, false, buffers->stroke, &used);
        memcpy(buffers->fill, buffers->stroke, stroke_count * sizeof *buffers->fill);
        if (close) {
            buffers->fill[used++] = (cui_plot_path_command){
                .kind = CUI_PLOT_PATH_LINE, .point = {samples[count - 1].x, baseline}};
            buffers->fill[used++] = (cui_plot_path_command){.kind = CUI_PLOT_PATH_LINE,
                                                            .point = {samples[0].x, baseline}};
            buffers->fill[used] = (cui_plot_path_command){.kind = CUI_PLOT_PATH_CLOSE};
        }
    }
    return CUI_PLOT_OK;
}

cui_plot_result cui_plot_dot_layout(cui_plot_point center, double size,
                                    cui_plot_dot_geometry *out) {
    if (!out || !isfinite(center.x) || !isfinite(center.y) || !isfinite(size) || size < 0)
        return CUI_PLOT_INVALID;
    cui_plot_dot_geometry geometry = {.radius = size / 2, .border_width = 1};
    cui_plot_result result =
        cui_plot_number((long double)center.x - size / 2, &geometry.frame.x_min);
    if (result == CUI_PLOT_OK)
        result = cui_plot_number((long double)center.y - size / 2, &geometry.frame.y_min);
    if (result == CUI_PLOT_OK)
        result = cui_plot_number((long double)center.x + size / 2, &geometry.frame.x_max);
    if (result == CUI_PLOT_OK)
        result = cui_plot_number((long double)center.y + size / 2, &geometry.frame.y_max);
    if (result == CUI_PLOT_OK)
        *out = geometry;
    return result;
}

cui_plot_result cui_plot_hover_track(cui_plot_hover_memory *memory,
                                     const cui_plot_tooltip_state *live,
                                     const cui_plot_point *cursor, double progress) {
    if (!memory || !isfinite(progress) || progress < 0 || progress > 1)
        return CUI_PLOT_INVALID;
    if (live) {
        if (!cursor || !isfinite(cursor->x) || !isfinite(cursor->y) ||
            !isfinite(live->cross_line.x) || !isfinite(live->cross_line.y) ||
            (live->dot_count && !live->dots) || live->dot_count > 4096)
            return CUI_PLOT_INVALID;
        for (size_t i = 0; i < live->dot_count; ++i)
            if (!isfinite(live->dots[i].x) || !isfinite(live->dots[i].y))
                return CUI_PLOT_INVALID;
    }
    cui_plot_hover_memory next = *memory;
    next.hovered = live != NULL;
    next.progress = progress;
    next.entering = next.hovered && progress == 0;
    if (live) {
        next.state = *live;
        next.cursor = *cursor;
        next.has_state = true;
    } else if (progress == 0) {
        next.state = (cui_plot_tooltip_state){0};
        next.has_state = false;
    }
    *memory = next;
    return CUI_PLOT_OK;
}
bool cui_plot_appear_complete(cui_plot_appear_scope *scope, uint64_t identity,
                              uint64_t generation) {
    if (!scope)
        return false;
    if (scope->identity != identity)
        *scope = (cui_plot_appear_scope){.identity = identity};
    return scope->has_completed && scope->completed_generation == generation;
}
cui_plot_result cui_plot_appear_record(cui_plot_appear_scope *scope, uint64_t identity,
                                       uint64_t generation, double progress) {
    if (!scope || !isfinite(progress) || progress < 0 || progress > 1)
        return CUI_PLOT_INVALID;
    (void)cui_plot_appear_complete(scope, identity, generation);
    if (progress == 1) {
        scope->completed_generation = generation;
        scope->has_completed = true;
    }
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_appear_stagger(double progress, size_t index, size_t count, double spread,
                                        double *out) {
    if (!out || !isfinite(progress) || progress < 0 || progress > 1 || !isfinite(spread))
        return CUI_PLOT_INVALID;
    double local = progress;
    if (count > 1 && progress < 1) {
        spread = fmax(0, fmin(.95, spread));
        if (index >= count)
            index = count - 1;
        double start = spread * ((double)index / (double)(count - 1));
        local = fmax(0, fmin(1, (progress - start) / (1 - spread)));
    }
    *out = local;
    return CUI_PLOT_OK;
}

cui_plot_result cui_plot_linear_extent_init(cui_plot_linear_extent *out, const double *domain,
                                            size_t count, double start, double end) {
    if (!out || (count && !domain) || count > 4096 || !isfinite(start) || !isfinite(end))
        return CUI_PLOT_INVALID;
    cui_plot_linear_extent next = {.range_start = start, .range_end = end};
    for (size_t i = 0; i < count; ++i) {
        if (!isfinite(domain[i]))
            return CUI_PLOT_INVALID;
        if (i == 0 || domain[i] < next.domain_min)
            next.domain_min = domain[i];
        if (i == 0 || domain[i] > next.domain_max)
            next.domain_max = domain[i];
    }
    *out = next;
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_linear_extent_tick(const cui_plot_linear_extent *s, double value,
                                            double *pixel) {
    if (!s || !pixel || !isfinite(value) || !isfinite(s->domain_min) || !isfinite(s->domain_max) ||
        !isfinite(s->range_start) || !isfinite(s->range_end) || s->domain_min > s->domain_max)
        return CUI_PLOT_INVALID;
    if (s->domain_min == s->domain_max)
        return CUI_PLOT_EMPTY;
    if (s->range_start == s->range_end) {
        *pixel = s->range_start;
        return CUI_PLOT_OK;
    }
    return cui_plot_number(cui_plot_lerp(s->range_start, s->range_end,
                                         cui_plot_fraction(value, s->domain_min, s->domain_max)),
                           pixel);
}

cui_plot_result cui_plot_bar_path(cui_plot_bar_alignment alignment, double cross, double width,
                                  double baseline, double value, const double radii[4],
                                  cui_plot_path_command *out, size_t capacity, size_t *required) {
    if (!required || !radii || (!out && capacity))
        return CUI_PLOT_INVALID;
    cui_plot_bar_geometry g;
    cui_plot_result result = cui_plot_bar_layout(alignment, cross, width, baseline, value, &g);
    if (result != CUI_PLOT_OK)
        return result;
    double r[4];
    double limit =
        fmin(g.frame.x_max / 2 - g.frame.x_min / 2, g.frame.y_max / 2 - g.frame.y_min / 2);
    for (size_t i = 0; i < 4; ++i) {
        if (!isfinite(radii[i]) || radii[i] < 0)
            return CUI_PLOT_INVALID;
        r[i] = fmin(radii[i], limit);
    }
    if (g.frame.x_min == g.frame.x_max || g.frame.y_min == g.frame.y_max) {
        *required = 0;
        return CUI_PLOT_EMPTY;
    }
    *required = 10;
    if (capacity < 10)
        return CUI_PLOT_CAPACITY;
    const double k = .5522847498307933984;
    double l = g.frame.x_min, t = g.frame.y_min, h = g.frame.x_max, b = g.frame.y_max;
    cui_plot_path_command path[10] = {{.kind = CUI_PLOT_PATH_MOVE, .point = {l + r[0], t}},
                                      {.kind = CUI_PLOT_PATH_LINE, .point = {h - r[1], t}},
                                      {.kind = CUI_PLOT_PATH_CUBIC,
                                       .point = {h, t + r[1]},
                                       .control1 = {h - r[1] + k * r[1], t},
                                       .control2 = {h, t + r[1] - k * r[1]}},
                                      {.kind = CUI_PLOT_PATH_LINE, .point = {h, b - r[2]}},
                                      {.kind = CUI_PLOT_PATH_CUBIC,
                                       .point = {h - r[2], b},
                                       .control1 = {h, b - r[2] + k * r[2]},
                                       .control2 = {h - r[2] + k * r[2], b}},
                                      {.kind = CUI_PLOT_PATH_LINE, .point = {l + r[3], b}},
                                      {.kind = CUI_PLOT_PATH_CUBIC,
                                       .point = {l, b - r[3]},
                                       .control1 = {l + r[3] - k * r[3], b},
                                       .control2 = {l, b - r[3] + k * r[3]}},
                                      {.kind = CUI_PLOT_PATH_LINE, .point = {l, t + r[0]}},
                                      {.kind = CUI_PLOT_PATH_CUBIC,
                                       .point = {l + r[0], t},
                                       .control1 = {l, t + r[0] - k * r[0]},
                                       .control2 = {l + r[0] - k * r[0], t}},
                                      {.kind = CUI_PLOT_PATH_CLOSE}};
    memcpy(out, path, sizeof path);
    return CUI_PLOT_OK;
}

cui_plot_result cui_plot_arc_path(const cui_plot_arc *arc, double inner, double outer,
                                  cui_plot_path_command *out, size_t capacity, size_t *required) {
    return cui_plot_arc_path_ex(arc, inner, outer, CUI_PLOT_ARC_SEAMLESS, out, capacity, required);
}
cui_plot_result cui_plot_arc_flatten(const cui_plot_arc *arc, double inner, double outer,
                                     size_t segments, cui_plot_path_command *out, size_t capacity,
                                     size_t *required) {
    return cui_plot_arc_flatten_ex(arc, inner, outer, segments, CUI_PLOT_ARC_SEAMLESS, out,
                                   capacity, required);
}

static bool cui_plot_cursor_valid(cui_plot_rect b, cui_plot_point p) {
    return isfinite(b.x_min) && isfinite(b.x_max) && isfinite(b.y_min) && isfinite(b.y_max) &&
           b.x_min < b.x_max && b.y_min < b.y_max && isfinite(p.x) && isfinite(p.y);
}
static bool cui_plot_cursor_inside(cui_plot_rect b, cui_plot_point p) {
    return p.x >= b.x_min && p.x < b.x_max && p.y >= b.y_min && p.y < b.y_max;
}
static cui_plot_result cui_plot_cursor_relative(cui_plot_rect b, cui_plot_point p,
                                                cui_plot_point *relative) {
    cui_plot_point next;
    cui_plot_result result = cui_plot_number((long double)p.x - b.x_min, &next.x);
    if (result != CUI_PLOT_OK)
        return result;
    result = cui_plot_number((long double)p.y - b.y_min, &next.y);
    if (result != CUI_PLOT_OK)
        return result;
    *relative = next;
    return CUI_PLOT_OK;
}
static bool cui_plot_cursor_changed(cui_plot_cursor_memory a, cui_plot_cursor_memory b) {
    return a.visible != b.visible ||
           (b.visible && (a.relative.x != b.relative.x || a.relative.y != b.relative.y));
}
cui_plot_result cui_plot_cursor_mouse(cui_plot_cursor_memory *memory, cui_plot_rect bounds,
                                      cui_plot_point global, bool hovered,
                                      cui_plot_cursor_update *update) {
    if (!memory || !update || !cui_plot_cursor_valid(bounds, global))
        return CUI_PLOT_INVALID;
    cui_plot_cursor_memory next = *memory;
    next.visible = hovered && cui_plot_cursor_inside(bounds, global);
    if (next.visible) {
        cui_plot_result result = cui_plot_cursor_relative(bounds, global, &next.relative);
        if (result != CUI_PLOT_OK)
            return result;
    }
    *update = (cui_plot_cursor_update){.changed = cui_plot_cursor_changed(*memory, next),
                                       .request_frame = memory->visible != next.visible};
    *memory = next;
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_cursor_long_press(cui_plot_cursor_memory *memory, cui_plot_rect bounds,
                                           cui_plot_point global, cui_plot_long_press_phase phase,
                                           bool hovered, bool prevented,
                                           cui_plot_cursor_update *update) {
    if (!memory || !update || !cui_plot_cursor_valid(bounds, global) ||
        phase < CUI_PLOT_LONG_PRESS_STARTED || phase > CUI_PLOT_LONG_PRESS_CANCELLED)
        return CUI_PLOT_INVALID;
    cui_plot_cursor_memory next = *memory;
    bool accepted = false;
    if (phase == CUI_PLOT_LONG_PRESS_STARTED) {
        if (!prevented && hovered && cui_plot_cursor_inside(bounds, global)) {
            next.visible = true;
            accepted = true;
        }
    } else if (phase == CUI_PLOT_LONG_PRESS_ENDED || phase == CUI_PLOT_LONG_PRESS_CANCELLED)
        next.visible = false;
    if ((phase == CUI_PLOT_LONG_PRESS_MOVED && memory->visible) || accepted) {
        cui_plot_result result = cui_plot_cursor_relative(bounds, global, &next.relative);
        if (result != CUI_PLOT_OK)
            return result;
    }
    bool changed = cui_plot_cursor_changed(*memory, next);
    *update = (cui_plot_cursor_update){changed, changed, accepted};
    *memory = next;
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_cursor_resolve(const cui_plot_cursor_memory *memory, cui_plot_rect bounds,
                                        cui_plot_point global, bool appearing,
                                        cui_plot_point *relative, bool *resolved) {
    if (!memory || !relative || !resolved || !cui_plot_cursor_valid(bounds, global))
        return CUI_PLOT_INVALID;
    bool visible = memory->visible && !appearing && cui_plot_cursor_inside(bounds, global);
    if (visible) {
        cui_plot_point next;
        cui_plot_result result = cui_plot_cursor_relative(bounds, global, &next);
        if (result != CUI_PLOT_OK)
            return result;
        *relative = next;
    }
    *resolved = visible;
    return CUI_PLOT_OK;
}

cui_plot_result cui_plot_sankey_link_path(const cui_plot_sankey_ribbon *link, double min_width,
                                          cui_plot_point origin, cui_plot_path_command *out,
                                          size_t capacity, size_t *required) {
    if (!link || !required || (!out && capacity) || !isfinite(min_width) || min_width < 0 ||
        !isfinite(origin.x) || !isfinite(origin.y) || !isfinite(link->source_x) ||
        !isfinite(link->target_x) || !isfinite(link->source_y) || !isfinite(link->target_y) ||
        !isfinite(link->source_width) || !isfinite(link->target_width) || link->source_width < 0 ||
        link->target_width < 0)
        return CUI_PLOT_INVALID;
    long double sx = link->source_x, tx = link->target_x, mx = sx / 2 + tx / 2;
    long double sh = fmax(link->source_width, min_width) / 2;
    long double th = fmax(link->target_width, min_width) / 2;
    long double xs[] = {sx, tx, mx};
    long double ys[] = {link->source_y - sh, link->target_y - th, link->source_y + sh,
                        link->target_y + th};
    double x[3], y[4];
    for (size_t i = 0; i < 3; ++i) {
        cui_plot_result result = cui_plot_number(xs[i] + origin.x, x + i);
        if (result != CUI_PLOT_OK)
            return result;
    }
    for (size_t i = 0; i < 4; ++i) {
        cui_plot_result result = cui_plot_number(ys[i] + origin.y, y + i);
        if (result != CUI_PLOT_OK)
            return result;
    }
    cui_plot_path_command path[5] = {{.kind = CUI_PLOT_PATH_MOVE, .point = {x[0], y[0]}},
                                     {.kind = CUI_PLOT_PATH_CUBIC,
                                      .point = {x[1], y[1]},
                                      .control1 = {x[2], y[0]},
                                      .control2 = {x[2], y[1]}},
                                     {.kind = CUI_PLOT_PATH_LINE, .point = {x[1], y[3]}},
                                     {.kind = CUI_PLOT_PATH_CUBIC,
                                      .point = {x[0], y[2]},
                                      .control1 = {x[2], y[3]},
                                      .control2 = {x[2], y[2]}},
                                     {.kind = CUI_PLOT_PATH_CLOSE}};
    *required = 5;
    if (capacity < 5)
        return CUI_PLOT_CAPACITY;
    memcpy(out, path, sizeof path);
    return CUI_PLOT_OK;
}

/* Shared Base extraction of the original CUI Chart Sankey solver, audited
 * against frozen Plot Sankey. Chart adapter migration is coordinated separately.
 * Source defaults/zero policies/scaled values/topology reuse are explicit here. */
typedef struct {
    size_t count, link_count;
    const cui_plot_sankey_link_layout *links;
    double width, height, node_width, node_padding;
    struct {
        double left, right, top, bottom;
    } sankey_margins;
    cui_plot_sankey_align sankey_align;
    cui_plot_sankey_value_scale sankey_value_scale;
    unsigned sankey_iterations;
    bool legacy_value_alignment;
} plot_sankey_work;
static double plot_sankey_scaled(const plot_sankey_work *o, double value) {
    (void)o;
    return value; /* graph values were scaled during topology */
}
static void plot_sankey_relax(const plot_sankey_work *o, cui_plot_sankey_node *nodes, size_t layers,
                              double padding, double ky);
static void plot_sankey_center_and_stagger(const plot_sankey_work *o, cui_plot_sankey_node *nodes,
                                           size_t layers);
static cui_plot_result plot_sankey_prepare(const plot_sankey_work *o, cui_plot_sankey_node *out,
                                           bool topology_only, bool reuse) {
    cui_plot_sankey_node nodes[256] = {{0}};
    if (reuse)
        memcpy(nodes, out, o->count * sizeof(*out));
    size_t indegree[256] = {0}, order[256] = {0}, order_count = 0;
    long double incoming[256] = {0}, outgoing[256] = {0};
    long double scaled_in[256] = {0}, scaled_out[256] = {0};
    for (size_t i = 0; i < o->count; ++i) {
        nodes[i].index = i;
        nodes[i].source_count = 0;
        nodes[i].target_count = 0;
    }
    for (size_t i = 0; i < o->link_count; i++) {
        const cui_plot_sankey_link_layout *link = &o->links[i];
        if (link->source >= o->count || link->target >= o->count || link->source == link->target ||
            !isfinite(link->value) || link->value < 0)
            return CUI_PLOT_INVALID;
        indegree[link->target]++;
        nodes[link->source].source_count++;
        nodes[link->target].target_count++;
        outgoing[link->source] += link->value;
        incoming[link->target] += link->value;
        double scaled = plot_sankey_scaled(o, link->value);
        scaled_out[link->source] += scaled;
        scaled_in[link->target] += scaled;
    }
    for (size_t i = 0; i < o->count; i++) {
        if (!isfinite(incoming[i]) || !isfinite(outgoing[i]) || incoming[i] > DBL_MAX ||
            outgoing[i] > DBL_MAX)
            return CUI_PLOT_OVERFLOW;
        nodes[i].value = (double)fmaxl(incoming[i], outgoing[i]);
        if (!indegree[i])
            order[order_count++] = i;
    }
    if (!reuse) {
        for (size_t pos = 0; pos < order_count; pos++) {
            size_t source = order[pos];
            for (size_t i = 0; i < o->link_count; i++)
                if (o->links[i].source == source) {
                    size_t target = o->links[i].target;
                    if (nodes[target].depth < nodes[source].depth + 1)
                        nodes[target].depth = nodes[source].depth + 1;
                    if (!--indegree[target])
                        order[order_count++] = target;
                }
        }
        if (order_count != o->count)
            return CUI_PLOT_INVALID; /* cyclic */
        for (size_t pos = order_count; pos > 0; pos--) {
            size_t source = order[pos - 1];
            for (size_t i = 0; i < o->link_count; i++)
                if (o->links[i].source == source) {
                    size_t target = o->links[i].target;
                    if (nodes[source].height < nodes[target].height + 1)
                        nodes[source].height = nodes[target].height + 1;
                }
        }
    }
    size_t layers = 1;
    for (size_t i = 0; i < o->count; i++)
        if (layers < nodes[i].depth + 1)
            layers = nodes[i].depth + 1;
    size_t counts[256] = {0};
    long double sums[256] = {0};
    for (size_t i = 0; i < o->count; i++) {
        size_t layer = nodes[i].depth;
        if (o->sankey_align == CUI_PLOT_SANKEY_RIGHT)
            layer = layers - 1 - nodes[i].height;
        else if (o->sankey_align == CUI_PLOT_SANKEY_JUSTIFY &&
                 (o->legacy_value_alignment ? !outgoing[i] : !nodes[i].source_count))
            layer = layers - 1;
        else if (o->sankey_align == CUI_PLOT_SANKEY_CENTER &&
                 (o->legacy_value_alignment ? (!incoming[i] && outgoing[i])
                                            : (!nodes[i].target_count && nodes[i].source_count))) {
            size_t nearest = layers;
            for (size_t j = 0; j < o->link_count; j++)
                if (o->links[j].source == i && nodes[o->links[j].target].depth < nearest)
                    nearest = nodes[o->links[j].target].depth;
            layer = nearest ? nearest - 1 : 0;
        }
        nodes[i].layer = layer;
        counts[layer]++;
        sums[layer] += fmaxl(scaled_in[i], scaled_out[i]);
    }
    double width = o->node_width;
    double padding = o->node_padding;
    double plot_left = o->sankey_margins.left;
    double plot_right = o->width - o->sankey_margins.right;
    double plot_top = o->sankey_margins.top;
    double plot_bottom = o->height - o->sankey_margins.bottom;
    double plot_height = plot_bottom - plot_top;
    for (size_t i = 0; i < o->count; ++i) {
        nodes[i].x0 =
            plot_left +
            (layers > 1 ? (plot_right - plot_left - width) * nodes[i].layer / (layers - 1) : 0);
        nodes[i].x1 = nodes[i].x0 + width;
    }
    if (topology_only) {
        memcpy(out, nodes, o->count * sizeof(*out));
        return CUI_PLOT_OK;
    }
    size_t max_count = 1;
    for (size_t l = 0; l < layers; l++)
        if (counts[l] > max_count)
            max_count = counts[l];
    padding = fmin(padding, plot_height / (max_count > 1 ? max_count - 1 : 1));
    long double ky = LDBL_MAX;
    for (size_t l = 0; l < layers; l++)
        if (sums[l] > 0) {
            long double available = plot_height - padding * (counts[l] - 1);
            long double candidate = available / sums[l];
            if (candidate < ky)
                ky = candidate;
        }
    if (ky == LDBL_MAX)
        ky = 0;
    if (!isfinite(ky) || ky < 0)
        return CUI_PLOT_OVERFLOW;
    for (size_t l = 0; l < layers; l++) {
        long double y = plot_top;
        if (!isfinite(y) || fabsl(y) > 32768)
            return CUI_PLOT_OVERFLOW;
        for (size_t i = 0; i < o->count; i++)
            if (nodes[i].layer == l) {
                double x = plot_left +
                           (layers > 1 ? (plot_right - plot_left - width) * l / (layers - 1) : 0);
                nodes[i].x0 = x;
                nodes[i].x1 = (x + width);
                nodes[i].y0 = y;
                y += fmaxl(scaled_in[i], scaled_out[i]) * ky;
                if (!isfinite(y) || fabsl(y) > 32768)
                    return CUI_PLOT_OVERFLOW;
                nodes[i].y1 = y;
                y += padding;
            }
        long double leftover = (plot_bottom - y + padding) / (counts[l] + 1);
        size_t ordinal = 0;
        for (size_t i = 0; i < o->count; i++)
            if (nodes[i].layer == l) {
                double shift = (double)(leftover * ++ordinal);
                nodes[i].y0 += shift;
                nodes[i].y1 += shift;
            }
    }
    plot_sankey_relax(o, nodes, layers, padding, (double)ky);
    plot_sankey_center_and_stagger(o, nodes, layers);
    memcpy(out, nodes, o->count * sizeof(*out));
    return CUI_PLOT_OK;
}
static void plot_sankey_center_and_stagger(const plot_sankey_work *o, cui_plot_sankey_node *nodes,
                                           size_t layers) {
    double top = o->sankey_margins.top;
    double bottom = o->height - o->sankey_margins.bottom;
    double lo[256], hi[256], height[256] = {0};
    size_t count[256] = {0}, single[256] = {0};
    for (size_t layer = 0; layer < layers; layer++) {
        lo[layer] = DBL_MAX;
        hi[layer] = -DBL_MAX;
    }
    for (size_t i = 0; i < o->count; i++) {
        size_t layer = nodes[i].layer;
        lo[layer] = fmin(lo[layer], nodes[i].y0);
        hi[layer] = fmax(hi[layer], nodes[i].y1);
        count[layer]++;
        single[layer] = i;
    }
    for (size_t layer = 0; layer < layers; layer++) {
        double shift = hi[layer] > lo[layer] ? (top + bottom - lo[layer] - hi[layer]) / 2 : 0;
        for (size_t i = 0; i < o->count; i++)
            if (nodes[i].layer == layer) {
                nodes[i].y0 += shift;
                nodes[i].y1 += shift;
            }
        if (count[layer] == 1)
            height[layer] = nodes[single[layer]].y1 - nodes[single[layer]].y0;
    }
    size_t run = 0;
    for (size_t layer = 1; layer < layers; layer++) {
        if (count[layer] == 1 && count[layer - 1] == 1 &&
            fabs(height[layer] - height[layer - 1]) < 1e-3) {
            run++;
            if (run % 2) {
                double slack = fmax(0, bottom - top - height[layer]);
                double shift = fmin(height[layer] * .15, slack / 2);
                nodes[single[layer]].y0 += shift;
                nodes[single[layer]].y1 += shift;
            }
        } else
            run = 0;
    }
}
static void plot_sankey_resolve(cui_plot_sankey_node *nodes, size_t count, size_t layer,
                                double padding, double top, double bottom, double beta) {
    size_t indices[256], n = 0;
    for (size_t i = 0; i < count; i++)
        if (nodes[i].layer == layer)
            indices[n++] = i;
    for (size_t i = 1; i < n; i++) {
        size_t item = indices[i], j = i;
        while (j && nodes[indices[j - 1]].y0 > nodes[item].y0) {
            indices[j] = indices[j - 1];
            j--;
        }
        indices[j] = item;
    }
    if (!n)
        return;
    size_t middle = n / 2;
    double cursor = nodes[indices[middle]].y0 - padding;
    for (size_t i = middle; i > 0; i--) {
        cui_plot_sankey_node *node = &nodes[indices[i - 1]];
        double shift = fmax(0, node->y1 - cursor) * beta;
        node->y0 -= shift;
        node->y1 -= shift;
        cursor = node->y0 - padding;
    }
    cursor = nodes[indices[middle]].y1 + padding;
    for (size_t i = middle + 1; i < n; i++) {
        cui_plot_sankey_node *node = &nodes[indices[i]];
        double shift = fmax(0, cursor - node->y0) * beta;
        node->y0 += shift;
        node->y1 += shift;
        cursor = node->y1 + padding;
    }
    cursor = bottom;
    for (size_t i = n; i > 0; i--) {
        cui_plot_sankey_node *node = &nodes[indices[i - 1]];
        double shift = fmax(0, node->y1 - cursor) * beta;
        node->y0 -= shift;
        node->y1 -= shift;
        cursor = node->y0 - padding;
    }
    cursor = top;
    for (size_t i = 0; i < n; i++) {
        cui_plot_sankey_node *node = &nodes[indices[i]];
        double shift = fmax(0, cursor - node->y0) * beta;
        node->y0 += shift;
        node->y1 += shift;
        cursor = node->y1 + padding;
    }
}
static void plot_sankey_sort_adj(const plot_sankey_work *o, const cui_plot_sankey_node *nodes,
                                 size_t *order, size_t first, size_t end, bool sources) {
    for (size_t i = first + 1; i < end; i++) {
        size_t item = order[i], j = i;
        size_t other = sources ? o->links[item].target : o->links[item].source;
        while (j > first) {
            size_t previous = order[j - 1];
            size_t previous_other = sources ? o->links[previous].target : o->links[previous].source;
            if (nodes[previous_other].y0 < nodes[other].y0 ||
                (nodes[previous_other].y0 == nodes[other].y0 && previous < item))
                break;
            order[j] = previous;
            j--;
        }
        order[j] = item;
    }
}
static double plot_sankey_link_width(const plot_sankey_work *o, size_t link, double ky) {
    return plot_sankey_scaled(o, o->links[link].value) * ky;
}
static double plot_sankey_target_top(const plot_sankey_work *o, const cui_plot_sankey_node *nodes,
                                     const size_t *out_order, const size_t *out_offsets,
                                     const size_t *in_order, const size_t *in_offsets,
                                     size_t source, size_t target, double padding, double ky) {
    size_t outgoing = out_offsets[source + 1] - out_offsets[source];
    double y = nodes[source].y0 - (outgoing ? outgoing - 1 : 0) * padding / 2;
    for (size_t pos = out_offsets[source]; pos < out_offsets[source + 1]; pos++) {
        size_t link = out_order[pos];
        if (o->links[link].target == target)
            break;
        y += plot_sankey_link_width(o, link, ky) + padding;
    }
    for (size_t pos = in_offsets[target]; pos < in_offsets[target + 1]; pos++) {
        size_t link = in_order[pos];
        if (o->links[link].source == source)
            break;
        y -= plot_sankey_link_width(o, link, ky);
    }
    return y;
}
static double plot_sankey_source_top(const plot_sankey_work *o, const cui_plot_sankey_node *nodes,
                                     const size_t *out_order, const size_t *out_offsets,
                                     const size_t *in_order, const size_t *in_offsets,
                                     size_t source, size_t target, double padding, double ky) {
    size_t incoming = in_offsets[target + 1] - in_offsets[target];
    double y = nodes[target].y0 - (incoming ? incoming - 1 : 0) * padding / 2;
    for (size_t pos = in_offsets[target]; pos < in_offsets[target + 1]; pos++) {
        size_t link = in_order[pos];
        if (o->links[link].source == source)
            break;
        y += plot_sankey_link_width(o, link, ky) + padding;
    }
    for (size_t pos = out_offsets[source]; pos < out_offsets[source + 1]; pos++) {
        size_t link = out_order[pos];
        if (o->links[link].target == target)
            break;
        y -= plot_sankey_link_width(o, link, ky);
    }
    return y;
}
static void plot_sankey_relax(const plot_sankey_work *o, cui_plot_sankey_node *nodes, size_t layers,
                              double padding, double ky) {
    size_t out_offsets[257] = {0}, in_offsets[257] = {0};
    size_t out_order[1024], in_order[1024];
    for (size_t link = 0; link < o->link_count; link++) {
        out_offsets[o->links[link].source + 1]++;
        in_offsets[o->links[link].target + 1]++;
    }
    for (size_t node = 1; node <= o->count; node++) {
        out_offsets[node] += out_offsets[node - 1];
        in_offsets[node] += in_offsets[node - 1];
    }
    size_t out_next[256], in_next[256];
    for (size_t node = 0; node < o->count; node++) {
        out_next[node] = out_offsets[node];
        in_next[node] = in_offsets[node];
    }
    for (size_t link = 0; link < o->link_count; link++) {
        out_order[out_next[o->links[link].source]++] = link;
        in_order[in_next[o->links[link].target]++] = link;
    }
    for (size_t node = 0; node < o->count; node++) {
        plot_sankey_sort_adj(o, nodes, out_order, out_offsets[node], out_offsets[node + 1], true);
        plot_sankey_sort_adj(o, nodes, in_order, in_offsets[node], in_offsets[node + 1], false);
    }
    unsigned iterations = o->sankey_iterations;
    for (unsigned pass = 0; pass < iterations; pass++) {
        double alpha = pow(.99, pass);
        double beta = fmax(1 - alpha, (pass + 1.0) / iterations);
        for (int direction = 0; direction < 2; direction++)
            for (size_t step = 1; step < layers; step++) {
                size_t layer = direction == 0 ? layers - 1 - step : step;
                size_t column[256], count = 0;
                for (size_t node = 0; node < o->count; node++)
                    if (nodes[node].layer == layer)
                        column[count++] = node;
                for (size_t i = 1; i < count; i++) {
                    size_t item = column[i], j = i;
                    while (j && nodes[column[j - 1]].y0 > nodes[item].y0) {
                        column[j] = column[j - 1];
                        j--;
                    }
                    column[j] = item;
                }
                for (size_t item = 0; item < count; item++) {
                    size_t node = column[item];
                    long double weighted = 0, total = 0;
                    size_t *order = direction == 0 ? out_order : in_order;
                    size_t *offsets = direction == 0 ? out_offsets : in_offsets;
                    for (size_t pos = offsets[node]; pos < offsets[node + 1]; pos++) {
                        size_t link = order[pos];
                        size_t source = o->links[link].source;
                        size_t target = o->links[link].target;
                        size_t other = direction == 0 ? target : source;
                        size_t distance = nodes[other].layer > layer ? nodes[other].layer - layer
                                                                     : layer - nodes[other].layer;
                        long double weight = plot_sankey_scaled(o, o->links[link].value) * distance;
                        double ideal =
                            direction == 0
                                ? plot_sankey_source_top(o, nodes, out_order, out_offsets, in_order,
                                                         in_offsets, source, target, padding, ky)
                                : plot_sankey_target_top(o, nodes, out_order, out_offsets, in_order,
                                                         in_offsets, source, target, padding, ky);
                        weighted += weight * ideal;
                        total += weight;
                    }
                    if (total > 0) {
                        double delta = ((double)(weighted / total) - nodes[node].y0) * alpha;
                        nodes[node].y0 += delta;
                        nodes[node].y1 += delta;
                        for (size_t pos = in_offsets[node]; pos < in_offsets[node + 1]; pos++) {
                            size_t source = o->links[in_order[pos]].source;
                            plot_sankey_sort_adj(o, nodes, out_order, out_offsets[source],
                                                 out_offsets[source + 1], true);
                        }
                        for (size_t pos = out_offsets[node]; pos < out_offsets[node + 1]; pos++) {
                            size_t target = o->links[out_order[pos]].target;
                            plot_sankey_sort_adj(o, nodes, in_order, in_offsets[target],
                                                 in_offsets[target + 1], false);
                        }
                    }
                }
                plot_sankey_resolve(nodes, o->count, layer, padding, o->sankey_margins.top,
                                    o->height - o->sankey_margins.bottom, beta);
            }
    }
}

cui_plot_sankey_options cui_plot_sankey_defaults(void) {
    return (cui_plot_sankey_options){.extent = {0, 0, 1, 1},
                                     .node_width = 24,
                                     .node_padding = 8,
                                     .iterations = 6,
                                     .align = CUI_PLOT_SANKEY_JUSTIFY};
}
static bool plot_sankey_options_valid(const cui_plot_sankey_options *o) {
    return o && isfinite(o->extent.x_min) && isfinite(o->extent.x_max) &&
           isfinite(o->extent.y_min) && isfinite(o->extent.y_max) &&
           fabs(o->extent.x_min) <= 16384 && fabs(o->extent.x_max) <= 16384 &&
           fabs(o->extent.y_min) <= 16384 && fabs(o->extent.y_max) <= 16384 &&
           o->extent.x_min <= o->extent.x_max && o->extent.y_min < o->extent.y_max &&
           isfinite(o->node_width) && o->node_width >= 0 && o->node_width <= 16384 &&
           isfinite(o->node_padding) && o->node_padding >= 0 && o->node_padding <= 16384 &&
           o->iterations <= 100 && o->align >= CUI_PLOT_SANKEY_JUSTIFY &&
           o->align <= CUI_PLOT_SANKEY_CENTER && o->value_scale >= CUI_PLOT_SANKEY_LINEAR &&
           o->value_scale <= CUI_PLOT_SANKEY_SQRT;
}
static bool plot_sankey_storage_valid(const cui_plot_sankey_graph *g) {
    return g && (!g->node_capacity || g->nodes) && (!g->link_capacity || g->links);
}
static plot_sankey_work plot_sankey_work_options(const cui_plot_sankey_graph *g,
                                                 const cui_plot_sankey_options *o) {
    return (plot_sankey_work){.count = g->node_count,
                              .link_count = g->link_count,
                              .links = g->links,
                              .width = o->extent.x_max,
                              .height = o->extent.y_max,
                              .node_width = o->node_width,
                              .node_padding = o->node_padding,
                              .sankey_iterations = o->iterations,
                              .sankey_align = o->align,
                              .legacy_value_alignment = o->legacy_value_alignment,
                              .sankey_margins = {o->extent.x_min, 0, o->extent.y_min, 0}};
}
static bool plot_sankey_graph_valid(const cui_plot_sankey_graph *g) {
    if (!plot_sankey_storage_valid(g) || g->node_count > 256 || g->link_count > 1024 ||
        g->node_count > g->node_capacity || g->link_count > g->link_capacity)
        return false;
    for (size_t i = 0; i < g->node_count; ++i)
        if (g->nodes[i].index != i || g->nodes[i].depth >= g->node_count ||
            g->nodes[i].height >= g->node_count || g->nodes[i].layer >= g->node_count ||
            !isfinite(g->nodes[i].x0) || !isfinite(g->nodes[i].x1) || !isfinite(g->nodes[i].y0) ||
            !isfinite(g->nodes[i].y1) || !isfinite(g->nodes[i].value) || g->nodes[i].value < 0)
            return false;
    for (size_t i = 0; i < g->link_count; ++i) {
        const cui_plot_sankey_link_layout *l = g->links + i;
        if (l->index != i || l->source >= g->node_count || l->target >= g->node_count ||
            !isfinite(l->value) || l->value < 0 ||
            g->nodes[l->source].depth >= g->nodes[l->target].depth ||
            g->nodes[l->source].height <= g->nodes[l->target].height)
            return false;
    }
    return true;
}
cui_plot_result cui_plot_sankey_topology(cui_plot_sankey_graph *g, size_t nodes,
                                         const cui_plot_sankey_link *input, size_t links,
                                         const cui_plot_sankey_options *o,
                                         cui_plot_sankey_error *error) {
    if (error)
        *error = (cui_plot_sankey_error){0};
    if (!plot_sankey_storage_valid(g) || !plot_sankey_options_valid(o) || (links && !input))
        return CUI_PLOT_INVALID;
    if (nodes > 256 || links > 1024 || nodes > g->node_capacity || links > g->link_capacity)
        return CUI_PLOT_CAPACITY;
    cui_plot_sankey_node ns[256];
    cui_plot_sankey_link_layout ls[1024];
    for (size_t i = 0; i < links; ++i) {
        if (input[i].source >= nodes || input[i].target >= nodes) {
            if (error)
                *error = (cui_plot_sankey_error){CUI_PLOT_SANKEY_MISSING_NODE,
                                                 input[i].source >= nodes ? input[i].source
                                                                          : input[i].target};
            return CUI_PLOT_INVALID;
        }
        if (!isfinite(input[i].value) || input[i].value < 0)
            return CUI_PLOT_INVALID;
        ls[i] = (cui_plot_sankey_link_layout){.index = i,
                                              .source = input[i].source,
                                              .target = input[i].target,
                                              .value = o->value_scale == CUI_PLOT_SANKEY_SQRT
                                                           ? sqrt(input[i].value)
                                                           : input[i].value};
    }
    cui_plot_sankey_graph temporary = {ns, ls, nodes, links, 256, 1024};
    plot_sankey_work work = plot_sankey_work_options(&temporary, o);
    cui_plot_result result = plot_sankey_prepare(&work, ns, true, false);
    if (result != CUI_PLOT_OK) {
        if (result == CUI_PLOT_INVALID && error)
            *error = (cui_plot_sankey_error){CUI_PLOT_SANKEY_CIRCULAR_LINK, 0};
        return result;
    }
    if (nodes)
        memcpy(g->nodes, ns, nodes * sizeof(*ns));
    if (links)
        memcpy(g->links, ls, links * sizeof(*ls));
    g->node_count = nodes;
    g->link_count = links;
    return CUI_PLOT_OK;
}
size_t cui_plot_sankey_layer_count(const cui_plot_sankey_graph *g) {
    if (!plot_sankey_graph_valid(g))
        return 0;
    size_t layers = 0;
    for (size_t i = 0; i < g->node_count; ++i)
        if (layers < g->nodes[i].layer + 1)
            layers = g->nodes[i].layer + 1;
    return layers;
}
static cui_plot_result plot_sankey_node_links(const cui_plot_sankey_graph *g, size_t node,
                                              bool outgoing, size_t *out, size_t capacity,
                                              size_t *required) {
    size_t order[1024], count = 0;
    for (size_t i = 0; i < g->link_count; ++i)
        if ((outgoing ? g->links[i].source : g->links[i].target) == node) {
            size_t j = count, other = outgoing ? g->links[i].target : g->links[i].source;
            while (j) {
                size_t previous = order[j - 1];
                size_t po = outgoing ? g->links[previous].target : g->links[previous].source;
                if (g->nodes[po].y0 <= g->nodes[other].y0)
                    break;
                order[j] = previous;
                --j;
            }
            order[j] = i;
            ++count;
        }
    *required = count;
    if (capacity < count)
        return CUI_PLOT_CAPACITY;
    if (count)
        memcpy(out, order, count * sizeof(*out));
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_sankey_node_links(const cui_plot_sankey_graph *g, size_t node,
                                           bool outgoing, size_t *out, size_t capacity,
                                           size_t *required) {
    if (!required || (!out && capacity) || !plot_sankey_graph_valid(g) || node >= g->node_count)
        return CUI_PLOT_INVALID;
    return plot_sankey_node_links(g, node, outgoing, out, capacity, required);
}
cui_plot_result cui_plot_sankey_layout_from(cui_plot_sankey_graph *g,
                                            const cui_plot_sankey_options *o) {
    if (!plot_sankey_graph_valid(g) || !plot_sankey_options_valid(o))
        return CUI_PLOT_INVALID;
    cui_plot_sankey_node ns[256];
    cui_plot_sankey_link_layout ls[1024];
    if (g->node_count)
        memcpy(ns, g->nodes, g->node_count * sizeof(*ns));
    if (g->link_count)
        memcpy(ls, g->links, g->link_count * sizeof(*ls));
    cui_plot_sankey_graph temporary = {ns, ls, g->node_count, g->link_count, 256, 1024};
    plot_sankey_work work = plot_sankey_work_options(&temporary, o);
    cui_plot_result result = plot_sankey_prepare(&work, ns, false, true);
    if (result != CUI_PLOT_OK)
        return result;
    for (size_t i = 0; i < g->link_count; ++i) {
        const cui_plot_sankey_link_layout *l = ls + i;
        const cui_plot_sankey_node *source = ns + l->source;
        ls[i].width = source->value > 0 ? l->value / source->value * (source->y1 - source->y0) : 0;
    }
    result = cui_plot_sankey_link_breadths(&temporary);
    if (result != CUI_PLOT_OK)
        return result;
    for (size_t i = 0; i < g->node_count; ++i)
        if (!isfinite(ns[i].x0) || !isfinite(ns[i].x1) || !isfinite(ns[i].y0) ||
            !isfinite(ns[i].y1))
            return CUI_PLOT_OVERFLOW;
    for (size_t i = 0; i < g->link_count; ++i)
        if (!isfinite(ls[i].width) || !isfinite(ls[i].source_width) ||
            !isfinite(ls[i].target_width) || !isfinite(ls[i].y0) || !isfinite(ls[i].y1))
            return CUI_PLOT_OVERFLOW;
    if (g->node_count)
        memcpy(g->nodes, ns, g->node_count * sizeof(*ns));
    if (g->link_count)
        memcpy(g->links, ls, g->link_count * sizeof(*ls));
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_sankey_layout(cui_plot_sankey_graph *g, size_t nodes,
                                       const cui_plot_sankey_link *input, size_t links,
                                       const cui_plot_sankey_options *o,
                                       cui_plot_sankey_error *error) {
    if (error)
        *error = (cui_plot_sankey_error){0};
    if (!plot_sankey_storage_valid(g))
        return CUI_PLOT_INVALID;
    if (nodes > 256 || links > 1024 || nodes > g->node_capacity || links > g->link_capacity)
        return CUI_PLOT_CAPACITY;
    cui_plot_sankey_node ns[256];
    cui_plot_sankey_link_layout ls[1024];
    cui_plot_sankey_graph temporary = {ns, ls, 0, 0, 256, 1024};
    cui_plot_result result = cui_plot_sankey_topology(&temporary, nodes, input, links, o, error);
    if (result != CUI_PLOT_OK)
        return result;
    result = cui_plot_sankey_layout_from(&temporary, o);
    if (result != CUI_PLOT_OK)
        return result;
    if (nodes)
        memcpy(g->nodes, ns, nodes * sizeof(*ns));
    if (links)
        memcpy(g->links, ls, links * sizeof(*ls));
    g->node_count = nodes;
    g->link_count = links;
    return CUI_PLOT_OK;
}

cui_plot_result cui_plot_sankey_link_breadths(cui_plot_sankey_graph *g) {
    if (!plot_sankey_storage_valid(g) || g->node_count > 256 || g->link_count > 1024 ||
        g->node_count > g->node_capacity || g->link_count > g->link_capacity)
        return CUI_PLOT_INVALID;
    for (size_t i = 0; i < g->link_count; ++i)
        if (g->links[i].source >= g->node_count || g->links[i].target >= g->node_count ||
            !isfinite(g->links[i].value) || g->links[i].value < 0)
            return CUI_PLOT_INVALID;
    for (size_t i = 0; i < g->node_count; ++i)
        if (!isfinite(g->nodes[i].y0) || !isfinite(g->nodes[i].y1) ||
            g->nodes[i].y0 > g->nodes[i].y1)
            return CUI_PLOT_INVALID;
    cui_plot_sankey_link_layout ls[1024];
    if (g->link_count)
        memcpy(ls, g->links, g->link_count * sizeof(*ls));
    cui_plot_sankey_graph temporary = *g;
    temporary.links = ls;
    temporary.link_capacity = 1024;
    cui_plot_result result;
    for (size_t node = 0; node < g->node_count; ++node)
        for (size_t side = 0; side < 2; ++side) {
            size_t order[1024], count;
            result = plot_sankey_node_links(&temporary, node, side == 0, order, 1024, &count);
            if (result != CUI_PLOT_OK)
                return result;
            long double total = 0;
            double used = 0;
            for (size_t i = 0; i < count; ++i)
                total += ls[order[i]].value;
            for (size_t i = 0; i < count; ++i) {
                cui_plot_sankey_link_layout *l = ls + order[i];
                double width =
                    total > 0 ? (double)(l->value / total) * (g->nodes[node].y1 - g->nodes[node].y0)
                              : 0;
                double y = g->nodes[node].y0 + used + width / 2;
                if (side == 0) {
                    l->source_width = width;
                    l->y0 = y;
                } else {
                    l->target_width = width;
                    l->y1 = y;
                }
                used += width;
            }
        }

    for (size_t i = 0; i < g->link_count; ++i)
        if (!isfinite(ls[i].source_width) || !isfinite(ls[i].target_width) || !isfinite(ls[i].y0) ||
            !isfinite(ls[i].y1))
            return CUI_PLOT_OVERFLOW;
    if (g->link_count)
        memcpy(g->links, ls, g->link_count * sizeof(*ls));
    return CUI_PLOT_OK;
}

static bool plot_appear_registry_valid(const cui_plot_appear_registry *registry) {
    return registry && registry->capacity <= 4096 && registry->count <= registry->capacity &&
           (!registry->capacity || registry->entries);
}
cui_plot_result cui_plot_appear_registry_init(cui_plot_appear_registry *registry,
                                              cui_plot_appear_scope *storage, size_t capacity) {
    if (!registry || capacity > 4096 || (capacity && !storage))
        return CUI_PLOT_INVALID;
    *registry = (cui_plot_appear_registry){.entries = storage, .capacity = capacity};
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_appear_registry_begin(cui_plot_appear_registry *registry,
                                               uint64_t identity, uint64_t frame) {
    if (!plot_appear_registry_valid(registry))
        return CUI_PLOT_INVALID;
    if (!registry->initialized || registry->identity != identity ||
        (registry->last_frame != frame &&
         (registry->last_frame == UINT64_MAX || frame != registry->last_frame + 1)))
        registry->count = 0;
    registry->identity = identity;
    registry->last_frame = frame;
    registry->initialized = true;
    registry->active = true;
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_appear_registry_end(cui_plot_appear_registry *registry) {
    if (!plot_appear_registry_valid(registry) || !registry->active)
        return CUI_PLOT_INVALID;
    registry->active = false;
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_appear_registry_complete(const cui_plot_appear_registry *registry,
                                                  uint64_t identity, uint64_t generation,
                                                  bool *out) {
    if (!out || !plot_appear_registry_valid(registry) || !registry->active)
        return CUI_PLOT_INVALID;
    bool complete = false;
    for (size_t i = 0; i < registry->count; ++i)
        if (registry->entries[i].identity == identity) {
            complete = registry->entries[i].has_completed &&
                       registry->entries[i].completed_generation == generation;
            break;
        }
    *out = complete;
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_appear_registry_record(cui_plot_appear_registry *registry,
                                                uint64_t identity, uint64_t generation,
                                                double progress) {
    if (!plot_appear_registry_valid(registry) || !registry->active || !isfinite(progress) ||
        progress < 0 || progress > 1)
        return CUI_PLOT_INVALID;
    if (progress < 1)
        return CUI_PLOT_OK;
    size_t index = 0;
    while (index < registry->count && registry->entries[index].identity != identity)
        ++index;
    if (index == registry->count && index == registry->capacity)
        return CUI_PLOT_CAPACITY;
    registry->entries[index] = (cui_plot_appear_scope){identity, generation, true};
    if (index == registry->count)
        ++registry->count;
    return CUI_PLOT_OK;
}

static double plot_transition_progress(double elapsed, double duration, double delay,
                                       cui_plot_motion_status *status) {
    double active = elapsed - delay;
    if (active < 0) {
        *status = CUI_PLOT_MOTION_DELAYED;
        return 0;
    }
    if (duration == 0 || active >= duration) {
        *status = CUI_PLOT_MOTION_FINISHED;
        return 1;
    }
    *status = CUI_PLOT_MOTION_RUNNING;
    return active / duration;
}
static double plot_transition_sample(cui_plot_transition_policy policy, double progress) {
    if (policy.easing)
        return policy.easing(progress, policy.user);
    double remaining = 1 - progress;
    return 1 - remaining * remaining * remaining;
}
cui_plot_result cui_plot_transition_step(cui_plot_transition_clock *clock, double target,
                                         double now, cui_plot_transition_policy policy,
                                         bool reduced_motion, cui_plot_motion_value *out) {
    if (!clock || !out || !isfinite(target) || !isfinite(now) || now < 0 ||
        !isfinite(policy.duration) || policy.duration < 0 || !isfinite(policy.delay))
        return CUI_PLOT_INVALID;
    cui_plot_transition_clock next = *clock;
    if (!next.initialized)
        next = (cui_plot_transition_clock){target, target, now, 1, policy.duration, true};
    if (!isfinite(next.from) || !isfinite(next.target) || !isfinite(next.started_at) ||
        next.started_at < 0 || !isfinite(next.reversing_factor) || next.reversing_factor < 0 ||
        next.reversing_factor > 1 || !isfinite(next.duration) || next.duration < 0)
        return CUI_PLOT_INVALID;
    cui_plot_motion_value value = {target, CUI_PLOT_MOTION_FINISHED, false};
    if (reduced_motion || policy.duration == 0) {
        if (next.from != target || next.target != target)
            next = (cui_plot_transition_clock){target, target, now, 1, policy.duration, true};
    } else {
        double elapsed = now > next.started_at ? now - next.started_at : 0;
        double progress =
            plot_transition_progress(elapsed, next.duration, policy.delay, &value.status);
        double eased = plot_transition_sample(policy, progress);
        if (!isfinite(eased))
            return CUI_PLOT_INVALID;
        /* Weighted interpolation avoids overflowing target-from. */
        double sampled = (1 - eased) * next.from + eased * next.target;
        if (next.target != target) {
            double factor =
                target == next.from ? eased * next.reversing_factor + 1 - next.reversing_factor : 1;
            factor = fmax(0, fmin(1, factor));
            next = (cui_plot_transition_clock){
                sampled, target, now, factor, policy.duration * factor, true};
            progress = plot_transition_progress(0, next.duration, policy.delay, &value.status);
            eased = plot_transition_sample(policy, progress);
            if (!isfinite(eased))
                return CUI_PLOT_INVALID;
            value.value = (1 - eased) * sampled + eased * target;
        } else {
            value.value = sampled;
            if (next.from == next.target)
                value.status = CUI_PLOT_MOTION_IDLE;
        }
        if (!isfinite(value.value) || !isfinite(next.from))
            return CUI_PLOT_OVERFLOW;
        value.request_frame =
            value.status == CUI_PLOT_MOTION_DELAYED || value.status == CUI_PLOT_MOTION_RUNNING;
    }
    *clock = next;
    *out = value;
    return CUI_PLOT_OK;
}

cui_plot_spring_policy cui_plot_spring_defaults(double response) {
    return (cui_plot_spring_policy){response, 1, .001, true};
}
/* Analytic damped oscillator, adapted from CUI tab spring math. This clock's
 * ordering differs: advance the old target, then adopt the incoming target. */
static bool plot_spring_advance(cui_plot_spring_clock *state, double elapsed,
                                cui_plot_spring_policy policy) {
    if (elapsed == 0)
        return true;
    double omega = 6.28318530717958647692 / policy.response;
    double displacement = state->position - state->target, speed = state->velocity;
    if (displacement == 0 && speed == 0)
        return true;
    double damping = policy.damping, next, next_speed;
    if (!isfinite(omega) || !isfinite(displacement))
        return false;
    if (damping == 1) {
        double sum = speed + omega * displacement;
        double decay = exp(-omega * elapsed);
        if (decay == 0) {
            next = next_speed = 0;
        } else {
            next = (displacement + sum * elapsed) * decay;
            next_speed = (speed - omega * sum * elapsed) * decay;
        }
    } else if (damping < 1) {
        double frequency = omega * sqrt((1 - damping) * (1 + damping));
        double decay = exp(-damping * omega * elapsed);
        double angle = frequency * elapsed;
        if (!isfinite(angle))
            return false;
        double sine = sin(angle), cosine = cos(angle);
        next = decay * (displacement * cosine +
                        (speed + damping * omega * displacement) / frequency * sine);
        next_speed =
            decay * (speed * cosine -
                     (damping * omega * speed + omega * omega * displacement) / frequency * sine);
    } else {
        double factor = damping + sqrt(damping - 1) * sqrt(damping + 1);
        double slow = -omega / factor, fast = -omega * factor;
        double first = (speed - fast * displacement) / (slow - fast);
        double second = displacement - first;
        next = first * exp(slow * elapsed) + second * exp(fast * elapsed);
        next_speed = slow * first * exp(slow * elapsed) + fast * second * exp(fast * elapsed);
    }
    state->position = state->target + next;
    state->velocity = next_speed;
    return isfinite(state->position) && isfinite(state->velocity);
}
cui_plot_result cui_plot_spring_step(cui_plot_spring_clock *clock, double target, double now,
                                     cui_plot_spring_policy policy, bool reduced_motion,
                                     bool entering, cui_plot_spring_value *out) {
    if (!clock || !out || !isfinite(target) || !isfinite(now) || now < 0 ||
        !isfinite(policy.response) || policy.response < 0 || !isfinite(policy.damping) ||
        policy.damping < 0 || !isfinite(policy.epsilon) || policy.epsilon <= 0)
        return CUI_PLOT_INVALID;
    cui_plot_spring_clock next = *clock;
    if (!next.initialized)
        next = (cui_plot_spring_clock){target, 0, target, now, true};
    if (!isfinite(next.position) || !isfinite(next.velocity) || !isfinite(next.target) ||
        !isfinite(next.updated_at) || next.updated_at < 0)
        return CUI_PLOT_INVALID;
    cui_plot_spring_value value = {next.position, next.velocity, false};
    /* Resting returns without updating even the stale timestamp, as source does. */
    if (next.position != target || next.velocity != 0) {
        if (reduced_motion || entering || !policy.travel || policy.response == 0) {
            next = (cui_plot_spring_clock){target, 0, target, now, true};
        } else {
            double elapsed = now > next.updated_at ? now - next.updated_at : 0;
            if (!plot_spring_advance(&next, elapsed, policy))
                return CUI_PLOT_OVERFLOW;
            if (fabs(next.position - target) < policy.epsilon &&
                fabs(next.velocity) < policy.epsilon)
                next = (cui_plot_spring_clock){target, 0, target, now, true};
            else {
                next.target = target;
                next.updated_at = now;
                value.request_frame = true;
            }
        }
        value.value = next.position;
        value.velocity = next.velocity;
    }
    *clock = next;
    *out = value;
    return CUI_PLOT_OK;
}

static void plot_shape_key_bytes(cui_plot_shape_key *key, const void *data, size_t count) {
    const unsigned char *bytes = data;
    for (size_t i = 0; i < count; ++i) {
        key->value ^= bytes[i];
        key->value *= UINT64_C(1099511628211);
    }
}
cui_plot_shape_key cui_plot_shape_key_init(uint64_t extra) {
    cui_plot_shape_key key = {UINT64_C(14695981039346656037)};
    plot_shape_key_bytes(&key, &extra, sizeof extra);
    return key;
}
cui_plot_result cui_plot_shape_key_float(cui_plot_shape_key *key, float value) {
    if (!key)
        return CUI_PLOT_INVALID;
    plot_shape_key_bytes(key, &value, sizeof value);
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_shape_key_point(cui_plot_shape_key *key, cui_plot_point point) {
    if (!key || !isfinite(point.x) || !isfinite(point.y) || fabs(point.x) > FLT_MAX ||
        fabs(point.y) > FLT_MAX)
        return CUI_PLOT_INVALID;
    (void)cui_plot_shape_key_float(key, (float)point.x);
    return cui_plot_shape_key_float(key, (float)point.y);
}

static double plot_appear_linear(double progress, void *user) {
    (void)user;
    return progress;
}
cui_plot_result cui_plot_appear_step(cui_plot_transition_clock *clock, double now,
                                     cui_plot_transition_policy policy, bool reduced_motion,
                                     cui_plot_appear_value *out) {
    if (!clock || !out || !isfinite(now) || now < 0 || !isfinite(policy.duration) ||
        policy.duration < 0 || !isfinite(policy.delay))
        return CUI_PLOT_INVALID;
    cui_plot_transition_clock next = *clock;
    if (!next.initialized)
        next = (cui_plot_transition_clock){0, 1, now, 1, policy.duration, true};
    cui_plot_transition_policy linear = policy;
    linear.easing = plot_appear_linear;
    linear.user = NULL;
    cui_plot_motion_value motion;
    cui_plot_result result =
        cui_plot_transition_step(&next, 1, now, linear, reduced_motion, &motion);
    if (result != CUI_PLOT_OK)
        return result;
    cui_plot_appear_value value = {
        motion.value, 1, policy.easing, policy.user, motion.value < 1, motion.request_frame};
    if (value.appearing)
        value.progress = plot_transition_sample(policy, motion.value);
    if (!isfinite(value.progress) || !isfinite(value.time) || value.time < 0 || value.time > 1)
        return CUI_PLOT_INVALID;
    *clock = next;
    *out = value;
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_appear_value_stagger(const cui_plot_appear_value *value, size_t index,
                                              size_t count, double spread, double *out) {
    if (!value || !out || !isfinite(value->time) || value->time < 0 || value->time > 1 ||
        !isfinite(value->progress) || !isfinite(spread))
        return CUI_PLOT_INVALID;
    double progress = 1;
    if (value->time < 1) {
        if (count <= 1)
            progress = value->progress;
        else {
            double time;
            cui_plot_result result =
                cui_plot_appear_stagger(value->time, index, count, spread, &time);
            if (result != CUI_PLOT_OK)
                return result;
            progress = plot_transition_sample(
                (cui_plot_transition_policy){.easing = value->easing, .user = value->user}, time);
        }
    }
    if (!isfinite(progress))
        return CUI_PLOT_INVALID;
    *out = progress;
    return CUI_PLOT_OK;
}

static double mesh_cross(cui_plot_point a, cui_plot_point b, cui_plot_point c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}
static bool mesh_between(double a, double b, double x) {
    return x >= fmin(a, b) && x <= fmax(a, b);
}
static bool mesh_edges_touch(cui_plot_point a, cui_plot_point b, cui_plot_point c,
                             cui_plot_point d) {
    double ab_c = mesh_cross(a, b, c), ab_d = mesh_cross(a, b, d);
    double cd_a = mesh_cross(c, d, a), cd_b = mesh_cross(c, d, b);
    if (((ab_c < 0 && ab_d > 0) || (ab_c > 0 && ab_d < 0)) &&
        ((cd_a < 0 && cd_b > 0) || (cd_a > 0 && cd_b < 0)))
        return true;
    return (ab_c == 0 && mesh_between(a.x, b.x, c.x) && mesh_between(a.y, b.y, c.y)) ||
           (ab_d == 0 && mesh_between(a.x, b.x, d.x) && mesh_between(a.y, b.y, d.y)) ||
           (cd_a == 0 && mesh_between(c.x, d.x, a.x) && mesh_between(c.y, d.y, a.y)) ||
           (cd_b == 0 && mesh_between(c.x, d.x, b.x) && mesh_between(c.y, d.y, b.y));
}
cui_plot_result cui_plot_fill_mesh(const cui_plot_point *p, size_t n, cui_plot_mesh_vertex *out,
                                   size_t capacity, size_t *required, cui_plot_rect *bounds) {
    if (!p || !required || !bounds || n < 3 || n > 256 || (capacity && !out))
        return CUI_PLOT_INVALID;
    cui_plot_rect box = {p[0].x, p[0].y, p[0].x, p[0].y};
    double area = 0;
    for (size_t i = 0; i < n; i++) {
        if (!isfinite(p[i].x) || !isfinite(p[i].y) || fabs(p[i].x) > 16384 || fabs(p[i].y) > 16384)
            return CUI_PLOT_INVALID;
        size_t j = (i + 1) % n;
        if (p[i].x == p[j].x && p[i].y == p[j].y)
            return CUI_PLOT_DEGENERATE;
        if (mesh_cross(p[(i + n - 1) % n], p[i], p[j]) == 0)
            return CUI_PLOT_DEGENERATE;
        area += p[i].x * p[j].y - p[j].x * p[i].y;
        box.x_min = fmin(box.x_min, p[i].x);
        box.x_max = fmax(box.x_max, p[i].x);
        box.y_min = fmin(box.y_min, p[i].y);
        box.y_max = fmax(box.y_max, p[i].y);
    }
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++) {
            if (j == i + 1 || (i == 0 && j == n - 1))
                continue;
            if (mesh_edges_touch(p[i], p[(i + 1) % n], p[j], p[(j + 1) % n]))
                return CUI_PLOT_INVALID;
        }
    if (area == 0)
        return CUI_PLOT_DEGENERATE;
    size_t indices[256], triangles[254 * 3], used = 0, left = n;
    for (size_t i = 0; i < n; i++)
        indices[i] = i;
    double sign = area > 0 ? 1 : -1;
    while (left > 3) {
        bool found = false;
        for (size_t i = 0; i < left; i++) {
            size_t a = indices[(i + left - 1) % left], b = indices[i], c = indices[(i + 1) % left];
            if (sign * mesh_cross(p[a], p[b], p[c]) <= 0)
                continue;
            bool blocked = false;
            for (size_t j = 0; j < left; j++) {
                size_t v = indices[j];
                if (v == a || v == b || v == c)
                    continue;
                if (sign * mesh_cross(p[a], p[b], p[v]) >= 0 &&
                    sign * mesh_cross(p[b], p[c], p[v]) >= 0 &&
                    sign * mesh_cross(p[c], p[a], p[v]) >= 0) {
                    blocked = true;
                    break;
                }
            }
            if (blocked)
                continue;
            triangles[used++] = a;
            triangles[used++] = b;
            triangles[used++] = c;
            for (size_t j = i; j + 1 < left; j++)
                indices[j] = indices[j + 1];
            left--;
            found = true;
            break;
        }
        if (!found)
            return CUI_PLOT_DEGENERATE;
    }
    for (size_t i = 0; i < 3; i++)
        triangles[used++] = indices[i];
    *required = used;
    if (!out || capacity < used)
        return CUI_PLOT_CAPACITY;
    for (size_t i = 0; i < used; i++)
        out[i] = (cui_plot_mesh_vertex){p[triangles[i]], {0, 1}};
    *bounds = box;
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_mesh_coverage(cui_plot_point st, cui_plot_point dx, cui_plot_point dy,
                                       double *coverage) {
    if (!coverage || !isfinite(st.x) || !isfinite(st.y) || !isfinite(dx.x) || !isfinite(dx.y) ||
        !isfinite(dy.x) || !isfinite(dy.y))
        return CUI_PLOT_INVALID;
    double alpha = 1;
    if (hypot(dx.x, dy.x) >= .001) {
        double gx = 2 * st.x * dx.x - dx.y, gy = 2 * st.x * dy.x - dy.y;
        double f = st.x * st.x - st.y, gradient = hypot(gx, gy);
        if (!isfinite(f) || !isfinite(gradient))
            return CUI_PLOT_OVERFLOW;
        if (gradient == 0)
            return CUI_PLOT_DEGENERATE;
        alpha = fmax(0, fmin(1, .5 - f / gradient));
    }
    *coverage = alpha;
    return CUI_PLOT_OK;
}

static bool mesh_point_valid(cui_plot_point p) {
    return isfinite(p.x) && isfinite(p.y) && fabs(p.x) <= 16384 && fabs(p.y) <= 16384;
}
static double mesh_chord_distance(cui_plot_point p, cui_plot_point a, cui_plot_point b) {
    double dx = b.x - a.x, dy = b.y - a.y, length = dx * dx + dy * dy;
    double t = length ? fmax(0, fmin(1, ((p.x - a.x) * dx + (p.y - a.y) * dy) / length)) : 0;
    return hypot(p.x - a.x - t * dx, p.y - a.y - t * dy);
}
static cui_plot_point mesh_midpoint(cui_plot_point a, cui_plot_point b) {
    return (cui_plot_point){a.x / 2 + b.x / 2, a.y / 2 + b.y / 2};
}
cui_plot_result cui_plot_cubic_flatten(cui_plot_point start, cui_plot_point c1, cui_plot_point c2,
                                       cui_plot_point end, double tolerance, cui_plot_point *out,
                                       size_t capacity, size_t *required) {
    if (!required || (capacity && !out) || !mesh_point_valid(start) || !mesh_point_valid(c1) ||
        !mesh_point_valid(c2) || !mesh_point_valid(end) || !isfinite(tolerance) ||
        tolerance < 1e-6 || tolerance > 16384)
        return CUI_PLOT_INVALID;
    typedef struct {
        cui_plot_point p[4];
        unsigned depth;
    } cubic_piece;
    cubic_piece stack[25] = {{{start, c1, c2, end}, 0}};
    size_t pending = 1, used = 1;
    cui_plot_point points[4097];
    points[0] = start;
    while (pending) {
        cubic_piece piece = stack[--pending];
        if (fmax(mesh_chord_distance(piece.p[1], piece.p[0], piece.p[3]),
                 mesh_chord_distance(piece.p[2], piece.p[0], piece.p[3])) <= tolerance) {
            if (used == 4097)
                return CUI_PLOT_CAPACITY;
            points[used++] = piece.p[3];
            continue;
        }
        if (piece.depth == 24 || pending + 2 > 25)
            return CUI_PLOT_CAPACITY;
        cui_plot_point a = mesh_midpoint(piece.p[0], piece.p[1]);
        cui_plot_point b = mesh_midpoint(piece.p[1], piece.p[2]);
        cui_plot_point c = mesh_midpoint(piece.p[2], piece.p[3]);
        cui_plot_point d = mesh_midpoint(a, b), e = mesh_midpoint(b, c), m = mesh_midpoint(d, e);
        stack[pending++] = (cubic_piece){{m, e, c, piece.p[3]}, piece.depth + 1};
        stack[pending++] = (cubic_piece){{piece.p[0], a, d, m}, piece.depth + 1};
    }
    *required = used;
    if (!out || capacity < used)
        return CUI_PLOT_CAPACITY;
    memcpy(out, points, used * sizeof *out);
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_stroke_mesh(const cui_plot_point *p, size_t n, double width, double limit,
                                     cui_plot_mesh_vertex *out, size_t capacity, size_t *required,
                                     cui_plot_rect *bounds) {
    if (!p || !required || !bounds || n < 2 || n > 64 || (capacity && !out) || !isfinite(width) ||
        width < 0 || width > 1024 || !isfinite(limit) || limit < 1 || limit > 64)
        return CUI_PLOT_INVALID;
    cui_plot_point direction[63];
    for (size_t i = 0; i < n; i++) {
        if (!mesh_point_valid(p[i]))
            return CUI_PLOT_INVALID;
        if (i) {
            double dx = p[i].x - p[i - 1].x, dy = p[i].y - p[i - 1].y, length = hypot(dx, dy);
            if (length == 0)
                return CUI_PLOT_DEGENERATE;
            direction[i - 1] = (cui_plot_point){dx / length, dy / length};
        }
    }
    if (width == 0) {
        *required = 0;
        *bounds = (cui_plot_rect){0};
        return CUI_PLOT_EMPTY;
    }
    cui_plot_point side[2][128], contour[256];
    size_t used[2] = {0, 0};
    double half = width / 2;
    for (size_t k = 0; k < 2; k++) {
        double sign = k == 0 ? 1 : -1;
        for (size_t i = 0; i < n; i++) {
            cui_plot_point prev = direction[i ? i - 1 : 0], next = direction[i + 1 < n ? i : n - 2];
            double cross = prev.x * next.y - prev.y * next.x,
                   dot = prev.x * next.x + prev.y * next.y;
            if (i && i + 1 < n && cross == 0 && dot > 0)
                continue;
            double denominator = 1 + dot;
            if (denominator <= 0)
                return CUI_PLOT_DEGENERATE;
            cui_plot_point m = {-(prev.y + next.y) / denominator, (prev.x + next.x) / denominator};
            if (sign * cross < 0 && hypot(m.x, m.y) > 2 * limit) {
                side[k][used[k]++] =
                    (cui_plot_point){p[i].x - sign * half * prev.y, p[i].y + sign * half * prev.x};
                side[k][used[k]++] =
                    (cui_plot_point){p[i].x - sign * half * next.y, p[i].y + sign * half * next.x};
            } else
                side[k][used[k]++] =
                    (cui_plot_point){p[i].x + sign * half * m.x, p[i].y + sign * half * m.y};
        }
    }
    size_t count = 0;
    for (size_t i = 0; i < used[0]; i++)
        contour[count++] = side[0][i];
    for (size_t i = used[1]; i; i--)
        contour[count++] = side[1][i - 1];
    for (size_t i = 0; i < count; i++)
        if (!mesh_point_valid(contour[i]))
            return CUI_PLOT_OVERFLOW;
    return cui_plot_fill_mesh(contour, count, out, capacity, required, bounds);
}

/* A horizontal arrangement decomposition: every endpoint and proper crossing
 * is a band boundary. Edge order is constant inside each open band. */
typedef struct {
    cui_plot_point a, b;
} plot_fill_edge;
/* In-place heapsort: unlike libc qsort, scratch allocation is never hidden. */
static void plot_fill_event_sift(long double *events, size_t root, size_t count) {
    while (root < count / 2) {
        size_t child = root * 2 + 1;
        if (child + 1 < count && events[child] < events[child + 1])
            ++child;
        if (events[root] >= events[child])
            return;
        long double value = events[root];
        events[root] = events[child];
        events[child] = value;
        root = child;
    }
}
static void plot_fill_event_sort(long double *events, size_t count) {
    for (size_t root = count / 2; root;)
        plot_fill_event_sift(events, --root, count);
    for (size_t end = count; end > 1;) {
        long double value = events[0];
        events[0] = events[--end];
        events[end] = value;
        plot_fill_event_sift(events, 0, end);
    }
}
static long double plot_fill_x(plot_fill_edge e, long double y) {
    return e.a.x + (y - e.a.y) * ((long double)e.b.x - e.a.x) / ((long double)e.b.y - e.a.y);
}
static void plot_fill_triangle(cui_plot_point a, cui_plot_point b, cui_plot_point c,
                               cui_plot_mesh_vertex *out, size_t *used, cui_plot_rect *box) {
    long double cross = ((long double)b.x - a.x) * ((long double)c.y - a.y) -
                        ((long double)b.y - a.y) * ((long double)c.x - a.x);
    if (cross == 0)
        return;
    cui_plot_point p[3] = {a, b, c};
    for (size_t i = 0; i < 3; i++) {
        if (*used == 0)
            *box = (cui_plot_rect){p[i].x, p[i].y, p[i].x, p[i].y};
        box->x_min = fmin(box->x_min, p[i].x);
        box->x_max = fmax(box->x_max, p[i].x);
        box->y_min = fmin(box->y_min, p[i].y);
        box->y_max = fmax(box->y_max, p[i].y);
        if (out)
            out[*used] = (cui_plot_mesh_vertex){p[i], {0, 1}};
        ++*used;
    }
}
cui_plot_result cui_plot_fill_mesh_contours(const cui_plot_contour *contours, size_t count,
                                            cui_plot_fill_rule rule,
                                            cui_plot_fill_workspace *workspace,
                                            cui_plot_mesh_vertex *out, size_t capacity,
                                            size_t *required, cui_plot_rect *bounds) {
    if (!contours || !count || count > 85 || !workspace || !required || !bounds ||
        (capacity && !out) || (rule != CUI_PLOT_FILL_EVEN_ODD && rule != CUI_PLOT_FILL_NON_ZERO))
        return CUI_PLOT_INVALID;
    plot_fill_edge edges[256];
    size_t n = 0;
    for (size_t c = 0; c < count; c++) {
        if (!contours[c].points || contours[c].count < 3 || contours[c].count > 256 - n)
            return CUI_PLOT_INVALID;
        for (size_t i = 0; i < contours[c].count; i++) {
            cui_plot_point a = contours[c].points[i];
            if (!mesh_point_valid(a))
                return CUI_PLOT_INVALID;
            edges[n++] = (plot_fill_edge){a, contours[c].points[(i + 1) % contours[c].count]};
        }
    }
    if (!workspace->events || !workspace->order ||
        workspace->events_capacity < n + n * (n - 1) / 2 || workspace->order_capacity < n)
        return CUI_PLOT_CAPACITY;
    long double *events = workspace->events;
    size_t events_count = 0;
    for (size_t i = 0; i < n; i++)
        events[events_count++] = edges[i].a.y;
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++) {
            plot_fill_edge a = edges[i], b = edges[j];
            long double ax = (long double)a.b.x - a.a.x, ay = (long double)a.b.y - a.a.y;
            long double bx = (long double)b.b.x - b.a.x, by = (long double)b.b.y - b.a.y;
            long double dx = (long double)b.a.x - a.a.x, dy = (long double)b.a.y - a.a.y;
            long double det = ax * by - ay * bx;
            if (det == 0)
                continue;
            long double t = (dx * by - dy * bx) / det, u = (dx * ay - dy * ax) / det;
            if (t > 0 && t < 1 && u > 0 && u < 1)
                events[events_count++] = (double)(a.a.y + t * ay);
        }
    plot_fill_event_sort(events, events_count);
    size_t unique = 0;
    for (size_t i = 0; i < events_count; i++)
        if (!unique || events[i] != events[unique - 1])
            events[unique++] = events[i];
    size_t needed = 0;
    cui_plot_rect box = {0};
    for (unsigned pass = 0; pass < 2; pass++) {
        size_t used = 0;
        box = (cui_plot_rect){0};
        for (size_t band = 1; band < unique; band++) {
            long double lo = events[band - 1], hi = events[band], mid = lo + (hi - lo) / 2;
            size_t active = 0;
            for (size_t e = 0; e < n; e++) {
                if (edges[e].a.y == edges[e].b.y || lo < fmin(edges[e].a.y, edges[e].b.y) ||
                    hi > fmax(edges[e].a.y, edges[e].b.y))
                    continue;
                long double x = plot_fill_x(edges[e], mid);
                size_t j = active;
                while (j && plot_fill_x(edges[workspace->order[j - 1]], mid) > x) {
                    workspace->order[j] = workspace->order[j - 1];
                    --j;
                }
                workspace->order[j] = e;
                ++active;
            }
            int winding = 0;
            for (size_t i = 0; i + 1 < active; i++) {
                plot_fill_edge left = edges[workspace->order[i]],
                               right = edges[workspace->order[i + 1]];
                winding += left.b.y > left.a.y ? 1 : -1;
                if (!(rule == CUI_PLOT_FILL_NON_ZERO ? winding != 0 : winding % 2 != 0))
                    continue;
                cui_plot_point a = {(double)plot_fill_x(left, lo), (double)lo};
                cui_plot_point b = {(double)plot_fill_x(right, lo), (double)lo};
                cui_plot_point c = {(double)plot_fill_x(right, hi), (double)hi};
                cui_plot_point d = {(double)plot_fill_x(left, hi), (double)hi};
                plot_fill_triangle(a, b, c, pass ? out : NULL, &used, &box);
                plot_fill_triangle(a, c, d, pass ? out : NULL, &used, &box);
            }
        }
        if (!pass) {
            needed = used;
            if (needed && (!out || capacity < needed)) {
                *required = needed;
                return CUI_PLOT_CAPACITY;
            }
        }
    }
    *required = needed;
    *bounds = box;
    return CUI_PLOT_OK;
}

cui_plot_stroke_options cui_plot_stroke_defaults(void) {
    return (cui_plot_stroke_options){
        1, 4, CUI_PLOT_CAP_BUTT, CUI_PLOT_CAP_BUTT, CUI_PLOT_JOIN_MITER, 32, false};
}
typedef struct {
    cui_plot_point points[256];
    cui_plot_contour contours[85];
    size_t point_count, contour_count;
} plot_stroke_polygons;
static cui_plot_result plot_stroke_polygon(plot_stroke_polygons *polygons,
                                           const cui_plot_point *points, size_t count) {
    if (count > 256 - polygons->point_count || polygons->contour_count == 85)
        return CUI_PLOT_CAPACITY;
    long double area = 0;
    for (size_t i = 0; i < count; i++) {
        if (!mesh_point_valid(points[i]))
            return CUI_PLOT_OVERFLOW;
        cui_plot_point a = points[i], b = points[(i + 1) % count];
        area += (long double)a.x * b.y - (long double)b.x * a.y;
    }
    if (area == 0)
        return CUI_PLOT_OK;
    cui_plot_point *out = polygons->points + polygons->point_count;
    for (size_t i = 0; i < count; i++)
        out[i] = points[area > 0 ? i : count - 1 - i];
    polygons->contours[polygons->contour_count++] = (cui_plot_contour){out, count};
    polygons->point_count += count;
    return CUI_PLOT_OK;
}
static cui_plot_result plot_stroke_disk(plot_stroke_polygons *polygons, cui_plot_point center,
                                        double radius, size_t segments) {
    cui_plot_point circle[64];
    for (size_t i = 0; i < segments; i++) {
        double angle = 6.28318530717958647692 * (double)i / (double)segments;
        circle[i] =
            (cui_plot_point){center.x + radius * cos(angle), center.y + radius * sin(angle)};
    }
    return plot_stroke_polygon(polygons, circle, segments);
}
static cui_plot_result plot_stroke_add(const cui_plot_point *points, size_t count,
                                       cui_plot_stroke_options options,
                                       plot_stroke_polygons *polygons) {
    size_t segments = options.closed ? count : count - 1;
    cui_plot_point direction[64];
    for (size_t i = 0; i < segments; i++) {
        cui_plot_point a = points[i], b = points[(i + 1) % count];
        double dx = b.x - a.x, dy = b.y - a.y, length = hypot(dx, dy);
        if (length == 0)
            return CUI_PLOT_DEGENERATE;
        direction[i] = (cui_plot_point){dx / length, dy / length};
    }
    double half = options.width / 2;
    for (size_t i = 0; i < segments; i++) {
        cui_plot_point a = points[i], b = points[(i + 1) % count], d = direction[i];
        if (!options.closed && i == 0 && options.start_cap == CUI_PLOT_CAP_SQUARE) {
            a.x -= half * d.x;
            a.y -= half * d.y;
        }
        if (!options.closed && i + 1 == segments && options.end_cap == CUI_PLOT_CAP_SQUARE) {
            b.x += half * d.x;
            b.y += half * d.y;
        }
        cui_plot_point quad[4] = {{a.x - half * d.y, a.y + half * d.x},
                                  {a.x + half * d.y, a.y - half * d.x},
                                  {b.x + half * d.y, b.y - half * d.x},
                                  {b.x - half * d.y, b.y + half * d.x}};
        cui_plot_result result = plot_stroke_polygon(polygons, quad, 4);
        if (result != CUI_PLOT_OK)
            return result;
    }
    size_t first = options.closed ? 0 : 1, last = options.closed ? count : count - 1;
    for (size_t i = first; i < last; i++) {
        cui_plot_point p = points[i], prev = direction[(i + segments - 1) % segments],
                       next = direction[i % segments];
        double cross = prev.x * next.y - prev.y * next.x, dot = prev.x * next.x + prev.y * next.y;
        if (cross == 0 && dot > 0)
            continue;
        if (options.join == CUI_PLOT_JOIN_ROUND) {
            cui_plot_result result =
                plot_stroke_disk(polygons, p, half, options.round_segments_per_turn);
            if (result != CUI_PLOT_OK)
                return result;
            continue;
        }
        if (cross == 0)
            continue; /* Reversal: segment union supplies the bevel/butt seam. */
        double sign = cross > 0 ? -1 : 1;
        cui_plot_point n0 = {-sign * prev.y, sign * prev.x}, n1 = {-sign * next.y, sign * next.x};
        cui_plot_point join[5] = {p, {p.x + half * n0.x, p.y + half * n0.y}};
        size_t join_count = 2;
        double denominator = 1 + dot;
        cui_plot_point m = {denominator > 0 ? (n0.x + n1.x) / denominator : 0,
                            denominator > 0 ? (n0.y + n1.y) / denominator : 0};
        double length = denominator > 0 ? hypot(m.x, m.y) : INFINITY;
        if (options.join != CUI_PLOT_JOIN_BEVEL && length <= 2 * options.miter_limit) {
            join[join_count++] = (cui_plot_point){p.x + half * m.x, p.y + half * m.y};
        } else if (options.join == CUI_PLOT_JOIN_MITER_CLIP && denominator > 0 &&
                   isfinite(length)) {
            cui_plot_point unit = {m.x / length, m.y / length};
            double projection = unit.x * n0.x + unit.y * n0.y;
            /* Clip against a line normal to the bisector at limit*half_width,
             * reproducing pinned Lyon's clip-distance convention. */
            double target = options.miter_limit;
            double t0 = (target - projection) / (unit.x * prev.x + unit.y * prev.y);
            double t1 = (target - projection) / (unit.x * next.x + unit.y * next.y);
            join[join_count++] = (cui_plot_point){p.x + half * (n0.x + t0 * prev.x),
                                                  p.y + half * (n0.y + t0 * prev.y)};
            join[join_count++] = (cui_plot_point){p.x + half * (n1.x + t1 * next.x),
                                                  p.y + half * (n1.y + t1 * next.y)};
        }
        join[join_count++] = (cui_plot_point){p.x + half * n1.x, p.y + half * n1.y};
        cui_plot_result result = plot_stroke_polygon(polygons, join, join_count);
        if (result != CUI_PLOT_OK)
            return result;
    }
    if (!options.closed) {
        for (size_t end = 0; end < 2; end++) {
            cui_plot_line_cap cap = end ? options.end_cap : options.start_cap;
            if (cap != CUI_PLOT_CAP_ROUND)
                continue;
            cui_plot_result result = plot_stroke_disk(polygons, points[end ? count - 1 : 0], half,
                                                      options.round_segments_per_turn);
            if (result != CUI_PLOT_OK)
                return result;
        }
    }
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_stroke_mesh_ex(const cui_plot_point *points, size_t count,
                                        cui_plot_stroke_options options,
                                        cui_plot_fill_workspace *workspace,
                                        cui_plot_mesh_vertex *out, size_t capacity,
                                        size_t *required, cui_plot_rect *bounds) {
    if (!points || !workspace || !required || !bounds || count < 2 || count > 64 ||
        (capacity && !out) || !isfinite(options.width) || options.width < 0 ||
        options.width > 1024 || !isfinite(options.miter_limit) || options.miter_limit < 1 ||
        options.miter_limit > 64 || options.start_cap < CUI_PLOT_CAP_BUTT ||
        options.start_cap > CUI_PLOT_CAP_ROUND || options.end_cap < CUI_PLOT_CAP_BUTT ||
        options.end_cap > CUI_PLOT_CAP_ROUND || options.join < CUI_PLOT_JOIN_MITER ||
        options.join > CUI_PLOT_JOIN_MITER_CLIP || options.round_segments_per_turn < 4 ||
        options.round_segments_per_turn > 64)
        return CUI_PLOT_INVALID;
    for (size_t i = 0; i < count; i++)
        if (!mesh_point_valid(points[i]))
            return CUI_PLOT_INVALID;
    if (options.closed && points[0].x == points[count - 1].x && points[0].y == points[count - 1].y)
        --count;
    if (options.closed && count < 3)
        return CUI_PLOT_INVALID;
    size_t segments = options.closed ? count : count - 1;
    for (size_t i = 0; i < segments; i++) {
        cui_plot_point a = points[i], b = points[(i + 1) % count];
        double dx = b.x - a.x, dy = b.y - a.y, length = hypot(dx, dy);
        if (length == 0)
            return CUI_PLOT_DEGENERATE;
    }
    if (options.width == 0) {
        *required = 0;
        *bounds = (cui_plot_rect){0};
        return CUI_PLOT_EMPTY;
    }
    plot_stroke_polygons polygons = {0};
    cui_plot_result result = plot_stroke_add(points, count, options, &polygons);
    if (result != CUI_PLOT_OK)
        return result;
    if (!polygons.contour_count) {
        *required = 0;
        *bounds = (cui_plot_rect){0};
        return CUI_PLOT_EMPTY;
    }
    return cui_plot_fill_mesh_contours(polygons.contours, polygons.contour_count,
                                       CUI_PLOT_FILL_NON_ZERO, workspace, out, capacity, required,
                                       bounds);
}

cui_plot_result cui_plot_stroke_mesh_dashed(const cui_plot_point *points, size_t count,
                                            cui_plot_stroke_options options, const double *dashes,
                                            size_t dash_count, cui_plot_fill_workspace *workspace,
                                            cui_plot_mesh_vertex *out, size_t capacity,
                                            size_t *required, cui_plot_rect *bounds) {
    if (!isfinite(options.width) || options.width < 0 || options.width > 1024 || !dashes ||
        !dash_count || dash_count > 64 || !required || !bounds || (capacity && !out))
        return CUI_PLOT_INVALID;
    for (size_t i = 0; i < dash_count; i++)
        if (!isfinite(dashes[i]) || dashes[i] <= 0)
            return CUI_PLOT_INVALID;
    cui_plot_stroke_options check = options;
    check.width = 0;
    size_t ignored;
    cui_plot_rect ignored_bounds;
    cui_plot_result result = cui_plot_stroke_mesh_ex(points, count, check, workspace, NULL, 0,
                                                     &ignored, &ignored_bounds);
    if (result != CUI_PLOT_EMPTY)
        return result;
    if (options.width == 0) {
        *required = 0;
        *bounds = (cui_plot_rect){0};
        return CUI_PLOT_EMPTY;
    }
    if (options.closed && points[0].x == points[count - 1].x && points[0].y == points[count - 1].y)
        --count;
    size_t segments = options.closed ? count : count - 1,
           pattern_count = dash_count * (dash_count % 2 ? 2 : 1);
    size_t dash_index = 0, run_count = 0, steps = 0;
    double remaining = dashes[0];
    cui_plot_point run[64];
    plot_stroke_polygons polygons = {0};
    cui_plot_stroke_options run_options = options;
    run_options.closed = false;
    for (size_t i = 0; i < segments; i++) {
        cui_plot_point a = points[i], b = points[(i + 1) % count];
        double dx = b.x - a.x, dy = b.y - a.y, length = hypot(dx, dy), position = 0;
        while (position < length) {
            if (++steps > 1024)
                return CUI_PLOT_CAPACITY;
            double distance = fmin(remaining, length - position), next = position + distance;
            if (!(next > position))
                return CUI_PLOT_DEGENERATE;
            bool drawing = dash_index % 2 == 0;
            cui_plot_point start = position == 0 ? a
                                                 : (cui_plot_point){a.x + dx * (position / length),
                                                                    a.y + dy * (position / length)};
            cui_plot_point end = next == length ? b
                                                : (cui_plot_point){a.x + dx * (next / length),
                                                                   a.y + dy * (next / length)};
            if (drawing) {
                if (!run_count)
                    run[run_count++] = start;
                if (run_count == 64)
                    return CUI_PLOT_CAPACITY;
                run[run_count++] = end;
            }
            position = next;
            remaining -= distance;
            if (remaining == 0) {
                if (drawing) {
                    result = plot_stroke_add(run, run_count, run_options, &polygons);
                    if (result != CUI_PLOT_OK)
                        return result;
                    run_count = 0;
                }
                dash_index = (dash_index + 1) % pattern_count;
                remaining = dashes[dash_index % dash_count];
            }
        }
    }
    if (run_count) {
        result = plot_stroke_add(run, run_count, run_options, &polygons);
        if (result != CUI_PLOT_OK)
            return result;
    }
    if (!polygons.contour_count) {
        *required = 0;
        *bounds = (cui_plot_rect){0};
        return CUI_PLOT_EMPTY;
    }
    return cui_plot_fill_mesh_contours(polygons.contours, polygons.contour_count,
                                       CUI_PLOT_FILL_NON_ZERO, workspace, out, capacity, required,
                                       bounds);
}
