/* SPDX-License-Identifier: MIT */
#include "cui/cui_virtual.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    uint64_t key;
    size_t index_plus_one;
} cui_virtual_private_key_slot;
struct cui_virtual_list {
    size_t capacity, count, map_capacity, memory_bytes;
    double default_height, total;
    uint64_t *keys, *next_keys;
    double *heights, *next_heights, *tree;
    cui_virtual_private_key_slot *map, *next_map;
};
static uint64_t cui_virtual_private_mix(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}
static size_t cui_virtual_private_lookup(const cui_virtual_private_key_slot *map, size_t capacity,
                                         uint64_t key) {
    size_t i = (size_t)cui_virtual_private_mix(key) & (capacity - 1);
    while (map[i].index_plus_one) {
        if (map[i].key == key)
            return map[i].index_plus_one - 1;
        i = (i + 1) & (capacity - 1);
    }
    return SIZE_MAX;
}
static bool cui_virtual_private_add_key(cui_virtual_private_key_slot *map, size_t capacity,
                                        uint64_t key, size_t index) {
    size_t i = (size_t)cui_virtual_private_mix(key) & (capacity - 1);
    while (map[i].index_plus_one) {
        if (map[i].key == key)
            return false;
        i = (i + 1) & (capacity - 1);
    }
    map[i] = (cui_virtual_private_key_slot){key, index + 1};
    return true;
}
cui_virtual_list *cui_virtual_create(cui_virtual_config c) {
    if (!c.capacity || !isfinite(c.default_height) || c.default_height <= 0 ||
        c.default_height > DBL_MAX / 2)
        return NULL;
    if (c.capacity > (SIZE_MAX - 1) / 2 || c.capacity > (size_t)PTRDIFF_MAX / sizeof(uint64_t) ||
        c.capacity > (size_t)PTRDIFF_MAX / sizeof(double) - 1)
        return NULL;
    size_t m = 2;
    while (m < c.capacity * 2) {
        if (m > SIZE_MAX / 2)
            return NULL;
        m *= 2;
    }
    if (m > (size_t)PTRDIFF_MAX / sizeof(cui_virtual_private_key_slot))
        return NULL;
    /* Bound the combined allocation sum, not only each product. */
    size_t bytes = sizeof(cui_virtual_list), part;
    if (c.capacity > SIZE_MAX / (2 * sizeof(uint64_t) + 2 * sizeof(double)))
        return NULL;
    part = c.capacity * (2 * sizeof(uint64_t) + 2 * sizeof(double));
    if (part > SIZE_MAX - bytes)
        return NULL;
    bytes += part;
    part = (c.capacity + 1) * sizeof(double);
    if (part > SIZE_MAX - bytes)
        return NULL;
    bytes += part;
    if (m > SIZE_MAX / (2 * sizeof(cui_virtual_private_key_slot)))
        return NULL;
    part = m * 2 * sizeof(cui_virtual_private_key_slot);
    if (part > SIZE_MAX - bytes)
        return NULL;
    bytes += part;
    cui_virtual_list *v = calloc(1, sizeof(*v));
    if (!v)
        return NULL;
    v->capacity = c.capacity;
    v->map_capacity = m;
    v->default_height = c.default_height;
    v->memory_bytes = bytes;
    v->keys = malloc(c.capacity * sizeof(*v->keys));
    v->next_keys = malloc(c.capacity * sizeof(*v->next_keys));
    v->heights = malloc(c.capacity * sizeof(*v->heights));
    v->next_heights = malloc(c.capacity * sizeof(*v->next_heights));
    v->tree = calloc(c.capacity + 1, sizeof(*v->tree));
    v->map = calloc(m, sizeof(*v->map));
    v->next_map = calloc(m, sizeof(*v->next_map));
    if (!v->keys || !v->next_keys || !v->heights || !v->next_heights || !v->tree || !v->map ||
        !v->next_map) {
        cui_virtual_destroy(v);
        return NULL;
    }
    return v;
}
void cui_virtual_destroy(cui_virtual_list *v) {
    if (v) {
        free(v->keys);
        free(v->next_keys);
        free(v->heights);
        free(v->next_heights);
        free(v->tree);
        free(v->map);
        free(v->next_map);
        free(v);
    }
}
cui_virtual_result cui_virtual_rebuild(cui_virtual_list *v, const uint64_t *keys,
                                       const double *heights, size_t count) {
    if (!v || (count && !keys))
        return CUI_VIRTUAL_INVALID;
    if (count > v->capacity)
        return CUI_VIRTUAL_CAPACITY;
    memset(v->next_map, 0, v->map_capacity * sizeof(*v->next_map));
    long double total = 0;
    for (size_t i = 0; i < count; i++) {
        if (!cui_virtual_private_add_key(v->next_map, v->map_capacity, keys[i], i))
            return CUI_VIRTUAL_DUPLICATE_KEY;
        size_t old = cui_virtual_private_lookup(v->map, v->map_capacity, keys[i]);
        double h = heights ? heights[i] : old == SIZE_MAX ? v->default_height : v->heights[old];
        if (!isfinite(h) || h <= 0)
            return CUI_VIRTUAL_INVALID;
        total += (long double)h;
        if (total > DBL_MAX / 2)
            return CUI_VIRTUAL_INVALID;
        v->next_keys[i] = keys[i];
        v->next_heights[i] = h;
    }
    uint64_t *k = v->keys;
    v->keys = v->next_keys;
    v->next_keys = k;
    double *h = v->heights;
    v->heights = v->next_heights;
    v->next_heights = h;
    cui_virtual_private_key_slot *map = v->map;
    v->map = v->next_map;
    v->next_map = map;
    v->count = count;
    v->total = (double)total;
    memset(v->tree, 0, (count + 1) * sizeof(*v->tree));
    for (size_t i = 1; i <= count; i++) {
        v->tree[i] += v->heights[i - 1];
        size_t parent = i + (i & (~i + 1));
        if (parent <= count)
            v->tree[parent] += v->tree[i];
    }
    return CUI_VIRTUAL_OK;
}
size_t cui_virtual_count(const cui_virtual_list *v) {
    return v ? v->count : 0;
}
size_t cui_virtual_memory_bytes(const cui_virtual_list *v) {
    return v ? v->memory_bytes : 0;
}
bool cui_virtual_key(const cui_virtual_list *v, size_t i, uint64_t *key) {
    if (!v || i >= v->count || !key)
        return false;
    *key = v->keys[i];
    return true;
}
bool cui_virtual_index(const cui_virtual_list *v, uint64_t key, size_t *index) {
    if (!v || !index)
        return false;
    size_t i = cui_virtual_private_lookup(v->map, v->map_capacity, key);
    if (i == SIZE_MAX)
        return false;
    *index = i;
    return true;
}
double cui_virtual_height(const cui_virtual_list *v, size_t i) {
    return v && i < v->count ? v->heights[i] : 0;
}
double cui_virtual_offset(const cui_virtual_list *v, size_t i) {
    if (!v)
        return 0;
    if (i >= v->count)
        return v->total;
    double sum = 0;
    while (i) {
        sum += v->tree[i];
        i -= i & (~i + 1);
    }
    return sum;
}
double cui_virtual_total(const cui_virtual_list *v) {
    return v ? v->total : 0;
}
size_t cui_virtual_lower_bound(const cui_virtual_list *v, double offset) {
    if (!v || !v->count || isnan(offset) || offset < 0)
        return 0;
    if (offset >= v->total)
        return v->count;
    size_t step = 1;
    while (step <= v->count / 2)
        step *= 2;
    size_t i = 0;
    double sum = 0;
    while (step) {
        size_t next = i + step;
        if (next <= v->count && sum + v->tree[next] <= offset) {
            sum += v->tree[next];
            i = next;
        }
        step /= 2;
    }
    return i;
}
cui_virtual_result cui_virtual_set_height(cui_virtual_list *v, size_t i, double height) {
    if (!v || !isfinite(height) || height <= 0)
        return CUI_VIRTUAL_INVALID;
    if (i >= v->count)
        return CUI_VIRTUAL_NOT_FOUND;
    long double total = (long double)v->total - v->heights[i] + height;
    if (total > DBL_MAX / 2)
        return CUI_VIRTUAL_INVALID;
    double delta = height - v->heights[i];
    v->heights[i] = height;
    v->total = (double)total;
    for (size_t n = i + 1; n <= v->count; n += n & (~n + 1))
        v->tree[n] += delta;
    return CUI_VIRTUAL_OK;
}
cui_virtual_result cui_virtual_set_key_height(cui_virtual_list *v, uint64_t key, double height) {
    size_t i;
    if (!v)
        return CUI_VIRTUAL_INVALID;
    if (!cui_virtual_index(v, key, &i))
        return CUI_VIRTUAL_NOT_FOUND;
    return cui_virtual_set_height(v, i, height);
}
static double cui_virtual_private_extent(double x) {
    return isfinite(x) && x > 0 ? x : 0;
}
static double cui_virtual_private_clamp_scroll(const cui_virtual_list *v, double scroll,
                                               double viewport) {
    double max = v ? fmax(0, v->total - cui_virtual_private_extent(viewport)) : 0;
    scroll = cui_virtual_private_extent(scroll);
    return fmin(scroll, max);
}
cui_virtual_view cui_virtual_viewport(const cui_virtual_list *v, double scroll, double viewport,
                                      size_t overscan) {
    if (!v)
        return (cui_virtual_view){0};
    viewport = cui_virtual_private_extent(viewport);
    scroll = cui_virtual_private_clamp_scroll(v, scroll, viewport);
    size_t begin = cui_virtual_lower_bound(v, scroll), end = begin;
    if (viewport > 0) {
        double edge = fmin(v->total, scroll + viewport);
        end = cui_virtual_lower_bound(v, edge);
        if (end < v->count && cui_virtual_offset(v, end) < edge)
            end++;
    }
    begin = overscan > begin ? 0 : begin - overscan;
    end = overscan > v->count - end ? v->count : end + overscan;
    return (cui_virtual_view){begin,
                              end,
                              cui_virtual_offset(v, begin),
                              fmax(0, v->total - cui_virtual_offset(v, end)),
                              v->total,
                              scroll};
}
cui_virtual_anchor cui_virtual_capture_anchor(const cui_virtual_list *v, double scroll) {
    if (!v || !v->count)
        return (cui_virtual_anchor){0};
    scroll = fmin(cui_virtual_private_extent(scroll), v->total);
    size_t i = cui_virtual_lower_bound(v, scroll);
    if (i == v->count)
        i--;
    return (cui_virtual_anchor){
        v->keys[i], i, fmin(v->heights[i], fmax(0, scroll - cui_virtual_offset(v, i))), true};
}
double cui_virtual_restore_anchor(const cui_virtual_list *v, cui_virtual_anchor a,
                                  double viewport) {
    if (!v || !v->count || !a.valid)
        return 0;
    size_t i;
    if (!cui_virtual_index(v, a.key, &i))
        i = a.fallback_index < v->count ? a.fallback_index : v->count - 1;
    double scroll =
        cui_virtual_offset(v, i) + fmin(cui_virtual_private_extent(a.within), v->heights[i]);
    return cui_virtual_private_clamp_scroll(v, scroll, viewport);
}

