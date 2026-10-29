/* SPDX-License-Identifier: MIT */
#ifndef CUI_EDIT_H
#define CUI_EDIT_H
#include "cui.h"
#include "cui_text_match.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct cui_edit cui_edit;
typedef enum {
    CUI_EDIT_OK,
    CUI_EDIT_INVALID_UTF8,
    CUI_EDIT_CAPACITY,
    CUI_EDIT_BOUNDARY,
    CUI_EDIT_NO_HISTORY,
    CUI_EDIT_INVALID_ARGUMENT,
    CUI_EDIT_LINE_BREAK,
    CUI_EDIT_CLIPBOARD_UNAVAILABLE,
    CUI_EDIT_PRESENTATION,
    CUI_EDIT_MATCHER,
    CUI_EDIT_MATCHER_STALE,
    CUI_EDIT_MATCHER_BUSY
} cui_edit_result;
typedef struct {
    char *buffer;
    size_t capacity;
    bool multiline;
    size_t history_bytes, max_history_steps;
} cui_edit_config;
/* Caller-owned reusable IME session, one primary selection only. Preedit and
 * fixed-mode view buffers must be disjoint and remain alive during use; grow_view
 * permits a null view buffer. Capacities include the trailing NUL. Zero-initialize
 * the remaining fields. */
typedef struct {
    char *buffer;
    size_t capacity, length, start, end, anchor, cursor;
    size_t document_anchor, document_cursor;
    uint64_t revision;
    char *view_buffer;
    size_t view_capacity;
    bool active;
    /* Opt-in model-owned rendered storage; preedit remains caller bounded.
     * Built-in default sessions enable this. Owned view lives until model destroy
     * or its next update; original caller view_buffer remains caller-owned. */
    bool grow_view, view_valid;
    size_t view_length;
    uintptr_t model_identity; /* Engine numeric binding; reset on model destruction/reuse. */
} cui_edit_composition;
/* Source single-line acceptance: empty values are valid; reject a candidate
 * only when its predicate fails and the previous visible value passes. */
typedef bool (*cui_edit_accept_fn)(void *user, cui_str candidate);
typedef struct {
    cui_edit_accept_fn validate;
    void *user;
} cui_edit_acceptance;
/* Opt-in source single-line CR/LF removal; zero preserves raw input. */
typedef enum {
    CUI_EDIT_NORMALIZE_NONE = 0,
    CUI_EDIT_NORMALIZE_SINGLE_LINE_CRLF
} cui_edit_normalization;
/* Pure bounded seed helper. Disjoint source/destination/output storage required.
 * Success appends NUL and borrows destination in normalized; failure writes none. */
cui_edit_result cui_edit_normalize_text(cui_str source, cui_edit_normalization policy,
                                        char *destination, size_t capacity, cui_str *normalized);
/* Pure scalar U+2022 display; source UTF-8 byte endpoints map to display
 * bytes (three per scalar). All outputs are atomic and disjoint from source. */
cui_edit_result cui_edit_mask_text(cui_str source, char *destination, size_t capacity,
                                   cui_str *display);
cui_edit_result cui_edit_mask_offset(cui_str source, size_t source_byte, size_t *display_byte);
cui_edit_result cui_edit_unmask_offset(cui_str source, size_t display_byte, size_t *source_byte);
typedef struct {
    void *data;
    size_t bytes;
} cui_edit_workspace;
/* Hard resource gate before acceptance/commit; returns only OK, CAPACITY or
 * PRESENTATION. Candidate is synchronous borrowed real text, not mask bytes. */
typedef cui_edit_result (*cui_edit_prepare_fn)(void *user, cui_str candidate, bool provisional);
typedef struct {
    cui_edit_prepare_fn prepare;
    void *user;
    size_t user_bytes;
    cui_edit_workspace caller_descriptor;
    cui_edit_workspace caller_workspaces[3];
    size_t caller_workspace_count;
} cui_edit_presentation;
/* Formatting changes stored text; it is independent of password display masks.
 * Tokens: 9 ASCII digit, A ASCII letter, # ASCII alphanumeric, * UTF-8 scalar;
 * all other scalars are literals. Empty pattern behaves as NONE. */
