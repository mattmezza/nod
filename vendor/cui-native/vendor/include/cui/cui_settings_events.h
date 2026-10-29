/* SPDX-License-Identifier: MIT */
#ifndef CUI_SETTINGS_EVENTS_H
#define CUI_SETTINGS_EVENTS_H
#include "cui_component_events.h"
#include "cui_settings.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { CUI_SETTINGS_CHANGE_EDIT = 0, CUI_SETTINGS_CHANGE_RESET } cui_settings_change_reason;
typedef struct {
    cui_component_event_metadata event;
    size_t previous_page, previous_group, page, group;
} cui_settings_navigation_change;
typedef struct {
    cui_component_event_metadata event;
    cui_id field_id;
    size_t page, group, item;
    bool previous, value;
    cui_settings_change_reason reason;
} cui_settings_bool_change;
typedef struct {
    cui_component_event_metadata event;
    cui_id field_id;
    size_t page, group, item;
    double previous, value;
    bool previous_finite, value_finite;
    cui_settings_change_reason reason;
} cui_settings_number_change;
typedef struct {
    cui_component_event_metadata event;
    cui_id field_id;
    size_t page, group, item;
    cui_id previous, value;
    cui_settings_change_reason reason;
} cui_settings_choice_change;
typedef struct {
    cui_component_event_metadata event;
    cui_id field_id;
    size_t page, group, item;
    uint64_t before_revision, after_revision;
    size_t before_bytes, after_bytes;
    cui_str text; /* Final committed UTF-8, borrowed only during callback. */
    cui_settings_change_reason reason;
} cui_settings_text_change;
typedef struct {
    void (*on_navigation)(void *, const cui_settings_navigation_change *);
    void *user;
    void (*on_boolean)(void *, const cui_settings_bool_change *);
    void (*on_number)(void *, const cui_settings_number_change *);
    void (*on_choice)(void *, const cui_settings_choice_change *);
    void (*on_text)(void *, const cui_settings_text_change *);
} cui_settings_handler;
typedef struct {
    cui_settings_result result; /* Exact legacy aggregate, including filter repairs. */
    size_t navigation_count;    /* Actual callbacks; saturates at SIZE_MAX. */
    bool count_overflow;
    size_t boolean_count, number_count, choice_count; /* Actual calls, independently saturated. */
    size_t text_count;
} cui_settings_event_result;
typedef struct {
    bool reset; /* Exact legacy result, including false after partial progress. */
    size_t boolean_count, number_count, choice_count, text_count;
    bool count_overflow;
} cui_settings_reset_event_result;
/* Explicit programmatic reset includes disabled matching built-ins. Observes final
 * values after legacy callbacks, reason RESET, SOURCE_PROGRAMMATIC without input
 * or frame evidence. Custom reset callbacks retain their legacy behavior and have
 * no typed observation. Requires nonzero root and page; page_index is caller's
 * original source index. Owner/callback lifetime restrictions below also apply. */
cui_settings_reset_event_result cui_settings_reset_page_events(cui_id root, size_t page_index,
                                                               const cui_setting_page *,
                                                               cui_str query, cui_settings_handler);
/* Notify each actual navigation pair transition after original writes/invalidation,
 * before content. Filter/default repair and same-pair pending-scroll work are silent.
 * Missing observer permits all legacy work; no passive/layout-only observations.
 * Scalar/navigation payloads copy by value; text UTF-8 remains borrowed.
 * Callback/user/payload pointer borrowed.
 * Do not mutate/rebind/free active pages/values/editors/states/labels, input or
 * context, or recursively submit UI. Unrelated flags allowed; defer removal and
 * teardown until outer return plus renderer consumption. Legacy callbacks retain
 * their existing timing; scalar owners must remain readable through the new observer.
 * Built-in bool/number/choice EDIT observers aggregate one adapter after legacy callback,
 * using UNKNOWN cause/frame only. Number payload finite flags describe validity;
 * two NaNs and signed zeros compare equal. Text observes committed revision differences after the
 * legacy callback, including net-zero byte changes. No preedit/selection/submit observation. Reset
 * observes each built-in adapter after its old callback, reason RESET; disabled fields remain
 * eligible. UI reset uses a unique genuine activation snapshot when available, otherwise frame
 * only. Text UTF-8 is borrowed; use cui_component_event_copy_utf8 for an owned bounded copy. These
 * unreleased event payload additions require rebuilding consumers. */
cui_settings_event_result cui_settings_events(cui_context *, cui_id, const cui_theme *,
                                              const cui_setting_page *, size_t,
                                              cui_settings_state *, cui_settings_options,
                                              cui_settings_handler);
#ifdef __cplusplus
}
#endif
#endif
