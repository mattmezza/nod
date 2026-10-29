/* SPDX-License-Identifier: MIT */
#ifndef CUI_MENU_EVENTS_H
#define CUI_MENU_EVENTS_H
#include "cui_component_events.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Applied command notification, not a vetoable request or value CHANGE.
 * Pointer-free copy; target is the public trigger, command is the leaf ID. */
typedef struct {
    cui_component_event_metadata event;
    cui_id command;
} cui_menu_command;
typedef void (*cui_menu_command_fn)(void *, const cui_menu_command *);
typedef struct {
    cui_menu_command_fn on_activate;
    void *user;
} cui_menu_command_handler;
typedef struct {
    bool valid, submitted, count_overflow;
    size_t command_count; /* Observer calls; missing observer means zero. */
    bool legacy_activated;
} cui_menu_command_result;
#ifdef __cplusplus
}
#endif
#endif
