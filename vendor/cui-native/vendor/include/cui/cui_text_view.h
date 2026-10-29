/* SPDX-License-Identifier: MIT */
#ifndef CUI_TEXT_VIEW_H
#define CUI_TEXT_VIEW_H
#include "cui.h"
#include "cui_platform.h"
#include "cui_scroll.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
#define CUI_TEXT_VIEW_BYTE_LIMIT 65536u
#define CUI_TEXT_VIEW_LIST_DEPTH_LIMIT 32
#define CUI_TEXT_VIEW_NODE_LIMIT 8192u
typedef enum {
    CUI_TEXT_VIEW_MARKDOWN,
    CUI_TEXT_VIEW_HTML,
    CUI_TEXT_VIEW_PLAIN
} cui_text_view_format;
typedef enum {
    CUI_TEXT_VIEW_OK,
    CUI_TEXT_VIEW_INVALID,
    CUI_TEXT_VIEW_CAPACITY,
    CUI_TEXT_VIEW_UNSUPPORTED
} cui_text_view_error;
typedef struct {
    size_t begin, end;
} cui_text_view_range;
enum {
    CUI_TEXT_VIEW_BOLD = 1u,
    CUI_TEXT_VIEW_ITALIC = 2u,
    CUI_TEXT_VIEW_CODE = 4u,
    CUI_TEXT_VIEW_STRIKE = 8u,
    CUI_TEXT_VIEW_MARK = 16u,
    CUI_TEXT_VIEW_MEDIUM = 32u,
    CUI_TEXT_VIEW_SEMIBOLD = 64u,
    CUI_TEXT_VIEW_INLINE_IMAGE = 128u
};
typedef enum {
    CUI_TEXT_VIEW_PARAGRAPH,
    CUI_TEXT_VIEW_HEADING,
    CUI_TEXT_VIEW_QUOTE,
    CUI_TEXT_VIEW_LIST_ITEM,
    CUI_TEXT_VIEW_CODE_BLOCK,
    CUI_TEXT_VIEW_RULE,
    CUI_TEXT_VIEW_TABLE_CELL,
    CUI_TEXT_VIEW_IMAGE
} cui_text_view_block_kind;
typedef struct {
    cui_text_view_range source;
    unsigned flags;
    float born;
} cui_text_view_mapping;
typedef struct {
    float value;
    bool specified, relative;
} cui_text_view_image_length;
typedef struct {
    cui_text_view_range text, source, link;
    size_t block;
    unsigned flags;
    cui_text_view_range resource, alt;
    cui_bitmap image;
    bool image_ready;
    cui_rect image_bounds;
    cui_text_view_image_length image_width, image_height;
} cui_text_view_span;
typedef enum {
    CUI_TEXT_VIEW_ALIGN_LEFT,
    CUI_TEXT_VIEW_ALIGN_CENTER,
    CUI_TEXT_VIEW_ALIGN_RIGHT
} cui_text_view_alignment;
typedef struct {
    cui_text_view_block_kind kind;
    cui_text_view_range text, source, resource;
    unsigned level, table, row, column, table_columns;
    cui_text_view_range list_marker;
    unsigned list_number, list_start, list_depth, list_source_indent;
    size_t list_parent;
    char marker_text[12], prefix_text[16];
    bool table_header, image_ready, task, checked, list_item, list_ordered, list_continuation,
        list_repeat_marker;
    cui_text_view_alignment alignment;
    cui_bitmap image;
    cui_rect bounds, table_frame, image_line;
    float table_track_width;
    cui_scroll_state table_scroll;
    float actions_height, text_inset, list_prefix_width;
    cui_text_view_range projected_selection, projected_next;
} cui_text_view_block;
typedef struct {
    cui_text_view_range text;
    size_t span, block;
    cui_rect bounds;
    float font_size, line_height;
    unsigned flags;
    float line_top;
} cui_text_view_glyph;
typedef struct {
    cui_text_view_range range;
    cui_color background;
} cui_text_view_highlight;
typedef struct {
    char *source, *rendered;
    size_t source_capacity, rendered_capacity;
    cui_text_view_mapping *mapping;
    size_t mapping_capacity;
    cui_text_view_span *spans;
    size_t span_capacity;
    cui_text_view_block *blocks;
    size_t block_capacity;
    cui_text_view_glyph *glyphs;
    size_t glyph_capacity;
    cui_text_view_highlight *highlights;
    size_t highlight_capacity;
} cui_text_view_workspace;
typedef enum {
    CUI_TEXT_VIEW_FADE_EASE_OUT_CUBIC,
    CUI_TEXT_VIEW_FADE_LINEAR,
    CUI_TEXT_VIEW_FADE_EASE_IN_OUT_CUBIC
} cui_text_view_fade_easing;
typedef struct {
    float duration, stagger;
    cui_text_view_fade_easing easing;
} cui_text_view_motion;
typedef struct {
    cui_text_view_workspace workspace;
    cui_text_view_format format;
    size_t source_length, text_length, span_count, block_count, glyph_count, highlight_count;
    size_t anchor, cursor;
    bool selecting, dragged, clamped, stream_fade, reduced_motion, selection_projected,
        selection_override, multi_click_release;
    float pointer_x, pointer_y, press_x, press_y, selection_x, selection_y;
    cui_id secondary_link;
    float time, fade_duration, fade_stagger, width, height;
    cui_text_view_fade_easing fade_easing;
    cui_scroll_state scroll;
    cui_rect viewport;
    bool reveal_pending;
    cui_text_view_range reveal;
    float reveal_age;
} cui_text_view_state;
bool cui_text_view_init(cui_text_view_state *, cui_text_view_workspace, cui_text_view_format);
/* Safe, bounded subset parser. No scripts, networking, filesystem access or allocation.
 * Capacity/UTF-8 errors leave the existing document unchanged. */
