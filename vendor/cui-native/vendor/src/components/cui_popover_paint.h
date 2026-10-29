/* SPDX-License-Identifier: MIT */
/* Private fixed-size frame-copy payload. No contained borrowed pointers. */
#include <math.h>
#include <string.h>
typedef struct {
    cui_mesh_vertex fill[15], outline[60];
    size_t fill_count, outline_count;
    cui_rect join;
    cui_color background;
    cui_linear_color fill_color, outline_color;
} cui_popover_paint;
static float cui_popover_channel(unsigned char channel) {
    float c = channel / 255.0f;
    return c <= .04045f ? c / 12.92f : powf((c + .055f) / 1.055f, 2.4f);
}
static cui_linear_color cui_popover_linear(cui_color c) {
    float a = c.a / 255.0f;
    return (cui_linear_color){cui_popover_channel(c.r) * a, cui_popover_channel(c.g) * a,
                              cui_popover_channel(c.b) * a, a};
}
static cui_rect cui_popover_intersection(cui_rect a, cui_rect b) {
    float x = fmaxf(a.x, b.x), y = fmaxf(a.y, b.y);
    return (cui_rect){x, y, fmaxf(0, fminf(a.x + a.w, b.x + b.w) - x),
                      fmaxf(0, fminf(a.y + a.h, b.y + b.h) - y)};
}
/* A triangle clipped by four rectangle half-planes has at most seven vertices.
 * Fan triangulation therefore needs at most15 output vertices. */
