/* SPDX-License-Identifier: MIT */
#ifndef CUI_COLOR_ENTRY_H
#define CUI_COLOR_ENTRY_H
#include "cui_components.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Additive inline hex entry, not popup/HSLA or the legacy HSV picker. */
typedef enum {
    CUI_COLOR_HEX_INVALID,
    CUI_COLOR_HEX_INCOMPLETE,
    CUI_COLOR_HEX_COMPLETE
} cui_color_hex_status;
/* Optional #; ASCII hex widths 3/4/6/8. Output written only on COMPLETE.
 * Empty and hex prefixes of widths 1/2/5/7 are INCOMPLETE. No CSS grammar.
 * Source and output must be disjoint; output may be NULL for validation. */
cui_color_hex_status cui_color_entry_parse(cui_str, cui_color *output);
typedef struct {
    bool present;
    cui_color color;
} cui_color_entry_value;
typedef enum {
    CUI_COLOR_ENTRY_OK,
    CUI_COLOR_ENTRY_INVALID_ARGUMENT,
    CUI_COLOR_ENTRY_CONFLICT,
    CUI_COLOR_ENTRY_INCOMPLETE,
    CUI_COLOR_ENTRY_EDIT_ERROR
} cui_color_entry_outcome;
typedef struct {
    char draft[10];
    cui_edit *edit;
    cui_input_state input;
    cui_color_entry_value committed, preview;
    cui_color_hex_status status;
    bool dirty;
} cui_color_entry_state;
/* Zero-init, then init exactly once; failure leaves state unchanged. Address
 * stable/noncopyable until destroy: editor borrows draft. Model fields are
 * read-only to callers; use operations below. Destroy after render consumption.
 * Destroy is idempotent. Do not expose/mutate the internal editor independently. */
bool cui_color_entry_init(cui_color_entry_state *, cui_color_entry_value);
void cui_color_entry_destroy(cui_color_entry_state *);
/* Dirty draft conflicts unless replace_draft. Successful owner sync is silent,
 * canonicalizes draft and resets editor content/history; absent value clears it.
 * Failure preserves all state. Never call during an active field submission. */
cui_color_entry_outcome cui_color_entry_sync(cui_color_entry_state *, cui_color_entry_value,
                                             bool replace_draft);
/* UI admission: empty or # followed by 0..8 ASCII hex digits. Transactional;
 * updates preview on COMPLETE only. Owner operations do not notify UI handlers. */
cui_color_entry_outcome cui_color_entry_set_draft(cui_color_entry_state *, cui_str);
/* COMPLETE only; same-value commits succeed. Does not rewrite the draft. */
cui_color_entry_outcome cui_color_entry_commit(cui_color_entry_state *);
/* Discard draft/preview and restore the committed optional selection silently. */
cui_color_entry_outcome cui_color_entry_restore(cui_color_entry_state *);
typedef struct {
    cui_input_result input;
    bool submitted; /* Eligible active field submission, not Enter. */
    bool draft_changed;
    size_t commit_count, rejected_commit_count;
    cui_color_hex_status status;
    cui_color_entry_value previous, committed, preview;
    cui_component_event_metadata event; /* UNKNOWN, actual frame only. */
} cui_color_entry_result;
typedef void (*cui_color_entry_commit_fn)(void *, const cui_color_entry_result *);
typedef struct {
    cui_color_entry_commit_fn on_commit;
    void *user;
} cui_color_entry_handler;
typedef struct {
    bool disabled, readonly;
    float width;
} cui_color_entry_options;
/* Input's focused, accepted, nonrepeated Enter notifications commit in queue
 * order. Incomplete attempts count separately. No callback for draft-only edits.
 * Handler snapshot runs once after semantics (Base) / paint (Styled) if commits
 * occurred. Counts are independent of handler; summary has no input index/cause.
 * Payload is copyable. Do not mutate/free state/context/borrowed label during
 * callback; update a separate owner model for next submission. No nested UI.
 * Base supplies semantics only; caller supplies layout for id. Styled lays out
 * and paints a standard Input. No popup close, HSLA tabs or eye-dropper added. */
cui_color_entry_result cui_base_color_entry(cui_context *, cui_id, cui_str name,
                                            cui_color_entry_state *, cui_color_entry_options,
                                            cui_color_entry_handler);
cui_color_entry_result cui_color_entry(cui_context *, cui_id, const cui_theme *, cui_str name,
                                       cui_color_entry_state *, cui_color_entry_options,
                                       cui_color_entry_handler);
#ifdef __cplusplus
}
#endif
#endif
