/* SPDX-License-Identifier: MIT */
#ifndef CUI_DATE_PICKER_CALENDAR_H
#define CUI_DATE_PICKER_CALENDAR_H
#include "cui_calendar_behavior.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Value-only Calendar extensions; zero preserves the exact legacy DatePicker.
 * Locale/month count/bounds/disabled policy remain in cui_date_picker_options.
 * Existing Calendar normalization, single-month toggle and host-today rules apply.
 * No clock, retained pointers/handlers or allocation. Rebuild matching library
 * and headers. Legacy public records are unchanged. Borrowed state, selection,
 * locale strings and pure predicate/user resources retain their existing
 * submission/frame/render-consumption lifetime. Do not mutate UI from predicates.
 * Existing typed CHANGE wrappers do not accept this extension; no observer or
 * open/close lifecycle notification is added. Builders/FFI require separate work. */
typedef struct {
    cui_calendar_presentation_options presentation;
    cui_calendar_behavior_options behavior;
} cui_date_picker_calendar_options;
/* Styled's existing Calendar ID, without changing legacy ID allocation. Returns
 * zero for a zero trigger or zero derived ID. Caller IDs still require collision
 * discipline; this helper does not promise global uniqueness. */
cui_id cui_date_picker_calendar_id(cui_id trigger);
/* Base reduces once and submits trigger semantics. Caller owns popup layout,
 * overlay begin/end and cui_calendar_submit_ex using the same presentation.
 * Escape prepass, overlay dismissal/trigger toggles, then the entire Calendar
 * queue retain legacy pass ordering. After that queue, an accepted single date
 * or completed range closes and restores trigger focus. No mid-queue close.
 * Endpoint restart remains open; rejected completion does not close. Opening or
 * supplying options never resets Calendar state or range phase. Outer disabled
 * closes; Calendar-only disabled keeps trigger operable and suppresses day input. */
cui_calendar_result cui_base_date_picker_ex(cui_context *, cui_id trigger, cui_id calendar,
                                            cui_str name, cui_date_picker_state *,
                                            cui_date_selection *, cui_date_picker_options,
                                            cui_date_picker_calendar_options);
/* Styled also places/submits/paints the popup. Exact legacy bool: a late invalid
 * viewport or overlay-begin failure can return false after an applied edit.
 * Use caller selection/state to inspect final values. No second reduction. */
bool cui_date_picker_ex(cui_context *, cui_id trigger, const cui_theme *, cui_str name,
                        cui_date_picker_state *, cui_date_selection *, cui_date_picker_options,
                        cui_date_picker_calendar_options);
#ifdef __cplusplus
}
#endif
#endif
