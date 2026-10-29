/* SPDX-License-Identifier: MIT */
#ifndef CUI_COMMAND_EVENTS_PRIVATE_H
#define CUI_COMMAND_EVENTS_PRIVATE_H
#include "cui/cui_command_events.h"
typedef struct {
    cui_id target;
    cui_command_event_handler handler;
    cui_command_event_result result;
    cui_component_event_metadata cause;
    bool has_cause, suppress;
} cui_command_events_private_observer;
bool cui_command_events_private_rebuild(cui_command_state *, const cui_command_entry *, size_t,
                                        cui_str, bool, cui_command_options,
                                        cui_command_events_private_observer *);
bool cui_command_events_private_select(cui_command_state *, cui_command_index, bool,
                                       cui_command_options, cui_command_events_private_observer *);
bool cui_command_events_private_move(cui_command_state *, bool, cui_command_options,
                                     cui_command_events_private_observer *);
bool cui_command_events_private_confirm(cui_command_state *, cui_command_options,
                                        cui_command_events_private_observer *);
#endif
