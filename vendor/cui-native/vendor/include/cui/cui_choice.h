/* SPDX-License-Identifier: MIT */
#ifndef CUI_CHOICE_H
#define CUI_CHOICE_H
#include "cui.h"
#include "cui_base.h"
#include "cui_content.h"
#include "cui_control_events.h"
#include "cui_edit.h"
#include "cui_menu_events.h"
#include "cui_overlay.h"
#include "cui_selection_events.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_id id;
    cui_str label;
    bool disabled;
} cui_choice_item;
/* Styled row body; semantic name/disabled/selection remain supplied by item.
 * index is relative to the displayed array, including filtered Combobox arrays. */
typedef void (*cui_choice_render_fn)(cui_context *, const cui_choice_item *, size_t index,
                                     void *user);
typedef void (*cui_choice_content_fn)(cui_context *, void *user);
typedef enum {
    CUI_TAB_DEFAULT,
    CUI_TAB_UNDERLINE,
    CUI_TAB_PILL,
    CUI_TAB_OUTLINE,
    CUI_TAB_SEGMENTED
} cui_tab_variant;
typedef enum { CUI_TAB_MEDIUM, CUI_TAB_SMALL, CUI_TAB_LARGE, CUI_TAB_XSMALL } cui_tab_size;
typedef struct {
    float height, inner_height, padding_x, font_size, icon_size, gap, part_gap;
    float bar_padding_x, margin_top, margin_bottom;
} cui_tab_metrics;
/* Frozen logical-pixel metrics scaled by positive density (invalid uses1). */
cui_tab_metrics cui_tab_size_metrics(cui_tab_variant, cui_tab_size, float density);
/* Caller-owned indicator spring; zero initialization snaps to the first target.
 * Position and velocity survive subsequent target changes. */
typedef struct {
    float left, width, left_velocity, width_velocity;
    bool initialized;
} cui_tab_motion;
void cui_tab_motion_step(cui_tab_motion *, float target_left, float target_width, float dt,
                         cui_spring_settings, bool reduced_motion);
/* Optional shared indicator state; keep alive until the next frame because
 * measured bounds are written by cui_end_frame. */
typedef struct {
    cui_tab_motion spring;
    cui_rect target_bounds, strip_bounds;
    cui_id selected;
    float last_scroll_offset, foreground_elapsed;
    bool ready, transitioning;
} cui_tab_animation;
typedef struct {
    cui_id candidate, before, moved;
    float start_x, start_y;
    bool active, requested, drop_valid;
} cui_tab_drag_state;
typedef struct cui_tab_overflow cui_tab_overflow;
typedef struct {
    float offset, maximum;
    cui_rect viewport;
    cui_id receiving_id;
    bool changed;
    cui_id reveal_id, last_selected; /* explicit reveal request and auto-reveal tracking */
    cui_id scrollbar_id;
    cui_rect scrollbar_bounds;
    float scrollbar_grab;
    bool scrollbar_dragging;
} cui_tab_scroll_state;
/* Structured per-tab content; explicit positive widths reserve nonshrinking
 * prefix/suffix space under a cap. Zero widths fit naturally without reserving
 * truncation space. Icon mode replaces the body and ignores max_width. */
typedef struct {
    cui_choice_render_fn prefix, suffix, icon;
    void *user;
    float prefix_width, suffix_width;
    bool omit_leading_border; /* first Default tab: source tab_bar_prefix(false) */
} cui_tab_parts;
/* Explicit opt-in segmented appearance; zero preserves existing paint.
 * SOURCE_TAB uses standalone XS. SOURCE_BAR requires animation and horizontal
 * orientation, with SM before measurement and private raised layers afterward. */
