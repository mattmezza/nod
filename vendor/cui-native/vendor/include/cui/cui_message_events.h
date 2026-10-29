/* SPDX-License-Identifier: MIT */
#ifndef CUI_MESSAGE_EVENTS_H
#define CUI_MESSAGE_EVENTS_H
#include "cui_component_events.h"
#include "cui_message.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_component_event_metadata event;
    double previous, value;
} cui_message_offset_change;
typedef struct {
    cui_component_event_metadata event;
    bool previous, value;
} cui_message_tail_change;
typedef void (*cui_message_offset_change_fn)(void *, const cui_message_offset_change *);
typedef void (*cui_message_tail_change_fn)(void *, const cui_message_tail_change *);
typedef struct {
    cui_message_offset_change_fn on_offset_change;
    cui_message_tail_change_fn on_tail_change;
    void *user;
} cui_message_scroller_handler;
typedef struct {
    cui_message_scroller_result control_result;
    bool valid, offset_changed, tail_changed, count_overflow;
    size_t offset_change_count, tail_change_count;
} cui_message_scroller_event_result;
/* Call-local aggregate observation around exactly one legacy operation. Snapshot
 * precedes width remeasure/row reconciliation/layout. Exact double inequality;
 * tail means following policy, not visible proximity. Frozen offset then tail
 * payloads delivered after legacy composition; animation-only changes are silent.
 * UNKNOWN metadata, genuine frame only; frame-zero maintenance emits no callback.
 * Non-submission rejects before work. Missing handlers preserve changed flags.
 * Explicit model mutators remain result-only; no delayed/replayed notification.
 * Pointer-free payloads copy by value; handlers/user borrowed through return.
 * New observers cannot mutate/free active state/records/options/input, recursively
 * submit or destroy context. State/records remain stable through end_frame because
 * legacy tracked bounds retain their existing lifetime. Existing legacy callback
 * behavior and restrictions are unchanged. No retained or completion listener. */
cui_message_scroller_event_result cui_base_message_scroller_events(cui_context *, cui_id,
                                                                   cui_message_scroller_state *,
                                                                   cui_message_scroller_options,
                                                                   cui_message_scroller_handler);
cui_message_scroller_event_result
cui_message_scroller_events(cui_context *, cui_id, const cui_theme *, cui_message_scroller_state *,
                            cui_message_row_render_fn, void *, cui_message_scroller_options,
                            cui_message_scroller_handler);
#ifdef __cplusplus
}
#endif
#endif
