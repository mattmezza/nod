/* SPDX-License-Identifier: MIT */
#ifndef CUI_ALERT_EVENTS_H
#define CUI_ALERT_EVENTS_H
#include "cui_component_events.h"
#include "cui_content.h"
#ifdef __cplusplus
extern "C" {
#endif
/* One completed built-in Dismiss action, coalesced across the accepted batch.
 * ACTIVATE metadata targets the Alert; only the actual entry frame is present.
 * UNKNOWN source, absent index/activation: never per-input or close evidence. */
typedef struct {
    cui_component_event_metadata event;
    bool visibility_assigned; /* Existing false write ran, not a value transition. */
} cui_alert_dismissal;
typedef void (*cui_alert_dismiss_fn)(void *user, const cui_alert_dismissal *dismissal);
typedef struct {
    cui_alert_dismiss_fn callback;
    void *user;
} cui_alert_dismiss_handler;
typedef struct {
    bool valid, submitted;
    size_t dismissal_count; /* Eligible completed dismissal, 0/1; no handler needed. */
    bool legacy_dismissed;  /* Exact cui_alert bool, independently of observation. */
} cui_alert_dismiss_result;
/* Calls cui_alert once, preserving slots, Button options, paint and owner writes.
 * Observation is synchronous after its end_box/optional false assignment.
 * Eligibility/frame/options/handler freeze at entry. Missing handler preserves
 * interaction and count. No visibility storage still permits a true result.
 * Payload is pointer-free and copyable past the frame/context lifetime; callback
 * views/users/resources are borrowed. Do not recurse, destroy context, unwind
 * across C or free/rebind active resources; defer teardown until after frame.
 * Handler may update visibility again: no veto, replay or second write follows. */
static inline cui_alert_dismiss_result cui_alert_events(cui_context *ui, cui_id id,
                                                        const cui_theme *theme,
                                                        cui_alert_options options,
                                                        cui_alert_dismiss_handler handler) {
    cui_alert_dismiss_result result;
    memset(&result, 0, sizeof result);
    if (!ui || !id || !theme || options.kind < CUI_ALERT_INFO || options.kind > CUI_ALERT_DEFAULT ||
        (options.title.length && !options.title.data) ||
        (options.message.length && !options.message.data) ||
        (options.icon.length && !options.icon.data) ||
        (options.icon_name.length && !options.icon_name.data) ||
        (options.accessible_name.length && !options.accessible_name.data))
        return result;
    const cui_alert_options snapshot = options;
    const cui_alert_dismiss_handler observer = handler;
    const uint64_t frame = cui_frame_sequence(ui);
    result.valid = true;
    result.submitted =
        cui_frame_is_submitting(ui) && frame != 0 && (!snapshot.visible || *snapshot.visible);
    result.legacy_dismissed = cui_alert(ui, id, theme, snapshot);
    if (result.submitted && result.legacy_dismissed) {
        result.dismissal_count = 1;
        if (observer.callback) {
            cui_alert_dismissal dismissal;
            memset(&dismissal, 0, sizeof dismissal);
            (void)cui_component_event_init(&dismissal.event, CUI_COMPONENT_EVENT_ACTIVATE, id,
                                           CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
            dismissal.event.frame_sequence = frame;
            dismissal.event.flags = CUI_COMPONENT_EVENT_HAS_FRAME;
            dismissal.visibility_assigned = snapshot.visible != NULL;
            observer.callback(observer.user, &dismissal);
        }
    }
    return result;
}
#ifdef __cplusplus
}
#endif
#endif
