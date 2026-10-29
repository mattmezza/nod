/* SPDX-License-Identifier: MIT */
#ifndef CUI_TABLE_H
#define CUI_TABLE_H
#include "cui.h"
#include "cui_icon.h"
#include "cui_scroll.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_id key;
    cui_str label;
    bool disabled;
} cui_table_row;
typedef enum {
    CUI_TABLE_ALIGN_LEFT,
    CUI_TABLE_ALIGN_CENTER,
    CUI_TABLE_ALIGN_RIGHT
} cui_table_align;
typedef struct {
    cui_id key;
    cui_str label;
    float width, minimum, maximum;
    bool sortable, resizable, movable, disabled, fixed_left;
    cui_table_align align;
    bool nonselectable; /* Selection-only opt-out; sorting/resizing/children stay enabled. */
} cui_table_column;
/* Reference defaults; zero-initialized structs remain supported. */
cui_table_column cui_table_column_default(cui_id key, cui_str label);
typedef enum {
    CUI_TABLE_SORT_DEFAULT,
    CUI_TABLE_SORT_ASCENDING,
    CUI_TABLE_SORT_DESCENDING
} cui_table_sort;
typedef enum { CUI_TABLE_SORT_CYCLE_LEGACY, CUI_TABLE_SORT_CYCLE_SOURCE } cui_table_sort_cycle;
typedef enum {
    CUI_TABLE_SELECT_ROW,
    CUI_TABLE_SELECT_COLUMN,
    CUI_TABLE_SELECT_CELL
} cui_table_selection_mode;
typedef enum {
    CUI_TABLE_OK,
    CUI_TABLE_INVALID,
    CUI_TABLE_CAPACITY,
    CUI_TABLE_DUPLICATE_KEY
} cui_table_error;
typedef struct cui_table_model cui_table_model;
cui_table_model *cui_table_create(size_t row_capacity);
void cui_table_destroy(cui_table_model *);
/* Borrowed rows/columns; columns are caller controlled and moved/resized in place.
 * Binding is transactional. Keys must be nonzero, unique within their dimension. */
cui_table_error cui_table_bind(cui_table_model *, const cui_table_row *, size_t rows,
                               cui_table_column *, size_t columns);
size_t cui_table_rows(const cui_table_model *);
size_t cui_table_columns(const cui_table_model *);
size_t cui_table_fixed_columns(const cui_table_model *);
float cui_table_fixed_width(const cui_table_model *);
bool cui_table_get_row(const cui_table_model *, size_t, cui_table_row *);
bool cui_table_get_column(const cui_table_model *, size_t, cui_table_column *);
bool cui_table_row_index(const cui_table_model *, cui_id, size_t *);
bool cui_table_resize(cui_table_model *, cui_id column, float width);
bool cui_table_move_column(cui_table_model *, cui_id column, size_t destination);
typedef struct {
    size_t begin, end;
    float before, after, total, scroll;
} cui_table_view;
cui_table_view cui_table_viewport(const cui_table_model *, float scroll, float height,
                                  float row_height, size_t overscan);
cui_table_view cui_table_column_viewport(const cui_table_model *, float scroll, float width,
                                         size_t overscan);
typedef struct {
    cui_id row, column;
} cui_table_selection;
typedef struct {
    cui_id row, column; /* Controlled selection; zero means none. */
    cui_id cursor_row, cursor_column, header_cursor;
    cui_id sort_column;
    cui_table_sort sort;
    float scroll_x, scroll_y;
    cui_id dragging;
    float drag_x;
    bool drag_active;
    cui_scroll_state scrollbar;
    cui_id resizing;
    float resize_x, resize_width;
    size_t visible_begin, visible_end, column_begin, column_end, previous_row_count;
    bool viewport_initialized, load_pending;
    cui_table_selection *selection;
    size_t selection_count, selection_capacity;
    cui_id anchor_row, anchor_column;
} cui_table_state;
typedef void (*cui_table_header_fn)(cui_context *, const cui_theme *, void *, cui_id column);
typedef void (*cui_table_row_header_fn)(cui_context *, const cui_theme *, void *, size_t row);
typedef void (*cui_table_placeholder_fn)(cui_context *, const cui_theme *, void *);
typedef struct {
    cui_str label;
    size_t span;
} cui_table_column_group;
typedef struct {
    const cui_table_column_group *groups;
    size_t count;
} cui_table_group_row;
typedef void (*cui_table_group_header_fn)(cui_context *, const cui_theme *, void *, size_t level,
                                          size_t group, cui_str label, size_t span, float width);
