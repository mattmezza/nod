/* SPDX-License-Identifier: MIT */
#ifndef CUI_DROPDOWN_METADATA_INTERNAL_H
#define CUI_DROPDOWN_METADATA_INTERNAL_H
#include "cui/cui_dropdown_button_metadata.h"
bool cui_choice_dropdown_metadata_prepare(cui_dropdown_button_metadata *,
                                          const cui_dropdown_button_metadata *, bool, bool);
bool cui_choice_dropdown_metadata_active(const cui_dropdown_button_metadata *);
cui_dropdown_button_result cui_choice_dropdown_submit_metadata(
    cui_context *, cui_id, cui_id, cui_str, cui_str, const cui_menu_entry *, size_t, cui_id *,
    cui_menu_tree_state *, bool, bool, bool, float, bool, cui_dropdown_event_handlers,
    cui_dropdown_event_result *, const cui_dropdown_button_metadata *);
#endif
