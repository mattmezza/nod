/* SPDX-License-Identifier: MIT */
#ifndef CUI_STYLED_SLIDER_EVENTS_H
#define CUI_STYLED_SLIDER_EVENTS_H
#include "cui_slider_events.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float width;        /* Zero defaults240 logical units; positive fixed width. */
    float track_height; /* Zero defaults8; visual bar. */
    float hit_height;   /* Zero defaults24; >= resolved track_height. */
    float font_size;    /* Zero uses theme.font_size. */
    bool hide_label;    /* Semantic name remains supplied. */
    bool custom_colors;
    cui_color track_color, fill_color;
} cui_slider_style_options;
/* Additive double controlled Styled Slider. id is the hit rectangle: its x/width
 * exactly match the visual bar. Pointer reduction uses previous completed layout.
 * Preview paints only during dragging; otherwise the immutable entry value paints.
 * Acceptance appears next submission. Base event/lifetime/count rules apply.
 * Invalid input submits nothing and does not mutate capture state or dispatch.
 * No legacy float slider behavior changes; no thumb/vertical variant is implied. */
cui_component_slider_result cui_slider_events(cui_context *, cui_id, const cui_theme *,
                                              cui_str name, double value, cui_slider_state *,
                                              cui_slider_options, cui_slider_style_options,
                                              cui_component_slider_handler);
#ifdef __cplusplus
}
#endif
#endif
