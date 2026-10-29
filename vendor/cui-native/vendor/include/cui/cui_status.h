/* SPDX-License-Identifier: MIT */
#ifndef CUI_STATUS_H
#define CUI_STATUS_H
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_TAG_SECONDARY,
    CUI_TAG_PRIMARY,
    CUI_TAG_DANGER,
    CUI_TAG_SUCCESS,
    CUI_TAG_WARNING,
    CUI_TAG_INFO,
    CUI_TAG_CUSTOM,
    CUI_TAG_PALETTE
} cui_tag_variant;
typedef enum {
    CUI_STATUS_MEDIUM,
    CUI_STATUS_XSMALL,
    CUI_STATUS_SMALL,
    CUI_STATUS_LARGE
} cui_status_size;
typedef enum {
    CUI_STATUS_COLOR_WHITE,
    CUI_STATUS_COLOR_BLACK,
    CUI_STATUS_COLOR_NEUTRAL,
    CUI_STATUS_COLOR_GRAY,
    CUI_STATUS_COLOR_RED,
    CUI_STATUS_COLOR_ORANGE,
    CUI_STATUS_COLOR_AMBER,
    CUI_STATUS_COLOR_YELLOW,
    CUI_STATUS_COLOR_LIME,
    CUI_STATUS_COLOR_GREEN,
    CUI_STATUS_COLOR_EMERALD,
    CUI_STATUS_COLOR_TEAL,
    CUI_STATUS_COLOR_CYAN,
    CUI_STATUS_COLOR_SKY,
    CUI_STATUS_COLOR_BLUE,
    CUI_STATUS_COLOR_INDIGO,
    CUI_STATUS_COLOR_VIOLET,
    CUI_STATUS_COLOR_PURPLE,
    CUI_STATUS_COLOR_FUCHSIA,
    CUI_STATUS_COLOR_PINK,
    CUI_STATUS_COLOR_ROSE
} cui_status_color;
/* Frozen default-colors palette; false leaves output untouched. */
/* Palette scales are supplied by the application, preserving its named palette.
 * These are the exact scales the reference uses for light/dark tags. */
typedef struct {
    cui_color scale50, scale200, scale300, scale600, scale800, scale950;
    bool dark;
} cui_tag_palette;
bool cui_tag_named_palette(cui_status_color name, bool dark, cui_tag_palette *palette);
typedef struct {
    cui_tag_variant variant;
    cui_status_size size;
    bool outline, custom_radius, pill;
    float radius;
    cui_color background, foreground, border;
    cui_tag_palette palette;
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
} cui_tag_options;
typedef struct {
    cui_color background, foreground, border;
    float font_size, padding_x, padding_y, radius;
} cui_tag_style;
cui_tag_options cui_tag_options_default(void);
/* Pure style resolution; malformed enum/geometry returns false without writing. */
bool cui_tag_resolve(const cui_theme *theme, cui_tag_options options, bool hovered,
                     cui_tag_style *style);
/* Pair end only after a successful begin. Children are application submitted;
 * text children should use the returned foreground/font size. */
/* Requires a unique nonzero ID; false on zero, before opening any boxes. */
bool cui_begin_tag(cui_context *ui, cui_id id, const cui_theme *theme, cui_tag_options options,
                   cui_tag_style *style);
void cui_end_tag(cui_context *ui, cui_id id, const cui_tag_style *style);
/* Requires a unique nonzero ID; zero reports CUI_ERROR_INVALID_ACTION. */
bool cui_tag(cui_context *ui, cui_id id, const cui_theme *theme, cui_str text,
             cui_tag_options options);
typedef enum { CUI_MARKER_PLAIN, CUI_MARKER_SEPARATOR, CUI_MARKER_BORDER } cui_marker_variant;
typedef enum {
    CUI_MARKER_ALIGN_DEFAULT,
    CUI_MARKER_ALIGN_START,
    CUI_MARKER_ALIGN_CENTER,
    CUI_MARKER_ALIGN_END
} cui_marker_alignment;
typedef enum { CUI_MARKER_SPINNER, CUI_MARKER_SHIMMER } cui_marker_loading;
typedef enum {
    CUI_MARKER_ICON,
    CUI_MARKER_TEXT,
    CUI_MARKER_CONTENT,
    CUI_MARKER_DIRECT
} cui_marker_child_kind;
/* Callback receives resolved text style and animated opacity. It owns children
 * and must multiply paint alpha by opacity for rich-only shimmer. */
