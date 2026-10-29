/* SPDX-License-Identifier: MIT */
#ifndef CUI_MESH_CPU_PRIVATE_H
#define CUI_MESH_CPU_PRIVATE_H
#include "cui/cui.h"
#include <math.h>
/* Private synchronous raster traversal. Caller validates paint with
 * cui_mesh_valid, supplies a finite clip and target dimensions <= INT_MAX,
 * supplies a nonnull emitter, and keeps paint/vertices/userdata alive through return. No ownership
 * retained. Emits in triangle order; destination blending belongs to the caller. */
typedef void (*cui_mesh_cpu_emit)(void *userdata, int x, int y, cui_color color);
/* Screen-space edge equations use double throughout, including clipping before
 * integer conversion. Positive orientation has its interior on the left. */
typedef struct {
    double x, y, s, t;
} cui_mesh_cpu_vertex;
static inline double cui_mesh_cpu_edge(cui_mesh_cpu_vertex a, cui_mesh_cpu_vertex b, double x,
                                       double y) {
    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}
static inline bool cui_mesh_cpu_top_left(cui_mesh_cpu_vertex a, cui_mesh_cpu_vertex b) {
    return b.y < a.y || (b.y == a.y && b.x > a.x);
}
static inline unsigned char cui_mesh_cpu_channel(double premultiplied, double alpha) {
    double linear = alpha > 0 ? premultiplied / alpha : 0;
    linear = fmax(0, fmin(1, linear));
    double encoded = linear <= .0031308 ? 12.92 * linear : 1.055 * pow(linear, 1 / 2.4) - .055;
    return (unsigned char)lround(encoded * 255);
}
/* Device-space triangle entry point: XY is already projected in double;
 * ST remains unchanged. Local copy permits winding normalization. */
static inline void cui_mesh_cpu_triangle(cui_mesh_cpu_vertex v[3], cui_color color, double alpha,
                                         unsigned width, unsigned height, cui_rect clip,
                                         cui_mesh_cpu_emit emit, void *userdata) {
    double area = cui_mesh_cpu_edge(v[0], v[1], v[2].x, v[2].y);
    if (area == 0)
        return;
    if (area < 0) {
        cui_mesh_cpu_vertex swap = v[1];
        v[1] = v[2];
        v[2] = swap;
        area = -area;
    }
    double min_x = fmax(0, fmax(clip.x, fmin(v[0].x, fmin(v[1].x, v[2].x))));
    double min_y = fmax(0, fmax(clip.y, fmin(v[0].y, fmin(v[1].y, v[2].y))));
    double max_x = fmin(width, fmin((double)clip.x + clip.w, fmax(v[0].x, fmax(v[1].x, v[2].x))));
    double max_y = fmin(height, fmin((double)clip.y + clip.h, fmax(v[0].y, fmax(v[1].y, v[2].y))));
    if (min_x >= max_x || min_y >= max_y)
        return;
    double sx =
        ((v[1].s - v[0].s) * (v[2].y - v[0].y) - (v[2].s - v[0].s) * (v[1].y - v[0].y)) / area;
    double sy =
        ((v[1].x - v[0].x) * (v[2].s - v[0].s) - (v[2].x - v[0].x) * (v[1].s - v[0].s)) / area;
    double tx =
        ((v[1].t - v[0].t) * (v[2].y - v[0].y) - (v[2].t - v[0].t) * (v[1].y - v[0].y)) / area;
    double ty =
        ((v[1].x - v[0].x) * (v[2].t - v[0].t) - (v[2].x - v[0].x) * (v[1].t - v[0].t)) / area;
    bool solid = hypot(sx, sy) < .001;
    for (int y = (int)floor(min_y); y < (int)ceil(max_y); y++)
        for (int x = (int)floor(min_x); x < (int)ceil(max_x); x++) {
            double e[3] = {cui_mesh_cpu_edge(v[1], v[2], x + .5, y + .5),
                           cui_mesh_cpu_edge(v[2], v[0], x + .5, y + .5),
                           cui_mesh_cpu_edge(v[0], v[1], x + .5, y + .5)};
            bool inside = true;
            for (size_t j = 0; j < 3; j++)
                if (e[j] < 0 ||
                    (e[j] == 0 && !cui_mesh_cpu_top_left(v[(j + 1) % 3], v[(j + 2) % 3])))
                    inside = false;
            if (!inside)
                continue;
            double coverage = 1;
            if (!solid) {
                double ss =
                    (e[0] / area) * v[0].s + (e[1] / area) * v[1].s + (e[2] / area) * v[2].s;
                double tt =
                    (e[0] / area) * v[0].t + (e[1] / area) * v[1].t + (e[2] / area) * v[2].t;
                double f = ss * ss - tt, gradient = hypot(2 * ss * sx - tx, 2 * ss * sy - ty);
                /* Deterministic extension at an undefined zero-gradient distance. */
                coverage = gradient > 0 ? fmax(0, fmin(1, .5 - f / gradient))
                                        : (f < 0   ? 1
                                           : f > 0 ? 0
                                                   : .5);
            }
            color.a = (unsigned char)lround(alpha * coverage * 255);
            emit(userdata, x, y, color);
        }
}
static inline void cui_mesh_cpu_raster(const cui_mesh_paint *p, unsigned width, unsigned height,
                                       cui_rect clip, cui_mesh_cpu_emit emit, void *userdata) {
    cui_color color = {cui_mesh_cpu_channel(p->color.r, p->color.a),
                       cui_mesh_cpu_channel(p->color.g, p->color.a),
                       cui_mesh_cpu_channel(p->color.b, p->color.a), 0};
    for (size_t i = 0; i < p->vertex_count; i += 3) {
        cui_mesh_cpu_vertex v[3];
        for (size_t j = 0; j < 3; j++) {
            const cui_mesh_vertex *in = &p->vertices[i + j];
            v[j] = (cui_mesh_cpu_vertex){p->origin_x + (double)p->scale * in->x,
                                         p->origin_y + (double)p->scale * in->y, in->s, in->t};
        }
        cui_mesh_cpu_triangle(v, color, p->color.a, width, height, clip, emit, userdata);
    }
}
#endif