typedef enum {
    CUI_EDIT_FORMAT_NONE = 0,
    CUI_EDIT_FORMAT_PATTERN,
    CUI_EDIT_FORMAT_NUMBER
} cui_edit_format_kind;
typedef struct {
    cui_edit_format_kind kind;
    cui_str pattern;    /* Immutable, borrowed only during a synchronous request. */
    bool has_separator; /* NUMBER: true uses a non-NUL Unicode scalar separator. */
    uint32_t separator;
    bool fraction_limited;
    size_t fraction_digits; /* NUMBER: truncates only when has_separator is true. */
} cui_edit_format;
/* Pure helpers: all input/destination/output extents must be disjoint. Outputs
 * are unchanged on malformed UTF-8, alias or capacity failure. Text helpers
 * append NUL; placeholder is empty for NONE. Validity accepts partial prefixes. */
cui_edit_result cui_edit_format_valid(cui_edit_format, cui_str, bool *valid);
cui_edit_result cui_edit_format_text(cui_edit_format, cui_str, char *destination, size_t capacity,
                                     cui_str *formatted);
cui_edit_result cui_edit_format_placeholder(cui_edit_format, char *destination, size_t capacity,
                                            cui_str *placeholder);
cui_edit_result cui_edit_unformat_text(cui_edit_format, cui_str, char *destination, size_t capacity,
                                       cui_str *unformatted);
typedef enum {
    CUI_EDIT_REQUEST_EVENT,
    CUI_EDIT_REQUEST_INSERT,
    CUI_EDIT_REQUEST_DELETE_SELECTION,
    CUI_EDIT_REQUEST_COMPOSITION,
    CUI_EDIT_REQUEST_PRESENT /* Nonmutating initial/current presentation; see below. */
} cui_edit_request_kind;
/* Pure numeric insertion normalization (fullwidth digits/signs/dot/comma,
 * U+2212 minus and U+3002 dot), then the requested CR/LF policy. Endpoint is
 * an original UTF-8 scalar boundary; mapped output is a normalized byte offset.
 * Disjoint outputs and source; failures leave outputs unchanged. */
cui_edit_result cui_edit_normalize_number_text(cui_str, cui_edit_normalization, char *destination,
                                               size_t capacity, cui_str *normalized);
cui_edit_result cui_edit_number_offset(cui_str, cui_edit_normalization, size_t source_byte,
                                       size_t *normalized_byte);
typedef struct {
    cui_edit_request_kind kind;
    cui_event event;
    cui_str text;
    cui_edit_composition *composition;
    bool editable;
    cui_edit_normalization normalization;
    cui_edit_presentation presentation;
    bool masked_words; /* Checked-call word boundaries become whole-value endpoints. */
    cui_edit_format format;
    cui_edit_workspace format_workspace; /* Final value + NUL; disjoint caller storage. */
    cui_text_match matcher; /* Single-line/single-selection synchronous whole-value gate. */
} cui_edit_request;
typedef struct {
    cui_edit_result error;
    bool rejected;
    bool presentation_prepared, presentation_applied;
} cui_edit_checked_result;
/* No new preview allocation: uses bounded engine/session workspaces. Predicate
 * borrows text synchronously; never retain/write it. Same-model mutators are
 * guarded during callbacks (including void destruction); outer call fails on
 * attempted reentry. Pure model getters remain available. External side effects
 * and direct caller-buffer writes are outside this protection. Multiline models
 * bypass the predicate, matching the source single-line contract. Undo/Redo and
 * composition START/CANCEL bypass the predicate; UPDATE/COMMIT validate provisional text.
 * Rejection preserves logical state and prior view's defined bytes; internal
 * scratch and unused buffer tails are unspecified. */
/* PRESENT prepares current committed text (NULL or inactive composition), or
 * an active model-bound already-materialized composed view. It never invokes
 * acceptance or changes logical/session state; editable is ignored. An inactive
 * previously bound session must still belong to this model. Successful prepare
 * returns prepared/applied=true despite unchanged text; absent prepare is a no-op.
 * Caller must reset sessions before destroying/replacing their model.
 * Presentation is supported for single-line models only. Declared user,
 * descriptor and up to three workspaces must be accurate disjoint extents,
 * outside model/session/payload storage. Unused spans must be zero. Preparation
 * runs once before predicates, including Undo/Redo resource preparation. With a
 * callback, composition START requires empty payload and prepares the empty-preedit
 * selected-range replacement before session mutation; no predicate runs. Zero
 * presentation retains legacy START behavior, and CANCEL never prepares. Caller
 * owns all staged resources: consume/discard before another request. Prepared
 * says prepare returned OK (even if later guard/predicate fails); applied says
 * the checked operation adopted that candidate. Completion is outside engine
 * rollback; no callbacks/foreign pointers are retained after the request. */
