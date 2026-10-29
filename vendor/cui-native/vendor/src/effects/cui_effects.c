/* SPDX-License-Identifier: MIT */
#include "cui/cui_effects.h"
#include "../core/cui_drop_shadow_bounds_internal.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
static bool effects_extent(unsigned w, unsigned h, size_t stride, size_t *extent) {
    if (!w || !h || SIZE_MAX / w < 4 || stride < (size_t)w * 4 ||
        (size_t)(h - 1) > (SIZE_MAX - (size_t)w * 4) / stride)
        return false;
    *extent = (size_t)(h - 1) * stride + (size_t)w * 4;
    return true;
}
static bool effects_buffer(cui_effect_buffer b, size_t *extent) {
    return b.pixels && effects_extent(b.width, b.height, b.stride, extent) && b.size >= *extent;
}
static bool effects_overlap(const void *a, size_t an, const void *b, size_t bn) {
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return x <= y ? y - x < an : x - y < bn;
}
static bool effects_pair(cui_bitmap s, cui_effect_buffer d, size_t *sn, size_t *dn) {
    if (!s.pixels || !effects_extent(s.width, s.height, s.stride, sn) || !effects_buffer(d, dn) ||
        s.width != d.width || s.height != d.height)
        return false;
    return (s.pixels == d.pixels && s.stride == d.stride) ||
           !effects_overlap(s.pixels, *sn, d.pixels, *dn);
}
static unsigned char effects_byte(double v) {
    return (unsigned char)(v <= 0 ? 0 : v >= 255 ? 255 : floor(v + 0.5));
}
bool cui_effect_linear_gradient(cui_effect_buffer d, const cui_gradient_stop *s, size_t n,
                                float angle) {
    size_t extent;
    if (!effects_buffer(d, &extent) || !s || !n || n > SIZE_MAX / sizeof(*s) || !isfinite(angle) ||
        effects_overlap(s, n * sizeof(*s), d.pixels, extent))
        return false;
    for (size_t i = 0; i < n; ++i)
        if (!isfinite(s[i].position) || s[i].position < 0 || s[i].position > 1 ||
            (i && s[i].position <= s[i - 1].position))
            return false;
    double radians = fmod((double)angle, 360.0) * 0.017453292519943295;
    double dx = cos(radians), dy = sin(radians);
    if (fabs(dx) < 1e-12)
        dx = 0;
    if (fabs(dy) < 1e-12)
        dy = 0;
    double xx = dx * (d.width - 1), yy = dy * (d.height - 1);
    double low = fmin(0, xx) + fmin(0, yy), span = fabs(xx) + fabs(yy);
    for (unsigned y = 0; y < d.height; ++y)
        for (unsigned x = 0; x < d.width; ++x) {
            double t = span ? (x * dx + y * dy - low) / span : 0;
            size_t i = 0;
            while (i + 1 < n && t > s[i + 1].position)
                ++i;
            cui_color a = s[i].color, b = a;
            double f = 0;
            if (i + 1 < n && t > s[i].position) {
                b = s[i + 1].color;
                f = (t - s[i].position) / (s[i + 1].position - s[i].position);
            }
            double alpha = a.a * (1 - f) + b.a * f;
            unsigned char *p = d.pixels + y * d.stride + (size_t)x * 4;
            p[0] = effects_byte(alpha ? (a.r * a.a * (1 - f) + b.r * b.a * f) / alpha : 0);
            p[1] = effects_byte(alpha ? (a.g * a.a * (1 - f) + b.g * b.a * f) / alpha : 0);
            p[2] = effects_byte(alpha ? (a.b * a.a * (1 - f) + b.b * b.a * f) / alpha : 0);
            p[3] = effects_byte(alpha);
        }
    return true;
}
bool cui_effect_opacity(cui_bitmap s, cui_effect_buffer d, float opacity) {
    size_t sn, dn;
    if (!isfinite(opacity) || opacity < 0 || opacity > 1 || !effects_pair(s, d, &sn, &dn))
        return false;
    for (unsigned y = 0; y < s.height; ++y)
        for (unsigned x = 0; x < s.width; ++x) {
            const unsigned char *p = s.pixels + y * s.stride + (size_t)x * 4;
            unsigned char *q = d.pixels + y * d.stride + (size_t)x * 4;
            q[0] = p[0];
            q[1] = p[1];
            q[2] = p[2];
            q[3] = effects_byte(p[3] * opacity);
        }
    return true;
}
size_t cui_effect_blur_scratch_floats(unsigned w, unsigned h) {
    if (!w || !h || (size_t)w > SIZE_MAX / h / 4 / sizeof(float))
        return 0;
    return (size_t)w * h * 4;
}
static double effects_channel(const unsigned char *p, unsigned c) {
    return c == 3 ? p[3] : p[c] * (double)p[3] / 255;
}
static unsigned effects_clamp_index(int64_t i, unsigned n) {
    return i < 0 ? 0 : (uint64_t)i >= n ? n - 1 : (unsigned)i;
}
bool cui_effect_blur(cui_bitmap s, cui_effect_buffer d, unsigned r, float *tmp, size_t count) {
    size_t sn, dn, need = cui_effect_blur_scratch_floats(s.width, s.height);
    if (!effects_pair(s, d, &sn, &dn) || !need || !tmp || (uintptr_t)tmp % _Alignof(float) != 0 ||
        count < need || effects_overlap(tmp, need * sizeof(float), s.pixels, sn) ||
        effects_overlap(tmp, need * sizeof(float), d.pixels, dn))
        return false;
    if (!r) {
        for (unsigned y = 0; y < s.height; ++y)
            memmove(d.pixels + y * d.stride, s.pixels + y * s.stride, (size_t)s.width * 4);
        return true;
    }
    double divisor = 2.0 * r + 1;
    for (unsigned y = 0; y < s.height; ++y)
        for (unsigned c = 0; c < 4; ++c) {
            const unsigned char *row = s.pixels + y * s.stride;
            double sum = effects_channel(row, c) * ((double)r + 1);
            unsigned last = r < s.width - 1 ? r : s.width - 1;
            for (unsigned i = 1; i <= last; ++i)
                sum += effects_channel(row + (size_t)i * 4, c);
            if (r > last)
                sum += effects_channel(row + (size_t)(s.width - 1) * 4, c) * (r - last);
            for (unsigned x = 0; x < s.width; ++x) {
                tmp[((size_t)y * s.width + x) * 4 + c] = (float)(sum / divisor);
                unsigned remove = effects_clamp_index((int64_t)x - r, s.width);
                unsigned add = effects_clamp_index((int64_t)x + r + 1, s.width);
                sum += effects_channel(row + (size_t)add * 4, c) -
                       effects_channel(row + (size_t)remove * 4, c);
            }
        }
    for (unsigned x = 0; x < s.width; ++x) {
        double sum[4] = {0};
        unsigned last = r < s.height - 1 ? r : s.height - 1;
        for (unsigned c = 0; c < 4; ++c) {
            sum[c] = tmp[(size_t)x * 4 + c] * ((double)r + 1);
            for (unsigned i = 1; i <= last; ++i)
                sum[c] += tmp[((size_t)i * s.width + x) * 4 + c];
            if (r > last)
                sum[c] += tmp[((size_t)(s.height - 1) * s.width + x) * 4 + c] * (r - last);
        }
        for (unsigned y = 0; y < s.height; ++y) {
            unsigned char *p = d.pixels + y * d.stride + (size_t)x * 4;
            double alpha = sum[3] / divisor;
            for (unsigned c = 0; c < 3; ++c)
                p[c] = effects_byte(alpha > 0 ? sum[c] / divisor * 255 / alpha : 0);
            p[3] = effects_byte(alpha);
            unsigned remove = effects_clamp_index((int64_t)y - r, s.height);
            unsigned add = effects_clamp_index((int64_t)y + r + 1, s.height);
            for (unsigned c = 0; c < 4; ++c)
                sum[c] += tmp[((size_t)add * s.width + x) * 4 + c] -
                          tmp[((size_t)remove * s.width + x) * 4 + c];
        }
    }
    return true;
}