typedef void (*cui_table_row_fn)(cui_context *, const cui_theme *, void *, size_t row);
typedef void (*cui_table_trailing_fn)(cui_context *, const cui_theme *, void *, bool header,
                                      size_t row);
typedef struct {
    float width, height, row_height, header_height, row_header_width, bar_thickness, trailing_width;
    size_t overscan, column_overscan, load_more_threshold, group_header_count;
    bool disabled, striped, bordered, manual_selection, loop_selection, load_more,
        multiple_selection, row_header, loading, scrollbars;
    cui_table_header_fn render_header;
    cui_table_row_header_fn render_row_header;
    cui_table_group_header_fn render_group_header;
    cui_table_row_fn render_row;
    cui_table_trailing_fn render_trailing;
    cui_table_placeholder_fn render_empty, render_loading;
    const cui_table_group_row *group_headers;
    void *render_userdata;
    cui_table_selection_mode selection_mode;
    const cui_scrollbar_style *scrollbar_style;
    bool row_selection_disabled, column_selection_disabled, cell_selection_disabled;
    cui_table_sort_cycle sort_cycle; /* Zero preserves ascending-first legacy behavior. */
    cui_str accessible_name;
} cui_table_options;
cui_table_options cui_table_options_default(void);
typedef struct {
    bool selection_changed, sort_changed, columns_changed;
    bool visible_rows_changed, visible_columns_changed, load_more, context_requested;
    cui_id context_row, context_column;
    bool selection_mode_requested;
    cui_table_selection_mode requested_selection_mode;
    cui_table_view rows, columns;
    cui_id activated_row, activated_column;
    cui_table_error error;
} cui_table_result;
cui_scroll_geometry cui_table_scroll_layout(const cui_table_model *, cui_table_options);
bool cui_table_group_headers_valid(const cui_table_model *, cui_table_options);
cui_id cui_table_group_header_id(cui_id table, size_t level, size_t group);
cui_id cui_table_trailing_id(cui_id table, bool header, cui_id row);
float cui_table_row_header_width(cui_table_options);
cui_id cui_table_row_header_id(cui_id table, cui_id row);
bool cui_table_is_selected(const cui_table_state *, cui_table_options, cui_id row, cui_id column);
cui_id cui_table_row_id(cui_id table, cui_id row);
cui_id cui_table_cell_id(cui_id table, cui_id row, cui_id column);
cui_id cui_table_header_id(cui_id table, cui_id column);
cui_id cui_table_resize_id(cui_id table, cui_id column);
cui_id cui_table_body_id(cui_id table);
typedef cui_str (*cui_table_label_fn)(void *, size_t row, cui_id column);
typedef void (*cui_table_cell_fn)(cui_context *, const cui_theme *, void *, size_t row,
                                  cui_id column);
/* Additive scalar, column, action/context and exact multi-set observers: include
 * cui_table_events.h. */
cui_table_result cui_table_reduce(cui_context *, cui_id, cui_table_model *, cui_table_state *,
                                  cui_table_options);
void cui_table_submit_root(cui_context *, cui_id, cui_table_model *, cui_table_state *,
                           cui_table_options);
void cui_table_submit_header(cui_context *, cui_id, cui_table_model *, cui_table_state *,
                             cui_table_options);
void cui_table_submit_body(cui_context *, cui_id, cui_table_model *, cui_table_state *,
                           cui_table_options, cui_table_label_fn, void *);
