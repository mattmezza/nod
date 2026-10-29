/* SPDX-License-Identifier: MIT */
#ifndef CUI_CALENDAR_PRESENTATION_H
#define CUI_CALENDAR_PRESENTATION_H
#include "cui_picker.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Additive opt-ins; zero is exactly the legacy Calendar path. No clock access.
 * Invalid/zero today omits decoration. Selected paint takes precedence; disabled
 * today retains muted foreground. Today does not affect eligibility/selection.
 * Month toggle applies only to a single displayed month (months 0 or 1).
 * Neighbors keep a single canonical day tab stop. Successful day activation
 * focuses that canonical ID and rebases only if outside the displayed window.
 * Existing CUI range endpoint swapping is retained. Queued targets resolve
 * against the outer call's original grid, with live eligibility per activation.
 * Locale strings and policy/user resources retain cui_picker.h's borrowed
 * lifetimes; this value-only record is not retained. No new event dispatch or
 * typed observer bridge is implied. Existing CHANGE observes selection only. */
typedef struct {
    bool month_toggle;
    cui_date today;
    bool neighboring_days; /* Paint and activate valid outside-month cell occurrences. */
} cui_calendar_presentation_options;
/* Rebuild with matching headers/library: this unpublished record now includes
 * neighboring_days. Legacy cui_calendar_options has not changed. */
typedef struct {
    cui_date date;
    cui_id id;
    bool neighboring;
} cui_calendar_cell_info;
/* Pure geometry/identity query, not a submission/eligibility test. Panel 0..2,
 * cell 0..41, first_weekday modulo 7; valid first_month is normalized to day 1.
 * In-month IDs are legacy day IDs; neighbors use date+panel occurrence IDs.
 * Nonzero root required. Invalid/boundary/zero-derived IDs fail and zero output.
 * NULL output fails. Root-scoped IDs still require ordinary collision discipline;
 * no hash scheme guarantees uniqueness across unrelated caller roots. */
bool cui_calendar_cell_resolve(cui_id root, cui_date first_month, unsigned first_weekday,
                               unsigned panel, unsigned cell, cui_calendar_cell_info *output);
/* Stable additional header ID, independent of the legacy year header (part 5).
 * Submitted only when month_toggle is effective. */
cui_id cui_calendar_month_toggle_id(cui_id root);
cui_calendar_result cui_calendar_reduce_ex(cui_context *, cui_id, cui_calendar_state *,
                                           cui_date_selection *, cui_calendar_options,
                                           cui_calendar_presentation_options);
void cui_calendar_submit_ex(cui_context *, cui_id, cui_calendar_state *, cui_date_selection,
                            cui_calendar_options, cui_calendar_presentation_options);
cui_calendar_result cui_base_calendar_ex(cui_context *, cui_id, cui_calendar_state *,
                                         cui_date_selection *, cui_calendar_options,
                                         cui_calendar_presentation_options);
cui_calendar_result cui_calendar_ex(cui_context *, cui_id, const cui_theme *, cui_calendar_state *,
                                    cui_date_selection *, cui_calendar_options,
                                    cui_calendar_presentation_options);
#ifdef __cplusplus
}
#endif
#endif
