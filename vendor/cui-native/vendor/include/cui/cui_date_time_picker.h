/* SPDX-License-Identifier: MIT */
#ifndef CUI_DATE_TIME_PICKER_H
#define CUI_DATE_TIME_PICKER_H
#include "cui_date_picker_actions.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Single civil date/time, Base and Styled. See docs/DATE_TIME_PICKER.md. */
typedef enum { CUI_DATE_TIME_MINUTE = 0, CUI_DATE_TIME_SECOND } cui_date_time_precision;
typedef struct {
    cui_date date; /* All-zero means absent. No timezone or range. */
    cui_time time; /* Valid retained time even when date is absent. */
} cui_date_time_value;
typedef struct {
    cui_id id;
    cui_str label;
    cui_date date;
    bool has_time;
    cui_time time; /* Ignored unless has_time. */
} cui_date_time_preset;
typedef struct {
    cui_date_picker_options picker;
    cui_date_picker_calendar_options calendar;
    cui_date_time_precision precision;
    bool twelve_hour;
    cui_time default_time;
    cui_str time_label; /* Empty uses "Time". */
    const cui_date_time_preset *presets;
    size_t preset_count;
    cui_id clear_id;
    cui_str clear_label;
    bool cleanable;
    cui_date_picker_delete_policy delete_policy;
    cui_date_picker_preset_policy preset_policy;
} cui_date_time_picker_options;
typedef struct {
    cui_date_picker_actions_state actions;
    cui_time_state time_field;
    cui_date_time_value observed;
    cui_time applied_default;
    cui_date_time_precision applied_precision;
    bool applied_twelve_hour, configured, assignment_pending;
    /* Context-local copied identities. Do not edit or move between contexts. */
    cui_id time_id;
    uint64_t time_submitted_frame;
} cui_date_time_picker_state;
typedef struct {
    cui_calendar_result calendar;
    bool valid, value_changed, source_change;
    bool time_edited, action_activated, confirmed;
} cui_date_time_picker_result;
void cui_date_time_picker_init(cui_date_time_picker_state *, cui_id popup, cui_date initial_month);
/* Between frames; valid assignment queues child synchronization even if equal. */
bool cui_date_time_picker_assign(cui_date_time_picker_state *, cui_date_time_value *controlled,
                                 cui_date_time_value next);
cui_id cui_date_time_picker_time_id(cui_id trigger);
cui_date_time_picker_result cui_base_date_time_picker(cui_context *, cui_id trigger,
                                                      cui_id calendar, cui_id time, cui_str name,
                                                      cui_date_time_picker_state *,
                                                      cui_date_time_value *,
                                                      cui_date_time_picker_options);
/* Styled: owns the popup scope and paints Calendar/time/actions once. */
cui_date_time_picker_result cui_date_time_picker(cui_context *, cui_id trigger, const cui_theme *,
                                                 cui_str name, cui_date_time_picker_state *,
                                                 cui_date_time_value *,
                                                 cui_date_time_picker_options);
/* Inside caller's one open popup scope AFTER Base reduction: semantics only.
 * Submit Calendar separately. Neither helper reduces input or begins a scope. */
void cui_date_time_picker_submit_time(cui_context *, cui_id time, cui_date_time_picker_state *,
                                      const cui_date_time_value *, cui_date_time_picker_options);
void cui_date_time_picker_submit_presets(cui_context *, cui_date_time_picker_state *,
                                         cui_date_time_picker_options);
#ifdef __cplusplus
}
#endif
#endif