void cui_table_submit_header_column(cui_context *, cui_id, cui_table_model *, cui_table_state *,
                                    cui_table_options, size_t column);
void cui_table_submit_body_root(cui_context *, cui_id, cui_table_model *, cui_table_state *,
                                cui_table_options);
void cui_table_submit_row(cui_context *, cui_id, cui_table_model *, cui_table_state *,
                          cui_table_options, size_t row);
void cui_table_submit_cell(cui_context *, cui_id, cui_table_model *, cui_table_state *,
                           cui_table_options, size_t row, size_t column, cui_table_label_fn,
                           void *);
cui_table_result cui_base_data_table(cui_context *, cui_id, cui_table_model *, cui_table_state *,
                                     cui_table_options);
/* Callback runs only for the virtual window, inside a clipped fixed-size cell.
 * Provide a stable borrowed cell label for accessibility via label callback. */
cui_table_result cui_data_table(cui_context *, cui_id, const cui_theme *, cui_table_model *,
                                cui_table_state *, cui_table_options, cui_table_label_fn,
                                cui_table_cell_fn, void *userdata);
bool cui_table_span_width(const float *widths, size_t count, size_t start, size_t span,
                          float *width);
typedef enum {
    CUI_TABLE_SECTION_HEADER,
    CUI_TABLE_SECTION_BODY,
    CUI_TABLE_SECTION_FOOTER
} cui_table_section;
void cui_table_begin_section(cui_context *, cui_id, const cui_theme *, cui_table_section);
void cui_table_begin_head_cell(cui_context *, cui_id, float width, float height, cui_table_align,
                               cui_str name);
bool cui_table_begin_cell_span(cui_context *, cui_id, const float *widths, size_t count,
                               size_t start, size_t span, float height, cui_table_align);
void cui_table_caption(cui_context *, cui_id, const cui_theme *, cui_str);
/* Stateless composable Table: caller determines section/cell hierarchy and content. */
void cui_table_begin(cui_context *, cui_id, const cui_theme *, float width);
void cui_table_begin_row(cui_context *, cui_id, const cui_theme *, float height, bool header);
typedef struct {
    float height;
    bool header, custom_background;
    cui_color background;
    cui_str name;
    cui_id parent;
    unsigned row_index;          /* One based; zero means unspecified. */
    cui_design_size design_size; /* Height zero uses selected design control height. */
} cui_table_row_options;
/* Set the background independently for each header/body row. Alternating rows
 * are application-controlled: use the model row index, not the visible index. */
void cui_table_begin_row_ex(cui_context *, cui_id, const cui_theme *, cui_table_row_options);
void cui_table_begin_cell(cui_context *, cui_id, float width, float height, cui_table_align);
typedef struct {
    float width;
    cui_str accessible_name;
    unsigned row_count, column_count;
} cui_table_basic_options;
typedef struct {
    float width, height, padding;
    bool custom_padding, header;
    cui_table_align align;
    cui_str name;
    cui_id parent;
    unsigned row_index, column_index; /* One based; zero means unspecified. */
    cui_design_size design_size;
} cui_table_cell_options;
/* Names are copied synchronously into Core frame storage. Invalid inputs create no box or
 * semantic node. End only after a true return; zero size retains legacy FIT/4px
 * padding. Explicit dimensions/padding override the selected design metrics.
 * Metadata must fit INT_MAX. Design padding uses vertical control padding; child
 * typography stays host-owned (use cui_design_measure for font/line height). */
bool cui_table_begin_ex(cui_context *, cui_id, const cui_theme *, cui_table_basic_options);
bool cui_table_begin_cell_ex(cui_context *, cui_id, const cui_theme *, cui_table_cell_options);
/* Checked row submission; legacy void row_ex retains its normalization contract. */
bool cui_table_begin_row_checked(cui_context *, cui_id, const cui_theme *, cui_table_row_options);
void cui_table_end(cui_context *);
#ifdef __cplusplus
}
#endif
#endif
