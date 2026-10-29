/* SPDX-License-Identifier: MIT */
#ifndef CUI_CALENDAR_BEHAVIOR_H
#define CUI_CALENDAR_BEHAVIOR_H
#include "cui_calendar_presentation.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_CALENDAR_RANGE_LEGACY = 0,
    CUI_CALENDAR_RANGE_RESTART_EARLIER
} cui_calendar_range_transition;
typedef enum {
    CUI_CALENDAR_YEAR_OPEN_MONTHS = 0,
    CUI_CALENDAR_YEAR_OPEN_DAYS
} cui_calendar_year_destination;
typedef enum {
    CUI_CALENDAR_RECENTER_LEGACY = 0,
    CUI_CALENDAR_RECENTER_ACTIVATED_DATE,
    CUI_CALENDAR_RECENTER_SELECTION_START
} cui_calendar_recenter_policy;
typedef enum {
    CUI_CALENDAR_VALIDATE_ACTIVATED_DATE = 0,
    CUI_CALENDAR_VALIDATE_RANGE_ENDPOINTS
} cui_calendar_range_validation;
/* Independent value-only policies. Zero preserves the exact existing Calendar
 * behavior. Unknown enum values independently fall back to zero. Rebuild with
 * matching headers/library; existing picker/presentation records are unchanged.
 * RESTART_EARLIER infers range phase from valid endpoint shape on activation,
 * ignoring selecting_end: incomplete earlier activation restarts; same/later
 * completes; completed/empty ranges start again. No passive synchronization.
 * Endpoint validation checks both endpoints of a completed candidate live and
 * atomically rejects selection/phase changes if either is disabled. Interior
 * range days are not checked. Single mode ignores range policies.
 * OPEN_DAYS preserves the displayed month on year activation, clamps cursor day,
 * and focuses an enabled day in that month or the year header if none exists.
 * Explicit recenter modes apply only to accepted day activation, always put the
 * chosen anchor month first, and focus the enabled canonical anchor (or first
 * enabled visible date/year header). Selection-start uses the resulting start.
 * No clock, allocation, retained handler or new dispatch is introduced. Borrowed
 * locale/policy/user resources retain existing submission/frame lifetimes.
 * No extended typed CHANGE wrapper or binding builder is supplied.
 * DatePicker forwarding is declared separately in cui_date_picker_calendar.h. */
typedef struct {
    cui_calendar_range_transition range_transition;
    cui_calendar_year_destination year_destination;
    cui_calendar_recenter_policy recenter;
    cui_calendar_range_validation range_validation;
} cui_calendar_behavior_options;
cui_calendar_result cui_calendar_reduce_with_behavior(cui_context *, cui_id, cui_calendar_state *,
                                                      cui_date_selection *, cui_calendar_options,
                                                      cui_calendar_presentation_options,
                                                      cui_calendar_behavior_options);
cui_calendar_result cui_base_calendar_with_behavior(cui_context *, cui_id, cui_calendar_state *,
                                                    cui_date_selection *, cui_calendar_options,
                                                    cui_calendar_presentation_options,
                                                    cui_calendar_behavior_options);
cui_calendar_result cui_calendar_with_behavior(cui_context *, cui_id, const cui_theme *,
                                               cui_calendar_state *, cui_date_selection *,
                                               cui_calendar_options,
                                               cui_calendar_presentation_options,
                                               cui_calendar_behavior_options);
/* Split callers reduce with behavior, then use cui_calendar_submit_ex with the
 * same presentation record. Submission itself applies no behavior policy. */
#ifdef __cplusplus
}
#endif
#endif
