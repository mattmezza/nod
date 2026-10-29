#include "../base/cui_dropdown_metadata_internal.h"
#include "cui/cui_dropdown_button_styles.h"
/* SPDX-License-Identifier: MIT */
#include "../base/cui_design_metrics.h"
#include "cui/cui_base.h"
#include "cui/cui_choice.h"
#include "cui/cui_components.h"
#include "cui/cui_icon.h"
#include "cui/cui_list_events.h"
#include "cui/cui_scroll.h"
#include "cui/cui_tabs_events.h"
#include <math.h>
/* Private Base reducer forwarding; not an application-facing List facade. */
bool cui_choice_private_list_events(cui_context *, const cui_choice_item *, size_t, cui_id *,
                                    cui_list_state *, cui_list_options, cui_id,
                                    cui_selection_handler, cui_selection_event_result *);
bool cui_choice_private_list_commands(cui_context *, const cui_choice_item *, size_t, cui_id *,
                                      cui_list_state *, cui_list_options, cui_id,
                                      cui_menu_command_handler, cui_menu_command_result *);
static void cui_choice_menu_level(cui_context *, const cui_theme *, const cui_menu_entry *, size_t,
                                  cui_menu_tree_state *, size_t, cui_rect, cui_menu_tree_options,
                                  cui_str);
