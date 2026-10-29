/* SPDX-License-Identifier: MIT */
#ifndef CUI_TABS_EVENTS_H
#define CUI_TABS_EVENTS_H
#include "cui_choice.h"
#include "cui_component_events.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Aggregate applied selection CHANGE, never a rejectable request. Pointer-free
 * stable IDs; metadata has UNKNOWN source, no index/activation, proven frame only. */
typedef struct {
    cui_component_event_metadata event;
    cui_id previous, value;
} cui_tabs_selection_change;
typedef void (*cui_tabs_change_fn)(void *, const cui_tabs_selection_change *);
typedef struct {
    cui_tabs_change_fn on_change;
    void *user;
} cui_tabs_handler;
typedef struct {
    bool changed;        /* Exact legacy bool; not admission/submission success. */
    size_t change_count; /* Zero or one actual observer invocation. */
} cui_tabs_event_result;
/* Base draws nothing. target is a nonzero compound identity chosen by caller.
 * Styled uses container. Missing observer still permits legacy writes; user is
 * ignored when on_change is NULL. Notification follows Base finalization, before
 * Styled paint/slots/panel. No notification for passive/layout-only/disabled work.
 * Same final ID means no notification, even after intermediate selections.
 * Callback/user borrowed during call. Do not mutate/free selection, item arrays,
 * active states or context, or recursively submit UI. Defer omission/removal until
 * outer return plus frame/render consumption; retained animation lifetimes apply. */
cui_tabs_event_result cui_base_tabs_events(cui_context *, cui_id target, const cui_choice_item *,
                                           size_t count, cui_id *selected, cui_tabs_options,
                                           cui_tabs_handler);
cui_tabs_event_result cui_tabs_events(cui_context *, cui_id container, const cui_theme *,
                                      const cui_choice_item *, size_t count, cui_id *selected,
                                      cui_tabs_options, cui_tab_panel_fn, void *panel_user,
                                      cui_tabs_handler);
#ifdef __cplusplus
}
#endif
#endif