cui_text_view_error cui_text_view_set_text(cui_text_view_state *, cui_str);
cui_str cui_text_view_source(const cui_text_view_state *);
cui_str cui_text_view_rendered_text(const cui_text_view_state *);
/* Decode a resource source range into a separate caller buffer. Supports the
 * parser's entity subset and Markdown punctuation escapes; HTML keeps backslashes.
 * Capacity failure reports required bytes excluding zero and leaves output intact. */
cui_text_view_error cui_text_view_decode_resource(const cui_text_view_state *,
                                                  cui_text_view_range source, char *output,
                                                  size_t capacity, size_t *length);
bool cui_text_view_select(cui_text_view_state *, size_t anchor, size_t cursor);
/* Frozen multi-click granularity: 2 selects a bounded word, >=3 a logical line.
 * Offsets clip left to UTF-8 boundaries, and image-separated text runs are isolated.
 * Counts below 2, image offsets and empty ranges return false without writing out. */
bool cui_text_view_selection_range_at(const cui_text_view_state *, size_t offset,
                                      unsigned click_count, cui_text_view_range *out);
/* Borrowed coordinate slice from workspace.rendered, valid until the next text mutation.
 * Includes one-byte image slots; use copy_selection(PLAIN) for image-free text output. */
cui_str cui_text_view_selected_text(const cui_text_view_state *);
typedef enum { CUI_TEXT_VIEW_COPY_PLAIN, CUI_TEXT_VIEW_COPY_SOURCE } cui_text_view_copy_format;
/* Writes a zero-terminated copy. On capacity failure, *length reports required
 * bytes excluding zero and output is untouched. HTML Source falls back to Plain.
 * Output must be separate from document workspace.
 * Source select-all is verbatim; partial unsupported blocks return UNSUPPORTED. */
cui_text_view_error cui_text_view_copy_selection(const cui_text_view_state *,
                                                 cui_text_view_copy_format, char *output,
                                                 size_t capacity, size_t *length);
/* Source-equivalent selected block serialization, used by styled clipboard delivery.
 * Plain includes paragraph/heading/code terminators and table row/cell structure. */
cui_text_view_error cui_text_view_copy_document_selection(const cui_text_view_state *,
                                                          cui_text_view_copy_format, char *, size_t,
                                                          size_t *length);
bool cui_text_view_selected_source_range(const cui_text_view_state *, cui_text_view_range *);
bool cui_text_view_range_for_source(const cui_text_view_state *, cui_text_view_range,
                                    cui_text_view_range *);
cui_text_view_error cui_text_view_set_highlights(cui_text_view_state *,
                                                 const cui_text_view_highlight *, size_t);
cui_text_view_error cui_text_view_reveal(cui_text_view_state *, cui_text_view_range);
/* Paint clock is application-driven. Returns true while animation/reveal needs a frame. */
bool cui_text_view_advance(cui_text_view_state *, float dt);
/* Default styled stream policy: .35 seconds, no stagger, ease-out cubic.
 * Stagger reveals words with trailing whitespace, or individual CJK scalars.
 * If an updated text leaf would start its last unit after duration, that leaf
 * fades as one chunk. Zero duration snaps. Invalid policy leaves state intact.
 * Set before set_text so a streamed update receives the chosen birth times. */
cui_text_view_motion cui_text_view_motion_default(void);
bool cui_text_view_set_motion(cui_text_view_state *, cui_text_view_motion);
/* Sample a rendered byte's alpha multiplier; invalid offsets, disabled stream
 * fade and reduced motion return one. No mutation or platform dependence. */
float cui_text_view_stream_opacity(const cui_text_view_state *, size_t rendered_byte);
typedef cui_size (*cui_text_view_measure_fn)(void *, cui_str, float font_size, unsigned flags);
typedef bool (*cui_text_view_font_metrics_fn)(void *, float font_size, unsigned flags,
                                              cui_font_metrics *);
