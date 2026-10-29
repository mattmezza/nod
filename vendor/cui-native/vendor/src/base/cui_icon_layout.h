/* SPDX-License-Identifier: MIT */
#ifndef CUI_ICON_LAYOUT_H
#define CUI_ICON_LAYOUT_H
#include "cui/cui_icon.h"
#include <math.h>
/* Shared canonical lookup; SIZE_MAX means unknown. */
size_t cui_icon_catalog_index_internal(cui_str);
static inline bool cui_icon_extent_valid(float x) {
    return isfinite(x) && x >= 0 && x <= 4096;
}
static inline bool cui_icon_layout_resolve(cui_icon_options o, float *width, float *height) {
    cui_icon_constraints c = o.constraints ? *o.constraints : (cui_icon_constraints){0};
    if ((c.custom_min_width && !cui_icon_extent_valid(c.min_width)) ||
        (c.custom_min_height && !cui_icon_extent_valid(c.min_height)) ||
        (c.custom_max_width && !cui_icon_extent_valid(c.max_width)) ||
        (c.custom_max_height && !cui_icon_extent_valid(c.max_height)))
        return false;
    const cui_box b = o.style ? *o.style : (cui_box){0};
    float maxw = c.custom_max_width ? c.max_width : b.max_width > 0 ? b.max_width : 4096;
    float maxh = c.custom_max_height ? c.max_height : b.max_height > 0 ? b.max_height : 4096;
    float minw = c.custom_min_width ? c.min_width : 0;
    float minh = c.custom_min_height ? c.min_height : 0;
    cui_border_edges p = b.custom_padding
                             ? b.padding_edges
                             : (cui_border_edges){b.padding, b.padding, b.padding, b.padding};
    cui_border_edges e = b.custom_border_edges ? b.border_edges
                                               : (cui_border_edges){b.border_width, b.border_width,
                                                                    b.border_width, b.border_width};
    const float edges[] = {p.left, p.right, p.top, p.bottom, e.left, e.right, e.top, e.bottom};
    for (size_t i = 0; i < 8; i++)
        if (!cui_icon_extent_valid(edges[i]))
            return false;
    *width = fmaxf(fmaxf(minw, fminf(*width, maxw)), p.left + p.right + e.left + e.right);
    *height = fmaxf(fmaxf(minh, fminf(*height, maxh)), p.top + p.bottom + e.top + e.bottom);
    return cui_icon_extent_valid(*width) && cui_icon_extent_valid(*height);
}
#endif
