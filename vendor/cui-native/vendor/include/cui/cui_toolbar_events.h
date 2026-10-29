/* SPDX-License-Identifier: MIT */
#ifndef CUI_TOOLBAR_EVENTS_H
#define CUI_TOOLBAR_EVENTS_H
#include "cui_control_events.h"
#include "cui_toolbar.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    bool valid, submitted;
    size_t activation_count; /* Actual observer calls: zero or one. */
} cui_toolbar_command_event_result;
/* Aggregate ACTIVATE after paint and the legacy on_activate. Evidence freezes
 * before icon/legacy callbacks: multiple accepted activations report only frame,
 * UNKNOWN source. Missing callback ignores user and preserves interaction.
 * Handler/user and active command/presentation/resources borrow through return
 * and renderer consumption. Copy metadata or update unrelated state; do not
 * mutate/free/rebind active resources, recurse, destroy context or unwind across C.
 * Container disabled suppresses navigation only, not enabled child commands. */
cui_toolbar_command_event_result cui_toolbar_command_events(cui_context *, const cui_theme *,
                                                            const cui_toolbar_presentation *,
                                                            cui_toolbar_command *,
                                                            cui_component_activation_handler);
#ifdef __cplusplus
}
#endif
#endif