typedef enum {
    CUI_TAB_SHADOW_LEGACY = 0,
    CUI_TAB_SHADOW_SOURCE_TAB,
    CUI_TAB_SHADOW_SOURCE_BAR
} cui_tab_shadow_policy;
typedef struct {
    bool vertical, disabled, manual_activation;
    cui_tab_size size;
    float density; /* zero uses theme font_size/16 */
    float height;  /* optional positive styled height override */
    cui_tab_variant variant;
    cui_choice_render_fn render; /* optional tab body, preserving the label semantics */
    void *render_user;
    cui_choice_content_fn prefix, suffix;
    void *content_user;
    float max_width; /* zero unconstrained; caps all bodies, built-in labels ellipsize */
    cui_tab_scroll_state *scroll; /* optional application-owned bounded viewport */
    float bar_extent;             /* scroll viewport width, or height for vertical tabs */
    cui_tab_overflow *overflow;   /* optional all-tabs menu when scroll maximum > 0 */
    cui_tab_drag_state *drag;   /* optional stable-ID reorder request; caller applies after frame */
    bool reveal_selected;       /* opt in to reveal selection changes in a bounded strip */
    const cui_tab_parts *parts; /* optional borrowed per-item slots */
    size_t parts_count;
    bool scrollbar;               /* optional pointer/keyboard scrollbar over the viewport edge */
    cui_tab_animation *animation; /* optional retained shared indicator; horizontal strips */
    float dt;
    bool reduced_motion;
    cui_choice_content_fn last_empty_space; /* suffix/menu scroll tail; optional custom body */
    void *last_empty_space_user;
    float last_empty_space_width;       /* logical px; zero defaults to12 unless flag set */
    bool custom_last_empty_space_width; /* permits an explicit zero-width tail */
    cui_design_size design_size;        /* INHERIT preserves legacy metrics. */
    const cui_scrollbar_style *scrollbar_style;
    cui_tab_shadow_policy shadow_policy;
} cui_tabs_options;
/* Checked before reducers/callbacks; other variants ignore valid policies. */
bool cui_tabs_shadow_policy_valid(cui_tabs_options);
typedef struct {
    bool vertical, disabled, multiple, allow_empty;
} cui_toggle_group_options;
typedef void (*cui_tab_panel_fn)(cui_context *, size_t index, void *user);
/* Selection is a stable item ID rather than a reorder-sensitive array index.
 * Zero means no selection. Invalid/disabled selections are left caller-owned;
 * they produce no visible panel. Base controls draw nothing. */
/* Reveal content coordinates in an existing bounded scroll range. */
bool cui_tab_scroll_reveal(cui_tab_scroll_state *, float start, float extent, float viewport);
/* Additive aggregate selection observers: include cui_tabs_events.h. */
bool cui_base_tabs(cui_context *, const cui_choice_item *, size_t count, cui_id *selected,
                   cui_tabs_options);
/* Additive per-write ToggleGroup observers: include cui_list_events.h. */
bool cui_base_toggle_group(cui_context *, const cui_choice_item *, size_t count, bool *selected,
                           cui_toggle_group_options);
bool cui_tabs(cui_context *, cui_id container, const cui_theme *, const cui_choice_item *,
              size_t count, cui_id *selected, cui_tabs_options, cui_tab_panel_fn, void *);
bool cui_toggle_group(cui_context *, cui_id container, const cui_theme *, const cui_choice_item *,
                      size_t count, bool *selected, cui_toggle_group_options);
/* Flat selectable lists and anchored popup controls. App-owned state preserves
 * the active cursor, ASCII typeahead prefix and scroll window across frames. */