typedef void (*cui_status_render_fn)(cui_context *, cui_id, void *, float font_size,
                                     cui_color color, float opacity);
typedef struct {
    cui_marker_child_kind kind;
    cui_str text;
    float width;
    cui_status_render_fn render;
    void *user;
} cui_marker_child;
#define CUI_MARKER_MAX_CHILDREN 32
typedef struct {
    cui_rect text_bounds[CUI_MARKER_MAX_CHILDREN];
    float last_phase;
    bool initialized;
} cui_marker_state;
typedef struct {
    cui_marker_variant variant;
    cui_marker_alignment alignment;
    cui_marker_loading loading_style;
    bool loading, reduced_motion, reverse, once, custom_highlight, absolute_spread;
    cui_role role;
    cui_str name;
    float width, shimmer_spread;
    double now_seconds, duration;
    cui_color highlight;
} cui_marker_options;
typedef struct {
    cui_marker_alignment alignment;
    bool spinner, animated;
    float phase, rich_opacity;
    double next_wake_seconds;
} cui_marker_plan;
cui_marker_options cui_marker_options_default(void);
bool cui_base_marker(cui_marker_options options, const cui_marker_child *children, size_t count,
                     cui_marker_plan *plan);
bool cui_marker(cui_context *ui, cui_id id, const cui_theme *theme, cui_marker_state *state,
                const cui_marker_child *children, size_t count, cui_marker_options options);
/* Standalone single-line shimmer. Caller supplies elapsed animation time. */
typedef struct {
    double now_seconds, duration;
    float spread, font_size, max_width;
    bool absolute_spread, reverse, once, reduced_motion, custom_color, custom_highlight;
    cui_color color, highlight;
    cui_role role;
} cui_shimmer_options;
typedef struct {
    cui_rect text_bounds;
    float last_phase;
    bool initialized;
} cui_shimmer_state;
typedef struct {
    float phase;
    bool animated;
    double next_wake_seconds;
} cui_shimmer_plan;
cui_shimmer_options cui_shimmer_options_default(void);
bool cui_base_shimmer(cui_shimmer_options, cui_shimmer_plan *);
/* State remains alive through end_frame; max_width clips without changing glyph positions. */
bool cui_shimmer_text(cui_context *, cui_id, const cui_theme *, cui_shimmer_state *, cui_str,
                      cui_shimmer_options);
#define CUI_NOTIFICATION_MAX_ITEMS 128
typedef struct {
    uint64_t type, key;
} cui_notification_id;
typedef enum {
    CUI_NOTIFICATION_UNTYPED,
    CUI_NOTIFICATION_INFO,
    CUI_NOTIFICATION_SUCCESS,
    CUI_NOTIFICATION_WARNING,
    CUI_NOTIFICATION_ERROR
} cui_notification_kind;
typedef enum {
    CUI_NOTIFICATION_TOP_LEFT,
    CUI_NOTIFICATION_TOP_CENTER,
    CUI_NOTIFICATION_TOP_RIGHT,
    CUI_NOTIFICATION_BOTTOM_LEFT,
    CUI_NOTIFICATION_BOTTOM_CENTER,
    CUI_NOTIFICATION_BOTTOM_RIGHT,
    CUI_NOTIFICATION_LEFT_CENTER,
    CUI_NOTIFICATION_RIGHT_CENTER
} cui_notification_anchor;
typedef enum {
    CUI_NOTIFICATION_IN_APP,
    CUI_NOTIFICATION_SYSTEM,
    CUI_NOTIFICATION_BOTH
} cui_notification_delivery;
typedef enum {
    CUI_NOTIFICATION_STARTING,
    CUI_NOTIFICATION_PRESENT,
    CUI_NOTIFICATION_ENDING
} cui_notification_phase;
typedef void (*cui_notification_callback)(void *user, cui_notification_id id);
typedef struct {
    cui_notification_id id;
    cui_str title, message, icon, action_label;
    cui_notification_kind kind;
    cui_notification_anchor placement;
    cui_notification_delivery delivery;
    bool custom_placement, custom_delivery, autohide;
    cui_status_render_fn content, action;
    void *content_user, *action_user;
    cui_notification_callback on_click, on_close, on_action;
    void *user;
    /* Optional fixed card height; zero measures content after layout. */
    float height;
} cui_notification;
/* Host callbacks implement OS delivery; send false rejects push transactionally.
 * Host owns OS API details. Response calls cui_notification_system_click. */
