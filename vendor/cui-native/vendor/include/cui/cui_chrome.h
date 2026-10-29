/* SPDX-License-Identifier: MIT */
#ifndef CUI_CHROME_H
#define CUI_CHROME_H
#include "cui_platform.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*cui_chrome_render_fn)(cui_context *, const cui_theme *, cui_id, void *);
typedef struct {
    cui_id id;
    cui_str text;
    cui_chrome_render_fn render;
    void *user;
} cui_chrome_item;
typedef struct {
    const cui_chrome_item *items;
    size_t count;
} cui_chrome_region;
typedef struct {
    float width;
    cui_str name;
    const cui_box *style;
} cui_status_bar_options;
/* Center aligns between the occupied ends; a left-only bar aligns it right. */
cui_align cui_status_bar_center_alignment(bool has_left, bool has_right);
/* IDs must be nonzero and unique across all three regions. Callbacks own input
 * and semantics. Strings and callbacks are borrowed only for this submission. */
bool cui_status_bar(cui_context *, cui_id, const cui_theme *, cui_chrome_region left,
                    cui_chrome_region center, cui_chrome_region right, cui_status_bar_options);
typedef struct {
    bool move_armed, moved, click_pending;
    double click_time;
    float click_x, click_y;
} cui_title_bar_state;
typedef struct {
    float width;
    cui_str name;
    const cui_box *style;
    const cui_window_host *host;
    void (*on_close)(void *user);
    void *close_user;
    /* Zero uses .4 seconds and 4 logical pixels for portable double clicks. */
    float double_click_seconds, double_click_distance;
} cui_title_bar_options;
/* Controls use parts 0 minimize, 1 maximize/restore, 2 close. */
cui_id cui_title_bar_control_id(cui_id, unsigned part);
/* Unstyled title-area pointer behavior. Child controls retain their targets. */
bool cui_base_title_bar(cui_context *, cui_id, cui_title_bar_state *, cui_window_info,
                        cui_title_bar_options);
bool cui_title_bar(cui_context *, cui_id, const cui_theme *, cui_title_bar_state *,
                   const cui_chrome_item *, size_t count, cui_title_bar_options);
#ifdef __cplusplus
}
#endif
#endif