typedef struct {
    cui_id active;
    char search[64];
    size_t search_length;
    float search_time;
    size_t first_visible;
    float scroll_remainder;  /* fractional wheel rows carried across frames */
    cui_id secondary_active; /* independent optional context-click highlight */
} cui_list_state;
typedef void (*cui_choice_activation_fn)(cui_id item, void *user);
typedef void (*cui_choice_confirm_fn)(cui_id item, bool secondary, void *user);
typedef void (*cui_choice_cancel_fn)(void *user);
typedef enum {
    CUI_LIST_MEDIUM,
    CUI_LIST_SMALL,
    CUI_LIST_LARGE,
    CUI_LIST_XSMALL,
    CUI_LIST_CUSTOM
} cui_list_size;
typedef struct {
    float height, font_size, line_height, padding_x, padding_y, gap;
    float check_size, check_width, density;
} cui_list_metrics;
cui_list_metrics cui_list_default_metrics(float density);
cui_list_metrics cui_searchable_list_size_metrics(cui_list_size, float custom_size, float density);
typedef struct {
    bool disabled, menu;
    float dt;
    size_t visible_begin, visible_count;  /* count=0 submits all items */
    cui_choice_activation_fn on_activate; /* synchronous, in event order */
    void *activation_user;
    cui_choice_render_fn render;
    void *render_user;
    cui_rect scroll_bounds; /* base scrolling receiving bounds; styled fills automatically */
    cui_id scroll_target;   /* eligible scrolling semantic ID */
    cui_id scroll_scope;    /* popup scope, or zero for a standalone list */
    bool window_changed;    /* discard pointer activation against pre-scroll layout */
    cui_choice_confirm_fn on_confirm; /* same event order, after on_activate */
    cui_choice_cancel_fn on_cancel;   /* Escape, independent of selection change */
    unsigned secondary_modifiers;     /* zero uses CTRL|META; caller selects host policy */
    bool track_secondary;             /* highlight secondary presses without confirming */
    float density; /* zero derives theme.font_size/16; ordinary List has no size variant */
    float height;  /* positive fixed row override; zero uses source natural text plus padding */
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
    const cui_scrollbar_style *scrollbar_style;
} cui_list_options;
typedef struct {
    cui_overlay overlay;
    cui_list_state list;
    size_t visible_rows; /* previous styled popup rows, used for ordered Combo scrolling */
    cui_input_behavior_state query_behavior;
    float query_scroll_x, query_font_size;
    cui_rect query_viewport; /* previous painted geometry for measured pointer selection */
} cui_choice_popup;
typedef struct {
    bool disabled;
    cui_rect viewport;
    float width, dt;
    cui_popup_side side;         /* preferred anchor side; defaults below */
    cui_choice_render_fn render; /* optional fixed-height option body */
    void *render_user;
    bool align_end; /* align popup right edge with anchor right edge */
    cui_list_size size;
    float custom_size;           /* CUI_LIST_CUSTOM logical font basis; font = size*0.875 */
    float density;               /* zero derives theme.font_size/16 */
    float row_height;            /* positive fixed row override; zero uses source natural height */
    float max_height;            /* positive popup cap; default source 20rem = 320*density */
    cui_design_size design_size; /* Trigger, query and popup rows share this scale. */
    const cui_scrollbar_style *scrollbar_style;
} cui_choice_popup_options;
void cui_choice_popup_init(cui_choice_popup *, cui_id popup_id);
/* Apply dismissal and trigger activation before submitting the open popup. */
void cui_base_choice_popup_trigger(cui_context *, cui_id trigger, cui_str name, bool disabled,
                                   const cui_choice_item *, size_t count, cui_id selected,
                                   cui_choice_popup *);
/* Additive typed List observers: include cui_list_events.h. */
bool cui_base_list(cui_context *, const cui_choice_item *, size_t count, cui_id *selected,
                   cui_list_state *, cui_list_options);
bool cui_list(cui_context *, cui_id container, const cui_theme *, const cui_choice_item *,
              size_t count, cui_id *selected, cui_list_state *, cui_list_options);
bool cui_select(cui_context *, cui_id trigger, const cui_theme *, cui_str name, cui_str placeholder,
                const cui_choice_item *, size_t count, cui_id *selected, cui_choice_popup *,
                cui_choice_popup_options);
/* Synchronous post-mutation Select observer; missing handler preserves writes.
 * Full list pass precedes existing popup close. No request/rejection semantics. */
typedef struct {
    cui_selection_event_result events;
    bool legacy_changed;
} cui_select_event_result;
cui_select_event_result cui_select_events(cui_context *, cui_id trigger, const cui_theme *,
                                          cui_str name, cui_str placeholder,
                                          const cui_choice_item *, size_t count, cui_id *selected,
                                          cui_choice_popup *, cui_choice_popup_options,
                                          cui_selection_handler);
