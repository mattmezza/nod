/* SPDX-License-Identifier: MIT */
#ifndef CUI_HOVER_EVENTS_H
#define CUI_HOVER_EVENTS_H
#include "cui_component_events.h"
#include "cui_content.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Pure model-open transition, not completed visibility or input acceptance.
 * Pointer-free and copyable beyond callback/state lifetime. UNKNOWN source and
 * no frame/index/activation evidence: policy booleans do not identify an input. */
typedef struct {
    cui_component_event_metadata event;
    bool previous_open, open;
} cui_hover_lifecycle_event;
typedef void (*cui_hover_lifecycle_fn)(void *, const cui_hover_lifecycle_event *);
typedef struct {
    cui_hover_lifecycle_fn on_change;
    void *user;
} cui_hover_handler;
typedef struct {
    bool valid, control_result, count_overflow;
    bool changed;        /* Entry open differs from final open, independent of handler. */
    size_t change_count; /* Actual callbacks: zero or one; overflow always false. */
} cui_hover_event_result;
/* Additive pure Base policy; all existing timing and bool behavior is preserved.
 * Nonzero target and nonnull state required; failure leaves state untouched.
 * control_result is the exact cui_hover_update bool, not submission success.
 * Missing handler preserves mutation, with count zero. Disabled closing is an
 * actual transition and may notify; no UI acceptance/disabled-routing claim.
 * Handler/user/payload borrowed only through this call, never retained in state.
 * Pending/elapsed-only changes do not notify; omission does not advance policy.
 * Do not mutate active state, recursively invoke its reducer, free active storage
 * or unwind a foreign exception through callbacks. Unrelated app state may change.
 * Styled observers below use UI admission; this Base function has no UI context. */
cui_hover_event_result cui_hover_update_events(cui_id target, cui_hover_state *,
                                               bool trigger_hovered, bool content_hovered,
                                               bool focused, bool escape, bool disabled, float dt,
                                               float open_delay, float close_delay,
                                               cui_hover_handler);
/* Styled final-model observation: control_result is the exact legacy Tooltip
 * bool / HoverCard begin admission. changed is independent, including fallback
 * closes; neither means completed visibility. End HoverCard only on admission.
 * Callback runs after legacy call, before arbitrary admitted HoverCard children.
 * Frame-zero/protected work preserves maintenance but suppresses callbacks.
 * Metadata has genuine frame only, UNKNOWN source and no index/activation.
 * No handlers retained across omission, delay or end. Same callback rules apply. */
cui_hover_event_result cui_tooltip_events(cui_context *, cui_id, const cui_theme *,
                                          cui_hover_state *, cui_str text, cui_hover_options,
                                          cui_hover_handler);
cui_hover_event_result cui_begin_hover_card_events(cui_context *, cui_id, const cui_theme *,
                                                   cui_hover_state *, cui_str name,
                                                   cui_hover_options, cui_hover_handler);
#ifdef __cplusplus
}
#endif
#endif
