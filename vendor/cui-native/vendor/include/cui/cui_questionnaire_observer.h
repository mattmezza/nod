/* SPDX-License-Identifier: MIT */
#ifndef CUI_QUESTIONNAIRE_OBSERVER_H
#define CUI_QUESTIONNAIRE_OBSERVER_H
#include "cui_component_events.h"
#include "cui_questionnaire_snapshots.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_QUESTIONNAIRE_PAYLOAD_READY,
    CUI_QUESTIONNAIRE_PAYLOAD_CAPACITY,
    CUI_QUESTIONNAIRE_PAYLOAD_OVERFLOW,
    CUI_QUESTIONNAIRE_PAYLOAD_NO_FRAME,
    CUI_QUESTIONNAIRE_PAYLOAD_INVALID
} cui_questionnaire_payload_status;
typedef struct {
    cui_component_event_metadata event;
    cui_questionnaire_event_kind kind;
    size_t item, previous; /* Exact legacy indices, including SIZE_MAX. */
    cui_questionnaire_payload_status payload_status;
    cui_questionnaire_snapshot_requirements required;
    bool has_previous_name, has_current_name;
    cui_str previous_name, current_name;
    cui_questionnaire_item_snapshot answer;
    cui_questionnaire_submission_snapshot submission;
} cui_questionnaire_observation;
/* Pure observational callback, before the live legacy callback. May copy payload
 * bytes and update unrelated host bookkeeping. Do not mutate/submit CUI or model,
 * attach/detach/reconfigure observers, invoke validators or destroy resources.
 * Pointer and nested views are borrowed only through this callback: copying the
 * record alone is not a retained deep copy. All copied data is independent of
 * subsequent model edits; frame reuse invalidates its storage after delivery. */
typedef void (*cui_questionnaire_observation_fn)(void *, const cui_questionnaire_observation *);
typedef struct {
    cui_questionnaire_observation_fn callback;
    void *user;
} cui_questionnaire_observation_handler;
typedef struct {
    cui_questionnaire_snapshot_workspace workspace;
} cui_questionnaire_observer_frame;
typedef struct {
    size_t attempted, delivered, payload_failures;
    bool count_overflow;
} cui_questionnaire_observer_statistics;
struct cui_questionnaire_observer {
    /* Zero-initialize once. Private bookkeeping: never write/move/copy while
     * attached/in flight. Handler/user/frame pointers are retained until detach
     * and the final in-flight operation return; pointees remain caller-owned. */
    cui_questionnaire_state *private_state;
    cui_questionnaire_observer_frame *private_frames;
    size_t private_frame_count, private_used, private_inflight;
    cui_questionnaire_observation_handler private_handler;
    cui_id private_target;
    bool private_active;
    cui_questionnaire_observer_statistics private_statistics;
};
typedef enum {
    CUI_QUESTIONNAIRE_OBSERVER_OK,
    CUI_QUESTIONNAIRE_OBSERVER_INVALID,
    CUI_QUESTIONNAIRE_OBSERVER_BUSY,
    CUI_QUESTIONNAIRE_OBSERVER_ATTACHED
} cui_questionnaire_observer_result;
/* Persistent caller-owned attachment; no allocation/registry/callback replacement.
 * State must be initialized, observer zero-initialized or fully detached, target
 * nonzero. Attach only while the model is idle (no active model/Styled operation).
 * Idle is a caller precondition for an unattached model, not a destruction guard.
 * A known in-flight attachment/reconfiguration returns BUSY, even after detach
 * requested. One attachment per state/observer; existing attachment returns
 * ATTACHED. Init clears attachment; never reinit an attached/in-flight model.
 * All frame records/arenas, observer, source/model/editor storage are disjoint,
 * valid caller objects. Nonzero capacity requires non-NULL storage. Zero frames
 * is valid: a non-NULL handler receives NO_FRAME. No overlap/provenance detection.
 * Attach copies handler and frame-array pointer, not the array/workspaces. Keep
 * them immutable/alive until fully detached and outer operation/renderer return.
 * Missing handler counts attempts but performs no payload copying/delivery.
 * Attach resets statistics; no synthetic attach/detach/lifecycle notifications. */
cui_questionnaire_observer_result
cui_questionnaire_observer_attach(cui_questionnaire_state *, cui_questionnaire_observer *,
                                  cui_id target, cui_questionnaire_observer_frame *,
                                  size_t frame_count, cui_questionnaire_observation_handler);
/* May be called from the legacy callback. Suppresses subsequent typed attempts
 * immediately; pins all storage until final in-flight release. Model association
 * clears when drained; BUSY reports that detach is pending, not rejected. Do not
 * free active state/user/frame storage on callback return; wait for outer return.
 * Already detached is OK. Typed handlers must not call this. Legacy callbacks
 * otherwise retain original synchronous mutations, nesting and handler changes. */
cui_questionnaire_observer_result cui_questionnaire_observer_detach(cui_questionnaire_observer *);
cui_questionnaire_observer_statistics
cui_questionnaire_observer_get_statistics(const cui_questionnaire_observer *);
/* One typed attempt per active legacy emission, before the original legacy call.
 * CHANGE=current/answer, COMMIT=completed, SUBMIT=submit. Default origin UNKNOWN:
 * no frame/index/source/activation flags. Target is caller identity, not a UI bind.
 * Completed/Submit share one frozen payload/failure captured after validation;
 * legacy callback edits/nested operations do not recapture that outer payload.
 * Nested legacy calls use extra frames. Frame/capacity/overflow failures deliver
 * empty copied fields with explicit status (CAPACITY includes exact needs), never
 * partial data, retries, legacy event loss, rollback or changed model returns.
 * INVALID defensively reports failed copy preflight; valid model/disjoint frame
 * preconditions still apply. Counts saturate. No concurrent use/typed reentrancy,
 * input dispatch change, physical AT evidence or foreign binding API implied. */
#ifdef __cplusplus
}
#endif
#endif