/* Nonempty PATTERN and NUMBER are single-line/single-selection only; inline tokens are
 * unsupported. Normalize then validate the unformatted whole candidate (custom
 * predicate before token validity); old-invalid healing is preserved. Committed
 * text is formatted once before mutation, with final resource prepare preceding
 * predicates. UPDATE remains unformatted; replay/PRESENT/START/CANCEL never
 * format. Formatting rewrites use one atomic history entry. Caller workspace and
 * pattern must not overlap any model/session/payload/presentation storage. */
cui_edit_checked_result cui_edit_apply_checked(cui_edit *, cui_edit_request, cui_edit_acceptance);
/* Disabled/read-only/unfocused callers pass editable=false to cancel. Update
 * never changes document/history. Commit is exactly one undo transaction. */
cui_edit_result cui_edit_composition_event(cui_edit *, cui_edit_composition *, cui_event,
                                           bool editable);
/* Compose a renderable document snapshot into caller storage, without mutation.
 * Active preedit replaces [start,end); anchor/cursor are relative preedit bytes. */
cui_str cui_edit_composition_view(const cui_edit *, const cui_edit_composition *);
cui_edit_result cui_edit_composition_text(const cui_edit *, const cui_edit_composition *, char *,
                                          size_t, cui_str *);
typedef struct {
    size_t length, cursor, anchor;
    bool can_undo, can_redo;
    uint64_t revision;
    bool multiline;
} cui_edit_state;
cui_edit_state cui_edit_composition_selection(const cui_edit *, const cui_edit_composition *);
/* Earliest byte that may differ after the most recent document revision.
 * Valid only for that revision; a consumer that skips revisions must rebuild.
 * Undo/Redo and complex indentation conservatively report byte zero. */
typedef struct {
    uint64_t revision;
    size_t first_byte;
    bool valid;
} cui_edit_change;
cui_edit_change cui_edit_get_change(const cui_edit *);
#define CUI_EDIT_MAX_SELECTIONS 64
/* Active selection is index0. IDs are caller-defined and must be unique.
 * Anchor/cursor preserve orientation; offsets remain UTF-8 bytes. */
typedef struct {
    uint64_t id;
    size_t anchor, cursor;
} cui_edit_selection;
/* One caller-owned cursor snapshot per existing document-history slot. */
typedef struct {
    size_t count;
    cui_edit_selection items[CUI_EDIT_MAX_SELECTIONS];
} cui_edit_selection_snapshot;
/* Optional per-ID vertical-motion anchor. x is in the current text metrics;
 * column is the logical grapheme column for metrics changes. */
typedef struct {
    uint64_t id;
    size_t cursor, column;
    float x, font_size;
    cui_text_style style;
    bool valid, upstream;
} cui_edit_column_anchor;
typedef struct {
    size_t count;
    cui_edit_column_anchor items[CUI_EDIT_MAX_SELECTIONS];
} cui_edit_column_snapshot;
/* Existing document slot count; attach does not allocate or resize history. */
size_t cui_edit_selection_history_slots(const cui_edit *);
/* Opt-in multiline selection storage. Arrays must be disjoint from each other
 * and model storage, alive and unchanged by callers until detach. History needs
 * at least selection_history_slots entries (NULL is valid with no history).
 * Attachment preserves document/history and initializes one primary cursor;
 * older history entries restore their existing primary selection only. */
cui_edit_result cui_edit_attach_selections(cui_edit *, cui_edit_selection *storage, size_t capacity,
                                           cui_edit_selection_snapshot *history,
                                           size_t history_slots);
void cui_edit_detach_selections(cui_edit *);
/* Optional caller-owned vertical anchors and parallel Undo storage. Attach
 * after selections; history needs one entry per document-history slot. */
cui_edit_result cui_edit_attach_columns(cui_edit *, cui_edit_column_anchor *storage,
                                        size_t capacity, cui_edit_column_snapshot *history,
                                        size_t history_slots);
void cui_edit_detach_columns(cui_edit *);
const cui_edit_column_anchor *cui_edit_get_columns(const cui_edit *, size_t *count);
cui_edit_result cui_edit_set_columns(cui_edit *, const cui_edit_column_anchor *, size_t count);
const cui_edit_selection *cui_edit_get_selections(const cui_edit *, size_t *count);
/* Validates grapheme endpoints, merges overlapping/adjacent ranges, and preserves
 * active ID/orientation. Failure preserves model and attached arrays. */
