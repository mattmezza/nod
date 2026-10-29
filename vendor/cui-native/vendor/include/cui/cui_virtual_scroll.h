/* SPDX-License-Identifier: MIT */
#ifndef CUI_VIRTUAL_SCROLL_H
#define CUI_VIRTUAL_SCROLL_H
#include "cui_scroll.h"
#include "cui_virtual.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { CUI_VIRTUAL_SIZE_AUTO, CUI_VIRTUAL_SIZE_INFER } cui_virtual_sizing;
/* Host intrinsic measurement; available_cross is zero when unconstrained.
 * The selected index is clamped. Return false to reject the frame. */
typedef bool (*cui_virtual_measure_item_fn)(void *user, size_t index, uint64_t key,
                                            float available_cross, cui_size *output);
typedef struct {
    cui_scroll_options scroll;
    size_t overscan;
    bool horizontal; /* Model heights become item widths for horizontal lists. */
    bool no_item_semantics;
    cui_virtual_sizing sizing;
    /* Infer: explicit scroll dimensions, then available dimensions, then
     * model main extent / measured cross extent. Zero means unspecified. */
    cui_size available;
    size_t measure_index;
    cui_virtual_measure_item_fn measure;
    void *measure_user;
} cui_virtual_widget_options;
typedef struct {
    cui_scroll_state scroll;
    cui_size last_viewport;
} cui_virtual_widget_state;
typedef struct {
    cui_virtual_result error;
    cui_virtual_view view;
    bool scroll_changed;
    /* A child-deferred scroll changed the window after callbacks ran. The
     * adapter invalidates the UI; next frame renders the new window. */
    bool window_pending;
} cui_virtual_widget_result;
/* Resolves main content extent from the model, optional cross extent from
 * scroll options. Gap must be zero: model extents already include spacing.
 * Invalid dimensions or totals outside float coordinate range reject. */
bool cui_virtual_widget_scroll_options(const cui_virtual_list *, cui_virtual_widget_options,
                                       cui_scroll_options *output);
/* Drawing-free reducer using scroll infrastructure; keyboard scrolling on the
 * focusable owner supports Home/End/PageUp/PageDown/main-axis arrows. */
cui_virtual_widget_result cui_base_virtual_list(cui_context *, cui_id, const cui_virtual_list *,
                                                cui_virtual_widget_state *,
                                                cui_virtual_widget_options);
/* Called only for the window (including overscan), inside a clipped fixed-size
 * item wrapper. Keep model keys/heights unchanged during the callback. Rows
 * may paint arbitrary content; selection and row controls remain host-owned. */
typedef void (*cui_virtual_render_item_fn)(cui_context *, const cui_theme *, void *user,
                                           size_t index, uint64_t key, cui_id item, cui_size size);
cui_virtual_widget_result cui_virtual_list_draw(cui_context *, cui_id, const cui_theme *,
                                                const cui_virtual_list *,
                                                cui_virtual_widget_state *,
                                                cui_virtual_widget_options,
                                                cui_virtual_render_item_fn, void *user);
#ifdef __cplusplus
}
#endif
#endif
