/* SPDX-License-Identifier: MIT */
#ifndef CUI_PICKER_PAINT_INTERNAL_H
#define CUI_PICKER_PAINT_INTERNAL_H
#include "cui/cui_picker.h"
void cui_picker_time_paint(cui_context *, cui_id, const cui_theme *, const cui_time *,
                           cui_time_options);
void cui_picker_actions_paint(cui_context *, cui_id, const cui_theme *, cui_calendar_state *,
                              cui_date_selection *, cui_calendar_options,
                              cui_calendar_presentation_options);
#endif