/* Menu returns true when an action ID was activated, including reactivation
 * of the currently selected action. The caller owns the command dispatch. */
bool cui_menu(cui_context *, cui_id trigger, const cui_theme *, cui_str name,
              const cui_choice_item *, size_t count, cui_id *action, cui_choice_popup *,
              cui_choice_popup_options);
/* Per-command notifications, including repeated same-ID activations. */
cui_menu_command_result cui_menu_events(cui_context *, cui_id trigger, const cui_theme *,
                                        cui_str name, const cui_choice_item *, size_t count,
                                        cui_id *action, cui_choice_popup *,
                                        cui_choice_popup_options, cui_menu_command_handler);

bool cui_base_selection_remove(cui_context *, cui_id chip, cui_str name, bool *selected,
                               bool disabled);
bool cui_base_list_multi(cui_context *, const cui_choice_item *, size_t count, bool *selected,
                         cui_list_state *, cui_list_options);
bool cui_list_multi(cui_context *, cui_id container, const cui_theme *, const cui_choice_item *,
                    size_t count, bool *selected, cui_list_state *, cui_list_options);
/* Multi-select keeps the popup open while toggling options; Escape/outside
 * dismiss. The selected summary and chips remain caller-controlled. */
bool cui_select_multi(cui_context *, cui_id trigger, const cui_theme *, cui_str name,
                      cui_str placeholder, const cui_choice_item *, size_t count, bool *selected,
                      cui_choice_popup *, cui_choice_popup_options);
typedef struct {
    bool changed, query_changed, scrolled;
    size_t matching_count;
    cui_edit_result error;
} cui_combobox_result;
/* Caller supplies filtered scratch capacity for at least count items. Query is
 * an independently owned single-line editor; committed IDs survive filtering.
 * Search is ASCII case-insensitive substring matching, byte-exact otherwise. */
cui_combobox_result cui_base_combobox(cui_context *, cui_id input, cui_str name, cui_edit *query,
                                      const cui_choice_item *, size_t count,
                                      cui_choice_item *filtered, size_t filtered_capacity,
                                      cui_id *selected, cui_choice_popup *, bool disabled);
cui_combobox_result cui_combobox(cui_context *, cui_id input, const cui_theme *, cui_str name,
                                 cui_edit *query, const cui_choice_item *, size_t count,
                                 cui_choice_item *filtered, size_t filtered_capacity,
                                 cui_id *selected, cui_choice_popup *, cui_choice_popup_options);
/* Selection CHANGE only; query edits and confirmation without transition do
 * not notify. Base query/Enter phase precedes Styled option-list phase. */
typedef struct {
    cui_selection_event_result events;
    cui_combobox_result legacy;
} cui_combobox_event_result;
cui_combobox_event_result cui_base_combobox_events(cui_context *, cui_id input, cui_str name,
                                                   cui_edit *, const cui_choice_item *, size_t,
                                                   cui_choice_item *, size_t filtered_capacity,
                                                   cui_id *selected, cui_choice_popup *,
                                                   bool disabled, cui_selection_handler);
cui_combobox_event_result cui_combobox_events(cui_context *, cui_id input, const cui_theme *,
                                              cui_str name, cui_edit *, const cui_choice_item *,
                                              size_t, cui_choice_item *, size_t filtered_capacity,
                                              cui_id *selected, cui_choice_popup *,
                                              cui_choice_popup_options, cui_selection_handler);
cui_combobox_result cui_base_combobox_multi(cui_context *, cui_id input, cui_str name, cui_edit *,
                                            const cui_choice_item *, size_t count,
                                            cui_choice_item *filtered, bool *filtered_selected,
                                            size_t filtered_capacity, bool *selected,
                                            cui_choice_popup *, bool disabled);
cui_combobox_result cui_combobox_multi(cui_context *, cui_id input, const cui_theme *, cui_str name,
                                       cui_edit *, const cui_choice_item *, size_t count,
                                       cui_choice_item *filtered, bool *filtered_selected,
                                       size_t filtered_capacity, bool *selected, cui_choice_popup *,
                                       cui_choice_popup_options);

