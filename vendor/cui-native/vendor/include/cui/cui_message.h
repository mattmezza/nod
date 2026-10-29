/* SPDX-License-Identifier: MIT */
#ifndef CUI_MESSAGE_H
#define CUI_MESSAGE_H
#include "cui_content.h"
#include "cui_scroll.h"
#include "cui_virtual.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { CUI_MESSAGE_START, CUI_MESSAGE_END } cui_message_alignment;
typedef enum {
    CUI_MESSAGE_INSET_DEFAULT,
    CUI_MESSAGE_INSET_ON,
    CUI_MESSAGE_INSET_OFF
} cui_message_inset;
typedef struct {
    float width, font_size;
    cui_color color;
    cui_message_alignment alignment;
} cui_message_style;
typedef void (*cui_message_render_fn)(cui_context *, cui_id, void *, cui_message_style);
typedef struct {
    bool present;
    cui_str text;
    float width, height;
    cui_message_inset inset;
    cui_message_render_fn render;
    void *user;
} cui_message_slot;
typedef struct {
    cui_message_slot slot;
    bool typed_bubble;
    cui_bubble_variant bubble;
} cui_message_body;
typedef struct {
    float width, stack_gap, row_gap, metadata_gap;
    cui_message_alignment alignment;
    cui_role role;
    cui_str name;
    cui_message_slot avatar, header, footer;
    const cui_message_body *body;
    size_t body_count;
    /* Optional compact column cap; zero preserves the legacy available width.
     * Row alignment keeps outgoing content at the end of the full message row. */
    float max_bubble_width;
} cui_message_options;
typedef struct {
    bool avatar, header, footer, ghost, header_inset, footer_inset;
    float column_width, footer_offset;
} cui_message_plan;
cui_message_options cui_message_options_default(void);
/* Pure composition validation/geometry. No theme implementation dependency. */
bool cui_base_message(cui_message_options, cui_message_plan *);
bool cui_message(cui_context *, cui_id, const cui_theme *, cui_message_options);
void cui_begin_message_group(cui_context *, cui_id, float width, float gap);
void cui_end_message_group(cui_context *);
/* Retains only layout/scroll bookkeeping, never application message records. */
typedef struct cui_message_scroller_state cui_message_scroller_state;
typedef struct {
    size_t capacity, item_count;
    double estimated_height;
} cui_message_scroller_config;
typedef struct {
    double viewport_height, padding_top, padding_bottom, row_gap;
} cui_message_scroller_metrics;
typedef struct {
    size_t begin, end;
    double scroll, total, before;
    bool following_tail, scrolled_up;
} cui_message_scroller_view;
cui_message_scroller_state *cui_message_scroller_create(cui_message_scroller_config);
void cui_message_scroller_destroy(cui_message_scroller_state *);
size_t cui_message_scroller_count(const cui_message_scroller_state *);
size_t cui_message_scroller_memory_bytes(const cui_message_scroller_state *);
bool cui_message_scroller_following(const cui_message_scroller_state *);
bool cui_message_scroller_scrolled_up(const cui_message_scroller_state *);
bool cui_message_scroller_reset(cui_message_scroller_state *, size_t count);
bool cui_message_scroller_splice(cui_message_scroller_state *, size_t begin, size_t end,
                                 size_t count);
bool cui_message_scroller_append(cui_message_scroller_state *, size_t count);
bool cui_message_scroller_prepend(cui_message_scroller_state *, size_t count);
bool cui_message_scroller_remeasure(cui_message_scroller_state *);
bool cui_message_scroller_remeasure_items(cui_message_scroller_state *, size_t begin, size_t end);
bool cui_message_scroller_to_item(cui_message_scroller_state *, size_t index);
void cui_message_scroller_to_end(cui_message_scroller_state *);
bool cui_message_scroller_scroll(cui_message_scroller_state *, double offset);
bool cui_message_scroller_measure(cui_message_scroller_state *, size_t index, double height);
bool cui_message_scroller_layout(cui_message_scroller_state *, cui_message_scroller_metrics,
                                 cui_message_scroller_view *);
/* Renderer is called only for visible rows plus 400px overdraw. Collection count
 * and state must agree; do not mutate either until cui_end_frame completes. */
typedef void (*cui_message_row_render_fn)(cui_context *, cui_id, size_t index, void *, float width);
typedef struct {
    float width, height, inset_left, inset_right, padding_top, padding_bottom, row_gap,
        bar_thickness, wheel_step;
    bool scrollbar, jump_button, bottom_fade, reduced_motion;
    double now_seconds, jump_transition;
    cui_color fade_color;
    cui_str name, jump_label;
    /* Adjust a configured button's appearance without replacing its action. */
    cui_message_slot jump_content;
    float jump_width, jump_height;
    const cui_scrollbar_style *scrollbar_style;
} cui_message_scroller_options;
typedef struct {
    cui_message_scroller_view view;
    bool valid, changed, jumped, jump_disabled;
    float jump_opacity, fade_opacity;
    double next_wake_seconds;
} cui_message_scroller_result;
cui_message_scroller_options cui_message_scroller_options_default(void);
bool cui_message_scroller_validate_options(cui_message_scroller_options);
cui_id cui_message_scroller_jump_id(cui_id);
cui_message_scroller_result cui_base_message_scroller(cui_context *, cui_id,
                                                      cui_message_scroller_state *,
                                                      cui_message_scroller_options);
cui_message_scroller_result cui_message_scroller(cui_context *, cui_id, const cui_theme *,
                                                 cui_message_scroller_state *,
                                                 cui_message_row_render_fn, void *,
                                                 cui_message_scroller_options);
#ifdef __cplusplus
}
#endif
#endif
