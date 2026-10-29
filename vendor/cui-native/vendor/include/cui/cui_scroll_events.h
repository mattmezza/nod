/* SPDX-License-Identifier: MIT */
#ifndef CUI_SCROLL_EVENTS_H
#define CUI_SCROLL_EVENTS_H
#include "cui_component_events.h"
#include "cui_scroll.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float x, y;
} cui_scroll_offset;
typedef struct {
    cui_component_event_metadata event;
    cui_scroll_offset previous, value;
    bool has_previous;
} cui_scroll_change;
typedef void (*cui_scroll_change_fn)(void *, const cui_scroll_change *);
typedef struct {
    cui_scroll_change_fn on_change;
    void *user;
} cui_scroll_handler;
typedef struct {
    bool valid, changed, count_overflow;
    size_t change_count;
} cui_scroll_event_result;
/* Zero initialize before first use; private fields are not caller state.
 * Borrowed context/state identity only, never a retained handler. Do not copy an
 * active token. Use a distinct token for each nested owner. */
typedef struct {
    cui_context *private_context;
    cui_scroll_state *private_state;
    cui_id private_owner;
    uint64_t private_frame;
    cui_scroll_offset private_entry;
    bool private_active, private_styled;
} cui_scroll_event_scope;
/* Base-only paired final-offset observation. Return is the legacy reducer bool:
 * false can be valid/no-change. scope.private_active is private: always finish
 * after an admitted call; finish.valid distinguishes invalid admission.
 * Null/zero arguments, active token or non-submission phase reject before work.
 * Same context/owner/state/options/ancestry/frame must surround the pair. Runtime
 * checks cannot detect cross-frame misuse when both frame sequences are zero.
 * End after children to include deferred wheel routing; finish runs once.
 * Base does not compose boxes/translation/paint; overlay adapters are separate. */
bool cui_base_scrollable_events(cui_context *, cui_id, cui_scroll_state *, cui_scroll_options,
                                cui_scroll_event_scope *);
/* Aggregate CHANGE includes normalization, not proof of user input. Frame-zero
 * inert/layout-only work preserves maintenance but suppresses callbacks. Disabled
 * ordinary normalization can notify UNKNOWN with genuine frame, no input index.
 * Missing observer still reduces; count is zero. Net-zero offsets are silent.
 * Nonfinite entry repair has has_previous=false and zero previous coordinates.
 * Payload is pointer-free/copyable. Handler/user borrowed through this call.
 * Do not mutate active state/options/input, recursively compose, destroy context
 * or free active resources in callbacks. Defer teardown until frame completion. */
cui_scroll_event_result cui_base_scrollable_finish_events(cui_context *, cui_id, cui_scroll_state *,
                                                          cui_scroll_options,
                                                          cui_scroll_event_scope *,
                                                          cui_scroll_handler);
/* Styled begin returns composition admission, never an offset-change flag.
 * False: no new pair/boxes, no end. True: always pair with end_events, including
 * passive work and no-change. Do not mix Base and Styled pairing entrypoints.
 * End performs legacy mutable finish/translation/paint/closure once, then observes.
 * Same options/theme lifetime/ancestry and normal layout balancing are required.
 * New private_styled extends an unreleased scope layout: rebuild consumers. */
bool cui_scrollable_begin_events(cui_context *, cui_id, const cui_theme *, cui_scroll_state *,
                                 cui_scroll_options, cui_scroll_event_scope *);
cui_scroll_event_result cui_scrollable_end_events(cui_context *, cui_id, const cui_theme *,
                                                  cui_scroll_state *, cui_scroll_options,
                                                  cui_scroll_event_scope *, cui_scroll_handler);
#ifdef __cplusplus
}
#endif
#endif
