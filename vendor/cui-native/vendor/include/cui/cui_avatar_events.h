/* SPDX-License-Identifier: MIT */
#ifndef CUI_AVATAR_EVENTS_H
#define CUI_AVATAR_EVENTS_H
#include "cui_content.h"
#include "cui_control_events.h"
#ifdef __cplusplus
extern "C" {
#endif
/* One original Avatar operation. Options.interactive still enables activation;
 * a handler never changes role/routing. Per-event ACTIVATE after original paint
 * and consumption, in existing event-index order. Missing handler still counts.
 * request_count is always zero; counts saturate with count_overflow.
 * valid is preflight only; submitted includes disabled/passive avatars but is
 * false outside active paint-enabled non-inert submission. Legacy API unchanged.
 * Genuine evidence is copied before consumption. If the legacy gate matches
 * without accepted evidence, metadata has proven frame only, UNKNOWN/no index.
 * Payload copyable by value; handler/user/payload pointer borrowed for this call.
 * Keep pixels/caches/icons/labels/users live through their rendering boundary.
 * Unrelated flags, copied payloads and consuming later input are allowed; later
 * eligibility is checked live. Do not rebind/free active resources, recursively
 * submit this component/frame or destroy context. Defer omission/teardown until
 * outer return and renderer consumption. No runtime destruction guard or veto,
 * CHANGE/COMMIT/SUBMIT, resource lifecycle or foreign callback facade implied. */
cui_component_action_result cui_avatar_events(cui_context *, cui_id, const cui_theme *, cui_str,
                                              cui_avatar_options, cui_component_activation_handler);
typedef struct {
    cui_component_event_metadata event; /* Target=stack; activation.target=child. */
    size_t index;
    uint64_t key; /* Original application key, including zero. */
} cui_avatar_stack_activation;
typedef void (*cui_avatar_stack_activation_fn)(void *, const cui_avatar_stack_activation *);
typedef struct {
    cui_avatar_stack_activation_fn callback;
    void *user;
} cui_avatar_stack_handler;
typedef struct {
    size_t control;                     /* Legacy highest activated visible index, or SIZE_MAX. */
    cui_component_action_result action; /* request_count always zero. */
} cui_avatar_stack_event_result;
/* One original stack submission. Child-submission order, then each child's
 * event-index order; not global input order. Hidden items/passive overflow never
 * notify. A callback does not enable interactive. Aggregate control retains the
 * original index semantics, while action counts every observed activation.
 * Same metadata/mutation/lifetime rules as above; items are borrowed and must
 * not be changed/rebound/freed during the active stack submission. */
cui_avatar_stack_event_result cui_avatar_stack_events(cui_context *, cui_id, const cui_theme *,
                                                      const cui_avatar_item *, size_t count,
                                                      cui_avatar_stack_options,
                                                      cui_avatar_stack_handler);
#ifdef __cplusplus
}
#endif
#endif