#define CUI_MENU_DEPTH_MAX 16
typedef enum {
    CUI_MENU_ACTION,
    CUI_MENU_CHECK,
    CUI_MENU_RADIO,
    CUI_MENU_SEPARATOR,
    CUI_MENU_LABEL,
    CUI_MENU_SUBMENU
} cui_menu_kind;
typedef void (*cui_menu_render_fn)(cui_context *, void *user);
typedef struct cui_menu_entry {
    cui_id id;
    cui_str label, icon, shortcut;
    bool disabled;
    cui_menu_kind kind;
    bool *checked;
    int *radio_value;
    int radio_option;
    const struct cui_menu_entry *children;
    size_t child_count;
    cui_menu_render_fn render; /* optional custom row body; keep within control height */
    void *render_user;
} cui_menu_entry;
typedef struct {
    cui_overlay overlay;
    cui_list_state levels[CUI_MENU_DEPTH_MAX];
    cui_id path[CUI_MENU_DEPTH_MAX - 1];
    cui_rect panels[CUI_MENU_DEPTH_MAX];
    size_t visible_rows[CUI_MENU_DEPTH_MAX]; /* previous painted window per level */
    size_t depth;
    bool depth_limit;
    float row_height, padding; /* previous painted metrics for pointer hover */
    bool contextual;
    cui_rect anchor;
    bool drag_scroll, scroll_pending, scroll_dragging;
    size_t scroll_level;
    float scroll_start_y, scroll_last_y;
    float label_height, separator_height, row_gap,
        available_height; /* previous nonuniform styled row metrics */
} cui_menu_tree_state;
typedef enum { CUI_MENU_MEDIUM, CUI_MENU_SMALL, CUI_MENU_LARGE, CUI_MENU_XSMALL } cui_menu_size;
typedef struct {
    float row_height, label_height, separator_height, gap;
    float padding, padding_x, font_size, icon_size, radius;
} cui_menu_metrics;
cui_menu_metrics cui_menu_size_metrics(cui_menu_size, float density, float radius);
typedef struct {
    cui_choice_popup_options popup;
    bool keep_open_checks, checks_on_right;
    bool drag_scroll;   /* opt-in primary-pointer drag, usable by touch adapters */
    cui_menu_size size; /* source Small20; other sizes26 logical px */
    float density;      /* zero derives theme.font_size/16 */
    float max_height;   /* positive override; default min(viewport.height/2,450*density) */
    const cui_scrollbar_style *scrollbar_style;
} cui_menu_tree_options;
struct cui_tab_overflow {
    cui_menu_tree_state menu;
    cui_menu_entry *entries; /* caller-owned scratch descriptors; capacity >= tab count */
    size_t capacity;
    cui_id trigger;
    cui_menu_tree_options options;
    bool visible;
};
void cui_tab_overflow_init(cui_tab_overflow *, cui_id popup, cui_id trigger,
                           cui_menu_entry *entries, size_t capacity);
void cui_menu_tree_init(cui_menu_tree_state *, cui_id popup);
/* Ordered reducer before painting. Descriptors and pointed-to check/radio values
 * are borrowed; IDs must be globally unique throughout the tree. */
bool cui_base_menu_tree(cui_context *, cui_id trigger, cui_str name, const cui_menu_entry *,
                        size_t count, cui_id *action, cui_menu_tree_state *, bool disabled,
                        float dt, bool keep_open_checks);
bool cui_menu_entry_is_disabled(const cui_menu_entry *);
void cui_base_menu_entry(cui_context *, const cui_menu_entry *, bool disabled, bool expanded,
                         cui_id parent);
bool cui_menu_tree(cui_context *, cui_id trigger, const cui_theme *, cui_str name,
                   const cui_menu_entry *, size_t count, cui_id *action, cui_menu_tree_state *,
                   cui_menu_tree_options);
/* Per-command synchronous ACTIVATE observer after applied check/radio/action
 * writes and close policy. Missing observer preserves normal mutation.
 * No context destruction, active storage freeing or recursive submission. */
