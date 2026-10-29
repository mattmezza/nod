/* SPDX-License-Identifier: MIT */
#ifndef CUI_NUMERIC_H
#define CUI_NUMERIC_H
#include "cui_base.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Strict ASCII decimal/exponent grammar; finite doubles only, no whitespace or locale commas. */
bool cui_number_parse(cui_str text, double *value);
/* Reports required bytes excluding NUL; always emits ASCII '.' regardless of locale. */
size_t cui_number_format(double value, char *output, size_t capacity);
typedef double (*cui_number_step_fn)(void *user, double value, int direction);
typedef enum {
    CUI_NUMBER_MEDIUM,
    CUI_NUMBER_SMALL,
    CUI_NUMBER_LARGE,
    CUI_NUMBER_XSMALL,
    CUI_NUMBER_CUSTOM
} cui_number_size;
typedef struct {
    float height, font_size, line_height, padding_x, padding_y, step_width, icon_size;
} cui_number_metrics;
typedef struct {
    double minimum, maximum, step;
    bool has_minimum, has_maximum, disabled, readonly, allow_exponent;
    cui_number_step_fn step_by;
    void *step_user;
    cui_str prefix, suffix;
    float width;
    cui_number_size size;
    float custom_size, density;
    bool no_appearance, hide_focus_ring;
    cui_str placeholder;
    cui_design_size design_size; /* INHERIT preserves legacy size; styled APIs only. */
} cui_number_options;
bool cui_number_measure(cui_number_options, float root_font_size, cui_number_metrics *);
typedef struct {
    cui_edit *edit; /* Borrowed single-line editor, draft limited to 127 UTF-8 bytes. */
    double last_value;
    bool initialized, was_focused, invalid;
    float scroll_x;
    cui_edit_composition *composition; /* Optional caller session; borrowed during use. */
    bool composition_stale; /* Internal cancellation guard until the next preedit update. */
    bool composition_owned; /* Internal default UI resource; leave false for caller storage. */
} cui_number_state;
typedef struct {
    bool changed, submitted, invalid;
    int step_direction;
    cui_edit_result error;
} cui_number_result;
/* Stable part IDs for custom base-only layouts: Number +/- use 1/2; Pagination
 * first/previous/next/last use 1..4 and numbered items 10+window index;
 * Rating clear uses 1 and stars 10+zero-based index; Stepper items use 1+index. */
cui_id cui_numeric_part_id(cui_id root, unsigned part);
cui_number_result cui_base_number_input(cui_context *, cui_id, cui_str, cui_number_state *,
                                        double *, cui_number_options);
/* Draft updates numeric value when valid; clamps on Enter/blur. Invalid draft remains editable. */
cui_number_result cui_number_input(cui_context *ui, cui_id id, const cui_theme *theme, cui_str name,
                                   cui_number_state *state, double *value,
                                   cui_number_options options);
bool cui_number_step(double *value, int direction, cui_number_options options);
typedef struct {
    size_t page;
    bool ellipsis;
} cui_page_item;
/* One-based pages. radius <= 3; max output count is 11. Out-of-range current is clamped. */
size_t cui_pagination_items(size_t current, size_t total, unsigned radius,
                            cui_page_item output[11]);
bool cui_base_pagination(cui_context *, cui_id, size_t *, size_t total, bool compact,
                         bool disabled);
bool cui_pagination(cui_context *ui, cui_id id, const cui_theme *theme, size_t *page, size_t total,
                    bool compact, bool disabled);
typedef struct {
    unsigned maximum;
    bool half_steps, clearable, disabled, readonly;
    float width;
} cui_rating_options;
bool cui_base_rating(cui_context *, cui_id, cui_str, float *, cui_rating_options);
bool cui_rating(cui_context *ui, cui_id id, const cui_theme *theme, cui_str name, float *value,
                cui_rating_options options);
typedef enum {
    CUI_STEP_PENDING,
    CUI_STEP_ACTIVE,
    CUI_STEP_COMPLETE,
    CUI_STEP_ERROR
} cui_step_status;
typedef struct {
    cui_str title, description;
    cui_step_status status;
    bool disabled;
} cui_step;
/* Controlled zero-based index. Activation changes the index; explicit statuses remain caller-owned.
 */
bool cui_base_stepper(cui_context *, cui_id, const cui_step *, size_t count, size_t *current,
                      bool disabled);
/* The selected index determines the visible Current step. Other items retain explicit
 * complete/error statuses; an inactive ACTIVE status is painted as Pending. */
bool cui_stepper(cui_context *ui, cui_id id, const cui_theme *theme, const cui_step *steps,
                 size_t count, size_t *current, bool vertical, bool disabled);
#ifdef __cplusplus
}
#endif
#endif