static float shadow_clamp(float v, float lo, float hi) {
    return fminf(hi, fmaxf(lo, v));
}
static bool shadow_geometry(cui_drop_shadow_paint p, float scale, float ox, float oy,
                            cui_rect *bounds, cui_corner_radii *radii) {
    cui_rect unused;
    if (!cui_drop_shadow_bounds_checked(p, &unused) || !isfinite(scale) || scale <= 0 ||
        scale > 64 || scale < 1.0f / 65536 ||
        (p.layer.sigma > 0 && p.layer.sigma * scale < 4 * sqrtf(FLT_MIN)) || !isfinite(ox) ||
        !isfinite(oy) || fabsf(ox) > 1048576 || fabsf(oy) > 1048576)
        return false;
    float limit = fminf(p.caster.w, p.caster.h) / 2;
    *radii = (cui_corner_radii){
        fminf(limit, p.radii.top_left) * scale, fminf(limit, p.radii.top_right) * scale,
        fminf(limit, p.radii.bottom_left) * scale, fminf(limit, p.radii.bottom_right) * scale};
    cui_rect logical = cui_drop_shadow_logical_bounds(p);
    float x = logical.x * scale + ox, y = logical.y * scale + oy;
    float far_x = (logical.x + logical.w) * scale + ox;
    float far_y = (logical.y + logical.h) * scale + oy;
    float near_x = floorf(x), near_y = floorf(y);
    *bounds = (cui_rect){near_x, near_y, fmaxf(ceilf(far_x), near_x) - near_x,
                         fmaxf(ceilf(far_y), near_y) - near_y};
    return isfinite(bounds->w) && isfinite(bounds->h);
}
bool cui_effect_drop_shadow_query(cui_drop_shadow_paint p, float scale, float ox, float oy,
                                  cui_drop_shadow_raster *out) {
    cui_rect b;
    cui_corner_radii r;
    if (!out || !shadow_geometry(p, scale, ox, oy, &b, &r))
        return false;
    cui_drop_shadow_raster next = {.covered_layer = b};
    if (b.w > 0 && b.h > 0) {
        float margin = 3 * (p.layer.sigma * scale);
        float x = floorf(b.x - margin), y = floorf(b.y - margin);
        float w = ceilf(b.x + b.w + margin) - x, h = ceilf(b.y + b.h + margin) - y;
        if (w > 16384 || h > 16384 || w * h > 16777216)
            return false;
        next.width = (unsigned)w;
        next.height = (unsigned)h;
        next.bytes = (size_t)next.width * next.height * 4;
        next.bounds = (cui_rect){(x - ox) / scale, (y - oy) / scale, w / scale, h / scale};
    }
    *out = next;
    return true;
}
static float shadow_erf(float v) {
    float a = fabsf(v), t = 1 + (.278393f + (.230389f + (.000972f + .078108f * a) * a) * a) * a;
    float s = v < 0 ? -1.0f : (v > 0 ? 1.0f : 0.0f);
    t *= t;
    return s - s / (t * t);
}
static float shadow_sample(cui_drop_shadow_paint p, float scale, cui_rect b, cui_corner_radii r,
                           float x, float y) {
    if (b.w <= 0 || b.h <= 0)
        return 0;
    float sigma = p.layer.sigma * scale, margin = 3 * sigma;
    if (x < b.x - margin || y < b.y - margin || x >= b.x + b.w + margin || y >= b.y + b.h + margin)
        return 0;
    float hx = b.w / 2, hy = b.h / 2, px = x - b.x - hx, py = y - b.y - hy;
    float corner =
        px < 0 ? (py < 0 ? r.top_left : r.bottom_left) : (py < 0 ? r.top_right : r.bottom_right);
    if (sigma == 0) {
        float qx = fabsf(px) - hx + corner, qy = fabsf(py) - hy + corner;
        float d = sqrtf(fmaxf(qx, 0) * fmaxf(qx, 0) + fmaxf(qy, 0) * fmaxf(qy, 0)) +
                  fminf(fmaxf(qx, qy), 0) - corner;
        return shadow_clamp(.5f - d, 0, 1);
    }
    float low = py - hy, high = py + hy, start = shadow_clamp(-margin, low, high);
    float end = shadow_clamp(margin, low, high), step = (end - start) / 4,
          y_sample = start + step * .5f, alpha = 0;
    for (unsigned i = 0; i < 4; i++, y_sample += step) {
        float delta = fminf(hy - corner - fabsf(py - y_sample), 0);
        float curved = hx - corner + sqrtf(fmaxf(0, corner * corner - delta * delta));
        float factor = .7071067811865475244f / sigma;
        float integral =
            .5f * (shadow_erf((px + curved) * factor) - shadow_erf((px - curved) * factor));
        float gaussian =
            expf(-(y_sample * y_sample) / (2 * sigma * sigma)) / (2.5066282746310005f * sigma);
        alpha += integral * gaussian * step;
    }
    return shadow_clamp(alpha, 0, 1);
}
bool cui_effect_drop_shadow_sample(cui_drop_shadow_paint p, float scale, float ox, float oy,
                                   float x, float y, float *out) {
    cui_rect b;
    cui_corner_radii r;
    if (!out || !isfinite(x) || !isfinite(y) || !shadow_geometry(p, scale, ox, oy, &b, &r))
        return false;
    *out = shadow_sample(p, scale, b, r, x, y);
    return true;
}
bool cui_effect_drop_shadow_raster(cui_drop_shadow_paint p, float scale, float ox, float oy,
                                   cui_effect_buffer output) {
    cui_drop_shadow_raster q;
    cui_rect b;
    cui_corner_radii r;
    size_t extent;
    if (!cui_effect_drop_shadow_query(p, scale, ox, oy, &q) ||
        !shadow_geometry(p, scale, ox, oy, &b, &r))
        return false;
    if (!q.width)
        return output.width == 0 && output.height == 0;
    if (output.width != q.width || output.height != q.height || !effects_buffer(output, &extent))
        return false;
    float x = q.bounds.x * scale + ox, y = q.bounds.y * scale + oy;
    for (unsigned iy = 0; iy < q.height; iy++)
        for (unsigned ix = 0; ix < q.width; ix++) {
            unsigned char *pixel = output.pixels + (size_t)iy * output.stride + (size_t)ix * 4;
            pixel[0] = p.layer.color.r;
            pixel[1] = p.layer.color.g;
            pixel[2] = p.layer.color.b;
            pixel[3] = (unsigned char)lroundf(
                p.layer.color.a * shadow_sample(p, scale, b, r, x + ix + .5f, y + iy + .5f));
        }
    return true;
}

