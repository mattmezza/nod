/* SPDX-License-Identifier: MIT */
#ifndef CUI_COLOR_ENTRY_INTERNAL_H
#define CUI_COLOR_ENTRY_INTERNAL_H
#include "cui/cui_color_entry.h"
typedef struct {
    cui_color_entry_state *state;
    cui_color_entry_result *result;
} cui_color_entry_observation;
bool cui_color_entry_private_admit(void *, cui_str);
void cui_color_entry_private_observe(void *, const cui_component_text_event *);
cui_color_entry_result cui_color_entry_private_begin(cui_context *, cui_id, cui_color_entry_state *,
                                                     cui_color_entry_options);
void cui_color_entry_private_finish(cui_color_entry_state *, cui_color_entry_result *,
                                    cui_color_entry_handler);
#endif
