/* SPDX-License-Identifier: MIT */
#ifndef CUI_SCROLL_H
#define CUI_SCROLL_H
#include "cui.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float x, y, grab_x, grab_y;
    bool dragging_x, dragging_y;
} cui_scroll_state;
typedef struct {
    float width, height, content_width, content_height, bar_thickness, wheel_step, gap;
    bool horizontal, vertical, disabled, content_row;
    /* Hide tracks and their controls without disabling wheel scrolling. */
    bool hide_scrollbars;
    /* Route gestures by their starting axis when exactly one axis is enabled. */
    bool axis_lock;
    bool gesture_lock;    /* Persistent precise axis lock; disabled on WASM. */
    bool trap_horizontal; /* Opt-in horizontal mask edge ownership. */
    /* Borrowed for this call/frame; NULL inherits the styled component's theme
     * or keeps legacy behavior in a standalone Base call. Never retained. */
    const cui_scrollbar_style *scrollbar_style;
    /* Measure FIT content on enabled axes when the corresponding explicit
     * content dimension is nonpositive/nonfinite. Uses preceding-frame layout;
     * the first frame starts at zero range. Explicit dimensions always win. */
    bool auto_measure_content;
} cui_scroll_options;
typedef struct {
    cui_size viewport;
    float maximum_x, maximum_y;
} cui_scroll_geometry;
cui_scroll_geometry cui_scroll_layout(cui_scroll_options);
/* Drawing-free, ordered wheel reduction and scrollbar semantics/behavior. */
bool cui_base_scrollable(cui_context *, cui_id, cui_scroll_state *, cui_scroll_options);
/* Reduce scrolls deferred by children, without repeating semantic submission
 * or keyboard/pointer scrollbar handling. Handled wheel events are consumed. */
bool cui_base_scrollable_finish(cui_context *, cui_id, cui_scroll_state *, cui_scroll_options);
/* Compose a scrollbar for an existing scroll owner without registering another group. */
bool cui_base_scrollbar(cui_context *, cui_id, cui_scroll_state *, cui_scroll_geometry,
                        cui_scroll_options, bool vertical);
/* Drawing only; call the base helper before painting the owner. */
void cui_scrollbar(cui_context *, cui_id, const cui_theme *, const cui_scroll_state *,
                   cui_scroll_geometry, cui_scroll_options, bool vertical);
cui_id cui_scrollbar_id(cui_id, bool vertical);
cui_rect cui_scrollbar_thumb(cui_rect track, float scroll, float viewport, float content,
                             bool vertical);
cui_rect cui_scrollbar_thumb_ex(cui_rect track, float scroll, float viewport, float content,
                                bool vertical, float minimum);
/* Resolve shared skin before geometry and Base behavior. Legacy hide wins. */
cui_scroll_options cui_scroll_resolve(const cui_theme *, cui_scroll_options);
/* Shared bounded context-owned idle/fade state, driven by frame_time. Calling
 * repeatedly during one frame does not advance time. No style/visible allocates
 * no timer. Hidden/zero-opacity bars must not submit pointer semantics. */
float cui_scrollbar_opacity(cui_context *, cui_id owner, cui_id scrollbar,
                            const cui_scrollbar_style *, float value, bool dragging);
/* Opt-in bars for a scrollable that had no bars by default. Call inside its
 * viewport box after content, then apply changed offsets to that box. Local
 * viewport coordinates are local to the current box; compensate a translated
 * content box's position when necessary. Content extents are finite.
 * Owns bounded drag state in the context, retains no caller pointers. */
bool cui_scrollbar_overlay(cui_context *, cui_id owner, const cui_theme *, float *x, float *y,
                           cui_rect viewport, cui_size content, cui_scroll_options);
/* Split form for editors/virtual content: reduce before laying out the current
 * offset, then paint after content inside the viewport. Base needs a resolved
 * override style; INHERIT keeps the no-bar legacy. Paint takes final model
 * offsets so deferred scrolling and discrete windows retain correct thumbs. */
bool cui_base_scrollbar_overlay(cui_context *, cui_id owner, float *x, float *y, cui_rect viewport,
                                cui_size content, cui_scroll_options);
void cui_scrollbar_overlay_paint(cui_context *, cui_id owner, const cui_theme *, float x, float y,
                                 cui_rect viewport, cui_size content, cui_scroll_options);
/* Pair end only when begin succeeds. Children are clipped and translated. */
bool cui_scrollable_begin(cui_context *, cui_id, const cui_theme *, cui_scroll_state *,
                          cui_scroll_options);
void cui_scrollable_end(cui_context *, cui_id, const cui_theme *, const cui_scroll_state *,
                        cui_scroll_options);
/* Preferred mutable end: handles child-deferred scrolls and updates this frame's
 * viewport translation and scrollbar paint. Pair with successful begin. */
void cui_scrollable_end_ex(cui_context *, cui_id, const cui_theme *, cui_scroll_state *,
                           cui_scroll_options);
#ifdef __cplusplus
}
#endif
#endif
