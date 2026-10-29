/* SPDX-License-Identifier: MIT */
#ifndef CUI_BACKEND_SHADOW_H
#define CUI_BACKEND_SHADOW_H
#include "cui/cui_effects.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* 32MiB CPU + at most 32MiB uploaded shadow copies = 64MiB owned cache. */
#define CUI_SHADOW_CACHE_BYTES ((size_t)32 * 1024 * 1024)
#define CUI_SHADOW_CACHE_SLOTS 64
typedef struct {
    cui_drop_shadow_paint key;
    cui_draw_type kind;
    float scale, origin_x, origin_y;
    unsigned char *pixels;
    cui_drop_shadow_raster raster;
    uint64_t revision, used;
} cui_shadow_entry;
typedef struct {
    cui_shadow_entry entries[CUI_SHADOW_CACHE_SLOTS];
    size_t bytes;
    uint64_t frame, revision, hits, misses;
    cui_draw_command *commands;
    size_t capacity;
} cui_shadow_cache;
static inline void cui_shadow_destroy(cui_shadow_cache *cache) {
    for (size_t i = 0; i < CUI_SHADOW_CACHE_SLOTS; i++)
        free(cache->entries[i].pixels);
    free(cache->commands);
    memset(cache, 0, sizeof(*cache));
}
static inline bool cui_shadow_type(cui_draw_type type) {
    return type == CUI_DRAW_DROP_SHADOW || type == CUI_DRAW_INSET_SHADOW;
}
static inline bool cui_shadow_query(cui_draw_type kind, cui_drop_shadow_paint p, float s, float x,
                                    float y, cui_drop_shadow_raster *out) {
    if (kind == CUI_DRAW_DROP_SHADOW)
        return cui_effect_drop_shadow_query(p, s, x, y, out);
    cui_inset_shadow_raster q;
    if (!cui_effect_inset_shadow_query(p, s, x, y, &q))
        return false;
    *out = (cui_drop_shadow_raster){.bounds = q.bounds,
                                    .covered_layer = q.covered_hole,
                                    .width = q.width,
                                    .height = q.height,
                                    .bytes = q.bytes};
    return true;
}
static inline bool cui_shadow_raster(cui_draw_type kind, cui_drop_shadow_paint p, float s, float x,
                                     float y, cui_effect_buffer b) {
    return kind == CUI_DRAW_DROP_SHADOW ? cui_effect_drop_shadow_raster(p, s, x, y, b)
                                        : cui_effect_inset_shadow_raster(p, s, x, y, b);
}
static inline bool cui_shadow_present(const cui_draw_list *list) {
    for (size_t i = 0; i < list->count; i++)
        if (cui_shadow_type(list->commands[i].type))
            return true;
    return false;
}
static inline bool cui_shadow_uniform(float sx, float sy) {
    return isfinite(sx) && isfinite(sy) && sx > 0 && sy > 0 &&
           fabsf(sx - sy) <= 4 * FLT_EPSILON * fmaxf(sx, sy);
}
static inline bool cui_shadow_key_equal(cui_drop_shadow_paint a, cui_drop_shadow_paint b) {
    return a.caster.x == b.caster.x && a.caster.y == b.caster.y && a.caster.w == b.caster.w &&
           a.caster.h == b.caster.h && a.radii.top_left == b.radii.top_left &&
           a.radii.top_right == b.radii.top_right && a.radii.bottom_left == b.radii.bottom_left &&
           a.radii.bottom_right == b.radii.bottom_right && a.layer.offset_x == b.layer.offset_x &&
           a.layer.offset_y == b.layer.offset_y && a.layer.sigma == b.layer.sigma &&
           a.layer.spread == b.layer.spread && a.layer.color.r == b.layer.color.r &&
           a.layer.color.g == b.layer.color.g && a.layer.color.b == b.layer.color.b;
}
static inline cui_rect cui_shadow_intersection(cui_rect a, cui_rect b) {
    float x = fmaxf(a.x, b.x), y = fmaxf(a.y, b.y);
    return (cui_rect){x, y, fmaxf(0, fminf(a.x + a.w, b.x + b.w) - x),
                      fmaxf(0, fminf(a.y + a.h, b.y + b.h) - y)};
}
/* Prepares value copies without mutating source commands or target pixels.
 * Frame-pinned entries cannot be evicted by a later command in this frame. */