static bool inset_geometry(cui_inset_shadow_paint p, float scale, float ox, float oy,
                           cui_rect *hole, cui_rect *element, cui_corner_radii *hr,
                           cui_corner_radii *er) {
    cui_drop_shadow_paint h = p, e = p;
    h.layer.spread = -p.layer.spread;
    e.layer.offset_x = e.layer.offset_y = e.layer.spread = 0;
    if (!shadow_geometry(h, scale, ox, oy, hole, hr) ||
        !shadow_geometry(e, scale, ox, oy, element, er))
        return false;
    float limit = fminf(p.caster.w, p.caster.h) / 2;
    *hr = (cui_corner_radii){fmaxf(fminf(limit, p.radii.top_left) - p.layer.spread, 0) * scale,
                             fmaxf(fminf(limit, p.radii.top_right) - p.layer.spread, 0) * scale,
                             fmaxf(fminf(limit, p.radii.bottom_left) - p.layer.spread, 0) * scale,
                             fmaxf(fminf(limit, p.radii.bottom_right) - p.layer.spread, 0) * scale};
    return true;
}
bool cui_effect_inset_shadow_query(cui_inset_shadow_paint p, float scale, float ox, float oy,
                                   cui_inset_shadow_raster *out) {
    cui_rect h, e;
    cui_corner_radii hr, er;
    if (!out || !inset_geometry(p, scale, ox, oy, &h, &e, &hr, &er))
        return false;
    cui_inset_shadow_raster next = {.covered_hole = h, .covered_element = e};
    if (h.w > 0 && h.h > 0 && e.w > 0 && e.h > 0) {
        if (e.w > 16384 || e.h > 16384 || (size_t)e.w * (size_t)e.h > 16777216)
            return false;
        next.width = (unsigned)e.w;
        next.height = (unsigned)e.h;
        next.bytes = (size_t)next.width * next.height * 4;
        next.bounds = (cui_rect){(e.x - ox) / scale, (e.y - oy) / scale, e.w / scale, e.h / scale};
    }
    *out = next;
    return true;
}
static float inset_sdf(cui_rect b, cui_corner_radii r, float x, float y) {
    float px = x - (b.x + b.w / 2), py = y - (b.y + b.h / 2);
    float radius =
        px < 0 ? (py < 0 ? r.top_left : r.bottom_left) : (py < 0 ? r.top_right : r.bottom_right);
    float qx = fabsf(px) - b.w / 2 + radius, qy = fabsf(py) - b.h / 2 + radius;
    if (radius == 0)
        return fmaxf(qx, qy);
    return sqrtf(fmaxf(qx, 0) * fmaxf(qx, 0) + fmaxf(qy, 0) * fmaxf(qy, 0)) +
           fminf(fmaxf(qx, qy), 0) - radius;
}
static float inset_sample(cui_inset_shadow_paint p, float scale, cui_rect h, cui_rect e,
                          cui_corner_radii hr, cui_corner_radii er, float x, float y) {
    if (h.w <= 0 || h.h <= 0 || e.w <= 0 || e.h <= 0 || x < e.x || y < e.y || x >= e.x + e.w ||
        y >= e.y + e.h)
        return 0;
    float sigma = p.layer.sigma * scale, coverage;
    if (sigma == 0)
        coverage = shadow_clamp(.5f - inset_sdf(h, hr, x, y), 0, 1);
    else {
        float hx = h.w / 2, hy = h.h / 2, px = x - (h.x + hx), py = y - (h.y + hy);
        float corner = px < 0 ? (py < 0 ? hr.top_left : hr.bottom_left)
                              : (py < 0 ? hr.top_right : hr.bottom_right);
        float start = shadow_clamp(-3 * sigma, py - hy, py + hy),
              end = shadow_clamp(3 * sigma, py - hy, py + hy);
        float step = (end - start) / 4, ys = start + step * .5f;
        coverage = 0;
        for (unsigned i = 0; i < 4; i++, ys += step) {
            float delta = fminf(hy - corner - fabsf(py - ys), 0);
            float curved = hx - corner + sqrtf(fmaxf(0, corner * corner - delta * delta));
            float factor = .7071067811865475244f / sigma;
            float low = .5f + .5f * shadow_erf((px - curved) * factor);
            float high = .5f + .5f * shadow_erf((px + curved) * factor);
            float gaussian = expf(-(ys * ys) / (2 * sigma * sigma)) / (2.5066282746310005f * sigma);
            coverage += (high - low) * gaussian * step;
        }
    }
    return shadow_clamp((1 - coverage) * shadow_clamp(.5f - inset_sdf(e, er, x, y), 0, 1), 0, 1);
}
bool cui_effect_inset_shadow_sample(cui_inset_shadow_paint p, float scale, float ox, float oy,
                                    float x, float y, float *out) {
    cui_rect h, e;
    cui_corner_radii hr, er;
    if (!out || !isfinite(x) || !isfinite(y) || !inset_geometry(p, scale, ox, oy, &h, &e, &hr, &er))
        return false;
    *out = inset_sample(p, scale, h, e, hr, er, x, y);
    return true;
}
bool cui_effect_inset_shadow_raster(cui_inset_shadow_paint p, float scale, float ox, float oy,
                                    cui_effect_buffer output) {
    cui_inset_shadow_raster q;
    cui_rect h, e;
    cui_corner_radii hr, er;
    size_t extent;
    if (!cui_effect_inset_shadow_query(p, scale, ox, oy, &q) ||
        !inset_geometry(p, scale, ox, oy, &h, &e, &hr, &er))
        return false;
    if (!q.width)
        return output.width == 0 && output.height == 0;
    if (output.width != q.width || output.height != q.height || !effects_buffer(output, &extent))
        return false;
    for (unsigned y = 0; y < q.height; y++)
        for (unsigned x = 0; x < q.width; x++) {
            unsigned char *pixel = output.pixels + (size_t)y * output.stride + (size_t)x * 4;
            pixel[0] = p.layer.color.r;
            pixel[1] = p.layer.color.g;
            pixel[2] = p.layer.color.b;
            pixel[3] = (unsigned char)lroundf(p.layer.color.a * inset_sample(p, scale, h, e, hr, er,
                                                                             e.x + x + .5f,
                                                                             e.y + y + .5f));
        }
    return true;
}
