/* SPDX-License-Identifier: MIT */
#ifndef CUI_SURFACE_EVENTS_H
#define CUI_SURFACE_EVENTS_H
#include "cui_component_events.h"
#include "cui_surface.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_component_event_metadata event;
    cui_surface_result result;
} cui_surface_lifecycle_event;
typedef void (*cui_surface_lifecycle_fn)(void *user, const cui_surface_lifecycle_event *event);
typedef struct {
    cui_surface_lifecycle_fn closed;
    void *user;
} cui_surface_event_handler;
/* Caller-owned, stable-address storage. Never copy/move while attached. Do not
 * edit adapter fields or control.on_close/close_user. Destruction and reentrant
 * surface operations during notification are unsupported. */
typedef struct {
    cui_surface_state control;
    cui_surface_event_handler handler;
    bool attached, active, notifying;
    size_t call_close_count;
} cui_surface_event_state;
typedef struct {
    bool valid;          /* Adapter preflight only, not rendering success. */
    bool control_result; /* Original update/begin result; false for void Alert. */
    size_t close_count;  /* Actual observer invocations during this call. */
} cui_surface_event_result;
/* Initialize fresh or closed detached storage only; not a teardown operation. */
void cui_surface_events_init(cui_surface_event_state *, cui_id, cui_surface_kind);
bool cui_surface_events_set_handler(cui_surface_event_state *, cui_surface_event_handler);
/* Closed-only, matching registration only; preserves native result/state. */
bool cui_surface_events_detach(cui_surface_event_state *);
/* Copy all options, preserving decide, and inject the retained registration.
 * Failure leaves output/state untouched. Plain legacy updates with ordinary
 * options on attached storage are unsupported: they overwrite registration. */
bool cui_surface_events_options(cui_surface_event_state *, cui_surface_options,
                                cui_surface_options *output);
cui_surface_event_result cui_base_surface_update_events(cui_context *, cui_surface_event_state *,
                                                        cui_surface_options);
cui_surface_event_result cui_base_surface_begin_events(cui_context *, cui_surface_event_state *,
                                                       cui_str, cui_str, cui_surface_options);
cui_surface_event_result cui_surface_begin_events(cui_context *, const cui_theme *,
                                                  cui_surface_event_state *, cui_str, cui_str,
                                                  cui_surface_options);
cui_surface_event_result cui_alert_dialog_events(cui_context *, const cui_theme *,
                                                 cui_surface_event_state *, cui_str, cui_str,
                                                 cui_str, cui_str, cui_surface_options);
/* Use existing open/close/request_close/take_result on &state.control. Imperative
 * close needs an active frame for focus-scope restoration; between frames use
 * request_close and submit an update on the UI thread. Calls to
 * end/footer are unchanged, and begin's control_result decides whether to end.
 * Payload metadata is LIFECYCLE/UNKNOWN without input/frame evidence. Callback
 * user/storage must outlive every possible close, including deferred Sheet exit. */
#ifdef __cplusplus
}
#endif
#endif