static inline bool cui_shadow_prepare(cui_shadow_cache *cache, const cui_draw_list *list, float sx,
                                      float sy, float ox, float oy, cui_draw_list *out) {
    if (!cui_shadow_present(list)) {
        *out = *list;
        return true;
    }
    if (!isfinite(list->width) || !isfinite(list->height) || list->width <= 0 ||
        list->height <= 0 || !cui_shadow_uniform(sx, sy) || !isfinite(list->width * sx + ox) ||
        !isfinite(list->height * sy + oy) || list->count > 131072 ||
        list->count > SIZE_MAX / sizeof(cui_draw_command))
        return false;
    if (cache->capacity < list->count) {
        void *next = realloc(cache->commands, list->count * sizeof(cui_draw_command));
        if (!next)
            return false;
        cache->commands = next;
        cache->capacity = list->count;
    }
    cache->frame++;
    if (!cache->frame) {
        for (size_t i = 0; i < CUI_SHADOW_CACHE_SLOTS; i++)
            cache->entries[i].used = 0;
        cache->frame = 1;
    }
    cui_rect clip = {0, 0, list->width, list->height}, stack[64];
    size_t depth = 0;
    for (size_t i = 0; i < list->count; i++) {
        cui_draw_command c = list->commands[i];
        if (c.type == CUI_DRAW_CLIP_PUSH) {
            if (!isfinite(c.bounds.x) || !isfinite(c.bounds.y) || !isfinite(c.bounds.w) ||
                !isfinite(c.bounds.h) || c.bounds.w < 0 || c.bounds.h < 0 ||
                !isfinite(c.bounds.x + c.bounds.w) || !isfinite(c.bounds.y + c.bounds.h))
                return false;
            if (depth == 64)
                return false;
            stack[depth++] = clip;
            clip = cui_shadow_intersection(clip, c.bounds);
        } else if (c.type == CUI_DRAW_CLIP_POP) {
            if (!depth)
                return false;
            clip = stack[--depth];
        }
        if (cui_shadow_type(c.type)) {
            cui_drop_shadow_raster q;
            if (!cui_shadow_query(c.type, c.shadow, sx, ox, oy, &q))
                return false;
            /* Exact query covering avoids recomputing offset/spread with a
             * different precision (notably JS doubles and high f32 origins). */
            cui_rect physical_layer = q.covered_layer;
            float cx = floorf(clip.x * sx + ox), cy = floorf(clip.y * sx + oy);
            cui_rect physical_clip = {cx, cy, ceilf((clip.x + clip.w) * sx + ox) - cx,
                                      ceilf((clip.y + clip.h) * sx + oy) - cy};
            cui_rect visible = cui_shadow_intersection(physical_layer, physical_clip);
            if (!q.width || visible.w <= 0 || visible.h <= 0 || !c.shadow.layer.color.a) {
                c = (cui_draw_command){.type = CUI_DRAW_RECT, .bounds = {0, 0, 0, 0}};
            } else {
                cui_drop_shadow_paint key = c.shadow;
                float dx = floorf(key.caster.x * sx + ox), dy = floorf(key.caster.y * sx + oy);
                key.caster.x = (key.caster.x * sx + ox - dx) / sx;
                key.caster.y = (key.caster.y * sx + oy - dy) / sx;
                key.layer.color.a = 255;
                float cache_ox = 0, cache_oy = 0;
                cui_drop_shadow_raster normalized;
                bool normalized_valid = cui_shadow_query(c.type, key, sx, 0, 0, &normalized);
                /* Large f32 origins can lose fractions before source covering.
                 * Reuse translation only when the actual covered geometry agrees. */
                if (!normalized_valid || normalized.width != q.width ||
                    normalized.height != q.height ||
                    (c.type == CUI_DRAW_INSET_SHADOW &&
                     (normalized.covered_layer.w != q.covered_layer.w ||
                      normalized.covered_layer.h != q.covered_layer.h ||
                      normalized.covered_layer.x + dx != q.covered_layer.x ||
                      normalized.covered_layer.y + dy != q.covered_layer.y)) ||
                    roundf(normalized.bounds.x * sx) + dx != roundf(q.bounds.x * sx + ox) ||
                    roundf(normalized.bounds.y * sx) + dy != roundf(q.bounds.y * sx + oy)) {
                    key = c.shadow;
                    key.layer.color.a = 255;
                    cache_ox = ox;
                    cache_oy = oy;
                    normalized = q;
                }
                cui_shadow_entry *entry = NULL;
                for (size_t j = 0; j < CUI_SHADOW_CACHE_SLOTS; j++)
                    if (cache->entries[j].pixels && cache->entries[j].kind == c.type &&
                        cache->entries[j].scale == sx && cache->entries[j].origin_x == cache_ox &&
                        cache->entries[j].origin_y == cache_oy &&
                        cui_shadow_key_equal(cache->entries[j].key, key)) {
                        entry = &cache->entries[j];
                        cache->hits++;
                        break;
                    }
                if (!entry) {
                    if (normalized.bytes > CUI_SHADOW_CACHE_BYTES)
                        return false;
                    for (;;) {
                        cui_shadow_entry *oldest = NULL, *empty = NULL;
                        for (size_t j = 0; j < CUI_SHADOW_CACHE_SLOTS; j++) {
                            cui_shadow_entry *e = &cache->entries[j];
                            if (!e->pixels)
                                empty = e;
                            else if (e->used != cache->frame && (!oldest || e->used < oldest->used))
                                oldest = e;
                        }
                        if (empty && cache->bytes + normalized.bytes <= CUI_SHADOW_CACHE_BYTES) {
                            entry = empty;
                            break;
                        }
                        if (!oldest)
                            return false;
                        cache->bytes -= oldest->raster.bytes;
                        free(oldest->pixels);
                        *oldest = (cui_shadow_entry){0};
                    }
                    unsigned char *pixels = malloc(normalized.bytes);
                    if (!pixels)
                        return false;
                    if (!cui_shadow_raster(
                            c.type, key, sx, cache_ox, cache_oy,
                            (cui_effect_buffer){pixels, normalized.width, normalized.height,
                                                (size_t)normalized.width * 4, normalized.bytes})) {
                        free(pixels);
                        return false;
                    }
                    *entry = (cui_shadow_entry){.key = key,
                                                .kind = c.type,
                                                .scale = sx,
                                                .origin_x = cache_ox,
                                                .origin_y = cache_oy,
                                                .pixels = pixels,
                                                .raster = normalized,
                                                .revision = ++cache->revision};
                    cache->bytes += normalized.bytes;
                    cache->misses++;
                }
                entry->used = cache->frame;
                float qx = roundf(q.bounds.x * sx + ox), qy = roundf(q.bounds.y * sx + oy);
                cui_rect crop = cui_shadow_intersection(
                    (cui_rect){qx, qy, (float)q.width, (float)q.height}, physical_clip);
                c.bounds =
                    (cui_rect){(crop.x - ox) / sx, (crop.y - oy) / sx, crop.w / sx, crop.h / sx};
                c.radius = 0;
                c.custom_corners = false;

                c.bitmap = (cui_bitmap_paint){.bitmap = {.pixels = entry->pixels,
                                                         .width = entry->raster.width,
                                                         .height = entry->raster.height,
                                                         .stride = (size_t)entry->raster.width * 4,
                                                         .revision = entry->revision},
                                              .source = {crop.x - qx, crop.y - qy, crop.w, crop.h},
                                              .opacity = c.shadow.layer.color.a / 255.0f};
            }
        }
        cache->commands[i] = c;
    }
    if (depth)
        return false;
    *out = (cui_draw_list){cache->commands, list->count, list->width, list->height};
    return true;
}
#endif