static void cui_choice_ellipsized_text(cui_context *ui, cui_str label, float font_size,
                                       cui_color color, float width) {
    cui_str ellipsis = cui_string("\xe2\x80\xa6");
    float marker_width = cui_measure_text(ui, ellipsis, font_size).width;
    if (marker_width > width)
        return;
    size_t bytes = 0, upper = label.length;
    while (bytes < upper) {
        size_t middle = bytes + (upper - bytes + 1) / 2;
        while (middle < label.length && middle &&
               ((unsigned char)label.data[middle] & 0xc0) == 0x80)
            middle--;
        if (middle <= bytes) {
            middle = bytes + 1;
            while (middle < label.length && ((unsigned char)label.data[middle] & 0xc0) == 0x80)
                middle++;
        }
        if (cui_measure_text(ui, (cui_str){label.data, middle}, font_size).width + marker_width <=
            width)
            bytes = middle;
        else {
            upper = middle - 1;
            while (upper && ((unsigned char)label.data[upper] & 0xc0) == 0x80)
                upper--;
        }
    }
    cui_begin_box(ui, 0, (cui_box){.row = true, .gap = 0});
    if (bytes)
        cui_text(ui, (cui_str){label.data, bytes}, font_size, color);
    cui_text(ui, ellipsis, font_size, color);
    cui_end_box(ui);
}
static cui_color cui_tab_palette(cui_box *box, const cui_theme *theme, cui_tab_variant variant,
                                 bool selected, bool disabled, bool hovered) {
    cui_color foreground =
        selected || hovered ? theme->tab_active_foreground : theme->tab_foreground;
    box->background = (cui_color){0};
    box->border = (cui_color){0};
    box->border_width = 0;
    box->radius = 0;
    box->custom_border_edges = false;
    if (variant == CUI_TAB_DEFAULT) {
        box->custom_border_edges = true;
        box->border_edges = (cui_border_edges){.left = 1, .right = 1};
        if (selected) {
            box->background = disabled ? (cui_color){0} : theme->tab_active;
            box->border = theme->border;
        }
    } else if (variant == CUI_TAB_OUTLINE) {
        box->radius = theme->control_height * .5f;
        box->border_width = 1;
        box->border = selected ? theme->primary : theme->border;
        if (selected)
            foreground = theme->primary;
        else if (hovered && !disabled) {
            box->background = theme->secondary_hover;
            foreground = theme->secondary_foreground;
        }
    } else if (variant == CUI_TAB_PILL) {
        box->radius = theme->control_height * .5f;
        foreground = theme->text;
        if (selected) {
            box->background = theme->primary;
            foreground = theme->on_primary;
            if (disabled) {
                box->background.a *= .5f;
                foreground.a *= .5f;
            }
        } else if (hovered && !disabled) {
            box->background = theme->secondary;
            foreground = theme->secondary_foreground;
        }
    } else if (variant == CUI_TAB_SEGMENTED) {
        box->background = disabled ? theme->tab_bar : (cui_color){0};
    } else if (variant == CUI_TAB_UNDERLINE) {
        box->custom_border_edges = true;
        box->border_edges = (cui_border_edges){.bottom = 2};
        if (selected)
            box->border = disabled ? theme->border : theme->primary;
    }
    if (disabled && !(variant == CUI_TAB_PILL && selected))
        foreground = theme->muted;
    return foreground;
}
static void cui_tab_hsl(cui_color color, float channels[4]) {
    float r = color.r / 255.f, g = color.g / 255.f, b = color.b / 255.f;
    float high = fmaxf(r, fmaxf(g, b)), low = fminf(r, fminf(g, b));
    float delta = high - low, light = (high + low) * .5f;
    float hue = 0, saturation = 0;
    if (delta > 0) {
        saturation = delta / (1 - fabsf(2 * light - 1));
        hue = high == r   ? fmodf((g - b) / delta, 6)
              : high == g ? (b - r) / delta + 2
                          : (r - g) / delta + 4;
        hue /= 6;
        if (hue < 0)
            hue += 1;
    }
    channels[0] = hue;
    channels[1] = saturation;
    channels[2] = light;
    channels[3] = color.a / 255.f;
}
static cui_color cui_tab_foreground_fade(cui_color from, cui_color to, float elapsed) {
    if (elapsed <= 0)
        return from;
    if (elapsed >= .2f)
        return to;
    float t = elapsed / .2f;
    t = t < .5f ? 4 * t * t * t : 1 - powf(-2 * t + 2, 3) * .5f;
    float a[4], b[4];
    cui_tab_hsl(from, a);
    cui_tab_hsl(to, b);
    for (size_t i = 0; i < 4; i++)
        a[i] += (b[i] - a[i]) * t;
    float chroma = (1 - fabsf(2 * a[2] - 1)) * a[1];
    float x = chroma * (1 - fabsf(fmodf(a[0] * 6, 2) - 1));
    float m = a[2] - chroma * .5f, r = 0, g = 0, blue = 0;
    int sector = (int)(a[0] * 6) % 6;
    if (sector == 0) {
        r = chroma;
        g = x;
    } else if (sector == 1) {
        r = x;
        g = chroma;
    } else if (sector == 2) {
        g = chroma;
        blue = x;
    } else if (sector == 3) {
        g = x;
        blue = chroma;
    } else if (sector == 4) {
        r = x;
        blue = chroma;
    } else {
        r = chroma;
        blue = x;
    }
    return (cui_color){(uint8_t)lroundf((r + m) * 255), (uint8_t)lroundf((g + m) * 255),
                       (uint8_t)lroundf((blue + m) * 255), (uint8_t)lroundf(a[3] * 255)};
}
typedef struct {
    cui_rect local;
    float radius;
    cui_color background;
} cui_tab_raised_paint;
static bool cui_tab_raised_prepaint(const cui_prepaint_info *info, const void *payload,
                                    cui_prepaint_writer *writer) {
    const cui_tab_raised_paint *p = payload;
    cui_rect caster = p->local;
    caster.x += info->bounds.x;
    caster.y += info->bounds.y;
    cui_corner_radii radii = {p->radius, p->radius, p->radius, p->radius};
    const cui_drop_shadow layers[] = {
        {.offset_y = 1, .sigma = 1.5f, .color = {0, 0, 0, 26}},
        {.offset_y = 1, .sigma = 1, .spread = -1, .color = {0, 0, 0, 26}}};
    for (size_t i = 0; i < 2; ++i) {
        cui_drop_shadow_paint shadow = {.caster = caster, .radii = radii, .layer = layers[i]};
        if (!cui_prepaint_drop_shadow(writer, &shadow, false))
            return false;
    }
    cui_draw_command surface = {.type = CUI_DRAW_RECT,
                                .bounds = caster,
                                .color = p->background,
                                .corner_radii = radii,
                                .custom_corners = true};
    return cui_prepaint_emit(writer, &surface, false);
}
static void cui_choice_draw_ex(cui_context *ui, const cui_choice_item *item, const cui_theme *theme,
                               bool selected, bool disabled, cui_tab_variant variant,
                               cui_choice_render_fn render, size_t index, void *render_user,
                               float max_width, const cui_tab_parts *parts,
                               const cui_tab_metrics *metrics, const cui_tab_animation *animation,
                               cui_tab_shadow_policy shadow_policy, float source_inner_radius) {
    bool indicator_ready = animation && animation->ready;
    float inner_radius =
        metrics && variant == CUI_TAB_SEGMENTED
            ? shadow_policy != CUI_TAB_SHADOW_LEGACY
                  ? source_inner_radius
                  : fmaxf(0, (metrics->inner_height >= 24 ? theme->radius_lg : theme->radius) -
                                 (metrics->inner_height >= 28 ? 3 : 2))
            : 0;
    float padding = metrics ? metrics->padding_x : theme->padding;
    float part_gap = metrics ? metrics->part_gap : theme->gap;
    bool composed = metrics || (parts && (parts->prefix || parts->suffix || parts->icon));
    bool centered_trigger = !composed && variant == CUI_TAB_DEFAULT;
    cui_choice_item effective = *item;
    effective.disabled = disabled;
    bool structured = parts && (parts->prefix || parts->suffix || parts->icon);
    bool icon = structured && parts->icon;
    if (icon)
        max_width = 0;
    float prefix = structured && parts->prefix && isfinite(parts->prefix_width)
                       ? fmaxf(0, parts->prefix_width)
                       : 0;
    float suffix = structured && parts->suffix && isfinite(parts->suffix_width)
                       ? fmaxf(0, parts->suffix_width)
                       : 0;
    float gaps = structured ? part_gap * ((parts->prefix != NULL) + (parts->suffix != NULL)) : 0;
    float text_width = cui_measure_text(ui, item->label, theme->font_size).width;
    float body_limit = fmaxf(0, max_width - 2 * padding - prefix - suffix - gaps);
    cui_box box = {.height = theme->control_height,
                   .padding = metrics ? 0 : theme->padding,
                   .radius = theme->radius,
                   .background = selected ? theme->primary : theme->surface,
                   .border = cui_focused(ui) == item->id ? theme->focus : theme->border,
                   .border_width = cui_focused(ui) == item->id ? 2 : 1};
    if (variant == CUI_TAB_UNDERLINE) {
        box.background = theme->background;
        box.radius = 0;
        box.padding = 0;
        box.border_width = 0;
    } else if (variant == CUI_TAB_PILL) {
        box.radius = theme->control_height * .5f;
        if (!selected)
            box.border_width = 0;
    } else if (variant == CUI_TAB_OUTLINE) {
        box.background = theme->background;
        box.border = selected ? theme->primary : theme->border;
    } else if (variant == CUI_TAB_SEGMENTED) {
        box.radius = 0;
    }
    if (cui_focused(ui) == item->id) {
        box.border = theme->focus;
        box.border_width = 2;
    }
    bool ellipsized = !render && isfinite(max_width) && max_width > 0 &&
                      text_width + 2 * padding + prefix + suffix + gaps > max_width;
    if (isfinite(max_width) && max_width > 0) {
        box.max_width = max_width;
        box.clip = true;
    }
    if (ellipsized)
        box.width = max_width;
    else if (structured && !render && !icon && (!parts->prefix || prefix > 0) &&
             (!parts->suffix || suffix > 0))
        box.width = text_width + 2 * padding + prefix + suffix + gaps;
    if (centered_trigger) {
        box.padding = 0;
        box.row = true;
        box.align_y = CUI_ALIGN_CENTER;
    }
    cui_color tab_foreground = theme->text;
    if (metrics) {
        box.align_y = CUI_ALIGN_CENTER;
        tab_foreground =
            cui_tab_palette(&box, theme, variant, selected, disabled, cui_hovered(ui, item->id));
        if (variant == CUI_TAB_DEFAULT && index == 0 && parts && parts->omit_leading_border)
            box.border_edges.left = 0;
    }
    if (metrics && indicator_ready && selected && !disabled) {
        if (variant == CUI_TAB_PILL)
            box.background = (cui_color){0};
        else if (variant == CUI_TAB_UNDERLINE)
            box.border = (cui_color){0};
    }
    cui_begin_box(ui, item->id, box);
    if (centered_trigger && padding > 0) {
        cui_begin_box(ui, 0, (cui_box){.width = padding, .height = 1});
        cui_end_box(ui);
    }
    if (variant == CUI_TAB_UNDERLINE) {
        cui_begin_box(ui, item->id ^ UINT64_C(0x9ae71a843e64b150),
                      (cui_box){.height = metrics ? metrics->inner_height + metrics->margin_top +
                                                        metrics->margin_bottom
                                          : theme->control_height > 2 ? theme->control_height - 2
                                                                      : 0,
                                .padding = metrics ? 0 : theme->padding});
        if (metrics && metrics->margin_top > 0) {
            cui_begin_box(ui, 0, (cui_box){.height = metrics->margin_top});
            cui_end_box(ui);
        }
    }
    if (composed) {
        cui_begin_box(
            ui, item->id ^ UINT64_C(0xacf7823ab98e17d0),
            (cui_box){.width = -1,
                      .height = metrics ? metrics->inner_height : 0,
                      .row = true,
                      .gap = 0,
                      .radius = inner_radius,
                      .background =
                          metrics && variant == CUI_TAB_SEGMENTED && selected &&
                                  !(indicator_ready &&
                                    (!disabled || shadow_policy == CUI_TAB_SHADOW_SOURCE_BAR))
                              ? theme->background
                              : (cui_color){0},
                      .align_y = CUI_ALIGN_CENTER});
        if (variant == CUI_TAB_SEGMENTED && selected && !disabled) {
            if (shadow_policy == CUI_TAB_SHADOW_SOURCE_TAB)
                cui_set_shadow_preset(
                    ui, CUI_SHADOW_XS,
                    (cui_corner_radii){inner_radius, inner_radius, inner_radius, inner_radius});
            else if (shadow_policy == CUI_TAB_SHADOW_SOURCE_BAR && !indicator_ready)
                cui_set_shadow_preset(
                    ui, CUI_SHADOW_SM,
                    (cui_corner_radii){inner_radius, inner_radius, inner_radius, inner_radius});
        }
        if (padding > 0) {
            cui_begin_box(ui, 0, (cui_box){.width = padding});
            cui_end_box(ui);
        }
        if (structured && parts->prefix) {
            cui_begin_box(ui, item->id ^ UINT64_C(0xacf7823ab98e17e0),
                          (cui_box){.width = prefix, .clip = prefix > 0});
            parts->prefix(ui, &effective, index, parts->user);
            cui_end_box(ui);
            cui_begin_box(ui, 0, (cui_box){.width = part_gap});
            cui_end_box(ui);
        }
        cui_begin_box(
            ui, item->id ^ UINT64_C(0xacf7823ab98e17f0),
            (cui_box){.width =
                          icon ? (metrics ? metrics->inner_height : theme->control_height) * 1.25f
                          : render ? -1
                                   : fminf(text_width, max_width > 0 ? body_limit : text_width),
                      .fixed_width = !render && !icon,
                      .clip = true});
    }
    if (icon)
        parts->icon(ui, &effective, index, parts->user);
    else if (render)
        render(ui, &effective, index, render_user);
    else {
        cui_color color = disabled ? theme->muted
                          : selected && variant != CUI_TAB_UNDERLINE && variant != CUI_TAB_OUTLINE
                              ? theme->on_primary
                              : theme->text;
        if (metrics) {
            color = tab_foreground;
            if (indicator_ready && animation->transitioning && selected && !disabled &&
                variant == CUI_TAB_PILL)
                color = cui_tab_foreground_fade(theme->text, color, animation->foreground_elapsed);
        }
        if (ellipsized)
            cui_choice_ellipsized_text(ui, item->label, theme->font_size, color,
                                       structured ? body_limit : fmaxf(0, max_width - 2 * padding));
        else
            cui_text(ui, item->label, theme->font_size, color);
    }
    if (composed) {
        cui_end_box(ui);
        if (structured && parts->suffix) {
            cui_begin_box(ui, 0, (cui_box){.width = part_gap});
            cui_end_box(ui);
            cui_begin_box(ui, item->id ^ UINT64_C(0xacf7823ab98e1710),
                          (cui_box){.width = suffix, .clip = suffix > 0});
            parts->suffix(ui, &effective, index, parts->user);
            cui_end_box(ui);
        }
        if (padding > 0) {
            cui_begin_box(ui, 0, (cui_box){.width = padding});
            cui_end_box(ui);
        }
        cui_end_box(ui);
    }
    if (centered_trigger && padding > 0) {
        cui_begin_box(ui, 0, (cui_box){.width = padding, .height = 1});
        cui_end_box(ui);
    }
    if (variant == CUI_TAB_UNDERLINE) {
        if (metrics && metrics->margin_bottom > 0) {
            cui_begin_box(ui, 0, (cui_box){.height = metrics->margin_bottom});
            cui_end_box(ui);
        }
        cui_end_box(ui);
        if (!metrics) {
            cui_begin_box(ui, 0,
                          (cui_box){.width = -1,
                                    .height = 2,
                                    .background = selected ? theme->primary : theme->background});
            cui_end_box(ui);
        }
    }
    cui_end_box(ui);
}
static void cui_choice_draw(cui_context *ui, const cui_choice_item *item, const cui_theme *theme,
                            bool selected, bool disabled, cui_tab_variant variant) {
    cui_choice_draw_ex(ui, item, theme, selected, disabled, variant, NULL, 0, NULL, 0, NULL, NULL,
                       NULL, CUI_TAB_SHADOW_LEGACY, 0);
}
static float cui_tab_extent(cui_context *ui, const cui_theme *theme, const cui_choice_item *item,
                            size_t index, cui_tabs_options options,
                            const cui_tab_metrics *metrics) {
    if (options.vertical)
        return theme->control_height;
    const cui_tab_parts *parts =
        options.parts && index < options.parts_count ? &options.parts[index] : NULL;
    bool natural_parts =
        parts && ((parts->prefix && (!isfinite(parts->prefix_width) || parts->prefix_width <= 0)) ||
                  (parts->suffix && (!isfinite(parts->suffix_width) || parts->suffix_width <= 0)));
    float extent =
        options.render || natural_parts ? cui_bounds(ui, item->id).w
        : parts && parts->icon
            ? metrics->inner_height * 1.25f + 2 * metrics->padding_x
            : cui_measure_text(ui, item->label, theme->font_size).width + 2 * theme->padding;
    if (parts && !options.render && !natural_parts) {
        if (parts->prefix)
            extent += fmaxf(0, parts->prefix_width) + metrics->part_gap;
        if (parts->suffix)
            extent += fmaxf(0, parts->suffix_width) + metrics->part_gap;
    }
    if ((!parts || !parts->icon) && options.max_width > 0 && extent > options.max_width)
        extent = options.max_width;
    return extent;
}
typedef struct {
    const cui_theme *theme;
    bool selected;
    float size;
} cui_tab_overflow_icon;
static void cui_tab_overflow_artwork(cui_context *ui, const cui_choice_item *item, size_t index,
                                     void *user) {
    (void)index;
    const cui_tab_overflow_icon *icon = user;
    cui_color color = item->disabled   ? icon->theme->muted
                      : icon->selected ? icon->theme->on_primary
                                       : icon->theme->text;
    color.a = (uint8_t)lroundf(color.a * .75f);
    (void)cui_icon_named(ui, item->id ^ UINT64_C(0x23f0b6d57ae48c19), icon->theme,
                         cui_string("chevron-down"),
                         (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                            .custom_size = icon->size,
                                            .custom_color = true,
                                            .color = color});
}
static bool cui_tabs_common(cui_context *ui, cui_id container, const cui_theme *theme,
                            const cui_choice_item *items, size_t count, cui_id *selected,
                            cui_tabs_options options, cui_tab_panel_fn panel, void *user,
                            cui_tabs_handler handler, cui_tabs_event_result *events) {
    if (!theme || !items || !selected || !cui_design_valid(options.design_size) ||
        !cui_tabs_shadow_policy_valid(options))
        return false;
    cui_id viewport_id = container ^ UINT64_C(0x739b1b53a6cef421);
    const cui_theme *supplied_theme = theme;
    float density =
        isfinite(options.density) && options.density > 0 ? options.density : theme->font_size / 16;
    if (!isfinite(density) || density <= 0)
        density = 1;
    cui_tab_metrics metrics = cui_tab_size_metrics(options.variant, options.size, density);
    if (options.design_size) {
        cui_design_metrics d;
        if (!cui_design_resolve(options.design_size, density, &d))
            return false;
        metrics = (cui_tab_metrics){.height = d.control_height,
                                    .inner_height = d.control_height,
                                    .padding_x = d.padding_x,
                                    .font_size = d.font_size,
                                    .icon_size = d.indicator_size,
                                    .gap = d.gap,
                                    .part_gap = d.gap};
    }
    if (isfinite(options.height) && options.height > 0) {
        metrics.inner_height *= options.height / metrics.height;
        metrics.height = options.height;
    }
    float source_inner_radius =
        options.design_size
            ? fmaxf(0, (metrics.inner_height >= 24 ? theme->radius_lg : theme->radius) -
                           (metrics.inner_height >= 28 ? 3 : 2))
            : fmaxf(0, (options.size == CUI_TAB_MEDIUM || options.size == CUI_TAB_LARGE
                            ? theme->radius_lg
                            : theme->radius) -
                           (options.size == CUI_TAB_LARGE ? 3 : 2));
    cui_theme tab_theme = *theme;
    tab_theme.control_height = metrics.height;
    tab_theme.font_size = metrics.font_size;
    tab_theme.padding = metrics.padding_x;
    tab_theme.gap = metrics.gap;
    theme = &tab_theme;
    bool has_tail = options.suffix || options.overflow;
    float tail_width = 12 * density;
    if ((options.custom_last_empty_space_width || options.last_empty_space_width > 0) &&
        isfinite(options.last_empty_space_width) && options.last_empty_space_width >= 0)
        tail_width = options.last_empty_space_width * density;
    bool scrollable = options.scroll && isfinite(options.bar_extent) && options.bar_extent > 0;
    cui_scrollbar_style scrollbar_skin =
        options.scrollbar_style ? *options.scrollbar_style : theme->scrollbar;
    scrollbar_skin.reduced_motion |= options.reduced_motion;
    options.scrollbar_style = &scrollbar_skin;
    if (options.scrollbar_style->visibility == CUI_SCROLLBAR_HIDDEN)
        options.scrollbar = false;
    else if (options.scrollbar_style->visibility == CUI_SCROLLBAR_VISIBLE ||
             options.scrollbar_style->visibility == CUI_SCROLLBAR_AUTO_HIDE)
        options.scrollbar = true;
    if (scrollable) {
        options.scroll->receiving_id = viewport_id;
        options.scroll->viewport = cui_bounds(ui, viewport_id);
        float content = count > 1 ? (float)(count - 1) * theme->gap : 0;
        for (size_t i = 0; i < count; i++) {
            float extent = cui_tab_extent(ui, theme, &items[i], i, options, &metrics);
            if (isfinite(extent) && extent > 0)
                content += extent;
        }
        if (has_tail)
            content += tail_width + (count ? theme->gap : 0);
        options.scroll->maximum = fmaxf(0, content - options.bar_extent);
        options.scroll->scrollbar_id = options.scrollbar && options.scroll->maximum > 0
                                           ? container ^ UINT64_C(0x973a1e35f90453bc)
                                           : 0;
        options.scroll->scrollbar_bounds = cui_bounds(ui, options.scroll->scrollbar_id);

    } else
        options.scroll = NULL;
    bool changed;
    if (events) {
        *events = cui_base_tabs_events(ui, container, items, count, selected, options, handler);
        changed = events->changed;
    } else {
        changed = cui_base_tabs(ui, items, count, selected, options);
    }
    if (scrollable) {
        cui_tab_scroll_state *scroll = options.scroll;
        if (options.reveal_selected && scroll->last_selected != *selected && !scroll->reveal_id)
            scroll->reveal_id = *selected;
        scroll->last_selected = *selected;
        if (scroll->reveal_id) {
            float start = 0;
            bool found = false;
            for (size_t i = 0; i < count; i++) {
                float extent = cui_tab_extent(ui, theme, &items[i], i, options, &metrics);
                if (items[i].id == scroll->reveal_id) {
                    found = true;
                    if (extent > 0 && isfinite(extent)) {
                        if (cui_tab_scroll_reveal(scroll, start, extent, options.bar_extent))
                            cui_invalidate(ui);
                        scroll->reveal_id = 0;
                    }
                    break;
                }
                if (extent > 0 && isfinite(extent))
                    start += extent + theme->gap;
            }
            if (!found)
                scroll->reveal_id = 0;
        }
    }

    cui_begin_box(ui, container, (cui_box){.gap = supplied_theme->gap});
    cui_id list_id = container ^ UINT64_C(0xcda0312561f50cab);
    cui_interact(ui, list_id, CUI_ROLE_TABLIST, cui_string(""), options.disabled, false, 0);
    cui_set_semantic_details(
        ui, list_id,
        (cui_semantic_details){.orientation = options.vertical ? CUI_ORIENTATION_VERTICAL
                                                               : CUI_ORIENTATION_HORIZONTAL});
    cui_begin_box(
        ui, list_id,
        (cui_box){.row = !options.vertical,
                  .gap = 0,
                  .align_y = CUI_ALIGN_CENTER,
                  .background = options.variant == CUI_TAB_SEGMENTED ? theme->tab_bar_segmented
                                : options.variant == CUI_TAB_DEFAULT ? theme->tab_bar
                                                                     : (cui_color){0},
                  .radius = options.variant == CUI_TAB_SEGMENTED
                                ? (options.size == CUI_TAB_MEDIUM || options.size == CUI_TAB_LARGE
                                       ? theme->radius_lg
                                       : theme->radius)
                                : 0});
    if (options.variant == CUI_TAB_DEFAULT || options.variant == CUI_TAB_UNDERLINE)
        cui_set_underlay_ex(ui,
                            (cui_underlay_layout){.bounds = {0, 0, 0, 1},
                                                  .grow_width = true,
                                                  .vertical = CUI_ALIGN_END},
                            theme->border, (cui_corner_radii){0});
    if (options.prefix)
        options.prefix(ui, options.content_user);
    float bar_padding = options.vertical ? 0 : metrics.bar_padding_x;
    cui_begin_box(ui, container ^ UINT64_C(0xa7eb5ea7941c2230), (cui_box){.row = true});
    if (bar_padding > 0) {
        cui_begin_box(ui, 0, (cui_box){.width = bar_padding});
        cui_end_box(ui);
    }
    cui_begin_box(ui, container ^ UINT64_C(0xa7eb5ea7941c2240),
                  (cui_box){.row = !options.vertical, .gap = theme->gap});
    if (scrollable) {
        cui_interact(ui, viewport_id, CUI_ROLE_GROUP, cui_string("Tab viewport"), options.disabled,
                     false, 0);
        cui_set_tab_stop(ui, viewport_id, false);
        cui_set_semantic_details(
            ui, viewport_id,
            (cui_semantic_details){.parent = list_id,
                                   .scrollable = true,
                                   .scroll_x = options.vertical ? 0 : options.scroll->offset,
                                   .scroll_y = options.vertical ? options.scroll->offset : 0,
                                   .scroll_max_x = options.vertical ? 0 : options.scroll->maximum,
                                   .scroll_max_y = options.vertical ? options.scroll->maximum : 0});
        cui_begin_box(
            ui, viewport_id,
            (cui_box){.row = !options.vertical,
                      .gap = theme->gap,
                      .clip = true,
                      .width = options.vertical ? 0 : options.bar_extent,
                      .height = options.vertical ? options.bar_extent : theme->control_height,
                      .offset_x = options.vertical ? 0 : -options.scroll->offset,
                      .offset_y = options.vertical ? -options.scroll->offset : 0});
    }
    cui_tab_animation *animation =
        options.variant == CUI_TAB_SEGMENTED && options.shadow_policy == CUI_TAB_SHADOW_SOURCE_TAB
            ? NULL
            : options.animation;
    bool animated_variant = options.variant == CUI_TAB_PILL ||
                            options.variant == CUI_TAB_SEGMENTED ||
                            options.variant == CUI_TAB_UNDERLINE;
    if (animation) {
        animation->ready = false;
        size_t target = count;
        for (size_t i = 0; i < count; i++)
            if (items[i].id == *selected && ((!items[i].disabled && !options.disabled) ||
                                             (options.variant == CUI_TAB_SEGMENTED &&
                                              options.shadow_policy == CUI_TAB_SHADOW_SOURCE_BAR)))
                target = i;
        cui_id strip_id = scrollable ? viewport_id : container ^ UINT64_C(0xa7eb5ea7941c2240);
        cui_rect strip = cui_bounds(ui, strip_id);
        if (animated_variant && !options.vertical && target < count) {
            cui_rect bounds = cui_bounds(ui, items[target].id);
            bool previous_exists = false;
            for (size_t i = 0; i < count; i++)
                previous_exists |= items[i].id == animation->selected;
            bool had_target = animation->spring.initialized;
            if (animation->selected != *selected) {
                animation->foreground_elapsed = 0;
                animation->transitioning = had_target && previous_exists;
                animation->selected = *selected;
            } else if (animation->transitioning && isfinite(options.dt) && options.dt > 0) {
                animation->foreground_elapsed =
                    fminf(.2f, animation->foreground_elapsed + options.dt);
            }
            if (options.reduced_motion || !theme->spring_move.travel ||
                theme->spring_move.response == 0)
                animation->foreground_elapsed = .2f;
            if (animation->foreground_elapsed >= .2f)
                animation->transitioning = false;
            if (bounds.w > 0 && strip.w > 0) {
                float offset = scrollable ? options.scroll->offset : 0;
                float left = bounds.x - strip.x + animation->last_scroll_offset - offset;
                cui_tab_motion_step(&animation->spring, left, bounds.w, options.dt,
                                    theme->spring_move, options.reduced_motion);
                cui_rect indicator = {animation->spring.left, 0, fmaxf(0, animation->spring.width),
                                      metrics.height};
                float radius = metrics.height * .5f;
                cui_color color = theme->primary;
                if (options.variant == CUI_TAB_UNDERLINE) {
                    indicator.y = metrics.height - 2;
                    indicator.h = 2;
                    radius = 0;
                } else if (options.variant == CUI_TAB_SEGMENTED) {
                    indicator.y = (metrics.height - metrics.inner_height) * .5f;
                    indicator.h = metrics.inner_height;
                    radius =
                        fmaxf(0, (options.size == CUI_TAB_MEDIUM || options.size == CUI_TAB_LARGE
                                      ? theme->radius_lg
                                      : theme->radius) -
                                     (options.size == CUI_TAB_LARGE ? 3 : 2));
                    if (options.shadow_policy == CUI_TAB_SHADOW_SOURCE_BAR)
                        radius = source_inner_radius;
                    color = theme->background;
                }
                if (options.variant == CUI_TAB_SEGMENTED &&
                    options.shadow_policy == CUI_TAB_SHADOW_SOURCE_BAR) {
                    cui_tab_raised_paint paint = {indicator, radius, color};
                    animation->ready = cui_set_prepaint(
                        ui, (cui_prepaint_options){.callback = cui_tab_raised_prepaint,
                                                   .payload = &paint,
                                                   .payload_bytes = sizeof(paint),
                                                   .command_budget = 5});
                } else
                    animation->ready = cui_set_underlay(
                        ui, indicator, color, (cui_corner_radii){radius, radius, radius, radius});
                if (animation->spring.left != left || animation->spring.width != bounds.w ||
                    animation->spring.left_velocity != 0 || animation->spring.width_velocity != 0 ||
                    (options.variant == CUI_TAB_PILL && animation->transitioning))
                    cui_invalidate(ui);
            }
            if (options.variant == CUI_TAB_SEGMENTED &&
                options.shadow_policy == CUI_TAB_SHADOW_SOURCE_BAR && !animation->ready)
                cui_invalidate(ui);
            cui_track_bounds(ui, items[target].id, &animation->target_bounds);
        }
        cui_track_bounds(ui, strip_id, &animation->strip_bounds);
        animation->last_scroll_offset = scrollable ? options.scroll->offset : 0;
    }
    size_t active = count;
    for (size_t i = 0; i < count; ++i) {
        cui_choice_draw_ex(ui, &items[i], theme, items[i].id == *selected,
                           options.disabled || items[i].disabled, options.variant, options.render,
                           i, options.render_user, options.max_width,
                           options.parts && i < options.parts_count ? &options.parts[i] : NULL,
                           &metrics, animation, options.shadow_policy, source_inner_radius);
        cui_set_semantic_details(
            ui, items[i].id,
            (cui_semantic_details){.selected = items[i].id == *selected,
                                   .controls = items[i].id ^ UINT64_C(0x691a17db80e22f45),
                                   .parent = list_id});
        if (items[i].id == *selected && !items[i].disabled)
            active = i;
    }
    if (has_tail) {
        cui_begin_box(ui, container ^ UINT64_C(0x4c2c9f0875e4b4c0),
                      (cui_box){.width = options.vertical ? 0 : tail_width,
                                .height = options.vertical ? tail_width : metrics.height,
                                .fixed_width = !options.vertical,
                                .fixed_height = options.vertical,
                                .clip = true});
        if (options.last_empty_space)
            options.last_empty_space(ui, options.last_empty_space_user);
        cui_end_box(ui);
    }
    if (options.drag && options.drag->active && options.drag->drop_valid) {
        cui_rect bounds = {0};
        if (options.drag->before)
            bounds = cui_bounds(ui, options.drag->before);
        else if (count) {
            bounds = cui_bounds(ui, items[count - 1].id);
            if (options.vertical)
                bounds.y += bounds.h;
            else
                bounds.x += bounds.w;
        }
        bool visible = bounds.w > 0 && bounds.h > 0;
        if (scrollable) {
            cui_rect viewport = options.scroll->viewport;
            visible &= bounds.x >= viewport.x && bounds.x <= viewport.x + viewport.w &&
                       bounds.y >= viewport.y && bounds.y <= viewport.y + viewport.h;
        }
        if (visible) {
            cui_rect marker = options.vertical ? (cui_rect){bounds.x, bounds.y, bounds.w, 2}
                                               : (cui_rect){bounds.x, bounds.y, 2, bounds.h};
            cui_begin_portal(ui, container ^ UINT64_C(0x5df27b10b325643e),
                             (cui_box){.background = theme->primary}, marker, 90);
            cui_end_box(ui);
        }
    }
    if (scrollable)
        cui_end_box(ui);
    cui_end_box(ui);
    if (bar_padding > 0) {
        cui_begin_box(ui, 0, (cui_box){.width = bar_padding});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    if (options.suffix)
        options.suffix(ui, options.content_user);
    if (options.overflow && options.overflow->visible) {
        cui_tab_overflow *overflow = options.overflow;
        cui_choice_item trigger = {.id = overflow->trigger,
                                   .label = {"All tabs", 8},
                                   .disabled =
                                       options.disabled || overflow->options.popup.disabled};
        float side = fmaxf(32, metrics.height);
        cui_tab_overflow_icon icon = {theme, overflow->menu.overlay.open,
                                      fmaxf(16 * density, metrics.icon_size)};
        cui_begin_box(ui, trigger.id,
                      (cui_box){.width = side,
                                .height = side,
                                .fixed_width = true,
                                .fixed_height = true,
                                .align_x = CUI_ALIGN_CENTER,
                                .align_y = CUI_ALIGN_CENTER,
                                .radius = theme->radius,
                                .background = overflow->menu.overlay.open ? theme->accent
                                                                          : theme->secondary});
        cui_tab_overflow_artwork(ui, &trigger, 0, &icon);
        cui_end_box(ui);
        cui_set_semantic_details(ui, overflow->trigger, (cui_semantic_details){.parent = list_id});
        overflow->menu.row_height = theme->control_height;
        overflow->menu.padding = theme->padding;
        if (overflow->menu.overlay.open)
            cui_choice_menu_level(ui, theme, overflow->entries, count, &overflow->menu, 0,
                                  cui_bounds(ui, overflow->trigger), overflow->options,
                                  cui_string("All tabs"));
    }

    cui_end_box(ui);
    if (scrollable && options.scroll->scrollbar_id) {
        cui_tab_scroll_state *scroll = options.scroll;
        const cui_scrollbar_style *style = options.scrollbar_style;
        float opacity = cui_scrollbar_opacity(ui, viewport_id, scroll->scrollbar_id, style,
                                              scroll->offset, scroll->scrollbar_dragging);
        cui_rect viewport = scroll->viewport;
        float thickness = isfinite(style->thickness) && style->thickness > 0 ? style->thickness : 6;
        thickness = fminf(thickness, fminf(viewport.w, viewport.h) * .5f);
        cui_rect track =
            options.vertical
                ? (cui_rect){viewport.x + viewport.w - thickness, viewport.y, thickness, viewport.h}
                : (cui_rect){viewport.x, viewport.y + viewport.h - thickness, viewport.w,
                             thickness};
        if (opacity > 0 && track.w > 0 && track.h > 0) {
            float length = options.vertical ? track.h : track.w;
            float thumb_length = length * (float)((double)options.bar_extent /
                                                  ((double)options.bar_extent + scroll->maximum));
            float minimum =
                isfinite(style->min_thumb) && style->min_thumb > 0 ? style->min_thumb : 16;
            thumb_length = fminf(length, fmaxf(minimum, thumb_length));
            float thumb_start = (length - thumb_length) * scroll->offset / scroll->maximum;
            cui_rect thumb =
                options.vertical
                    ? (cui_rect){track.x, track.y + thumb_start, track.w, thumb_length}
                    : (cui_rect){track.x + thumb_start, track.y, thumb_length, track.h};
            cui_begin_portal(
                ui, scroll->scrollbar_id,
                (cui_box){.background = style->no_track             ? (cui_color){0}
                                        : style->custom_track_color ? style->track_color
                                                                    : theme->border,
                          .radius = isfinite(style->track_radius) && style->track_radius > 0
                                        ? style->track_radius
                                    : style->visibility == CUI_SCROLLBAR_INHERIT ? 3
                                                                                 : 0,
                          .custom_opacity = true,
                          .opacity = opacity,
                          .border = theme->focus,
                          .border_width =
                              !style->no_track && cui_focused(ui) == scroll->scrollbar_id ? 1 : 0},
                track, 1);
            cui_interact(ui, scroll->scrollbar_id, CUI_ROLE_SCROLLBAR, cui_string("Tab scrollbar"),
                         options.disabled, false, scroll->offset);
            cui_end_box(ui);
            cui_begin_portal(
                ui, scroll->scrollbar_id ^ UINT64_C(0x247d15723f61c98b),
                (cui_box){.background = style->custom_thumb_color ? style->thumb_color
                                        : options.disabled        ? theme->muted
                                                                  : theme->primary,
                          .radius = isfinite(style->thumb_radius) && style->thumb_radius > 0
                                        ? style->thumb_radius
                                    : style->visibility == CUI_SCROLLBAR_INHERIT ? 3
                                                                                 : 0,
                          .custom_opacity = true,
                          .opacity = opacity},
                thumb, 2);
            cui_end_box(ui);
        }
        cui_update_semantic(ui, scroll->scrollbar_id, false, scroll->offset);
        cui_set_semantic_details(
            ui, scroll->scrollbar_id,
            (cui_semantic_details){.orientation = options.vertical ? CUI_ORIENTATION_VERTICAL
                                                                   : CUI_ORIENTATION_HORIZONTAL,
                                   .minimum = 0,
                                   .maximum = scroll->maximum,
                                   .controls = viewport_id,
                                   .parent = list_id,
                                   .scrollable = true,
                                   .scroll_x = options.vertical ? 0 : scroll->offset,
                                   .scroll_y = options.vertical ? scroll->offset : 0,
                                   .scroll_max_x = options.vertical ? 0 : scroll->maximum,
                                   .scroll_max_y = options.vertical ? scroll->maximum : 0});
    }
    theme = supplied_theme;
    if (active != count && panel) {
        cui_id panel_id = items[active].id ^ UINT64_C(0x691a17db80e22f45);
        cui_interact(ui, panel_id, CUI_ROLE_TABPANEL, items[active].label, false, false, 0);
        cui_set_semantic_details(
            ui, panel_id,
            (cui_semantic_details){.labelled_by = items[active].id, .parent = container});
        cui_begin_box(ui, items[active].id ^ UINT64_C(0x691a17db80e22f45),
                      (cui_box){.padding = theme->padding, .gap = theme->gap});
        panel(ui, active, user);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return changed;
}
bool cui_tabs(cui_context *ui, cui_id container, const cui_theme *theme,
              const cui_choice_item *items, size_t count, cui_id *selected,
              cui_tabs_options options, cui_tab_panel_fn panel, void *user) {
    return cui_tabs_common(ui, container, theme, items, count, selected, options, panel, user,
                           (cui_tabs_handler){0}, NULL);
}
cui_tabs_event_result cui_tabs_events(cui_context *ui, cui_id container, const cui_theme *theme,
                                      const cui_choice_item *items, size_t count, cui_id *selected,
                                      cui_tabs_options options, cui_tab_panel_fn panel,
                                      void *panel_user, cui_tabs_handler handler) {
    cui_tabs_event_result result = {0};
    if (!ui || !container || !selected)
        return result;
    result.changed = cui_tabs_common(ui, container, theme, items, count, selected, options, panel,
                                     panel_user, handler, &result);
    return result;
}

static bool cui_toggle_group_common(cui_context *ui, cui_id container, const cui_theme *theme,
                                    const cui_choice_item *items, size_t count, bool *selected,
                                    cui_toggle_group_options options,
                                    const cui_choice_item_handler *handler,
                                    cui_choice_items_event_result *result) {
    if (handler && (!ui || !container))
        return false;
    if (!theme || !items || !selected)
        return false;
    bool changed;
    if (handler) {
        *result =
            cui_base_toggle_group_events(ui, container, items, count, selected, options, *handler);
        changed = result->legacy_result;
    } else
        changed = cui_base_toggle_group(ui, items, count, selected, options);
    cui_interact(ui, container, CUI_ROLE_GROUP, cui_string(""), options.disabled, false, 0);
    cui_set_semantic_details(
        ui, container,
        (cui_semantic_details){.orientation = options.vertical ? CUI_ORIENTATION_VERTICAL
                                                               : CUI_ORIENTATION_HORIZONTAL});
    cui_begin_box(ui, container, (cui_box){.row = !options.vertical, .gap = theme->gap});
    for (size_t i = 0; i < count; ++i)
        cui_toggle_paint(ui, items[i].id, theme, items[i].label, selected[i],
                         (cui_toggle_options){.disabled = options.disabled || items[i].disabled});
    cui_end_box(ui);
    return changed;
}
bool cui_toggle_group(cui_context *ui, cui_id container, const cui_theme *theme,
                      const cui_choice_item *items, size_t count, bool *selected,
                      cui_toggle_group_options options) {
    return cui_toggle_group_common(ui, container, theme, items, count, selected, options, NULL,
                                   NULL);
}
cui_choice_items_event_result
cui_toggle_group_events(cui_context *ui, cui_id container, const cui_theme *theme,
                        const cui_choice_item *items, size_t count, bool *selected,
                        cui_toggle_group_options options, cui_choice_item_handler handler) {
    cui_choice_items_event_result result = {0};
    (void)cui_toggle_group_common(ui, container, theme, items, count, selected, options, &handler,
                                  &result);
    return result;
}

cui_list_metrics cui_list_default_metrics(float density) {
    if (!isfinite(density) || density <= 0)
        density = 1;
    return (cui_list_metrics){.height = (16 * 1.618034f + 8) * density,
                              .font_size = 16 * density,
                              .line_height = 16 * 1.618034f * density,
                              .padding_x = 12 * density,
                              .padding_y = 4 * density,
                              .gap = 4 * density,
                              .check_size = 14 * density,
                              .check_width = 20 * density,
                              .density = density};
}
cui_list_metrics cui_searchable_list_size_metrics(cui_list_size size, float custom_size,
                                                  float density) {
    if (!isfinite(density) || density <= 0)
        density = 1;
    float font = size == CUI_LIST_XSMALL ? 12 : size == CUI_LIST_LARGE ? 16 : 14;
    if (size == CUI_LIST_CUSTOM && isfinite(custom_size) && custom_size > 0)
        font = custom_size * .875f;
    float padding_x = size == CUI_LIST_XSMALL   ? 1
                      : size == CUI_LIST_LARGE  ? 9
                      : size == CUI_LIST_MEDIUM ? 7
                                                : 5;
    float padding_y = size == CUI_LIST_SMALL ? 2 : size == CUI_LIST_LARGE ? 8 : 4;
    return (cui_list_metrics){.height = (font * 1.618034f + 2 * padding_y) * density,
                              .font_size = font * density,
                              .line_height = font * 1.618034f * density,
                              .padding_x = padding_x * density,
                              .padding_y = padding_y * density,
                              .gap = 4 * density,
                              .check_size = 12 * density,
                              .check_width = 12 * density,
                              .density = density};
}
static bool cui_choice_design_theme(const cui_theme *theme, cui_design_size size, cui_theme *out) {
    *out = *theme;
    if (size == CUI_DESIGN_INHERIT)
        return true;
    cui_design_metrics m;
    if (!cui_design_measure(size, theme->font_size / 16, &m))
        return false;
    out->control_height = m.control_height;
    out->font_size = m.font_size;
    out->padding = m.padding_x;
    out->gap = m.gap;
    return true;
}
static void cui_choice_design_rows(cui_list_metrics *metrics, cui_design_size size) {
    cui_design_metrics m;
    if (size != CUI_DESIGN_INHERIT && cui_design_measure(size, metrics->density, &m)) {
        metrics->height = m.control_height;
        metrics->font_size = m.font_size;
        metrics->line_height = m.line_height;
        metrics->padding_x = m.padding_x;
        metrics->padding_y = m.padding_y;
        metrics->gap = m.gap;
        metrics->check_size = metrics->check_width = m.indicator_size;
    }
}
static cui_list_metrics cui_choice_list_metrics(const cui_theme *theme, cui_list_options options) {
    cui_list_metrics metrics = cui_list_default_metrics(
        isfinite(options.density) && options.density > 0 ? options.density : theme->font_size / 16);
    cui_choice_design_rows(&metrics, options.design_size);
    if (isfinite(options.height) && options.height > 0)
        metrics.height = options.height;
    return metrics;
}
static cui_list_metrics cui_choice_search_metrics(const cui_theme *theme,
                                                  cui_choice_popup_options options) {
    cui_list_metrics metrics = cui_searchable_list_size_metrics(
        options.size, options.custom_size,
        isfinite(options.density) && options.density > 0 ? options.density : theme->font_size / 16);
    cui_choice_design_rows(&metrics, options.design_size);
    if (isfinite(options.row_height) && options.row_height > 0)
        metrics.height = options.row_height;
    return metrics;
}
static void cui_choice_paint_list(cui_context *ui, cui_id container, const cui_theme *theme,
                                  const cui_choice_item *items, size_t count, cui_id *selected,
                                  cui_list_state *state, cui_list_options options,
                                  const bool *multiple_selected, bool searchable,
                                  const cui_list_metrics *metrics) {

    cui_interact(ui, container, options.menu ? CUI_ROLE_MENU : CUI_ROLE_LISTBOX, cui_string(""),
                 options.disabled, false, 0);
    cui_set_semantic_details(
        ui, container,
        (cui_semantic_details){.multiselectable = multiple_selected != NULL,
                               .orientation = CUI_ORIENTATION_VERTICAL,
                               .scrollable = options.visible_count && count > options.visible_count,
                               .scroll_y = state->first_visible * metrics->height,
                               .scroll_max_y =
                                   options.visible_count && count > options.visible_count
                                       ? (count - options.visible_count) * metrics->height
                                       : 0});
    cui_scroll_options bars =
        cui_scroll_resolve(theme, (cui_scroll_options){.vertical = true,
                                                       .disabled = options.disabled,
                                                       .wheel_step = metrics->height,
                                                       .scrollbar_style = options.scrollbar_style});
    bool overlay = bars.scrollbar_style &&
                   (bars.scrollbar_style->visibility == CUI_SCROLLBAR_VISIBLE ||
                    bars.scrollbar_style->visibility == CUI_SCROLLBAR_AUTO_HIDE) &&
                   options.visible_count && isfinite(metrics->height) && metrics->height > 0;
    size_t visible = options.visible_count < count ? options.visible_count : count;
    float width = cui_bounds(ui, container).w;
    cui_rect bar_viewport = {0, 0, width, visible * metrics->height};
    cui_size bar_content = {width, count * metrics->height};
    if (overlay && count > visible) {
        float x = 0, y = state->first_visible * metrics->height;
        if (width <= 0)
            cui_invalidate(ui);
        if (cui_base_scrollbar_overlay(ui, container, &x, &y, bar_viewport, bar_content, bars)) {
            size_t first = (size_t)floorf(y / metrics->height);
            state->first_visible = first < count - visible ? first : count - visible;
            size_t active = count;
            for (size_t i = 0; i < count; i++)
                if (items[i].id == state->active)
                    active = i;
            if (active < state->first_visible || active >= state->first_visible + visible) {
                bool focused = cui_focused(ui) == state->active;
                state->active = 0;
                for (size_t i = state->first_visible; i < state->first_visible + visible; i++)
                    if (!items[i].disabled) {
                        state->active = items[i].id;
                        break;
                    }
                if (focused)
                    cui_focus(ui, state->active);
            }
        }
    }
    cui_begin_box(
        ui, container,
        (cui_box){.width = -1, .gap = 0, .height = overlay ? bar_viewport.h : 0, .clip = overlay});
    size_t begin = options.visible_count ? state->first_visible : 0;
    size_t length = options.visible_count ? options.visible_count : count;
    if (begin > count)
        begin = count;
    if (length > count - begin)
        length = count - begin;
    for (size_t i = begin; i < begin + length; i++) {
        bool active = state->active == items[i].id || state->secondary_active == items[i].id;
        bool disabled = options.disabled || items[i].disabled;
        bool checked = multiple_selected ? multiple_selected[i] : *selected == items[i].id;
        cui_color background = {0};
        if (!disabled) {
            if (state->active == items[i].id)
                background = theme->list_active_highlight ? theme->list_active : theme->accent;
            else if (!active && !checked && cui_hovered(ui, items[i].id))
                background = theme->list_hover;
        }
        if (searchable || options.menu) {
            background = (cui_color){0};
            if (active && (searchable || !disabled))
                background = theme->accent;
            else if (!disabled && cui_hovered(ui, items[i].id)) {
                background = theme->accent;
                if (searchable)
                    background.a = (uint8_t)lroundf(background.a * .7f);
            }
        }
        cui_box box = {.height = metrics->height,
                       .width = -1,
                       .padding = 0,
                       .row = true,
                       .align_y = CUI_ALIGN_CENTER,
                       .gap = 0,
                       .radius = searchable ? theme->radius : 0,
                       .background = background,
                       .border = !searchable && !options.menu && !disabled &&
                                         state->secondary_active == items[i].id
                                     ? theme->selection
                                     : theme->focus,
                       .border_width = !searchable && !options.menu && !disabled &&
                                               state->secondary_active == items[i].id
                                           ? 1
                                           : 0};
        cui_begin_box(ui, items[i].id, box);
        cui_begin_box(ui, 0, (cui_box){.width = metrics->padding_x});
        cui_end_box(ui);
        cui_begin_box(ui, items[i].id ^ UINT64_C(0x9e3f5d2a91bca002),
                      (cui_box){.width = -1, .clip = true});
        if (options.render)
            options.render(ui, &items[i], i, options.render_user);
        else
            cui_text_with_line_height(ui, items[i].label, metrics->font_size,
                                      disabled                 ? theme->muted
                                      : searchable             ? theme->text
                                      : active && options.menu ? theme->accent_foreground
                                      : options.menu && cui_hovered(ui, items[i].id)
                                          ? theme->accent_foreground
                                          : theme->text,
                                      metrics->line_height);
        cui_end_box(ui);
        if (searchable || multiple_selected) {
            cui_begin_box(ui, 0, (cui_box){.width = metrics->gap});
            cui_end_box(ui);
            cui_begin_box(ui, items[i].id ^ UINT64_C(0x4b705ad687e1a963),
                          (cui_box){.width = metrics->check_width,
                                    .height = metrics->check_size,
                                    .align_x = CUI_ALIGN_CENTER,
                                    .align_y = CUI_ALIGN_CENTER});
            if (checked)
                cui_checkmark(ui, metrics->check_size, searchable ? theme->text : theme->muted);
            cui_end_box(ui);
        }
        cui_begin_box(ui, 0, (cui_box){.width = metrics->padding_x});
        cui_end_box(ui);
        cui_end_box(ui);
        cui_update_semantic(ui, items[i].id, checked, (float)i);
        cui_set_semantic_details(ui, items[i].id,
                                 (cui_semantic_details){.selected = checked, .parent = container});
    }
    if (overlay)
        cui_scrollbar_overlay_paint(ui, container, theme, 0, state->first_visible * metrics->height,
                                    bar_viewport, bar_content, bars);
    cui_end_box(ui);
}
static bool cui_list_common(cui_context *ui, cui_id container, const cui_theme *theme,
                            const cui_choice_item *items, size_t count, cui_id *selected,
                            cui_list_state *state, cui_list_options options,
                            const cui_selection_handler *handler, cui_list_event_result *result) {
    if (handler && (!ui || !container))
        return false;
    if (!theme || (!items && count) || !selected || !state)
        return false;
    cui_theme validation_theme;
    if (!cui_choice_design_theme(theme, options.design_size, &validation_theme))
        return false;
    cui_list_metrics metrics = cui_choice_list_metrics(theme, options);
    options.scroll_bounds = cui_bounds(ui, container);
    options.scroll_target = container;
    bool activated;
    if (handler) {
        *result =
            cui_base_list_events(ui, container, items, count, selected, state, options, *handler);
        activated = result->legacy_activated;
    } else
        activated = cui_base_list(ui, items, count, selected, state, options);
    cui_choice_paint_list(ui, container, theme, items, count, selected, state, options, NULL, false,
                          &metrics);
    return activated;
}
bool cui_list(cui_context *ui, cui_id container, const cui_theme *theme,
              const cui_choice_item *items, size_t count, cui_id *selected, cui_list_state *state,
              cui_list_options options) {
    return cui_list_common(ui, container, theme, items, count, selected, state, options, NULL,
                           NULL);
}
cui_list_event_result cui_list_events(cui_context *ui, cui_id container, const cui_theme *theme,
                                      const cui_choice_item *items, size_t count, cui_id *selected,
                                      cui_list_state *state, cui_list_options options,
                                      cui_selection_handler handler) {
    cui_list_event_result result = {0};
    (void)cui_list_common(ui, container, theme, items, count, selected, state, options, &handler,
                          &result);
    return result;
}

static bool cui_choice_popup_render(cui_context *ui, cui_id trigger, const cui_theme *theme,
                                    cui_str name, cui_str placeholder, const cui_choice_item *items,
                                    size_t count, cui_id *selected, cui_choice_popup *state,
                                    cui_choice_popup_options options, bool menu,
                                    cui_selection_handler handler,
                                    cui_selection_event_result *observed,
                                    cui_menu_command_handler commands,
                                    cui_menu_command_result *command_result) {
    if (!theme || !selected || !state || !state->overlay.id || (!items && count) ||
        !isfinite(options.viewport.x) || !isfinite(options.viewport.y) ||
        !isfinite(options.viewport.w) || !isfinite(options.viewport.h) || options.viewport.w <= 0 ||
        options.viewport.h <= 0 || !isfinite(theme->control_height) || theme->control_height <= 0 ||
        !isfinite(theme->padding) || theme->padding < 0 || theme->padding > options.viewport.h / 2)
        return false;
    cui_theme trigger_theme;
    if (!cui_choice_design_theme(theme, options.design_size, &trigger_theme))
        return false;
    if (observed) {
        observed->valid = true;
        observed->submitted = cui_frame_is_submitting(ui) && cui_frame_sequence(ui) != 0;
    }
    if (command_result) {
        command_result->valid = true;
        command_result->submitted = cui_frame_is_submitting(ui) && cui_frame_sequence(ui) != 0;
    }
    cui_base_choice_popup_trigger(ui, trigger, name, options.disabled, items, count, *selected,
                                  state);
    cui_str display = menu ? name : placeholder;
    if (!menu)
        for (size_t i = 0; i < count; i++)
            if (items[i].id == *selected)
                display = items[i].label;
    cui_set_semantic_details(ui, trigger,
                             (cui_semantic_details){.expanded = state->overlay.open,
                                                    .controls = state->overlay.id,
                                                    .description = display});
    cui_choice_item trigger_item = {trigger, display, options.disabled};
    cui_choice_draw(ui, &trigger_item, &trigger_theme, false, options.disabled, CUI_TAB_DEFAULT);
    if (!state->overlay.open)
        return false;
    cui_list_metrics metrics = cui_choice_search_metrics(theme, options);
    cui_theme row_theme = *theme;
    row_theme.control_height = metrics.height;
    row_theme.padding = 4 * metrics.density;
    theme = &row_theme;
    float width = options.width > 0 && isfinite(options.width) ? options.width : 240;
    if (width > options.viewport.w)
        width = options.viewport.w;
    float max_height = isfinite(options.max_height) && options.max_height > 0
                           ? options.max_height
                           : 320 * metrics.density;
    float available = fminf(options.viewport.h - 8, max_height) - 2 * theme->padding;
    size_t rows = count;
    if (available / theme->control_height < (float)rows) {
        rows = available > theme->control_height ? (size_t)(available / theme->control_height) : 1;
    }
    if (rows > 4096)
        rows = 4096;
    float height = 2 * theme->padding + (rows ? rows : 1) * theme->control_height;
    cui_popup_result placement = cui_popup_place(cui_bounds(ui, trigger), (cui_size){width, height},
                                                 options.viewport, options.side, 4);
    cui_overlay_set_bounds(&state->overlay, placement.bounds);
    if (!cui_overlay_begin(ui, &state->overlay))
        return false;
    cui_list_options list_options = {.scrollbar_style = options.scrollbar_style,
                                     .disabled = options.disabled,
                                     .menu = menu,
                                     .dt = options.dt,
                                     .visible_begin = state->list.first_visible,
                                     .visible_count = rows,
                                     .render = options.render,
                                     .render_user = options.render_user,
                                     .scroll_bounds = cui_bounds(ui, state->overlay.id),
                                     .scroll_scope = state->overlay.id,
                                     .scroll_target =
                                         state->overlay.id ^ UINT64_C(0xd1bc905ee4216f11)};
    cui_id old = *selected;
    bool activated =
        count
            ? (command_result ? cui_choice_private_list_commands(ui, items, count, selected,
                                                                 &state->list, list_options,
                                                                 trigger, commands, command_result)
               : observed ? cui_choice_private_list_events(ui, items, count, selected, &state->list,
                                                           list_options, trigger, handler, observed)
                          : cui_base_list(ui, items, count, selected, &state->list, list_options))
            : false;
    if (activated) {
        cui_id scope = state->overlay.id;
        cui_overlay_close(ui, &state->overlay);
        cui_dismiss_focus_scope(ui, scope, trigger);
        cui_overlay_end(ui);
        cui_update_semantic(ui, trigger, false, 0);
        cui_set_semantic_details(ui, trigger,
                                 (cui_semantic_details){.controls = scope, .description = display});
        return menu || old != *selected;
    }
    cui_begin_portal(ui, state->overlay.id,
                     (cui_box){.padding = theme->padding,
                               .clip = true,
                               .background = theme->popover,
                               .border = theme->border,
                               .border_width = 1,
                               .radius = theme->radius},
                     placement.bounds, 100);
    cui_interact(ui, state->overlay.id, menu ? CUI_ROLE_MENU : CUI_ROLE_LISTBOX, name, false, false,
                 0);
    cui_set_semantic_details(
        ui, state->overlay.id,
        (cui_semantic_details){.labelled_by = trigger,
                               .orientation = CUI_ORIENTATION_VERTICAL,
                               .scrollable = count > rows,
                               .scroll_y = state->list.first_visible * theme->control_height,
                               .scroll_max_y =
                                   count > rows ? (count - rows) * theme->control_height : 0});
    if (count)
        cui_choice_paint_list(ui, state->overlay.id ^ UINT64_C(0xd1bc905ee4216f11), theme, items,
                              count, selected, &state->list, list_options, NULL, !menu, &metrics);
    else
        cui_text(ui, cui_string("No options"), theme->font_size, theme->muted);
    cui_end_box(ui);
    cui_overlay_end(ui);
    return false;
}
bool cui_select(cui_context *ui, cui_id trigger, const cui_theme *theme, cui_str name,
                cui_str placeholder, const cui_choice_item *items, size_t count, cui_id *selected,
                cui_choice_popup *state, cui_choice_popup_options options) {
    return cui_choice_popup_render(ui, trigger, theme, name, placeholder, items, count, selected,
                                   state, options, false, (cui_selection_handler){0}, NULL,
                                   (cui_menu_command_handler){0}, NULL);
}
cui_select_event_result cui_select_events(cui_context *ui, cui_id trigger, const cui_theme *theme,
                                          cui_str name, cui_str placeholder,
                                          const cui_choice_item *items, size_t count,
                                          cui_id *selected, cui_choice_popup *state,
                                          cui_choice_popup_options options,
                                          cui_selection_handler handler) {
    cui_select_event_result result;
    memset(&result, 0, sizeof(result));
    if (!ui || !trigger)
        return result;
    result.legacy_changed = cui_choice_popup_render(
        ui, trigger, theme, name, placeholder, items, count, selected, state, options, false,
        handler, &result.events, (cui_menu_command_handler){0}, NULL);
    return result;
}

bool cui_menu(cui_context *ui, cui_id trigger, const cui_theme *theme, cui_str name,
              const cui_choice_item *items, size_t count, cui_id *action, cui_choice_popup *state,
              cui_choice_popup_options options) {
    return cui_choice_popup_render(ui, trigger, theme, name, name, items, count, action, state,
                                   options, true, (cui_selection_handler){0}, NULL,
                                   (cui_menu_command_handler){0}, NULL);
}

cui_menu_command_result cui_menu_events(cui_context *ui, cui_id trigger, const cui_theme *theme,
                                        cui_str name, const cui_choice_item *items, size_t count,
                                        cui_id *action, cui_choice_popup *state,
                                        cui_choice_popup_options options,
                                        cui_menu_command_handler handler) {
    cui_menu_command_result result;
    memset(&result, 0, sizeof(result));
    if (!ui || !trigger)
        return result;
    result.legacy_activated =
        cui_choice_popup_render(ui, trigger, theme, name, name, items, count, action, state,
                                options, true, (cui_selection_handler){0}, NULL, handler, &result);
    return result;
}

static void cui_choice_paint_query(cui_context *ui, cui_id input, const cui_theme *theme,
                                   cui_str name, cui_edit *query, cui_choice_popup *state,
                                   bool disabled, cui_edit_result error,
                                   cui_design_size design_size) {
    cui_input_state paint_state = {.behavior = state->query_behavior,
                                   .scroll_x = state->query_scroll_x};
    cui_input_paint(ui, input, theme, name, query, &paint_state,
                    (cui_input_options){.disabled = disabled,
                                        .invalid = error != CUI_EDIT_OK,
                                        .design_size = design_size});
    state->query_scroll_x = paint_state.scroll_x;
}
static cui_combobox_result
cui_combobox_render(cui_context *ui, cui_id input, const cui_theme *theme, cui_str name,
                    cui_edit *query, const cui_choice_item *items, size_t count,
                    cui_choice_item *filtered, size_t capacity, cui_id *selected,
                    cui_choice_popup *state, cui_choice_popup_options options,
                    cui_selection_handler handler, cui_selection_event_result *observed) {
    cui_combobox_result result = {.error = CUI_EDIT_OK};
    if (!theme || !query || !selected || !state || !state->overlay.id || (!items && count) ||
        (!filtered && capacity) || capacity < count || cui_edit_get_state(query).multiline ||
        !isfinite(options.viewport.w) || !isfinite(options.viewport.h) || options.viewport.w <= 0 ||
        options.viewport.h <= 0 || !isfinite(options.viewport.x) || !isfinite(options.viewport.y) ||
        !isfinite(theme->control_height) || theme->control_height <= 0 ||
        !isfinite(theme->padding) || theme->padding < 0 ||
        theme->padding > options.viewport.h / 2) {
        result.error = capacity < count ? CUI_EDIT_CAPACITY : CUI_EDIT_INVALID_ARGUMENT;
        return result;
    }
    state->query_viewport = cui_bounds(ui, cui_input_viewport_id(input));
    cui_input_metrics input_metrics;
    if (!cui_input_measure((cui_input_options){.design_size = options.design_size},
                           theme->font_size, &input_metrics)) {
        result.error = CUI_EDIT_INVALID_ARGUMENT;
        return result;
    }
    state->query_font_size = input_metrics.font_size;
    if (observed) {
        cui_combobox_event_result base =
            cui_base_combobox_events(ui, input, name, query, items, count, filtered, capacity,
                                     selected, state, options.disabled, handler);
        result = base.legacy;
        *observed = base.events;
    } else
        result = cui_base_combobox(ui, input, name, query, items, count, filtered, capacity,
                                   selected, state, options.disabled);
    cui_choice_paint_query(ui, input, theme, name, query, state, options.disabled, result.error,
                           options.design_size);
    cui_str text = cui_edit_get_text(query);
    if (!state->overlay.open)
        return result;
    cui_list_metrics metrics = cui_choice_search_metrics(theme, options);
    cui_theme row_theme = *theme;
    row_theme.control_height = metrics.height;
    row_theme.padding = 4 * metrics.density;
    theme = &row_theme;
    size_t matched = result.matching_count;
    float width = options.width > 0 && isfinite(options.width) ? options.width : 280;
    if (width > options.viewport.w)
        width = options.viewport.w;
    float max_height = isfinite(options.max_height) && options.max_height > 0
                           ? options.max_height
                           : 320 * metrics.density;
    float available = fminf(options.viewport.h - 8, max_height) - 2 * theme->padding;
    size_t rows = matched;
    if (available / theme->control_height < (float)rows)
        rows = available > theme->control_height ? (size_t)(available / theme->control_height) : 1;
    if (rows > 4096)
        rows = 4096;
    state->visible_rows = rows;
    cui_popup_result placement = cui_popup_place(
        cui_bounds(ui, input),
        (cui_size){width, 2 * theme->padding + (rows ? rows : 1) * theme->control_height},
        options.viewport, options.side, 4);
    cui_overlay_set_bounds(&state->overlay, placement.bounds);
    if (!cui_overlay_begin(ui, &state->overlay))
        return result;
    cui_list_options list_options = {.scrollbar_style = options.scrollbar_style,
                                     .dt = options.dt,
                                     .visible_begin = state->list.first_visible,
                                     .visible_count = rows,
                                     .render = options.render,
                                     .render_user = options.render_user,
                                     .window_changed = result.scrolled};
    cui_id old = *selected;
    bool activated =
        matched
            ? (observed
                   ? cui_choice_private_list_events(ui, filtered, matched, selected, &state->list,
                                                    list_options, input, handler, observed)
                   : cui_base_list(ui, filtered, matched, selected, &state->list, list_options))
            : false;
    if (activated) {
        cui_overlay_close(ui, &state->overlay);
        cui_dismiss_focus_scope(ui, state->overlay.id, input);
        cui_overlay_end(ui);
        cui_set_semantic_details(
            ui, input, (cui_semantic_details){.controls = state->overlay.id, .description = text});
        result.changed = old != *selected;
        return result;
    }
    cui_begin_portal(ui, state->overlay.id,
                     (cui_box){.padding = theme->padding,
                               .clip = true,
                               .background = theme->popover,
                               .border = theme->border,
                               .border_width = 1,
                               .radius = theme->radius},
                     placement.bounds, 100);
    cui_interact(ui, state->overlay.id, CUI_ROLE_LISTBOX, name, false, false, 0);
    cui_set_semantic_details(
        ui, state->overlay.id,
        (cui_semantic_details){.labelled_by = input,
                               .orientation = CUI_ORIENTATION_VERTICAL,
                               .scrollable = matched > rows,
                               .scroll_y = state->list.first_visible * theme->control_height,
                               .scroll_max_y =
                                   matched > rows ? (matched - rows) * theme->control_height : 0});
    if (matched)
        cui_choice_paint_list(ui, state->overlay.id ^ UINT64_C(0xd1bc905ee4216f11), theme, filtered,
                              matched, selected, &state->list, list_options, NULL, true, &metrics);
    else
        cui_text(ui, cui_string("No matches"), theme->font_size, theme->muted);
    cui_end_box(ui);
    cui_overlay_end(ui);
    return result;
}

cui_combobox_result cui_combobox(cui_context *ui, cui_id input, const cui_theme *theme,
                                 cui_str name, cui_edit *query, const cui_choice_item *items,
                                 size_t count, cui_choice_item *filtered, size_t capacity,
                                 cui_id *selected, cui_choice_popup *state,
                                 cui_choice_popup_options options) {
    return cui_combobox_render(ui, input, theme, name, query, items, count, filtered, capacity,
                               selected, state, options, (cui_selection_handler){0}, NULL);
}
cui_combobox_event_result cui_combobox_events(cui_context *ui, cui_id input, const cui_theme *theme,
                                              cui_str name, cui_edit *query,
                                              const cui_choice_item *items, size_t count,
                                              cui_choice_item *filtered, size_t capacity,
                                              cui_id *selected, cui_choice_popup *state,
                                              cui_choice_popup_options options,
                                              cui_selection_handler handler) {
    cui_combobox_event_result result;
    memset(&result, 0, sizeof(result));
    if (!ui || !input) {
        result.legacy.error = CUI_EDIT_INVALID_ARGUMENT;
        return result;
    }
    result.legacy =
        cui_combobox_render(ui, input, theme, name, query, items, count, filtered, capacity,
                            selected, state, options, handler, &result.events);
    return result;
}

static bool cui_list_multi_common(cui_context *ui, cui_id container, const cui_theme *theme,
                                  const cui_choice_item *items, size_t count, bool *selected,
                                  cui_list_state *state, cui_list_options options,
                                  const cui_choice_item_handler *handler,
                                  cui_choice_items_event_result *result) {
    if (handler && (!ui || !container))
        return false;
    if (!theme || !items || !selected || !state)
        return false;
    cui_theme validation_theme;
    if (!cui_choice_design_theme(theme, options.design_size, &validation_theme))
        return false;
    cui_list_metrics metrics = cui_choice_list_metrics(theme, options);
    options.scroll_bounds = cui_bounds(ui, container);
    options.scroll_target = container;
    bool activated;
    if (handler) {
        *result = cui_base_list_multi_events(ui, container, items, count, selected, state, options,
                                             *handler);
        activated = result->legacy_result;
    } else
        activated = cui_base_list_multi(ui, items, count, selected, state, options);
    cui_id action = 0;
    cui_choice_paint_list(ui, container, theme, items, count, &action, state, options, selected,
                          false, &metrics);
    return activated;
}
bool cui_list_multi(cui_context *ui, cui_id container, const cui_theme *theme,
                    const cui_choice_item *items, size_t count, bool *selected,
                    cui_list_state *state, cui_list_options options) {
    return cui_list_multi_common(ui, container, theme, items, count, selected, state, options, NULL,
                                 NULL);
}
cui_choice_items_event_result cui_list_multi_events(cui_context *ui, cui_id container,
                                                    const cui_theme *theme,
                                                    const cui_choice_item *items, size_t count,
                                                    bool *selected, cui_list_state *state,
                                                    cui_list_options options,
                                                    cui_choice_item_handler handler) {
    cui_choice_items_event_result result = {0};
    (void)cui_list_multi_common(ui, container, theme, items, count, selected, state, options,
                                &handler, &result);
    return result;
}

static bool cui_choice_chips(cui_context *ui, cui_id input, const cui_theme *theme,
                             const cui_choice_item *items, size_t count, bool *selected,
                             bool disabled) {
    bool changed = false;
    cui_begin_box(ui, input ^ UINT64_C(0x97c22bb751e0d56a), (cui_box){.gap = theme->gap});
    for (size_t i = 0; i < count; i++)
        if (selected[i]) {
            cui_id chip = items[i].id ^ UINT64_C(0x45f2b82e6c9bf447);
            bool removed = cui_base_selection_remove(ui, chip, items[i].label, &selected[i],
                                                     disabled || items[i].disabled);
            cui_choice_item chip_item = {chip, items[i].label, items[i].disabled};
            cui_choice_draw(ui, &chip_item, theme, true, disabled || items[i].disabled,
                            CUI_TAB_PILL);
            if (removed) {
                changed = true;
                cui_focus(ui, input);
            }
        }
    cui_end_box(ui);
    return changed;
}
bool cui_select_multi(cui_context *ui, cui_id trigger, const cui_theme *theme, cui_str name,
                      cui_str placeholder, const cui_choice_item *items, size_t count,
                      bool *selected, cui_choice_popup *state, cui_choice_popup_options options) {
    if (!theme || !selected || !state || !state->overlay.id || (!items && count) ||
        !isfinite(options.viewport.w) || !isfinite(options.viewport.h) || options.viewport.w <= 0 ||
        options.viewport.h <= 0 || !isfinite(theme->control_height) || theme->control_height <= 0 ||
        !isfinite(theme->padding) || theme->padding < 0 || theme->padding > options.viewport.h / 2)
        return false;
    cui_id preferred = 0;
    size_t selected_count = 0;
    for (size_t i = 0; i < count; i++)
        if (selected[i]) {
            if (!preferred)
                preferred = items[i].id;
            selected_count++;
        }
    cui_theme trigger_theme;
    if (!cui_choice_design_theme(theme, options.design_size, &trigger_theme))
        return false;
    cui_base_choice_popup_trigger(ui, trigger, name, options.disabled, items, count, preferred,
                                  state);
    cui_begin_box(ui, trigger ^ UINT64_C(0x471d50d3bf973ac9), (cui_box){.gap = theme->gap});
    cui_choice_item trigger_item = {trigger, placeholder, options.disabled};
    if (selected_count)
        trigger_item.label = cui_string("Selected options");
    cui_choice_draw(ui, &trigger_item, &trigger_theme, false, options.disabled, CUI_TAB_DEFAULT);
    bool changed = false;
    if (selected_count)
        changed |= cui_choice_chips(ui, trigger, theme, items, count, selected, options.disabled);
    cui_end_box(ui);
    if (!state->overlay.open)
        return changed;
    cui_list_metrics metrics = cui_choice_search_metrics(theme, options);
    cui_theme row_theme = *theme;
    row_theme.control_height = metrics.height;
    row_theme.padding = 4 * metrics.density;
    theme = &row_theme;
    float width = options.width > 0 && isfinite(options.width) ? options.width : 240;
    if (width > options.viewport.w)
        width = options.viewport.w;
    float max_height = isfinite(options.max_height) && options.max_height > 0
                           ? options.max_height
                           : 320 * metrics.density;
    float available = fminf(options.viewport.h - 8, max_height) - 2 * theme->padding;
    size_t rows = count;
    if (available / theme->control_height < (float)rows)
        rows = available > theme->control_height ? (size_t)(available / theme->control_height) : 1;
    if (rows > 4096)
        rows = 4096;
    cui_popup_result placement = cui_popup_place(
        cui_bounds(ui, trigger),
        (cui_size){width, 2 * theme->padding + (rows ? rows : 1) * theme->control_height},
        options.viewport, options.side, 4);
    cui_overlay_set_bounds(&state->overlay, placement.bounds);
    if (!cui_overlay_begin(ui, &state->overlay))
        return changed;
    cui_list_options list_options = {.scrollbar_style = options.scrollbar_style,
                                     .disabled = options.disabled,
                                     .dt = options.dt,
                                     .visible_begin = state->list.first_visible,
                                     .visible_count = rows,
                                     .render = options.render,
                                     .render_user = options.render_user,
                                     .scroll_bounds = cui_bounds(ui, state->overlay.id),
                                     .scroll_scope = state->overlay.id,
                                     .scroll_target =
                                         state->overlay.id ^ UINT64_C(0xd1bc905ee4216f11)};
    if (count)
        changed |= cui_base_list_multi(ui, items, count, selected, &state->list, list_options);
    cui_begin_portal(ui, state->overlay.id,
                     (cui_box){.padding = theme->padding,
                               .clip = true,
                               .background = theme->popover,
                               .border = theme->border,
                               .border_width = 1,
                               .radius = theme->radius},
                     placement.bounds, 100);
    cui_interact(ui, state->overlay.id, CUI_ROLE_LISTBOX, name, false, false, 0);
    cui_set_semantic_details(
        ui, state->overlay.id,
        (cui_semantic_details){.labelled_by = trigger,
                               .multiselectable = true,
                               .orientation = CUI_ORIENTATION_VERTICAL,
                               .scrollable = count > rows,
                               .scroll_y = state->list.first_visible * theme->control_height,
                               .scroll_max_y =
                                   count > rows ? (count - rows) * theme->control_height : 0});
    cui_id action = 0;
    if (count)
        cui_choice_paint_list(ui, state->overlay.id ^ UINT64_C(0xd1bc905ee4216f11), theme, items,
                              count, &action, &state->list, list_options, selected, true, &metrics);
    else
        cui_text(ui, cui_string("No options"), theme->font_size, theme->muted);
    cui_end_box(ui);
    cui_overlay_end(ui);
    return changed;
}

cui_combobox_result cui_combobox_multi(cui_context *ui, cui_id input, const cui_theme *theme,
                                       cui_str name, cui_edit *query, const cui_choice_item *items,
                                       size_t count, cui_choice_item *filtered,
                                       bool *filtered_selected, size_t capacity, bool *selected,
                                       cui_choice_popup *state, cui_choice_popup_options options) {
    cui_combobox_result result = {.error = CUI_EDIT_OK};
    if (!theme || !query || !selected || !state || !state->overlay.id || (!items && count) ||
        (!filtered && capacity) || capacity < count || cui_edit_get_state(query).multiline ||
        !isfinite(options.viewport.w) || !isfinite(options.viewport.h) || options.viewport.w <= 0 ||
        options.viewport.h <= 0 || !isfinite(options.viewport.x) || !isfinite(options.viewport.y) ||
        !isfinite(theme->control_height) || theme->control_height <= 0 ||
        !isfinite(theme->padding) || theme->padding < 0 ||
        theme->padding > options.viewport.h / 2) {
        result.error = capacity < count ? CUI_EDIT_CAPACITY : CUI_EDIT_INVALID_ARGUMENT;
        return result;
    }
    state->query_viewport = cui_bounds(ui, cui_input_viewport_id(input));
    cui_input_metrics input_metrics;
    if (!cui_input_measure((cui_input_options){.design_size = options.design_size},
                           theme->font_size, &input_metrics)) {
        result.error = CUI_EDIT_INVALID_ARGUMENT;
        return result;
    }
    state->query_font_size = input_metrics.font_size;
    result =
        cui_base_combobox_multi(ui, input, name, query, items, count, filtered, filtered_selected,
                                capacity, selected, state, options.disabled);
    cui_choice_paint_query(ui, input, theme, name, query, state, options.disabled, result.error,
                           options.design_size);
    result.changed |= cui_choice_chips(ui, input, theme, items, count, selected, options.disabled);
    if (!state->overlay.open)
        return result;
    cui_list_metrics metrics = cui_choice_search_metrics(theme, options);
    cui_theme row_theme = *theme;
    row_theme.control_height = metrics.height;
    row_theme.padding = 4 * metrics.density;
    theme = &row_theme;
    size_t matched = result.matching_count;
    float width = options.width > 0 && isfinite(options.width) ? options.width : 280;
    if (width > options.viewport.w)
        width = options.viewport.w;
    float max_height = isfinite(options.max_height) && options.max_height > 0
                           ? options.max_height
                           : 320 * metrics.density;
    float available = fminf(options.viewport.h - 8, max_height) - 2 * theme->padding;
    size_t rows = matched;
    if (available / theme->control_height < (float)rows)
        rows = available > theme->control_height ? (size_t)(available / theme->control_height) : 1;
    if (rows > 4096)
        rows = 4096;
    state->visible_rows = rows;
    cui_popup_result placement = cui_popup_place(
        cui_bounds(ui, input),
        (cui_size){width, 2 * theme->padding + (rows ? rows : 1) * theme->control_height},
        options.viewport, options.side, 4);
    cui_overlay_set_bounds(&state->overlay, placement.bounds);
    if (!cui_overlay_begin(ui, &state->overlay))
        return result;
    cui_list_options list_options = {.scrollbar_style = options.scrollbar_style,
                                     .dt = options.dt,
                                     .visible_begin = state->list.first_visible,
                                     .visible_count = rows,
                                     .render = options.render,
                                     .render_user = options.render_user,
                                     .window_changed = result.scrolled};
    bool activated = matched ? cui_base_list_multi(ui, filtered, matched, filtered_selected,
                                                   &state->list, list_options)
                             : false;
    if (activated) {
        size_t source = 0;
        for (size_t i = 0; i < matched; i++) {
            while (source < count && items[source].id != filtered[i].id)
                source++;
            if (source < count)
                selected[source++] = filtered_selected[i];
        }
        result.changed = true;
    }
    cui_begin_portal(ui, state->overlay.id,
                     (cui_box){.padding = theme->padding,
                               .clip = true,
                               .background = theme->popover,
                               .border = theme->border,
                               .border_width = 1,
                               .radius = theme->radius},
                     placement.bounds, 100);
    cui_interact(ui, state->overlay.id, CUI_ROLE_LISTBOX, name, false, false, 0);
    cui_set_semantic_details(
        ui, state->overlay.id,
        (cui_semantic_details){.labelled_by = input,
                               .multiselectable = true,
                               .orientation = CUI_ORIENTATION_VERTICAL,
                               .scrollable = matched > rows,
                               .scroll_y = state->list.first_visible * theme->control_height,
                               .scroll_max_y =
                                   matched > rows ? (matched - rows) * theme->control_height : 0});
    cui_id action = 0;
    if (matched)
        cui_choice_paint_list(ui, state->overlay.id ^ UINT64_C(0xd1bc905ee4216f11), theme, filtered,
                              matched, &action, &state->list, list_options, filtered_selected, true,
                              &metrics);
    else
        cui_text(ui, cui_string("No matches"), theme->font_size, theme->muted);
    cui_end_box(ui);
    cui_overlay_end(ui);
    return result;
}

cui_menu_metrics cui_menu_size_metrics(cui_menu_size size, float density, float radius) {
    if (!isfinite(density) || density <= 0)
        density = 1;
    if (!isfinite(radius) || radius < 0)
        radius = 0;
    radius = fminf(radius, 8 * density);
    return (cui_menu_metrics){.row_height = (size == CUI_MENU_SMALL ? 20 : 26) * density,
                              .label_height = 14 * 1.618034f * density,
                              .separator_height = 6 * density,
                              .gap = 2 * density,
                              .padding = 4 * density,
                              .padding_x = 8 * density,
                              .font_size = 14 * density,
                              .icon_size = 12 * density,
                              .radius = size == CUI_MENU_SMALL ? radius / 2 : radius};
}
static cui_menu_metrics cui_choice_menu_metrics(const cui_theme *theme,
                                                cui_menu_tree_options options) {
    float density =
        options.density > 0 && isfinite(options.density) ? options.density : theme->font_size / 16;
    return cui_menu_size_metrics(options.size, density, theme->radius);
}
static float cui_choice_menu_row_height(const cui_menu_entry *entry, cui_menu_metrics metrics) {
    return entry->kind == CUI_MENU_SEPARATOR ? metrics.separator_height
           : entry->kind == CUI_MENU_LABEL   ? metrics.label_height
                                             : metrics.row_height;
}
static float cui_choice_menu_window_height(const cui_menu_entry *entries, size_t begin, size_t rows,
                                           cui_menu_metrics metrics) {
    float height = 0;
    for (size_t i = 0; i < rows; i++)
        height += cui_choice_menu_row_height(&entries[begin + i], metrics) + (i ? metrics.gap : 0);
    return height;
}
static size_t cui_choice_menu_fit(const cui_menu_entry *entries, size_t count, size_t begin,
                                  float available, cui_menu_metrics metrics) {
    size_t rows = 0;
    float height = 0;
    while (begin + rows < count && rows < 4096) {
        float next =
            cui_choice_menu_row_height(&entries[begin + rows], metrics) + (rows ? metrics.gap : 0);
        if (rows && height + next > available)
            break;
        height += next;
        rows++;
    }
    return rows;
}
static void cui_choice_menu_retain_metrics(cui_menu_tree_state *state, cui_menu_metrics metrics) {
    state->row_height = metrics.row_height + metrics.gap;
    state->padding = metrics.padding;
    state->label_height = metrics.label_height;
    state->separator_height = metrics.separator_height;
    state->row_gap = metrics.gap;
}
static void cui_choice_menu_level(cui_context *ui, const cui_theme *theme,
                                  const cui_menu_entry *entries, size_t count,
                                  cui_menu_tree_state *state, size_t level, cui_rect anchor,
                                  cui_menu_tree_options options, cui_str name) {
    cui_menu_metrics metrics = cui_choice_menu_metrics(theme, options);
    cui_list_state *cursor = &state->levels[level];
    float density = metrics.padding / 4;
    float maximum_height = options.max_height > 0 && isfinite(options.max_height)
                               ? options.max_height
                               : fminf(options.popup.viewport.h / 2, 450 * density);
    maximum_height = fminf(maximum_height, options.popup.viewport.h - 8);
    float available = fmaxf(metrics.row_height, maximum_height - 2 * metrics.padding);
    state->available_height = available;
    if (cursor->first_visible >= count)
        cursor->first_visible = count ? count - 1 : 0;
    size_t active = count;
    for (size_t i = 0; i < count; i++)
        if (entries[i].id == cursor->active)
            active = i;
    size_t rows = cui_choice_menu_fit(entries, count, cursor->first_visible, available, metrics);
    if (active < count &&
        (active < cursor->first_visible || active >= cursor->first_visible + rows)) {
        cursor->first_visible = active;
        while (cursor->first_visible && active - cursor->first_visible < 4095 &&
               cui_choice_menu_window_height(entries, cursor->first_visible - 1,
                                             active - cursor->first_visible + 2,
                                             metrics) <= available)
            cursor->first_visible--;
        rows = cui_choice_menu_fit(entries, count, cursor->first_visible, available, metrics);
    }
    state->visible_rows[level] = rows;
    float width =
        options.popup.width > 0 && isfinite(options.popup.width) ? options.popup.width : 240;
    if (width > options.popup.viewport.w)
        width = options.popup.viewport.w;
    float height =
        2 * metrics.padding +
        (rows ? cui_choice_menu_window_height(entries, cursor->first_visible, rows, metrics)
              : metrics.row_height);
    cui_scroll_options bars =
        cui_scroll_resolve(theme, (cui_scroll_options){.vertical = true,
                                                       .disabled = options.popup.disabled,
                                                       .wheel_step = metrics.row_height,
                                                       .scrollbar_style = options.scrollbar_style});
    bool overlay =
        bars.scrollbar_style && (bars.scrollbar_style->visibility == CUI_SCROLLBAR_VISIBLE ||
                                 bars.scrollbar_style->visibility == CUI_SCROLLBAR_AUTO_HIDE);
    float total_height = cui_choice_menu_window_height(entries, 0, count, metrics);
    if (overlay)
        height =
            fminf(maximum_height, 2 * metrics.padding + fmaxf(total_height, metrics.row_height));
    cui_popup_result placement = cui_popup_place(
        anchor, (cui_size){width, height}, options.popup.viewport,
        level ? CUI_POPUP_RIGHT : options.popup.side, !level && state->contextual ? 0 : 4);
    if (!level && options.popup.align_end &&
        (options.popup.side == CUI_POPUP_BELOW || options.popup.side == CUI_POPUP_ABOVE)) {
        float x = anchor.x + anchor.w - placement.bounds.w;
        float max_x = options.popup.viewport.x + options.popup.viewport.w - placement.bounds.w;
        if (x > max_x)
            x = max_x;
        if (x < options.popup.viewport.x)
            x = options.popup.viewport.x;
        placement.bounds.x = x;
    }
    state->panels[level] = placement.bounds;
    if (!level)
        cui_overlay_set_bounds(&state->overlay, placement.bounds);
    cui_id scope =
        level ? state->path[level - 1] ^ UINT64_C(0x4b64d23e91a738c1) : state->overlay.id;
    if (!cui_begin_focus_scope(ui, scope, !level))
        return;
    cui_begin_portal(ui, scope,
                     (cui_box){.padding = metrics.padding,
                               .gap = metrics.gap,
                               .clip = true,
                               .background = theme->popover,
                               .border = theme->border,
                               .border_width = 1,
                               .radius = theme->radius},
                     placement.bounds, (int16_t)(100 + level));
    cui_interact(ui, scope, CUI_ROLE_MENU, name, false, false, 0);
    cui_set_semantic_details(
        ui, scope,
        (cui_semantic_details){
            .orientation = CUI_ORIENTATION_VERTICAL,
            .labelled_by = level ? state->path[level - 1] : state->overlay.trigger,
            .scrollable = count > rows,
            .scroll_y = cui_choice_menu_window_height(entries, 0, cursor->first_visible, metrics) +
                        (cursor->first_visible ? metrics.gap : 0),
            .scroll_max_y = fmaxf(0, cui_choice_menu_window_height(entries, 0, count, metrics) -
                                         (height - 2 * metrics.padding))});
    cui_rect bar_viewport = {metrics.padding, metrics.padding,
                             fmaxf(0, placement.bounds.w - 2 * metrics.padding),
                             fmaxf(0, placement.bounds.h - 2 * metrics.padding)};
    cui_size bar_content = {bar_viewport.w, total_height};
    if (overlay) {
        float x = 0, y = cui_choice_menu_window_height(entries, 0, cursor->first_visible, metrics) +
                         (cursor->first_visible ? metrics.gap : 0);
        if (cui_base_scrollbar_overlay(ui, scope, &x, &y, bar_viewport, bar_content, bars)) {
            size_t first = 0;
            float offset = 0;
            while (first + 1 < count) {
                float next =
                    offset + cui_choice_menu_row_height(&entries[first], metrics) + metrics.gap;
                if (next > y)
                    break;
                offset = next;
                first++;
            }
            cursor->first_visible = first;
            rows = cui_choice_menu_fit(entries, count, first, bar_viewport.h, metrics);
            state->visible_rows[level] = rows;
            bool active_visible = false;
            for (size_t i = first; i < first + rows; i++)
                active_visible |= entries[i].id == cursor->active;
            if (!active_visible) {
                bool focused = cui_focused(ui) == cursor->active;
                cursor->active = 0;
                for (size_t i = first; i < first + rows; i++)
                    if (!entries[i].disabled && entries[i].kind != CUI_MENU_LABEL &&
                        entries[i].kind != CUI_MENU_SEPARATOR) {
                        cursor->active = entries[i].id;
                        break;
                    }
                if (focused)
                    cui_focus(ui, cursor->active);
                if (state->depth > level + 1)
                    state->depth = level + 1;
            }
        }
    }
    size_t begin = cursor->first_visible, end = rows > count - begin ? count : begin + rows;
    float icon_size = metrics.icon_size;
    bool has_left_icon = false;
    for (size_t i = 0; i < count; i++) {
        const cui_menu_entry *entry = &entries[i];
        bool checked = entry->kind == CUI_MENU_CHECK && entry->checked ? *entry->checked
                       : entry->kind == CUI_MENU_RADIO && entry->radio_value
                           ? *entry->radio_value == entry->radio_option
                           : false;
        has_left_icon |= entry->icon.length || (!options.checks_on_right && checked);
    }
    for (size_t i = begin; i < end; i++) {
        const cui_menu_entry *entry = &entries[i];
        bool expanded = level + 1 < state->depth && state->path[level] == entry->id;
        cui_base_menu_entry(ui, entry, options.popup.disabled, expanded, scope);
        if (entry->kind != CUI_MENU_SEPARATOR && entry->kind != CUI_MENU_LABEL)
            cui_set_tab_stop(ui, entry->id, entry->id == cursor->active);
        bool highlighted = !cui_menu_entry_is_disabled(entry) && !options.popup.disabled &&
                           (cursor->active == entry->id || cui_hovered(ui, entry->id));
        bool checked = entry->kind == CUI_MENU_CHECK && entry->checked ? *entry->checked
                       : entry->kind == CUI_MENU_RADIO && entry->radio_value
                           ? *entry->radio_value == entry->radio_option
                           : false;
        cui_color foreground = cui_menu_entry_is_disabled(entry) || options.popup.disabled
                                   ? theme->muted
                               : highlighted ? theme->accent_foreground
                                             : theme->popover_foreground;
        cui_begin_box(
            ui, entry->id,
            (cui_box){.height = cui_choice_menu_row_height(entry, metrics),
                      .width = -1,
                      .radius = entry->kind == CUI_MENU_SEPARATOR ? 0 : metrics.radius,
                      .gap = 4 * density,
                      .row = true,
                      .align_y = CUI_ALIGN_CENTER,
                      .background = highlighted ? theme->accent : (cui_color){0},
                      .border = cui_focused(ui) == entry->id ? theme->focus : theme->surface,
                      .border_width = cui_focused(ui) == entry->id ? 1 : 0});
        if (entry->kind == CUI_MENU_SEPARATOR) {
            cui_begin_box(
                ui, 0, (cui_box){.height = 2 * density, .width = -1, .background = theme->border});
            cui_end_box(ui);
        } else {
            cui_begin_box(ui, 0, (cui_box){.width = metrics.padding_x - 4 * density});
            cui_end_box(ui);
            if (has_left_icon) {
                /* Paint-only wrapper: derive anonymous identity from its row,
                 * avoiding cross-entry collisions with named icon resources. */
                cui_begin_box(ui, 0,
                              (cui_box){.width = icon_size,
                                        .height = icon_size,
                                        .align_x = CUI_ALIGN_CENTER,
                                        .align_y = CUI_ALIGN_CENTER,
                                        .clip = true});
                if (entry->icon.length)
                    cui_text(ui, entry->icon, icon_size, foreground);
                else if (checked && !options.checks_on_right)
                    cui_checkmark(ui, icon_size, foreground);
                cui_end_box(ui);
            }
            cui_begin_box(ui, 0, (cui_box){.width = -1});
            if (entry->render)
                entry->render(ui, entry->render_user);
            else
                cui_text_with_line_height(ui, entry->label, metrics.font_size,
                                          entry->kind == CUI_MENU_LABEL ? theme->muted : foreground,
                                          metrics.font_size * 1.618034f);
            cui_end_box(ui);
            if (checked && options.checks_on_right)
                cui_checkmark(ui, icon_size, foreground);
            if (entry->shortcut.length)
                cui_text_with_line_height(ui, entry->shortcut, metrics.font_size, foreground,
                                          metrics.font_size * 1.618034f);
            if (entry->kind == CUI_MENU_SUBMENU)
                (void)cui_icon_named(ui, entry->id ^ UINT64_C(0x9e3f5d2a91bca002), theme,
                                     cui_string("chevron-right"),
                                     (cui_icon_options){.size = CUI_ICON_XSMALL,
                                                        .custom_color = true,
                                                        .color = theme->muted});
            cui_begin_box(ui, 0, (cui_box){.width = metrics.padding_x - 4 * density});
            cui_end_box(ui);
        }
        cui_end_box(ui);
    }
    if (!count)
        cui_text(ui, cui_string("No actions"), theme->font_size, theme->muted);
    if (overlay)
        cui_scrollbar_overlay_paint(
            ui, scope, theme, 0,
            cui_choice_menu_window_height(entries, 0, cursor->first_visible, metrics) +
                (cursor->first_visible ? metrics.gap : 0),
            bar_viewport, bar_content, bars);
    if (level + 1 < state->depth) {
        size_t parent = count;
        for (size_t i = 0; i < count; i++)
            if (entries[i].id == state->path[level])
                parent = i;
        if (parent >= begin && parent < end) {
            cui_rect child_anchor = {
                placement.bounds.x,
                placement.bounds.y + metrics.padding +
                    cui_choice_menu_window_height(entries, begin, parent - begin, metrics) +
                    (parent > begin ? metrics.gap : 0),
                placement.bounds.w, cui_choice_menu_row_height(&entries[parent], metrics)};
            cui_choice_menu_level(ui, theme, entries[parent].children, entries[parent].child_count,
                                  state, level + 1, child_anchor, options, entries[parent].label);
        }
    }
    cui_end_box(ui);
    cui_end_focus_scope(ui);
}
static bool cui_choice_render_menu_tree(cui_context *ui, cui_id trigger, const cui_theme *theme,
                                        cui_str name, const cui_menu_entry *items, size_t count,
                                        cui_id *action, cui_menu_tree_state *state,
                                        cui_menu_tree_options options,
                                        cui_menu_command_handler handler,
                                        cui_menu_command_result *observed) {
    if (!theme || !state || !state->overlay.id || !action || (!items && count) ||
        !isfinite(theme->control_height) || theme->control_height <= 0 ||
        !isfinite(theme->padding) || theme->padding < 0 || !isfinite(options.popup.viewport.x) ||
        !isfinite(options.popup.viewport.y) || !isfinite(options.popup.viewport.w) ||
        !isfinite(options.popup.viewport.h) || options.popup.viewport.w <= 0 ||
        options.popup.viewport.h <= 2 * theme->padding)
        return false;
    state->drag_scroll = options.drag_scroll;
    bool activated;
    if (observed) {
        *observed = cui_base_menu_tree_events(ui, trigger, name, items, count, action, state,
                                              options.popup.disabled, options.popup.dt,
                                              options.keep_open_checks, handler);
        activated = observed->legacy_activated;
    } else
        activated =
            cui_base_menu_tree(ui, trigger, name, items, count, action, state,
                               options.popup.disabled, options.popup.dt, options.keep_open_checks);
    cui_choice_item trigger_item = {trigger, name, options.popup.disabled};
    cui_choice_draw(ui, &trigger_item, theme, false, options.popup.disabled, CUI_TAB_DEFAULT);
    cui_choice_menu_retain_metrics(state, cui_choice_menu_metrics(theme, options));
    if (state->overlay.open)
        cui_choice_menu_level(ui, theme, items, count, state, 0, cui_bounds(ui, trigger), options,
                              name);
    return activated;
}

bool cui_menu_tree(cui_context *ui, cui_id trigger, const cui_theme *theme, cui_str name,
                   const cui_menu_entry *items, size_t count, cui_id *action,
                   cui_menu_tree_state *state, cui_menu_tree_options options) {
    return cui_choice_render_menu_tree(ui, trigger, theme, name, items, count, action, state,
                                       options, (cui_menu_command_handler){0}, NULL);
}
cui_menu_command_result cui_menu_tree_events(cui_context *ui, cui_id trigger,
                                             const cui_theme *theme, cui_str name,
                                             const cui_menu_entry *items, size_t count,
                                             cui_id *action, cui_menu_tree_state *state,
                                             cui_menu_tree_options options,
                                             cui_menu_command_handler handler) {
    cui_menu_command_result result;
    memset(&result, 0, sizeof(result));
    if (!ui || !trigger)
        return result;
    result.legacy_activated = cui_choice_render_menu_tree(ui, trigger, theme, name, items, count,
                                                          action, state, options, handler, &result);
    return result;
}

bool cui_menu_popup_paint(cui_context *ui, const cui_theme *theme, cui_str name,
                          const cui_menu_entry *items, size_t count, cui_menu_tree_state *state,
                          cui_rect anchor, cui_menu_tree_options options) {
    if (!ui || !theme || !state || !state->overlay.id || (!items && count) ||
        !isfinite(theme->font_size) || theme->font_size <= 0 || !isfinite(theme->radius) ||
        theme->radius < 0 || !isfinite(options.popup.viewport.x) ||
        !isfinite(options.popup.viewport.y) || !isfinite(options.popup.viewport.w) ||
        !isfinite(options.popup.viewport.h) || options.popup.viewport.w <= 0 ||
        options.popup.viewport.h <= 8 || !isfinite(options.popup.width) ||
        !isfinite(options.density) || options.density < 0 || !isfinite(options.max_height) ||
        options.max_height < 0 || options.size < CUI_MENU_MEDIUM ||
        options.size > CUI_MENU_XSMALL || !isfinite(anchor.x) || !isfinite(anchor.y) ||
        !isfinite(anchor.w) || !isfinite(anchor.h) || anchor.w < 0 || anchor.h < 0)
        return false;
    cui_choice_menu_retain_metrics(state, cui_choice_menu_metrics(theme, options));
    if (!state->overlay.open)
        return false;
    cui_choice_menu_level(ui, theme, items, count, state, 0, anchor, options, name);
    return true;
}

static bool cui_choice_render_context_menu(cui_context *ui, cui_id target, const cui_theme *theme,
                                           cui_str name, const cui_menu_entry *items, size_t count,
                                           cui_id *action, cui_menu_tree_state *state,
                                           cui_menu_tree_options options,
                                           cui_menu_command_handler handler,
                                           cui_menu_command_result *observed) {
    if (!theme || !state || !state->overlay.id || !action || (!items && count) ||
        !isfinite(theme->control_height) || theme->control_height <= 0 ||
        !isfinite(theme->padding) || theme->padding < 0 || !isfinite(options.popup.viewport.x) ||
        !isfinite(options.popup.viewport.y) || !isfinite(options.popup.viewport.w) ||
        !isfinite(options.popup.viewport.h) || options.popup.viewport.w <= 0 ||
        options.popup.viewport.h <= 2 * theme->padding)
        return false;
    state->drag_scroll = options.drag_scroll;
    bool activated;
    if (observed) {
        *observed = cui_base_context_menu_events(ui, target, items, count, action, state,
                                                 options.popup.disabled, options.popup.dt,
                                                 options.keep_open_checks, handler);
        activated = observed->legacy_activated;
    } else
        activated =
            cui_base_context_menu(ui, target, items, count, action, state, options.popup.disabled,
                                  options.popup.dt, options.keep_open_checks);
    cui_choice_menu_retain_metrics(state, cui_choice_menu_metrics(theme, options));
    if (state->overlay.open)
        cui_choice_menu_level(ui, theme, items, count, state, 0, state->anchor, options, name);
    return activated;
}

bool cui_context_menu(cui_context *ui, cui_id target, const cui_theme *theme, cui_str name,
                      const cui_menu_entry *items, size_t count, cui_id *action,
                      cui_menu_tree_state *state, cui_menu_tree_options options) {
    return cui_choice_render_context_menu(ui, target, theme, name, items, count, action, state,
                                          options, (cui_menu_command_handler){0}, NULL);
}
cui_menu_command_result cui_context_menu_events(cui_context *ui, cui_id target,
                                                const cui_theme *theme, cui_str name,
                                                const cui_menu_entry *items, size_t count,
                                                cui_id *action, cui_menu_tree_state *state,
                                                cui_menu_tree_options options,
                                                cui_menu_command_handler handler) {
    cui_menu_command_result result;
    memset(&result, 0, sizeof(result));
    if (!ui || !target)
        return result;
    result.legacy_activated = cui_choice_render_context_menu(
        ui, target, theme, name, items, count, action, state, options, handler, &result);
    return result;
}

static cui_color cui_dropdown_opacity(cui_color color, float factor) {
    color.a = (uint8_t)fminf(255, color.a * factor);
    return color;
}
static cui_theme_button_kind cui_dropdown_kind(cui_dropdown_button_variant variant) {
    switch (variant) {
    case CUI_DROPDOWN_BUTTON_SECONDARY:
        return CUI_THEME_BUTTON_SECONDARY;
    case CUI_DROPDOWN_BUTTON_DANGER:
        return CUI_THEME_BUTTON_DANGER;
    case CUI_DROPDOWN_BUTTON_WARNING:
        return CUI_THEME_BUTTON_WARNING;
    case CUI_DROPDOWN_BUTTON_SUCCESS:
        return CUI_THEME_BUTTON_SUCCESS;
    case CUI_DROPDOWN_BUTTON_INFO:
        return CUI_THEME_BUTTON_INFO;
    case CUI_DROPDOWN_BUTTON_DEFAULT:
    case CUI_DROPDOWN_BUTTON_OUTLINE:
        return CUI_THEME_BUTTON_DEFAULT;
    default:
        return CUI_THEME_BUTTON_PRIMARY;
    }
}
static cui_box cui_dropdown_button_box(cui_context *ui, cui_id id, const cui_theme *theme,
                                       cui_dropdown_button_options options, bool trigger,
                                       bool disabled, bool pair_surfaced, cui_color *foreground,
                                       const cui_dropdown_button_metrics *metrics) {
    bool no_padding =
        options.variant == CUI_DROPDOWN_BUTTON_LINK || options.variant == CUI_DROPDOWN_BUTTON_TEXT;
    bool loading = !trigger && options.action_loading && !options.action_disabled &&
                   !options.disabled && !options.menu.popup.disabled;
    bool inactive = disabled && !loading;
    bool hovered = !disabled && cui_hovered(ui, id);
    bool pressed = !disabled && cui_pressed(ui, id);
    bool active = options.selected || pressed;
    bool outlined = options.outline || options.variant == CUI_DROPDOWN_BUTTON_OUTLINE;
    cui_theme_button_kind kind = cui_dropdown_kind(options.variant);
    cui_theme_button_colors colors = theme->buttons[kind];
    cui_box box = {.height = isfinite(options.height) && options.height > 0 ? options.height
                             : no_padding                                   ? 0
                                                                            : metrics->height,
                   .width = trigger ? (no_padding ? metrics->caret_size : metrics->height) : 0,
                   .padding = 0,
                   .row = true,
                   .align_x = CUI_ALIGN_CENTER,
                   .align_y = CUI_ALIGN_CENTER,
                   .radius = theme->radius,
                   .background = outlined ? (active    ? colors.outline_active
                                             : hovered ? colors.outline_hover
                                                       : colors.outline_normal)
                                          : (active    ? colors.active
                                             : hovered ? colors.hover
                                                       : colors.normal),
                   .border = outlined ? colors.outline_border : colors.border,
                   .border_width = outlined || kind == CUI_THEME_BUTTON_DEFAULT ? 1 : 0};
    *foreground = outlined ? colors.outline_foreground : colors.foreground;
    if (!trigger)
        box.width = options.width;
    if (options.variant == CUI_DROPDOWN_BUTTON_GHOST) {
        box.border_width = 0;
        box.background = pressed            ? theme->buttons[CUI_THEME_BUTTON_DEFAULT].active
                         : options.selected ? theme->buttons[CUI_THEME_BUTTON_SECONDARY].active
                         : hovered          ? theme->accent
                         : pair_surfaced && !disabled ? cui_dropdown_opacity(theme->accent, .5f)
                                                      : (cui_color){0};
        *foreground = hovered ? theme->accent_foreground : theme->secondary_foreground;
    } else if (options.variant == CUI_DROPDOWN_BUTTON_LINK) {
        box.background = (cui_color){0};
        box.border_width = 0;
        *foreground = active ? theme->link_active : hovered ? theme->link_hover : theme->link;
    } else if (options.variant == CUI_DROPDOWN_BUTTON_TEXT) {
        box.background = (cui_color){0};
        box.border_width = 0;
        *foreground = cui_dropdown_opacity(theme->text, active ? .7f : hovered ? 1 : .9f);
    } else if (options.variant == CUI_DROPDOWN_BUTTON_CUSTOM) {
        box.background = active    ? options.custom.active
                         : hovered ? options.custom.hover
                                   : cui_dropdown_opacity(options.custom.color, .2f);
        box.border =
            outlined ? cui_dropdown_opacity(options.custom.color, .4f) : options.custom.color;
        box.border_width = outlined ? 1 : 0;
        *foreground = options.custom.foreground;
    }
    if (inactive) {
        *foreground = cui_dropdown_opacity(theme->muted, .5f);
        if (options.variant == CUI_DROPDOWN_BUTTON_GHOST ||
            options.variant == CUI_DROPDOWN_BUTTON_LINK ||
            options.variant == CUI_DROPDOWN_BUTTON_TEXT)
            box.background = box.border = (cui_color){0};
        else if (options.variant == CUI_DROPDOWN_BUTTON_CUSTOM) {
            box.background = cui_dropdown_opacity(options.custom.color, outlined ? .1f : .15f);
            box.border = cui_dropdown_opacity(box.border, outlined ? .5f : .15f);
        } else if (outlined) {
            box.background = cui_dropdown_opacity(colors.outline_normal, .5f);
            box.border = cui_dropdown_opacity(colors.outline_border, .5f);
        } else if (kind == CUI_THEME_BUTTON_DEFAULT) {
            box.background = cui_dropdown_opacity(theme->input_background, .5f);
            box.border = cui_dropdown_opacity(theme->input, .5f);
        } else {
            float opacity = kind == CUI_THEME_BUTTON_SECONDARY ? 1.5f : .15f;
            box.background = cui_dropdown_opacity(colors.normal, opacity);
            box.border = cui_dropdown_opacity(colors.border, opacity);
        }
    }
    if (loading) {
        box.custom_opacity = true;
        box.opacity = .8f;
    }
    if (cui_focused(ui) == id) {
        box.border = theme->focus;
        box.border_width = 2;
    }
    box.custom_corners = true;
    box.corner_radii =
        trigger ? (cui_corner_radii){.top_right = theme->radius, .bottom_right = theme->radius}
                : (cui_corner_radii){.top_left = theme->radius, .bottom_left = theme->radius};
    box.custom_border_edges = true;
    box.border_edges = (cui_border_edges){.left = trigger ? 0 : box.border_width,
                                          .right = box.border_width,
                                          .top = box.border_width,
                                          .bottom = box.border_width};
    return box;
}

static cui_box cui_dropdown_style_box(const cui_theme *theme, cui_dropdown_button_options options,
                                      bool trigger, bool disabled, bool pair_surfaced,
                                      cui_color *foreground,
                                      const cui_dropdown_button_metrics *metrics, bool hovered,
                                      bool pressed, bool focused, bool source) {
    bool no_padding =
        options.variant == CUI_DROPDOWN_BUTTON_LINK || options.variant == CUI_DROPDOWN_BUTTON_TEXT;
    bool loading = !trigger && options.action_loading && !options.action_disabled &&
                   !options.disabled && !options.menu.popup.disabled;
    bool inactive = disabled && !loading;
    hovered = !disabled && hovered;
    pressed = !disabled && pressed;
    if (source) {
        pressed = pressed && !options.selected;
        hovered = hovered && !pressed && !options.selected;
    }
    bool active = options.selected || pressed;
    bool outlined = options.outline || options.variant == CUI_DROPDOWN_BUTTON_OUTLINE;
    cui_theme_button_kind kind = cui_dropdown_kind(options.variant);
    cui_theme_button_colors colors = theme->buttons[kind];
    cui_box box = {.height = isfinite(options.height) && options.height > 0 ? options.height
                             : no_padding                                   ? 0
                                                                            : metrics->height,
                   .width = trigger ? (no_padding ? metrics->caret_size : metrics->height) : 0,
                   .padding = 0,
                   .row = true,
                   .align_x = CUI_ALIGN_CENTER,
                   .align_y = CUI_ALIGN_CENTER,
                   .radius = theme->radius,
                   .background = outlined ? (active    ? colors.outline_active
                                             : hovered ? colors.outline_hover
                                                       : colors.outline_normal)
                                          : (active    ? colors.active
                                             : hovered ? colors.hover
                                                       : colors.normal),
                   .border = outlined ? colors.outline_border : colors.border,
                   .border_width = outlined || kind == CUI_THEME_BUTTON_DEFAULT ? 1 : 0};
    *foreground = outlined ? colors.outline_foreground : colors.foreground;
    if (!trigger)
        box.width = options.width;
    if (options.variant == CUI_DROPDOWN_BUTTON_GHOST) {
        box.border_width = 0;
        box.background = pressed            ? theme->buttons[CUI_THEME_BUTTON_DEFAULT].active
                         : options.selected ? theme->buttons[CUI_THEME_BUTTON_SECONDARY].active
                         : hovered          ? theme->accent
                         : pair_surfaced && !disabled ? cui_dropdown_opacity(theme->accent, .5f)
                                                      : (cui_color){0};
        *foreground = hovered ? theme->accent_foreground : theme->secondary_foreground;
    } else if (options.variant == CUI_DROPDOWN_BUTTON_LINK) {
        box.background = (cui_color){0};
        box.border_width = 0;
        *foreground = active ? theme->link_active : hovered ? theme->link_hover : theme->link;
    } else if (options.variant == CUI_DROPDOWN_BUTTON_TEXT) {
        box.background = (cui_color){0};
        box.border_width = 0;
        *foreground = cui_dropdown_opacity(theme->text, active ? .7f : hovered ? 1 : .9f);
    } else if (options.variant == CUI_DROPDOWN_BUTTON_CUSTOM) {
        box.background = active    ? options.custom.active
                         : hovered ? options.custom.hover
                                   : cui_dropdown_opacity(options.custom.color, .2f);
        box.border =
            outlined ? cui_dropdown_opacity(options.custom.color, .4f) : options.custom.color;
        box.border_width = outlined ? 1 : 0;
        *foreground = options.custom.foreground;
    }
    if (source) {
        if (options.variant == CUI_DROPDOWN_BUTTON_GHOST) {
            box.border_width = outlined ? 1 : 0;
            box.border = (cui_color){0};
            box.background = options.selected
                                 ? (outlined ? theme->buttons[CUI_THEME_BUTTON_DEFAULT].active
                                             : theme->buttons[CUI_THEME_BUTTON_SECONDARY].active)
                             : pressed ? theme->buttons[CUI_THEME_BUTTON_DEFAULT].active
                             : hovered ? cui_dropdown_opacity(theme->accent, theme->dark ? .5f : 1)
                             : pair_surfaced && !disabled && !loading
                                 ? cui_dropdown_opacity(theme->accent, theme->dark ? .25f : .5f)
                                 : (cui_color){0};
            *foreground = hovered ? theme->accent_foreground : theme->secondary_foreground;
        } else if (options.variant == CUI_DROPDOWN_BUTTON_LINK ||
                   options.variant == CUI_DROPDOWN_BUTTON_TEXT) {
            box.border_width = outlined ? 1 : 0;
            box.border = (cui_color){0};
            bool selected_outline = options.selected && outlined;
            if (options.variant == CUI_DROPDOWN_BUTTON_LINK)
                *foreground = selected_outline ? theme->link
                              : active         ? theme->link_active
                              : hovered        ? theme->link_hover
                                               : theme->link;
            else
                *foreground = cui_dropdown_opacity(theme->text, selected_outline ? .9f
                                                                : active         ? .7f
                                                                : hovered        ? 1
                                                                                 : .9f);
        }
    }
    if (inactive && source) {
        *foreground = cui_dropdown_opacity(theme->muted, .5f);
        if (outlined) {
            if (options.variant == CUI_DROPDOWN_BUTTON_GHOST ||
                options.variant == CUI_DROPDOWN_BUTTON_LINK ||
                options.variant == CUI_DROPDOWN_BUTTON_TEXT)
                box.background = box.border = (cui_color){0};
            else if (options.variant == CUI_DROPDOWN_BUTTON_CUSTOM) {
                box.background = cui_dropdown_opacity(options.custom.color, .1f);
                box.border = cui_dropdown_opacity(options.custom.color, .2f);
            } else {
                box.background = cui_dropdown_opacity(colors.outline_normal, .5f);
                box.border = cui_dropdown_opacity(colors.outline_border, .5f);
            }
        } else if (options.variant == CUI_DROPDOWN_BUTTON_GHOST ||
                   options.variant == CUI_DROPDOWN_BUTTON_LINK ||
                   options.variant == CUI_DROPDOWN_BUTTON_TEXT)
            box.background = box.border = (cui_color){0};
        else if (options.variant == CUI_DROPDOWN_BUTTON_CUSTOM)
            box.background = box.border = cui_dropdown_opacity(options.custom.color, .15f);
        else if (kind == CUI_THEME_BUTTON_DEFAULT) {
            box.background = cui_dropdown_opacity(theme->input_background, .5f);
            box.border = cui_dropdown_opacity(theme->input, .5f);
        } else {
            float factor = kind == CUI_THEME_BUTTON_SECONDARY ? 1 : .15f;
            box.background = box.border = cui_dropdown_opacity(colors.normal, factor);
        }
    } else if (inactive) {
        *foreground = cui_dropdown_opacity(theme->muted, .5f);
        if (options.variant == CUI_DROPDOWN_BUTTON_GHOST ||
            options.variant == CUI_DROPDOWN_BUTTON_LINK ||
            options.variant == CUI_DROPDOWN_BUTTON_TEXT)
            box.background = box.border = (cui_color){0};
        else if (options.variant == CUI_DROPDOWN_BUTTON_CUSTOM) {
            box.background = cui_dropdown_opacity(options.custom.color, outlined ? .1f : .15f);
            box.border = cui_dropdown_opacity(box.border, outlined ? .5f : .15f);
        } else if (outlined) {
            box.background = cui_dropdown_opacity(colors.outline_normal, .5f);
            box.border = cui_dropdown_opacity(colors.outline_border, .5f);
        } else if (kind == CUI_THEME_BUTTON_DEFAULT) {
            box.background = cui_dropdown_opacity(theme->input_background, .5f);
            box.border = cui_dropdown_opacity(theme->input, .5f);
        } else {
            float opacity = kind == CUI_THEME_BUTTON_SECONDARY ? 1.5f : .15f;
            box.background = cui_dropdown_opacity(colors.normal, opacity);
            box.border = cui_dropdown_opacity(colors.border, opacity);
        }
    }
    if (loading) {
        box.custom_opacity = true;
        box.opacity = .8f;
    }
    if (focused) {
        box.border = theme->focus;
        box.border_width = 2;
    }
    box.custom_corners = true;
    box.corner_radii =
        trigger ? (cui_corner_radii){.top_right = theme->radius, .bottom_right = theme->radius}
                : (cui_corner_radii){.top_left = theme->radius, .bottom_left = theme->radius};
    box.custom_border_edges = true;
    box.border_edges = (cui_border_edges){.left = trigger ? 0 : box.border_width,
                                          .right = box.border_width,
                                          .top = box.border_width,
                                          .bottom = box.border_width};
    return box;
}

static bool cui_dropdown_patch_valid(cui_button_paint_patch p) {
    if (p.fields & ~63u)
        return false;
    if ((p.fields & CUI_BUTTON_PAINT_RADIUS) &&
        (!isfinite(p.paint.root.radius) || p.paint.root.radius < 0))
        return false;
    if ((p.fields & CUI_BUTTON_PAINT_OPACITY) &&
        (!isfinite(p.paint.root.opacity) || p.paint.root.opacity < 0 || p.paint.root.opacity > 1))
        return false;
    if (p.fields & CUI_BUTTON_PAINT_CORNERS) {
        float radii[] = {p.paint.root.corner_radii.top_left, p.paint.root.corner_radii.top_right,
                         p.paint.root.corner_radii.bottom_left,
                         p.paint.root.corner_radii.bottom_right};
        for (size_t i = 0; i < 4; i++)
            if (!isfinite(radii[i]) || radii[i] < 0)
                return false;
    }
    return true;
}
static void cui_dropdown_apply_patch(cui_button_style_paint *p, cui_button_paint_patch patch) {
    if (patch.fields & CUI_BUTTON_PAINT_BACKGROUND)
        p->root.background = patch.paint.root.background;
    if (patch.fields & CUI_BUTTON_PAINT_BORDER)
        p->root.border = patch.paint.root.border;
    if (patch.fields & CUI_BUTTON_PAINT_FOREGROUND)
        p->foreground = patch.paint.foreground;
    if (patch.fields & CUI_BUTTON_PAINT_RADIUS)
        p->root.radius = patch.paint.root.radius;
    if (patch.fields & CUI_BUTTON_PAINT_CORNERS) {
        p->root.custom_corners = patch.paint.root.custom_corners;
        p->root.corner_radii = patch.paint.root.corner_radii;
    }
    if (patch.fields & CUI_BUTTON_PAINT_OPACITY) {
        p->root.custom_opacity = true;
        p->root.opacity = patch.paint.root.opacity;
    }
}
static cui_button_paint_patch cui_dropdown_style_layer(cui_button_style_paint *p,
                                                       cui_button_style_info *info,
                                                       cui_button_style_phase phase,
                                                       cui_button_style_fn callback, void *user) {
    cui_button_paint_patch patch = {0};
    if (callback) {
        info->phase = phase;
        callback(user, info, p, &patch);
        if (!cui_dropdown_patch_valid(patch))
            patch = (cui_button_paint_patch){0};
        cui_dropdown_apply_patch(p, patch);
    }
    return patch;
}

static bool cui_dropdown_styles_active(const cui_dropdown_button_styles *s) {
    if (!s)
        return false;
    const cui_button_styles halves[] = {s->action, s->trigger};
    for (size_t i = 0; i < 2; i++)
        if (halves[i].instance || halves[i].hovered || halves[i].active || halves[i].selected ||
            halves[i].disabled || halves[i].focused)
            return true;
    return s->paint_policy || s->container.fields || s->custom_shadow || s->host_shadows_enabled;
}
static cui_button_style_paint cui_dropdown_builtin(const cui_theme *theme,
                                                   cui_dropdown_button_options o, bool trigger,
                                                   bool disabled, bool pair,
                                                   const cui_dropdown_button_metrics *metrics,
                                                   bool hover, bool pressed, bool source) {
    cui_button_style_paint p;
    p.root = cui_dropdown_style_box(theme, o, trigger, disabled, pair, &p.foreground, metrics,
                                    hover, pressed, false, source);
    return p;
}
static void cui_dropdown_builtin_colors(cui_button_style_paint *p, cui_button_style_paint built) {
    p->root.background = built.root.background;
    p->root.border = built.root.border;
    p->foreground = built.foreground;
}
static cui_button_style_paint cui_dropdown_resolve(cui_context *ui, cui_id id,
                                                   const cui_theme *theme,
                                                   cui_dropdown_button_options o, bool trigger,
                                                   bool disabled, bool pair, bool open,
                                                   const cui_dropdown_button_metrics *metrics,
                                                   const cui_dropdown_button_styles *descriptor) {
    bool source = descriptor->paint_policy == CUI_DROPDOWN_BUTTON_PAINT_BUTTON;
    bool loading = !trigger && o.action_loading && !disabled;
    cui_interaction state = cui_interaction_state(ui, id);
    cui_button_style_info info = {.id = id,
                                  .hovered = state.hovered,
                                  .pressed = state.pressed,
                                  .focus_visible = state.focus_visible,
                                  .selected = o.selected,
                                  .open = open,
                                  .disabled = disabled,
                                  .loading = loading};
    cui_button_styles styles = trigger ? descriptor->trigger : descriptor->action;
    cui_dropdown_button_options normal = o;
    normal.selected = false;
    normal.action_loading = false;
    pair = loading ? false : source ? (!trigger && pair) : pair;
    cui_button_style_paint p =
        cui_dropdown_builtin(theme, normal, trigger, false, pair, metrics, false, false, source);
    cui_button_paint_patch instance = cui_dropdown_style_layer(&p, &info, CUI_BUTTON_STYLE_INSTANCE,
                                                               styles.instance, styles.user);
    bool selected = o.selected || (source && open);
    bool pointer = !disabled && !loading && (!source || !selected);
    if (pointer && (state.pressed || state.hovered)) {
        cui_dropdown_builtin_colors(&p, cui_dropdown_builtin(theme, normal, trigger, false, pair,
                                                             metrics, state.hovered, state.pressed,
                                                             source));
        cui_dropdown_style_layer(&p, &info,
                                 state.pressed ? CUI_BUTTON_STYLE_ACTIVE : CUI_BUTTON_STYLE_HOVER,
                                 state.pressed ? styles.active : styles.hovered, styles.user);
    }
    if (selected) {
        cui_dropdown_button_options selected_options = normal;
        selected_options.selected = true;
        cui_dropdown_builtin_colors(
            &p, cui_dropdown_builtin(theme, selected_options, trigger, false, pair, metrics,
                                     source ? false : state.hovered, source ? false : state.pressed,
                                     source));
        cui_dropdown_style_layer(&p, &info, CUI_BUTTON_STYLE_SELECTED, styles.selected,
                                 styles.user);
        cui_dropdown_apply_patch(&p, instance);
    }
    if (disabled) {
        cui_dropdown_builtin_colors(&p, cui_dropdown_builtin(theme, normal, trigger, true, pair,
                                                             metrics, false, false, source));
        cui_dropdown_style_layer(&p, &info, CUI_BUTTON_STYLE_DISABLED, styles.disabled,
                                 styles.user);
        cui_dropdown_apply_patch(&p, instance);
    } else if (loading) {
        p.root.opacity = (p.root.custom_opacity ? p.root.opacity : 1) * .8f;
        p.root.custom_opacity = true;
    }
    if (source ? (state.focus_visible && !theme->hide_focus_ring) : cui_focused(ui) == id) {
        p.root.border = theme->focus;
        p.root.border_width = 2;
        cui_dropdown_style_layer(&p, &info, CUI_BUTTON_STYLE_FOCUS, styles.focused, styles.user);
    }
    p.root.border_edges = (cui_border_edges){.left = trigger ? 0 : p.root.border_width,
                                             .right = p.root.border_width,
                                             .top = p.root.border_width,
                                             .bottom = p.root.border_width};
    return p;
}
static void cui_dropdown_shadow(cui_context *ui, const cui_dropdown_button_styles *s,
                                cui_dropdown_button_options o, bool disabled, cui_box box) {
    if (s && s->paint_policy == CUI_DROPDOWN_BUTTON_PAINT_BUTTON && s->custom_shadow &&
        s->host_shadows_enabled && o.variant == CUI_DROPDOWN_BUTTON_CUSTOM && !disabled) {
        cui_corner_radii r =
            box.custom_corners ? box.corner_radii
                               : (cui_corner_radii){box.radius, box.radius, box.radius, box.radius};
        (void)cui_set_shadow_preset(ui, CUI_SHADOW_XS, r);
    }
}
static void cui_dropdown_label(cui_context *ui, cui_id id, cui_str text, float size,
                               cui_color color, bool underline) {
    cui_begin_box(ui, id ^ UINT64_C(0xc8134e39119b750d),
                  (cui_box){.height = size * 1.25f, .align_y = CUI_ALIGN_CENTER});
    cui_text(ui, text, size, color);
    if (underline) {
        cui_size measured = cui_measure_text(ui, text, size);
        cui_begin_box(ui, id ^ UINT64_C(0xc8134e39119b751d),
                      (cui_box){.width = measured.width, .height = 1, .background = color});
        cui_end_box(ui);
    }
    cui_end_box(ui);
}
static cui_dropdown_button_result cui_choice_render_dropdown(
    cui_context *ui, cui_id container, cui_id action_id, cui_id trigger_id, const cui_theme *theme,
    cui_str action_name, cui_str menu_name, const cui_menu_entry *items, size_t count,
    cui_id *menu_action, cui_menu_tree_state *state, cui_dropdown_button_options options,
    cui_dropdown_event_handlers handlers, cui_dropdown_event_result *observed,
    const cui_dropdown_button_styles *styles, const cui_dropdown_button_metadata *metadata) {
    cui_dropdown_button_result result = {0};
    cui_dropdown_button_metadata metadata_copy;
    if (!cui_choice_dropdown_metadata_prepare(&metadata_copy, metadata, !options.hide_action,
                                              !options.hide_menu))
        return result;
    metadata = cui_choice_dropdown_metadata_active(&metadata_copy) ? &metadata_copy : NULL;
    cui_dropdown_button_styles copied = {0};
    if (styles)
        copied = *styles;
    if (copied.paint_policy != CUI_DROPDOWN_BUTTON_PAINT_LEGACY &&
        copied.paint_policy != CUI_DROPDOWN_BUTTON_PAINT_BUTTON)
        return result;
    if ((copied.container.fields & CUI_BUTTON_PAINT_FOREGROUND) ||
        !cui_dropdown_patch_valid(copied.container))
        return result;
    styles = cui_dropdown_styles_active(&copied) ? &copied : NULL;
    if (!ui || !theme || !state || !cui_design_valid(options.design_size) || !state->overlay.id ||
        !menu_action || (!items && count) || !container ||
        (options.hide_action && options.hide_menu) || (!options.hide_action && !action_id) ||
        (!options.hide_menu && !trigger_id) || action_id == trigger_id || container == action_id ||
        container == trigger_id || !isfinite(theme->control_height) || theme->control_height <= 0 ||
        !isfinite(theme->padding) || theme->padding < 0 || !isfinite(options.width))
        return result;
    if (options.inherit_action_variant)
        options.variant = options.action_variant;
    if (options.inherit_action_size)
        options.size = options.action_size;
    options.selected = options.selected || (!options.hide_action && options.action_selected);
    float density =
        isfinite(options.density) && options.density > 0 ? options.density : theme->font_size / 16;
    cui_dropdown_button_metrics metrics = cui_dropdown_button_size_metrics(options.size, density);
    if (!cui_design_valid(options.design_size))
        return (cui_dropdown_button_result){0};
    if (options.design_size) {
        cui_design_metrics d;
        if (!cui_design_resolve(options.design_size, density, &d))
            return (cui_dropdown_button_result){0};
        metrics = (cui_dropdown_button_metrics){d.control_height, d.padding_x, d.font_size, d.gap,
                                                d.indicator_size};
    }
    state->drag_scroll = options.menu.drag_scroll;
    if (metadata)
        result = cui_choice_dropdown_submit_metadata(
            ui, options.hide_action ? 0 : action_id, options.hide_menu ? 0 : trigger_id,
            action_name, menu_name, items, count, menu_action, state,
            options.disabled || options.menu.popup.disabled, options.action_disabled,
            options.action_loading, options.menu.popup.dt, options.menu.keep_open_checks, handlers,
            observed, metadata);
    else if (observed) {
        *observed = cui_base_dropdown_button_events(
            ui, options.hide_action ? 0 : action_id, options.hide_menu ? 0 : trigger_id,
            action_name, menu_name, items, count, menu_action, state,
            options.disabled || options.menu.popup.disabled, options.action_disabled,
            options.action_loading, options.menu.popup.dt, options.menu.keep_open_checks, handlers);
        result = observed->legacy;
    } else
        result = cui_base_dropdown_button(
            ui, options.hide_action ? 0 : action_id, options.hide_menu ? 0 : trigger_id,
            action_name, menu_name, items, count, menu_action, state,
            options.disabled || options.menu.popup.disabled, options.action_disabled,
            options.action_loading, options.menu.popup.dt, options.menu.keep_open_checks);
    if (options.variant == CUI_DROPDOWN_BUTTON_LINK) {
        if (!options.hide_action)
            cui_set_semantic_role(ui, action_id, CUI_ROLE_LINK);
        if (!options.hide_menu)
            cui_set_semantic_role(ui, trigger_id, CUI_ROLE_LINK);
    }
    bool all_disabled = options.disabled || options.menu.popup.disabled;
    bool action_disabled = all_disabled || options.action_disabled || options.action_loading;
    bool pair_surfaced = state->overlay.open ||
                         (!options.hide_action && cui_hovered(ui, action_id)) ||
                         (!options.hide_menu && cui_hovered(ui, trigger_id));
    cui_color trigger_foreground, action_foreground;
    bool no_padding =
        options.variant == CUI_DROPDOWN_BUTTON_LINK || options.variant == CUI_DROPDOWN_BUTTON_TEXT;
    float padding = no_padding ? 0 : metrics.padding_x;
    cui_button_style_paint container_paint = {
        .root = {.row = true, .gap = 0, .align_y = CUI_ALIGN_CENTER}};
    if (styles)
        cui_dropdown_apply_patch(&container_paint, styles->container);
    cui_begin_box(ui, container, container_paint.root);
    if (!options.hide_action) {
        cui_dropdown_button_options action_options = options;
        action_options.outline = options.outline || options.action_outline;
        cui_box action_box =
            cui_dropdown_button_box(ui, action_id, theme, action_options, false, action_disabled,
                                    pair_surfaced, &action_foreground, &metrics);
        if (styles) {
            cui_button_style_paint paint = cui_dropdown_resolve(
                ui, action_id, theme, action_options, false,
                all_disabled || options.action_disabled, pair_surfaced, false, &metrics, styles);
            action_box = paint.root;
            action_foreground = paint.foreground;
        }
        cui_begin_box(ui, action_id, action_box);
        cui_dropdown_shadow(ui, styles, options, all_disabled || options.action_disabled,
                            action_box);
        if (padding > 0) {
            cui_begin_box(ui, 0, (cui_box){.width = padding});
            cui_end_box(ui);
        }
        cui_begin_box(
            ui, action_id ^ UINT64_C(0x4aef702b79ab3160),
            (cui_box){.row = true,
                      .gap = metrics.gap,
                      .max_width = options.width > 0 ? fmaxf(0, options.width - 2 * padding) : 0,
                      .clip = true,
                      .align_y = CUI_ALIGN_CENTER});
        {
            if (options.action_loading && options.leading.length)
                (void)cui_icon_spinner(
                    ui, action_id ^ UINT64_C(0x4aef702b79ab3181), theme,
                    (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                       .custom_size =
                                           !options.design_size &&
                                                   options.size == CUI_DROPDOWN_BUTTON_LARGE
                                               ? 24 * theme->font_size / 16
                                               : metrics.caret_size,
                                       .custom_color = true,
                                       .color = action_foreground});
            else if (options.leading.length)
                cui_dropdown_label(ui, action_id ^ UINT64_C(0x4aef702b79ab3180), options.leading,
                                   metrics.font_size, action_foreground, false);
            if (options.content)
                options.content(ui, options.content_user);
            else
                cui_dropdown_label(ui, action_id, action_name, metrics.font_size, action_foreground,
                                   options.variant == CUI_DROPDOWN_BUTTON_LINK);
            if (options.trailing.length)
                cui_dropdown_label(ui, action_id ^ UINT64_C(0x4aef702b79ab3190), options.trailing,
                                   metrics.font_size, action_foreground, false);
        }
        cui_end_box(ui);
        if (padding > 0) {
            cui_begin_box(ui, 0, (cui_box){.width = padding});
            cui_end_box(ui);
        }
        cui_end_box(ui);
    }
    if (!options.hide_menu) {
        cui_box trigger_box =
            cui_dropdown_button_box(ui, trigger_id, theme, options, true, all_disabled,
                                    pair_surfaced, &trigger_foreground, &metrics);
        if (styles) {
            cui_button_style_paint paint =
                cui_dropdown_resolve(ui, trigger_id, theme, options, true, all_disabled,
                                     pair_surfaced, state->overlay.open, &metrics, styles);
            trigger_box = paint.root;
            trigger_foreground = paint.foreground;
        }
        cui_begin_box(ui, trigger_id, trigger_box);
        cui_dropdown_shadow(ui, styles, options, all_disabled, trigger_box);
        cui_begin_box(ui, trigger_id ^ UINT64_C(0x4aef702b79ab3170),
                      (cui_box){.width = metrics.caret_size,
                                .height = metrics.caret_size,
                                .align_x = CUI_ALIGN_CENTER,
                                .align_y = CUI_ALIGN_CENTER});
        cui_color caret = trigger_foreground;
        caret.a = (uint8_t)lroundf(caret.a * .75f);
        (void)cui_icon_named(ui, trigger_id ^ UINT64_C(0x4aef702b79ab3171), theme,
                             cui_string("chevron-down"),
                             (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                                .custom_size = metrics.caret_size,
                                                .custom_color = true,
                                                .color = caret});
        cui_end_box(ui);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    cui_choice_menu_retain_metrics(state, cui_choice_menu_metrics(theme, options.menu));
    if (options.tooltip_id && options.tooltip_state) {
        cui_hover_options tooltip = options.tooltip_options;
        tooltip.trigger = action_id;
        tooltip.disabled =
            tooltip.disabled || action_disabled || options.hide_action || state->overlay.open;
        cui_tooltip(ui, options.tooltip_id, theme, options.tooltip_state, options.tooltip, tooltip);
    }
    options.menu.popup.side = options.anchor == CUI_DROPDOWN_ANCHOR_BOTTOM_RIGHT ||
                                      options.anchor == CUI_DROPDOWN_ANCHOR_BOTTOM_LEFT
                                  ? CUI_POPUP_ABOVE
                                  : CUI_POPUP_BELOW;
    options.menu.popup.align_end = options.anchor == CUI_DROPDOWN_ANCHOR_TOP_RIGHT ||
                                   options.anchor == CUI_DROPDOWN_ANCHOR_BOTTOM_RIGHT;
    if (state->overlay.open)
        cui_choice_menu_level(ui, theme, items, count, state, 0, cui_bounds(ui, trigger_id),
                              options.menu, menu_name);
    return result;
}

cui_dropdown_button_result cui_dropdown_button(cui_context *ui, cui_id container, cui_id action_id,
                                               cui_id trigger_id, const cui_theme *theme,
                                               cui_str action_name, cui_str menu_name,
                                               const cui_menu_entry *items, size_t count,
                                               cui_id *menu_action, cui_menu_tree_state *state,
                                               cui_dropdown_button_options options) {
    return cui_choice_render_dropdown(ui, container, action_id, trigger_id, theme, action_name,
                                      menu_name, items, count, menu_action, state, options,
                                      (cui_dropdown_event_handlers){0}, NULL, NULL, NULL);
}
cui_dropdown_event_result
cui_dropdown_button_events(cui_context *ui, cui_id container, cui_id action_id, cui_id trigger_id,
                           const cui_theme *theme, cui_str action_name, cui_str menu_name,
                           const cui_menu_entry *items, size_t count, cui_id *menu_action,
                           cui_menu_tree_state *state, cui_dropdown_button_options options,
                           cui_dropdown_event_handlers handlers) {
    cui_dropdown_event_result result;
    memset(&result, 0, sizeof(result));
    result.legacy = cui_choice_render_dropdown(ui, container, action_id, trigger_id, theme,
                                               action_name, menu_name, items, count, menu_action,
                                               state, options, handlers, &result, NULL, NULL);
    return result;
}

cui_dropdown_button_result cui_dropdown_button_styled_ex(
    cui_context *ui, cui_id container, cui_id action_id, cui_id trigger_id, const cui_theme *theme,
    cui_str action_name, cui_str menu_name, const cui_menu_entry *items, size_t count,
    cui_id *menu_action, cui_menu_tree_state *state, cui_dropdown_button_options options,
    const cui_dropdown_button_styles *styles) {
    return cui_choice_render_dropdown(ui, container, action_id, trigger_id, theme, action_name,
                                      menu_name, items, count, menu_action, state, options,
                                      (cui_dropdown_event_handlers){0}, NULL, styles, NULL);
}
cui_dropdown_event_result cui_dropdown_button_styled_events(
    cui_context *ui, cui_id container, cui_id action_id, cui_id trigger_id, const cui_theme *theme,
    cui_str action_name, cui_str menu_name, const cui_menu_entry *items, size_t count,
    cui_id *menu_action, cui_menu_tree_state *state, cui_dropdown_button_options options,
    const cui_dropdown_button_styles *styles, cui_dropdown_event_handlers handlers) {
    cui_dropdown_event_result result = {0};
    result.legacy = cui_choice_render_dropdown(ui, container, action_id, trigger_id, theme,
                                               action_name, menu_name, items, count, menu_action,
                                               state, options, handlers, &result, styles, NULL);
    return result;
}

cui_dropdown_button_result cui_dropdown_button_with_metadata_ex(
    cui_context *ui, cui_id container, cui_id action_id, cui_id trigger_id, const cui_theme *theme,
    cui_str action_name, cui_str menu_name, const cui_menu_entry *items, size_t count,
    cui_id *menu_action, cui_menu_tree_state *state, cui_dropdown_button_options options,
    const struct cui_dropdown_button_styles *styles, const cui_dropdown_button_metadata *metadata) {
    return cui_choice_render_dropdown(ui, container, action_id, trigger_id, theme, action_name,
                                      menu_name, items, count, menu_action, state, options,
                                      (cui_dropdown_event_handlers){0}, NULL, styles, metadata);
}
cui_dropdown_event_result cui_dropdown_button_with_metadata_events(
    cui_context *ui, cui_id container, cui_id action_id, cui_id trigger_id, const cui_theme *theme,
    cui_str action_name, cui_str menu_name, const cui_menu_entry *items, size_t count,
    cui_id *menu_action, cui_menu_tree_state *state, cui_dropdown_button_options options,
    const struct cui_dropdown_button_styles *styles, const cui_dropdown_button_metadata *metadata,
    cui_dropdown_event_handlers handlers) {
    cui_dropdown_event_result result = {0};
    result.legacy = cui_choice_render_dropdown(ui, container, action_id, trigger_id, theme,
                                               action_name, menu_name, items, count, menu_action,
                                               state, options, handlers, &result, styles, metadata);
    return result;
}
