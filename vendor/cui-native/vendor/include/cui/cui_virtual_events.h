/* SPDX-License-Identifier: MIT */
#ifndef CUI_VIRTUAL_EVENTS_H
#define CUI_VIRTUAL_EVENTS_H
#include "cui_scroll_events.h"
#include "cui_virtual_scroll.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_virtual_widget_result control_result;
    bool valid, changed, count_overflow;
    size_t change_count;
} cui_virtual_event_result;
/* Call-local aggregate float offset observation. Legacy operation runs once.
 * Styled observes after child-deferred finish/paint/closure; control_result.view
 * still describes rendered rows, including its unchanged window_pending meaning.
 * Non-submission rejects before work. Legacy validation/measurement rejection
 * preserves legacy work (including measurement service calls), without notification.
 * Frame-zero maintenance
 * preserves results but emits no callback. UNKNOWN source, genuine frame only.
 * Missing handler does not erase changed. Nonfinite entry has no previous value.
 * Payload is pointer-free/copyable; handler/user borrowed through return only.
 * New observers must not mutate/free active model/state/options/input, recursively
 * submit, or destroy context; defer teardown through frame completion. Existing
 * legacy measure/row callback contracts and timing are unchanged. */
cui_virtual_event_result cui_base_virtual_list_events(cui_context *, cui_id,
                                                      const cui_virtual_list *,
                                                      cui_virtual_widget_state *,
                                                      cui_virtual_widget_options,
                                                      cui_scroll_handler);
cui_virtual_event_result
cui_virtual_list_draw_events(cui_context *, cui_id, const cui_theme *, const cui_virtual_list *,
                             cui_virtual_widget_state *, cui_virtual_widget_options,
                             cui_virtual_render_item_fn, void *, cui_scroll_handler);
#ifdef __cplusplus
}
#endif
#endif
