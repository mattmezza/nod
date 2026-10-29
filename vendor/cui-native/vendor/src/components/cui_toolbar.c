/* SPDX-License-Identifier: MIT */
#include "../base/cui_design_metrics.h"
#include "cui/cui_toolbar_events.h"
#include <math.h>
static bool cui_toolbar_items_valid(cui_id id, const cui_toolbar_item *items, size_t count) {
    if (!id || count > CUI_TOOLBAR_ITEM_LIMIT || (count && !items))
        return false;
    for (size_t i = 0; i < count; i++) {
        if (!items[i].render || (items[i].sized && !items[i].id) || items[i].id == id)
            return false;
        if (items[i].id)
            for (size_t j = 0; j < i; j++)
                if (items[j].id == items[i].id)
                    return false;
    }
    return true;
}
cui_toolbar_options cui_toolbar_default(void) {
    return (cui_toolbar_options){0};
}
static bool cui_toolbar_render_items(cui_context *ui, cui_id id, const cui_theme *theme,
                                     const cui_toolbar_item *items, size_t count,
                                     cui_toolbar_options o, bool group) {
    cui_toolbar_metrics metrics;
    if (!ui || !theme || !cui_toolbar_items_valid(id, items, count) ||
        !cui_design_valid(o.design_size) || !cui_toolbar_measure(o.size, o.custom_size, &metrics))
        return false;
    if (o.design_size) {
        cui_design_metrics d;
        if (!cui_design_resolve(o.design_size, theme->font_size / 16, &d))
            return false;
        metrics.design_size = o.design_size;
        metrics.height = d.control_height + 2 * d.padding_y;
        metrics.padding = d.padding_y;
        metrics.gap = d.gap;
        metrics.font_size = d.font_size;
        metrics.control_height = d.control_height;
    }
    cui_box box = o.custom_box ? o.box
                               : (cui_box){.row = true,
                                           .align_y = CUI_ALIGN_CENTER,
                                           .height = group ? 0 : metrics.height,
                                           .padding = group ? 0 : metrics.padding,
                                           .gap = group ? 0 : metrics.gap};
    bool begun = group ? cui_base_toolbar_group_begin(ui, id, o.label, box)
                       : cui_base_toolbar_begin(ui, id, o.label, box);
    if (!begun)
        return false;
    for (size_t i = 0; i < count; i++) {
        cui_toolbar_presentation p = {.metrics = metrics, .prepared = items[i].sized};
        if (items[i].sized)
            cui_begin_box(ui, items[i].id,
                          (cui_box){.row = true,
                                    .align_y = CUI_ALIGN_CENTER,
                                    .height = metrics.control_height});
        items[i].render(ui, theme, &p, items[i].user);
        if (items[i].sized)
            cui_end_box(ui);
    }
    if (group)
        cui_base_toolbar_group_end(ui);
    else
        cui_base_toolbar_end(ui, id, o.disabled);
    return true;
}
bool cui_toolbar(cui_context *ui, cui_id id, const cui_theme *theme, const cui_toolbar_item *items,
                 size_t count, cui_toolbar_options o) {
    return cui_toolbar_render_items(ui, id, theme, items, count, o, false);
}
bool cui_toolbar_group(cui_context *ui, cui_id id, const cui_theme *theme,
                       const cui_toolbar_item *items, size_t count, cui_toolbar_options o) {
    return cui_toolbar_render_items(ui, id, theme, items, count, o, true);
}
static cui_toolbar_command_event_result
cui_toolbar_command_common(cui_context *ui, const cui_theme *theme,
                           const cui_toolbar_presentation *p, cui_toolbar_command *c,
                           cui_component_activation_handler handler) {
    cui_toolbar_command_event_result result = {0};
    if (!ui || !theme || !p || !c || !cui_design_valid(p->metrics.design_size) || !c->id ||
        (!c->label.data && c->label.length) ||
        (!c->accessible_name.data && c->accessible_name.length))
        return result;
    cui_toolbar_presentation normalized = *p;
    if (!p->metrics.design_size &&
        !cui_toolbar_measure(p->metrics.size, p->metrics.custom_size, &normalized.metrics))
        return result;
    p = &normalized;
    cui_str name = c->accessible_name.length ? c->accessible_name : c->label;
    if (!name.length)
        return result;
    result.valid = true;
    result.submitted = cui_frame_is_submitting(ui) && cui_frame_sequence(ui) != 0;
    cui_interaction state =
        cui_interact(ui, c->id, CUI_ROLE_BUTTON, name, c->disabled, c->selected, 0);
    cui_component_event_metadata evidence = {0};
    bool notify = handler.callback && result.submitted && state.activated;
    if (notify) {
        (void)cui_component_event_init(&evidence, CUI_COMPONENT_EVENT_ACTIVATE, c->id,
                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
        evidence.flags = CUI_COMPONENT_EVENT_HAS_FRAME;
        evidence.frame_sequence = cui_frame_sequence(ui);
        size_t count = 0, matches = 0;
        (void)cui_frame_events(ui, &count);
        cui_activation_snapshot snapshot = {0};
        for (size_t i = 0; i < count; ++i) {
            if (cui_event_accepted_activation(ui, i) == c->id) {
                ++matches;
                if (matches == 1)
                    (void)cui_event_activation_snapshot(ui, i, &snapshot);
            }
        }
        if (matches == 1)
            (void)cui_component_event_from_activation(&evidence, CUI_COMPONENT_EVENT_ACTIVATE,
                                                      &snapshot);
    }
    cui_set_semantic_details(ui, c->id, (cui_semantic_details){.cursor = CUI_CURSOR_DEFAULT});
    float height = p->metrics.control_height;
    float padding = p->metrics.size == CUI_TOOLBAR_XSMALL  ? 4
                    : p->metrics.size == CUI_TOOLBAR_SMALL ? 6
                                                           : 8;
    if (p->metrics.size == CUI_TOOLBAR_CUSTOM)
        padding = p->metrics.custom_size * .2f;
    cui_color fg =
        state.hovered && !c->selected ? theme->accent_foreground : theme->secondary_foreground;
    cui_color bg = c->selected     ? theme->buttons[CUI_THEME_BUTTON_SECONDARY].active
                   : state.pressed ? theme->buttons[CUI_THEME_BUTTON_DEFAULT].active
                   : state.hovered ? theme->accent
                                   : (cui_color){0};
    if (c->disabled) {
        bg = (cui_color){0};
        fg = theme->muted;
        fg.a = (unsigned char)(fg.a * .5f);
    }
    float icon_size = p->metrics.size == CUI_TOOLBAR_CUSTOM   ? p->metrics.custom_size * .75f
                      : p->metrics.size == CUI_TOOLBAR_XSMALL ? 12
                      : p->metrics.size == CUI_TOOLBAR_SMALL  ? 14
                                                              : 16;
    float font_size = p->metrics.size == CUI_TOOLBAR_CUSTOM ? 16 : p->metrics.font_size;
    if (p->metrics.design_size) {
        icon_size = p->metrics.font_size;
        font_size = p->metrics.font_size;
        padding = p->metrics.gap;
    }
    float gap =
        p->metrics.size == CUI_TOOLBAR_SMALL || p->metrics.size == CUI_TOOLBAR_XSMALL ? 4 : 8;
    if (p->metrics.design_size)
        gap = p->metrics.gap;
    cui_box box = {.height = height,
                   .row = true,
                   .align_x = CUI_ALIGN_CENTER,
                   .align_y = CUI_ALIGN_CENTER,
                   .radius = theme->radius,
                   .background = bg,
                   .gap = gap};
    if (!c->label.length) {
        box.width = p->metrics.size == CUI_TOOLBAR_CUSTOM ? p->metrics.custom_size : height;
        box.height = box.width;
        box.fixed_width = box.fixed_height = true;
    } else {
        float width = cui_measure_text(ui, c->label, font_size).width + 2 * padding;
        if (c->icon)
            width += icon_size + gap;
        box.width = p->metrics.size == CUI_TOOLBAR_CUSTOM ? width : fmaxf(width, height);
        if (p->metrics.size == CUI_TOOLBAR_CUSTOM)
            box.height = 0;
    }
    if (state.focus_visible) {
        box.border = theme->focus;
        box.border_width = 1;
    }
    cui_begin_box(ui, c->id, box);
    if (c->icon) {
        cui_begin_box(ui, 0, (cui_box){.width = icon_size, .height = icon_size});
        c->icon(ui, icon_size, fg, c->icon_user);
        cui_end_box(ui);
    }
    if (c->label.length)
        cui_text(ui, c->label, font_size, fg);
    cui_end_box(ui);
    if (state.activated && c->on_activate)
        c->on_activate(c->activate_user);
    if (notify) {
        handler.callback(handler.user, &evidence);
        result.activation_count = 1;
    }
    return result;
}
void cui_toolbar_command_render(cui_context *ui, const cui_theme *theme,
                                const cui_toolbar_presentation *p, void *command) {
    (void)cui_toolbar_command_common(ui, theme, p, command, (cui_component_activation_handler){0});
}
cui_toolbar_command_event_result
cui_toolbar_command_events(cui_context *ui, const cui_theme *theme,
                           const cui_toolbar_presentation *p, cui_toolbar_command *command,
                           cui_component_activation_handler handler) {
    return cui_toolbar_command_common(ui, theme, p, command, handler);
}