cui_edit_result cui_edit_select_many(cui_edit *, const cui_edit_selection *, size_t count);
/* Preserve ordered disjoint row ranges without adjacency/coincidence merging.
 * Endpoints must be grapheme boundaries; IDs unique, first entry active. Selection
 * changes update current history metadata but create no document Undo entry. */
cui_edit_result cui_edit_select_disjoint(cui_edit *, const cui_edit_selection *, size_t count);
/* Repeat the same UTF-8 text across normalized selections, atomically. One
 * document/history step; resulting cursors preserve IDs and active-first order.
 * Candidate cursor endpoints must remain grapheme boundaries. Undo/Redo restore
 * all selections with attached history. Single-selection APIs collapse extras.
 * Inline-token content is excluded while selection storage is attached. */
cui_edit_result cui_edit_replace_selections(cui_edit *, cui_str text);
/* Interactive text input. Adjacent batches with unchanged cursor collection
 * coalesce into one bounded Undo entry (at most 1000 cursor changes). Newlines,
 * other edits, navigation and selection break the typing group. */
cui_edit_result cui_edit_type_selections(cui_edit *, cui_str text);
typedef enum {
    CUI_EDIT_PREVIOUS,
    CUI_EDIT_NEXT,
    CUI_EDIT_LINE_START,
    CUI_EDIT_LINE_END,
    CUI_EDIT_START,
    CUI_EDIT_END,
    CUI_EDIT_WORD_PREVIOUS,
    CUI_EDIT_WORD_NEXT,
    CUI_EDIT_LINE_PREVIOUS,
    CUI_EDIT_LINE_NEXT,
    CUI_EDIT_GRAPHEME_PREVIOUS,
    CUI_EDIT_GRAPHEME_NEXT,
    CUI_EDIT_UNICODE_WORD_PREVIOUS,
    CUI_EDIT_UNICODE_WORD_NEXT
} cui_edit_motion;
/* Read-only replacement preflight; internal scratch may change, model/history
 * do not. Same validation as replace_selections. */
cui_edit_result cui_edit_can_replace_selections(cui_edit *, cui_str);
/* Empty clipboard preserves ranges. LF lines (including final empty line) are
 * distributed in document order only when count matches normalized selections;
 * otherwise whole text repeats. One Undo restores the original collection. */
cui_edit_result cui_edit_paste_selections(cui_edit *, cui_str);
/* Join nonempty ranges in document order with LF and a trailing NUL. Caller
 * buffer/output must be disjoint from all model storage. Both stay untouched on
 * validation/capacity failure; output borrows buffer. Does not change selection. */
cui_edit_result cui_edit_copy_selections(const cui_edit *, char *, size_t, cui_str *output);
size_t cui_edit_selection_capacity(const cui_edit *);
/* Add collapsed cursors, retaining active-first order. Duplicate offsets and
 * points inside existing selections are ignored. New IDs must be unique.
 * Batch capacity/validation errors preserve the full collection/history. */
cui_edit_result cui_edit_add_cursors(cui_edit *, const cui_edit_selection *, size_t count);
/* Multi-cursor movement merges ranges while preserving active ID; document
 * START/END intentionally collapse extras. Visual row movement belongs to view. */
cui_edit_result cui_edit_move_selections(cui_edit *, cui_edit_motion, bool extend);
/* Read-only single-selection motion probe for measured view adapters. Writes
 * output only on success; output must not overlap model/live selection storage. */
cui_edit_result cui_edit_selection_motion(const cui_edit *, cui_edit_selection, cui_edit_motion,
                                          bool extend, cui_edit_selection *output);
/* Expand collapsed cursors over one grapheme/Unicode word, merge overlaps, and
 * delete atomically. Undo restores original cursors rather than expanded ranges. */
cui_edit_result cui_edit_erase_selections(cui_edit *, bool backwards, bool by_word);
/* Interactive grapheme erasure; adjacent batches of the same direction and
 * cursor shape share a bounded Undo step. Word and selected-range deletion
 * remain atomic. */
cui_edit_result cui_edit_erase_selections_interactive(cui_edit *, bool backwards, bool by_word);
/* The application owns the NUL-terminated buffer. Only this model may mutate
   it while attached. Capacity includes NUL; offsets are UTF-8 byte offsets. */
