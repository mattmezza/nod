/* SPDX-License-Identifier: MIT */
#ifndef CUI_QUESTIONNAIRE_SNAPSHOTS_H
#define CUI_QUESTIONNAIRE_SNAPSHOTS_H
#include "cui_questionnaire.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Pure Base queries: no allocation, validation, callbacks, editor synchronization,
 * timer service or model mutation. Only initialized, unmodified valid model
 * storage is accepted; callers must not forge or concurrently edit its fields.
 * Snapshot strings/records belong to caller workspace and survive model edits
 * and source teardown. Reusing workspace successfully invalidates earlier copies.
 * Copying records alone does not copy/rebase their nested pointers.
 * All workspace regions, output records and source/model/schema/editor regions
 * must be mutually disjoint. Overlap/pointer provenance is a caller precondition,
 * not runtime-detected. All provided records/pointers must be valid C objects. */
typedef enum {
    CUI_QUESTIONNAIRE_SNAPSHOT_OK,
    CUI_QUESTIONNAIRE_SNAPSHOT_INVALID,
    CUI_QUESTIONNAIRE_SNAPSHOT_UNKNOWN_ITEM,
    CUI_QUESTIONNAIRE_SNAPSHOT_CAPACITY,
    CUI_QUESTIONNAIRE_SNAPSHOT_OVERFLOW
} cui_questionnaire_snapshot_result;

typedef struct {
    const cui_str *choices; /* copied values in choice-definition order */
    size_t choice_count;
    bool has_freeform;
    cui_str freeform; /* copied effective answer, not editor draft */
} cui_questionnaire_answer_snapshot;

typedef struct {
    size_t item;  /* original schema index, not enabled index */
    cui_str name; /* copied identity, not presentation label */
    cui_questionnaire_status status;
    bool disabled; /* useful for an individual disabled item */
    cui_questionnaire_answer_snapshot answer;
} cui_questionnaire_item_snapshot;

typedef struct {
    const cui_questionnaire_item_snapshot *items;
    size_t count; /* enabled items in schema order */
} cui_questionnaire_submission_snapshot;

typedef struct {
    size_t items, choices, text_bytes; /* bytes INCLUDE all appended terminators */
} cui_questionnaire_snapshot_requirements;

typedef struct {
    cui_questionnaire_item_snapshot *items;
    size_t item_capacity;
    cui_str *choices;
    size_t choice_capacity;
    char *text;
    size_t text_capacity;
} cui_questionnaire_snapshot_workspace;

typedef struct {
    size_t current_item;   /* schema index, SIZE_MAX when none */
    size_t position;       /* enabled zero-based index, SIZE_MAX if none */
    size_t current, total; /* one-based progress (or 0), enabled total */
    bool previous_visible, next_visible, skip_visible, submit_visible;
    bool confirmable, complete;
} cui_questionnaire_query_snapshot;

cui_questionnaire_snapshot_result
cui_questionnaire_answer_measure(const cui_questionnaire_state *, size_t item,
                                 cui_questionnaire_snapshot_requirements *required);
cui_questionnaire_snapshot_result cui_questionnaire_answer_copy(
    const cui_questionnaire_state *, size_t item, cui_questionnaire_snapshot_workspace,
    cui_questionnaire_snapshot_requirements *required, cui_questionnaire_item_snapshot *out);
cui_questionnaire_snapshot_result
cui_questionnaire_submission_measure(const cui_questionnaire_state *,
                                     cui_questionnaire_snapshot_requirements *required);
cui_questionnaire_snapshot_result cui_questionnaire_submission_copy(
    const cui_questionnaire_state *, cui_questionnaire_snapshot_workspace,
    cui_questionnaire_snapshot_requirements *required, cui_questionnaire_submission_snapshot *out);
cui_questionnaire_snapshot_result
cui_questionnaire_query_copy(const cui_questionnaire_state *,
                             cui_questionnaire_query_snapshot *out);

/* required/out pointers are mandatory. Failure empties provided outputs (query
 * item/position become SIZE_MAX); CAPACITY alone publishes exact requirements.
 * Entire capacity/arithmetic preflight precedes any workspace write: failed copy
 * preserves workspace bytes and therefore an older successful snapshot.
 * NULL workspace pointers require zero capacity; zero capacity permits non-NULL.
 * Measure/copy can see different states: copy remeasures independently.
 * Disabled individual items keep name/status but have empty answers; submissions
 * omit disabled items, include all other statuses in schema order. Choices use
 * selected enabled values in definition order, never presentation labels.
 * Text contains names, choice values, then present freeform for each item, each
 * followed by NUL. cui_str lengths exclude NUL; required.text_bytes includes all
 * terminators. Empty names/values consume one byte. No truncation/deduplication.
 * Current model UTF-8 validation rejects embedded NUL. Absent freeform,
 * zero choices and empty submissions have NULL data pointers and zero lengths.
 * Freeform is retained effective input, not unsynchronized editor drafts.
 * Submission copying does not validate or submit; data may be unanswered/invalid.
 * Query confirmable means answered/skipped, not validated; visibility ignores
 * presentation options. This is no event snapshot or observer API. */
#ifdef __cplusplus
}
#endif
#endif
