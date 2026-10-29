/* SPDX-License-Identifier: MIT */
#ifndef CUI_PRIVATE_DESIGN_METRICS_H
#define CUI_PRIVATE_DESIGN_METRICS_H
#include "cui/cui_theme.h"
#include <math.h>
/* Private inline resolver: Base-only consumers must not link styled Theme code.
 * Keep this table checked against cui_design_measure by the component tests. */
static inline bool cui_design_valid(cui_design_size size) {
    return size >= CUI_DESIGN_INHERIT && size <= CUI_DESIGN_9XL;
}
static inline bool cui_design_resolve(cui_design_size size, float density,
                                      cui_design_metrics *out) {
    if (!out || size <= CUI_DESIGN_INHERIT || !cui_design_valid(size) || !isfinite(density) ||
        density <= 0 || density > 32)
        return false;
    static const float font[] = {0, 12, 14, 16, 16, 18, 20, 24, 30, 36, 48, 60, 72, 96, 128};
    static const float line[] = {0, 16, 20, 24, 24, 28, 28, 32, 36, 40, 48, 60, 72, 96, 128};
    static const float height[] = {0, 20, 24, 32, 32, 40, 48, 56, 64, 80, 104, 128, 152, 200, 264};
    static const float padding[] = {0, 4, 8, 10, 10, 12, 16, 20, 24, 28, 32, 40, 48, 56, 64};
    static const float gap[] = {0, 4, 4, 6, 6, 8, 8, 12, 12, 16, 20, 24, 28, 32, 40};
    *out = (cui_design_metrics){font[size] * density,
                                line[size] * density,
                                height[size] * density,
                                padding[size] * density,
                                (height[size] - line[size]) * .5f * density,
                                gap[size] * density,
                                font[size] * density};
    return true;
}
#endif
