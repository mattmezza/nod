/* SPDX-License-Identifier: MIT */
#ifndef CUI_BACKEND_SHAPE_H
#define CUI_BACKEND_SHAPE_H
#include "cui/cui.h"
#include <math.h>
/* Backend-private geometry shared by raster and GPU adapters. */
static inline bool cui_shape_valid(const cui_draw_command *c) {
    cui_corner_radii r = c->corner_radii;
    cui_border_edges e = c->border_edges;
    return (!c->custom_corners || (isfinite(r.top_left) && isfinite(r.top_right) &&
                                   isfinite(r.bottom_left) && isfinite(r.bottom_right))) &&
           (!c->custom_border_edges ||
            (isfinite(e.left) && isfinite(e.right) && isfinite(e.top) && isfinite(e.bottom)));
}
static inline void cui_shape_values(const cui_draw_command *c, float radii[4], float edges[4]) {
    cui_corner_radii r = c->custom_corners
                             ? c->corner_radii
                             : (cui_corner_radii){c->radius, c->radius, c->radius, c->radius};
    cui_border_edges e = c->custom_border_edges
                             ? c->border_edges
                             : (cui_border_edges){c->stroke, c->stroke, c->stroke, c->stroke};
    float limit = fminf(c->bounds.w, c->bounds.h) * .5f;
    float values[4] = {r.top_left, r.top_right, r.bottom_left, r.bottom_right};
    float widths[4] = {e.left, e.right, e.top, e.bottom};
    for (unsigned i = 0; i < 4; i++) {
        radii[i] = fminf(fmaxf(values[i], 0), limit);
        edges[i] = fmaxf(widths[i], 0);
    }
}
static inline float cui_shape_arc(float rx, float ry, float y) {
    if (rx <= 0 || ry <= 0 || y >= ry)
        return 0;
    float q = (ry - y) / ry;
    return rx * (1 - sqrtf(fmaxf(0, 1 - q * q)));
}
/* Horizontal interval for an outer outline or its inset elliptical inner outline. */
static inline bool cui_shape_span(const cui_draw_command *c, const float r[4], const float e[4],
                                  bool inner, float y, float *left, float *right) {
    float l = inner ? e[0] : 0, rr = inner ? e[1] : 0;
    float t = inner ? e[2] : 0, b = inner ? e[3] : 0;
    float w = c->bounds.w - l - rr, h = c->bounds.h - t - b;
    y -= t;
    if (w <= 0 || h <= 0 || y < 0 || y >= h)
        return false;
    float lt = cui_shape_arc(fmaxf(0, r[0] - l), fmaxf(0, r[0] - t), y);
    float lb = cui_shape_arc(fmaxf(0, r[2] - l), fmaxf(0, r[2] - b), h - y);
    float rt = cui_shape_arc(fmaxf(0, r[1] - rr), fmaxf(0, r[1] - t), y);
    float rb = cui_shape_arc(fmaxf(0, r[3] - rr), fmaxf(0, r[3] - b), h - y);
    *left = l + fmaxf(lt, lb);
    *right = c->bounds.w - rr - fmaxf(rt, rb);
    return *right > *left;
}
#endif
