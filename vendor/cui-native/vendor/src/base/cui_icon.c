/* SPDX-License-Identifier: MIT */
#include "cui/cui_icon.h"
#include "cui_icon_layout.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
/* Keep the embedded SVG engine separate from an application NanoSVG copy. */
#define nsvgParse cui_icon_private_nsvgParse
#define nsvgParseFromFile cui_icon_private_nsvgParseFromFile
#define nsvgDuplicatePath cui_icon_private_nsvgDuplicatePath
#define nsvgDelete cui_icon_private_nsvgDelete
#define nsvgCreateRasterizer cui_icon_private_nsvgCreateRasterizer
#define nsvgDeleteRasterizer cui_icon_private_nsvgDeleteRasterizer
#define nsvgRasterize cui_icon_private_nsvgRasterize
#define nsvg__colors cui_icon_private_nsvg__colors
#define nsvg__parseXML cui_icon_private_nsvg__parseXML
#ifndef NANOSVG_IMPLEMENTATION
#define CUI_ICON_UNDEF_NANOSVG_IMPLEMENTATION
#define NANOSVG_IMPLEMENTATION
#endif
#ifndef NANOSVGRAST_IMPLEMENTATION
#define CUI_ICON_UNDEF_NANOSVGRAST_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#endif
#include "../../third_party/nanosvg/nanosvg.h"
#include "../../third_party/nanosvg/nanosvgrast.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
typedef struct {
    const char *name, *path, *svg;
    bool default_asset;
} cui_icon_asset_record;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverlength-strings"
#endif
#include "cui_icon_assets.inc"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
typedef struct cui_icon_mask {
    struct cui_icon_mask *next;
    cui_bitmap bitmap;
    cui_rect bounds;
    uint64_t key;
    size_t bytes;
} cui_icon_mask;
typedef struct {
    char *source;
    size_t source_length;
    bool path, dirty, empty;
    cui_icon_loader_fn loader;
    void *loader_user;
    NSVGimage *document;
    NSVGrasterizer *rasterizer;
    uint64_t document_hash;
    cui_icon_limits limits;
    cui_icon_mask *masks, *active;
} cui_icon_private;
static cui_icon_error cui_icon_fail(cui_icon_state *state, cui_icon_error error) {
    if (state)
        state->error = error;
    return error;
}
cui_icon_limits cui_icon_default_limits(void) {
    return (cui_icon_limits){256 * 1024, 1024 * 1024, 8 * 1024 * 1024, 16};
}
void cui_icon_state_init(cui_icon_state *state) {
    if (state)
        *state = (cui_icon_state){0};
}
static cui_icon_private *cui_icon_private_get(cui_icon_state *state) {
    if (!state)
        return NULL;
    if (!state->internal) {
        cui_icon_private *internal = calloc(1, sizeof(*internal));
        if (!internal) {
            cui_icon_fail(state, CUI_ICON_ALLOCATION);
            return NULL;
        }
        internal->limits = cui_icon_default_limits();
        internal->empty = true;
        state->internal = internal;
    }
    return state->internal;
}
static void cui_icon_mask_free(cui_icon_mask *mask) {
    free((void *)mask->bitmap.pixels);
    free(mask);
}
void cui_icon_state_collect(cui_icon_state *state) {
    if (!state || !state->internal)
        return;
    cui_icon_private *internal = state->internal;
    cui_icon_mask *mask = internal->masks;
    while (mask) {
        cui_icon_mask *next = mask->next;
        if (mask != internal->active)
            cui_icon_mask_free(mask);
        mask = next;
    }
    internal->masks = internal->active;
    if (internal->active)
        internal->active->next = NULL;
    state->cache_bytes = internal->active ? internal->active->bytes : 0;
    state->cache_versions = internal->active ? 1 : 0;
}
void cui_icon_state_destroy(cui_icon_state *state) {
    if (!state)
        return;
    cui_icon_private *internal = state->internal;
    if (internal) {
        internal->active = NULL;
        cui_icon_state_collect(state);
        nsvgDelete(internal->document);
        nsvgDeleteRasterizer(internal->rasterizer);
        free(internal->source);
        free(internal);
    }
    *state = (cui_icon_state){0};
}
bool cui_icon_set_limits(cui_icon_state *state, cui_icon_limits limits) {
    cui_icon_private *internal = cui_icon_private_get(state);
    if (!internal)
        return false;
    cui_icon_limits defaults = cui_icon_default_limits();
    if (!limits.max_svg_bytes)
        limits.max_svg_bytes = defaults.max_svg_bytes;
    if (!limits.max_pixels)
        limits.max_pixels = defaults.max_pixels;
    if (!limits.max_cache_bytes)
        limits.max_cache_bytes = defaults.max_cache_bytes;
    if (!limits.max_versions)
        limits.max_versions = defaults.max_versions;
    if (limits.max_svg_bytes > 1024 * 1024 || limits.max_pixels > 16 * 1024 * 1024 ||
        limits.max_cache_bytes > 64 * 1024 * 1024 || limits.max_versions > 128 ||
        state->cache_bytes > limits.max_cache_bytes ||
        state->cache_versions > limits.max_versions) {
        cui_icon_fail(state, CUI_ICON_LIMIT);
        return false;
    }
    internal->limits = limits;
    state->error = CUI_ICON_OK;
    return true;
}
static bool cui_icon_string_equal(cui_str text, const char *literal) {
    return text.length == strlen(literal) && !memcmp(text.data, literal, text.length);
}
size_t cui_icon_catalog_count(void) {
    return sizeof(cui_icon_assets) / sizeof(cui_icon_assets[0]);
}
cui_str cui_icon_catalog_name(size_t index) {
    return index < cui_icon_catalog_count() ? cui_string(cui_icon_assets[index].name)
                                            : (cui_str){0};
}
cui_str cui_icon_catalog_path(size_t index) {
    return index < cui_icon_catalog_count() ? cui_string(cui_icon_assets[index].path)
                                            : (cui_str){0};
}
bool cui_icon_catalog_default(size_t index) {
    return index < cui_icon_catalog_count() && cui_icon_assets[index].default_asset;
}
static const cui_icon_asset_record *cui_icon_lookup(cui_str name) {
    if (!name.data || !name.length)
        return NULL;
    static const struct {
        const char *alias, *path, *canonical;
    } aliases[] = {{"filter", "icons/filter.svg", "funnel"},
                   {"trash-2", "icons/trash-2.svg", "trash"},
                   {"more-vertical", "icons/more-vertical.svg", "ellipsis-vertical"}};
    for (size_t i = 0; i < sizeof aliases / sizeof aliases[0]; i++) {
        if (cui_icon_string_equal(name, aliases[i].alias) ||
            cui_icon_string_equal(name, aliases[i].path)) {
            name = cui_string(aliases[i].canonical);
            break;
        }
    }
    for (size_t i = 0; i < cui_icon_catalog_count(); i++)
        if (cui_icon_string_equal(name, cui_icon_assets[i].name) ||
            cui_icon_string_equal(name, cui_icon_assets[i].path))
            return &cui_icon_assets[i];
    return NULL;
}
size_t cui_icon_catalog_index_internal(cui_str name) {
    const cui_icon_asset_record *asset = cui_icon_lookup(name);
    return asset ? (size_t)(asset - cui_icon_assets) : SIZE_MAX;
}
cui_icon_availability cui_icon_query(cui_str name) {
    const cui_icon_asset_record *asset = cui_icon_lookup(name);
    return !asset       ? CUI_ICON_UNKNOWN
           : asset->svg ? CUI_ICON_EMBEDDED
                        : CUI_ICON_CATALOG_NOT_EMBEDDED;
}
bool cui_icon_asset(cui_str name, cui_str *svg) {
    if (!svg)
        return false;
    *svg = (cui_str){0};
    const cui_icon_asset_record *asset = cui_icon_lookup(name);
    if (!asset || !asset->svg)
        return false;
    *svg = cui_string(asset->svg);
    return true;
}
static bool cui_icon_source_set(cui_icon_state *state, cui_str source, bool path,
                                cui_icon_loader_fn loader, void *user) {
    cui_icon_private *internal = cui_icon_private_get(state);
    if (!internal)
        return false;
    if (source.length > (path ? 4096 : internal->limits.max_svg_bytes)) {
        cui_icon_fail(state, CUI_ICON_LIMIT);
        return false;
    }
    if ((source.length && !source.data) ||
        (source.length && memchr(source.data, 0, source.length))) {
        cui_icon_fail(state, CUI_ICON_INVALID);
        return false;
    }
    char *copy = malloc(source.length + 1);
    if (!copy) {
        cui_icon_fail(state, CUI_ICON_ALLOCATION);
        return false;
    }
    if (source.length)
        memcpy(copy, source.data, source.length);
    copy[source.length] = 0;
    free(internal->source);
    internal->source = copy;
    internal->source_length = source.length;
    internal->path = path;
    internal->loader = loader;
    internal->loader_user = user;
    internal->dirty = true;
    internal->empty = !source.length;
    state->error = CUI_ICON_OK;
    return true;
}
bool cui_icon_set_data(cui_icon_state *state, cui_str svg) {
    return cui_icon_source_set(state, svg, false, NULL, NULL);
}
bool cui_icon_set_path(cui_icon_state *state, cui_str path, cui_icon_loader_fn loader, void *user) {
    return cui_icon_source_set(state, path, true, loader, user);
}
bool cui_icon_set_name(cui_icon_state *state, cui_str name, cui_icon_loader_fn loader, void *user) {
    const cui_icon_asset_record *asset = cui_icon_lookup(name);
    if (!asset) {
        cui_icon_fail(state, CUI_ICON_NOT_FOUND);
        return false;
    }
    return cui_icon_set_path(state, cui_string(asset->path), loader, user);
}
bool cui_icon_set_empty(cui_icon_state *state) {
    return cui_icon_set_path(state, (cui_str){0}, NULL, NULL);
}
void cui_icon_reload(cui_icon_state *state) {
    if (state && state->internal)
        ((cui_icon_private *)state->internal)->dirty = true;
}
cui_icon_transform cui_icon_identity(void) {
    return (cui_icon_transform){1, 0, 0, 1, 0, 0};
}
cui_icon_transform cui_icon_rotation(float radians) {
    float c = cosf(radians), s = sinf(radians);
    return (cui_icon_transform){c, s, -s, c, 0, 0};
}
static uint64_t cui_icon_hash(uint64_t hash, const void *bytes, size_t length) {
    const unsigned char *data = bytes;
    for (size_t i = 0; i < length; i++) {
        hash ^= data[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
static cui_icon_error cui_icon_parse(cui_icon_state *state, cui_icon_private *internal) {
    cui_str svg = {internal->source, internal->source_length};
    if (internal->path) {
        bool loaded = internal->loader && internal->loader(internal->loader_user, svg, &svg);
        if (!loaded && !cui_icon_asset((cui_str){internal->source, internal->source_length}, &svg))
            return cui_icon_fail(state, CUI_ICON_NOT_FOUND);
    }
    if (svg.length > internal->limits.max_svg_bytes)
        return cui_icon_fail(state, CUI_ICON_LIMIT);
    if (!svg.data || !svg.length || memchr(svg.data, 0, svg.length))
        return cui_icon_fail(state, CUI_ICON_INVALID);
    uint64_t hash = cui_icon_hash(UINT64_C(14695981039346656037), svg.data, svg.length);
    if (internal->document && internal->document_hash == hash) {
        internal->dirty = false;
        return CUI_ICON_OK;
    }
    char *copy = malloc(svg.length + 1);
    if (!copy)
        return cui_icon_fail(state, CUI_ICON_ALLOCATION);
    memcpy(copy, svg.data, svg.length);
    copy[svg.length] = 0;
    const char *root = strstr(copy, "<svg");
    const char *root_end = root ? strchr(root, '>') : NULL;
    if (!root || !root_end || (!strstr(root_end, "</svg>") && root_end[-1] != '/') ||
        strstr(copy, "<!DOCTYPE") || strstr(copy, "<!ENTITY")) {
        free(copy);
        return cui_icon_fail(state, CUI_ICON_PARSE);
    }
    NSVGimage *document = nsvgParse(copy, "px", 96);
    free(copy);
    state->parse_count++;
    if (!document)
        return cui_icon_fail(state, CUI_ICON_ALLOCATION);
    bool valid = isfinite(document->width) && isfinite(document->height) && document->width > 0 &&
                 document->height > 0 && document->width <= 1000000 && document->height <= 1000000;
    size_t shapes = 0, points = 0;
    for (NSVGshape *shape = document->shapes; shape && valid; shape = shape->next) {
        valid = ++shapes <= 4096 && isfinite(shape->strokeWidth) && shape->strokeWidth >= 0 &&
                shape->strokeWidth <= 1000000;
        for (NSVGpath *path = shape->paths; path && valid; path = path->next) {
            valid = path->npts >= 1 && (size_t)path->npts <= 65536 - points;
            if (!valid)
                break;
            points += (size_t)path->npts;
            for (int i = 0; i < path->npts * 2; i++)
                if (!isfinite(path->pts[i]) || fabsf(path->pts[i]) > 1000000)
                    valid = false;
        }
    }
    if (!valid) {
        nsvgDelete(document);
        return cui_icon_fail(state, CUI_ICON_PARSE);
    }
    nsvgDelete(internal->document);
    internal->document = document;
    internal->document_hash = hash;
    internal->dirty = false;
    return CUI_ICON_OK;
}
static float cui_icon_alpha_sample(const unsigned char *pixels, unsigned width, unsigned height,
                                   float x, float y) {
    if (!isfinite(x) || !isfinite(y) || x < -1 || y < -1 || x > width || y > height)
        return 0;
    int left = (int)floorf(x), top = (int)floorf(y);
    float dx = x - left, dy = y - top, alpha = 0;
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++) {
            int px = left + i, py = top + j;
            if (px >= 0 && py >= 0 && (unsigned)px < width && (unsigned)py < height)
                alpha += pixels[((size_t)py * width + (unsigned)px) * 4 + 3] * (i ? dx : 1 - dx) *
                         (j ? dy : 1 - dy);
        }
    return alpha;
}
cui_icon_error cui_icon_prepare(cui_icon_state *state, float width, float height,
                                cui_icon_transform transform) {
    cui_icon_private *internal = cui_icon_private_get(state);
    if (!internal)
        return state ? state->error : CUI_ICON_INVALID;
    const float matrix[] = {transform.a, transform.b,  transform.c,
                            transform.d, transform.tx, transform.ty};
    if (!isfinite(width) || !isfinite(height) || width < 0 || height < 0 || width > 4096 ||
        height > 4096)
        return cui_icon_fail(state, CUI_ICON_INVALID);
    for (size_t i = 0; i < 6; i++)
        if (!isfinite(matrix[i]) || fabsf(matrix[i]) > (i < 4 ? 64 : 1000000))
            return cui_icon_fail(state, CUI_ICON_INVALID);
    if (internal->empty || width == 0 || height == 0) {
        internal->active = NULL;
        return cui_icon_fail(state, CUI_ICON_OK);
    }
    if (internal->dirty && cui_icon_parse(state, internal) != CUI_ICON_OK)
        return state->error;
    if (!internal->document)
        return cui_icon_fail(state, CUI_ICON_PARSE);
    float determinant = transform.a * transform.d - transform.b * transform.c;
    if (fabsf(determinant) < .000001f) {
        internal->active = NULL;
        return cui_icon_fail(state, CUI_ICON_OK);
    }
    float paint_height = width * internal->document->height / internal->document->width;
    if (!isfinite(paint_height) || paint_height <= 0 || paint_height > 4096)
        return cui_icon_fail(state, CUI_ICON_LIMIT);
    float minimum_x = INFINITY, minimum_y = INFINITY, maximum_x = -INFINITY, maximum_y = -INFINITY;
    for (unsigned i = 0; i < 4; i++) {
        float x = (i & 1) ? width / 2 : -width / 2;
        float y = (i & 2) ? paint_height / 2 : -paint_height / 2;
        float px = transform.a * x + transform.c * y + transform.tx + width / 2;
        float py = transform.b * x + transform.d * y + transform.ty + height / 2;
        minimum_x = fminf(minimum_x, px);
        maximum_x = fmaxf(maximum_x, px);
        minimum_y = fminf(minimum_y, py);
        maximum_y = fmaxf(maximum_y, py);
    }
    minimum_x = floorf(minimum_x);
    minimum_y = floorf(minimum_y);
    unsigned output_width = (unsigned)ceilf(maximum_x - minimum_x);
    unsigned output_height = (unsigned)ceilf(maximum_y - minimum_y);
    unsigned base_width = (unsigned)ceilf(width), base_height = (unsigned)ceilf(paint_height);
    size_t pixels = (size_t)output_width * output_height;
    size_t base_pixels = (size_t)base_width * base_height;
    if (!output_width || !output_height || output_width > 4096 || output_height > 4096 ||
        pixels > internal->limits.max_pixels || base_pixels > internal->limits.max_pixels)
        return cui_icon_fail(state, CUI_ICON_LIMIT);
    uint64_t key = cui_icon_hash(internal->document_hash, &width, sizeof(width));
    key = cui_icon_hash(key, &height, sizeof(height));
    key = cui_icon_hash(key, matrix, sizeof(matrix));
    for (cui_icon_mask *mask = internal->masks; mask; mask = mask->next)
        if (mask->key == key) {
            internal->active = mask;
            return cui_icon_fail(state, CUI_ICON_OK);
        }
    size_t bytes = pixels * 4;
    if (state->cache_versions >= internal->limits.max_versions ||
        bytes > internal->limits.max_cache_bytes - state->cache_bytes)
        return cui_icon_fail(state, CUI_ICON_LIMIT);
    if (!internal->rasterizer)
        internal->rasterizer = nsvgCreateRasterizer();
    unsigned char *base = calloc(base_pixels, 4), *output = calloc(pixels, 4);
    cui_icon_mask *mask = calloc(1, sizeof(*mask));
    if (!internal->rasterizer || !base || !output || !mask) {
        free(base);
        free(output);
        free(mask);
        return cui_icon_fail(state, CUI_ICON_ALLOCATION);
    }
    /* Lucide uses .01-unit paths for round-cap dots (e.g. signal-zero).
     * The vendor's default .01 pixel merge tolerance erases those strokes. */
    internal->rasterizer->distTol = .0001f;
    nsvgRasterize(internal->rasterizer, internal->document, 0, 0, width / internal->document->width,
                  base, (int)base_width, (int)base_height, (int)(base_width * 4));
    for (unsigned y = 0; y < output_height; y++)
        for (unsigned x = 0; x < output_width; x++) {
            float px = minimum_x + x + .5f - width / 2 - transform.tx;
            float py = minimum_y + y + .5f - height / 2 - transform.ty;
            float sx = (transform.d * px - transform.c * py) / determinant + width / 2 - .5f;
            float sy =
                (-transform.b * px + transform.a * py) / determinant + paint_height / 2 - .5f;
            size_t index = ((size_t)y * output_width + x) * 4;
            output[index] = output[index + 1] = output[index + 2] = 255;
            output[index + 3] = (unsigned char)fminf(
                255,
                fmaxf(0, lroundf(cui_icon_alpha_sample(base, base_width, base_height, sx, sy))));
        }
    free(base);
    mask->bitmap =
        (cui_bitmap){output, output_width, output_height, output_width * 4, key ? key : 1};
    mask->bounds = (cui_rect){minimum_x, minimum_y, (float)output_width, (float)output_height};
    mask->bytes = bytes;
    mask->key = key;
    mask->next = internal->masks;
    internal->masks = internal->active = mask;
    state->cache_bytes += bytes;
    state->cache_versions++;
    state->raster_count++;
    return cui_icon_fail(state, CUI_ICON_OK);
}
cui_bitmap cui_icon_state_bitmap(const cui_icon_state *state) {
    const cui_icon_private *internal = state ? state->internal : NULL;
    return internal && internal->active ? internal->active->bitmap : (cui_bitmap){0};
}
cui_rect cui_icon_state_paint_bounds(const cui_icon_state *state) {
    const cui_icon_private *internal = state ? state->internal : NULL;
    return internal && internal->active ? internal->active->bounds : (cui_rect){0};
}

#undef nsvgParse
#undef nsvgParseFromFile
#undef nsvgDuplicatePath
#undef nsvgDelete
#undef nsvgCreateRasterizer
#undef nsvgDeleteRasterizer
#undef nsvgRasterize
#undef nsvg__colors
#undef nsvg__parseXML
#ifdef CUI_ICON_UNDEF_NANOSVG_IMPLEMENTATION
#undef NANOSVG_IMPLEMENTATION
#undef CUI_ICON_UNDEF_NANOSVG_IMPLEMENTATION
#endif
#ifdef CUI_ICON_UNDEF_NANOSVGRAST_IMPLEMENTATION
#undef NANOSVGRAST_IMPLEMENTATION
#undef CUI_ICON_UNDEF_NANOSVGRAST_IMPLEMENTATION
#endif
