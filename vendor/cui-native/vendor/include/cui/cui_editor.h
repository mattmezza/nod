/* SPDX-License-Identifier: MIT */
#ifndef CUI_EDITOR_H
#define CUI_EDITOR_H
#include "cui_base.h"
#include "cui_textarea.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Return the exclusive UTF-8 byte end of the next token, within line_end.
 * Invalid boundaries fall back to ordinary text. Never mutate the edit model. */
typedef size_t (*cui_editor_highlight_fn)(void *user, cui_str language, cui_str document,
                                          size_t start, size_t line_end, cui_color *color);
typedef struct {
    cui_str label, detail, insert_text;
} cui_editor_completion_item;
typedef struct {
    const cui_editor_completion_item *items; /* Borrowed until close. */
    size_t count, selected, replace_start, replace_end;
    size_t cursor, anchor; /* Selection captured when the proposal was opened. */
    uint64_t revision;
    bool open;
} cui_editor_completion_state;
typedef struct cui_editor_matcher cui_editor_matcher;
/* Source column identity: byte offset plus rounded space-width columns past
 * a short row's end. Rectangles clip byte columns left to grapheme boundaries. */
typedef struct {
    size_t offset, columns_past_end;
} cui_editor_column_point;
typedef struct {
    uint64_t id;
    size_t anchor, cursor;
    float x;
    bool valid, upstream;
} cui_editor_cursor_anchor;
typedef struct {
    cui_edit *edit; /* Borrowed, must outlive this state. */
    cui_textarea_state view;
    cui_editor_completion_state completion;
    cui_editor_cursor_anchor *cursor_anchors; /* Optional borrowed per-ID visual columns. */
    size_t cursor_anchor_capacity, cursor_anchor_count;
    uint64_t cursor_anchor_revision, next_cursor_id;
    cui_text_style cursor_anchor_style;
    float cursor_anchor_font_size;
    bool multi_pointer, clipboard_active;
    cui_editor_column_point column_start;
    uint64_t column_revision;
    cui_text_style column_style;
    float column_font_size, column_wrap_width;
    float column_pointer_x, column_pointer_y;
    bool column_dragged;
    const cui_edit *gutter_model; /* Identity only; never dereferenced after rebinding. */
    uint64_t gutter_revision;
    size_t gutter_line_count;
    bool gutter_valid;

} cui_editor_state;
typedef struct {
    cui_textarea_options textarea;
    bool line_numbers;
    cui_str language; /* Borrowed for the draw call. */
    cui_editor_highlight_fn highlight;
    void *highlight_user;
    float completion_width;     /* Maximum popup width; zero defaults to 320. */
    size_t completion_rows;     /* Zero defaults to eight; maximum 32 visible rows. */
    cui_editor_matcher *search; /* Optional borrowed retained matcher, synchronized by view. */
    bool custom_search_colors;
    cui_color search_color, search_current_color;
    /* Optional caller scratch for multicursor Copy/Cut, including joined LF/NUL.
     * Disjoint from all model/view resources, borrowed for submission. Clipboard
     * callbacks must not reenter UI/editor or mutate model/resources; returned
     * text stays immutable through the synchronous operation. */
    char *clipboard_scratch;
    size_t clipboard_capacity;
} cui_editor_options;
bool cui_editor_state_init(cui_editor_state *, cui_edit *, cui_textarea_line *, size_t capacity);
/* Required before submitting a model with attached multi-selection storage.
 * Borrowed anchor array must cover selection_capacity(edit), be disjoint from
 * model/live selection/state storage, and remain unchanged by the host. No alloc.
 * Reinitialize to clear preferred columns after host font-provider changes. */
bool cui_editor_multicursor_init(cui_editor_state *, cui_editor_cursor_anchor *, size_t capacity);
/* Uses the current style-aware visual layout (must match model revision).
 * Topmost visible row is active; one fresh-ID range per visible wrapped/folded
 * row, ordered left-to-right. Capacity/validation failure preserves collection,
 * history and ID counter. Selection itself creates no document Undo step. */
cui_edit_result cui_editor_select_rectangle(cui_editor_state *, cui_editor_column_point start,
                                            cui_editor_column_point end);
/* Host supplies validated UTF-8 items and one original document byte range.
 * No copying/filtering/provider calls. Empty count closes the menu. Invalid
 * input preserves both existing menu and document. Item bytes remain immutable
 * and alive until the menu closes. Later document revisions dismiss it. */
cui_edit_result cui_editor_show_completions(cui_editor_state *, const cui_editor_completion_item *,
                                            size_t count, size_t replace_start, size_t replace_end);
void cui_editor_cancel_completion(cui_editor_state *);
cui_id cui_editor_completion_item_id(cui_id editor_id, size_t index);
/* One atomic replace/history step. Failure preserves text/selection/history
 * and leaves the current menu open, except that stale revisions dismiss it. */
cui_edit_result cui_editor_accept_completion(cui_editor_state *);
typedef cui_textarea_fold cui_editor_fold;
/* Attach caller-owned ranges; edits detach them, requiring host refresh. */
bool cui_editor_set_folds(cui_editor_state *, cui_editor_fold *, size_t count);
/* Collapsing moves hidden selection endpoints to the opening line end.
 * Other selections, text, and edit history stay unchanged. */
cui_edit_result cui_editor_set_folded(cui_editor_state *, size_t index, bool collapsed);
cui_editor_options cui_editor_default(void);
/* Literal, case-sensitive UTF-8 search; selects a match, optionally wrapping.
 * Missing/invalid queries preserve selection. Reverse search starts before the
 * selection; forward search starts after it. No query pointer is retained. */
