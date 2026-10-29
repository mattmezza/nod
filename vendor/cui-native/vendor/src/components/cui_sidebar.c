/* SPDX-License-Identifier: MIT */
#include "cui/cui_sidebar.h"
#include <math.h>
#include <string.h>
static void cui_sidebar_spacer(cui_context *ui, float width, float height) {
    cui_begin_box(ui, 0,
                  (cui_box){.width = width,
                            .height = height,
                            .fixed_width = width == 0,
                            .fixed_height = height == 0});
    cui_end_box(ui);
}
static void cui_sidebar_icon(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                             float size, cui_color color, bool rotate) {
    if (!name.length)
        return;
    cui_icon_named(ui, id, t, name,
                   (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                      .custom_size = size,
                                      .custom_color = true,
                                      .color = color,
                                      .custom_transform = rotate,
                                      .transform = rotate ? cui_icon_rotation(1.5707963268f)
                                                          : cui_icon_identity()});
}
static bool cui_sidebar_string(cui_str s) {
    return !s.length || s.data;
}
static bool cui_sidebar_menu_valid(const cui_sidebar_menu_item *items, size_t count, unsigned depth,
                                   cui_id *ids, size_t *used) {
    if (depth > CUI_SIDEBAR_DEPTH_LIMIT || count > CUI_SIDEBAR_ENTRY_LIMIT || (count && !items))
        return false;
    for (size_t i = 0; i < count; i++) {
        const cui_sidebar_menu_item *item = &items[i];
        if (!item->id || !cui_sidebar_string(item->label) ||
            !cui_sidebar_string(item->accessibility_label) || !cui_sidebar_string(item->icon) ||
            (item->child_count && !item->state))
            return false;
        cui_id parts[] = {item->id,
                          cui_sidebar_part_id(item->id, 0),
                          cui_sidebar_part_id(item->id, 1),
                          cui_sidebar_part_id(item->id, 2),
                          cui_sidebar_part_id(item->id, 3),
                          cui_sidebar_part_id(item->id, 4)};
        for (size_t p = 0; p < 6; p++) {
            if (*used >= CUI_SIDEBAR_ENTRY_LIMIT * 6u)
                return false;
            for (size_t j = 0; j < *used; j++)
                if (ids[j] == parts[p])
                    return false;
            ids[(*used)++] = parts[p];
        }
        if (!cui_sidebar_menu_valid(item->children, item->child_count, depth + 1, ids, used))
            return false;
    }
    return true;
}
typedef struct {
    cui_id target;
    cui_sidebar_item_handler handler;
    cui_sidebar_menu_event_result *result;
} cui_sidebar_menu_observer;
static void cui_sidebar_menu_notify(void *user, const cui_sidebar_activation *event) {
    cui_sidebar_menu_observer *observer = user;
    cui_sidebar_activation copy = *event;
    copy.event.target = observer->target;
    observer->handler.on_activate(observer->handler.user, &copy);
    if (observer->result->activation_count == SIZE_MAX)
        observer->result->count_overflow = true;
    else
        observer->result->activation_count++;
}
static void cui_sidebar_menu_expansion(void *user, const cui_sidebar_expansion_event *event) {
    cui_sidebar_menu_observer *observer = user;
    cui_sidebar_expansion_event copy = *event;
    copy.event.target = observer->target;
    bool request = copy.event.kind == CUI_COMPONENT_EVENT_REQUEST;
    cui_sidebar_expansion_fn callback =
        request ? observer->handler.on_expand_request : observer->handler.on_expand_change;
    callback(observer->handler.user, &copy);
    size_t *count = request ? &observer->result->request_count : &observer->result->change_count;
    if (*count == SIZE_MAX)
        observer->result->count_overflow = true;
    else
        (*count)++;
}
static void cui_sidebar_menu_render(cui_context *ui, const cui_theme *t,
                                    const cui_sidebar_menu_item *items, size_t count,
                                    bool parent_collapsed, unsigned level,
                                    cui_sidebar_menu_observer *observer) {
    for (size_t i = 0; i < count; i++) {
        const cui_sidebar_menu_item *item = &items[i];
        bool collapsed = level > 1 ? item->collapsed : parent_collapsed;
        cui_sidebar_item_result state;
        if (observer && (observer->handler.on_activate || observer->handler.on_expand_request ||
                         observer->handler.on_expand_change)) {
            cui_sidebar_item_event_result result = cui_base_sidebar_item_events(
                ui, item, collapsed, level,
                (cui_sidebar_item_handler){
                    .on_activate = observer->handler.on_activate ? cui_sidebar_menu_notify : NULL,
                    .user = observer,
                    .on_expand_request =
                        observer->handler.on_expand_request ? cui_sidebar_menu_expansion : NULL,
                    .on_expand_change =
                        observer->handler.on_expand_change ? cui_sidebar_menu_expansion : NULL});
            state = result.control;
        } else
            state = cui_base_sidebar_item(ui, item, collapsed, level);
        bool hover = !item->disabled && !item->active && cui_hovered(ui, item->id);
        cui_color fg = item->disabled          ? t->muted
                       : item->active || hover ? t->sidebar_accent_foreground
                                               : t->sidebar_foreground;
        cui_color bg = item->active || hover ? t->sidebar_accent : (cui_color){0};
        if (hover)
            bg.a = (uint8_t)(bg.a * .8f);
        cui_box row =
            item->custom_box ? item->box : (cui_box){.padding = 8, .gap = 8, .radius = t->radius};
        row.width = -1;
        row.row = true;
        row.align_y = CUI_ALIGN_CENTER;
        row.clip = true;
        if (item->active || hover || !item->custom_box)
            row.background = bg;
        if (collapsed) {
            if (!item->custom_box)
                row.height = 0;
            row.align_x = CUI_ALIGN_CENTER;
        } else
            row.height = 28;
        cui_begin_box(ui, 0, (cui_box){.width = -1});
        cui_begin_box(ui, item->id, row);
        if (item->icon_content) {
            cui_theme scoped = *t;
            scoped.text = fg;
            item->icon_content(ui, &scoped, collapsed, item->icon_user);
        } else
            cui_sidebar_icon(ui, cui_sidebar_part_id(item->id, 2), t, item->icon, 16, fg, false);
        if (!collapsed) {
            cui_begin_box(
                ui, 0,
                (cui_box){
                    .width = -1, .row = true, .gap = 8, .align_y = CUI_ALIGN_CENTER, .clip = true});
            cui_begin_box(ui, 0,
                          item->custom_label_box ? item->label_box
                                                 : (cui_box){.width = -1, .clip = true});
            if (item->label_content) {
                cui_theme scoped = *t;
                scoped.text = fg;
                item->label_content(ui, &scoped, false, item->label_user);
            } else
                cui_text(ui, item->label, 14, fg);
            cui_end_box(ui);
            if (item->suffix) {
                cui_theme scoped = *t;
                scoped.text = fg;
                item->suffix(ui, &scoped, false, item->suffix_user);
            }
            cui_end_box(ui);
            if (item->child_count) {
                cui_id caret = cui_sidebar_part_id(item->id, 0);
                bool highlighted = cui_hovered(ui, caret) || cui_pressed(ui, caret);
                cui_begin_box(
                    ui, caret,
                    (cui_box){.width = 20,
                              .height = 20,
                              .radius = t->radius,
                              .align_x = CUI_ALIGN_CENTER,
                              .align_y = CUI_ALIGN_CENTER,
                              .background = highlighted ? t->accent : (cui_color){0},
                              .border = cui_focused(ui) == caret ? t->focus : (cui_color){0},
                              .border_width = cui_focused(ui) == caret ? 1 : 0});
                cui_sidebar_icon(ui, cui_sidebar_part_id(item->id, 3), t,
                                 cui_string("chevron-right"), 16, t->muted, state.open);
                cui_end_box(ui);
            }
        }
        cui_end_box(ui);
        if (!collapsed && state.open && item->child_count) {
            cui_begin_box(ui, 0, (cui_box){.width = -1, .row = true});
            cui_sidebar_spacer(ui, 14, 0);
            cui_begin_box(ui, cui_sidebar_part_id(item->id, 1),
                          (cui_box){.width = -1,
                                    .padding = 0,
                                    .gap = 0,
                                    .border = t->sidebar_border,
                                    .custom_border_edges = true,
                                    .border_edges = {.left = 1}});
            cui_sidebar_spacer(ui, 0, 2);
            /* Border plus10px inset leaves the rule at x14 and children at25. */
            cui_begin_box(ui, 0, (cui_box){.width = -1, .row = true});
            cui_sidebar_spacer(ui, 11, 0);
            cui_begin_box(ui, 0, (cui_box){.width = -1, .gap = 4});
            cui_sidebar_menu_render(ui, t, item->children, item->child_count, false, level + 1,
                                    observer);
            cui_end_box(ui);
            cui_end_box(ui);
            cui_sidebar_spacer(ui, 0, 2);
            cui_end_box(ui);
            cui_end_box(ui);
        }
        cui_end_box(ui);
        if (item->state && collapsed && (item->icon.length || item->icon_content)) {
            cui_hover_options o = item->tooltip_options;
            o.trigger = item->id;
            o.side = CUI_POPUP_RIGHT;
            if (o.dt <= 0)
                o.dt = .016f;
            if (o.open_delay <= 0)
                o.open_delay = .5f;
            cui_tooltip(ui, cui_sidebar_part_id(item->id, 4), t, &item->state->tooltip, item->label,
                        o);
        } else if (item->state)
            item->state->tooltip = (cui_hover_state){0};
        if (item->context)
            item->context(ui, t, collapsed, item->context_user);
    }
}
static bool cui_sidebar_menu_common(cui_context *ui, cui_id id, const cui_theme *t,
                                    const cui_sidebar_menu_item *items, size_t count,
                                    bool collapsed, cui_sidebar_menu_observer *observer) {
    if (!ui || !id || !t)
        return false;
    cui_id ids[CUI_SIDEBAR_ENTRY_LIMIT * 6u];
    size_t used = 0;
    if (!cui_sidebar_menu_valid(items, count, 1, ids, &used))
        return false;
    for (size_t i = 0; i < used; i++)
        if (ids[i] == id)
            return false;
    if (observer)
        observer->result->valid = true;
    cui_interact(ui, id, CUI_ROLE_TREE, (cui_str){0}, false, false, 0);
    cui_begin_box(ui, id, (cui_box){.width = -1, .gap = 8});
    cui_sidebar_menu_render(ui, t, items, count, collapsed, 1, observer);
    cui_end_box(ui);
    return true;
}
bool cui_sidebar_menu(cui_context *ui, cui_id id, const cui_theme *t,
                      const cui_sidebar_menu_item *items, size_t count, bool collapsed) {
    return cui_sidebar_menu_common(ui, id, t, items, count, collapsed, NULL);
}
cui_sidebar_menu_event_result cui_sidebar_menu_events(cui_context *ui, cui_id id,
                                                      const cui_theme *t,
                                                      const cui_sidebar_menu_item *items,
                                                      size_t count, bool collapsed,
                                                      cui_sidebar_item_handler handler) {
    cui_sidebar_menu_event_result result;
    memset(&result, 0, sizeof(result));
    cui_sidebar_menu_observer observer = {id, handler, &result};
    result.submitted = cui_sidebar_menu_common(ui, id, t, items, count, collapsed, &observer);
    return result;
}
bool cui_sidebar_group(cui_context *ui, cui_id id, const cui_theme *t, cui_str label,
                       cui_sidebar_content_fn content, void *user, bool collapsed) {
    if (!ui || !id || !t || !content || !cui_sidebar_string(label))
        return false;
    cui_interact(ui, id, CUI_ROLE_GROUP, label, false, false, 0);
    cui_begin_box(ui, id, (cui_box){.width = -1});
    if (!collapsed) {
        cui_color fg = t->sidebar_foreground;
        fg.a = (uint8_t)(fg.a * .7f);
        cui_begin_box(
            ui, 0, (cui_box){.width = -1, .height = 32, .row = true, .align_y = CUI_ALIGN_CENTER});
        cui_sidebar_spacer(ui, 8, 0);
        cui_text(ui, label, 12, fg);
        cui_end_box(ui);
    }
    cui_begin_box(ui, 0, (cui_box){.width = -1, .gap = 8});
    content(ui, t, collapsed, user);
    cui_end_box(ui);
    cui_end_box(ui);
    return true;
}
static bool cui_sidebar_section(cui_context *ui, cui_id id, const cui_theme *t,
                                cui_sidebar_section_options o, bool footer,
                                cui_component_activation_handler handler,
                                cui_component_action_result *observed) {
    if (!ui || !id || !t || !o.content || !cui_begin_focus_scope(ui, id, false))
        return false;
    if (observed)
        observed->valid = observed->submitted = true;
    cui_interaction interaction =
        cui_interact(ui, id, CUI_ROLE_GROUP,
                     cui_string(footer ? "Sidebar footer" : "Sidebar header"), false, false, 0);
    cui_set_semantic_details(ui, id, (cui_semantic_details){.pointer_activatable = true});
    if (interaction.activated && (o.on_activate || handler.callback)) {
        size_t count = 0;
        cui_frame_events(ui, &count);
        for (size_t i = 0; i < count; i++)
            if (cui_event_activation(ui, i) == id) {
                cui_component_event_metadata event;
                cui_activation_snapshot snapshot;
                bool captured = cui_event_activation_snapshot(ui, i, &snapshot) &&
                                cui_component_event_from_activation(
                                    &event, CUI_COMPONENT_EVENT_ACTIVATE, &snapshot);
                if (!captured) {
                    (void)cui_component_event_init(&event, CUI_COMPONENT_EVENT_ACTIVATE, id,
                                                   CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
                    (void)cui_component_event_set_frame_index(&event, cui_frame_sequence(ui), i);
                }
                event.target = id;
                if (o.on_activate)
                    o.on_activate(o.activate_user);
                if (handler.callback) {
                    cui_control_events_private_increment(&observed->activation_count,
                                                         &observed->count_overflow);
                    handler.callback(handler.user, &event);
                }
            }
        cui_invalidate(ui);
    }
    bool highlighted = o.selected || cui_hover_within(ui, id) || cui_hovered(ui, id);
    cui_box box = o.box;
    box.width = -1;
    box.row = true;
    box.padding = 8;
    box.gap = 8;
    box.radius = t->radius;
    box.background = highlighted    ? t->sidebar_accent
                     : o.custom_box ? o.box.background
                                    : (cui_color){0};
    cui_begin_box(ui, id, box);
    cui_theme scoped = *t;
    scoped.text = highlighted ? t->sidebar_accent_foreground : t->sidebar_foreground;
    o.content(ui, &scoped, o.collapsed, o.user);
    cui_end_box(ui);
    cui_end_focus_scope(ui);
    return true;
}
bool cui_sidebar_header(cui_context *ui, cui_id id, const cui_theme *t,
                        cui_sidebar_section_options o) {
    return cui_sidebar_section(ui, id, t, o, false, (cui_component_activation_handler){0}, NULL);
}
bool cui_sidebar_footer(cui_context *ui, cui_id id, const cui_theme *t,
                        cui_sidebar_section_options o) {
    return cui_sidebar_section(ui, id, t, o, true, (cui_component_activation_handler){0}, NULL);
}
cui_component_action_result cui_sidebar_header_events(cui_context *ui, cui_id id,
                                                      const cui_theme *t,
                                                      cui_sidebar_section_options o,
                                                      cui_component_activation_handler handler) {
    cui_component_action_result result;
    memset(&result, 0, sizeof(result));
    (void)cui_sidebar_section(ui, id, t, o, false, handler, &result);
    return result;
}
cui_component_action_result cui_sidebar_footer_events(cui_context *ui, cui_id id,
                                                      const cui_theme *t,
                                                      cui_sidebar_section_options o,
                                                      cui_component_activation_handler handler) {
    cui_component_action_result result;
    memset(&result, 0, sizeof(result));
    (void)cui_sidebar_section(ui, id, t, o, true, handler, &result);
    return result;
}
static bool cui_sidebar_toggle_button_common(cui_context *ui, cui_id id, const cui_theme *t,
                                             cui_sidebar_toggle_options o,
                                             cui_sidebar_collapse_handler handler,
                                             cui_sidebar_toggle_event_result *observed) {
    if (!ui || !id || !t || o.side < CUI_SIDEBAR_LEFT || o.side > CUI_SIDEBAR_RIGHT ||
        !cui_sidebar_string(o.accessibility_label))
        return false;
    bool activated;
    if (observed) {
        *observed = cui_base_sidebar_toggle_events(ui, id, o, handler);
        activated = observed->activated;
    } else
        activated = cui_base_sidebar_toggle(ui, id, o);
    bool hover = cui_hovered(ui, id), pressed = cui_pressed(ui, id);
    cui_begin_box(ui, id,
                  (cui_box){.width = 24,
                            .height = 24,
                            .radius = t->radius,
                            .align_x = CUI_ALIGN_CENTER,
                            .align_y = CUI_ALIGN_CENTER,
                            .background = o.disabled ? (cui_color){0}
                                          : pressed  ? t->buttons[CUI_THEME_BUTTON_DEFAULT].active
                                          : hover    ? t->accent
                                                     : (cui_color){0},
                            .border = cui_focused(ui) == id ? t->focus : (cui_color){0},
                            .border_width = cui_focused(ui) == id ? 1 : 0});
    cui_color fg = o.disabled          ? t->muted
                   : hover && !pressed ? t->accent_foreground
                                       : t->secondary_foreground;
    if (o.disabled)
        fg.a = (uint8_t)(fg.a * .5f);
    const char *name = o.side == CUI_SIDEBAR_LEFT
                           ? (o.collapsed ? "panel-left-open" : "panel-left-close")
                           : (o.collapsed ? "panel-right-open" : "panel-right-close");
    cui_sidebar_icon(ui, cui_sidebar_part_id(id, 0), t, cui_string(name), 16, fg, false);
    cui_end_box(ui);
    return activated;
}
bool cui_sidebar_toggle_button(cui_context *ui, cui_id id, const cui_theme *t,
                               cui_sidebar_toggle_options o) {
    return cui_sidebar_toggle_button_common(ui, id, t, o, (cui_sidebar_collapse_handler){0}, NULL);
}
cui_sidebar_toggle_event_result
cui_sidebar_toggle_button_events(cui_context *ui, cui_id id, const cui_theme *t,
                                 cui_sidebar_toggle_options o,
                                 cui_sidebar_collapse_handler handler) {
    cui_sidebar_toggle_event_result result;
    memset(&result, 0, sizeof(result));
    (void)cui_sidebar_toggle_button_common(ui, id, t, o, handler, &result);
    return result;
}
static void cui_sidebar_section_outer(cui_context *ui, const cui_theme *t,
                                      cui_sidebar_content_fn content, void *user, bool collapsed,
                                      bool footer) {
    float pad = collapsed ? 8 : 12;
    cui_begin_box(ui, 0, (cui_box){.width = -1});
    if (!footer || collapsed)
        cui_sidebar_spacer(ui, 0, footer ? 8 : pad);
    cui_begin_box(ui, 0, (cui_box){.width = -1, .row = true, .gap = 0});
    cui_sidebar_spacer(ui, pad, 0);
    cui_begin_box(ui, 0, (cui_box){.width = -1, .row = true, .gap = 8});
    cui_theme scoped = *t;
    scoped.text = t->sidebar_foreground;
    content(ui, &scoped, collapsed, user);
    cui_end_box(ui);
    cui_sidebar_spacer(ui, pad, 0);
    cui_end_box(ui);
    if (footer)
        cui_sidebar_spacer(ui, 0, 12);
    cui_end_box(ui);
}
bool cui_sidebar(cui_context *ui, cui_id id, const cui_theme *t, cui_sidebar_state *s,
                 const cui_sidebar_entry *entries, size_t count, cui_sidebar_options o) {
    if (!ui || !id || !t || !s || count > s->capacity || count > CUI_SIDEBAR_ENTRY_LIMIT ||
        !cui_sidebar_string(o.label))
        return false;
    float available = s->bounds.w > 0 ? s->bounds.w : 255;
    if (o.width_percent) {
        cui_rect previous = cui_bounds(ui, id);
        if (o.width > 0 && previous.w > 0)
            available = previous.w / o.width;
    }
    for (size_t i = 0; i < count && entries; i++)
        if (entries[i].id == id || entries[i].id == cui_sidebar_part_id(id, 0) ||
            entries[i].id == cui_sidebar_part_id(id, 1) ||
            entries[i].id == cui_sidebar_part_id(id, 2))
            return false;
    if (!cui_sidebar_prepare(s, entries, count, o, available))
        return false;
    bool pixel = o.width >= 0 && !o.width_percent;
    cui_box wrapper = {.width = pixel || s->icon_collapsed || !s->render_child ? s->width : o.width,
                       .height = o.height > 0 ? o.height : -1,
                       .clip = true,
                       .row = true,
                       .fixed_width = pixel || s->icon_collapsed || !s->render_child,
                       .width_percent = o.width_percent && !s->icon_collapsed && s->render_child,
                       .align_x =
                           (o.collapse == CUI_SIDEBAR_OFFCANVAS ? o.side == CUI_SIDEBAR_LEFT
                                                                : o.side == CUI_SIDEBAR_RIGHT)
                               ? CUI_ALIGN_END
                               : CUI_ALIGN_START};
    cui_interact(ui, id, CUI_ROLE_GROUP, o.label, false, false, 0);
    cui_begin_box(ui, id, wrapper);
    cui_track_bounds(ui, id, &s->bounds);
    if (s->animating)
        cui_invalidate(ui);
    if (!s->render_child) {
        cui_end_box(ui);
        return true;
    }
    cui_box surface =
        o.custom_box ? o.box : (cui_box){.background = t->sidebar, .border = t->sidebar_border};
    surface.width = pixel || s->icon_collapsed ? s->target_child_width : -1;
    surface.height = -1;
    surface.padding = 0;
    surface.row = false;
    surface.clip = true;
    surface.gap = s->icon_collapsed ? 8 : o.custom_box ? o.box.gap : 0;
    surface.custom_border_edges = true;
    surface.border_edges = (cui_border_edges){.right = o.side == CUI_SIDEBAR_LEFT ? 1 : 0,
                                              .left = o.side == CUI_SIDEBAR_RIGHT ? 1 : 0};
    bool align_end = o.collapse == CUI_SIDEBAR_OFFCANVAS ? o.side == CUI_SIDEBAR_LEFT
                                                         : o.side == CUI_SIDEBAR_RIGHT;
    float offset = pixel && align_end ? s->width - s->target_child_width : 0;
    cui_begin_absolute_box(ui, cui_sidebar_part_id(id, 0), surface,
                           (cui_rect){.x = offset,
                                      .w = pixel || s->icon_collapsed ? s->target_child_width : -1,
                                      .h = -1});
    if (o.header)
        cui_sidebar_section_outer(ui, t, o.header, o.header_user, s->icon_collapsed, false);
    cui_id body = cui_sidebar_part_id(id, 1);
    float width = s->viewport.w > 0 ? s->viewport.w : s->target_child_width;
    float height = s->viewport.h > 0 ? s->viewport.h : o.height > 0 ? o.height : 300;
    float total = cui_sidebar_content_height(s) + (s->icon_collapsed ? 16 : 0);
    cui_scrollbar_style scrollbar_skin = o.scrollbar_style ? *o.scrollbar_style : t->scrollbar;
    scrollbar_skin.reduced_motion |= o.reduced_motion;
    cui_scroll_options scroll = {.width = width,
                                 .height = height,
                                 .content_height = total,
                                 .vertical = true,
                                 .bar_thickness = 8,
                                 .wheel_step = 40,
                                 .scrollbar_style = &scrollbar_skin};
    scroll = cui_scroll_resolve(t, scroll);
    cui_scroll_geometry geometry = cui_scroll_layout(scroll);
    cui_base_scrollable(ui, body, &s->scroll, scroll);
    cui_begin_box(ui, body, (cui_box){.width = -1, .height = -1, .clip = true, .row = true});
    cui_track_bounds(ui, body, &s->viewport);
    cui_begin_absolute_box(ui, cui_sidebar_part_id(id, 2), (cui_box){0},
                           (cui_rect){.y = -s->scroll.y, .w = -1, .h = total});
    float pad = s->icon_collapsed ? 8 : 12, top = count               ? s->icon_collapsed ? 20 : 12
                                                  : s->icon_collapsed ? 8
                                                                      : 0;
    float overdraw = .3f * (o.viewport_height > 0 ? o.viewport_height
                            : s->bounds.h > 0     ? s->bounds.h
                            : o.height > 0        ? o.height
                                                  : 300);
    cui_sidebar_spacer(ui, 0, top);
    for (size_t i = 0; i < count; i++) {
        cui_sidebar_cache *cache = &s->cache[i];
        bool visible =
            top + cache->height >= s->scroll.y - overdraw && top <= s->scroll.y + height + overdraw;
        if (visible) {
            cui_begin_box(ui, 0, (cui_box){.width = -1, .row = true});
            cui_sidebar_spacer(ui, pad, 0);
            cui_begin_box(ui, entries[i].id, (cui_box){.width = -1});
            cui_theme scoped = *t;
            scoped.text = t->sidebar_foreground;
            entries[i].render(ui, &scoped, s->icon_collapsed, entries[i].user);
            cui_track_bounds(ui, entries[i].id, &cache->bounds);
            cui_end_box(ui);
            cui_sidebar_spacer(ui, pad + (geometry.maximum_y > 0 ? 8 : 0), 0);
            cui_end_box(ui);
        } else
            cui_sidebar_spacer(ui, 0, cache->height);
        top += cache->height;
    }
    cui_sidebar_spacer(ui, 0, count ? s->icon_collapsed ? 20 : 12 : s->icon_collapsed ? 8 : 0);
    cui_end_box(ui);
    if (geometry.maximum_y > 0) {
        cui_sidebar_spacer(ui, -1, 0);
        cui_begin_box(ui, 0,
                      (cui_box){.width = scroll.hide_scrollbars ? 0 : scroll.bar_thickness,
                                .fixed_width = true,
                                .height = -1,
                                .clip = true});
        cui_begin_absolute_box(ui, 0, (cui_box){0}, (cui_rect){.w = -1, .h = -1});
        cui_scrollbar(ui, body, t, &s->scroll, geometry, scroll, true);
        cui_end_box(ui);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    if (o.footer)
        cui_sidebar_section_outer(ui, t, o.footer, o.footer_user, s->icon_collapsed, true);
    cui_end_box(ui);
    cui_end_box(ui);
    return true;
}
