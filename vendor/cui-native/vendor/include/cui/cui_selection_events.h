/* SPDX-License-Identifier: MIT */
#ifndef CUI_SELECTION_EVENTS_H
#define CUI_SELECTION_EVENTS_H
#include "cui_component_events.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_SELECTION_REASON_ACTIVATION,
    CUI_SELECTION_REASON_NAVIGATION
} cui_selection_reason;
/* Actual post-operation transition, not an owner-rejectable request.
 * event.target is the compound control. RadioGroup omits HAS_ACTIVATION:
 * its legacy semantic registration occurs after reduction.
 * Pointer-free snapshot; callback/user borrowed only during submission. */
typedef struct {
    cui_component_event_metadata event;
    cui_id previous, value, item;
    cui_selection_reason reason;
} cui_selection_change;
typedef void (*cui_selection_change_fn)(void *, const cui_selection_change *);
typedef struct {
    cui_selection_change_fn on_change;
    void *user;
} cui_selection_handler;
typedef struct {
    bool valid, submitted, count_overflow;
    size_t change_count; /* Observer invocations; missing observer means zero. */
} cui_selection_event_result;
#ifdef __cplusplus
}
#endif
#endif
