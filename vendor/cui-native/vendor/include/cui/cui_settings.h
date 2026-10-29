/* SPDX-License-Identifier: MIT */
#ifndef CUI_SETTINGS_H
#define CUI_SETTINGS_H
#include "cui.h"
#include "cui_choice.h"
#ifndef CUI_COMPONENTS_H
#include "cui_components.h"
#endif
#include "cui_edit.h"
#include "cui_resizable.h"
#include "cui_sidebar.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Source indices are never renumbered by filtering. */
typedef enum {
    CUI_SETTINGS_MEDIUM,
    CUI_SETTINGS_XSMALL,
    CUI_SETTINGS_SMALL,
    CUI_SETTINGS_LARGE
} cui_settings_size;
typedef enum { CUI_SETTINGS_NORMAL, CUI_SETTINGS_FILL, CUI_SETTINGS_OUTLINE } cui_settings_variant;
typedef enum {
    CUI_SETTING_CUSTOM,
    CUI_SETTING_SWITCH,
    CUI_SETTING_CHECKBOX,
    CUI_SETTING_INPUT,
    CUI_SETTING_DROPDOWN,
    CUI_SETTING_NUMBER
} cui_setting_kind;
typedef struct {
    size_t page, group, item;
    cui_settings_size size;
    cui_settings_variant variant;
    bool vertical, disabled;
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
} cui_settings_render_options;
typedef void (*cui_setting_render_fn)(cui_context *, cui_id, const cui_theme *,
                                      cui_settings_render_options, void *);
typedef bool (*cui_setting_dirty_fn)(void *);
typedef void (*cui_setting_reset_fn)(void *);
/* Borrowed workspace; capacity must cover every choice. No per-frame allocation. */
typedef struct {
    cui_menu_tree_state menu;
    cui_menu_entry *entries;
    bool *checked;
    size_t capacity;
} cui_settings_dropdown_state;
bool cui_settings_dropdown_init(cui_settings_dropdown_state *, cui_id popup, cui_menu_entry *,
                                bool *checked, size_t capacity);
typedef struct {
    cui_str title, description;
    const cui_str *keywords;
    size_t keyword_count;
    bool disabled, vertical, custom_item;
    cui_setting_kind kind;
    bool *boolean;
    cui_switch_state *switch_state;
    cui_edit *edit;
    cui_input_state *input_state;
    double *number;
    cui_number_state *number_state;
    cui_number_options number_options;
    const cui_choice_item *choices;
    size_t choice_count;
    cui_id *choice;
    cui_choice_popup *popup;
    bool has_default;
    bool default_boolean;
    double default_number;
    cui_str default_text;
    cui_id default_choice;
    void (*on_change)(void *); /* Called after a built-in value changes or resets. */
    /* Custom field/full-row rendering; overrides the built-in adapter. */
    cui_setting_render_fn render;
    bool (*matches)(cui_str query, void *); /* Optional host Unicode/search policy. */
    cui_setting_dirty_fn dirty;
    cui_setting_reset_fn reset;
    void *user;
    cui_settings_dropdown_state *dropdown_state; /* Source Menu; NULL keeps legacy Select. */
    bool dropdown_scrollable;
} cui_setting_item;
typedef struct {
    cui_str title, description;
    const cui_setting_item *items;
    size_t count;
    bool custom_variant;
    cui_settings_variant variant;
    cui_setting_render_fn footer;
    void *footer_user;
} cui_setting_group;
typedef struct {
    cui_str title, description, icon;
    const cui_setting_group *groups;
    size_t count;
    bool default_open, no_reset;
    cui_setting_render_fn title_suffix;
    void *suffix_user;
} cui_setting_page;
typedef struct {
    cui_sidebar_item_state menu;
    uint64_t query_hash;
    bool query_initialized;
    cui_scroll_state scroll_state;
    float scroll, maximum;
    cui_rect viewport, content;
    cui_hover_state reset_tooltip;
} cui_settings_page_state;
typedef struct {
    cui_settings_page_state *pages;
    size_t capacity, selected_page, selected_group;
    cui_resizable_state resize;
    cui_resizable_slot resize_slots[2];
    cui_input_state search_input;
    cui_edit *search; /* Borrowed single-line editor; optional controlled-query API below. */
    float sidebar_width;
    cui_rect bounds, sidebar;
    bool initialized, pending_group_scroll;
} cui_settings_state;
typedef struct {
    cui_settings_size size;
    cui_settings_variant variant;
    float width, height, sidebar_width, sidebar_minimum, sidebar_maximum;
    cui_str query; /* Used when state.search is NULL. */
    cui_str search_placeholder, reset_label;
    size_t default_page, default_group;
    bool default_group_selected; /* Zero-init selects the page, not group zero. */
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
} cui_settings_options;
typedef struct {
    bool selection_changed, reset, resized, scrolled;
    size_t visible_pages, visible_groups, visible_items;
    bool invalid;
    cui_edit_result edit_error;
} cui_settings_result;
#define CUI_SETTINGS_NO_GROUP ((size_t)-1)
bool cui_settings_state_init(cui_settings_state *, cui_settings_page_state *, size_t,
                             cui_edit *search);
cui_settings_options cui_settings_default(void);
bool cui_setting_item_matches(const cui_setting_item *, cui_str query);
bool cui_setting_group_matches(const cui_setting_group *, cui_str query);
/* Validate arrays and reconcile selection without allocating or rendering. */
cui_settings_result cui_settings_filter(const cui_setting_page *, size_t, cui_settings_state *,
                                        cui_settings_options);
bool cui_setting_item_dirty(const cui_setting_item *);
bool cui_settings_reset_page(const cui_setting_page *, cui_str query);
cui_id cui_settings_part_id(cui_id, size_t page, size_t group, size_t item, unsigned part);
/* Applied navigation/scalar observers: see cui_settings_events.h. */
cui_settings_result cui_settings(cui_context *, cui_id, const cui_theme *, const cui_setting_page *,
                                 size_t, cui_settings_state *, cui_settings_options);
#ifdef __cplusplus
}
#endif
#endif