cui_edit *cui_edit_create(cui_edit_config config);
void cui_edit_destroy(cui_edit *edit);
cui_edit_state cui_edit_get_state(const cui_edit *edit);
cui_str cui_edit_get_text(const cui_edit *edit);
bool cui_edit_valid_utf8(cui_str text);
#define CUI_GRAPHEME_UNICODE_VERSION "17.0.0"
/* Default extended grapheme clusters (UAX #29). Length-bounded UTF-8 may contain
 * NUL. Invalid text, non-scalar offsets or offsets beyond length return SIZE_MAX
 * (boundary returns false). Next/previous are strict, clamped at either end.
 * Allocation-free full scans; offsets remain UTF-8 bytes. */
size_t cui_grapheme_next(cui_str text, size_t offset);
size_t cui_grapheme_previous(cui_str text, size_t offset);
bool cui_grapheme_boundary(cui_str text, size_t offset);
/* Forward iterator validates once, then visits clusters in linear total time.
 * Keep the borrowed text unchanged. For already validated text, an aggregate
 * initializer {text, known_grapheme_boundary} may seed traversal without a scan.
 * Other offsets must only be changed by next.
 * Next returns the new byte offset, clamped at length; invalid UTF-8 or an
 * invalid scalar offset encountered during traversal returns SIZE_MAX. */
typedef struct {
    cui_str text;
    size_t offset;
} cui_grapheme_iterator;
bool cui_grapheme_iterator_init(cui_grapheme_iterator *iterator, cui_str text);
size_t cui_grapheme_iterator_next(cui_grapheme_iterator *iterator);
#define CUI_WORD_UNICODE_VERSION "17.0.0"
/* Default UAX #29 word boundaries, including punctuation/whitespace segments.
 * Validation and strict/clamped offset behavior match the grapheme functions. */
size_t cui_word_next(cui_str text, size_t offset);
size_t cui_word_previous(cui_str text, size_t offset);
bool cui_word_boundary(cui_str text, size_t offset);
/* Borrowed atomic inline content for ordinary Input/Textarea models. IDs may
 * repeat. All span/string storage stays immutable and alive until detached. */
typedef struct {
    cui_str id, text, label;
} cui_edit_inline_token;
typedef struct {
    size_t start, end;
    cui_edit_inline_token token;
} cui_edit_token_span;
typedef struct {
    cui_str text;
    const cui_edit_token_span *tokens;
    size_t token_count;
} cui_edit_content;
/* Spans are sorted, nonoverlapping, nonempty grapheme ranges matching token.text.
 * IDs must contain a non-whitespace scalar; text/label reject controls and LS/PS.
 * All strings use the edit model's NUL-free UTF-8 contract. Maximum 65536 spans. */
bool cui_edit_content_valid(cui_edit_content);
/* Pure helpers leave output untouched on rejection. Boundary snaps only strict
 * token interiors; forward chooses end, otherwise start. Ranges expand outward;
 * collapsed insertion ranges snap right. Inputs must be scalar byte offsets;
 * the two range output pointers must differ. */
cui_edit_result cui_edit_token_boundary(cui_edit_content, size_t offset, bool forward, size_t *out);
cui_edit_result cui_edit_normalize_token_range(cui_edit_content, size_t start, size_t end,
                                               size_t *out_start, size_t *out_end);
cui_edit_content cui_edit_get_content(const cui_edit *);
/* Atomic document replacement, then annotation adoption. Capacity/validation
 * failure preserves text, selection, history and previous annotation attachment.
 * Changing annotated content cannot borrow token strings from mutable edit storage. */
cui_edit_result cui_edit_set_content(cui_edit *, cui_edit_content);
/* Annotation-only attachment preserves text/selection/history. Any subsequent
 * document revision (including undo/redo) detaches spans; the host must refresh.
 * No metadata allocation, incremental remapping or token history is supplied. */
/* Atomic borrowed-token insertion/replacement. Original endpoints must be grapheme
 * boundaries; overlapping atoms expand the replacement. Output storage must be
 * disjoint from attached spans, token strings, and model storage, and remains
 * borrowed until detached. Count and output are untouched on failure. Exact
 * token/range no-ops preserve selection/history but adopt the copied output.
 * Undo restores text only and detaches tokens; metadata-only edits add no history. */
