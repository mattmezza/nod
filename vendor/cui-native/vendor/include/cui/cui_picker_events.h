/* SPDX-License-Identifier: MIT */
#ifndef CUI_PICKER_EVENTS_H
#define CUI_PICKER_EVENTS_H
#include "cui_component_events.h"
#include "cui_picker.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Applied date selection only, not presentation navigation or a rejectable request.
 * Pointer-free/copyable. UNKNOWN source, proven active frame only, no index/cause. */
typedef struct {
    cui_component_event_metadata event;
    cui_date_selection previous, value;
} cui_date_selection_change;
typedef void (*cui_date_selection_change_fn)(void *, const cui_date_selection_change *);
typedef struct {
    cui_date_selection_change_fn on_change;
    void *user;
} cui_date_selection_handler;
typedef struct {
    cui_calendar_result control; /* Exact legacy aggregate result. */
    size_t change_count;         /* Zero or one actual observer invocation. */
} cui_calendar_event_result;
/* Each calls its legacy operation once, then notifies at the outer return.
 * Reduce-only does not submit semantics; Base does; Styled also paints first.
 * Date comparison uses cui_date_compare on both endpoints, including zero dates.
 * Missing observer still edits; unused user ignored. Passive/disabled work retains
 * legacy maintenance but never notifies. No submit-only observer is provided.
 * Required nonnull pointers/nonzero id are checked before legacy calls.
 * Do not mutate/free/rebind active state, selection, options/policy resources or
 * context, nor recursively submit UI in the observer. Defer teardown until outer
 * return plus frame/render consumption. Unrelated application flags may change. */
cui_calendar_event_result cui_calendar_reduce_events(cui_context *, cui_id, cui_calendar_state *,
                                                     cui_date_selection *, cui_calendar_options,
                                                     cui_date_selection_handler);
cui_calendar_event_result cui_base_calendar_events(cui_context *, cui_id, cui_calendar_state *,
                                                   cui_date_selection *, cui_calendar_options,
                                                   cui_date_selection_handler);
cui_calendar_event_result cui_calendar_events(cui_context *, cui_id, const cui_theme *,
                                              cui_calendar_state *, cui_date_selection *,
                                              cui_calendar_options, cui_date_selection_handler);
/* DatePicker wraps the entire outer operation, with trigger-ID metadata.
 * Popup open/close and same-date activation alone do not emit CHANGE. */
typedef struct {
    bool changed;        /* Exact Styled legacy bool, including false on late errors. */
    size_t change_count; /* Zero or one actual callback. */
} cui_picker_event_result;
cui_calendar_event_result cui_base_date_picker_events(cui_context *, cui_id trigger,
                                                      cui_id calendar, cui_str name,
                                                      cui_date_picker_state *, cui_date_selection *,
                                                      cui_date_picker_options,
                                                      cui_date_selection_handler);
cui_picker_event_result cui_date_picker_events(cui_context *, cui_id trigger, const cui_theme *,
                                               cui_str name, cui_date_picker_state *,
                                               cui_date_selection *, cui_date_picker_options,
                                               cui_date_selection_handler);
/* Applied RGBA only. Latent HSV/capture/synchronization changes are not CHANGE. */
typedef struct {
    cui_component_event_metadata event;
    cui_color previous, value;
} cui_color_change;
typedef void (*cui_color_change_fn)(void *, const cui_color_change *);
typedef struct {
    cui_color_change_fn on_change;
    void *user;
} cui_color_handler;
cui_picker_event_result cui_base_color_picker_events(cui_context *, cui_id, cui_color_state *,
                                                     cui_color *, cui_color_picker_options,
                                                     cui_color_handler);
cui_picker_event_result cui_color_picker_events(cui_context *, cui_id, const cui_theme *,
                                                cui_color_state *, cui_color *,
                                                cui_color_picker_options, cui_color_handler);
/* Applied final time only. Draft/focus changes and net-zero edits are silent.
 * Calls the complete legacy operation once, then observes after semantics (Base)
 * or paint (Styled). UNKNOWN cause, proven active frame only, no input index.
 * Payload copies by value; handler/user borrowed. The lifetime and mutation
 * restrictions above apply. Disabled/readonly/passive/inert never notify.
 * Missing observer preserves edits. Invalid time, including hidden seconds,
 * retains legacy rejection. No request/commit/submit or IME contract is added. */
typedef struct {
    cui_component_event_metadata event;
    cui_time previous, value;
} cui_time_change;
typedef void (*cui_time_change_fn)(void *, const cui_time_change *);
typedef struct {
    cui_time_change_fn on_change;
    void *user;
} cui_time_handler;
cui_picker_event_result cui_base_time_field_events(cui_context *, cui_id, cui_str, cui_time_state *,
                                                   cui_time *, cui_time_options, cui_time_handler);
cui_picker_event_result cui_time_field_events(cui_context *, cui_id, const cui_theme *, cui_str,
                                              cui_time_state *, cui_time *, cui_time_options,
                                              cui_time_handler);
#ifdef __cplusplus
}
#endif
#endif
