/* SPDX-License-Identifier: MIT */
#ifndef CUI_FORM_PRIVATE_H
#define CUI_FORM_PRIVATE_H
#include "cui/cui_form_events.h"
/* Synchronous delegation at the original nested Input seam; Base-only types. */
typedef cui_input_event_result (*cui_form_private_input_fn)(void *, cui_context *, cui_id, cui_str,
                                                            cui_edit *, cui_input_behavior_state *,
                                                            cui_base_input_options,
                                                            bool synthetic_hit,
                                                            cui_component_text_handler);
cui_input_group_result
cui_form_private_input_group(cui_context *, cui_id, cui_id, cui_edit *, cui_input_group_state *,
                             const cui_input_group_addon *, size_t, cui_input_group_options,
                             cui_input_group_handler, cui_input_group_event_result *,
                             cui_form_private_input_fn, void *);
#endif
