/* SPDX-License-Identifier: MIT */
#ifndef CUI_MEDIA_EVENTS_H
#define CUI_MEDIA_EVENTS_H
#include "cui_component_events.h"
#include "cui_media.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_ATTACHMENT_ACTION_OPEN,
    CUI_ATTACHMENT_ACTION_REMOVE,
    CUI_ATTACHMENT_ACTION_RETRY
} cui_attachment_action;
typedef struct {
    cui_component_event_metadata event;
    cui_attachment_action action;
} cui_attachment_action_event;
typedef void (*cui_attachment_event_fn)(void *, const cui_attachment_action_event *);
typedef struct {
    cui_attachment_event_fn on_activate, on_remove, on_retry;
    void *user;
} cui_attachment_handler;
typedef struct {
    cui_attachment_result control;
    size_t activation_count, remove_count, retry_count;
} cui_attachment_event_result;
/* One original operation; each category runs its legacy callback then observer,
 * in fixed OPEN/REMOVE/RETRY order after paint and before focus-scope end. Typed
 * handlers enable only their corresponding existing action path. Counts are
 * actual calls (zero/one per category), not input counts or completed host work.
 * Unique genuine accepted evidence is copied at the actual interaction seam;
 * ambiguous/missing evidence gives proven frame only, UNKNOWN/no index.
 * Passive/layout/inert never notify. Loading is not a global action disable.
 * Pointer-free payload copies by value. Handler/user/payload pointer borrowed.
 * Keep active resources live through outer return plus renderer consumption;
 * do not mutate/free/rebind active state/options/children/context/input, recursively
 * submit UI or destroy context in observer. Unrelated flags/copying allowed;
 * defer omission/teardown. Legacy callbacks keep old userdata/timing and must keep
 * resources needed by remaining work/observers live. No reject/CHANGE/COMMIT implied. */
cui_attachment_event_result cui_attachment_events(cui_context *, cui_id, const cui_theme *,
                                                  cui_attachment_state *, cui_attachment_options,
                                                  cui_attachment_handler);
#ifdef __cplusplus
}
#endif
#endif
