/* SPDX-License-Identifier: MIT */
#ifndef CUI_SIDEBAR_EVENTS_H
#define CUI_SIDEBAR_EVENTS_H
#include "cui_component_events.h"
#include "cui_control_events.h"
#include "cui_sidebar.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Applied row invocation, after legacy expansion/activation callbacks. */
typedef struct {
    cui_component_event_metadata event;
    cui_id item;
    unsigned level;
} cui_sidebar_activation;
typedef void (*cui_sidebar_activation_fn)(void *, const cui_sidebar_activation *);
typedef struct {
    cui_component_event_metadata event;
    cui_id item;
    unsigned level;
    bool previous, value;
} cui_sidebar_expansion_event;
typedef void (*cui_sidebar_expansion_fn)(void *, const cui_sidebar_expansion_event *);
typedef struct {
    cui_sidebar_activation_fn on_activate;
    void *user;
    cui_sidebar_expansion_fn on_expand_request, on_expand_change;
} cui_sidebar_item_handler;
typedef struct {
    cui_sidebar_item_result control;
    size_t activation_count; /* Actual observer calls; missing observer means zero. */
    bool valid, count_overflow;
    size_t request_count, change_count;
} cui_sidebar_item_event_result;
typedef struct {
    bool valid, submitted, count_overflow;
    size_t activation_count;
    size_t request_count, change_count;
} cui_sidebar_menu_event_result;
/* No retained handler; header/footer events remain separate.
 * Only a controlled REQUEST observer may assign its explicit expanded bool.
 * Other active item/input mutation, recursive composition and ui destruction are unsupported.
 * Expansion fields extend an unreleased branch API: rebuild all consumers. */
cui_sidebar_item_event_result cui_base_sidebar_item_events(cui_context *,
                                                           const cui_sidebar_menu_item *,
                                                           bool collapsed, unsigned level,
                                                           cui_sidebar_item_handler);
cui_sidebar_menu_event_result cui_sidebar_menu_events(cui_context *, cui_id, const cui_theme *,
                                                      const cui_sidebar_menu_item *, size_t,
                                                      bool collapsed, cui_sidebar_item_handler);
/* Collapse requests follow the existing local flip sequence from entry options.
 * Assigning an external owner bool does not redirect that local sequence. */
typedef struct {
    cui_component_event_metadata event;
    bool previous, value;
} cui_sidebar_collapse_request;
typedef void (*cui_sidebar_collapse_fn)(void *, const cui_sidebar_collapse_request *);
typedef struct {
    cui_sidebar_collapse_fn on_request;
    void *user;
} cui_sidebar_collapse_handler;
typedef struct {
    bool activated, valid, count_overflow;
    size_t request_count; /* Actual observer calls, not aggregate activations. */
} cui_sidebar_toggle_event_result;
cui_sidebar_toggle_event_result cui_base_sidebar_toggle_events(cui_context *, cui_id,
                                                               cui_sidebar_toggle_options,
                                                               cui_sidebar_collapse_handler);
cui_sidebar_toggle_event_result cui_sidebar_toggle_button_events(cui_context *, cui_id,
                                                                 const cui_theme *,
                                                                 cui_sidebar_toggle_options,
                                                                 cui_sidebar_collapse_handler);
/* Styled section actions: old callback then observer, once per actual activation.
 * No Base section counterpart; GROUP pointer_activatable also permits Core a11y ACTIVATE. */
cui_component_action_result cui_sidebar_header_events(cui_context *, cui_id, const cui_theme *,
                                                      cui_sidebar_section_options,
                                                      cui_component_activation_handler);
cui_component_action_result cui_sidebar_footer_events(cui_context *, cui_id, const cui_theme *,
                                                      cui_sidebar_section_options,
                                                      cui_component_activation_handler);
#ifdef __cplusplus
}
#endif
#endif
