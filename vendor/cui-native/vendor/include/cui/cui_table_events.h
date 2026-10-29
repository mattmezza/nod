/* SPDX-License-Identifier: MIT */
#ifndef CUI_TABLE_EVENTS_H
#define CUI_TABLE_EVENTS_H
#include "cui_component_events.h"
#include "cui_table.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_component_event_metadata event;
    cui_id previous_column, column;
    cui_table_sort previous, value;
} cui_table_sort_change;
typedef void (*cui_table_sort_change_fn)(void *user, const cui_table_sort_change *event);
typedef struct {
    cui_component_event_metadata event;
    cui_table_selection previous, value;
    cui_table_selection_mode mode;
} cui_table_selection_change;
typedef void (*cui_table_selection_change_fn)(void *user, const cui_table_selection_change *event);
typedef struct {
    cui_component_event_metadata event;
    cui_id column;
    float previous_width, width;
} cui_table_column_width_change;
typedef struct {
    cui_component_event_metadata event;
    cui_id column;
    size_t previous_index, index;
} cui_table_column_move_change;
typedef void (*cui_table_column_width_change_fn)(void *user,
                                                 const cui_table_column_width_change *event);
typedef void (*cui_table_column_move_change_fn)(void *user,
                                                const cui_table_column_move_change *event);
typedef enum { CUI_TABLE_ACTION_CELL, CUI_TABLE_ACTION_ROW_HEADER } cui_table_action_area;
typedef struct {
    cui_component_event_metadata event;
    cui_table_action_area area;
    cui_id row, column; /* ROW_HEADER always column=0. */
} cui_table_activation;
typedef void (*cui_table_activation_fn)(void *user, const cui_table_activation *event);
typedef enum {
    CUI_TABLE_CONTEXT_CELL,
    CUI_TABLE_CONTEXT_ROW_HEADER,
    CUI_TABLE_CONTEXT_COLUMN_HEADER
} cui_table_context_area;
typedef struct {
    cui_component_event_metadata event;
    cui_table_context_area area;
    cui_id row, column;
} cui_table_context_request;
typedef void (*cui_table_context_request_fn)(void *user, const cui_table_context_request *event);
typedef enum {
    CUI_TABLE_MODE_FROM_ACTIVATION,
    CUI_TABLE_MODE_FROM_NAVIGATION
} cui_table_mode_request_reason;
typedef struct {
    cui_component_event_metadata event;
    cui_table_selection_mode current, requested;
    cui_id row;
    cui_table_mode_request_reason reason;
} cui_table_mode_request;
typedef void (*cui_table_mode_request_fn)(void *user, const cui_table_mode_request *event);
typedef struct {
    cui_table_selection_change_fn on_selection;
    cui_table_sort_change_fn on_sort;
    cui_table_column_width_change_fn on_column_width;
    cui_table_column_move_change_fn on_column_move;
    cui_table_activation_fn on_activate;
    cui_table_context_request_fn on_context_request;
    cui_table_mode_request_fn on_mode_request;
    void *user;
} cui_table_event_handler;
typedef struct {
    cui_table_result control;
    bool valid, submitted, count_overflow;
    size_t selection_count, sort_count; /* Actual synchronous calls, not the legacy result flag. */
    size_t column_width_count, column_move_count;
    size_t activation_count, context_request_count, mode_request_count;
} cui_table_event_result;
/* Cell/row-header ACTIVATE observes accepted attempts, even unchanged or failed
 * selection; context REQUEST observes secondary press without selecting/opening.
 * Effective IDs/evidence freeze before selection observers. These pointer-free
 * payloads do not reinterpret the legacy last-value fields or add header invoke,
 * viewport/load hooks. Missing callback ignores user; no request rejection.
 * Mode REQUEST reports each legacy row-mode intent after its flags are written,
 * before navigation focus/reveal. current is the options snapshot; requested is
 * ROW even when already ROW or selection fails. It cannot veto or mutate options.
 * Width/move observers report successful reducer operations only; programmatic
 * cui_table_resize/cui_table_move_column remain silent. Indices are operation-time
 * positions; the copied column key stays authoritative. Cancel does not roll back.
 * Applied single-selection/sort transitions inside reduction, before final
 * semantics/rendering. on_selection + multiple_selection is rejected atomically
 * with valid=false/control.error=INVALID. Sort-only/nil observers preserve multi.
 * Selection payload captures primary IDs/effective operation mode, not multi sets.
 * No sorting of caller data, request rejection, retained registration or replay.
 * Missing observer ignores user; passive/disabled/loading paths preserve behavior.
 * Payload is pointer-free; handler/user borrow synchronously. During callbacks no
 * model/state/row/column/workspace mutation, rebind, recursion, destruction or
 * foreign unwinding. Copy facts/change unrelated state; defer edits/removal and
 * teardown until outer submission and renderer consumption are complete. */
