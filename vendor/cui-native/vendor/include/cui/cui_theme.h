/* SPDX-License-Identifier: MIT */
#ifndef CUI_THEME_H
#define CUI_THEME_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Optional application SVG source override; false uses the bundled artwork.
 * Bytes are borrowed only while the icon is prepared. No implicit I/O. */
typedef bool (*cui_theme_icon_loader_fn)(void *user, cui_str path, cui_str *svg);
typedef enum {
    CUI_SCROLLBAR_INHERIT,
    CUI_SCROLLBAR_VISIBLE,
    CUI_SCROLLBAR_HIDDEN,
    CUI_SCROLLBAR_AUTO_HIDE
} cui_scrollbar_visibility;
/* Shared skin. INHERIT preserves each component's legacy bar availability.
 * Zero thickness/minimum keep that component's default. Timing defaults to
 * 1 second idle + .15 second fade; no_fade/reduced_motion hide without fading. */
typedef struct cui_scrollbar_style {
    cui_scrollbar_visibility visibility;
    bool no_track, custom_thumb_color, custom_track_color, no_fade, reduced_motion;
    float thickness, min_thumb, thumb_radius, track_radius, idle_seconds, fade_seconds;
    cui_color thumb_color, track_color;
} cui_scrollbar_style;
typedef enum {
    CUI_THEME_BUTTON_DEFAULT,
    CUI_THEME_BUTTON_PRIMARY,
    CUI_THEME_BUTTON_SECONDARY,
    CUI_THEME_BUTTON_DANGER,
    CUI_THEME_BUTTON_WARNING,
    CUI_THEME_BUTTON_SUCCESS,
    CUI_THEME_BUTTON_INFO,
    CUI_THEME_BUTTON_COUNT
} cui_theme_button_kind;
/* Explicit design scale; INHERIT keeps a component's frozen legacy size. */
typedef enum {
    CUI_DESIGN_INHERIT,
    CUI_DESIGN_XS,
    CUI_DESIGN_SM,
    CUI_DESIGN_BASE,
    CUI_DESIGN_MD,
    CUI_DESIGN_LG,
    CUI_DESIGN_XL,
    CUI_DESIGN_2XL,
    CUI_DESIGN_3XL,
    CUI_DESIGN_4XL,
    CUI_DESIGN_5XL,
    CUI_DESIGN_6XL,
    CUI_DESIGN_7XL,
    CUI_DESIGN_8XL,
    CUI_DESIGN_9XL
} cui_design_size;
typedef struct {
    float font_size, line_height, control_height, padding_x, padding_y, gap, indicator_size;
} cui_design_metrics;
/* Invalid size/density leaves output untouched. BASE and MD share the default metrics. */
bool cui_design_measure(cui_design_size, float density, cui_design_metrics *);
/* Four logical pixels per unit; negative units are useful for offsets, not padding. */
float cui_space(float units);
typedef enum {
    CUI_RADIUS_NONE,
    CUI_RADIUS_XS,
    CUI_RADIUS_SM,
    CUI_RADIUS_MD,
    CUI_RADIUS_LG,
    CUI_RADIUS_XL,
    CUI_RADIUS_2XL,
    CUI_RADIUS_3XL,
    CUI_RADIUS_FULL
} cui_radius_size;
float cui_radius(cui_radius_size, float density);
typedef struct {
    cui_color normal, hover, active, foreground;
    cui_color outline_normal, outline_hover, outline_active, border, outline_border,
        outline_foreground;
} cui_theme_button_colors;
typedef struct {
    float response, damping, epsilon;
    bool travel;
} cui_spring_settings;
typedef struct {
    cui_color background, surface, text, muted, primary, on_primary, border, danger, focus;
    float font_size, padding, gap, radius, control_height;
    /* Semantic intent surfaces and their readable foregrounds. */
    cui_color warning, on_warning, success, on_success, info, on_info, on_danger;
    cui_theme_button_colors buttons[CUI_THEME_BUTTON_COUNT];
    cui_color link, link_hover, link_active, input, input_background;
    cui_color secondary_foreground, accent, accent_foreground;
    cui_spring_settings spring_control;
    cui_color popover, secondary;
    float radius_lg;
    cui_color secondary_hover, tab_foreground, tab_active_foreground, tab_active, tab_bar,
        tab_bar_segmented;
    cui_spring_settings spring_move;
    cui_color switch_background, switch_thumb;
    cui_color popover_foreground, list_hover, list_active, selection;
    bool list_active_highlight;
    cui_color description_list_label, description_list_label_foreground;
    cui_color status_bar, status_bar_border;
    cui_color title_bar, title_bar_border;
    cui_color sidebar, sidebar_foreground, sidebar_border, sidebar_accent,
        sidebar_accent_foreground;
    cui_theme_icon_loader_fn icon_loader;
    void *icon_loader_user;
    uint64_t icon_revision; /* Increment when the same loader supplies new artwork. */
    cui_color caret;
    cui_color group_box, group_box_foreground;
    cui_color muted_background;
    bool hide_focus_ring; /* Keep focused borders while suppressing outside bands. */
    bool dark;            /* Explicit palette mode; independent of individual color luminance. */
    cui_color chart_1, chart_2, chart_3, chart_4, chart_5;
    cui_color chart_bullish, chart_bearish, chart_grid;
    cui_scrollbar_style scrollbar;
} cui_theme;
cui_theme cui_theme_light(void);
cui_theme cui_theme_dark(void);
cui_theme cui_theme_high_contrast(void);
/* Independent, square-cornered amber/ink design system using the same base. */
cui_theme cui_theme_terminal(void);
/* Source popover: ring, offset(0,4) sigma3 spread-1, offset(0,2) sigma2
 * spread-2. Finite strength clamps [0,1]. Copies three values atomically. */
bool cui_theme_popover_shadows(const cui_theme *, float strength, cui_drop_shadow out[3]);
/* Named palette adaptations from the pinned GPUI Kit theme collection.
 * Invalid indices return an empty name/false and leave output unchanged. */
size_t cui_theme_preset_count(void);
cui_str cui_theme_preset_name(size_t index);
bool cui_theme_preset_is_dark(size_t index);
bool cui_theme_preset_get(size_t index, cui_theme *output);
/* Source semantic motion policy; stored separately so legacy themes/consumers
 * remain unchanged. Distances are rem units until explicitly projected. */
typedef struct {
    float x1, y1, x2, y2;
} cui_theme_bezier;
typedef struct {
    float duration_instant, duration_fast, duration_normal, duration_slow;
    cui_theme_bezier easing_enter, easing_exit, easing_move;
    cui_spring_settings spring_control, spring_move;
    float distance_short, distance_medium;
} cui_theme_motion_tokens;
cui_theme_motion_tokens cui_theme_motion_default(void);
bool cui_theme_motion_valid(const cui_theme_motion_tokens *);
/* Copies full policy, converts distances to pixels; reduced motion sets duration,
 * distance and spring travel/response to zero without modifying the input. */
bool cui_theme_motion_project(const cui_theme_motion_tokens *, float rem_pixels,
                              bool reduced_motion, cui_theme_motion_tokens *);
bool cui_theme_bezier_sample(cui_theme_bezier, float progress, float *output);
#ifdef __cplusplus
}
#endif
#endif