cui_edit_result cui_editor_find(cui_editor_state *, cui_str query, bool reverse, bool wrap);
/* Replace the exact selected literal match. No match returns BOUNDARY unchanged.
 * Successful replacement is one undo step and collapses the selection to its end.
 * Programmatic APIs do not enforce view readonly/disabled options. */
cui_edit_result cui_editor_replace_current(cui_editor_state *, cui_str query, cui_str replacement);
/* Replace all nonoverlapping case-sensitive literal matches in one undo step.
 * Scratch is caller-owned byte storage, disjoint from model storage/query/
 * replacement/count; its contents are unspecified on failure. Count is written
 * only on success. No matches succeeds with zero and preserves state. Success
 * with matches resets selection and scroll to document start. */
cui_edit_result cui_editor_replace_all(cui_editor_state *, cui_str query, cui_str replacement,
                                       char *scratch, size_t capacity, size_t *count);
typedef struct {
    size_t start, end;
} cui_editor_match;
/* Initialize with caller-owned range storage. Query bytes are borrowed, immutable
 * until the next set_query; range storage must not overlap document/query bytes.
 * Keep public fields unchanged except through these functions. */
struct cui_editor_matcher {
    cui_editor_match *matches;
    size_t capacity, count, current;
    cui_str query;
    cui_edit *document;
    uint64_t revision, query_hash;
    bool case_insensitive, active, initialized;
};
bool cui_editor_matcher_init(cui_editor_matcher *, cui_editor_match *, size_t capacity);
/* Empty query is valid and clears matches. ASCII-insensitive matches fold A-Z
 * only, matching the frozen source. Failure preserves session and range storage. */
cui_edit_result cui_editor_matcher_set_query(cui_editor_state *, cui_editor_matcher *,
                                             cui_str query, bool case_insensitive);
cui_edit_result cui_editor_matcher_sync(cui_editor_state *, cui_editor_matcher *);
void cui_editor_matcher_close(cui_editor_matcher *);
/* Next increments current before selecting, previous decrements; both wrap.
 * A fresh nonempty query starts at current0, so Next first selects index1. */
cui_edit_result cui_editor_matcher_navigate(cui_editor_state *, cui_editor_matcher *, bool reverse);
/* Choose first match containing or ending at/after offset, else last match. */
cui_edit_result cui_editor_matcher_anchor(cui_editor_state *, cui_editor_matcher *, size_t offset);
/* Replace retained current match and preserve its logical index after rescanning;
 * replacing the last match wraps index0. Scratch rules match replace_all above.
 * Replacement plus rescan is transactional; count/output unchanged on failure. */
cui_edit_result cui_editor_matcher_replace_current(cui_editor_state *, cui_editor_matcher *,
                                                   cui_str replacement, char *scratch,
                                                   size_t capacity);
cui_edit_result cui_editor_matcher_replace_all(cui_editor_state *, cui_editor_matcher *,
                                               cui_str replacement, char *scratch, size_t capacity,
                                               size_t *count);
typedef struct {
    cui_edit *query; /* Borrowed single-line model. */
    cui_input_behavior_state input;
    float scroll_x;
    bool open;
    /* Optional borrowed replacement UI resources. Keep alive while attached.
     * replace_disabled enforces host readonly policy while retaining navigation. */
    cui_editor_matcher *matcher;
    cui_edit *replacement;
    cui_input_behavior_state replacement_input;
    float replacement_scroll_x;
    char *scratch;
    size_t scratch_capacity;
    bool replace_mode, replace_disabled;
    uint64_t activation_revision;
} cui_editor_search_state;
typedef struct {
    bool query_changed, navigated, closed;
    size_t matches;
    cui_edit_result error;
    bool replacement_changed;
    size_t replaced;
} cui_editor_search_result;
bool cui_editor_search_init(cui_editor_search_state *, cui_edit *query);
/* Retained opener seeds a nonempty document selection into the query, anchors a
 * changed query at the first visible projected row, and advances activation.
 * Failed preflight preserves query/session/open state. Unchanged reopening keeps
 * current index. No matcher preserves legacy query-retention behavior. */
cui_edit_result cui_editor_search_open(cui_editor_state *, cui_editor_search_state *,
                                       bool replace_mode);
/* Enable the optional replacement row and retained ASCII-insensitive matching.
 * Models must be distinct; replacement must be single-line. Storage is borrowed.
 * Ctrl-H / Meta-Shift-F opens replacement mode; Escape from either field closes. */
bool cui_editor_search_enable_replace(cui_editor_search_state *, cui_editor_matcher *,
                                      cui_edit *replacement, char *scratch, size_t capacity);
/* Counts nonoverlapping literal matches; invalid/empty queries return zero. */
size_t cui_editor_match_count(const cui_editor_state *, cui_str query);
/* Draw before the editor to intercept Ctrl/Meta-F on editor_id. Enter and
 * Shift-Enter in the query navigate; Escape closes and returns editor focus.
 * Call every frame even when closed. Width zero defaults to 500 pixels. */
cui_editor_search_result cui_editor_find_bar(cui_context *, cui_id, cui_id editor_id,
                                             const cui_theme *, cui_editor_state *,
                                             cui_editor_search_state *, float width, bool disabled);
/* Additive aggregate CHANGE facades are declared in cui_editor_events.h. */
cui_textarea_result cui_base_editor(cui_context *, cui_id, cui_str name, cui_editor_state *,
                                    cui_editor_options, cui_textarea_metrics);
cui_textarea_result cui_editor(cui_context *, cui_id, const cui_theme *, cui_str name,
                               cui_editor_state *, cui_editor_options);
#ifdef __cplusplus
}
#endif
#endif
