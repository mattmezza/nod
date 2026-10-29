/* SPDX-License-Identifier: MIT */
#ifndef CUI_SIDEBAR_H
#define CUI_SIDEBAR_H
#include "cui_content.h"
#include "cui_icon.h"
#include "cui_scroll.h"
#ifdef __cplusplus
extern "C" {
#endif
#define CUI_SIDEBAR_ENTRY_LIMIT 1024u
#define CUI_SIDEBAR_DEPTH_LIMIT 64u
typedef enum { CUI_SIDEBAR_ICON, CUI_SIDEBAR_OFFCANVAS, CUI_SIDEBAR_NONE } cui_sidebar_collapse;
typedef enum { CUI_SIDEBAR_LEFT, CUI_SIDEBAR_RIGHT } cui_sidebar_side;
typedef struct {
    cui_id id;
    uint64_t revision;
    float height;
    bool measured, retained; /* private reconciliation marker */
    cui_rect bounds;         /* Last rendered entry's layout, retained through end_frame. */
} cui_sidebar_cache;
typedef struct {
    cui_sidebar_cache *cache;
    size_t capacity, count;
    cui_scroll_state scroll;
    cui_rect bounds, viewport;
    float width, from_width, target_width, target_child_width;
    double started, last_time;
    bool initialized, render_child, animating, icon_collapsed;
} cui_sidebar_state;
bool cui_sidebar_state_init(cui_sidebar_state *, cui_sidebar_cache *, size_t capacity);
typedef void (*cui_sidebar_content_fn)(cui_context *, const cui_theme *, bool collapsed, void *);
typedef struct {
    cui_id id;
    cui_sidebar_content_fn render;
    void *user;
    float estimated_height; /* Positive estimate for virtual variable-height entry. */
    uint64_t revision;      /* Change when unrendered content/height changes. */
} cui_sidebar_entry;
typedef struct {
    cui_sidebar_collapse collapse;
    cui_sidebar_side side;
    bool collapsed, reduced_motion;
    float width, height;   /* zero=255/default parent height; negative width=grow. */
    bool width_percent;    /* Nonpixel width: no width animation, matching source. */
    float viewport_height; /* Host window height for source30% list overdraw; zero uses sidebar. */
    double now_seconds;
    float duration_seconds; /* default constructor200ms; zero immediate */
    cui_sidebar_content_fn header, footer;
    void *header_user, *footer_user;
    cui_str label;
    bool custom_box;
    cui_box box; /* Surface appearance; component owns padding/layout/dimensions. */
    const cui_scrollbar_style *scrollbar_style;
} cui_sidebar_options;
cui_sidebar_options cui_sidebar_default(void);
/* Pure bounded cache reconciliation and motion; no theme implementation needed. */
bool cui_sidebar_prepare(cui_sidebar_state *, const cui_sidebar_entry *, size_t,
                         cui_sidebar_options, float available_width);
float cui_sidebar_content_height(const cui_sidebar_state *);
bool cui_sidebar(cui_context *, cui_id, const cui_theme *, cui_sidebar_state *,
                 const cui_sidebar_entry *, size_t, cui_sidebar_options);
typedef struct {
    cui_id id;
    bool initialized, open;
    cui_hover_state tooltip;
} cui_sidebar_item_state;
typedef struct cui_sidebar_menu_item cui_sidebar_menu_item;
struct cui_sidebar_menu_item {
    cui_id id;
    cui_str label, accessibility_label, icon;
    cui_sidebar_content_fn icon_content, label_content; /* Optional custom Icon/label style. */
    void *icon_user, *label_user;
    bool custom_label_box;
    cui_box label_box;
    bool active, disabled, disable_caret, default_open, click_to_open, click_to_toggle;
    bool collapsed; /* Nested children retain this; top-level menu overrides with its flag. */
    bool *expanded; /* Optional controlled value; owner updates in on_expand or typed REQUEST. */
    cui_sidebar_item_state *state; /* Required for submenu or collapsed tooltip. */
    const cui_sidebar_menu_item *children;
    size_t child_count;
    void (*on_activate)(void *);
    void (*on_expand)(bool open, void *);
    void *user;
    cui_sidebar_content_fn suffix;
    void *suffix_user;
    cui_sidebar_content_fn context; /* Called after row; compose host context-menu helper. */
    void *context_user;
    bool custom_box;
    cui_box box;
    cui_hover_options tooltip_options;
};
typedef struct {
    bool activated, expansion_changed, open;
} cui_sidebar_item_result;
/* Drawing-free actual input; does not invoke styled suffix/context callbacks. */
cui_sidebar_item_result cui_base_sidebar_item(cui_context *, const cui_sidebar_menu_item *,
                                              bool collapsed, unsigned level);
/* Group and menu forward collapsed to children. Group label is hidden when collapsed. */
bool cui_sidebar_menu(cui_context *, cui_id, const cui_theme *, const cui_sidebar_menu_item *,
                      size_t, bool collapsed);
bool cui_sidebar_group(cui_context *, cui_id, const cui_theme *, cui_str label,
                       cui_sidebar_content_fn, void *, bool collapsed);
typedef struct {
    bool collapsed, selected, custom_box;
    cui_sidebar_content_fn content;
    void *user;
    void (*on_activate)(void *); /* Optional Header/Footer host action (pointer or accepted
                                    accessibility activation). */
    void *activate_user;
    cui_box box;
} cui_sidebar_section_options;
/* Source Header/Footer hover surfaces; collapsed flag is forwarded to content. */
bool cui_sidebar_header(cui_context *, cui_id, const cui_theme *, cui_sidebar_section_options);
bool cui_sidebar_footer(cui_context *, cui_id, const cui_theme *, cui_sidebar_section_options);
typedef struct {
    cui_sidebar_side side;
    bool collapsed, disabled;
    cui_str accessibility_label;
    void (*on_change)(bool collapsed, void *);
    void *user;
} cui_sidebar_toggle_options;
bool cui_base_sidebar_toggle(cui_context *, cui_id, cui_sidebar_toggle_options);
bool cui_sidebar_toggle_button(cui_context *, cui_id, const cui_theme *,
                               cui_sidebar_toggle_options);
cui_id cui_sidebar_part_id(cui_id, unsigned part);
#ifdef __cplusplus
}
#endif
/* Additive typed row notifications are discoverable from the Sidebar header. */
#include "cui_sidebar_events.h"
#endif
