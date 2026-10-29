/* SPDX-License-Identifier: MIT */
#ifndef CUI_DATE_PICKER_ACTIONS_H
#define CUI_DATE_PICKER_ACTIONS_H
#include "cui_date_picker_calendar.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Opt-in date-only actions. See docs/DATE_PICKER_ACTIONS.md for validation,
 * independent child ownership, focus/queue policies and borrowed lifetimes.
 * Existing picker APIs/layouts are unchanged. No observer/binding facade. */
#define CUI_DATE_PICKER_PRESET_LIMIT 8u
#define CUI_DATE_PICKER_LABEL_LIMIT 96u
#define CUI_DATE_PICKER_TARGET_LIMIT 144u

typedef enum { CUI_DATE_PICKER_SINGLE = 0, CUI_DATE_PICKER_RANGE } cui_date_picker_shape;

typedef struct {
    cui_date_selection dates;
    cui_date_picker_shape shape;
} cui_date_picker_value;

typedef struct {
    cui_id id;
    cui_str label;
    cui_date_picker_value value;
} cui_date_picker_preset;

typedef enum {
    CUI_DATE_PICKER_PRESET_SOURCE = 0,
    CUI_DATE_PICKER_PRESET_MATCH_SHAPE,
    CUI_DATE_PICKER_PRESET_MATCH_SHAPE_ENABLED
} cui_date_picker_preset_policy;

typedef enum {
    CUI_DATE_PICKER_DELETE_OFF = 0,
    CUI_DATE_PICKER_DELETE_TRIGGER,
    CUI_DATE_PICKER_DELETE_SCOPE
} cui_date_picker_delete_policy;

typedef struct {
    cui_date_picker_options picker;
    cui_date_picker_calendar_options calendar;
    const cui_date_picker_preset *presets;
    size_t preset_count;
    cui_id clear_id;
    cui_str clear_label;
    bool cleanable;
    cui_date_picker_delete_policy delete_policy;
    cui_date_picker_preset_policy preset_policy;
} cui_date_picker_actions_options;

/* Separate retained Calendar selection is required for source policy. */
typedef struct {
    cui_date_picker_state picker;
    cui_date_picker_value calendar_value;
    cui_date_picker_value observed_value;
    bool synchronized;
    /* Previous submission identities, copied values only; do not edit directly. */
    cui_calendar_state submitted_calendar;
    cui_id submitted_root, submitted_presets[CUI_DATE_PICKER_PRESET_LIMIT];
    size_t submitted_count;
    bool submitted_open, submitted_clear;
    uint64_t prepared_frame, submitted_frame, clear_frame;
    cui_id prepared_root, prepared_trigger, submitted_clear_id;
    unsigned submitted_months, submitted_first_weekday;
    cui_id submitted_targets[CUI_DATE_PICKER_TARGET_LIMIT];
    size_t submitted_target_count;
} cui_date_picker_actions_state;

void cui_date_picker_actions_init(cui_date_picker_actions_state *, cui_id popup,
                                  cui_date initial_month);

typedef struct {
    cui_calendar_result calendar;
    bool valid;
    bool changed;
    bool action_activated;
} cui_date_picker_actions_result;

/* Inside caller-owned open popup scope, after a valid Base reduction.
 * Publishes bounded prior-submission identity only after full successful submission.
 * Semantics only: no reduction, callbacks or scope begin/end. */
void cui_date_picker_actions_submit_presets(cui_context *, cui_date_picker_actions_state *,
                                            cui_date_picker_actions_options);

cui_date_picker_actions_result cui_base_date_picker_actions(cui_context *, cui_id trigger,
                                                            cui_id calendar, cui_str name,
                                                            cui_date_picker_actions_state *,
                                                            cui_date_picker_value *,
                                                            cui_date_picker_actions_options);

cui_date_picker_actions_result cui_date_picker_actions(cui_context *, cui_id trigger,
                                                       const cui_theme *, cui_str name,
                                                       cui_date_picker_actions_state *,
                                                       cui_date_picker_value *,
                                                       cui_date_picker_actions_options);

#ifdef __cplusplus
}
#endif
#endif
