/* SPDX-License-Identifier: MIT */
#ifndef CUI_MEDIA_H
#define CUI_MEDIA_H
#include "cui_content.h"
#include "cui_scroll.h"
#include "cui_status.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
/* All pixels and frame arrays are borrowed; retain them through frame completion. */
typedef struct {
    const unsigned char *pixels;
    uint32_t width, height;
    size_t stride;
    uint64_t revision;
    double duration;
} cui_image_frame;
typedef enum { CUI_IMAGE_PENDING, CUI_IMAGE_READY, CUI_IMAGE_FAILED } cui_image_status;
typedef enum {
    CUI_IMAGE_ASSET,
    CUI_IMAGE_PATH,
    CUI_IMAGE_URI,
    CUI_IMAGE_ENCODED,
    CUI_IMAGE_CUSTOM
} cui_image_source_kind;
typedef struct {
    cui_image_source_kind kind;
    cui_str name;
    const void *bytes;
    size_t byte_count;
    uint64_t identity;
} cui_image_source;
typedef struct {
    cui_image_status status;
    const cui_image_frame *frames;
    size_t count;
} cui_image_load;
/* Poll must reuse its host-side request/cache, rather than starting a request each frame. */
typedef cui_image_load (*cui_image_loader_fn)(void *, cui_image_source);
typedef enum {
    CUI_IMAGE_CONTAIN,
    CUI_IMAGE_COVER,
    CUI_IMAGE_FILL,
    CUI_IMAGE_SCALE_DOWN,
    CUI_IMAGE_NONE
} cui_image_fit;
typedef struct {
    uint64_t identity;
    cui_image_status status;
    const cui_image_frame *frames;
    size_t count, frame;
    double started, last_time, elapsed;
    bool initialized;
} cui_image_state;
typedef struct {
    float width, height, aspect_ratio;
    cui_image_fit fit;
} cui_image_geometry;
typedef struct {
    float width, height;
    cui_rect source, destination;
} cui_image_layout;
typedef struct {
    bool identified, active, reduced_motion;
    double now_seconds;
} cui_image_clock;
typedef struct {
    bool valid, loading_visible, changed;
    cui_image_status status;
    const cui_image_frame *frame;
    double next_wake_seconds;
} cui_image_result;
bool cui_image_frame_valid(cui_image_frame);
bool cui_image_layout_resolve(cui_image_geometry, uint32_t intrinsic_width,
                              uint32_t intrinsic_height, cui_image_layout *);
void cui_image_state_reset(cui_image_state *);
cui_image_result cui_image_update(cui_image_state *, cui_image_source, cui_image_loader_fn, void *,
                                  cui_image_clock);
typedef enum {
    CUI_ATTACHMENT_PENDING,
    CUI_ATTACHMENT_UPLOADING,
    CUI_ATTACHMENT_PROCESSING,
    CUI_ATTACHMENT_FAILED,
    CUI_ATTACHMENT_COMPLETE
} cui_attachment_status;
typedef enum {
    CUI_MEDIA_MEDIUM,
    CUI_MEDIA_XSMALL,
    CUI_MEDIA_SMALL,
    CUI_MEDIA_LARGE,
    CUI_MEDIA_CUSTOM
} cui_media_size;
typedef struct {
    float height, chip_width, media, glyph, padding_start, padding_end, card_padding, gap, text,
        description;
} cui_attachment_metrics;
bool cui_attachment_metrics_resolve(cui_media_size, float root_font_size, float custom_density,
                                    cui_attachment_metrics *);
