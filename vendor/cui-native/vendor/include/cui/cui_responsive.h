/* SPDX-License-Identifier: MIT */
#ifndef CUI_RESPONSIVE_H
#define CUI_RESPONSIVE_H
#include <math.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Container-local logical units, independent of framebuffer density. */
typedef enum {
    CUI_BREAKPOINT_BASE,
    CUI_BREAKPOINT_SM,
    CUI_BREAKPOINT_MD,
    CUI_BREAKPOINT_LG,
    CUI_BREAKPOINT_XL,
    CUI_BREAKPOINT_2XL
} cui_breakpoint;
typedef struct {
    float sm, md, lg, xl, xxl;
} cui_breakpoints;
typedef struct {
    float width;
    cui_breakpoint breakpoint;
    bool valid;
} cui_responsive;
/* NULL selects 640/768/1024/1280/1536. Invalid widths/thresholds produce an invalid
 * zero-width BASE query. Equal thresholds intentionally allow skipped classes. */
static inline cui_responsive cui_responsive_query(float width, const cui_breakpoints *custom) {
    const cui_breakpoints defaults = {640, 768, 1024, 1280, 1536};
    cui_breakpoints b = custom ? *custom : defaults;
    cui_responsive r = {0, CUI_BREAKPOINT_BASE, false};
    if (!isfinite(width) || width < 0 || !isfinite(b.sm) || !isfinite(b.md) || !isfinite(b.lg) ||
        !isfinite(b.xl) || !isfinite(b.xxl) || b.sm < 0 || b.md < b.sm || b.lg < b.md ||
        b.xl < b.lg || b.xxl < b.xl)
        return r;
    r.width = width;
    r.valid = true;
    r.breakpoint = width >= b.xxl  ? CUI_BREAKPOINT_2XL
                   : width >= b.xl ? CUI_BREAKPOINT_XL
                   : width >= b.lg ? CUI_BREAKPOINT_LG
                   : width >= b.md ? CUI_BREAKPOINT_MD
                   : width >= b.sm ? CUI_BREAKPOINT_SM
                                   : CUI_BREAKPOINT_BASE;
    return r;
}
static inline bool cui_responsive_at_least(cui_responsive r, cui_breakpoint breakpoint) {
    return r.valid && breakpoint >= CUI_BREAKPOINT_BASE && breakpoint <= CUI_BREAKPOINT_2XL &&
           r.breakpoint >= breakpoint;
}
/* Select a dimension; the same named condition can select row/column or whether
 * a subtree is submitted using at_least. Arguments are BASE value first,
 * at-or-above-breakpoint value second. Values remain caller-owned. */
static inline float cui_responsive_value(cui_responsive r, cui_breakpoint breakpoint, float base,
                                         float expanded) {
    return cui_responsive_at_least(r, breakpoint) ? expanded : base;
}
/* Minimum pane widths express content constraints without another breakpoint. */
static inline bool cui_responsive_fits(cui_responsive r, float first, float second, float gap) {
    return r.valid && isfinite(first) && isfinite(second) && isfinite(gap) && first >= 0 &&
           second >= 0 && gap >= 0 && first <= r.width && second <= r.width - first &&
           gap <= r.width - first - second;
}
#ifdef __cplusplus
}
#endif
#endif
