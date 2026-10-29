/* SPDX-License-Identifier: MIT */
#ifndef CUI_PICKER_COMPOSITION_INTERNAL_H
#define CUI_PICKER_COMPOSITION_INTERNAL_H
#include "cui/cui_date_picker_calendar.h"
typedef enum { CUI_PICKER_ACCEPT_CONTINUE, CUI_PICKER_ACCEPT_STOP } cui_picker_accept_flow;
typedef cui_picker_accept_flow (*cui_picker_accept_visitor)(void *, size_t, cui_date_selection);
/* Private value composition; legacy public factories still reduce+submit/close. */
cui_calendar_result cui_picker_datetime_reduce(cui_context *, cui_id, cui_id,
                                               cui_date_picker_state *, cui_date_selection *,
                                               cui_date_picker_options,
                                               cui_date_picker_calendar_options,
                                               bool *open_after_trigger, cui_picker_accept_visitor,
                                               void *);
bool cui_picker_time_reduce(cui_context *, cui_id, cui_str, cui_time_state *, cui_time *,
                            cui_time_options);
void cui_picker_time_submit(cui_context *, cui_id, cui_str, cui_time_state *, cui_time *,
                            cui_time_options);
#endif
