/* SPDX-License-Identifier: MIT */
#ifndef CUI_COMMAND_EVENTS_H
#define CUI_COMMAND_EVENTS_H
#include "cui_command.h"
#include "cui_component_events.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_component_event_metadata event;
    bool has_previous;
    cui_command_index previous, value;
} cui_command_selection_change;
typedef struct {
    cui_component_event_metadata event;
    cui_str value; /* Raw borrowed UTF-8, only through callback; copy explicitly. */
} cui_command_query_change;
typedef struct {
    cui_component_event_metadata event;
    cui_command_index value;
} cui_command_activation;
typedef struct {
    cui_component_event_metadata event; /* REQUEST, not completed host dismissal. */
} cui_command_cancel_request;
typedef void (*cui_command_selection_change_fn)(void *, const cui_command_selection_change *);
typedef void (*cui_command_query_change_fn)(void *, const cui_command_query_change *);
typedef void (*cui_command_activation_fn)(void *, const cui_command_activation *);
typedef void (*cui_command_cancel_request_fn)(void *, const cui_command_cancel_request *);
typedef struct {
    cui_command_selection_change_fn on_selection;
    cui_command_query_change_fn on_query;
    cui_command_activation_fn on_activate;
    cui_command_cancel_request_fn on_cancel;
    void *user;
} cui_command_event_handler;
typedef struct {
    bool control; /* Exact legacy bool; false may mean unchanged, not invalid. */
    bool count_overflow;
    size_t selection_count, query_count, activation_count, cancel_request_count;
} cui_command_event_result;
/* Contextless Base operations carry PROGRAMMATIC target-only metadata. Zero
 * target rejects before mutation; other validation/return policies remain legacy.
 * Rebuild freezes paths, observes raw query BEFORE any legacy hook, then old
 * select -> new selection -> old query. Other pointer-free observers follow old
 * hooks; confirm snapshots identity before action -> captured old confirm -> new.
 * Missing observer ignores user. Query text borrows only through its callback;
 * use cui_component_event_copy_utf8 for retention. Other payloads are copyable.
 * New handlers must not mutate/rebind active state/editor/model/resources, recurse,
 * destroy resources or unwind across C. Copy facts/change unrelated flags and
 * defer teardown. Legacy hook mutations retain their existing timing/lifetimes. */
cui_command_event_result cui_command_rebuild_events(cui_command_state *, cui_id target,
                                                    const cui_command_entry *, size_t,
                                                    cui_str query, bool query_changed,
                                                    cui_command_options, cui_command_event_handler);
cui_command_event_result cui_command_select_events(cui_command_state *, cui_id target,
                                                   cui_command_index, bool scroll,
                                                   cui_command_options, cui_command_event_handler);
cui_command_event_result cui_command_move_events(cui_command_state *, cui_id target, bool forward,
                                                 cui_command_options, cui_command_event_handler);
cui_command_event_result cui_command_confirm_events(cui_command_state *, cui_id target,
                                                    cui_command_options, cui_command_event_handler);
/* Styled leading editor-hash rebuild stays PROGRAMMATIC, even under passive
 * ancestry. UI observers require a proven ordinary active frame. Aggregate query
 * and hover have UNKNOWN/frame-only cause; row activation uses unique genuine
 * evidence or frame-only ambiguity. Old hook ordering and early query exception
 * remain unchanged. Empty-query cancel observer does not close/consume Escape. */
cui_command_event_result cui_command_events(cui_context *, cui_id, const cui_theme *,
                                            cui_command_state *, const cui_command_entry *, size_t,
                                            cui_command_options, cui_command_event_handler);
#ifdef __cplusplus
}
#endif
#endif
