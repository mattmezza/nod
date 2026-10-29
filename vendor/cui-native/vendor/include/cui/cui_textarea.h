/* SPDX-License-Identifier: MIT */
#ifndef CUI_TEXTAREA_H
#define CUI_TEXTAREA_H
#include "cui.h"
#include "cui_edit.h"
#include "cui_platform.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { CUI_TEXTAREA_OK, CUI_TEXTAREA_INVALID, CUI_TEXTAREA_CAPACITY } cui_textarea_error;
typedef enum {
    CUI_TEXTAREA_MEDIUM,
    CUI_TEXTAREA_XSMALL,
    CUI_TEXTAREA_SMALL,
    CUI_TEXTAREA_LARGE,
    CUI_TEXTAREA_CUSTOM
} cui_textarea_size;
typedef struct {
    size_t start, end, next; /* UTF-8 byte range; next skips CR/LF or U+2028. */
    float width;
    bool hard_break;
    size_t paragraph_start, paragraph_end;
    size_t logical_line; /* Zero-based hard-line index; shared by wrapped rows. */
} cui_textarea_line;
/* Zero-based logical lines. Collapsing hides only interior lines, retaining
 * the opening and closing lines. Caller owns this mutable array. */
typedef struct {
    size_t start_line, end_line;
    bool collapsed;
} cui_textarea_fold;
typedef struct {
    cui_textarea_line *lines;
    size_t capacity, count, caret_line;
    cui_rect viewport;
    float scroll_x, scroll_y, maximum_x, maximum_y;
    float font_size, line_height, wrap_width, preferred_x;
    uint64_t revision;
    uint64_t text_metrics_revision;
    size_t cursor, anchor;
    bool dragging, preferred_valid, upstream_affinity, initialized, focused;
    double blink_started;
    cui_textarea_error error;
    cui_text_style text_style; /* Style of measured visual rows. */
    bool layout_wrap;          /* Last successful visual-row layout mode. */
    size_t layout_fold_count, layout_token_count;
    cui_textarea_line *layout_lines;
    size_t layout_capacity;
    size_t widest_row;
    float widest_width;
    cui_textarea_fold *folds;
    size_t fold_count;
    uint64_t fold_revision;
    cui_edit_composition *composition;
    bool composition_owned, layout_composing;
    /* Visual pointer affinity; model identity is compared only, never retained/dereferenced. */
    const cui_edit *layout_model; /* Identity only; rebind invalidates cached rows. */
    const cui_edit *visual_model;
    uint64_t visual_revision;
    size_t visual_caret_byte;
    cui_text_affinity visual_affinity;
    bool visual_affinity_valid;
} cui_textarea_state;
typedef struct {
    cui_textarea_size size;
    float custom_size, width, height; /* Width0 defaults280; negative grows. Height0 uses rows. */
    size_t rows, min_rows,
        max_rows; /* Zero rows defaults1. Auto-grow uses min1/max10 by default. */
    bool auto_grow, no_wrap, no_appearance, no_border;
    bool submit_on_enter; /* Shift-Enter still inserts a newline. */
    bool disabled, readonly, required, invalid, no_tab_stop;
    bool no_indent, hard_tabs; /* Editable text handles Tab; zero tab_size defaults to two. */
    size_t tab_size;
    bool custom_role;
    cui_role role;
    cui_str placeholder;
    const cui_platform *clipboard;
    cui_design_size design_size; /* Styled control only; base uses supplied metrics. */
    cui_text_style text_style;   /* Zero preserves the legacy default font. */
    const cui_scrollbar_style *scrollbar_style;
} cui_textarea_options;
typedef struct {
    float font_size, line_height, padding_x, padding_y;
} cui_textarea_metrics;
typedef struct {
    bool changed, selection_changed, scrolled;
    bool pressed_enter, enter_secondary, enter_shift; /* Last Enter in this frame. */
    cui_edit_result edit_error;
    cui_textarea_error error;
    /* First visible-line host geometry failure; legacy fallback remains painted.
     * UNAVAILABLE also means no factory. Independent from model/row errors. */
    cui_text_layout_status visual_error;
} cui_textarea_result;
/* Caller owns workspace/editor. Initialization allocates nothing; optional host
 * geometry and idle-preedit snapshots are UI-resource owned. Do not share state. */
bool cui_textarea_state_init(cui_textarea_state *, cui_textarea_line *, size_t capacity);
/* Sorted unique starts, in-document bounds, at least one interior line.
 * Invalid attachment preserves state. Text revisions detach stale ranges;
 * selection endpoints inside a fold automatically expand it during layout. */
bool cui_textarea_set_folds(cui_textarea_state *, cui_edit *, cui_textarea_fold *, size_t count);
cui_textarea_options cui_textarea_default(void);
bool cui_textarea_measure(cui_textarea_options, float root_font_size, cui_textarea_metrics *);
cui_id cui_textarea_viewport_id(cui_id);
/* Build measured visual rows; viewport is the editable inner bounds. */
cui_textarea_error cui_textarea_layout(cui_context *, cui_edit *, cui_textarea_state *,
                                       cui_textarea_metrics, float viewport_width, bool wrap);
/* Style-aware row layout; legacy layout above uses the default zero style. */
cui_textarea_error cui_textarea_layout_ex(cui_context *, cui_edit *, cui_textarea_state *,
                                          cui_textarea_metrics, float viewport_width, bool wrap,
                                          cui_text_style);
size_t cui_textarea_hit_test(cui_context *, cui_edit *, cui_textarea_state *, float x, float y);
/* Read-only measured row geometry for retained multi-cursor adapters. Invalid
 * row/state/offset returns SIZE_MAX or NAN; no pointer/selection mutation. */
/* Borrowed descriptor; source bytes need only survive synchronous submission/query. */
cui_text_line_source cui_textarea_line_source(cui_str, cui_textarea_line);
size_t cui_textarea_row_for_offset(const cui_textarea_state *, size_t offset, bool upstream);
size_t cui_textarea_project_offset(const cui_textarea_state *, size_t offset, bool upwards);
size_t cui_textarea_row_offset(cui_context *, cui_edit *, const cui_textarea_state *, size_t row,
                               float x);
float cui_textarea_row_x(cui_context *, cui_edit *, const cui_textarea_state *, size_t row,
                         size_t offset);
/* Optional aggregate CHANGE observers are declared in cui_multiline_events.h.
 * Include that header to use the additive facades; legacy declarations stay independent. */
/* Base uses the state's last viewport and supplied metrics, preserving caller layout. */
cui_textarea_result cui_base_textarea(cui_context *, cui_id, cui_str name, cui_edit *,
                                      cui_textarea_state *, cui_textarea_options,
                                      cui_textarea_metrics);
cui_textarea_result cui_textarea(cui_context *, cui_id, const cui_theme *, cui_str name, cui_edit *,
                                 cui_textarea_state *, cui_textarea_options);
#ifdef __cplusplus
}
#endif
#endif
