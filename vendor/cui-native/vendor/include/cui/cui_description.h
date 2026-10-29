/* SPDX-License-Identifier: MIT */
#ifndef CUI_DESCRIPTION_H
#define CUI_DESCRIPTION_H
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_DESCRIPTION_MEDIUM,
    CUI_DESCRIPTION_SMALL,
    CUI_DESCRIPTION_LARGE,
    CUI_DESCRIPTION_XSMALL
} cui_description_size;
typedef void (*cui_description_render_fn)(cui_context *, const cui_theme *, cui_id, void *);
typedef struct {
    cui_id id;
    cui_str label, value;
    unsigned span;
    bool separator;
    cui_description_render_fn render_label, render_value;
    void *label_user, *value_user;
} cui_description_item;
typedef struct {
    size_t first, count;
    uint64_t spans;
    bool separator;
} cui_description_row;
typedef struct {
    unsigned columns;
    cui_description_size size;
    bool vertical, unbordered;
    float width, label_width;
    bool custom_label_width, label_width_percent;
    cui_str name;
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
} cui_description_options;
cui_description_options cui_description_options_default(void);
/* Columns clamp to 1..10 (zero chooses 3). Separators occupy isolated rows.
 * A zero span behaves as 1; larger spans retain source overflow/shrink behavior. */
bool cui_description_next_row(const cui_description_item *, size_t count, unsigned columns,
                              size_t *cursor, cui_description_row *);
/* CSS flex basis span/columns with equal free-space growth, weighted shrink. */
float cui_description_fraction(const cui_description_row *, unsigned span, unsigned columns);
/* Strings and callbacks are borrowed for this call; no persistent state needed.
 * Width 0 uses the available parent width. Custom callbacks own their semantics. */
bool cui_description_list(cui_context *, cui_id, const cui_theme *, const cui_description_item *,
                          size_t count, cui_description_options);
#ifdef __cplusplus
}
#endif
#endif
