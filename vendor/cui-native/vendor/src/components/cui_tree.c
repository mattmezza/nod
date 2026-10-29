/* SPDX-License-Identifier: MIT */
#include "cui/cui_icon.h"
#include "cui/cui_scroll.h"
#include "cui/cui_tree_events.h"
#include <math.h>
static cui_id cui_tree_style_id(cui_id id, unsigned index) {
    return id ^ (UINT64_C(0x18d9f87aab972b61) + UINT64_C(0x9e3779b97f4a7c15) * index);
}
static cui_tree_result cui_tree_common(cui_context *ui, cui_id id, const cui_theme *t,
                                       cui_tree_model *model, cui_tree_state *state,
                                       cui_id *selected, cui_tree_options options,
                                       cui_tree_handler handler, cui_tree_event_result *out) {
    if (!ui || !t || !state || !model || !selected)
        return (cui_tree_result){.error = CUI_TREE_INVALID};
    float row = isfinite(options.row_height) && options.row_height > 0
                    ? fminf(options.row_height, 4096)
                    : 32;
    float height = isfinite(options.height) && options.height > 0 ? options.height : 240;
    float width = isfinite(options.width) && options.width > 0 ? options.width : 320;
    cui_tree_result result;
    if (out) {
        *out = cui_tree_reduce_events(ui, id, model, state, selected, options, handler);
        result = out->result;
    } else {
        result = cui_tree_reduce(ui, id, model, state, selected, options);
    }
    cui_tree_view view = cui_tree_viewport(model, state->scroll, height, row, options.overscan);
    cui_scroll_options bars =
        cui_scroll_resolve(t, (cui_scroll_options){.vertical = true,
                                                   .disabled = options.disabled,
                                                   .wheel_step = row,
                                                   .scrollbar_style = options.scrollbar_style});
    float x = 0;
    bool bar_changed =
        cui_base_scrollbar_overlay(ui, id, &x, &state->scroll, (cui_rect){0, 0, width, height},
                                   (cui_size){width, view.total}, bars);
    if (bar_changed)
        view = cui_tree_viewport(model, state->scroll, height, row, options.overscan);
    cui_begin_box(ui, id,
                  (cui_box){.width = width,
                            .height = height,
                            .clip = true,
                            .offset_y = view.before - view.scroll,
                            .background = t->surface,
                            .border = t->border,
                            .border_width = 1,
                            .radius = t->radius});
    cui_tree_submit(ui, id, model, state, *selected, options);
    for (size_t p = view.begin; p < view.end; ++p) {
        size_t index;
        cui_tree_node node;
        if (!cui_tree_visible_node(model, p, &index) || !cui_tree_get_node(model, index, &node))
            continue;
        cui_id row_id = cui_tree_row_id(id, node.key);
        bool disabled = options.disabled || node.disabled, active = *selected == node.key;
        cui_begin_box(ui, row_id,
                      (cui_box){.width = width,
                                .height = row,
                                .row = true,
                                .gap = 4,
                                .padding = 4,
                                .background = active ? t->primary : t->surface,
                                .border = cui_focused(ui) == row_id ? t->focus : t->border,
                                .border_width = cui_focused(ui) == row_id ? 2 : 0});
        if (node.depth) {
            cui_begin_box(ui, cui_tree_style_id(id, 100 + (unsigned)index),
                          (cui_box){.width = node.depth * 16.f});
            cui_end_box(ui);
        }
        if (cui_tree_is_branch(model, index)) {
            cui_begin_box(ui, cui_tree_toggle_id(id, node.key),
                          (cui_box){.width = 20, .height = fmaxf(1, row - 8)});
            (void)cui_icon_named(
                ui, cui_tree_toggle_id(id, node.key) ^ UINT64_C(0xd17a83c481e860f2), t,
                cui_string("chevron-right"),
                (cui_icon_options){.size = CUI_ICON_MEDIUM,
                                   .custom_color = true,
                                   .color = disabled ? t->muted
                                            : active ? t->on_primary
                                                     : t->text,
                                   .custom_transform = true,
                                   .transform = cui_icon_rotation(cui_tree_expanded(model, index)
                                                                      ? 1.57079632679489661923f
                                                                      : 0)});
            cui_end_box(ui);
        } else {
            cui_begin_box(ui, cui_tree_style_id(id, 200000 + (unsigned)index),
                          (cui_box){.width = 20});
            cui_end_box(ui);
        }
        cui_text(ui, node.label, t->font_size,
                 disabled ? t->muted
                 : active ? t->on_primary
                          : t->text);
        cui_end_box(ui);
    }
    cui_scrollbar_overlay_paint(ui, id, t, 0, state->scroll, (cui_rect){0, 0, width, height},
                                (cui_size){width, view.total}, bars);
    cui_end_box(ui);
    return result;
}

cui_tree_result cui_tree(cui_context *ui, cui_id id, const cui_theme *theme, cui_tree_model *model,
                         cui_tree_state *state, cui_id *selected, cui_tree_options options) {
    return cui_tree_common(ui, id, theme, model, state, selected, options, (cui_tree_handler){0},
                           NULL);
}
cui_tree_event_result cui_tree_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                      cui_tree_model *model, cui_tree_state *state,
                                      cui_id *selected, cui_tree_options options,
                                      cui_tree_handler handler) {
    cui_tree_event_result out = {0};
    if (!ui || !id || !theme || !model || !state || !selected) {
        out.result.error = CUI_TREE_INVALID;
        return out;
    }
    (void)cui_tree_common(ui, id, theme, model, state, selected, options, handler, &out);
    return out;
}
