/* SPDX-License-Identifier: MIT */
#include "cui/cui_disclosure_events.h"
#include "cui/cui_icon.h"
#include <math.h>
static void cui_disclosure_trigger(cui_context *ui, cui_id id, const cui_theme *theme,
                                   cui_str label, bool open, bool disabled, bool bordered,
                                   const cui_disclosure_item *item, size_t index,
                                   cui_disclosure_header_fn render, void *user,
                                   const cui_box *style) {
    cui_box box = {.height = theme->control_height,
                   .padding = theme->padding,
                   .gap = theme->gap,
                   .row = true,
                   .align_y = CUI_ALIGN_CENTER,
                   .radius = theme->radius,
                   .background = theme->surface,
                   .border = theme->border,
                   .border_width = bordered ? 1 : 0};
    if (style)
        box = *style;
    if (cui_focused(ui) == id) {
        box.border = theme->focus;
        box.border_width = 2;
    }
    cui_begin_box(ui, id, box);
    if (render) {
        cui_disclosure_item effective = *item;
        effective.disabled = disabled;
        render(ui, &effective, index, open, user);
    } else {
        cui_text(ui, label, theme->font_size, disabled ? theme->muted : theme->text);
        if (!disabled) {
            cui_begin_box(ui, id ^ UINT64_C(0x52fc0dc74f3d9a21), (cui_box){.width = -1});
            cui_end_box(ui);
            (void)cui_icon_named(
                ui, id ^ UINT64_C(0x52fc0dc74f3d9a22), theme, cui_string("chevron-down"),
                (cui_icon_options){.size = CUI_ICON_XSMALL,
                                   .custom_color = true,
                                   .color = theme->muted,
                                   .custom_transform = true,
                                   .transform = open ? (cui_icon_transform){-1, 0, 0, -1, 0, 0}
                                                     : cui_icon_identity()});
        }
    }
    cui_end_box(ui);
}
static void cui_disclosure_panel(cui_context *ui, cui_id trigger, cui_id parent, cui_str label,
                                 bool open, cui_box box, cui_disclosure_motion *motion, float dt,
                                 float response, cui_spring_settings settings, bool reduced_motion,
                                 size_t index, cui_disclosure_panel_fn panel, void *user) {
    if (!panel || (!open && !motion))
        return;
    cui_id panel_id = trigger ^ UINT64_C(0x1d8f322539deca61);
    if (motion) {
        if (motion->id != trigger) {
            *motion = (cui_disclosure_motion){.id = trigger};
        }
        if (isfinite(response) && response > 0)
            settings.response = response;
        cui_disclosure_motion_step_ex(motion, open, dt, settings, reduced_motion);
        float height = isfinite(motion->measured.h) ? fmaxf(0, motion->measured.h) : 0;
        cui_begin_box(ui, trigger ^ UINT64_C(0x48aaf2a21d3ed09f),
                      (cui_box){.height = height * fminf(1, fmaxf(0, motion->progress)),
                                .fixed_height = true,
                                .clip = true,
                                .layout_only = motion->progress == 0,
                                .inert = !open});
        if (motion->progress != (open ? 1 : 0))
            cui_invalidate(ui);
    }
    cui_begin_box(ui, panel_id, box);
    cui_interact(ui, panel_id, CUI_ROLE_GROUP, label, false, false, 0);
    cui_set_semantic_details(ui, panel_id,
                             (cui_semantic_details){.labelled_by = trigger, .parent = parent});
    if (motion)
        cui_track_bounds(ui, panel_id, &motion->measured);
    panel(ui, index, user);
    cui_end_box(ui);
    if (motion)
        cui_end_box(ui);
}
static cui_disclosure_event_result
cui_collapsible_styled_common(cui_context *ui, cui_id id, const cui_theme *theme, cui_str label,
                              bool *open, cui_collapsible_options options,
                              cui_disclosure_panel_fn panel, void *user,
                              cui_disclosure_handler handler) {
    if (!theme || !open)
        return (cui_disclosure_event_result){0};
    cui_disclosure_event_result result = {0};
    if (handler.on_change)
        result = cui_base_collapsible_events(ui, id, label, open, options.disabled, handler);
    else
        result.changed = cui_base_collapsible(ui, id, label, open, options.disabled);
    cui_begin_box(ui, id ^ UINT64_C(0xb7c1c9d453922783), (cui_box){.gap = theme->gap});
    cui_disclosure_item item = {.id = id, .label = label, .disabled = options.disabled};
    cui_disclosure_trigger(ui, id, theme, label, *open, options.disabled, !options.borderless,
                           &item, 0, options.render_header, options.header_user,
                           options.trigger_box);
    cui_box panel_box = options.panel_box ? *options.panel_box
                                          : (cui_box){.padding = theme->padding, .gap = theme->gap};
    cui_disclosure_panel(ui, id, 0, label, *open, panel_box, options.motion, options.dt,
                         options.response, theme->spring_control, options.reduced_motion, 0, panel,
                         user);
    cui_end_box(ui);
    return result;
}
bool cui_collapsible_ex(cui_context *ui, cui_id id, const cui_theme *theme, cui_str label,
                        bool *open, cui_collapsible_options options, cui_disclosure_panel_fn panel,
                        void *user) {
    return cui_collapsible_styled_common(ui, id, theme, label, open, options, panel, user,
                                         (cui_disclosure_handler){0})
        .changed;
}
cui_disclosure_event_result cui_collapsible_events(cui_context *ui, cui_id id,
                                                   const cui_theme *theme, cui_str label,
                                                   bool *open, cui_collapsible_options options,
                                                   cui_disclosure_panel_fn panel, void *user,
                                                   cui_disclosure_handler handler) {
    if (!ui || !id || !theme || !open)
        return (cui_disclosure_event_result){0};
    return cui_collapsible_styled_common(ui, id, theme, label, open, options, panel, user, handler);
}
bool cui_collapsible(cui_context *ui, cui_id id, const cui_theme *theme, cui_str label, bool *open,
                     bool disabled, cui_disclosure_panel_fn panel, void *user) {
    return cui_collapsible_ex(ui, id, theme, label, open,
                              (cui_collapsible_options){.disabled = disabled}, panel, user);
}
static cui_disclosure_event_result
cui_accordion_styled_common(cui_context *ui, cui_id container, const cui_theme *theme,
                            const cui_disclosure_item *items, size_t count, bool *open,
                            cui_accordion_options options, cui_disclosure_panel_fn panel,
                            void *user, cui_disclosure_handler handler) {
    if (!theme || !items || !open)
        return (cui_disclosure_event_result){0};
    cui_disclosure_event_result result = {0};
    if (handler.on_change)
        result = cui_base_accordion_events(ui, container, items, count, open, options, handler);
    else
        result.changed = cui_base_accordion(ui, items, count, open, options);
    cui_interact(ui, container, CUI_ROLE_GROUP, cui_string(""), options.disabled, false, 0);
    cui_set_semantic_details(ui, container,
                             (cui_semantic_details){.orientation = CUI_ORIENTATION_VERTICAL});
    cui_begin_box(ui, container, (cui_box){.gap = theme->gap});
    for (size_t i = 0; i < count; ++i) {
        cui_disclosure_trigger(
            ui, items[i].id, theme, items[i].label, open[i], options.disabled || items[i].disabled,
            options.bordered, &items[i], i, options.render_header, options.header_user,
            options.trigger_boxes && i < options.style_count ? &options.trigger_boxes[i] : NULL);
        cui_set_semantic_details(
            ui, items[i].id,
            (cui_semantic_details){.expanded = open[i],
                                   .controls = items[i].id ^ UINT64_C(0x1d8f322539deca61),
                                   .parent = container});
        cui_box panel_box = options.panel_boxes && i < options.style_count
                                ? options.panel_boxes[i]
                                : (cui_box){.padding = theme->padding, .gap = theme->gap};
        cui_disclosure_motion *motion =
            options.motion && i < options.motion_count ? &options.motion[i] : NULL;
        cui_disclosure_panel(ui, items[i].id, container, items[i].label, open[i], panel_box, motion,
                             options.dt, options.response, theme->spring_control,
                             options.reduced_motion, i, panel, user);
    }
    cui_end_box(ui);
    return result;
}

bool cui_accordion(cui_context *ui, cui_id container, const cui_theme *theme,
                   const cui_disclosure_item *items, size_t count, bool *open,
                   cui_accordion_options options, cui_disclosure_panel_fn panel, void *user) {
    return cui_accordion_styled_common(ui, container, theme, items, count, open, options, panel,
                                       user, (cui_disclosure_handler){0})
        .changed;
}
cui_disclosure_event_result cui_accordion_events(cui_context *ui, cui_id container,
                                                 const cui_theme *theme,
                                                 const cui_disclosure_item *items, size_t count,
                                                 bool *open, cui_accordion_options options,
                                                 cui_disclosure_panel_fn panel, void *user,
                                                 cui_disclosure_handler handler) {
    if (!ui || !container || !theme || !items || !count || !open)
        return (cui_disclosure_event_result){0};
    for (size_t i = 0; i < count; ++i)
        if (!items[i].id)
            return (cui_disclosure_event_result){0};
    return cui_accordion_styled_common(ui, container, theme, items, count, open, options, panel,
                                       user, handler);
}
