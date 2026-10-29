/* SPDX-License-Identifier: MIT */
#include "cui/cui_stepper_visual.h"
#include <stdio.h>

cui_stepper_navigation_result
cui_stepper_visual_navigation(cui_context *ui, cui_id id, const cui_theme *t, const cui_step *steps,
                              size_t count, size_t selected, cui_stepper_visual_options o,
                              cui_stepper_navigation_handler handler) {
    if (!t || o.density < CUI_STEPPER_DENSITY_MD || o.density > CUI_STEPPER_DENSITY_LG ||
        count > 100 || (o.icons ? o.icon_count != count : o.icon_count != 0))
        return (cui_stepper_navigation_result){0};
    cui_design_metrics design = {0};
    if (o.design_size < CUI_DESIGN_INHERIT || o.design_size > CUI_DESIGN_9XL ||
        (o.design_size != CUI_DESIGN_INHERIT &&
         !cui_design_measure(o.design_size, t->font_size / 16, &design)))
        return (cui_stepper_navigation_result){0};
    for (size_t i = 0; o.icons && i < count; ++i)
        if (o.icons[i].present && !cui_bitmap_valid(o.icons[i].bitmap))
            return (cui_stepper_navigation_result){0};
    cui_stepper_navigation_result result =
        cui_base_stepper_navigation(ui, id, steps, count, selected, o.disabled, handler);
    if (!result.valid)
        return result;
    float side = o.density == CUI_STEPPER_DENSITY_XS   ? 8
                 : o.density == CUI_STEPPER_DENSITY_SM ? 18
                 : o.density == CUI_STEPPER_DENSITY_LG ? 32
                                                       : 24;
    float thickness = o.density == CUI_STEPPER_DENSITY_XS   ? 1.5f
                      : o.density == CUI_STEPPER_DENSITY_LG ? 3
                                                            : 2;
    float scale = o.design_size == CUI_DESIGN_INHERIT ? 1 : design.indicator_size / 16;
    float gap = o.design_size == CUI_DESIGN_INHERIT ? 4 : design.gap;
    float font = o.design_size == CUI_DESIGN_INHERIT ? t->font_size : design.font_size;
    side *= scale;
    thickness *= scale;
    cui_begin_box(ui, id, (cui_box){.row = !o.vertical, .gap = gap});
    for (size_t i = 0; i < count; ++i) {
        bool checked = i <= selected;
        cui_color foreground = checked ? t->on_primary : t->text;
        cui_begin_box(ui, cui_numeric_part_id(id, 1 + (unsigned)i),
                      (cui_box){.row = o.vertical, .gap = o.vertical ? gap * 2 : gap});
        cui_begin_box(ui, 0,
                      (cui_box){.width = side,
                                .height = side,
                                .radius = side / 2,
                                .background = checked ? t->primary : t->surface,
                                .clip = true,
                                .align_x = CUI_ALIGN_CENTER,
                                .align_y = CUI_ALIGN_CENTER});
        if (o.density != CUI_STEPPER_DENSITY_XS) {
            if (o.icons && o.icons[i].present) {
                cui_paint_bitmap(ui, 0,
                                 (cui_bitmap_paint){.bitmap = o.icons[i].bitmap,
                                                    .opacity = 1,
                                                    .custom_tint = true,
                                                    .tint = foreground},
                                 (cui_box){.width = side, .height = side});
            } else {
                char ordinal[4];
                (void)snprintf(ordinal, sizeof(ordinal), "%u", (unsigned)i + 1);
                cui_text(ui, cui_string(ordinal), font * .8f, foreground);
            }
        }
        cui_end_box(ui);
        cui_begin_box(ui, 0, (cui_box){.gap = gap});
        cui_text(ui, steps[i].title, font, o.disabled || steps[i].disabled ? t->muted : t->text);
        if (steps[i].description.length)
            cui_text(ui, steps[i].description, font * .8f, t->muted);
        if (steps[i].status == CUI_STEP_ERROR)
            cui_text(ui, cui_string("Error"), font * .8f, t->danger);
        cui_end_box(ui);
        cui_end_box(ui);
        if (i + 1 < count) {
            cui_begin_box(ui, 0,
                          (cui_box){.width = o.vertical ? side : 36 * scale,
                                    .height = o.vertical ? 20 * scale : side,
                                    .align_x = CUI_ALIGN_CENTER,
                                    .align_y = CUI_ALIGN_CENTER});
            cui_begin_box(ui, 0,
                          (cui_box){.width = o.vertical ? thickness : 36 * scale,
                                    .height = o.vertical ? 20 * scale : thickness,
                                    .background = i < selected ? t->primary : t->border});
            cui_end_box(ui);
            cui_end_box(ui);
        }
    }
    cui_end_box(ui);
    return result;
}
