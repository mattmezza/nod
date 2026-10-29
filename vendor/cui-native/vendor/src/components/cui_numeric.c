/* SPDX-License-Identifier: MIT */
#include "cui/cui_numeric.h"
#include "cui/cui_components.h"
#include "cui/cui_icon.h"
#include "cui/cui_number_events.h"
#include "cui/cui_numeric_selection_events.h"
#include <math.h>
#include <stdio.h>
static void cui_numeric_paint_button(cui_context *ui, cui_id id, const cui_theme *t, cui_str text,
                                     bool disabled) {
    cui_begin_box(ui, id,
                  (cui_box){.height = t->control_height,
                            .padding = t->padding,
                            .align_x = CUI_ALIGN_CENTER,
                            .align_y = CUI_ALIGN_CENTER,
                            .radius = t->radius,
                            .background = disabled ? t->surface : t->primary,
                            .border = cui_focused(ui) == id ? t->focus : t->border,
                            .border_width = 1});
    cui_text(ui, text, t->font_size, disabled ? t->muted : t->on_primary);
    cui_end_box(ui);
}
bool cui_number_measure(cui_number_options options, float root_font_size,
                        cui_number_metrics *metrics) {
    if (!metrics || options.size < CUI_NUMBER_MEDIUM || options.size > CUI_NUMBER_CUSTOM)
        return false;
    cui_input_metrics input;
    if (!cui_input_measure((cui_input_options){.size = (cui_input_size)options.size,
                                               .custom_size = options.custom_size,
                                               .density = options.density,
                                               .design_size = options.design_size},
                           root_font_size, &input))
        return false;
    float d = options.density > 0 ? options.density : root_font_size / 16;
    float width = options.size == CUI_NUMBER_XSMALL || options.size == CUI_NUMBER_SMALL ? 24 : 32;
    float icon = options.size == CUI_NUMBER_XSMALL  ? 12
                 : options.size == CUI_NUMBER_SMALL ? 14
                 : options.size == CUI_NUMBER_LARGE ? 24
                                                    : 16;
    if (options.design_size != CUI_DESIGN_INHERIT) {
        width = input.height / d;
        icon = input.font_size / d;
    } else if (options.size == CUI_NUMBER_CUSTOM) {
        width = options.custom_size / d;
        icon = options.custom_size / d;
    }
    *metrics =
        (cui_number_metrics){input.height,    input.font_size, input.line_height, input.padding_x,
                             input.padding_y, width * d,       icon * d};
    return true;
}
static void cui_number_step_paint(cui_context *ui, cui_id id, const cui_theme *theme,
                                  cui_number_metrics metrics, bool increment, bool disabled,
                                  float height, float radius) {
    cui_color background = {0};
    if (!disabled && (cui_hovered(ui, id) || cui_pressed(ui, id))) {
        background = theme->input;
        background.a = (uint8_t)lroundf(background.a * (cui_pressed(ui, id) ? .6f : .4f));
    }
    cui_begin_box(
        ui, id,
        (cui_box){.width = metrics.step_width,
                  .height = height,
                  .fixed_height = true,
                  .align_x = CUI_ALIGN_CENTER,
                  .align_y = CUI_ALIGN_CENTER,
                  .background = background,
                  .custom_corners = true,
                  .corner_radii =
                      increment ? (cui_corner_radii){.top_right = radius, .bottom_right = radius}
                                : (cui_corner_radii){.top_left = radius, .bottom_left = radius}});
    cui_color color = theme->secondary_foreground;
    cui_icon_named(ui, id ^ UINT64_C(0xf753713882a65049), theme,
                   increment ? cui_string("plus") : cui_string("minus"),
                   (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                      .custom_size = metrics.icon_size,
                                      .custom_color = true,
                                      .color = color});
    cui_end_box(ui);
}
static cui_number_result cui_number_input_common(cui_context *ui, cui_id id, const cui_theme *t,
                                                 cui_str name, cui_number_state *s, double *value,
                                                 cui_number_options o,
                                                 const cui_number_event_handler *handler,
                                                 size_t *change_count) {
    cui_number_metrics metrics;
    if (!ui || !id || !t || !cui_number_measure(o, t->font_size, &metrics) || !isfinite(o.width) ||
        (o.prefix.length && !o.prefix.data) || (o.suffix.length && !o.suffix.data) ||
        (o.placeholder.length && !o.placeholder.data))
        return (cui_number_result){.invalid = true, .error = CUI_EDIT_INVALID_ARGUMENT};
    cui_number_result result;
    if (handler) {
        cui_number_event_result notification =
            cui_base_number_input_events(ui, id, name, s, value, o, *handler);
        result = notification.control;
        *change_count = notification.change_count;
    } else
        result = cui_base_number_input(ui, id, name, s, value, o);
    if (!s || !s->edit || !value || result.error == CUI_EDIT_INVALID_ARGUMENT)
        return result;
    bool editable = !o.disabled && !o.readonly;
    float width = o.width ? o.width : 200, border = o.no_appearance ? 0 : 1;
    float inner = fmaxf(0, metrics.height - 2 * border);
    cui_color frame_border = t->input;
    if (o.disabled)
        frame_border.a = (uint8_t)lroundf(frame_border.a * .5f);
    else if (s->invalid)
        frame_border = t->danger;
    else if (cui_focused(ui) == id && !o.hide_focus_ring)
        frame_border = t->focus;
    cui_color background = t->input_background;
    if (o.disabled) {
        background = t->input;
        background.a = (uint8_t)lroundf(background.a * .2f);
    }
    cui_begin_box(ui, cui_numeric_part_id(id, 0),
                  (cui_box){.width = width,
                            .height = metrics.height,
                            .fixed_height = true,
                            .row = true,
                            .padding = border,
                            .gap = 0,
                            .align_y = CUI_ALIGN_CENTER,
                            .radius = t->radius,
                            .clip = true,
                            .background = o.no_appearance ? (cui_color){0} : background,
                            .border = frame_border,
                            .border_width = border,
                            .custom_opacity = o.disabled,
                            .opacity = .5f});
    float radius = o.no_appearance ? t->radius : fmaxf(0, t->radius - 1);
    cui_number_step_paint(ui, cui_numeric_part_id(id, 1), t, metrics, false, !editable, inner,
                          radius);
    if (o.prefix.length)
        cui_text_with_line_height(ui, o.prefix, metrics.font_size, t->text, metrics.line_height);
    float middle_width =
        width > 0 ? fmaxf(1, width - 2 * border - 2 * metrics.step_width -
                                 cui_measure_text(ui, o.prefix, metrics.font_size).width -
                                 cui_measure_text(ui, o.suffix, metrics.font_size).width)
                  : -1;
    cui_input_state input = {.behavior = {.composition = s->composition}, .scroll_x = s->scroll_x};
    cui_input_paint(ui, id, t, name, s->edit, &input,
                    (cui_input_options){.size = (cui_input_size)o.size,
                                        .design_size = o.design_size,
                                        .custom_size = o.custom_size,
                                        .density = o.density,
                                        .height = inner,
                                        .custom_height = true,
                                        .width = middle_width,
                                        .no_appearance = true,
                                        .no_border = true,
                                        .hide_focus_ring = true,
                                        .disabled = o.disabled,
                                        .readonly = o.readonly,
                                        .invalid = s->invalid,
                                        .placeholder = o.placeholder,
                                        .text_align = CUI_ALIGN_CENTER});
    s->scroll_x = input.scroll_x;
    if (o.suffix.length)
        cui_text_with_line_height(ui, o.suffix, metrics.font_size, t->text, metrics.line_height);
    cui_number_step_paint(ui, cui_numeric_part_id(id, 2), t, metrics, true, !editable, inner,
                          radius);
    cui_end_box(ui);
    return result;
}
cui_number_result cui_number_input(cui_context *ui, cui_id id, const cui_theme *theme, cui_str name,
                                   cui_number_state *state, double *value,
                                   cui_number_options options) {
    return cui_number_input_common(ui, id, theme, name, state, value, options, NULL, NULL);
}
cui_number_event_result cui_number_input_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                                cui_str name, cui_number_state *state,
                                                double *value, cui_number_options options,
                                                cui_number_event_handler handler) {
    cui_number_event_result result = {0};
    result.control = cui_number_input_common(ui, id, theme, name, state, value, options, &handler,
                                             &result.change_count);
    return result;
}
static bool cui_pagination_common(cui_context *ui, cui_id id, const cui_theme *t, size_t *page,
                                  size_t total, bool compact, bool disabled,
                                  const cui_pagination_event_handler *handler,
                                  size_t *change_count) {
    if (!t || !page)
        return false;
    bool changed;
    if (handler) {
        cui_numeric_selection_event_result result =
            cui_base_pagination_events(ui, id, page, total, compact, disabled, *handler);
        changed = result.control;
        *change_count = result.change_count;
    } else
        changed = cui_base_pagination(ui, id, page, total, compact, disabled);
    cui_begin_box(ui, id, (cui_box){.row = true, .gap = t->gap, .align_y = CUI_ALIGN_CENTER});
    cui_numeric_paint_button(ui, cui_numeric_part_id(id, 1), t, cui_string("First"),
                             disabled || *page <= 1);
    cui_numeric_paint_button(ui, cui_numeric_part_id(id, 2), t, cui_string("Previous"),
                             disabled || *page <= 1);
    if (compact) {
        char label[64];
        snprintf(label, sizeof(label), "%zu / %zu", *page, total);
        cui_text(ui, cui_string(label), t->font_size, t->text);
    } else {
        cui_page_item items[11];
        size_t n = cui_pagination_items(*page, total, 2, items);
        for (size_t i = 0; i < n; ++i) {
            if (items[i].ellipsis)
                cui_text(ui, cui_string("..."), t->font_size, t->muted);
            else {
                char label[32];
                snprintf(label, sizeof(label), "%zu", items[i].page);
                cui_numeric_paint_button(ui, cui_numeric_part_id(id, 10 + (unsigned)i), t,
                                         cui_string(label), disabled || items[i].page == *page);
            }
        }
    }
    cui_numeric_paint_button(ui, cui_numeric_part_id(id, 3), t, cui_string("Next"),
                             disabled || !total || *page >= total);
    cui_numeric_paint_button(ui, cui_numeric_part_id(id, 4), t, cui_string("Last"),
                             disabled || !total || *page >= total);
    cui_end_box(ui);
    return changed;
}
bool cui_pagination(cui_context *ui, cui_id id, const cui_theme *theme, size_t *page, size_t total,
                    bool compact, bool disabled) {
    return cui_pagination_common(ui, id, theme, page, total, compact, disabled, NULL, NULL);
}
cui_numeric_selection_event_result cui_pagination_events(cui_context *ui, cui_id id,
                                                         const cui_theme *theme, size_t *page,
                                                         size_t total, bool compact, bool disabled,
                                                         cui_pagination_event_handler handler) {
    cui_numeric_selection_event_result result = {0};
    result.control = cui_pagination_common(ui, id, theme, page, total, compact, disabled, &handler,
                                           &result.change_count);
    return result;
}
typedef struct {
    cui_icon_state outline, fill;
    bool initialized;
} cui_rating_icons;
static void cui_rating_icons_collect(void *payload) {
    cui_rating_icons *icons = payload;
    cui_icon_state_collect(&icons->outline);
    cui_icon_state_collect(&icons->fill);
}
static void cui_rating_icons_destroy(void *payload) {
    cui_rating_icons *icons = payload;
    cui_icon_state_destroy(&icons->outline);
    cui_icon_state_destroy(&icons->fill);
}
static cui_rating_icons *cui_rating_icons_get(cui_context *ui, cui_id id, float size) {
    cui_rating_icons *icons = cui_resource_get(ui, id, UINT64_C(0x929f4a4758cf0103), sizeof(*icons),
                                               cui_rating_icons_collect, cui_rating_icons_destroy);
    if (!icons)
        return NULL;
    if (!icons->initialized) {
        cui_icon_state_init(&icons->outline);
        cui_icon_state_init(&icons->fill);
        icons->initialized = true;
        const char *outline = "<svg width='24' height='24' viewBox='0 0 24 24' "
                              "xmlns='http://www.w3.org/2000/svg'><path d='M12 2 L15 8.2 L22 9.2 "
                              "L17 14 L18.2 21 L12 17.7 L5.8 21 L7 14 L2 9.2 L9 8.2 Z' fill='none' "
                              "stroke='white' stroke-width='1.6' stroke-linejoin='round'/></svg>";
        const char *fill =
            "<svg width='24' height='24' viewBox='0 0 24 24' "
            "xmlns='http://www.w3.org/2000/svg'><path d='M12 2 L15 8.2 L22 9.2 L17 14 L18.2 21 L12 "
            "17.7 L5.8 21 L7 14 L2 9.2 L9 8.2 Z' fill='white'/></svg>";
        if (!cui_icon_set_data(&icons->outline, cui_string(outline)) ||
            !cui_icon_set_data(&icons->fill, cui_string(fill)))
            return NULL;
    }
    if (cui_icon_prepare(&icons->outline, size, size, cui_icon_identity()) != CUI_ICON_OK ||
        cui_icon_prepare(&icons->fill, size, size, cui_icon_identity()) != CUI_ICON_OK)
        return NULL;
    return icons;
}
static bool cui_rating_common(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                              float *value, cui_rating_options o,
                              const cui_rating_event_handler *handler, size_t *change_count) {
    if (!t || !value)
        return false;
    bool changed;
    if (handler) {
        cui_numeric_selection_event_result result =
            cui_base_rating_events(ui, id, name, value, o, *handler);
        changed = result.control;
        *change_count = result.change_count;
    } else
        changed = cui_base_rating(ui, id, name, value, o);
    unsigned maximum = o.maximum ? o.maximum : 5;
    if (maximum > 100)
        maximum = 100;
    float width = isfinite(o.width) && o.width > 0 ? o.width : maximum * 36;
    float slot = fmaxf(1, (width - t->padding * 2 - t->gap * (maximum - 1)) / maximum);
    cui_begin_box(ui, cui_numeric_part_id(id, 0), (cui_box){.row = true, .gap = t->gap});
    cui_begin_box(ui, id,
                  (cui_box){.row = true,
                            .width = width,
                            .padding = t->padding,
                            .gap = t->gap,
                            .radius = t->radius,
                            .background = t->surface});
    float side = fminf(slot, fminf(t->control_height, t->font_size * 1.3f));
    cui_rating_icons *icons = side > 0 ? cui_rating_icons_get(ui, id, side) : NULL;
    cui_color color = o.disabled ? t->muted : t->primary;
    for (unsigned i = 0; i < maximum; ++i) {
        cui_begin_box(ui, cui_numeric_part_id(id, 10 + i),
                      (cui_box){.width = slot,
                                .height = t->control_height,
                                .align_x = CUI_ALIGN_CENTER,
                                .align_y = CUI_ALIGN_CENTER});
        cui_begin_box(ui, 0, (cui_box){.width = side, .height = side});
        if (icons) {
            cui_bitmap outline = cui_icon_state_bitmap(&icons->outline);
            cui_bitmap fill = cui_icon_state_bitmap(&icons->fill);
            cui_set_bitmap_underlay(
                ui, (cui_underlay_layout){.bounds = {0, 0, side, side}},
                (cui_bitmap_paint){
                    .bitmap = outline, .opacity = 1, .custom_tint = true, .tint = color},
                (cui_corner_radii){0});
            float fraction = fmaxf(0, fminf(1, *value - (float)i));
            if (fraction > 0) {
                cui_begin_absolute_box(ui, 0, (cui_box){0},
                                       (cui_rect){0, 0, side * fraction, side});
                cui_paint_bitmap(
                    ui, 0,
                    (cui_bitmap_paint){.bitmap = fill,
                                       .source = {0, 0, fill.width * fraction, fill.height},
                                       .opacity = 1,
                                       .custom_tint = true,
                                       .tint = color},
                    (cui_box){.width = side * fraction, .height = side});
                cui_end_box(ui);
            }
        }
        cui_end_box(ui);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    if (o.clearable)
        cui_numeric_paint_button(ui, cui_numeric_part_id(id, 1), t, cui_string("Clear"),
                                 o.disabled || o.readonly);
    cui_end_box(ui);
    return changed;
}
bool cui_rating(cui_context *ui, cui_id id, const cui_theme *theme, cui_str name, float *value,
                cui_rating_options options) {
    return cui_rating_common(ui, id, theme, name, value, options, NULL, NULL);
}
cui_numeric_selection_event_result cui_rating_events(cui_context *ui, cui_id id,
                                                     const cui_theme *theme, cui_str name,
                                                     float *value, cui_rating_options options,
                                                     cui_rating_event_handler handler) {
    cui_numeric_selection_event_result result = {0};
    result.control =
        cui_rating_common(ui, id, theme, name, value, options, &handler, &result.change_count);
    return result;
}
/* Private shared paint seam; not part of the public API. */
void cui_numeric_stepper_paint(cui_context *, cui_id, const cui_theme *, const cui_step *, size_t,
                               size_t, bool, bool);
void cui_numeric_stepper_paint(cui_context *ui, cui_id id, const cui_theme *t,
                               const cui_step *steps, size_t count, size_t current, bool vertical,
                               bool disabled) {
    cui_begin_box(ui, id, (cui_box){.row = !vertical, .gap = t->gap});
    for (size_t i = 0; i < count; ++i) {
        bool selected = i == current;
        const char *status = selected                               ? "Current"
                             : steps[i].status == CUI_STEP_ERROR    ? "Error"
                             : steps[i].status == CUI_STEP_COMPLETE ? "Complete"
                                                                    : "Pending";
        cui_begin_box(ui, cui_numeric_part_id(id, 100 + (unsigned)i), (cui_box){.gap = t->gap});
        cui_button_paint(
            ui, cui_numeric_part_id(id, 1 + (unsigned)i), t, steps[i].title,
            (cui_button_options){.variant = selected ? CUI_BUTTON_PRIMARY : CUI_BUTTON_OUTLINE,
                                 .disabled = disabled || steps[i].disabled,
                                 .selected = selected});
        cui_text(ui, cui_string(status), t->font_size * .8f,
                 steps[i].status == CUI_STEP_ERROR ? t->danger : t->muted);
        if (steps[i].description.length)
            cui_text(ui, steps[i].description, t->font_size * .8f, t->muted);
        cui_end_box(ui);
    }
    cui_end_box(ui);
}
static bool cui_stepper_common(cui_context *ui, cui_id id, const cui_theme *t,
                               const cui_step *steps, size_t count, size_t *current, bool vertical,
                               bool disabled, const cui_stepper_event_handler *handler,
                               size_t *change_count) {
    if (!t || !current || (!steps && count) || count > 100)
        return false;
    bool changed;
    if (handler) {
        cui_numeric_selection_event_result result =
            cui_base_stepper_events(ui, id, steps, count, current, disabled, *handler);
        changed = result.control;
        *change_count = result.change_count;
    } else
        changed = cui_base_stepper(ui, id, steps, count, current, disabled);
    cui_numeric_stepper_paint(ui, id, t, steps, count, *current, vertical, disabled);
    return changed;
}
bool cui_stepper(cui_context *ui, cui_id id, const cui_theme *theme, const cui_step *steps,
                 size_t count, size_t *current, bool vertical, bool disabled) {
    return cui_stepper_common(ui, id, theme, steps, count, current, vertical, disabled, NULL, NULL);
}
cui_numeric_selection_event_result cui_stepper_events(cui_context *ui, cui_id id,
                                                      const cui_theme *theme, const cui_step *steps,
                                                      size_t count, size_t *current, bool vertical,
                                                      bool disabled,
                                                      cui_stepper_event_handler handler) {
    cui_numeric_selection_event_result result = {0};
    result.control = cui_stepper_common(ui, id, theme, steps, count, current, vertical, disabled,
                                        &handler, &result.change_count);
    return result;
}