typedef bool (*cui_notification_send_fn)(void *, const cui_notification *);
typedef struct {
    cui_notification_send_fn send;
    cui_notification_callback retract, activate;
    void *user;
} cui_notification_host;
typedef enum {
    CUI_NOTIFICATION_SHADOW_LEGACY = 0,
    CUI_NOTIFICATION_SHADOW_SOURCE = 1
} cui_notification_shadow_policy;
typedef struct {
    cui_notification_anchor placement;
    cui_notification_delivery delivery;
    float width, margin_left, margin_right, margin_top, margin_bottom;
    size_t max_visible;
    bool reduced_motion;
    cui_notification_shadow_policy shadow_policy; /* zero preserves legacy approximation */
} cui_notification_settings;
typedef enum {
    CUI_NOTIFICATION_MOTION_LEGACY = 0,
    CUI_NOTIFICATION_MOTION_SOURCE = 1
} cui_notification_motion_policy;
/* Pure checked settings validation, including the shadow policy. */
bool cui_notification_settings_valid(cui_notification_settings);
typedef struct {
    cui_notification notification;
    cui_notification_phase phase;
    double elapsed, remaining, last_advance;
    bool system_posted, in_app, explicit_close;
    cui_rect bounds;
    float motion_y, motion_width, velocity_y, velocity_width, visibility, velocity_visibility;
    bool motion_initialized;
} cui_notification_entry;
typedef struct {
    cui_notification_entry *entries;
    size_t count, capacity;
    cui_notification_settings settings;
    cui_notification_host host;
    /* Retained stack bounds and focus IDs remain stable across placement changes. */
    cui_rect stack_bounds[8];
    cui_notification system_registry[100];
    size_t system_count;
    double last_render;
    float stack_height[8], stack_velocity[8];
    bool stack_initialized[8];
    cui_notification_id middle_capture;
    bool middle_pressed;
    /* Post-init opt-in: SOURCE uses 96px/400ms enter and 96px/200ms exit.
     * Independent from settings.shadow_policy; zero preserves legacy motion. */
    cui_notification_motion_policy motion_policy;
} cui_notification_state;
typedef struct {
    bool changed;
    size_t presented, ending, removed;
    double next_wake_seconds;
} cui_notification_advance;
typedef struct {
    float collapsed_height, expanded_height, collapsed_y, expanded_y;
    size_t rank;
} cui_notification_geometry;
cui_notification cui_notification_default(void);
cui_notification_settings cui_notification_settings_default(void);
bool cui_notification_init(cui_notification_state *, cui_notification_entry *, size_t capacity,
                           cui_notification_settings, cui_notification_host);
bool cui_notification_push(cui_notification_state *, cui_notification, double now_seconds);
bool cui_notification_dismiss(cui_notification_state *, cui_notification_id, double now_seconds);
size_t cui_notification_dismiss_type(cui_notification_state *, uint64_t type, double now_seconds);
size_t cui_notification_clear(cui_notification_state *, double now_seconds);
cui_notification_advance cui_notification_update(cui_notification_state *, double now_seconds,
                                                 bool paused);
bool cui_notification_visible(const cui_notification_state *, size_t index);
bool cui_notification_stack_geometry(const float *heights, size_t count, size_t index, bool bottom,
                                     cui_notification_geometry *);
bool cui_notification_system_click(cui_notification_state *, cui_notification_id,
                                   double now_seconds);
/* Render at viewport coordinates. Host owns scheduling returned deadlines.
 * Call once per frame after application content, with a stable overlay ID. */
cui_notification_advance cui_notifications(cui_context *, cui_id, const cui_theme *,
                                           cui_notification_state *, cui_rect viewport,
                                           double now_seconds);
/* Call during an active UI frame, with independent caller-owned output storage.
 * Reject invalid inputs before reductions/callbacks/UI and leave output untouched.
 * Later UI capacity errors cannot undo already accepted owner/host callbacks;
 * end_frame and renderer preflight retain their existing failure contracts. */
bool cui_notifications_checked(cui_context *, cui_id, const cui_theme *, cui_notification_state *,
                               cui_rect viewport, double now_seconds,
                               cui_notification_advance *out);

#ifdef __cplusplus
}
#endif
#endif
