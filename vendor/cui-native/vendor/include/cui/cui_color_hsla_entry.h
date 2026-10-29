/* SPDX-License-Identifier: MIT */
#ifndef CUI_COLOR_HSLA_ENTRY_H
#define CUI_COLOR_HSLA_ENTRY_H
#include "cui_color_hsla.h"
#include "cui_components.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    bool present;
    cui_color_hsla color;
} cui_color_hsla_value;
typedef enum {
    CUI_COLOR_HSLA_ENTRY_OK,
    CUI_COLOR_HSLA_ENTRY_INVALID_ARGUMENT,
    CUI_COLOR_HSLA_ENTRY_CONFLICT,
    CUI_COLOR_HSLA_ENTRY_INCOMPLETE,
    CUI_COLOR_HSLA_ENTRY_ALLOCATION_FAILED,
    CUI_COLOR_HSLA_ENTRY_EDIT_ERROR
} cui_color_hsla_entry_outcome;
typedef enum {
    CUI_COLOR_HSLA_COMMIT_SELECT, /* Successful commit requests close. */
    CUI_COLOR_HSLA_COMMIT_UPDATE  /* Successful commit leaves open policy alone. */
} cui_color_hsla_commit_kind;
typedef struct {
    cui_color_hsla_entry_outcome outcome;
    bool committed, close_requested;
    cui_color_hsla_value previous, value;
} cui_color_hsla_commit_result;
typedef struct {
    char draft[10];
    cui_edit *edit;
    cui_input_state input;
    cui_color_hsla_value committed, preview;
    cui_color_hsla_hex_status status;
    bool dirty;
} cui_color_hsla_entry_state;
/* Zero-init then init once. Address-stable/noncopyable: editor borrows draft.
 * All members read-only to callers. Destroy is idempotent after init; wait until
 * prior draw consumption. Model operations forbidden during a UI submission.
 * Present values must be finite normalized floats; absence ignores/zeros color.
 * Failed transactions preserve draft, editor identity/history and model fields.
 * Replacement operations allocate a fresh editor, resetting Input/history only
 * on success. No model operation calls handlers or fabricates input metadata. */
cui_color_hsla_entry_outcome cui_color_hsla_entry_init(cui_color_hsla_entry_state *,
                                                       cui_color_hsla_value);
void cui_color_hsla_entry_destroy(cui_color_hsla_entry_state *);
/* Dirty conflicts unless replace_draft, including equal owner values. Silent
 * sync copies committed/preview directly; formatting never reparses the model. */
cui_color_hsla_entry_outcome cui_color_hsla_entry_sync(cui_color_hsla_entry_state *,
                                                       cui_color_hsla_value, bool replace_draft);
/* Empty or # plus <=8 ASCII hex digits; complete draft previews only. Incomplete
 * retains prior preview. Whole-content editor change may retain undo history. */
cui_color_hsla_entry_outcome cui_color_hsla_entry_set_draft(cui_color_hsla_entry_state *, cui_str);
/* Replace preview/display only; restore uses committed optional selection.
 * Both preserve committed, clear dirty, and respect explicit dirty replacement. */
cui_color_hsla_entry_outcome cui_color_hsla_entry_preview(cui_color_hsla_entry_state *,
                                                          cui_color_hsla, bool replace_draft);
cui_color_hsla_entry_outcome cui_color_hsla_entry_restore(cui_color_hsla_entry_state *,
                                                          bool replace_draft);
/* Explicit hex commit always reparses current text, even unchanged formatted
 * display; precision loss is intentional here. No text/history rewrite. */
cui_color_hsla_commit_result cui_color_hsla_entry_commit_hex(cui_color_hsla_entry_state *,
                                                             cui_color_hsla_commit_kind);
/* Commit supplied floats directly using replacement transaction, not hex parse.
 * SELECT requests close, UPDATE does not. No popup is owned or dismissed here.
 * Equal-value commits succeed. Failure copies unchanged previous/value with
 * committed/close_requested false. Invalid state returns zero selection facts. */
cui_color_hsla_commit_result cui_color_hsla_entry_commit_value(cui_color_hsla_entry_state *,
                                                               cui_color_hsla,
                                                               cui_color_hsla_commit_kind,
                                                               bool replace_draft);

typedef struct {
    bool disabled, readonly;
    float width; /* Styled Input width; Base caller supplies layout. */
} cui_color_hsla_entry_options;
typedef struct {
    cui_input_result input;
    bool submitted; /* Eligible active field submission, not an Enter count. */
    bool draft_changed;
    size_t commit_count, rejected_commit_count, close_request_count;
    cui_color_hsla_hex_status status;
    cui_color_hsla_value previous, committed, preview;
    cui_component_event_metadata event;
} cui_color_hsla_entry_result;
typedef void (*cui_color_hsla_entry_commit_fn)(void *, const cui_color_hsla_entry_result *);
typedef struct {
    cui_color_hsla_entry_commit_fn on_commit;
    void *user;
} cui_color_hsla_entry_handler;
/* Actual Input field; accepted focused nonrepeated Enter uses SELECT in queue
 * order. Incomplete attempts count separately from input.rejected admission.
 * Counts independent of observer; close_request_count is not dismissal evidence.
 * submitted means active non-inert/non-layout submission, disabled/readonly off.
 * Summary snapshots entry-previous/final values, UNKNOWN + actual frame only.
 * Handler/user/options snapshotted at entry; one callback only if commits>0,
 * after Base semantics or styled paint. Payload copyable beyond callback/context.
 * Borrowed labels/handler user must survive outer return. Do not mutate/free live
 * state/context/borrowed resources, nor recursively submit UI from callback;
 * update a separate owner for next submission. Base caller supplies layout.
 * No popup, HSLA tabs, screen sampler, byte ColorEntry or foreign wrappers added. */
cui_color_hsla_entry_result cui_base_color_hsla_entry(cui_context *, cui_id, cui_str name,
                                                      cui_color_hsla_entry_state *,
                                                      cui_color_hsla_entry_options,
                                                      cui_color_hsla_entry_handler);
cui_color_hsla_entry_result cui_color_hsla_entry(cui_context *, cui_id, const cui_theme *,
                                                 cui_str name, cui_color_hsla_entry_state *,
                                                 cui_color_hsla_entry_options,
                                                 cui_color_hsla_entry_handler);

#ifdef __cplusplus
}
#endif
#endif
