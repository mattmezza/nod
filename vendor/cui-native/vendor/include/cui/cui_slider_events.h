/* SPDX-License-Identifier: MIT */
#ifndef CUI_SLIDER_EVENTS_H
#define CUI_SLIDER_EVENTS_H
#include "cui_base.h"
#include "cui_component_events.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Cause of emission, not owner acceptance or consumption. INPUT includes a
 * generic cancellation whose physical origin may be unknown. */
typedef enum {
    CUI_SLIDER_EVENT_INPUT,
    CUI_SLIDER_EVENT_DISABLED,
    CUI_SLIDER_EVENT_READONLY,
    CUI_SLIDER_EVENT_ZERO_SPAN
} cui_slider_event_reason;
/* Pointer-free subset of actual input; deliberately excludes borrowed text,
 * composition and unrelated payloads. has_input controls presence. */
typedef struct {
    cui_event_type type;
    float x, y;
    cui_key key;
    unsigned modifiers;
    bool repeat;
    cui_pointer_type pointer_type;
    cui_pointer_button pointer_button;
    uint64_t pointer_id, pointer_device;
} cui_slider_event_input;
typedef struct {
    cui_component_event_metadata event;
    cui_slider_phase phase;
    double current, requested; /* Immutable entry value; phase's proposed value. */
    cui_slider_event_reason reason;
    bool has_input;
    cui_slider_event_input input;
} cui_component_slider_request;
typedef void (*cui_component_slider_request_fn)(void *, const cui_component_slider_request *);
typedef struct {
    cui_component_slider_request_fn callback;
    void *user;
} cui_component_slider_handler;
typedef struct {
    bool valid, submitted, count_overflow;
    size_t begin_count, request_count, commit_count, cancel_count;
} cui_component_slider_result;
/* Draws nothing; submit inside same-ID box. Existing controlled double reducer,
 * options and capture state semantics are preserved. on_request/user must be
 * NULL: callback paths cannot be combined. Missing handler is read-only.
 * Counts are actual callback invocations, not accepted values or input counts.
 * UPDATE is REQUEST, COMMIT is COMMIT, BEGIN/CANCEL are LIFECYCLE. Never CHANGE.
 * Payload is pointer-free/copyable; handler/user borrowed through this call.
 * Do not destroy context/state/users, recursively submit or mutate active drag
 * state in callbacks. Owner value changes are observed next submission. */
cui_component_slider_result cui_base_slider_events(cui_context *, cui_id, cui_str, double value,
                                                   cui_slider_state *, cui_slider_options,
                                                   cui_component_slider_handler);
#ifdef __cplusplus
}
#endif
#endif
