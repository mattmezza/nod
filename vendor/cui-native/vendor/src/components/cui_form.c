/* SPDX-License-Identifier: MIT */
#include "../base/cui_form_private.h"
#include "cui/cui_form_events.h"
#include "cui/cui_icon.h"
#include "cui_input_private.h"
#include <math.h>
static float cui_form_private_form_gap(cui_form_options o) {
    return o.size == CUI_FORM_LARGE                                ? 12
           : o.size == CUI_FORM_SMALL || o.size == CUI_FORM_XSMALL ? 6
                                                                   : 8;
}
static void cui_form_private_label(cui_context *ui, const cui_theme *t, const cui_form_field *f,
                                   cui_form_options o, float width) {
    cui_begin_box(ui, cui_form_label_id(f->id),
                  (cui_box){.width = width,
                            .row = true,
                            .gap = o.design_size == CUI_DESIGN_INHERIT ? 4 : t->gap / 2,
                            .align_y = CUI_ALIGN_CENTER,
                            .clip = true});
    if (f->render_label)
        f->render_label(ui, t, f->user, width);
    else if (f->label.length)
        cui_text(ui, f->label, o.label_text_size > 0 ? o.label_text_size : t->font_size * .875f,
                 t->text);
    if (f->required && (f->label.length || f->render_label))
        cui_text(ui, cui_string("*"),
                 o.label_text_size > 0 ? o.label_text_size : t->font_size * .875f, t->danger);
    cui_end_box(ui);
}
static void cui_form_private_field(cui_context *ui, const cui_theme *t, const cui_form_field *f,
                                   cui_form_cell cell, cui_form_options o) {
    float gap = o.design_size == CUI_DESIGN_INHERIT ? (o.size == CUI_FORM_LARGE ? 8 : 4) : t->gap;
    float inner_gap = o.horizontal_labels ? gap : gap / 2;
    float lw = o.label_width > 0 ? o.label_width : 140;
    lw = fminf(lw, fmaxf(0, cell.width - inner_gap - 1));
    bool has_label = !f->no_label_indent;
    float control_width = cell.width - (o.horizontal_labels && has_label ? lw + inner_gap : 0);
    cui_begin_box(ui, f->id, (cui_box){.width = cell.width, .gap = gap / 2});
    cui_begin_box(
        ui, f->id ^ UINT64_C(0x77f9cf130c3825ba),
        (cui_box){.width = -1, .row = o.horizontal_labels, .gap = inner_gap, .align_y = f->align});
    if (has_label)
        cui_form_private_label(ui, t, f, o, o.horizontal_labels ? lw : cell.width);
    cui_begin_box(ui, f->id ^ UINT64_C(0x6d83e029ab908f21), (cui_box){.width = control_width});
    if (f->render_control)
        f->render_control(ui, t, f->user, control_width);
    cui_end_box(ui);
    cui_end_box(ui);
    if (f->render_description || f->description.length || (f->invalid && f->error.length)) {
        cui_begin_box(ui, cui_form_description_id(f->id),
                      (cui_box){.width = -1, .row = o.horizontal_labels, .gap = inner_gap});
        if (has_label && o.horizontal_labels) {
            cui_begin_box(ui, 0, (cui_box){.width = lw});
            cui_end_box(ui);
        }
        cui_begin_box(ui, f->id ^ UINT64_C(0x51e73c496a39c41b), (cui_box){.width = control_width});
        if (f->invalid && f->error.length)
            cui_text(ui, f->error, t->font_size * .75f, t->danger);
        else if (f->render_description)
            f->render_description(ui, t, f->user, control_width);
        else
            cui_text(ui, f->description, t->font_size * .75f, t->muted);
        cui_end_box(ui);
        cui_end_box(ui);
    }
    cui_form_associate_control(ui, f);
    cui_end_box(ui);
}
cui_form_error cui_form(cui_context *ui, cui_id id, const cui_theme *t, const cui_form_field *f,
                        size_t count, cui_form_options o) {
    if (!ui || !id || !t || o.design_size < CUI_DESIGN_INHERIT || o.design_size > CUI_DESIGN_9XL)
        return CUI_FORM_INVALID;
    cui_theme scaled;
    if (o.design_size != CUI_DESIGN_INHERIT) {
        cui_design_metrics design;
        if (!cui_design_measure(o.design_size, t->font_size / 16, &design))
            return CUI_FORM_INVALID;
        scaled = *t;
        scaled.font_size = design.font_size;
        scaled.control_height = design.control_height;
        scaled.padding = design.padding_x;
        scaled.gap = design.gap;
        t = &scaled;
        if (o.gap_x == 0)
            o.gap_x = design.gap * 3;
        if (o.gap_y == 0)
            o.gap_y = design.gap;
    }
    cui_form_cell cells[256];
    size_t rows;
    cui_form_error e = cui_form_plan(f, count, o, cells, 256, &rows);
    if (e != CUI_FORM_OK)
        return e;
    for (size_t i = 0; i < count; i++)
        if (f[i].id == id)
            return CUI_FORM_DUPLICATE_ID;
    float width = o.width > 0 ? o.width : 480;
    float gx = o.gap_x > 0 ? o.gap_x : cui_form_private_form_gap(o) * 3;
    float gy = o.gap_y > 0 ? o.gap_y : cui_form_private_form_gap(o);
    float unit = (width - gx * ((o.columns ? o.columns : 1) - 1)) / (o.columns ? o.columns : 1);
    cui_begin_box(ui, id, (cui_box){.width = width, .gap = gy});
    e = cui_base_form(ui, id, f, count, o);
    for (size_t row = 0; row < rows; row++) {
        cui_begin_box(ui, id ^ (UINT64_C(0x514d63acb48d9587) + row),
                      (cui_box){.width = -1, .row = true, .align_y = CUI_ALIGN_START});
        size_t cursor = 0;
        for (size_t i = 0; i < count; i++)
            if (cells[i].visible && cells[i].row == row) {
                float spacer = (cells[i].column - cursor) * (unit + gx) + (cursor ? gx : 0);
                if (spacer > 0) {
                    cui_begin_box(ui, 0, (cui_box){.width = spacer});
                    cui_end_box(ui);
                }
                cui_form_private_field(ui, t, &f[i], cells[i], o);
                cursor = cells[i].column + cells[i].span;
            }
        cui_end_box(ui);
    }
    if (o.render_footer) {
        cui_begin_box(ui, id ^ UINT64_C(0x21782c34a96dc151),
                      (cui_box){.width = -1, .row = true, .align_x = CUI_ALIGN_END});
        o.render_footer(ui, t, o.footer_user, width);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return e;
}
/* Styled groups reuse the editor's reducer and paint-only control. */
#include "cui/cui_components.h"
static void cui_form_private_addon(cui_context *ui, const cui_theme *t,
                                   const cui_input_group_addon *a, cui_input_group_options o,
                                   float height, float rem_size) {
    bool disabled = o.input.disabled || a->disabled || a->loading;
    cui_box box = {.width = a->width,
                   .fixed_width = o.design_size != CUI_DESIGN_INHERIT,
                   .height = height,
                   .clip = o.design_size != CUI_DESIGN_INHERIT,
                   .row = true,
                   .align_y = CUI_ALIGN_CENTER,
                   .align_x = CUI_ALIGN_CENTER};
    cui_color ink = disabled ? t->muted : t->text;
    if (a->kind == CUI_INPUT_GROUP_BUTTON) {
        box.radius = t->radius * .5f;
        if (a->variant == CUI_INPUT_GROUP_PRIMARY) {
            box.background = t->primary;
            ink = t->on_primary;
        } else if (a->variant == CUI_INPUT_GROUP_DANGER) {
            box.background = t->danger;
            ink = t->on_primary;
        } else if (a->variant == CUI_INPUT_GROUP_SECONDARY)
            box.background = t->background;
        else if (a->variant == CUI_INPUT_GROUP_OUTLINE) {
            box.border = t->border;
            box.border_width = 1;
        }
        if (cui_focused(ui) == a->id && !disabled) {
            box.border = t->focus;
            box.border_width = 2;
        }
    }
    cui_begin_box(ui, a->id, box);
    if (a->render)
        a->render(ui, t, a->user, disabled, o.input.readonly, a->width);
    else if (a->loading)
        (void)cui_icon_spinner(
            ui, a->id ^ UINT64_C(0x9cd0876164b3fe2a), t,
            (cui_icon_options){.size = CUI_ICON_CUSTOM,
                               .custom_size = o.design_size != CUI_DESIGN_INHERIT
                                                  ? t->font_size
                                                  : (o.size == CUI_FORM_XSMALL  ? 12
                                                     : o.size == CUI_FORM_SMALL ? 14
                                                     : o.size == CUI_FORM_LARGE ? 24
                                                                                : 16) *
                                                        rem_size / 16,
                               .custom_color = true,
                               .color = a->kind == CUI_INPUT_GROUP_TEXT ? t->muted : ink});
    else
        cui_text(ui, a->label, t->font_size, a->kind == CUI_INPUT_GROUP_TEXT ? t->muted : ink);
    cui_end_box(ui);
}
static void cui_form_private_addons(cui_context *ui, const cui_theme *t,
                                    const cui_input_group_addon *a, size_t count,
                                    cui_input_group_options o, cui_input_group_alignment alignment,
                                    float height, float rem_size) {
    for (size_t i = 0; i < count; i++)
        if (a[i].alignment == alignment)
            cui_form_private_addon(ui, t, &a[i], o, height, rem_size);
}
typedef struct {
    const cui_theme *theme;
    cui_input_group_state *state;
    cui_input_options *paint;
} cui_form_private_secure;
static cui_input_event_result
cui_form_private_secure_input(void *user, cui_context *ui, cui_id id, cui_str name, cui_edit *edit,
                              cui_input_behavior_state *behavior, cui_base_input_options options,
                              bool synthetic_hit, cui_component_text_handler handler) {
    cui_form_private_secure *secure = user;
    cui_input_state temporary = {.behavior = *behavior, .scroll_x = secure->state->scroll_x};
    cui_input_event_result result = cui_input_private_reduce(
        ui, id, secure->theme, name, edit, &temporary, behavior, secure->paint, options,
        secure->state->viewport, synthetic_hit, handler);
    secure->state->scroll_x = temporary.scroll_x;
    return result;
}
static cui_input_group_result
cui_form_private_styled_group(cui_context *ui, cui_id group, cui_id input, const cui_theme *theme,
                              cui_edit *edit, cui_input_group_state *state,
                              const cui_input_group_addon *a, size_t count,
                              cui_input_group_options o, cui_input_group_handler handler,
                              cui_input_group_event_result *notifications) {
    if (!theme || o.design_size < CUI_DESIGN_INHERIT || o.design_size > CUI_DESIGN_9XL)
        return (cui_input_group_result){.error = CUI_FORM_INVALID,
                                        .input.error = CUI_EDIT_INVALID_ARGUMENT};
    cui_input_size input_size = o.size == CUI_FORM_XSMALL  ? CUI_INPUT_XSMALL
                                : o.size == CUI_FORM_SMALL ? CUI_INPUT_SMALL
                                : o.size == CUI_FORM_LARGE ? CUI_INPUT_LARGE
                                                           : CUI_INPUT_MEDIUM;
    float nominal_font = o.size == CUI_FORM_XSMALL ? 12 : o.size == CUI_FORM_LARGE ? 16 : 14;
    cui_input_options paint_options = {.width = -1,
                                       .size = input_size,
                                       .design_size = o.design_size,
                                       .density = o.font_size > 0 ? o.font_size / nominal_font
                                                                  : theme->font_size / 16,
                                       .no_appearance = true,
                                       .disabled = o.input.disabled,
                                       .readonly = o.input.readonly,
                                       .required = o.input.required,
                                       .placeholder = o.placeholder};
    if (o.design_size != CUI_DESIGN_INHERIT) {
        cui_design_metrics design;
        if (!isfinite(o.font_size) || o.font_size < 0 ||
            !cui_design_measure(o.design_size, theme->font_size / 16, &design))
            return (cui_input_group_result){.error = CUI_FORM_INVALID,
                                            .input.error = CUI_EDIT_INVALID_ARGUMENT};
        paint_options.density = theme->font_size / 16;
        if (o.font_size > 0) {
            paint_options.density *= o.font_size / design.font_size;
            paint_options.custom_height = true;
            paint_options.height = design.control_height;
            paint_options.custom_padding = true;
            paint_options.padding_x = design.padding_x;
            paint_options.padding_y = design.padding_y;
        }
    }
    cui_input_metrics metrics;
    if (!cui_input_measure(paint_options, theme->font_size, &metrics))
        return (cui_input_group_result){.error = CUI_FORM_INVALID,
                                        .input.error = CUI_EDIT_INVALID_ARGUMENT};
    cui_theme t = *theme;
    t.control_height = metrics.height;
    t.font_size = metrics.font_size;
    t.padding = metrics.padding_x;
    o.font_size = t.font_size;
    cui_input_group_addon bounded[64];
    if (o.design_size != CUI_DESIGN_INHERIT) {
        if (!ui || !edit || !state || count > 64 || (count && !a) || !isfinite(o.width) ||
            o.width < 0)
            return (cui_input_group_result){.error = CUI_FORM_INVALID,
                                            .input.error = CUI_EDIT_INVALID_ARGUMENT};
        float group_width = o.width > 0 ? o.width : 280;
        float available = group_width - 2;
        if (available <= 0)
            return (cui_input_group_result){.error = CUI_FORM_INVALID,
                                            .input.error = CUI_EDIT_INVALID_ARGUMENT};
        for (size_t i = 0; i < count; ++i) {
            if ((a[i].label.length && !a[i].label.data) || !isfinite(a[i].width) ||
                a[i].width < 0 || a[i].alignment < CUI_INPUT_GROUP_INLINE_START ||
                a[i].alignment > CUI_INPUT_GROUP_BLOCK_END)
                return (cui_input_group_result){.error = CUI_FORM_INVALID,
                                                .input.error = CUI_EDIT_INVALID_ARGUMENT};
            bounded[i] = a[i];
            if (!a[i].width) {
                float content = a[i].loading
                                    ? metrics.font_size
                                    : cui_measure_text(ui, a[i].label, metrics.font_size).width;
                bounded[i].width = fminf(available, content + metrics.padding_x * 2);
            }
        }
        /* Inline addons share one row; block-start/end each have their own row.
         * Preserve explicit widths, fit inferred widths, reserve usable editor space. */
        for (unsigned row = 0; row < 3; ++row) {
            float explicit_width = 0, inferred_width = 0;
            for (size_t i = 0; i < count; ++i) {
                unsigned addon_row = a[i].alignment <= CUI_INPUT_GROUP_INLINE_END    ? 0
                                     : a[i].alignment == CUI_INPUT_GROUP_BLOCK_START ? 1
                                                                                     : 2;
                if (addon_row == row) {
                    if (a[i].width > 0)
                        explicit_width += a[i].width;
                    else
                        inferred_width += bounded[i].width;
                }
            }
            float budget = row == 0 ? available - fminf(available / 2, metrics.height) : available;
            if (explicit_width > budget)
                return (cui_input_group_result){.error = CUI_FORM_INVALID,
                                                .input.error = CUI_EDIT_INVALID_ARGUMENT};
            float factor =
                inferred_width > 0 ? fminf(1, (budget - explicit_width) / inferred_width) : 1;
            for (size_t i = 0; i < count; ++i) {
                unsigned addon_row = a[i].alignment <= CUI_INPUT_GROUP_INLINE_END    ? 0
                                     : a[i].alignment == CUI_INPUT_GROUP_BLOCK_START ? 1
                                                                                     : 2;
                if (addon_row == row && !a[i].width)
                    bounded[i].width *= factor;
            }
        }
        float inline_width = 0;
        for (size_t i = 0; i < count; ++i)
            if (bounded[i].alignment <= CUI_INPUT_GROUP_INLINE_END)
                inline_width += bounded[i].width;
        paint_options.width = group_width - inline_width;
        if (paint_options.width <= metrics.padding_x * 2 + 1) {
            paint_options.custom_padding = true;
            paint_options.padding_x = fmaxf(0, (paint_options.width - 1) / 2);
            paint_options.padding_y = metrics.padding_y;
        }
        a = bounded;
    }
    paint_options.masked = o.input.masked;
    paint_options.content_type = o.input.content_type;
    paint_options.format = o.input.format;
    paint_options.format_workspace = o.input.format_workspace;
    paint_options.clipboard = o.input.clipboard;
    paint_options.validate = o.input.validate;
    paint_options.validate_user = o.input.validate_user;
    paint_options.normalization = o.input.normalization;
    paint_options.matcher = o.input.matcher;
    cui_input_group_result result;
    if (o.input.masked) {
        cui_form_private_secure secure = {&t, state, &paint_options};
        result =
            cui_form_private_input_group(ui, group, input, edit, state, a, count, o, handler,
                                         notifications, cui_form_private_secure_input, &secure);
    } else if (notifications) {
        cui_input_group_event_result observed =
            cui_base_input_group_events(ui, group, input, edit, state, a, count, o, handler);
        result = observed.control;
        notifications->addon_count = observed.addon_count;
        notifications->change_count = observed.change_count;
        notifications->submit_count = observed.submit_count;
        notifications->count_overflow = observed.count_overflow;
    } else
        result = cui_base_input_group(ui, group, input, edit, state, a, count, o);
    if (result.error != CUI_FORM_OK)
        return result;
    bool focused = cui_focused(ui) == input;
    cui_color border = o.input.invalid || result.input.error != CUI_EDIT_OK ? t.danger
                       : focused                                            ? t.focus
                                                                            : t.border;
    float h = t.control_height;
    cui_begin_box(ui, group,
                  (cui_box){.width = o.width > 0 ? o.width : 280,
                            .radius = t.radius,
                            .border = border,
                            .border_width = focused ? 2 : 1,
                            .background = o.input.disabled ? t.background : t.surface,
                            .clip = true});
    for (unsigned side = 0; side < 2; side++) {
        cui_input_group_alignment align =
            side == 0 ? CUI_INPUT_GROUP_BLOCK_START : CUI_INPUT_GROUP_BLOCK_END;
        if (side == 1) {
            cui_begin_box(ui, group ^ UINT64_C(0x6138a725bd65061b),
                          (cui_box){.width = -1,
                                    .row = true,
                                    .align_y = CUI_ALIGN_CENTER,
                                    .height = cui_edit_get_state(edit).multiline ? 0 : h});
            cui_form_private_addons(ui, &t, a, count, o, CUI_INPUT_GROUP_INLINE_START, h,
                                    theme->font_size);
            cui_theme frameless = t;
            frameless.border = frameless.focus = frameless.surface = frameless.background =
                (cui_color){0};
            cui_input_state paint_state = {.behavior = state->behavior,
                                           .scroll_x = state->scroll_x};
            cui_input_paint(ui, input, &frameless, cui_string(""), edit, &paint_state,
                            paint_options);
            state->scroll_x = paint_state.scroll_x;
            (void)cui_track_bounds(ui, cui_input_viewport_id(input), &state->viewport);
            cui_form_private_addons(ui, &t, a, count, o, CUI_INPUT_GROUP_INLINE_END, h,
                                    theme->font_size);
            cui_end_box(ui);
        }
        bool any = false;
        for (size_t i = 0; i < count; i++)
            any |= a[i].alignment == align;
        if (any) {
            cui_begin_box(ui, group ^ (UINT64_C(0x4871b368093d6ade) + side),
                          (cui_box){.width = -1, .row = true, .align_y = CUI_ALIGN_CENTER});
            cui_form_private_addons(ui, &t, a, count, o, align, h, theme->font_size);
            cui_end_box(ui);
        }
    }
    cui_end_box(ui);
    return result;
}

cui_input_group_result cui_input_group(cui_context *ui, cui_id group, cui_id input,
                                       const cui_theme *theme, cui_edit *edit,
                                       cui_input_group_state *state,
                                       const cui_input_group_addon *addons, size_t count,
                                       cui_input_group_options options) {
    return cui_form_private_styled_group(ui, group, input, theme, edit, state, addons, count,
                                         options, (cui_input_group_handler){0}, NULL);
}
cui_input_group_event_result cui_input_group_events(cui_context *ui, cui_id group, cui_id input,
                                                    const cui_theme *theme, cui_edit *edit,
                                                    cui_input_group_state *state,
                                                    const cui_input_group_addon *addons,
                                                    size_t count, cui_input_group_options options,
                                                    cui_input_group_handler handler) {
    cui_input_group_event_result result = {0};
    result.control = cui_form_private_styled_group(ui, group, input, theme, edit, state, addons,
                                                   count, options, handler, &result);
    return result;
}