static bool cui_popover_clip_triangle(cui_popover_point a, cui_popover_point b, cui_popover_point c,
                                      cui_rect viewport, cui_mesh_vertex *vertices, size_t capacity,
                                      size_t *used) {
    cui_popover_point polygon[8] = {a, b, c}, scratch[8];
    size_t count = 3;
    for (unsigned edge = 0; edge < 4 && count; ++edge) {
        size_t next = 0;
        double limit = edge == 0   ? viewport.x
                       : edge == 1 ? (double)viewport.x + viewport.w
                       : edge == 2 ? viewport.y
                                   : (double)viewport.y + viewport.h;
        for (size_t i = 0; i < count; ++i) {
            cui_popover_point from = polygon[i], to = polygon[(i + 1) % count];
            double f = edge < 2 ? from.x : from.y, t = edge < 2 ? to.x : to.y;
            bool inside_f = edge == 0 || edge == 2 ? f >= limit : f <= limit;
            bool inside_t = edge == 0 || edge == 2 ? t >= limit : t <= limit;
            if (inside_f != inside_t) {
                double weight = (limit - f) / (t - f);
                cui_popover_point crossing = {(float)(from.x + ((double)to.x - from.x) * weight),
                                              (float)(from.y + ((double)to.y - from.y) * weight)};
                if (next == 8)
                    return false;
                scratch[next++] = crossing;
            }
            if (inside_t) {
                if (next == 8)
                    return false;
                scratch[next++] = to;
            }
        }
        memcpy(polygon, scratch, next * sizeof(*polygon));
        count = next;
    }
    if (count < 3)
        return true;
    if ((count - 2) * 3 > capacity - *used)
        return false;
    for (size_t i = 1; i + 1 < count; ++i) {
        cui_popover_point points[3] = {polygon[0], polygon[i], polygon[i + 1]};
        for (unsigned j = 0; j < 3; ++j)
            vertices[(*used)++] = (cui_mesh_vertex){points[j].x, points[j].y, 0, 1};
    }
    return true;
}
static bool cui_popover_paint_build(cui_rect surface, cui_rect trigger, cui_rect viewport,
                                    cui_popover_position p, const cui_theme *theme,
                                    cui_popover_style style, cui_surface_options options,
                                    cui_popover_paint *out) {
    float rem = p.reference_rem == 0 ? 16 : p.reference_rem;
    cui_popover_arrow_geometry geometry;
    if (!cui_popover_arrow_resolve(surface, trigger, p.anchor, rem * .375f, theme->radius,
                                   &geometry))
        return false;
    *out = (cui_popover_paint){0};
    out->background = options.popover_appearance == CUI_POPOVER_APPEARANCE_NONE &&
                              !(options.popover_refinement.flags & CUI_POPOVER_REFINE_BACKGROUND)
                          ? theme->popover
                          : style.background;
    out->fill_color = cui_popover_linear(out->background);
    out->outline_color =
        cui_popover_linear((cui_color){theme->text.r, theme->text.g, theme->text.b, 26});
    out->join = cui_popover_intersection(geometry.join, viewport);
    cui_popover_point *v = geometry.points;
    if (!cui_popover_clip_triangle(v[0], v[1], v[2], viewport, out->fill, 15, &out->fill_count))
        return false;
    if (options.popover_appearance == CUI_POPOVER_APPEARANCE_NONE)
        return true;
    double dx1 = (double)v[1].x - v[0].x, dy1 = (double)v[1].y - v[0].y;
    double dx2 = (double)v[2].x - v[1].x, dy2 = (double)v[2].y - v[1].y;
    double length1 = hypot(dx1, dy1), length2 = hypot(dx2, dy2);
    if (length1 == 0 || length2 == 0)
        return true;
    double nx1 = -dy1 / length1 * .5, ny1 = dx1 / length1 * .5;
    double nx2 = -dy2 / length2 * .5, ny2 = dx2 / length2 * .5;
    double denom = 1 + 4 * (nx1 * nx2 + ny1 * ny2);
    double mx = denom > 0 ? (nx1 + nx2) / denom : 0;
    double my = denom > 0 ? (ny1 + ny2) / denom : 0;
    double miter = hypot(mx, my);
    if (miter > 2) {
        mx *= 2 / miter;
        my *= 2 / miter;
    }
    cui_popover_point strip[6] = {{(float)(v[0].x + nx1), (float)(v[0].y + ny1)},
                                  {(float)(v[0].x - nx1), (float)(v[0].y - ny1)},
                                  {(float)(v[1].x + mx), (float)(v[1].y + my)},
                                  {(float)(v[1].x - mx), (float)(v[1].y - my)},
                                  {(float)(v[2].x + nx2), (float)(v[2].y + ny2)},
                                  {(float)(v[2].x - nx2), (float)(v[2].y - ny2)}};
    const unsigned indices[12] = {0, 1, 2, 1, 3, 2, 2, 3, 4, 3, 5, 4};
    for (unsigned i = 0; i < 12; i += 3)
        if (!cui_popover_clip_triangle(strip[indices[i]], strip[indices[i + 1]],
                                       strip[indices[i + 2]], viewport, out->outline, 60,
                                       &out->outline_count))
            return false;
    return true;
}
static bool cui_popover_paint_callback(const cui_prepaint_info *info, const void *copied,
                                       cui_prepaint_writer *writer) {
    const cui_popover_paint *p = copied;
    (void)info; /* Input eligibility does not suppress visual decoration. */
    if (p->fill_count) {
        cui_draw_command fill = {.type = CUI_DRAW_MESH,
                                 .mesh = {.vertices = p->fill,
                                          .vertex_count = p->fill_count,
                                          .scale = 1,
                                          .color = p->fill_color}};
        if (!cui_prepaint_emit(writer, &fill, false))
            return false;
    }
    if (p->join.w > 0 && p->join.h > 0) {
        cui_draw_command join = {.type = CUI_DRAW_RECT, .bounds = p->join, .color = p->background};
        if (!cui_prepaint_emit(writer, &join, false))
            return false;
    }
    if (p->outline_count) {
        cui_draw_command outline = {.type = CUI_DRAW_MESH,
                                    .mesh = {.vertices = p->outline,
                                             .vertex_count = p->outline_count,
                                             .scale = 1,
                                             .color = p->outline_color}};
        if (!cui_prepaint_emit(writer, &outline, false))
            return false;
    }
    return true;
}