cui_menu_command_result cui_base_menu_tree_events(cui_context *, cui_id trigger, cui_str name,
                                                  const cui_menu_entry *, size_t count,
                                                  cui_id *action, cui_menu_tree_state *,
                                                  bool disabled, float dt, bool keep_open_checks,
                                                  cui_menu_command_handler);
cui_menu_command_result cui_menu_tree_events(cui_context *, cui_id trigger, const cui_theme *,
                                             cui_str name, const cui_menu_entry *, size_t count,
                                             cui_id *action, cui_menu_tree_state *,
                                             cui_menu_tree_options, cui_menu_command_handler);
/* Paint an already reduced Menu popup anchored to a caller-painted trigger.
 * Does not submit the trigger, reduce events or mutate action/check values.
 * Registers visible popup/row semantics and retains measured window metrics. */
bool cui_menu_popup_paint(cui_context *, const cui_theme *, cui_str name, const cui_menu_entry *,
                          size_t count, cui_menu_tree_state *, cui_rect anchor,
                          cui_menu_tree_options);
/* Context-menu target is an existing visible box; this does not submit or paint
 * that target again. Secondary press within its previous bounds opens at the
 * pointer. Call once per state after rendering the receiving area. */
void cui_context_menu_open(cui_context *, cui_menu_tree_state *, cui_id restore_focus,
                           cui_rect anchor);
bool cui_base_context_menu(cui_context *, cui_id target, const cui_menu_entry *, size_t count,
                           cui_id *action, cui_menu_tree_state *, bool disabled, float dt,
                           bool keep_open_checks);
bool cui_context_menu(cui_context *, cui_id target, const cui_theme *, cui_str name,
                      const cui_menu_entry *, size_t count, cui_id *action, cui_menu_tree_state *,
                      cui_menu_tree_options);
/* Per-command forwarding only: opening/reanchoring emits no command. Metadata
 * target is the receiving target ID, independent of restore-focus identity. */
cui_menu_command_result cui_base_context_menu_events(cui_context *, cui_id target,
                                                     const cui_menu_entry *, size_t count,
                                                     cui_id *action, cui_menu_tree_state *,
                                                     bool disabled, float dt, bool keep_open_checks,
                                                     cui_menu_command_handler);
cui_menu_command_result cui_context_menu_events(cui_context *, cui_id target, const cui_theme *,
                                                cui_str name, const cui_menu_entry *, size_t count,
                                                cui_id *action, cui_menu_tree_state *,
                                                cui_menu_tree_options, cui_menu_command_handler);
/* Split action and menu trigger. Zero action/trigger IDs omit that half in the
 * base function; both zero is invalid. The explicit IDs remain independently
 * focusable; container is paint-only in the styled function. */
typedef enum {
    CUI_DROPDOWN_BUTTON_DEFAULT,
    CUI_DROPDOWN_BUTTON_PRIMARY,
    CUI_DROPDOWN_BUTTON_SECONDARY,
    CUI_DROPDOWN_BUTTON_DANGER,
    CUI_DROPDOWN_BUTTON_OUTLINE,
    CUI_DROPDOWN_BUTTON_GHOST,
    CUI_DROPDOWN_BUTTON_WARNING,
    CUI_DROPDOWN_BUTTON_SUCCESS,
    CUI_DROPDOWN_BUTTON_INFO,
    CUI_DROPDOWN_BUTTON_LINK,
    CUI_DROPDOWN_BUTTON_TEXT,
    CUI_DROPDOWN_BUTTON_CUSTOM
} cui_dropdown_button_variant;
typedef enum {
    CUI_DROPDOWN_BUTTON_MEDIUM,
    CUI_DROPDOWN_BUTTON_SMALL,
    CUI_DROPDOWN_BUTTON_LARGE,
    CUI_DROPDOWN_BUTTON_XSMALL
} cui_dropdown_button_size;
typedef struct {
    float height, padding_x, font_size, gap, caret_size;
} cui_dropdown_button_metrics;
cui_dropdown_button_metrics cui_dropdown_button_size_metrics(cui_dropdown_button_size,
                                                             float density);
