/* SPDX-License-Identifier: MIT */
#ifndef CUI_STEPPER_NAVIGATION_H
#define CUI_STEPPER_NAVIGATION_H
#include "cui_component_events.h"
#include "cui_numeric.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_component_event_metadata event;
    size_t selected, requested, count;
} cui_stepper_navigation_request;
typedef void (*cui_stepper_navigation_fn)(void *, const cui_stepper_navigation_request *);
typedef struct {
    cui_stepper_navigation_fn request;
    void *user;
} cui_stepper_navigation_handler;
typedef struct {
    bool valid;           /* Argument preflight, not completed layout/visibility. */
    size_t request_count; /* Accepted input requests, independent of handler. */
} cui_stepper_navigation_result;
/* Controlled REQUEST surface: selected is captured by value and never mutated.
 * Each accepted item activation requests its index, including the selected item.
 * Callbacks run after semantic registration and before styled paint, in input
 * order. App validation/selection updates take effect on its next submission.
 * Count <=100, nonzero id, valid string pairs/statuses; nonempty selected<count,
 * empty selected==0. Invalid arguments register nothing and never dispatch.
 * Genuine metadata targets the Stepper; activation.target identifies its item.
 * Borrow steps/strings/theme through this call, keeping them stable until return;
 * handler is snapshotted. Copy payload to retain it. Store validation errors in
 * separate owner fields and rebuild descriptors on the next submission.
 * Do not mutate descriptors, active routing/options, recurse, destroy UI or
 * borrowed storage, or unwind foreign exceptions from a callback. No validator,
 * veto protocol, normalization, retained callback or automatic state ownership.
 * Legacy Stepper reducers and aggregate CHANGE observers remain unchanged. */
cui_stepper_navigation_result cui_base_stepper_navigation(cui_context *, cui_id, const cui_step *,
                                                          size_t count, size_t selected,
                                                          bool disabled,
                                                          cui_stepper_navigation_handler);
cui_stepper_navigation_result cui_stepper_navigation(cui_context *, cui_id, const cui_theme *,
                                                     const cui_step *, size_t count,
                                                     size_t selected, bool vertical, bool disabled,
                                                     cui_stepper_navigation_handler);
#ifdef __cplusplus
}
#endif
#endif
