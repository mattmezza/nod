/* SPDX-License-Identifier: MIT */
#include "cui/cui_description.h"
#include "../base/cui_design_metrics.h"
#include <math.h>
static cui_id cui_description_part(cui_id id, unsigned part) {
    return id ^ (UINT64_C(0x918c45e73f120a61) * (part + 1));
}
static void cui_description_spacer(cui_context *ui, cui_id id, float width, float height) {
    cui_begin_box(
        ui, id,
        (cui_box){.width = width, .height = height, .fixed_width = true, .fixed_height = true});
    cui_end_box(ui);
}
static void cui_description_content(cui_context *ui, cui_id id, const cui_theme *theme,
                                    cui_str text, cui_description_render_fn render, void *user,
                                    cui_box box, float px, float py, bool label) {
    cui_begin_box(ui, id, box);
    cui_description_spacer(ui, cui_description_part(id, 0), 0, py + box.border_edges.top);
    cui_begin_box(ui, cui_description_part(id, 1), (cui_box){.width = -1, .row = true});
    cui_description_spacer(ui, cui_description_part(id, 2), px + box.border_edges.left, 0);
    cui_id body = cui_description_part(id, 3);
    cui_begin_box(ui, body, (cui_box){.width = -1});
    cui_theme content = *theme;
    if (label) {
        content.font_size *= .875f;
        content.text = theme->description_list_label_foreground;
    }
    if (render)
        render(ui, &content, body, user);
    else {
        cui_interact(ui, body, CUI_ROLE_LABEL, text, false, false, 0);
        cui_set_tab_stop(ui, body, false);
        cui_text_with_line_height(ui, text, content.font_size, content.text,
                                  content.font_size * 1.618034f);
    }
    cui_end_box(ui);
    cui_description_spacer(ui, cui_description_part(id, 4), px + box.border_edges.right, 0);
    cui_end_box(ui);
    cui_description_spacer(ui, cui_description_part(id, 5), 0, py + box.border_edges.bottom);
    cui_end_box(ui);
}
bool cui_description_list(cui_context *ui, cui_id id, const cui_theme *theme,
                          const cui_description_item *items, size_t count,
                          cui_description_options options) {
    if (!ui || !id || !theme || !cui_design_valid(options.design_size) || (!items && count) ||
        !isfinite(options.width) || options.width < -1 || !isfinite(theme->font_size) ||
        theme->font_size <= 0 ||
        (options.custom_label_width && (!isfinite(options.label_width) || options.label_width < 0)))
        return false;
    for (size_t i = 0; i < count; ++i) {
        if (!items[i].id || items[i].id == id)
            return false;
        for (size_t j = 0; j < i; ++j)
            if (items[j].id == items[i].id)
                return false;
    }
    bool bordered = !options.unbordered;
    float px = 8, py = 4, gap = 4;
    if (options.size == CUI_DESCRIPTION_SMALL || options.size == CUI_DESCRIPTION_XSMALL) {
        px = 4;
        py = gap = 2;
    } else if (options.size == CUI_DESCRIPTION_LARGE) {
        px = 12;
        py = 6;
        gap = 8;
    }
    cui_theme scaled;
    if (options.design_size) {
        cui_design_metrics d;
        if (!cui_design_resolve(options.design_size, theme->font_size / 16, &d))
            return false;
        scaled = *theme;
        scaled.font_size = d.font_size;
        theme = &scaled;
        px = d.padding_x;
        py = d.padding_y;
        gap = d.gap;
    }
    if (!bordered)
        px = py = 0;
    cui_begin_box(ui, id,
                  (cui_box){.width = options.width > 0 ? options.width : -1,
                            .padding = bordered ? 1 : 0,
                            .gap = bordered ? 0 : gap,
                            .border = theme->border,
                            .border_width = bordered ? 1 : 0,
                            .radius = bordered ? theme->radius : 0,
                            .clip = true});
    cui_interact(ui, id, CUI_ROLE_GROUP, options.name, false, false, 0);
    cui_set_tab_stop(ui, id, false);
    size_t cursor = 0;
    cui_description_row row;
    while (cui_description_next_row(items, count, options.columns, &cursor, &row)) {
        cui_id row_id = cui_description_part(items[row.first].id, 17);
        cui_begin_box(ui, row_id, (cui_box){.width = -1, .row = true});
        if (row.separator) {
            cui_begin_box(
                ui, items[row.first].id,
                (cui_box){.width = -1,
                          .height = 8,
                          .background = bordered ? theme->description_list_label : (cui_color){0}});
            cui_end_box(ui);
        } else {
            for (size_t col = 0; col < row.count; ++col) {
                const cui_description_item *item = &items[row.first + col];
                float fraction = cui_description_fraction(&row, item->span, options.columns);
                float default_label_width = 120;
                if (options.width > 0 && !options.vertical && !options.custom_label_width)
                    default_label_width =
                        fminf(default_label_width, fmaxf(0, (options.width - 2) * fraction * .5f));
                cui_begin_box(ui, item->id,
                              (cui_box){.width = fraction,
                                        .width_percent = true,
                                        .row = !options.vertical,
                                        .height = options.vertical ? 0 : -1});
                cui_box label = {.width = options.vertical             ? -1
                                          : options.custom_label_width ? options.label_width
                                                                       : default_label_width,
                                 .fixed_width = !options.vertical,
                                 .width_percent = !options.vertical && options.custom_label_width &&
                                                  options.label_width_percent,
                                 .height = options.vertical ? 0 : -1,
                                 .background =
                                     bordered ? theme->description_list_label : (cui_color){0},
                                 .border = theme->border,
                                 .custom_border_edges = true};
                if (bordered) {
                    if (options.vertical)
                        label.border_edges.bottom = 1;
                    else {
                        label.border_edges.right = 1;
                        label.border_edges.left = col ? 1 : 0;
                    }
                }
                cui_description_content(ui, cui_description_part(item->id, 10), theme, item->label,
                                        item->render_label, item->label_user, label, px, py, true);
                cui_description_content(ui, cui_description_part(item->id, 11), theme, item->value,
                                        item->render_value, item->value_user,
                                        (cui_box){.width = -1}, px, py, false);
                cui_end_box(ui);
            }
        }
        cui_end_box(ui);
        if (bordered && cursor < count) {
            cui_begin_box(ui, cui_description_part(row_id, 18),
                          (cui_box){.width = -1, .height = 1, .background = theme->border});
            cui_end_box(ui);
        }
    }
    cui_end_box(ui);
    return true;
}