typedef enum {
    CUI_TEXT_VIEW_TABLE_WRAP,
    CUI_TEXT_VIEW_TABLE_ADAPTIVE,
    CUI_TEXT_VIEW_TABLE_NOWRAP
} cui_text_view_table_mode;
/* Application-owned shared selection. Registered states/workspaces must remain alive until
 * removed. Registration storage remains alive until the manager is discarded. */
typedef struct {
    cui_text_view_state *state;
    cui_id id, scope;
    uint64_t document_order;
    cui_text_view_table_mode table_mode;
    bool enabled;
} cui_text_view_selection_member;
typedef struct {
    cui_text_view_selection_member *members;
    size_t count, capacity;
    cui_id owner, scope;
    float anchor_x, anchor_y, cursor_x, cursor_y;
    bool active, dragging, dragged;
} cui_text_view_selection;
bool cui_text_view_selection_init(cui_text_view_selection *, cui_text_view_selection_member *,
                                  size_t capacity);
bool cui_text_view_selection_register(cui_text_view_selection *, cui_text_view_selection_member);
bool cui_text_view_selection_remove(cui_text_view_selection *, cui_id);
void cui_text_view_selection_clear(cui_text_view_selection *);
/* Projects retained window-space points through current viewport/scroll geometry. */
bool cui_text_view_selection_project(cui_text_view_selection *, cui_id scope, float anchor_x,
                                     float anchor_y, float cursor_x, float cursor_y);
cui_text_view_error cui_text_view_selection_copy(const cui_text_view_selection *,
                                                 cui_text_view_copy_format, char *, size_t,
                                                 size_t *length);
/* Layout dimensions and measured text widths are bounded to 10,000,000 logical pixels. */
typedef struct {
    float width, font_size, line_height, paragraph_gap;
    float code_actions_height, table_actions_height;
    size_t max_lines;
    bool scrollable;
    cui_text_view_table_mode table_mode;
    bool font_metrics_available;
    cui_font_metrics body_metrics;
    cui_text_view_font_metrics_fn font_metrics;
    void *font_metrics_user;
} cui_text_view_layout_options;
cui_text_view_error cui_text_view_layout(cui_text_view_state *, cui_text_view_layout_options,
                                         cui_text_view_measure_fn, void *);
size_t cui_text_view_hit(const cui_text_view_state *, float x, float y);
typedef struct {
    cui_color foreground, muted, link, selection, code_background, border, mark, table_background;
    float font_size, line_height, paragraph_gap;
} cui_text_view_style;
cui_text_view_style cui_text_view_style_default(void);
cui_text_view_style cui_text_view_style_from_theme(const cui_theme *);
typedef struct {
    float width, height, dt;
    cui_text_view_table_mode table_mode;
    float code_actions_height, table_actions_height;
    bool selectable, scrollable, disabled, stream_fade, reduced_motion;
    size_t max_lines;
    bool custom_style;
    cui_text_view_style style;
    bool custom_motion;
    cui_text_view_motion motion;
    const cui_platform *clipboard;
    cui_text_view_copy_format copy_format;
    char *copy_buffer;
    size_t copy_capacity;
    /* Decoded URIs are borrowed only during each callback. Copy for deferred use. */
    void (*on_link)(cui_str url, const cui_event *, void *);
    bool (*open_url)(cui_str url, void *);
    void *link_user;
    bool (*image)(cui_str uri, cui_bitmap *out, void *);
    void *image_user;
    void (*code_actions)(cui_context *, cui_id actions_id, const cui_theme *, cui_str code,
                         cui_str language, void *);
    void (*table_actions)(cui_context *, cui_id actions_id, const cui_theme *,
                          const cui_text_view_state *, unsigned table, void *);
    void *actions_user;
    bool (*highlighter)(cui_str code, cui_str language, size_t code_byte, cui_color *foreground,
                        void *);
    void *highlighter_user;
    void (*on_reveal)(cui_rect window_line, void *);
    void *reveal_user;
    cui_text_view_selection *shared_selection;
    const cui_scrollbar_style *scrollbar_style;
} cui_text_view_options;
cui_text_view_options cui_text_view_default(void);
/* Call once after begin_frame and before any member is painted. Uses retained layout;
 * first-frame members cannot be hit until they have submitted geometry. Clipboard options
 * are read from options; member selection flags/lifetimes come from registration. */
bool cui_text_view_selection_update(cui_context *, cui_text_view_selection *,
                                    cui_text_view_options);
/* Retained per-view selection and document render. Shared window selection,
 * touch handles and complete CommonMark/HTML5 parsing are tracked separately. */
bool cui_text_view(cui_context *, cui_id, const cui_theme *, cui_text_view_state *,
                   cui_text_view_options);
#ifdef __cplusplus
}
#endif
#endif