typedef enum {
    CUI_DROPDOWN_ANCHOR_TOP_RIGHT,
    CUI_DROPDOWN_ANCHOR_TOP_LEFT,
    CUI_DROPDOWN_ANCHOR_BOTTOM_RIGHT,
    CUI_DROPDOWN_ANCHOR_BOTTOM_LEFT
} cui_dropdown_button_anchor;
typedef struct {
    cui_color color, foreground, hover, active;
} cui_dropdown_button_palette;
typedef struct {
    bool disabled, action_disabled, action_loading, selected;
    bool outline;                         /* orthogonal to filled color variants */
    bool action_selected, action_outline; /* preserve inner button presentation */
    float width; /* action-half width: zero fits contents; negative grows */
    cui_dropdown_button_variant variant;
    cui_dropdown_button_palette custom; /* custom variant palette, both halves */
    cui_dropdown_button_size size;
    cui_str leading, trailing;
    cui_choice_content_fn content;
    void *content_user;
    cui_menu_tree_options menu;
    /* When an outer property is unset, inherit the action button's property
     * for both halves. Zero-initialized options use source Default and Medium. */
    bool inherit_action_variant, inherit_action_size;
    bool hide_action, hide_menu;       /* at least one half must remain visible */
    cui_dropdown_button_anchor anchor; /* popup corner; default top right */
    cui_dropdown_button_variant action_variant;
    cui_dropdown_button_size action_size;
    /* Optional action tooltip composition; ID must be globally unique and state
     * persists across frames. Its trigger is always the action-half ID. */
    cui_id tooltip_id;
    cui_str tooltip;
    cui_hover_state *tooltip_state;
    cui_hover_options tooltip_options;
    float density;               /* zero uses theme font_size/16 */
    float height;                /* optional positive styled height override, including Link/Text */
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
} cui_dropdown_button_options;
typedef struct {
    bool action_activated, menu_activated;
} cui_dropdown_button_result;
/* Callback counts are actual invocations; missing handlers count zero. */
typedef struct {
    cui_component_activation_handler action;
    cui_menu_command_handler menu;
} cui_dropdown_event_handlers;
typedef struct {
    bool valid, submitted, count_overflow;
    size_t action_count, command_count;
    cui_dropdown_button_result legacy;
} cui_dropdown_event_result;
cui_dropdown_event_result
cui_base_dropdown_button_events(cui_context *, cui_id action_id, cui_id trigger_id,
                                cui_str action_name, cui_str menu_name, const cui_menu_entry *,
                                size_t count, cui_id *menu_action, cui_menu_tree_state *,
                                bool disabled, bool action_disabled, bool action_loading, float dt,
                                bool keep_open_checks, cui_dropdown_event_handlers);
cui_dropdown_event_result
cui_dropdown_button_events(cui_context *, cui_id container, cui_id action_id, cui_id trigger_id,
                           const cui_theme *, cui_str action_name, cui_str menu_name,
                           const cui_menu_entry *, size_t count, cui_id *menu_action,
                           cui_menu_tree_state *, cui_dropdown_button_options,
                           cui_dropdown_event_handlers);
cui_dropdown_button_result cui_base_dropdown_button(
    cui_context *, cui_id action_id, cui_id trigger_id, cui_str action_name, cui_str menu_name,
    const cui_menu_entry *, size_t count, cui_id *menu_action, cui_menu_tree_state *, bool disabled,
    bool action_disabled, bool action_loading, float dt, bool keep_open_checks);
cui_dropdown_button_result cui_dropdown_button(cui_context *, cui_id container, cui_id action_id,
                                               cui_id trigger_id, const cui_theme *,
                                               cui_str action_name, cui_str menu_name,
                                               const cui_menu_entry *, size_t count,
                                               cui_id *menu_action, cui_menu_tree_state *,
                                               cui_dropdown_button_options);
#ifdef __cplusplus
}
#endif
#endif
