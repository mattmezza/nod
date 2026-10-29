/* SPDX-License-Identifier: MIT */
#ifndef CUI_OTP_H
#define CUI_OTP_H
#include "cui.h"
#include "cui_edit.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { CUI_OTP_MEDIUM, CUI_OTP_XSMALL, CUI_OTP_SMALL, CUI_OTP_LARGE } cui_otp_size;
/* Optional caller-owned bounded UTF-8 preedit. Zero-initialize all fields except
 * buffer/capacity; keep storage alive and disjoint from the OTP value/state.
 * Default UI-owned session holds 4095 preedit bytes. No OTP undo history. */
typedef struct {
    char *buffer;
    size_t capacity, length, anchor, cursor, document_length;
    const char *document;
    uint64_t document_hash;
    bool active, stale;
} cui_otp_composition;
typedef struct {
    char *buffer;
    size_t capacity, length;
    bool masked, focused, cursor_visible;
    double blink_epoch;
    cui_otp_composition *composition;
    bool composition_owned; /* Internal UI resource; false for caller storage. */
} cui_otp_state;
typedef struct {
    cui_str name;
    size_t groups; /* Use default() for two groups; zero explicitly clamps to one. */
    cui_otp_size size;
    float cell_size, font_size, gap, group_gap;
    bool disabled, hide_focus_ring;
    double now_seconds, blink_interval; /* Host monotonic time; interval defaults to .5s. */
    cui_design_size design_size;        /* INHERIT preserves legacy size; styled APIs only. */
} cui_otp_options;
typedef struct {
    bool changed, complete, focused, blurred;
    size_t change_count, complete_count;
    double next_wake_seconds; /* Absolute host time, or zero if no visible caret needs a wake. */
    cui_edit_result error;
} cui_otp_result;
/* Borrow a valid NUL-terminated buffer. Target length is bounded to 256 cells.
 * Programmatic UTF-8 values may be arbitrary/overlength, as in the reference. */
cui_edit_result cui_otp_init(cui_otp_state *, char *, size_t capacity, size_t length);
cui_edit_result cui_otp_set_value(cui_otp_state *, cui_str);
cui_str cui_otp_value(const cui_otp_state *);
size_t cui_otp_count(const cui_otp_state *);
cui_otp_options cui_otp_options_default(void);
size_t cui_otp_groups(size_t length, size_t requested);
cui_id cui_otp_cell_id(cui_id input, size_t cell);
cui_otp_result cui_base_otp(cui_context *, cui_id, cui_otp_state *, cui_otp_options);
cui_otp_result cui_otp(cui_context *, cui_id, const cui_theme *, cui_otp_state *, cui_otp_options);
#ifdef __cplusplus
}
#endif
#endif
