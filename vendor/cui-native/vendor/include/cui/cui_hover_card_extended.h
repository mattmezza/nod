/* SPDX-License-Identifier: MIT */
#ifndef CUI_HOVER_CARD_EXTENDED_H
#define CUI_HOVER_CARD_EXTENDED_H
#include "cui_hover_events.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { CUI_HOVER_CARD_DESKTOP_HOVER, CUI_HOVER_CARD_TAP_TOGGLE } cui_hover_card_mode;
/* Caller-owned, zero-initialized workspace. Treat fields as private. Reset on
 * context recreation; no handlers or event pointers are retained. Ambiguous
 * multi-contact input disables outside dismissal until reset/close/cancel. */
typedef struct {
    const cui_context *context;
    cui_id card;
    uint64_t frame, contact, device;
    cui_pointer_type type;
    float start_x, start_y;
    bool active, pending, ambiguous;
} cui_hover_card_contact;
typedef struct {
    cui_hover_card_mode mode;
    /* Application assertion from this frame's submitted trigger Button result.
     * Aggregated once from entry open; never inferred from raw release/focus. */
    bool accepted_trigger_activation;
    bool relationship; /* Own trigger controls/expanded; preserve other details. */
    bool outside_tap;  /* TAP_TOGGLE only; requires a contact workspace. */
} cui_hover_card_policy;
typedef struct {
    cui_hover_event_result lifecycle;
    /* Current trigger details patched; independent of begin admission. */
    bool relationship_applied;
} cui_hover_card_extended_result;
/* Zero/NULL policy preserves legacy begin behavior. lifecycle.control_result
 * alone determines whether to compose children and call cui_end_hover_card.
 * TAP_TOGGLE ignores hover/focus/delays; legacy global Escape still wins, even
 * consumed/repeated Escape. Outside taps require matching primary down/up outside
 * previous trigger/card bounds and <8px movement, without cancellation,
 * consumption or gesture takeover. No input is consumed or captured here.
 * Observe every frame; omission resets a pending contact. See guide for limits.
 * Callback: final model/relation applied, before arbitrary children, UNKNOWN
 * source/frame only. Existing hover callback mutation/reentry rules apply. */
cui_hover_card_extended_result
cui_begin_hover_card_extended(cui_context *, cui_id, const cui_theme *, cui_hover_state *,
                              cui_str name, cui_hover_options, const cui_hover_card_policy *,
                              cui_hover_card_contact *, cui_hover_handler);
#ifdef __cplusplus
}
#endif
#endif