cui_table_event_result cui_table_reduce_events(cui_context *, cui_id, cui_table_model *,
                                               cui_table_state *, cui_table_options,
                                               cui_table_event_handler);
cui_table_event_result cui_base_data_table_events(cui_context *, cui_id, cui_table_model *,
                                                  cui_table_state *, cui_table_options,
                                                  cui_table_event_handler);
cui_table_event_result cui_data_table_events(cui_context *, cui_id, const cui_theme *,
                                             cui_table_model *, cui_table_state *,
                                             cui_table_options, cui_table_label_fn,
                                             cui_table_cell_fn, void *, cui_table_event_handler);
/* Separate exact multiset observation. The two exclusive writable arrays each
 * contain capacity entries (<=4096), at least state.selection_capacity. Known
 * overlap/size checks fail before mutation. Scratch must additionally not alias
 * context/internal allocations, strings, callback userdata or other resources;
 * those opaque allocations cannot be validated. NULL on_selection ignores scratch.
 * Spans are canonical (row,column) multisets, borrowed ONLY during callback; copy
 * entries explicitly for retention. No allocation, owner sorting or per-item delta.
 * The callback restrictions above apply to both owner and scratch storage. */
typedef struct {
    const cui_table_selection *items;
    size_t count;
} cui_table_selection_span;
typedef struct {
    cui_table_selection *previous, *value;
    size_t capacity;
} cui_table_selection_workspace;
typedef struct {
    cui_component_event_metadata event;
    cui_table_selection previous_primary, value_primary;
    cui_table_selection_mode mode;
    cui_table_selection_span previous, value;
} cui_table_selection_set_change;
typedef void (*cui_table_selection_set_change_fn)(void *user,
                                                  const cui_table_selection_set_change *change);
typedef struct {
    cui_table_selection_set_change_fn on_selection;
    cui_table_sort_change_fn on_sort;
    cui_table_column_width_change_fn on_column_width;
    cui_table_column_move_change_fn on_column_move;
    cui_table_activation_fn on_activate;
    cui_table_context_request_fn on_context_request;
    cui_table_mode_request_fn on_mode_request;
    void *user;
} cui_table_set_event_handler;
typedef enum {
    CUI_TABLE_SET_PREFLIGHT_OK,
    CUI_TABLE_SET_PREFLIGHT_INVALID,
    CUI_TABLE_SET_PREFLIGHT_WORKSPACE_CAPACITY,
    CUI_TABLE_SET_PREFLIGHT_OVERLAP
} cui_table_set_preflight;
typedef struct {
    cui_table_event_result events;
    cui_table_set_preflight preflight;
} cui_table_set_event_result;
cui_table_set_event_result cui_table_reduce_set_events(cui_context *, cui_id, cui_table_model *,
                                                       cui_table_state *, cui_table_options,
                                                       cui_table_selection_workspace,
                                                       cui_table_set_event_handler);
cui_table_set_event_result cui_base_data_table_set_events(cui_context *, cui_id, cui_table_model *,
                                                          cui_table_state *, cui_table_options,
                                                          cui_table_selection_workspace,
                                                          cui_table_set_event_handler);
cui_table_set_event_result cui_data_table_set_events(cui_context *, cui_id, const cui_theme *,
                                                     cui_table_model *, cui_table_state *,
                                                     cui_table_options, cui_table_label_fn,
                                                     cui_table_cell_fn, void *,
                                                     cui_table_selection_workspace,
                                                     cui_table_set_event_handler);
#ifdef __cplusplus
}
#endif
#endif
