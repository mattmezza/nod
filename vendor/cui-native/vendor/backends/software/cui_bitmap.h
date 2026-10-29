/* SPDX-License-Identifier: MIT */
#ifndef CUI_BITMAP_INTERNAL_H
#define CUI_BITMAP_INTERNAL_H
#include "cui_shape.h"
#include <stdlib.h>
#include <string.h>
/* Adapter-local immutable raster key: opacity and placement do not invalidate
 * pixels. A revised source or a changed crop/shape does. */
typedef struct {
    cui_bitmap bitmap;
    cui_rect source;
    cui_corner_radii radii;
    unsigned width, height;
    bool grayscale;
    cui_color tint;
} cui_bitmap_key;
static bool cui_bitmap_key_make(const cui_draw_command *c, cui_bitmap_key *key) {
    const cui_bitmap_paint *p = &c->bitmap;
    cui_rect s = p->source;
    if (!cui_bitmap_valid(p->bitmap) || !isfinite(p->opacity) || p->opacity < 0 || p->opacity > 1 ||
        !isfinite(s.x) || !isfinite(s.y) || !isfinite(s.w) || !isfinite(s.h) || s.x < 0 ||
        s.y < 0 || s.w <= 0 || s.h <= 0 || s.x + s.w > p->bitmap.width ||
        s.y + s.h > p->bitmap.height || !isfinite(c->bounds.w) || !isfinite(c->bounds.h) ||
        c->bounds.w <= 0 || c->bounds.h <= 0 ||
        ((c->type != CUI_DRAW_DROP_SHADOW && c->type != CUI_DRAW_INSET_SHADOW) &&
         (c->bounds.w > 16384 || c->bounds.h > 16384)))
        return false;
    float radii[4], edges[4];
    cui_shape_values(c, radii, edges);
    *key = (cui_bitmap_key){
        .bitmap = p->bitmap,
        .source = s,
        .radii = {radii[0], radii[1], radii[2], radii[3]},
        .width = (c->type == CUI_DRAW_DROP_SHADOW || c->type == CUI_DRAW_INSET_SHADOW)
                     ? (unsigned)p->source.w
                     : (unsigned)ceilf(c->bounds.w),
        .height = (c->type == CUI_DRAW_DROP_SHADOW || c->type == CUI_DRAW_INSET_SHADOW)
                      ? (unsigned)p->source.h
                      : (unsigned)ceilf(c->bounds.h),
        .grayscale = p->grayscale,
        .tint = p->custom_tint ? p->tint : (cui_color){255, 255, 255, 255}};
    return true;
}
static bool cui_bitmap_key_equal(const cui_bitmap_key *a, const cui_bitmap_key *b) {
    return a->bitmap.pixels == b->bitmap.pixels && a->bitmap.width == b->bitmap.width &&
           a->bitmap.height == b->bitmap.height && a->bitmap.stride == b->bitmap.stride &&
           a->bitmap.revision == b->bitmap.revision && a->width == b->width &&
           a->height == b->height && a->grayscale == b->grayscale && a->tint.r == b->tint.r &&
           a->tint.g == b->tint.g && a->tint.b == b->tint.b && a->tint.a == b->tint.a &&
           a->source.x == b->source.x && a->source.y == b->source.y && a->source.w == b->source.w &&
           a->source.h == b->source.h && a->radii.top_left == b->radii.top_left &&
           a->radii.top_right == b->radii.top_right &&
           a->radii.bottom_left == b->radii.bottom_left &&
           a->radii.bottom_right == b->radii.bottom_right;
}
static unsigned char *cui_bitmap_raster(const cui_bitmap_key *key, size_t budget) {
    size_t bytes = (size_t)key->width * key->height * 4;
    if (bytes > budget)
        return NULL;
    unsigned char *rgba = calloc(bytes, 1);
    if (!rgba)
        return NULL;
    cui_draw_command shape = {.bounds = {0, 0, (float)key->width, (float)key->height},
                              .custom_corners = true,
                              .corner_radii = key->radii};
    float radii[4], edges[4];
    cui_shape_values(&shape, radii, edges);
    for (unsigned y = 0; y < key->height; y++) {
        float left, right;
        if (!cui_shape_span(&shape, radii, edges, false, y + .5f, &left, &right))
            continue;
        unsigned sy =
            (unsigned)fminf(key->bitmap.height - 1,
                            floorf(key->source.y + (y + .5f) / key->height * key->source.h));
        for (unsigned x = 0; x < key->width; x++) {
            if (x + .5f < left || x + .5f >= right)
                continue;
            unsigned sx =
                (unsigned)fminf(key->bitmap.width - 1,
                                floorf(key->source.x + (x + .5f) / key->width * key->source.w));
            const unsigned char *src = key->bitmap.pixels + sy * key->bitmap.stride + sx * 4;
            unsigned char *dst = rgba + ((size_t)y * key->width + x) * 4;
            memcpy(dst, src, 4);
            if (key->grayscale)
                dst[0] = dst[1] = dst[2] =
                    (unsigned char)lroundf(.2126f * src[0] + .7152f * src[1] + .0722f * src[2]);
            dst[0] = (unsigned char)((unsigned)dst[0] * key->tint.r / 255);
            dst[1] = (unsigned char)((unsigned)dst[1] * key->tint.g / 255);
            dst[2] = (unsigned char)((unsigned)dst[2] * key->tint.b / 255);
            dst[3] = (unsigned char)((unsigned)dst[3] * key->tint.a / 255);
        }
    }
    return rgba;
}
#endif
