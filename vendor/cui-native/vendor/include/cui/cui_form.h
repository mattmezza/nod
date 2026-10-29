/* SPDX-License-Identifier: MIT */
#ifndef CUI_FORM_H
#define CUI_FORM_H
#include "cui.h"
#include "cui_base.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { CUI_FORM_MEDIUM, CUI_FORM_XSMALL, CUI_FORM_SMALL, CUI_FORM_LARGE } cui_form_size;
typedef enum {
    CUI_FORM_OK,
    CUI_FORM_INVALID,
    CUI_FORM_CAPACITY,
    CUI_FORM_DUPLICATE_ID
} cui_form_error;
typedef void (*cui_form_content_fn)(cui_context *, const cui_theme *, void *user, float width);
typedef struct {
    cui_id id, control;
    cui_str label, description, error;
    bool hidden, required, invalid, disabled, readonly, no_label_indent;
    size_t column_span;           /* Zero defaults to one. */
    int column_start, column_end; /* CSS grid lines: one-based; negatives count from end. */
    cui_align align;
    cui_form_content_fn render_control, render_label, render_description;
    void *user;
} cui_form_field;
typedef struct {
    float width, label_width, label_text_size, gap_x, gap_y;
    size_t columns; /* Zero defaults to one; maximum 64. */
    bool horizontal_labels;
    cui_form_size size;
    cui_str name;
    cui_form_content_fn render_footer;
    void *footer_user;
    cui_design_size design_size; /* INHERIT preserves legacy size; styled APIs only. */
} cui_form_options;
typedef struct {
    size_t row, column, span;
    float width;
    bool visible;
} cui_form_cell;
cui_form_options cui_form_options_default(void);
cui_id cui_form_label_id(cui_id field);
cui_id cui_form_description_id(cui_id field);
cui_form_error cui_form_plan(const cui_form_field *, size_t count, cui_form_options,
                             cui_form_cell *, size_t capacity, size_t *rows);
/* Passive semantics and association only; caller owns values and validation. */
cui_form_error cui_base_form(cui_context *, cui_id, const cui_form_field *, size_t,
                             cui_form_options);
void cui_form_associate_control(cui_context *, const cui_form_field *);
cui_form_error cui_form(cui_context *, cui_id, const cui_theme *, const cui_form_field *, size_t,
                        cui_form_options);
typedef enum {
    CUI_INPUT_GROUP_INLINE_START,
    CUI_INPUT_GROUP_INLINE_END,
    CUI_INPUT_GROUP_BLOCK_START,
    CUI_INPUT_GROUP_BLOCK_END
} cui_input_group_alignment;
typedef enum {
    CUI_INPUT_GROUP_TEXT,
    CUI_INPUT_GROUP_BUTTON,
    CUI_INPUT_GROUP_CUSTOM
} cui_input_group_kind;
typedef enum {
    CUI_INPUT_GROUP_GHOST,
    CUI_INPUT_GROUP_PRIMARY,
    CUI_INPUT_GROUP_SECONDARY,
    CUI_INPUT_GROUP_OUTLINE,
    CUI_INPUT_GROUP_DANGER
} cui_input_group_variant;
typedef void (*cui_input_group_content_fn)(cui_context *, const cui_theme *, void *, bool disabled,
                                           bool readonly, float width);
typedef struct {
    cui_id id;
    cui_input_group_alignment alignment;
    cui_input_group_kind kind;
    cui_input_group_variant variant;
    cui_str label;
    float width;
    bool disabled, loading;
    cui_input_group_content_fn render;
    void *user;
} cui_input_group_addon;
typedef struct {
    cui_input_behavior_state behavior;
    float scroll_x;
    cui_rect viewport;
} cui_input_group_state;
typedef struct {
    float width, font_size; /* Zero chooses styled theme defaults. */
    cui_form_size size;
    cui_str name, placeholder;
    cui_base_input_options input;
    cui_design_size design_size; /* INHERIT preserves legacy size; styled APIs only. */
} cui_input_group_options;
typedef struct {
    cui_input_result input;
    cui_id activated_addon;
    cui_form_error error;
} cui_input_group_result;
cui_input_group_result cui_base_input_group(cui_context *, cui_id group, cui_id input, cui_edit *,
                                            cui_input_group_state *, const cui_input_group_addon *,
                                            size_t, cui_input_group_options);
cui_input_group_result cui_input_group(cui_context *, cui_id group, cui_id input, const cui_theme *,
                                       cui_edit *, cui_input_group_state *,
                                       const cui_input_group_addon *, size_t,
                                       cui_input_group_options);
#ifdef __cplusplus
}
#endif
#endif