bool cui_attachment_in_progress(cui_attachment_status);
typedef struct {
    cui_attachment_status status;
    cui_media_size size;
    bool vertical, media, image, content, actions, identified;
    bool remove, retry, touch, hovered, progress_known;
    float progress, root_font_size, custom_density, width, media_aspect;
    bool title_status_override, description_status_override;
    cui_attachment_status title_status, description_status;
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
} cui_attachment_model;
typedef struct {
    cui_attachment_metrics metrics;
    float width, height, media_width, media_height, overhang, progress, scrim;
    bool flush, remove_visible, retry_media, retry_description, progress_ring, progress_bar,
        spinner, rejection, title_loading, description_failed, append_percentage, dashed;
} cui_attachment_plan;
cui_attachment_model cui_attachment_model_default(void);
bool cui_base_attachment(cui_attachment_model, cui_attachment_plan *);
typedef void (*cui_media_render_fn)(cui_context *, cui_id, void *, float width, float height);
typedef struct {
    cui_image_geometry geometry;
    cui_corner_radii corners;
    bool grayscale;
    float opacity;
    cui_str description;
    cui_media_render_fn loading, fallback;
    void *user;
    cui_image_clock clock;
} cui_image_options;
cui_image_options cui_image_options_default(void);
cui_image_result cui_image(cui_context *, cui_id, cui_image_state *, cui_image_source,
                           cui_image_loader_fn, void *, cui_image_options);
typedef enum {
    CUI_ATTACHMENT_TITLE,
    CUI_ATTACHMENT_DESCRIPTION,
    CUI_ATTACHMENT_CUSTOM
} cui_attachment_child_kind;
typedef struct {
    cui_attachment_child_kind kind;
    cui_str text;
    bool status_override;
    cui_attachment_status status;
    bool custom_color, custom_shimmer;
    cui_color color;
    float font_size;
    cui_marker_options shimmer;
    cui_media_render_fn render;
    void *user;
} cui_attachment_child;
#define CUI_ATTACHMENT_MAX_CHILDREN 128
typedef struct {
    cui_rect card, media;
    cui_marker_state titles[CUI_ATTACHMENT_MAX_CHILDREN];
    cui_hover_state tooltip;
} cui_attachment_state;
typedef void (*cui_attachment_action_fn)(void *, cui_id);
typedef struct {
    cui_attachment_model model;
    const cui_attachment_child *children;
    size_t child_count;
    cui_media_render_fn media_content, media_overlay, actions;
    void *media_user, *overlay_user, *actions_user;
    float actions_width;
    cui_image_state *image_state;
    cui_image_source image_source;
    cui_image_loader_fn image_loader;
    void *image_user;
    cui_attachment_action_fn on_click, on_remove, on_retry;
    void *action_user;
    cui_str name, remove_label, retry_label, tooltip;
    float delta_seconds;
    cui_hover_options tooltip_options;
    double now_seconds;
    bool reduced_motion, active;
} cui_attachment_options;
typedef struct {
    bool valid, clicked, removed, retried;
    cui_attachment_plan plan;
} cui_attachment_result;
cui_attachment_options cui_attachment_options_default(void);
cui_id cui_attachment_remove_id(cui_id);
cui_id cui_attachment_retry_id(cui_id);
cui_attachment_result cui_attachment(cui_context *, cui_id, const cui_theme *,
                                     cui_attachment_state *, cui_attachment_options);
typedef struct {
    cui_scroll_state scroll;
    cui_rect viewport, content;
    float leading, trailing, leading_start, trailing_start, leading_target, trailing_target;
    double leading_epoch, trailing_epoch;
    bool initialized;
} cui_attachment_group_state;
typedef struct {
    float width, height, gap, wheel_step;
    bool edge_fade, reduced_motion;
    cui_color fade_color;
    double now_seconds;
    cui_scroll_state *external_scroll;
    cui_media_render_fn children;
    void *user;
    const cui_scrollbar_style *scrollbar_style;
    const cui_theme
        *scrollbar_theme; /* Optional borrowed palette; this API has no theme argument. */
} cui_attachment_group_options;
typedef struct {
    bool valid, changed;
    float maximum, leading_opacity, trailing_opacity;
    double next_wake_seconds;
} cui_attachment_group_result;
cui_attachment_group_options cui_attachment_group_options_default(void);
cui_attachment_group_result cui_attachment_group(cui_context *, cui_id,
                                                 cui_attachment_group_state *,
                                                 cui_attachment_group_options);
#ifdef __cplusplus
}
#endif
#endif