cui_edit_result cui_edit_replace_range_with_token(cui_edit *, size_t start, size_t end,
                                                  cui_edit_inline_token,
                                                  cui_edit_token_span *output, size_t capacity,
                                                  size_t *count);
cui_edit_result cui_edit_replace_with_token(cui_edit *, cui_edit_inline_token,
                                            cui_edit_token_span *output, size_t capacity,
                                            size_t *count);
cui_edit_result cui_edit_set_tokens(cui_edit *, const cui_edit_token_span *, size_t count);
/* Token-safe view selection: nonempty ranges expand outward, collapsed cursors
 * snap left. The legacy exact scalar selection function below remains available. */
cui_edit_result cui_edit_select_atomic(cui_edit *, size_t anchor, size_t cursor);
cui_edit_result cui_edit_select(cui_edit *edit, size_t anchor, size_t cursor);
/* Legacy WORD motions skip punctuation/space and move between word starts. Letters,
 * digits, underscores and common Unicode letter/combining ranges form words.
 * UNICODE_WORD motions use Unicode default word segments: previous targets
 * the last non-whitespace segment start in the prefix, next targets the first
 * non-whitespace segment end in the suffix. Targets are grapheme-safe.
 * Vertical motions preserve a scalar column across logical LF-delimited lines;
 * visual wrapping and pixel-column movement belong to the styled textarea. */
cui_edit_result cui_edit_move(cui_edit *edit, cui_edit_motion motion, bool extend);
cui_edit_result cui_edit_insert(cui_edit *edit, cui_str text);
cui_edit_result cui_edit_type(cui_edit *edit, cui_str text);
void cui_edit_break_coalescing(cui_edit *edit);
/* Replace a UTF-8 byte range as one history step without first changing the
 * selection. Attached atomic spans expand overlapping ranges outward; collapsed
 * insertion snaps right. Undo restores the original selection; success collapses
 * it after the inserted text. Rejection leaves text, selection and history unchanged. */
cui_edit_result cui_edit_replace_range(cui_edit *, size_t start, size_t end, cui_str text);
/* Indent selected lines (inclusive of a selection ending at line start), or
 * insert at the collapsed cursor. Outdent removes only an exact line prefix.
 * One successful change is one undo step with both selection endpoints mapped.
 * Attached inline tokens reject indent; detach or use host token-aware editing. */
/* Attached multicursor indent: any noncollapsed range (or block=true) selects
 * whole LF lines, inclusive endpoint row and deduplicated shared rows. Inline
 * inserts at cursors. Outdent removes one exact leading unit per touched row.
 * Remaps every endpoint across all edits; atomic one Undo, no token support. */
cui_edit_result cui_edit_indent_selections(cui_edit *, bool outdent, bool block, size_t tab_size,
                                           bool hard_tabs);
cui_edit_result cui_edit_indent(cui_edit *, bool outdent, size_t tab_size, bool hard_tabs);
cui_edit_result cui_edit_backspace(cui_edit *edit);
cui_edit_result cui_edit_delete(cui_edit *edit);
/* Grapheme erasure expands partial cluster selections outward. At an interior
 * scalar offset it removes the containing cluster. Scalar erasure APIs above
 * retain their original behavior. Undo restores the original byte selection. */
cui_edit_result cui_edit_backspace_grapheme(cui_edit *edit);
cui_edit_result cui_edit_delete_grapheme(cui_edit *edit);
/* Atomic Unicode word erasure. Nonempty selections use grapheme erasure;
 * collapsed cursors delete to the corresponding UNICODE_WORD motion target. */
cui_edit_result cui_edit_backspace_word(cui_edit *edit);
cui_edit_result cui_edit_delete_word(cui_edit *edit);
cui_edit_result cui_edit_undo(cui_edit *edit);
cui_edit_result cui_edit_redo(cui_edit *edit);
/* Apply one event in caller-defined order. Ordinary Left/Right and erasure use
 * graphemes; Ctrl/Alt arrows and erasure use Unicode word segments. Explicit
 * legacy word/vertical motions retain their contracts. Pointer
 * events are ignored. */
cui_edit_result cui_edit_apply_event(cui_edit *, cui_event);
/* Applies events already routed to id; returns the first rejected edit while
   continuing independent later events. Disabled input is ignored. */
cui_edit_result cui_edit_apply_events(cui_edit *edit, cui_context *ui, cui_id id, bool disabled);
#ifdef __cplusplus
}
#endif
#endif
