/* SPDX-License-Identifier: MIT */
#ifndef CUI_INPUT_PRIVATE_H
#define CUI_INPUT_PRIVATE_H
#include "cui/cui_components.h"
/* Prepare and reduce once, never paint. Actual behavior owner remains observable
 * during callbacks; local hit scratch does not escape the synchronous call. */
cui_input_event_result cui_input_private_reduce(cui_context *, cui_id, const cui_theme *, cui_str,
                                                cui_edit *, cui_input_state *,
                                                cui_input_behavior_state *, cui_input_options *,
                                                cui_base_input_options, cui_rect viewport,
                                                bool synthetic_hit, cui_component_text_handler);
#endif