/* Main-axis alignment is independent from any context or renderer. */
cui_virtual_result cui_virtual_scroll_to(const cui_virtual_list *v, size_t index, double viewport,
                                         double current, cui_virtual_alignment align,
                                         double *output) {
    if (!v || !output || !isfinite(viewport) || viewport <= 0 || !isfinite(current) ||
        align < CUI_VIRTUAL_ALIGN_TOP || align > CUI_VIRTUAL_ALIGN_NEAREST)
        return CUI_VIRTUAL_INVALID;
    if (index >= v->count)
        return CUI_VIRTUAL_NOT_FOUND;
    current = cui_virtual_private_clamp_scroll(v, current, viewport);
    double top = cui_virtual_offset(v, index), height = v->heights[index];
    double target = top;
    if (align == CUI_VIRTUAL_ALIGN_CENTER)
        target += (height - viewport) / 2;
    else if (align == CUI_VIRTUAL_ALIGN_BOTTOM)
        target += height - viewport;
    else if (align == CUI_VIRTUAL_ALIGN_NEAREST) {
        if (top >= current && top + height <= current + viewport)
            target = current;
        else if (height <= viewport && top >= current)
            target += height - viewport;
    }
    *output = cui_virtual_private_clamp_scroll(v, target, viewport);
    return CUI_VIRTUAL_OK;
}
uint64_t cui_virtual_item_id(uint64_t root, uint64_t key) {
    uint64_t id = cui_virtual_private_mix(key ^ root ^ UINT64_C(0xa170952df38e2b65));
    return id ? id : UINT64_C(0x7940e6872b01359a);
}
