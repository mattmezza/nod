/* SPDX-License-Identifier: MIT */
#include "cui/cui_description.h"
static unsigned cui_description_columns(unsigned columns) {
    return !columns ? 3 : columns > 10 ? 10 : columns;
}
cui_description_options cui_description_options_default(void) {
    return (cui_description_options){.columns = 3, .label_width = 120};
}
bool cui_description_next_row(const cui_description_item *items, size_t count, unsigned columns,
                              size_t *cursor, cui_description_row *row) {
    if ((!items && count) || !cursor || !row || *cursor >= count)
        return false;
    columns = cui_description_columns(columns);
    size_t first = *cursor;
    *row = (cui_description_row){.first = first};
    if (items[first].separator) {
        row->count = 1;
        row->spans = columns;
        row->separator = true;
        (*cursor)++;
        return true;
    }
    while (*cursor < count && !items[*cursor].separator) {
        unsigned span = items[*cursor].span ? items[*cursor].span : 1;
        if (row->count && (row->spans >= columns || span > columns - row->spans))
            break;
        row->spans += span;
        row->count++;
        (*cursor)++;
    }
    return true;
}
float cui_description_fraction(const cui_description_row *row, unsigned span, unsigned columns) {
    if (!row || !row->count || !row->spans)
        return 0;
    if (row->separator)
        return 1;
    columns = cui_description_columns(columns);
    span = span ? span : 1;
    if (row->spans > columns)
        return (float)span / (float)row->spans;
    return (float)span / columns + (1 - (float)row->spans / columns) / (float)row->count;
}
