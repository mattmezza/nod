/* SPDX-License-Identifier: MIT */
#ifndef CUI_OTP_EVENTS_H
#define CUI_OTP_EVENTS_H
#include "cui_component_events.h"
#include "cui_otp.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_component_event_metadata event;
    size_t before_bytes, after_bytes;
    size_t before_count, after_count, target_length;
    size_t accepted_count, completion_count;
    bool final_complete, has_text;
    cui_str text; /* Final committed text borrowed only through callback; masked = empty. */
} cui_otp_change_event;
typedef void (*cui_otp_change_fn)(void *user, const cui_otp_change_event *event);
typedef struct {
    cui_otp_change_fn change;
    void *user;
} cui_otp_event_handler;
typedef struct {
    cui_otp_result control; /* Original result including accepted counts and partial errors. */
    size_t change_count;    /* Actual aggregate callbacks, zero or one. */
} cui_otp_event_result;
/* Aggregate accepted operations, including no-ops/net-zero; not SUBMIT or
 * validation. Completion counts are length matches during reduction; final text
 * may be incomplete. Missing callback ignores user; disabled/passive calls reduce
 * normally without observation. Metadata gives UNKNOWN origin/completion frame.
 * Handler/user borrowed synchronously. Copy text explicitly with the bounded
 * cui_component_event_copy_utf8 helper; a struct copy does not own text.
 * No model/state/composition/context mutation, rebind, destruction, recursion or
 * foreign unwinding in callbacks. Defer edits/removal/teardown until outer call
 * and renderer consumption complete. Unrelated application state may change. */
cui_otp_event_result cui_base_otp_events(cui_context *, cui_id, cui_otp_state *, cui_otp_options,
                                         cui_otp_event_handler);
cui_otp_event_result cui_otp_events(cui_context *, cui_id, const cui_theme *, cui_otp_state *,
                                    cui_otp_options, cui_otp_event_handler);
#ifdef __cplusplus
}
#endif
#endif
