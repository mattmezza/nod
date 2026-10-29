/* SPDX-License-Identifier: MIT */
#include "cui/cui_table.h"
#include "cui/cui_icon.h"
#include "cui/cui_table_events.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
static cui_id style_id(cui_id id, unsigned part) {
    return id ^ (UINT64_C(0x09153d4ea892181d) + part * UINT64_C(0x9e3779b97f4a7c15));
}
static cui_align align(cui_table_align a) {
    return a == CUI_TABLE_ALIGN_RIGHT    ? CUI_ALIGN_END
           : a == CUI_TABLE_ALIGN_CENTER ? CUI_ALIGN_CENTER
                                         : CUI_ALIGN_START;
}
static bool table_dimension_valid(float value) {
    return isfinite(value) && value >= 0 && value <= 65536;
}
static bool table_name_valid(cui_str name) {
    return !name.length || name.data;
}
bool cui_table_begin_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_table_basic_options o) {
    if (!ui || !t || !id || !table_dimension_valid(o.width) ||
        !table_name_valid(o.accessible_name) || o.row_count > INT_MAX || o.column_count > INT_MAX)
        return false;
    cui_interact(ui, id, CUI_ROLE_TABLE,
                 o.accessible_name.length ? o.accessible_name : cui_string("Table"), false, false,
                 0);
    cui_set_tab_stop(ui, id, false);
    cui_set_semantic_details(
        ui, id, (cui_semantic_details){.row_count = o.row_count, .column_count = o.column_count});
    cui_begin_box(ui, id,
                  (cui_box){.width = o.width,
                            .background = t->surface,
                            .border = t->border,
                            .border_width = 1,
                            .radius = t->radius});
    return true;
}
bool cui_table_begin_cell_ex(cui_context *ui, cui_id id, const cui_theme *t,
                             cui_table_cell_options o) {
    if (!ui || !t || !id || !table_dimension_valid(o.width) || !table_dimension_valid(o.height) ||
        !isfinite(o.padding) || o.padding < 0 || o.padding > 4096 || !table_name_valid(o.name) ||
        o.align < CUI_TABLE_ALIGN_LEFT || o.align > CUI_TABLE_ALIGN_RIGHT ||
        o.design_size < CUI_DESIGN_INHERIT || o.design_size > CUI_DESIGN_9XL ||
        o.row_index > INT_MAX || o.column_index > INT_MAX || o.parent == id)
        return false;
    float padding = o.custom_padding ? o.padding : 4;
    if (o.design_size != CUI_DESIGN_INHERIT) {
        cui_design_metrics metrics;
        if (!cui_design_measure(o.design_size, t->font_size / 16, &metrics))
            return false;
        if (!o.height)
            o.height = metrics.control_height;
        if (!o.custom_padding)
            padding = metrics.padding_y;
    }
    cui_interact(ui, id, o.header ? CUI_ROLE_COLUMNHEADER : CUI_ROLE_CELL,
                 o.name.length ? o.name : cui_string(o.header ? "Column" : "Cell"), false, false,
                 0);
    cui_set_tab_stop(ui, id, false);
    cui_set_semantic_details(ui, id,
                             (cui_semantic_details){.parent = o.parent,
                                                    .row_index = o.row_index,
                                                    .column_index = o.column_index,
                                                    .readonly = o.header});
    cui_begin_box(ui, id,
                  (cui_box){.width = o.width,
                            .height = o.height,
                            .padding = padding,
                            .clip = true,
                            .align_x = align(o.align),
                            .align_y = CUI_ALIGN_CENTER});
    return true;
}
void cui_table_begin(cui_context *ui, cui_id id, const cui_theme *t, float width) {
    if (!ui || !t)
        return;
    cui_interact(ui, id, CUI_ROLE_TABLE, cui_string("Table"), false, false, 0);
    cui_set_tab_stop(ui, id, false);
    cui_begin_box(ui, id,
                  (cui_box){.width = width,
                            .background = t->surface,
                            .border = t->border,
                            .border_width = 1,
                            .radius = t->radius});
}
void cui_table_begin_row(cui_context *ui, cui_id id, const cui_theme *t, float height,
                         bool header) {
    cui_table_begin_row_ex(ui, id, t, (cui_table_row_options){.height = height, .header = header});
}
void cui_table_begin_row_ex(cui_context *ui, cui_id id, const cui_theme *t,
                            cui_table_row_options o) {
    if (!ui || !t)
        return;
    cui_interact(ui, id, CUI_ROLE_ROW,
                 o.name.length && o.name.data ? o.name : cui_string(o.header ? "Header" : "Row"),
                 false, false, 0);
    cui_set_tab_stop(ui, id, false);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.parent = o.parent,
                               .row_index = o.row_index <= INT_MAX ? o.row_index : 0});
    if (!o.height && o.design_size > CUI_DESIGN_INHERIT && o.design_size <= CUI_DESIGN_9XL) {
        cui_design_metrics metrics;
        if (cui_design_measure(o.design_size, t->font_size / 16, &metrics))
            o.height = metrics.control_height;
    }
    cui_begin_box(ui, id,
                  (cui_box){.height = o.height,
                            .row = true,
                            .background = o.custom_background ? o.background
                                          : o.header          ? t->background
                                                              : t->surface});
}
bool cui_table_begin_row_checked(cui_context *ui, cui_id id, const cui_theme *t,
                                 cui_table_row_options o) {
    if (!ui || !t || !id || !table_dimension_valid(o.height) || !table_name_valid(o.name) ||
        o.row_index > INT_MAX || o.design_size < CUI_DESIGN_INHERIT ||
        o.design_size > CUI_DESIGN_9XL || o.parent == id)
        return false;
    if (o.design_size != CUI_DESIGN_INHERIT) {
        cui_design_metrics metrics;
        if (!cui_design_measure(o.design_size, t->font_size / 16, &metrics))
            return false;
    }
    cui_table_begin_row_ex(ui, id, t, o);
    return true;
}
void cui_table_begin_cell(cui_context *ui, cui_id id, float width, float height,
                          cui_table_align a) {
    cui_interact(ui, id, CUI_ROLE_CELL, cui_string("Cell"), false, false, 0);
    cui_set_tab_stop(ui, id, false);
    cui_begin_box(ui, id,
                  (cui_box){.width = width,
                            .height = height,
                            .padding = 4,
                            .clip = true,
                            .align_x = align(a),
                            .align_y = CUI_ALIGN_CENTER});
}
void cui_table_end(cui_context *ui) {
    cui_end_box(ui);
}
void cui_table_begin_section(cui_context *ui, cui_id id, const cui_theme *t,
                             cui_table_section section) {
    if (!ui || !t)
        return;
    cui_interact(ui, id, CUI_ROLE_ROWGROUP,
                 cui_string(section == CUI_TABLE_SECTION_HEADER   ? "Table header"
                            : section == CUI_TABLE_SECTION_FOOTER ? "Table footer"
                                                                  : "Table body"),
                 false, false, 0);
    cui_set_tab_stop(ui, id, false);
    cui_begin_box(
        ui, id,
        (cui_box){.background = section == CUI_TABLE_SECTION_HEADER ? t->background : t->surface,
                  .border = t->border,
                  .border_width = section == CUI_TABLE_SECTION_FOOTER ? 1 : 0});
}
void cui_table_begin_head_cell(cui_context *ui, cui_id id, float width, float height,
                               cui_table_align a, cui_str name) {
    cui_interact(ui, id, CUI_ROLE_COLUMNHEADER, name, false, false, 0);
    cui_set_tab_stop(ui, id, false);
    cui_set_semantic_details(ui, id, (cui_semantic_details){.readonly = true});
    cui_begin_box(ui, id,
                  (cui_box){.width = width,
                            .height = height,
                            .padding = 4,
                            .clip = true,
                            .align_x = align(a),
                            .align_y = CUI_ALIGN_CENTER});
}
bool cui_table_begin_cell_span(cui_context *ui, cui_id id, const float *widths, size_t count,
                               size_t start, size_t span, float height, cui_table_align a) {
    float width;
    if (!ui || !cui_table_span_width(widths, count, start, span, &width))
        return false;
    cui_table_begin_cell(ui, id, width, height, a);
    return true;
}
void cui_table_caption(cui_context *ui, cui_id id, const cui_theme *t, cui_str text) {
    if (!ui || !t)
        return;
    cui_interact(ui, id, CUI_ROLE_LABEL, text, false, false, 0);
    cui_set_tab_stop(ui, id, false);
    cui_begin_box(ui, id, (cui_box){.height = t->font_size + 12, .padding = 4});
    cui_text(ui, text, t->font_size, t->muted);
    cui_end_box(ui);
}
static void header_cell(cui_context *ui, cui_id id, const cui_theme *t, cui_table_model *m,
                        cui_table_state *s, cui_table_options o, size_t c, float header) {
    cui_table_column col;
    if (!cui_table_get_column(m, c, &col))
        return;
    cui_id cid = cui_table_header_id(id, col.key);
    cui_begin_box(ui, cid,
                  (cui_box){.width = col.width,
                            .height = header,
                            .row = true,
                            .padding = 4,
                            .gap = 2,
                            .clip = true,
                            .background = t->background,
                            .border = t->focus,
                            .border_width = cui_focused(ui) == cid ? 2 : 0});
    cui_table_submit_header_column(ui, id, m, s, o, c);
    float grip = col.resizable ? 8 : 0;
    cui_begin_box(ui, style_id(cid, 1),
                  (cui_box){.width = fmaxf(1, col.width - 8 - grip),
                            .height = fmaxf(1, header - 8),
                            .row = true,
                            .gap = 4,
                            .align_x = align(col.align),
                            .align_y = CUI_ALIGN_CENTER});
    cui_begin_box(ui, style_id(cid, 2),
                  (cui_box){.width = -1,
                            .height = -1,
                            .clip = true,
                            .align_x = align(col.align),
                            .align_y = CUI_ALIGN_CENTER});
    if (o.render_header)
        o.render_header(ui, t, o.render_userdata, col.key);
    else
        cui_text(ui, col.label, t->font_size, col.disabled || o.disabled ? t->muted : t->text);
    cui_end_box(ui);
    if (col.sortable) {
        bool sorted = s->sort_column == col.key && s->sort != CUI_TABLE_SORT_DEFAULT;
        cui_str icon = cui_string(!sorted                               ? "chevrons-up-down"
                                  : s->sort == CUI_TABLE_SORT_ASCENDING ? "arrow-up"
                                                                        : "arrow-down");
        cui_icon_named(ui, style_id(cid, 3), t, icon,
                       (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                          .custom_size = fmaxf(1, fminf(header - 8, t->font_size)),
                                          .custom_color = true,
                                          .color = col.disabled || o.disabled ? t->muted
                                                   : sorted                   ? t->primary
                                                                              : t->muted});
    }
    cui_end_box(ui);
    if (col.resizable) {
        cui_begin_box(ui, cui_table_resize_id(id, col.key),
                      (cui_box){.width = 8,
                                .height = fmaxf(1, header - 8),
                                .background = cui_focused(ui) == cui_table_resize_id(id, col.key)
                                                  ? t->focus
                                                  : t->border});
        cui_end_box(ui);
    }
    cui_end_box(ui);
}
static cui_str cached_cell_label(void *user, size_t row, cui_id column) {
    (void)row;
    (void)column;
    return *(const cui_str *)user;
}
static void body_cell(cui_context *ui, cui_id id, const cui_theme *t, cui_table_model *m,
                      cui_table_state *s, cui_table_options o, size_t r, size_t c, float row,
                      cui_table_label_fn label, cui_table_cell_fn render, void *data) {
    cui_table_row rr;
    cui_table_column col;
    if (!cui_table_get_row(m, r, &rr))
        return;
    if (!cui_table_get_column(m, c, &col))
        return;
    cui_id cid = cui_table_cell_id(id, rr.key, col.key);
    bool selected = (o.selection_mode == CUI_TABLE_SELECT_ROW || !col.nonselectable) &&
                    cui_table_is_selected(s, o, rr.key, col.key);
    cui_begin_box(ui, cid,
                  (cui_box){.width = col.width,
                            .height = row,
                            .padding = 4,
                            .clip = true,
                            .align_x = align(col.align),
                            .align_y = CUI_ALIGN_CENTER,
                            .background = selected               ? t->primary
                                          : o.striped && (r & 1) ? t->background
                                                                 : t->surface,
                            .border = cui_focused(ui) == cid ? t->focus : t->border,
                            .border_width = cui_focused(ui) == cid ? 2
                                            : o.bordered           ? 1
                                                                   : 0});
    cui_str name = label ? label(data, r, col.key) : rr.label;
    cui_table_submit_cell(ui, id, m, s, o, r, c, cached_cell_label, &name);
    if (render)
        render(ui, t, data, r, col.key);
    else
        cui_text(ui, name, t->font_size,
                 o.disabled || rr.disabled || col.disabled ? t->muted
                 : selected                                ? t->on_primary
                                                           : t->text);
    cui_end_box(ui);
}
static float component_trailing_width(cui_table_options o) {
    if (!o.render_trailing)
        return 0;
    return isfinite(o.trailing_width) && o.trailing_width > 0 ? fminf(o.trailing_width, 4096) : 12;
}
static float group_width(cui_table_model *m, size_t start, size_t span) {
    float width = 0;
    cui_table_column column;
    for (size_t c = start; c < start + span; c++) {
        if (!cui_table_get_column(m, c, &column))
            return 0;
        width += column.width;
    }
    return width;
}
static void group_cell(cui_context *ui, cui_id id, const cui_theme *t, cui_table_options o,
                       size_t level, size_t group, size_t column, cui_table_column_group spec,
                       float width) {
    cui_id gid = cui_table_group_header_id(id, level, group);
    cui_interact(ui, gid, CUI_ROLE_COLUMNHEADER, spec.label, o.disabled || o.loading, false, 0);
    cui_set_tab_stop(ui, gid, false);
    cui_set_semantic_details(ui, gid,
                             (cui_semantic_details){.parent = id,
                                                    .readonly = true,
                                                    .column_index = (unsigned)column + 1});
    cui_begin_box(
        ui, gid,
        (cui_box){.width = width,
                  .height = isfinite(o.header_height) && o.header_height > 0 ? o.header_height : 32,
                  .padding = 4,
                  .clip = true,
                  .align_x = CUI_ALIGN_CENTER,
                  .align_y = CUI_ALIGN_CENTER,
                  .background = t->background,
                  .border = t->border,
                  .border_width = 1});
    if (o.render_group_header)
        o.render_group_header(ui, t, o.render_userdata, level, group, spec.label, spec.span, width);
    else
        cui_text(ui, spec.label, t->font_size, o.disabled ? t->muted : t->text);
    cui_end_box(ui);
}
static void trailing_cell(cui_context *ui, cui_id id, const cui_theme *t, cui_table_options o,
                          bool header, size_t row, cui_id row_key, float height) {
    float width = component_trailing_width(o);
    if (width <= 0)
        return;
    cui_id tid = cui_table_trailing_id(id, header, row_key);
    cui_begin_box(ui, tid,
                  (cui_box){.width = width,
                            .height = height,
                            .clip = true,
                            .background = header ? t->background : t->surface});
    o.render_trailing(ui, t, o.render_userdata, header, row);
    cui_end_box(ui);
}
static void group_header_row(cui_context *ui, cui_id id, const cui_theme *t, cui_table_model *m,
                             cui_table_state *s, cui_table_options o, size_t level, float width,
                             float header, float row_header, size_t fixed, float pin,
                             float available) {
    const cui_table_group_row *groups = o.group_headers + level;
    cui_begin_box(ui, style_id(id, 20 + (unsigned)level),
                  (cui_box){.width = width,
                            .height = header,
                            .row = true,
                            .clip = true,
                            .background = t->background});
    if (row_header) {
        cui_begin_box(ui, 0, (cui_box){.width = row_header, .height = header});
        cui_end_box(ui);
    }
    for (unsigned pane = 0; pane < 2; pane++) {
        size_t pane_begin = pane ? fixed : 0, pane_end = pane ? cui_table_columns(m) : fixed;
        if (pane_begin == pane_end)
            continue;
        float pane_width = pane ? available : pin;
        cui_begin_box(ui, style_id(id, 40 + (unsigned)level * 2 + pane),
                      (cui_box){.width = pane_width,
                                .height = header,
                                .clip = true,
                                .offset_x = pane ? -s->scroll_x : 0});
        cui_begin_box(ui, 0, (cui_box){.height = header, .row = true});
        size_t column = 0;
        for (size_t group = 0; group < groups->count; group++) {
            cui_table_column_group spec = groups->groups[group];
            if (column >= pane_begin && column + spec.span <= pane_end)
                group_cell(ui, id, t, o, level, group, column, spec,
                           group_width(m, column, spec.span));
            column += spec.span;
        }
        if (pane && pane_end == cui_table_columns(m))
            trailing_cell(ui, id, t, o, true, SIZE_MAX, (cui_id)level + 1, header);
        cui_end_box(ui);
        cui_end_box(ui);
    }
    cui_end_box(ui);
}
static cui_table_result cui_data_table_common(
    cui_context *ui, cui_id id, const cui_theme *t, cui_table_model *m, cui_table_state *s,
    cui_table_options o, cui_table_label_fn label, cui_table_cell_fn render, void *data,
    const cui_table_event_handler *handler, cui_table_event_result *events,
    const cui_table_set_event_handler *set_handler, cui_table_selection_workspace workspace,
    cui_table_set_event_result *set_events) {
    if (!ui || !t || !m || !s)
        return (cui_table_result){.error = CUI_TABLE_INVALID};
    if (!o.scrollbar_style)
        o.scrollbar_style = &t->scrollbar;
    if (o.scrollbar_style->visibility == CUI_SCROLLBAR_HIDDEN)
        o.scrollbars = false;
    else if (o.scrollbar_style->visibility == CUI_SCROLLBAR_VISIBLE ||
             o.scrollbar_style->visibility == CUI_SCROLLBAR_AUTO_HIDE)
        o.scrollbars = true;
    cui_table_result result;
    if (set_handler) {
        *set_events = cui_table_reduce_set_events(ui, id, m, s, o, workspace, *set_handler);
        result = set_events->events.control;
    } else if (handler) {
        *events = cui_table_reduce_events(ui, id, m, s, o, *handler);
        result = events->control;
    } else
        result = cui_table_reduce(ui, id, m, s, o);
    if (result.error != CUI_TABLE_OK && result.error != CUI_TABLE_CAPACITY)
        return result;
    cui_table_options original = o;
    cui_scroll_geometry geometry = cui_table_scroll_layout(m, o);
    if (o.scrollbars) {
        o.width = geometry.viewport.width;
        o.height = geometry.viewport.height;
        o.scrollbars = false;
    }
    float width = isfinite(o.width) && o.width > 0 ? o.width : 480,
          h = isfinite(o.height) && o.height > 0 ? o.height : 240;
    float row = isfinite(o.row_height) && o.row_height > 0 ? fminf(o.row_height, 4096) : 32,
          header = isfinite(o.header_height) && o.header_height > 0 ? o.header_height : 32;
    size_t fixed = cui_table_fixed_columns(m);
    float pin = cui_table_fixed_width(m), row_header = cui_table_row_header_width(o),
          available = fmaxf(1, width - pin - row_header);
    bool groups_valid = cui_table_group_headers_valid(m, o);
    cui_table_view cols = cui_table_column_viewport(m, s->scroll_x, width - row_header,
                                                    o.column_overscan),
                   rows = cui_table_viewport(m, s->scroll_y, h, row, o.overscan);
    float span = cols.total - cols.before - cols.after;
    cui_begin_box(
        ui, id,
        (cui_box){.width =
                      original.scrollbars
                          ? (isfinite(original.width) && original.width > 0 ? original.width : 480)
                          : width,
                  .background = t->surface,
                  .border = t->border,
                  .border_width = 1,
                  .radius = t->radius});
    cui_table_submit_root(ui, id, m, s, o);
    if (groups_valid)
        for (size_t level = 0; level < o.group_header_count; level++)
            group_header_row(ui, id, t, m, s, o, level, width, header, row_header, fixed, pin,
                             available);
    cui_begin_box(ui, style_id(id, 1),
                  (cui_box){.width = width,
                            .height = header,
                            .row = true,
                            .clip = true,
                            .background = t->background});
    if (row_header) {
        cui_begin_box(
            ui, style_id(id, 5),
            (cui_box){.width = row_header, .height = header, .background = t->background});
        cui_end_box(ui);
    }
    if (fixed) {
        cui_begin_box(ui, style_id(id, 3),
                      (cui_box){.width = pin, .height = header, .row = true, .clip = true});
        for (size_t c = 0; c < fixed; c++)
            header_cell(ui, id, t, m, s, o, c, header);
        cui_end_box(ui);
    }
    cui_begin_box(ui, style_id(id, 4),
                  (cui_box){.width = available,
                            .height = header,
                            .clip = true,
                            .offset_x = cols.before - s->scroll_x});
    cui_begin_box(ui, style_id(id, 2), (cui_box){.width = span, .height = header, .row = true});
    for (size_t c = cols.begin; c < cols.end; c++)
        header_cell(ui, id, t, m, s, o, c, header);
    if (cols.end == cui_table_columns(m))
        trailing_cell(ui, id, t, o, true, SIZE_MAX, 0, header);
    cui_end_box(ui);
    cui_end_box(ui);
    cui_end_box(ui);
    if (original.scrollbars)
        cui_begin_box(ui, style_id(id, 6),
                      (cui_box){.width = isfinite(original.width) && original.width > 0
                                             ? original.width
                                             : 480,
                                .height = h,
                                .row = true});
    cui_begin_box(ui, cui_table_body_id(id),
                  (cui_box){.width = width,
                            .height = h,
                            .clip = true,
                            .offset_y = o.loading ? 0 : rows.before - rows.scroll,
                            .background = t->surface});
    cui_table_submit_body_root(ui, id, m, s, o);
    if (o.loading || !cui_table_rows(m)) {
        cui_id placeholder = cui_table_body_id(id) ^ UINT64_C(0x790416281c39bdd1);
        cui_begin_box(ui, placeholder,
                      (cui_box){.width = width,
                                .height = h,
                                .align_x = CUI_ALIGN_CENTER,
                                .align_y = CUI_ALIGN_CENTER});
        cui_interact(ui, placeholder, CUI_ROLE_STATUS,
                     cui_string(o.loading ? "Loading rows" : "No rows"), false, false, 0);
        cui_set_tab_stop(ui, placeholder, false);
        cui_set_semantic_details(
            ui, placeholder,
            (cui_semantic_details){.parent = cui_table_body_id(id), .busy = o.loading});
        cui_table_placeholder_fn placeholder_render = o.loading ? o.render_loading : o.render_empty;
        if (placeholder_render)
            placeholder_render(ui, t, o.render_userdata);
        else
            cui_text(ui, cui_string(o.loading ? "Loading rows" : "No rows"), t->font_size,
                     t->muted);
        cui_end_box(ui);
    }
    for (size_t r = rows.begin; r < rows.end && !o.loading; r++) {
        cui_table_row rr;
        if (!cui_table_get_row(m, r, &rr))
            break;
        cui_id rid = cui_table_row_id(id, rr.key);
        cui_begin_box(ui, rid, (cui_box){.width = width, .height = row, .row = true});
        cui_table_submit_row(ui, id, m, s, o, r);
        if (o.render_row) {
            cui_begin_box(ui, style_id(rid, 8), (cui_box){.width = 0, .height = row});
            o.render_row(ui, t, o.render_userdata, r);
            cui_end_box(ui);
        }
        if (row_header) {
            cui_id row_id = cui_table_row_header_id(id, rr.key);
            cui_begin_box(ui, row_id,
                          (cui_box){.width = row_header,
                                    .height = row,
                                    .padding = 4,
                                    .clip = true,
                                    .background = t->background,
                                    .border = t->focus,
                                    .border_width = cui_focused(ui) == row_id ? 2 : 0});
            char number[32];
            snprintf(number, sizeof(number), "%zu", r + 1);
            if (o.render_row_header)
                o.render_row_header(ui, t, o.render_userdata, r);
            else
                cui_text(ui, cui_string(number), t->font_size,
                         o.disabled || rr.disabled ? t->muted : t->text);
            cui_end_box(ui);
        }
        if (fixed) {
            cui_begin_box(ui, style_id(rid, 3),
                          (cui_box){.width = pin, .height = row, .row = true, .clip = true});
            for (size_t c = 0; c < fixed; c++)
                body_cell(ui, id, t, m, s, o, r, c, row, label, render, data);
            cui_end_box(ui);
        }
        cui_begin_box(ui, style_id(rid, 4),
                      (cui_box){.width = available,
                                .height = row,
                                .clip = true,
                                .offset_x = cols.before - s->scroll_x});
        cui_begin_box(ui, style_id(rid, 2), (cui_box){.width = span, .height = row, .row = true});
        for (size_t c = cols.begin; c < cols.end; c++)
            body_cell(ui, id, t, m, s, o, r, c, row, label, render, data);
        if (cols.end == cui_table_columns(m))
            trailing_cell(ui, id, t, o, false, r, rr.key, row);
        cui_end_box(ui);
        cui_end_box(ui);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    if (original.scrollbars) {
        cui_scroll_options bars = {
            .width = isfinite(original.width) && original.width > 0 ? original.width : 480,
            .height = isfinite(original.height) && original.height > 0 ? original.height : 240,
            .bar_thickness = original.bar_thickness,
            .scrollbar_style = original.scrollbar_style,
            .disabled = o.disabled || o.loading};
        if (geometry.maximum_y > 0)
            cui_scrollbar(ui, id, t, &s->scrollbar, geometry, bars, true);
        cui_end_box(ui);
        if (geometry.maximum_x > 0)
            cui_scrollbar(ui, id, t, &s->scrollbar, geometry, bars, false);
    }
    cui_end_box(ui);
    return result;
}

cui_table_result cui_data_table(cui_context *ui, cui_id id, const cui_theme *theme,
                                cui_table_model *model, cui_table_state *state,
                                cui_table_options options, cui_table_label_fn label,
                                cui_table_cell_fn render, void *user) {
    return cui_data_table_common(ui, id, theme, model, state, options, label, render, user, NULL,
                                 NULL, NULL, (cui_table_selection_workspace){0}, NULL);
}
cui_table_event_result cui_data_table_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                             cui_table_model *model, cui_table_state *state,
                                             cui_table_options options, cui_table_label_fn label,
                                             cui_table_cell_fn render, void *user,
                                             cui_table_event_handler handler) {
    cui_table_event_result result = {0};
    result.control =
        cui_data_table_common(ui, id, theme, model, state, options, label, render, user, &handler,
                              &result, NULL, (cui_table_selection_workspace){0}, NULL);
    return result;
}

cui_table_set_event_result
cui_data_table_set_events(cui_context *ui, cui_id id, const cui_theme *theme,
                          cui_table_model *model, cui_table_state *state, cui_table_options options,
                          cui_table_label_fn label, cui_table_cell_fn render, void *user,
                          cui_table_selection_workspace workspace,
                          cui_table_set_event_handler handler) {
    cui_table_set_event_result result = {0};
    result.preflight = CUI_TABLE_SET_PREFLIGHT_INVALID;
    result.events.control =
        cui_data_table_common(ui, id, theme, model, state, options, label, render, user, NULL, NULL,
                              &handler, workspace, &result);
    return result;
}
