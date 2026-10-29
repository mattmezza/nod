/* SPDX-License-Identifier: MIT */
#include "cui/cui_dock.h"
#include "cui/cui_components.h"
#include "cui/cui_resizable.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    cui_dock_node node;
    cui_resizable_state resize;
    cui_resizable_slot slots[2];
    size_t child_capacity;
    cui_resizable_slot *child_slots;
    cui_dock_child_geometry *child_geometry;
    cui_resizable_panel *child_panels;
    struct dock_child_scene *child_scenes;
    cui_dock_slot_size *child_sizes;
    cui_tab_scroll_state scroll;
    cui_tab_drag_state drag;
    cui_menu_tree_state menu;
    bool menu_initialized;
    uint64_t menu_key;
    cui_menu_entry menu_entries[CUI_DOCK_CUSTOM_MENU_LIMIT + 10];
} dock_cache;
struct cui_dock_view {
    size_t node_capacity, panel_capacity, count;
    dock_cache *cache;
    cui_choice_item *items;
    cui_tab_parts *parts;
    uint64_t *keys;
    const cui_dock *model;
    cui_id id;
    bool dragging, crossed, zoomed;
    uint64_t zoom_key;
    uint64_t drag_key;
    float drag_x, drag_y, pointer_x, pointer_y;
};
typedef struct dock_scene dock_scene;
struct dock_scene {
    cui_context *ui;
    cui_id id;
    const cui_theme *theme;
    cui_dock *dock;
    cui_dock_view *view;
    const cui_dock_panel *panels;
    size_t panel_count;
    cui_dock_view_options options;
    cui_dock_view_result result;
    bool pending_close, pending_reorder, pending_zoom, zoom_in, cross_destination;
    uint64_t zoom_key;
    bool pending_bulk, keep_active, pending_menu_split, pending_balance, pending_custom;
    cui_dock_node menu_node;
    uint64_t menu_key;
    cui_dock_kind menu_axis;
    cui_id custom_action;
    uint64_t close_key, reorder_key;
    size_t reorder_index;
    bool pending_drop, edge_before;
    cui_dock_node destination;
    cui_dock_kind edge_axis;
};
static cui_dock_group_constraints
dock_group_constraints(cui_dock_view_options options, const cui_dock *model, cui_dock_node node) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(model, node, &info) || info.kind != CUI_DOCK_TABS)
        return (cui_dock_group_constraints){0};
    return options.group_constraints
               ? options.group_constraints(options.constraints_user, model, node)
               : (cui_dock_group_constraints){0};
}
static bool dock_panel_visible(cui_dock_view_options options, const cui_dock *model, uint64_t key) {
    return !options.panel_visible || options.panel_visible(options.visibility_user, model, key);
}
static bool dock_group_draggable(cui_dock_view_options options, const cui_dock *model,
                                 cui_dock_node node) {
    cui_dock_group_constraints constraints = dock_group_constraints(options, model, node);
    cui_dock_node_info info;
    return cui_dock_node_get(model, node, &info) && !constraints.locked && !constraints.collapsed &&
           !(constraints.preserve_last && cui_dock_visible_count(model, node, options.panel_visible,
                                                                 options.visibility_user) <= 1);
}
static bool dock_group_closable(cui_dock_view_options options, const cui_dock *model,
                                cui_dock_node node) {
    return dock_group_draggable(options, model, node) &&
           !dock_group_constraints(options, model, node).unclosable;
}
static bool dock_allowed(dock_scene *s, cui_dock_event operation, uint64_t panel,
                         cui_dock_node destination, size_t index, cui_dock_kind axis, bool before) {
    cui_dock_node source = 0;
    cui_dock_panel_find(s->dock, panel, &source, NULL);
    if ((operation == CUI_DOCK_CLOSED || operation == CUI_DOCK_REORDERED ||
         operation == CUI_DOCK_MOVED || operation == CUI_DOCK_SPLIT) &&
        !dock_panel_visible(s->options, s->dock, panel))
        return false;
    if (operation == CUI_DOCK_CLOSED && !dock_group_closable(s->options, s->dock, source))
        return false;
    if ((operation == CUI_DOCK_REORDERED || operation == CUI_DOCK_MOVED ||
         operation == CUI_DOCK_SPLIT) &&
        (!dock_group_draggable(s->options, s->dock, source) ||
         (!s->cross_destination &&
          dock_group_constraints(s->options, s->dock, destination).locked)))
        return false;
    if (!s->options.allow_operation)
        return true;
    cui_dock_request request = {operation, panel, source, destination, index, axis, before};
    return s->options.allow_operation(s->options.policy_user, &request);
}
static bool dock_contains(cui_rect b, float x, float y) {
    return b.w > 0 && b.h > 0 && x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h;
}
static void dock_drop_target(dock_scene *s, cui_dock_node node, float x, float y) {
    if (s->view->zoomed && node == cui_dock_root(s->dock)) {
        cui_dock_node maximized;
        if (!cui_dock_panel_find(s->dock, s->view->zoom_key, &maximized, NULL))
            return;
        node = maximized;
    }
    cui_dock_node_info info;
    if (!cui_dock_node_get(s->dock, node, &info))
        return;
    if (info.kind != CUI_DOCK_TABS) {
        for (size_t i = 0; i < info.child_count; i++) {
            cui_dock_node child;
            if (!cui_dock_child_at(s->dock, node, i, &child, NULL))
                continue;
            dock_drop_target(s, child, x, y);
        }
        return;
    }
    if (dock_group_constraints(s->options, s->dock, node).locked)
        return;
    cui_rect b = cui_bounds(s->ui, cui_dock_group_id(s->id, node));
    if (!dock_contains(b, x, y))
        return;
    cui_dock_node source;
    if (!cui_dock_panel_find(s->dock, s->view->drag_key, &source, NULL))
        return;
    float dx = fminf(32, b.w * .2f), dy = fminf(32, b.h * .2f);
    bool edge = false;
    if (y >= b.y + s->options.tab_height) {
        if (x < b.x + dx || x > b.x + b.w - dx) {
            edge = true;
            s->edge_axis = CUI_DOCK_SPLIT_HORIZONTAL;
            s->edge_before = x < b.x + dx;
        } else if (y < b.y + s->options.tab_height + dy || y > b.y + b.h - dy) {
            edge = true;
            s->edge_axis = CUI_DOCK_SPLIT_VERTICAL;
            s->edge_before = y < b.y + s->options.tab_height + dy;
        }
    }
    if (source != node || edge) {
        s->pending_drop = true;
        s->destination = node;
        if (!edge)
            s->edge_axis = CUI_DOCK_TABS;
    }
}
static void dock_drag_events(dock_scene *s) {
    cui_dock_view *v = s->view;
    if (s->options.disabled || !s->options.allow_reorder || s->options.collapsed) {
        v->dragging = false;
        return;
    }
    if (v->dragging) {
        bool enabled = false;
        for (size_t i = 0; i < s->panel_count; ++i)
            if (s->panels[i].key == v->drag_key && !s->panels[i].disabled &&
                dock_panel_visible(s->options, s->dock, v->drag_key))
                enabled = true;
        cui_dock_node source;
        if (!enabled || !cui_dock_panel_find(s->dock, v->drag_key, &source, NULL) ||
            !dock_group_draggable(s->options, s->dock, source)) {
            v->dragging = false;
            for (size_t i = 0; i < v->count; ++i)
                memset(&v->cache[i].drag, 0, sizeof v->cache[i].drag);
        }
    }
    size_t count;
    const cui_event *events = cui_frame_events(s->ui, &count);
    for (size_t e = 0; e < count; ++e) {
        cui_event event = events[e];
        if (event.type == CUI_EVENT_POINTER_MOVE || event.type == CUI_EVENT_POINTER_DOWN ||
            event.type == CUI_EVENT_POINTER_UP) {
            v->pointer_x = event.x;
            v->pointer_y = event.y;
        }
        if (event.type == CUI_EVENT_CANCEL ||
            (event.type == CUI_EVENT_KEY_DOWN && event.key == CUI_KEY_ESCAPE)) {
            v->dragging = false;
            for (size_t i = 0; i < v->count; ++i)
                memset(&v->cache[i].drag, 0, sizeof v->cache[i].drag);
        } else if (event.type == CUI_EVENT_POINTER_DOWN &&
                   event.pointer_button == CUI_POINTER_PRIMARY) {
            for (size_t i = 0; i < s->panel_count; ++i) {
                const cui_dock_panel *panel = &s->panels[i];
                cui_dock_node source;
                if (!panel->disabled && dock_panel_visible(s->options, s->dock, panel->key) &&
                    cui_dock_panel_find(s->dock, panel->key, &source, NULL) &&
                    dock_group_draggable(s->options, s->dock, source) &&
                    dock_contains(cui_bounds(s->ui, cui_dock_tab_id(s->id, panel->key)), event.x,
                                  event.y) &&
                    !dock_contains(cui_bounds(s->ui, cui_dock_close_id(s->id, panel->key)), event.x,
                                   event.y)) {
                    v->dragging = true;
                    v->crossed = false;
                    v->drag_key = panel->key;
                    v->drag_x = event.x;
                    v->drag_y = event.y;
                    break;
                }
            }
        } else if (v->dragging && event.type == CUI_EVENT_POINTER_MOVE) {
            if (hypotf(event.x - v->drag_x, event.y - v->drag_y) >= 6)
                v->crossed = true;
        } else if (v->dragging && event.type == CUI_EVENT_POINTER_UP &&
                   event.pointer_button == CUI_POINTER_PRIMARY) {
            if (v->crossed)
                dock_drop_target(s, cui_dock_root(s->dock), event.x, event.y);
            v->dragging = false;
        }
    }
}
static void dock_drag_preview(dock_scene *s) {
    cui_dock_view *v = s->view;
    if (!v->dragging || !v->crossed || s->options.disable_preview || s->options.disabled)
        return;
    dock_scene preview = *s;
    preview.pending_drop = false;
    dock_drop_target(&preview, cui_dock_root(s->dock), v->pointer_x, v->pointer_y);
    cui_dock_drag_palette palette =
        s->options.custom_palette ? s->options.drag_palette
                                  : (cui_dock_drag_palette){s->theme->primary, s->theme->danger,
                                                            s->theme->surface, s->theme->text};
    if (preview.pending_drop) {
        bool permitted = dock_allowed(
            s, preview.edge_axis == CUI_DOCK_TABS ? CUI_DOCK_MOVED : CUI_DOCK_SPLIT, v->drag_key,
            preview.destination, SIZE_MAX, preview.edge_axis, preview.edge_before);
        cui_rect b = cui_bounds(s->ui, cui_dock_group_id(s->id, preview.destination));
        b.y += s->options.tab_height;
        b.h = fmaxf(0, b.h - s->options.tab_height);
        if (preview.edge_axis == CUI_DOCK_SPLIT_HORIZONTAL) {
            b.w *= .5f;
            if (!preview.edge_before)
                b.x += b.w;
        } else if (preview.edge_axis == CUI_DOCK_SPLIT_VERTICAL) {
            b.h *= .5f;
            if (!preview.edge_before)
                b.y += b.h;
        }
        cui_color ink = permitted ? palette.accepted : palette.rejected;
        cui_color fill = ink;
        fill.a = (uint8_t)(ink.a / 5);
        cui_begin_portal(
            s->ui, s->id ^ UINT64_C(0x17d932756d9271a9),
            (cui_box){.background = fill, .border = ink, .border_width = 2, .inert = true}, b, 91);
        cui_end_box(s->ui);
    }
    const cui_dock_panel *panel = NULL;
    for (size_t i = 0; i < s->panel_count; ++i)
        if (s->panels[i].key == v->drag_key)
            panel = &s->panels[i];
    cui_rect workspace = cui_bounds(s->ui, s->id);
    float width = fminf(180, workspace.w), height = fminf(32, workspace.h);
    float x = fmaxf(workspace.x, fminf(v->pointer_x + 12, workspace.x + workspace.w - width));
    float y = fmaxf(workspace.y, fminf(v->pointer_y + 12, workspace.y + workspace.h - height));
    cui_begin_portal(s->ui, s->id ^ UINT64_C(0x86ab919826a387b5),
                     (cui_box){.background = palette.background,
                               .border = s->theme->border,
                               .border_width = 1,
                               .clip = true,
                               .inert = true,
                               .padding = 6},
                     (cui_rect){x, y, width, height}, 92);
    cui_text(s->ui, panel ? panel->title : cui_string("Panel"), s->theme->font_size, palette.text);
    cui_end_box(s->ui);
}
static bool dock_first_active(const cui_dock *model, cui_dock_node node, uint64_t *key,
                              cui_dock_view_options options) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(model, node, &info))
        return false;
    if (info.kind == CUI_DOCK_TABS)
        return cui_dock_displayed_panel(model, node, options.panel_visible, options.visibility_user,
                                        key);
    for (size_t i = 0; i < info.child_count; i++) {
        cui_dock_node child;
        if (!cui_dock_child_at(model, node, i, &child, NULL))
            continue;
        if (dock_first_active(model, child, key, options))
            return true;
    }
    return false;
}
static cui_id dock_part(cui_id id, uint64_t key, uint64_t tag) {
    uint64_t x = id ^ (key + tag);
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x ^= x >> 31;
    return x ? x : tag;
}
cui_id cui_dock_menu_id(cui_id view, uint64_t panel) {
    return dock_part(view, panel, UINT64_C(0x734ca23d83928712));
}
cui_id cui_dock_menu_action_id(cui_id view, uint64_t panel, cui_dock_menu_action action) {
    return dock_part(view, panel,
                     UINT64_C(0x5129837a389cbe01) + (uint64_t)action * UINT64_C(0x178ca9376153));
}
cui_id cui_dock_zoom_id(cui_id view, uint64_t panel) {
    return dock_part(view, panel, UINT64_C(0x46ac8d37ab16317e));
}
bool cui_dock_view_set_zoom(cui_dock_view *view, cui_dock *model, uint64_t panel, bool zoomed) {
    if (!view || (zoomed && !cui_dock_panel_find(model, panel, NULL, NULL)))
        return false;
    if (zoomed && view->model && view->model != model)
        cui_dock_view_reset(view);
    if (zoomed) {
        view->model = model;
        cui_dock_activate(model, panel);
    }
    view->zoomed = zoomed;
    view->zoom_key = panel;
    return true;
}
bool cui_dock_view_zoomed(const cui_dock_view *view, uint64_t *panel) {
    if (!view || !view->zoomed)
        return false;
    if (panel)
        *panel = view->zoom_key;
    return true;
}
cui_id cui_dock_tab_id(cui_id view, uint64_t panel) {
    return dock_part(view, panel, UINT64_C(0x4989cd35cae1d3a7));
}
cui_id cui_dock_close_id(cui_id view, uint64_t panel) {
    return dock_part(view, panel, UINT64_C(0x137a4ef7928dcad1));
}
cui_id cui_dock_group_id(cui_id view, cui_dock_node node) {
    return dock_part(view, node, UINT64_C(0xe3d2a54f74c98b51));
}
cui_id cui_dock_resize_id_at(cui_id view, const cui_dock *model, cui_dock_node split,
                             size_t index) {
    cui_dock_node_info info;
    cui_dock_node child;
    if (!cui_dock_node_get(model, split, &info) || index >= info.child_count ||
        index + 1 >= info.child_count || !cui_dock_child_at(model, split, index, &child, NULL))
        return 0;
    cui_id preceding = dock_part(view, child, UINT64_C(0x729d1cc28b3a17a1));
    return cui_resizable_handle_id(cui_dock_group_id(view, split), preceding);
}
cui_id cui_dock_resize_id(cui_id view, const cui_dock *model, cui_dock_node split) {
    return cui_dock_resize_id_at(view, model, split, 0);
}
cui_dock_view *cui_dock_view_create(size_t nodes, size_t panels) {
    if (!nodes)
        nodes = 128;
    if (!panels)
        panels = 256;
    if (nodes > CUI_DOCK_NODE_LIMIT || panels > CUI_DOCK_PANEL_LIMIT)
        return NULL;
    cui_dock_view *view = calloc(1, sizeof *view);
    if (!view)
        return NULL;
    view->node_capacity = nodes;
    view->panel_capacity = panels;
    view->cache = calloc(nodes, sizeof *view->cache);
    view->items = calloc(panels, sizeof *view->items);
    view->parts = calloc(panels, sizeof *view->parts);
    view->keys = calloc(panels, sizeof *view->keys);
    if (!view->cache || !view->items || !view->parts || !view->keys) {
        cui_dock_view_destroy(view);
        return NULL;
    }
    return view;
}
static void dock_cache_release_children(dock_cache *cache) {
    free(cache->child_slots);
    free(cache->child_geometry);
    free(cache->child_panels);
    free(cache->child_scenes);
    free(cache->child_sizes);
    cache->child_slots = NULL;
    cache->child_geometry = NULL;
    cache->child_panels = NULL;
    cache->child_scenes = NULL;
    cache->child_sizes = NULL;
    cache->child_capacity = 0;
}
void cui_dock_view_destroy(cui_dock_view *view) {
    if (!view)
        return;
    if (view->cache)
        for (size_t i = 0; i < view->count; i++)
            dock_cache_release_children(&view->cache[i]);
    free(view->cache);
    free(view->items);
    free(view->parts);
    free(view->keys);
    free(view);
}
void cui_dock_view_reset(cui_dock_view *view) {
    if (!view)
        return;
    for (size_t i = 0; i < view->count; i++)
        dock_cache_release_children(&view->cache[i]);
    memset(view->cache, 0, view->node_capacity * sizeof *view->cache);
    view->count = 0;
    view->dragging = false;
    view->crossed = false;
    view->zoomed = false;
    view->model = NULL;
    view->id = 0;
}
cui_dock_view_options cui_dock_view_options_default(void) {
    return (cui_dock_view_options){
        .width = 640, .height = 480, .tab_height = 36, .key_step = 8, .allow_reorder = true};
}
static const cui_dock_panel *dock_panel_get(const dock_scene *s, uint64_t key) {
    for (size_t i = 0; i < s->panel_count; ++i)
        if (s->panels[i].key == key)
            return &s->panels[i];
    return NULL;
}
static void dock_emit(dock_scene *s, cui_dock_event event, uint64_t key, cui_dock_node node) {
    if (s->options.on_event)
        s->options.on_event(s->options.event_user, event, key, node);
}
static dock_cache *dock_cache_get(dock_scene *s, cui_dock_node node) {
    for (size_t i = 0; i < s->view->count; ++i)
        if (s->view->cache[i].node == node)
            return &s->view->cache[i];
    size_t index = s->view->count;
    for (size_t i = 0; i < s->view->count; ++i) {
        cui_dock_node_info info;
        if (!cui_dock_node_get(s->dock, s->view->cache[i].node, &info)) {
            index = i;
            break;
        }
    }
    if (index == s->view->node_capacity)
        return NULL;
    if (index == s->view->count)
        ++s->view->count;
    dock_cache *cache = &s->view->cache[index];
    dock_cache_release_children(cache);
    memset(cache, 0, sizeof *cache);
    cache->node = node;
    cui_resizable_state_init(&cache->resize, cache->slots, 2);
    return cache;
}
static cui_dock_result dock_preflight(const cui_dock *model, cui_dock_node node, size_t *nodes,
                                      size_t *panels) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(model, node, &info))
        return CUI_DOCK_NOT_FOUND;
    ++*nodes;
    if (info.kind == CUI_DOCK_TABS) {
        *panels += info.panel_count;
        return CUI_DOCK_OK;
    }
    for (size_t i = 0; i < info.child_count; i++) {
        cui_dock_node child;
        if (!cui_dock_child_at(model, node, i, &child, NULL))
            return CUI_DOCK_NOT_FOUND;
        cui_dock_result error = dock_preflight(model, child, nodes, panels);
        if (error != CUI_DOCK_OK)
            return error;
    }
    return CUI_DOCK_OK;
}
static void dock_close_suffix(cui_context *ui, const cui_choice_item *item, size_t index,
                              void *user) {
    dock_scene *s = user;
    (void)item;
    uint64_t key = s->view->keys[index];
    const cui_dock_panel *panel = dock_panel_get(s, key);
    if (!panel || !panel->closable)
        return;
    if (cui_button_ex(ui, cui_dock_close_id(s->id, key), s->theme, cui_string("Close panel"),
                      (cui_button_options){.variant = CUI_BUTTON_GHOST,
                                           .size = CUI_CONTROL_SMALL,
                                           .icon = cui_string("close"),
                                           .icon_only = true,
                                           .disabled = s->options.disabled || panel->disabled})) {
        s->pending_close = true;
        s->close_key = key;
    }
}
static void dock_render_menu(dock_scene *s, dock_cache *cache, cui_dock_node_info info,
                             const cui_dock_panel *active) {
    if (!active || s->options.hide_menu)
        return;
    cui_id trigger = cui_dock_menu_id(s->id, active->key);
    if (!cache->menu_initialized || cache->menu_key != active->key) {
        cui_menu_tree_init(&cache->menu, dock_part(s->id, info.id, UINT64_C(0x896ec6237838192a)));
        cache->menu_initialized = true;
        cache->menu_key = active->key;
    }
    bool disabled = s->options.disabled || active->disabled;
    bool group_close = dock_group_closable(s->options, s->dock, info.id);
    bool group_drag = dock_group_draggable(s->options, s->dock, info.id);
    size_t count = 0;
    for (size_t i = 0; i < active->menu_count; ++i) {
        cache->menu_entries[count] = active->menu_items[i];
        cache->menu_entries[count++].disabled |= disabled;
    }
    if (count)
        cache->menu_entries[count++] = (cui_menu_entry){.kind = CUI_MENU_SEPARATOR};
    const char *labels[] = {
        "Close panel",         "Close other panels",
        "Close all panels",    s->view->zoomed ? "Restore workspace" : "Maximize panel",
        "Split horizontally",  "Split vertically",
        "Balance parent split"};
    const char *icons[] = {
        "close",      "copy",         "close",   s->view->zoomed ? "minimize" : "maximize",
        "panel-left", "panel-bottom", "maximize"};
    size_t closable = 0, others = 0;
    for (size_t i = 0; i < info.panel_count; ++i) {
        uint64_t key;
        if (!cui_dock_panel_at(s->dock, info.id, i, &key))
            continue;
        const cui_dock_panel *panel = dock_panel_get(s, key);
        if (panel && panel->closable && !panel->disabled &&
            dock_panel_visible(s->options, s->dock, key)) {
            ++closable;
            if (key != active->key)
                ++others;
        }
    }

    for (unsigned i = 0; i < 7; ++i) {
        if (i == CUI_DOCK_MENU_ZOOM && !s->view->zoomed &&
            active->zoom_control != CUI_DOCK_ZOOM_BOTH &&
            active->zoom_control != CUI_DOCK_ZOOM_MENU)
            continue;
        bool unavailable = disabled;
        if (i == 0)
            unavailable |= !active->closable || !group_close;
        if (i == 1)
            unavailable |= !others || !group_close;
        if (i == 2)
            unavailable |= !closable || !group_close;
        if (i == 3)
            unavailable = s->view->zoomed ? false : disabled || !active->zoomable;
        if (i == 4 || i == 5)
            unavailable |= cui_dock_visible_count(s->dock, info.id, s->options.panel_visible,
                                                  s->options.visibility_user) < 2 ||
                           s->view->zoomed || !group_drag;
        if (i == 6)
            unavailable |= !info.parent || s->view->zoomed;
        cache->menu_entries[count++] = (cui_menu_entry){
            .id = cui_dock_menu_action_id(s->id, active->key, (cui_dock_menu_action)i),
            .label = cui_string(labels[i]),
            .icon = cui_string(icons[i]),
            .disabled = unavailable,
            .kind = CUI_MENU_ACTION};
    }
    cui_id action = 0;
    bool activated = cui_base_menu_tree(
        s->ui, trigger, cui_string("Panel actions"), cache->menu_entries, count, &action,
        &cache->menu, disabled && !s->view->zoomed, cui_frame_delta(s->ui), false);
    cui_button_paint(s->ui, trigger, s->theme, cui_string("Panel actions"),
                     (cui_button_options){.variant = CUI_BUTTON_GHOST,
                                          .size = CUI_CONTROL_SMALL,
                                          .icon = cui_string("ellipsis"),
                                          .icon_only = true,
                                          .selected = cache->menu.overlay.open,
                                          .disabled = disabled && !s->view->zoomed});
    cui_rect viewport = cui_bounds(s->ui, s->id);
    if (viewport.w <= 0 || viewport.h <= 8)
        viewport = (cui_rect){0, 0, s->options.width, s->options.height};
    cui_menu_popup_paint(
        s->ui, s->theme, cui_string("Panel actions"), cache->menu_entries, count, &cache->menu,
        cui_bounds(s->ui, trigger),
        (cui_menu_tree_options){.popup = {.viewport = viewport, .width = 240, .align_end = true},
                                .size = CUI_MENU_SMALL});
    if (!activated)
        return;
    for (unsigned i = 0; i < 7; ++i)
        if (action == cui_dock_menu_action_id(s->id, active->key, (cui_dock_menu_action)i)) {
            s->menu_key = active->key;
            s->menu_node = info.id;
            if (i == 0) {
                s->pending_close = true;
                s->close_key = active->key;
            }
            if (i == 1 || i == 2) {
                s->pending_bulk = true;
                s->keep_active =
                    i == 1 || dock_group_constraints(s->options, s->dock, info.id).preserve_last;
            }
            if (i == 3) {
                s->pending_zoom = true;
                s->zoom_in = !s->view->zoomed;
                s->zoom_key = active->key;
            }
            if (i == 4 || i == 5) {
                s->pending_menu_split = true;
                s->menu_axis = i == 4 ? CUI_DOCK_SPLIT_HORIZONTAL : CUI_DOCK_SPLIT_VERTICAL;
            }
            if (i == 6)
                s->pending_balance = true;
            return;
        }
    s->pending_custom = true;
    s->menu_key = active->key;
    s->custom_action = action;
}
typedef struct {
    dock_scene *scene;
    cui_id *selected;
    float suffix_width, height;
    const cui_dock_title_style *style;
} dock_title_context;
static void dock_title_body(cui_context *ui, const cui_choice_item *item, size_t index,
                            void *user) {
    dock_title_context *context = user;
    dock_scene *s = context->scene;
    uint64_t key = s->view->keys[index];
    const cui_dock_panel *panel = dock_panel_get(s, key);
    if (panel && panel->title_render) {
        cui_begin_box(ui, dock_part(s->id, key, UINT64_C(0x348651f58752a026)),
                      (cui_box){.width = panel->title_width,
                                .max_width = panel->title_width,
                                .clip = true,
                                .inert = item->disabled});
        panel->title_render(ui, key, panel->user);
        cui_end_box(ui);
    } else {
        float density = s->theme->font_size / 16;
        cui_tab_metrics metrics = cui_tab_size_metrics(CUI_TAB_DEFAULT, CUI_TAB_MEDIUM, density);
        cui_color ink = *context->selected == item->id || cui_hovered(ui, item->id)
                            ? s->theme->tab_active_foreground
                            : s->theme->tab_foreground;
        if (context->style)
            ink = context->style->foreground;
        if (item->disabled)
            ink = s->theme->muted;
        cui_text(ui, item->label, metrics.font_size, ink);
    }
}
static void dock_title_tail(cui_context *ui, void *user) {
    dock_title_context *context = user;
    dock_scene *s = context->scene;
    if (s->options.collapsed)
        return;
    for (size_t i = 0; i < s->panel_count; ++i) {
        const cui_dock_panel *panel = &s->panels[i];
        if (cui_dock_tab_id(s->id, panel->key) != *context->selected)
            continue;
        cui_begin_box(ui, dock_part(s->id, panel->key, UINT64_C(0xf52720d3a973ba34)),
                      (cui_box){.width = context->suffix_width,
                                .max_width = context->suffix_width,
                                .height = context->height,
                                .clip = true,
                                .inert = s->options.disabled || panel->disabled});
        if (panel->title_suffix)
            panel->title_suffix(ui, panel->key, panel->user);
        cui_end_box(ui);
        return;
    }
}
static void dock_simple_title(dock_scene *s, cui_dock_node_info info, dock_title_context *context,
                              float width, float height) {
    const cui_dock_panel *panel = info.has_active ? dock_panel_get(s, info.active_panel) : NULL;
    float close_width =
        panel && panel->closable && dock_group_closable(s->options, s->dock, info.id) ? 24 : 0;
    float leading = fminf(12, width);
    float suffix = fminf(context->suffix_width, fmaxf(0, width - leading - close_width));
    context->suffix_width = suffix;
    context->height = height;
    cui_begin_box(
        s->ui, dock_part(s->id, info.id, UINT64_C(0x124b79d89af392e1)),
        (cui_box){.width = width,
                  .max_width = width,
                  .height = height,
                  .row = true,
                  .clip = true,
                  .align_y = CUI_ALIGN_CENTER,
                  .background = context->style ? context->style->background : s->theme->tab_bar});
    cui_begin_box(s->ui, 0, (cui_box){.width = leading});
    cui_end_box(s->ui);
    cui_id title = s->view->items[0].id;
    cui_interact(s->ui, title, CUI_ROLE_LABEL, s->view->items[0].label, s->view->items[0].disabled,
                 false, 0);
    cui_set_semantic_details(
        s->ui, title, (cui_semantic_details){.focusable = true, .pointer_activatable = true});
    float body = fmaxf(0, width - leading - suffix - close_width);
    cui_begin_box(s->ui, title,
                  (cui_box){.width = body,
                            .max_width = body,
                            .fixed_width = body == 0,
                            .height = height,
                            .clip = true,
                            .align_y = CUI_ALIGN_CENTER,
                            .inert = s->view->items[0].disabled});
    dock_title_body(s->ui, &s->view->items[0], 0, context);
    cui_end_box(s->ui);
    if (suffix > 0)
        dock_title_tail(s->ui, context);
    if (close_width > 0 &&
        cui_button_ex(s->ui, cui_dock_close_id(s->id, panel->key), s->theme,
                      cui_string("Close panel"),
                      (cui_button_options){.variant = CUI_BUTTON_GHOST,
                                           .size = CUI_CONTROL_SMALL,
                                           .icon = cui_string("close"),
                                           .icon_only = true,
                                           .disabled = s->options.disabled || panel->disabled})) {
        s->pending_close = true;
        s->close_key = panel->key;
    }
    cui_end_box(s->ui);
}
static bool dock_title_valid(const cui_dock_panel *panel) {
    return isfinite(panel->title_width) && panel->title_width >= 0 &&
           isfinite(panel->title_suffix_width) && panel->title_suffix_width >= 0 &&
           (!panel->title_render || panel->title_width > 0) &&
           (!panel->title_suffix || panel->title_suffix_width > 0);
}
static void dock_render_node(dock_scene *, cui_dock_node, float, float);
static void dock_render_tabs(dock_scene *s, cui_dock_node_info info, dock_cache *cache, float w,
                             float h) {
    cui_id group = cui_dock_group_id(s->id, info.id);
    cui_begin_box(s->ui, group,
                  (cui_box){.width = w,
                            .height = h,
                            .fixed_width = w == 0,
                            .fixed_height = h == 0,
                            .clip = true,
                            .background = s->theme->surface});
    cui_interact(s->ui, group, CUI_ROLE_GROUP, cui_string("Dock tab group"), s->options.disabled,
                 false, 0);
    if (!info.panel_count) {
        cui_text(s->ui, cui_string("Empty workspace"), s->theme->font_size, s->theme->muted);
        cui_end_box(s->ui);
        return;
    }
    uint64_t displayed;
    if (!cui_dock_displayed_panel(s->dock, info.id, s->options.panel_visible,
                                  s->options.visibility_user, &displayed)) {
        cui_text(s->ui, cui_string("No visible panels"), s->theme->font_size, s->theme->muted);
        cui_end_box(s->ui);
        return;
    }
    info.active_panel = displayed;
    info.has_active = true;
    cui_id selected = 0;
    size_t visible_count = 0;
    bool custom_title = false;
    float suffix_width = 0;
    for (size_t i = 0; i < info.panel_count; ++i) {
        uint64_t key;
        if (!cui_dock_panel_at(s->dock, info.id, i, &key))
            continue;
        if (!dock_panel_visible(s->options, s->dock, key)) {
            cui_id focused = cui_focused(s->ui);
            if (focused == cui_dock_tab_id(s->id, key) ||
                focused == cui_dock_close_id(s->id, key)) {
                const cui_dock_panel *fallback = dock_panel_get(s, displayed);
                cui_focus(s->ui, s->options.focus_content && fallback && fallback->focus_target
                                     ? fallback->focus_target
                                     : cui_dock_tab_id(s->id, displayed));
            }
            continue;
        }
        size_t j = visible_count++;
        const cui_dock_panel *panel = dock_panel_get(s, key);
        if (panel) {
            custom_title |= panel->title_render != NULL;
            if (panel->title_suffix && !s->options.collapsed)
                suffix_width = fmaxf(suffix_width, panel->title_suffix_width);
        }
        s->view->keys[j] = key;
        s->view->items[j] =
            (cui_choice_item){.id = cui_dock_tab_id(s->id, key),
                              .label = panel ? panel->title : cui_string("Unavailable panel"),
                              .disabled = s->options.disabled || (panel && panel->disabled)};
        s->view->parts[j] = (cui_tab_parts){0};
        if (panel && panel->closable && !s->options.collapsed &&
            dock_group_closable(s->options, s->dock, info.id))
            s->view->parts[j] =
                (cui_tab_parts){.suffix = dock_close_suffix, .suffix_width = 28, .user = s};
        if (!s->options.collapsed && info.has_active && info.active_panel == key)
            selected = s->view->items[j].id;
    }
    const cui_dock_panel *initial = info.has_active ? dock_panel_get(s, info.active_panel) : NULL;
    bool simple = s->options.panel_style == CUI_DOCK_PANEL_AUTO && visible_count == 1 &&
                  !s->options.collapsed;
    bool hidden_title = simple && initial && initial->hide_title_bar;
    float header_height = hidden_title ? 0 : fminf(simple ? 30 : s->options.tab_height, h);
    bool group_drag = dock_group_draggable(s->options, s->dock, info.id);
    if (!s->options.allow_reorder || s->options.disabled || s->options.collapsed || simple ||
        !group_drag)
        memset(&cache->drag, 0, sizeof cache->drag);
    cui_id strip = dock_part(s->id, info.id, UINT64_C(0x124b79d89af392e1));
    suffix_width = fminf(suffix_width, fmaxf(0, w - 1));
    dock_title_context title_context = {.scene = s,
                                        .selected = &selected,
                                        .suffix_width = suffix_width,
                                        .height = s->options.tab_height,
                                        .style = simple && initial ? initial->title_style : NULL};
    bool changed = false;
    if (simple) {
        if (!hidden_title)
            dock_simple_title(s, info, &title_context, w, header_height);
    } else
        changed = cui_tabs(s->ui, strip, s->theme, s->view->items, visible_count, &selected,
                           (cui_tabs_options){.disabled = s->options.disabled,
                                              .variant = CUI_TAB_DEFAULT,
                                              .height = s->options.tab_height,
                                              .scroll = &cache->scroll,
                                              .bar_extent = fmaxf(1, w - suffix_width),
                                              .render = custom_title ? dock_title_body : NULL,
                                              .render_user = &title_context,
                                              .suffix = suffix_width > 0 ? dock_title_tail : NULL,
                                              .content_user = &title_context,
                                              .reveal_selected = true,
                                              .scrollbar = true,
                                              .parts = s->view->parts,
                                              .parts_count = visible_count,
                                              .drag = s->options.allow_reorder &&
                                                              !s->options.collapsed && group_drag
                                                          ? &cache->drag
                                                          : NULL},
                           NULL, NULL);
    if (!hidden_title) {
        bool touch_drag =
            group_drag && s->options.allow_reorder && !s->options.disabled && !s->options.collapsed;
        for (size_t i = 0; i < visible_count; ++i)
            cui_set_touch_drag_priority(s->ui, s->view->items[i].id,
                                        touch_drag && !s->view->items[i].disabled);
    }
    if (changed) {
        for (size_t i = 0; i < visible_count; ++i)
            if (s->view->items[i].id == selected) {
                uint64_t key = s->view->keys[i];
                if (cui_dock_activate(s->dock, key) == CUI_DOCK_OK) {
                    if (s->view->zoomed)
                        s->view->zoom_key = key;
                    info.active_panel = key;
                    info.has_active = true;
                    s->result.selected = true;
                    s->result.selected_panel = key;
                    dock_emit(s, CUI_DOCK_SELECTED, key, info.id);
                }
                break;
            }
    }
    if (cache->drag.requested) {
        size_t source = SIZE_MAX, target = info.panel_count;
        for (size_t i = 0; i < visible_count; ++i) {
            if (s->view->items[i].id == cache->drag.moved)
                cui_dock_panel_find(s->dock, s->view->keys[i], NULL, &source);
            if (s->view->items[i].id == cache->drag.before)
                cui_dock_panel_find(s->dock, s->view->keys[i], NULL, &target);
        }
        if (source != SIZE_MAX) {
            if (target > source)
                --target;
            if (target != source) {
                s->pending_reorder = true;
                for (size_t i = 0; i < visible_count; ++i)
                    if (s->view->items[i].id == cache->drag.moved)
                        s->reorder_key = s->view->keys[i];
                s->reorder_index = target;
            }
        }
        cache->drag.requested = false;
    }
    cui_id focused = cui_focused(s->ui);
    size_t event_count;
    const cui_event *events = cui_frame_events(s->ui, &event_count);
    const cui_dock_panel *active = info.has_active ? dock_panel_get(s, info.active_panel) : NULL;
    for (size_t i = 0; active && active->closable && !active->disabled && !s->options.disabled &&
                       !s->options.collapsed && dock_group_closable(s->options, s->dock, info.id) &&
                       i < event_count;
         ++i) {
        cui_event e = events[i];
        if (e.type == CUI_EVENT_KEY_DOWN && e.key == CUI_KEY_W && !e.repeat &&
            (e.modifiers & (CUI_MOD_CTRL | CUI_MOD_META)) && !e.ime_composing &&
            !cui_event_consumed(s->ui, i) &&
            (focused == cui_dock_tab_id(s->id, active->key) ||
             focused == cui_dock_close_id(s->id, active->key))) {
            s->pending_close = true;
            s->close_key = active->key;
            cui_consume_event(s->ui, i);
        }
    }
    if (s->options.collapsed) {
        cui_end_box(s->ui);
        return;
    }
    bool toolbar_zoom = active && active->zoomable &&
                        (active->zoom_control == CUI_DOCK_ZOOM_BOTH ||
                         active->zoom_control == CUI_DOCK_ZOOM_TOOLBAR);
    bool controls = active && (!hidden_title || s->view->zoomed) &&
                    (active->toolbar || toolbar_zoom || s->view->zoomed || !s->options.hide_menu);
    float toolbar_height =
        controls ? fminf(s->options.toolbar_height, fmaxf(0, h - header_height)) : 0;
    if (controls) {
        cui_begin_box(s->ui, dock_part(s->id, info.id, UINT64_C(0x2e89ac418f65c15a)),
                      (cui_box){.width = w,
                                .max_width = w,
                                .height = toolbar_height,
                                .fixed_height = toolbar_height == 0,
                                .row = true,
                                .clip = true,
                                .background = s->theme->surface});
        if (active->toolbar) {
            cui_begin_box(s->ui, dock_part(s->id, info.id, UINT64_C(0x4f856e247ac4c168)),
                          (cui_box){.width = -1,
                                    .height = -1,
                                    .clip = true,
                                    .inert = s->options.disabled || active->disabled});
            active->toolbar(s->ui, active->key, active->user);
            cui_end_box(s->ui);
        }
        if (toolbar_zoom || s->view->zoomed) {
            bool zoomed = s->view->zoomed;
            if (cui_button_ex(
                    s->ui, cui_dock_zoom_id(s->id, active->key), s->theme,
                    cui_string(zoomed ? "Restore workspace" : "Maximize panel"),
                    (cui_button_options){.variant = CUI_BUTTON_GHOST,
                                         .size = CUI_CONTROL_SMALL,
                                         .icon = cui_string(zoomed ? "minimize" : "maximize"),
                                         .icon_only = true,
                                         .disabled = !zoomed &&
                                                     (s->options.disabled || active->disabled)})) {
                s->pending_zoom = true;
                s->zoom_in = !zoomed;
                s->zoom_key = active->key;
            }
        }
        dock_render_menu(s, cache, info, active);
        cui_end_box(s->ui);
    }
    float content_height = fmaxf(0, h - header_height - toolbar_height);
    cui_begin_box(s->ui, dock_part(s->id, info.id, UINT64_C(0x3721baf9de47c631)),
                  (cui_box){.width = w,
                            .max_width = w,
                            .height = content_height,
                            .fixed_height = content_height == 0,
                            .clip = true,
                            .inert = s->options.disabled || (active && active->disabled)});
    if (s->options.inner_padding && info.panel_count > 1 &&
        (!active || !active->omit_inner_padding)) {
        cui_begin_box(s->ui, 0, (cui_box){.height = fminf(8, content_height)});
        cui_end_box(s->ui);
    }
    if (active && active->render)
        active->render(s->ui, active->key, active->user);
    else
        cui_text(s->ui, cui_string("Panel content is not registered"), s->theme->font_size,
                 s->theme->muted);
    if (changed && s->options.focus_content && active && active->focus_target)
        cui_focus(s->ui, active->focus_target);
    cui_end_box(s->ui);
    cui_end_box(s->ui);
}
typedef struct dock_child_scene {
    dock_scene *scene;
    cui_dock_node node;
    dock_cache *cache;
    size_t index;
    float cross;
    bool horizontal;
} dock_child_scene;
static void dock_render_child(cui_context *ui, void *user) {
    dock_child_scene *child = user;
    (void)ui;
    float span = child->cache->resize.slots[child->index].size;
    dock_render_node(child->scene, child->node, child->horizontal ? span : child->cross,
                     child->horizontal ? child->cross : span);
}
static bool dock_cache_children(dock_cache *cache, size_t count) {
    if (cache->child_capacity == count)
        return true;
    cui_resizable_slot *slots = calloc(count, sizeof(*slots));
    cui_dock_child_geometry *geometry = calloc(count, sizeof(*geometry));
    cui_resizable_panel *panels = calloc(count, sizeof(*panels));
    dock_child_scene *scenes = calloc(count, sizeof(*scenes));
    cui_dock_slot_size *sizes = calloc(count, sizeof(*sizes));
    if (!slots || !geometry || !panels || !scenes || !sizes) {
        free(slots);
        free(geometry);
        free(panels);
        free(scenes);
        free(sizes);
        return false;
    }
    dock_cache_release_children(cache);
    cache->child_slots = slots;
    cache->child_geometry = geometry;
    cache->child_panels = panels;
    cache->child_scenes = scenes;
    cache->child_sizes = sizes;
    cache->child_capacity = count;
    cui_resizable_state_init(&cache->resize, slots, count);
    return true;
}
static void dock_render_nary(dock_scene *s, cui_dock_node_info info, dock_cache *cache, float w,
                             float h) {
    if (!info.child_count)
        return;
    if (!dock_cache_children(cache, info.child_count)) {
        s->result.error = CUI_DOCK_CAPACITY;
        return;
    }
    bool fit;
    size_t count;
    cui_dock_result error = cui_dock_children_layout(
        s->dock, info.id, (cui_rect){0, 0, w, h}, 0, s->options.panel_visible,
        s->options.visibility_user, cache->child_geometry, cache->child_capacity, &count, &fit);
    if (error != CUI_DOCK_OK) {
        s->result.error = error;
        return;
    }
    s->result.fit_minimum &= fit;
    size_t visible = 0, sole = 0;
    bool changed_visibility = false;
    for (size_t i = 0; i < count; i++) {
        if (cache->child_geometry[i].visible) {
            visible++;
            sole = i;
        }
        if (cache->resize.count == count &&
            (cache->child_slots[i].hidden == cache->child_geometry[i].visible ||
             cache->child_slots[i].id !=
                 dock_part(s->id, cache->child_geometry[i].node, UINT64_C(0x729d1cc28b3a17a1))))
            changed_visibility = true;
    }
    if (changed_visibility)
        cui_resizable_clear(&cache->resize);
    if (visible <= 1) {
        cui_resizable_clear(&cache->resize);
        if (visible)
            dock_render_node(s, cache->child_geometry[sole].node, w, h);
        return;
    }
    bool horizontal = info.kind == CUI_DOCK_SPLIT_HORIZONTAL;
    float extent = horizontal ? w : h, cross = horizontal ? h : w;
    for (size_t i = 0; i < count; i++) {
        cui_dock_child_geometry *geometry = &cache->child_geometry[i];
        float span = horizontal ? geometry->bounds.w : geometry->bounds.h;
        if (cache->resize.count == count && !cache->resize.dragging)
            cache->child_slots[i].size = span;
        cache->child_scenes[i] = (dock_child_scene){s, geometry->node, cache, i, cross, horizontal};
        cui_resizable_panel panel = cui_resizable_panel_default(
            dock_part(s->id, geometry->node, UINT64_C(0x729d1cc28b3a17a1)));
        panel.initial_size = span;
        panel.has_initial_size = true;
        panel.custom_range = true;
        panel.minimum = fit ? (horizontal ? geometry->minimum.width : geometry->minimum.height) : 0;
        panel.maximum = fmaxf(extent, panel.minimum);
        panel.hidden = !geometry->visible;
        panel.content = dock_render_child;
        panel.content_user = &cache->child_scenes[i];
        panel.accessibility_label = cui_string("Dock region");
        cache->child_panels[i] = panel;
    }
    cui_resizable_options options = cui_resizable_options_default();
    options.axis = horizontal ? CUI_RESIZABLE_HORIZONTAL : CUI_RESIZABLE_VERTICAL;
    options.extent = extent;
    options.cross_size = cross;
    options.disabled = s->options.disabled;
    options.key_step = s->options.key_step;
    options.reduced_motion = s->options.reduced_motion;
    options.now_seconds = s->options.now_seconds;
    options.box = (cui_box){
        .width = w, .height = h, .fixed_width = w == 0, .fixed_height = h == 0, .clip = true};
    uint64_t policy_key = 0;
    bool allowed = !s->options.allow_operation ||
                   (dock_first_active(s->dock, info.id, &policy_key, s->options) &&
                    dock_allowed(s, CUI_DOCK_RESIZED, policy_key, info.id, 0, info.kind, false));
    if (!allowed) {
        size_t event_count;
        const cui_event *events = cui_frame_events(s->ui, &event_count);
        for (size_t e = 0; e < event_count; e++) {
            cui_id target = cui_event_target(s->ui, e);
            for (size_t i = 0; i + 1 < count; i++) {
                cui_id handle = cui_resizable_handle_id(cui_dock_group_id(s->id, info.id),
                                                        cache->child_panels[i].id);
                if (target == handle &&
                    (events[e].type == CUI_EVENT_POINTER_DOWN ||
                     (events[e].type == CUI_EVENT_POINTER_MOVE &&
                      cache->resize.active_handle == handle) ||
                     (events[e].type == CUI_EVENT_KEY_DOWN &&
                      (events[e].key == CUI_KEY_LEFT || events[e].key == CUI_KEY_RIGHT ||
                       events[e].key == CUI_KEY_UP || events[e].key == CUI_KEY_DOWN ||
                       events[e].key == CUI_KEY_HOME || events[e].key == CUI_KEY_END))))
                    s->result.denied = true;
            }
        }
    }
    options.disabled |= !allowed;
    cui_resizable_error resize = cui_resizable(s->ui, cui_dock_group_id(s->id, info.id), s->theme,
                                               &cache->resize, cache->child_panels, count, options);
    if (resize != CUI_RESIZABLE_OK && resize != CUI_RESIZABLE_INFEASIBLE) {
        s->result.error = CUI_DOCK_INVALID;
        return;
    }
    bool changed = false;
    for (size_t i = 0; i < count; i++) {
        if (!cui_dock_child_at(s->dock, info.id, i, NULL, &cache->child_sizes[i])) {
            s->result.error = CUI_DOCK_NOT_FOUND;
            return;
        }
        if (!cache->child_geometry[i].visible)
            continue;
        float initial =
            horizontal ? cache->child_geometry[i].bounds.w : cache->child_geometry[i].bounds.h;
        changed |= fabsf(cache->child_slots[i].size - initial) > .001f;
        cache->child_sizes[i] = (cui_dock_slot_size){true, cache->child_slots[i].size};
    }
    if (changed && allowed && !s->options.disabled) {
        if (cui_dock_set_sizes(s->dock, info.id, cache->child_sizes, count) == CUI_DOCK_OK) {
            s->result.resized = true;
            dock_emit(s, CUI_DOCK_RESIZED, 0, info.id);
        }
    }
}
static void dock_render_node(dock_scene *s, cui_dock_node node, float w, float h) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(s->dock, node, &info)) {
        s->result.error = CUI_DOCK_NOT_FOUND;
        return;
    }
    dock_cache *cache = dock_cache_get(s, node);
    if (!cache) {
        s->result.error = CUI_DOCK_CAPACITY;
        return;
    }
    if (info.kind == CUI_DOCK_TABS) {
        bool collapsed = s->options.collapsed;
        s->options.collapsed |= dock_group_constraints(s->options, s->dock, node).collapsed;
        dock_render_tabs(s, info, cache, w, h);
        s->options.collapsed = collapsed;
        return;
    }
    if (info.nary) {
        dock_render_nary(s, info, cache, w, h);
        return;
    }
    bool horizontal = info.kind == CUI_DOCK_SPLIT_HORIZONTAL;
    float extent = horizontal ? w : h, cross = horizontal ? h : w;
    cui_dock_visible_split_geometry filtered;
    cui_dock_result error = cui_dock_split_layout_visible(s->dock, node, (cui_rect){0, 0, w, h}, 0,
                                                          s->options.panel_visible,
                                                          s->options.visibility_user, &filtered);
    if (error != CUI_DOCK_OK) {
        s->result.error = error;
        return;
    }
    cui_dock_split_geometry geometry = filtered.geometry;
    s->result.fit_minimum &= geometry.fit_minimum;
    if (!filtered.first_visible || !filtered.second_visible) {
        cui_resizable_clear(&cache->resize);
        if (filtered.first_visible)
            dock_render_node(s, info.first, w, h);
        if (filtered.second_visible)
            dock_render_node(s, info.second, w, h);
        return;
    }
    float first = horizontal ? geometry.first.w : geometry.first.h;
    if (cache->resize.count == 2 && !cache->resize.dragging) {
        cache->slots[0].size = first;
        cache->slots[1].size = extent - first;
    }
    dock_child_scene children[2] = {{s, info.first, cache, 0, cross, horizontal},
                                    {s, info.second, cache, 1, cross, horizontal}};
    cui_resizable_panel panels[2];
    for (size_t i = 0; i < 2; ++i) {
        cui_size minimum = i == 0 ? geometry.first_minimum : geometry.second_minimum;
        panels[i] = cui_resizable_panel_default(
            dock_part(s->id, children[i].node, UINT64_C(0x729d1cc28b3a17a1)));
        panels[i].initial_size = i == 0 ? first : extent - first;
        panels[i].has_initial_size = true;
        panels[i].custom_range = true;
        panels[i].minimum =
            geometry.fit_minimum ? (horizontal ? minimum.width : minimum.height) : 0;
        panels[i].maximum = fmaxf(extent, panels[i].minimum);
        panels[i].content = dock_render_child;
        panels[i].content_user = &children[i];
        panels[i].accessibility_label =
            cui_string(i == 0 ? "First dock region" : "Second dock region");
    }
    cui_resizable_options options = cui_resizable_options_default();
    options.axis = horizontal ? CUI_RESIZABLE_HORIZONTAL : CUI_RESIZABLE_VERTICAL;
    options.extent = extent;
    options.cross_size = cross;
    options.disabled = s->options.disabled;
    options.key_step = s->options.key_step;
    options.reduced_motion = s->options.reduced_motion;
    options.now_seconds = s->options.now_seconds;
    options.box.clip = true;
    options.box.fixed_width = w == 0;
    options.box.fixed_height = h == 0;
    options.box.width = w;
    options.box.height = h;
    cui_resizable_error resize = cui_resizable(s->ui, cui_dock_group_id(s->id, node), s->theme,
                                               &cache->resize, panels, 2, options);
    if (resize != CUI_RESIZABLE_OK && resize != CUI_RESIZABLE_INFEASIBLE) {
        s->result.error = CUI_DOCK_INVALID;
        return;
    }
    if (extent > 0) {
        double ratio = fmin(.999999, fmax(.000001, cache->slots[0].size / extent));
        if (fabs(ratio - info.ratio) > 1e-6 && fabsf(cache->slots[0].size - first) > .001f) {
            if (cui_dock_set_ratio(s->dock, node, ratio) == CUI_DOCK_OK) {
                s->result.resized = true;
                dock_emit(s, CUI_DOCK_RESIZED, 0, node);
            }
        }
    }
}
static bool dock_custom_menu_valid(cui_id view, uint64_t key, const cui_menu_entry *entries,
                                   size_t count, size_t depth,
                                   cui_id ids[CUI_DOCK_CUSTOM_MENU_LIMIT], size_t *used,
                                   size_t *total) {
    if ((count && !entries) || depth >= CUI_MENU_DEPTH_MAX)
        return false;
    for (size_t i = 0; i < count; ++i) {
        const cui_menu_entry *entry = &entries[i];
        if (++*total > CUI_DOCK_CUSTOM_MENU_LIMIT || entry->kind < CUI_MENU_ACTION ||
            entry->kind > CUI_MENU_SUBMENU)
            return false;
        if ((entry->label.length && !entry->label.data) ||
            (entry->icon.length && !entry->icon.data) ||
            (entry->shortcut.length && !entry->shortcut.data))
            return false;
        if (entry->kind != CUI_MENU_SEPARATOR && entry->kind != CUI_MENU_LABEL) {
            if (!entry->id)
                return false;
            for (size_t j = 0; j < *used; ++j)
                if (ids[j] == entry->id)
                    return false;
            for (unsigned j = 0; j < 7; ++j)
                if (entry->id == cui_dock_menu_action_id(view, key, (cui_dock_menu_action)j))
                    return false;
            ids[(*used)++] = entry->id;
        }
        if (entry->child_count &&
            !dock_custom_menu_valid(view, key, entry->children, entry->child_count, depth + 1, ids,
                                    used, total))
            return false;
    }
    return true;
}
cui_dock_view_result cui_dock_view_render(cui_context *ui, cui_id id, const cui_theme *theme,
                                          cui_dock *model, cui_dock_view *view,
                                          const cui_dock_panel *panels, size_t count,
                                          cui_dock_view_options options) {
    cui_dock_view_result invalid = {.error = CUI_DOCK_INVALID};
    if (!ui || !id || !theme || !model || !view || (count && !panels) ||
        count > CUI_DOCK_PANEL_LIMIT || !isfinite(options.width) || options.width <= 0 ||
        !isfinite(options.height) || options.height <= 0 || !isfinite(options.tab_height) ||
        options.tab_height < 0 || !isfinite(options.key_step) || options.key_step < 0 ||
        !isfinite(options.now_seconds) || !isfinite(options.toolbar_height) ||
        options.toolbar_height < 0 || options.panel_style < CUI_DOCK_PANEL_TAB_BAR ||
        options.panel_style > CUI_DOCK_PANEL_AUTO)
        return invalid;
    if (!options.tab_height)
        options.tab_height = 36;
    if (!options.toolbar_height)
        options.toolbar_height = 32;
    if (!options.key_step)
        options.key_step = 8;
    for (size_t i = 0; i < count; ++i) {
        cui_id custom_ids[CUI_DOCK_CUSTOM_MENU_LIMIT];
        size_t used = 0, total = 0;
        if (!dock_custom_menu_valid(id, panels[i].key, panels[i].menu_items, panels[i].menu_count,
                                    0, custom_ids, &used, &total))
            return invalid;
        if (!dock_title_valid(&panels[i]))
            return invalid;
        if (panels[i].zoom_control < CUI_DOCK_ZOOM_BOTH ||
            panels[i].zoom_control > CUI_DOCK_ZOOM_NONE)
            return invalid;
        if (panels[i].title.length && !panels[i].title.data)
            return invalid;
        for (size_t j = 0; j < i; ++j)
            if (panels[i].key == panels[j].key)
                return invalid;
    }
    size_t nodes = 0, model_panels = 0;
    cui_dock_result error = dock_preflight(model, cui_dock_root(model), &nodes, &model_panels);
    if (error != CUI_DOCK_OK)
        return (cui_dock_view_result){.error = error};
    if (nodes > view->node_capacity || model_panels > view->panel_capacity)
        return (cui_dock_view_result){.error = CUI_DOCK_CAPACITY};
    if ((view->model && view->model != model) || (view->id && view->id != id))
        cui_dock_view_reset(view);
    view->model = model;
    view->id = id;
    for (size_t i = 0; i < view->count; i++) {
        cui_dock_node_info cached;
        if (!cui_dock_node_get(model, view->cache[i].node, &cached))
            dock_cache_release_children(&view->cache[i]);
    }
    dock_scene scene = {.ui = ui,
                        .id = id,
                        .theme = theme,
                        .dock = model,
                        .view = view,
                        .panels = panels,
                        .panel_count = count,
                        .options = options,
                        .result = {.error = CUI_DOCK_OK, .fit_minimum = true}};
    dock_drag_events(&scene);
    size_t leaves = 0;
    bool fit = true;
    error = cui_dock_layout_visible(model, (cui_rect){0, 0, options.width, options.height}, 0,
                                    options.panel_visible, options.visibility_user, NULL, 0,
                                    &leaves, &fit);
    if (error != CUI_DOCK_OK && error != CUI_DOCK_CAPACITY)
        return (cui_dock_view_result){.error = error};
    scene.result.fit_minimum = fit;
    cui_begin_box(ui, id,
                  (cui_box){.width = options.width,
                            .height = options.height,
                            .clip = true,
                            .background = theme->background});
    cui_interact(ui, id, CUI_ROLE_GROUP, cui_string("Dock workspace"), options.disabled, false, 0);
    cui_dock_node visible_root = cui_dock_root(model);
    if (view->zoomed) {
        if (cui_dock_panel_find(model, view->zoom_key, &visible_root, NULL) &&
            cui_dock_visible_count(model, visible_root, options.panel_visible,
                                   options.visibility_user)) {
            cui_dock_node_info info;
            if (!cui_dock_node_get(model, visible_root, &info)) {
                cui_end_box(ui);
                return (cui_dock_view_result){.error = CUI_DOCK_NOT_FOUND};
            }
            scene.result.fit_minimum =
                options.width >= info.min_width && options.height >= info.min_height;
        } else {
            view->zoomed = false;
            scene.result.restored = true;
            dock_emit(&scene, CUI_DOCK_RESTORED, view->zoom_key, 0);
            uint64_t survivor;
            if (dock_first_active(model, cui_dock_root(model), &survivor, options))
                cui_focus(ui, cui_dock_tab_id(id, survivor));
            visible_root = cui_dock_root(model);
        }
    }
    dock_render_node(&scene, visible_root, options.width, options.height);
    dock_drag_preview(&scene);
    cui_end_box(ui);
    if (scene.pending_zoom) {
        if (scene.zoom_in && !dock_allowed(&scene, CUI_DOCK_ZOOMED, scene.zoom_key, visible_root, 0,
                                           CUI_DOCK_TABS, false))
            scene.result.denied = true;
        else {
            view->zoomed = scene.zoom_in;
            view->zoom_key = scene.zoom_key;
            scene.result.zoomed = scene.zoom_in;
            scene.result.restored = !scene.zoom_in;
            dock_emit(&scene, scene.zoom_in ? CUI_DOCK_ZOOMED : CUI_DOCK_RESTORED, scene.zoom_key,
                      visible_root);
            cui_focus(ui, cui_dock_tab_id(id, scene.zoom_key));
            cui_invalidate(ui);
        }
    } else if (scene.pending_close) {
        cui_dock_node node;
        if (!dock_allowed(&scene, CUI_DOCK_CLOSED, scene.close_key, 0, 0, CUI_DOCK_TABS, false))
            scene.result.denied = true;
        else if (cui_dock_panel_find(model, scene.close_key, &node, NULL) &&
                 cui_dock_remove(model, scene.close_key) == CUI_DOCK_OK) {
            scene.result.closed = true;
            scene.result.closed_panel = scene.close_key;
            scene.result.closed_count = 1;
            dock_emit(&scene, CUI_DOCK_CLOSED, scene.close_key, node);
            uint64_t survivor;
            if (cui_dock_displayed_panel(model, node, options.panel_visible,
                                         options.visibility_user, &survivor))
                cui_focus(ui, cui_dock_tab_id(id, survivor));
            else if (dock_first_active(model, cui_dock_root(model), &survivor, options))
                cui_focus(ui, cui_dock_tab_id(id, survivor));
            cui_invalidate(ui);
        }
    } else if (scene.pending_bulk) {
        cui_dock_node_info info;
        size_t count = 0;
        if (cui_dock_node_get(model, scene.menu_node, &info))
            for (size_t i = 0; i < info.panel_count; ++i) {
                uint64_t key;
                if (!cui_dock_panel_at(model, scene.menu_node, i, &key))
                    continue;
                const cui_dock_panel *panel = dock_panel_get(&scene, key);
                if ((scene.keep_active && key == scene.menu_key) || !panel || !panel->closable ||
                    panel->disabled || !dock_panel_visible(scene.options, model, key))
                    continue;
                if (!dock_allowed(&scene, CUI_DOCK_CLOSED, key, 0, 0, CUI_DOCK_TABS, false)) {
                    scene.result.denied = true;
                    break;
                }
                view->keys[count++] = key;
            }
        if (!scene.result.denied && count) {
            scene.result.error = cui_dock_remove_many(model, view->keys, count);
            if (scene.result.error == CUI_DOCK_OK) {
                scene.result.closed = true;
                scene.result.closed_count = count;
                scene.result.closed_panel = view->keys[count - 1];
                for (size_t i = 0; i < count; ++i)
                    dock_emit(&scene, CUI_DOCK_CLOSED, view->keys[i], scene.menu_node);
                uint64_t survivor;
                if (dock_first_active(model, cui_dock_root(model), &survivor, options))
                    cui_focus(ui, cui_dock_tab_id(id, survivor));
                cui_invalidate(ui);
            }
        }
    } else if (scene.pending_menu_split) {
        if (!dock_allowed(&scene, CUI_DOCK_SPLIT, scene.menu_key, scene.menu_node, SIZE_MAX,
                          scene.menu_axis, false))
            scene.result.denied = true;
        else if (nodes + 2 > view->node_capacity)
            scene.result.error = CUI_DOCK_CAPACITY;
        else {
            cui_dock_node destination;
            scene.result.error = cui_dock_move_split(model, scene.menu_key, scene.menu_node,
                                                     scene.menu_axis, .5, false, &destination);
            if (scene.result.error == CUI_DOCK_OK) {
                scene.result.moved = scene.result.split = true;
                scene.result.moved_panel = scene.menu_key;
                dock_emit(&scene, CUI_DOCK_SPLIT, scene.menu_key, destination);
                cui_focus(ui, cui_dock_tab_id(id, scene.menu_key));
                cui_invalidate(ui);
            }
        }
    } else if (scene.pending_balance) {
        cui_dock_node_info info;
        if (cui_dock_node_get(model, scene.menu_node, &info) && info.parent) {
            if (!dock_allowed(&scene, CUI_DOCK_RESIZED, scene.menu_key, info.parent, 0,
                              CUI_DOCK_TABS, false))
                scene.result.denied = true;
            else if (cui_dock_balance(model, info.parent) == CUI_DOCK_OK) {
                scene.result.resized = true;
                dock_emit(&scene, CUI_DOCK_RESIZED, scene.menu_key, info.parent);
                cui_invalidate(ui);
            }
        }
    } else if (scene.pending_custom) {
        const cui_dock_panel *panel = dock_panel_get(&scene, scene.menu_key);
        if (panel && panel->menu_action)
            panel->menu_action(panel->user, scene.menu_key, scene.custom_action);
    } else if (scene.pending_drop) {
        cui_dock_node destination = scene.destination;
        cui_dock_result drop;
        if (!dock_allowed(
                &scene, scene.edge_axis == CUI_DOCK_TABS ? CUI_DOCK_MOVED : CUI_DOCK_SPLIT,
                view->drag_key, destination, SIZE_MAX, scene.edge_axis, scene.edge_before)) {
            scene.result.denied = true;
            drop = CUI_DOCK_OK;
        } else if (scene.edge_axis == CUI_DOCK_TABS)
            drop = cui_dock_move(model, view->drag_key, destination, SIZE_MAX);
        else if (nodes + 2 > view->node_capacity)
            drop = CUI_DOCK_CAPACITY;
        else
            drop = cui_dock_move_split(model, view->drag_key, destination, scene.edge_axis, .5,
                                       scene.edge_before, &destination);
        if (drop == CUI_DOCK_OK && !scene.result.denied) {
            scene.result.moved = true;
            scene.result.split = scene.edge_axis != CUI_DOCK_TABS;
            scene.result.moved_panel = view->drag_key;
            dock_emit(&scene, scene.result.split ? CUI_DOCK_SPLIT : CUI_DOCK_MOVED, view->drag_key,
                      destination);
            cui_focus(ui, cui_dock_tab_id(id, view->drag_key));
            cui_invalidate(ui);
        } else
            scene.result.error = drop;
    } else if (scene.pending_reorder) {
        cui_dock_node node;
        if (!dock_allowed(&scene, CUI_DOCK_REORDERED, scene.reorder_key, 0, scene.reorder_index,
                          CUI_DOCK_TABS, false))
            scene.result.denied = true;
        else if (cui_dock_panel_find(model, scene.reorder_key, &node, NULL) &&
                 cui_dock_reorder(model, scene.reorder_key, scene.reorder_index) == CUI_DOCK_OK) {
            scene.result.reordered = true;
            scene.result.reordered_panel = scene.reorder_key;
            dock_emit(&scene, CUI_DOCK_REORDERED, scene.reorder_key, node);
            cui_invalidate(ui);
        }
    }
    return scene.result;
}

struct cui_dock_workspace {
    cui_dock_workspace_snapshot snapshot;
    cui_rect bounds;
    bool locked;
    bool schema_present[3], has_version;
    size_t version;
    cui_resizable_state resize[3];
    cui_resizable_slot slots[3][2];
    cui_id id;
    bool dragging, crossed, pending_focus;
    cui_dock_region source_region, focus_region;
    uint64_t drag_key, focus_key, initial_active;
    bool has_initial_active, cross_region;
    bool bottom_gesture;
    cui_dock_workspace_snapshot bottom_initial;
    float start_x, start_y, pointer_x, pointer_y;
};
cui_dock_workspace *cui_dock_workspace_create(void) {
    cui_dock_workspace *state = calloc(1, sizeof *state);
    if (!state)
        return NULL;
    state->snapshot.size[0] = state->snapshot.size[1] = 200;
    state->snapshot.size[2] = 160;
    for (size_t i = 0; i < 3; ++i)
        cui_resizable_state_init(&state->resize[i], state->slots[i], 2);
    return state;
}
void cui_dock_workspace_destroy(cui_dock_workspace *state) {
    free(state);
}
void cui_dock_workspace_set_locked(cui_dock_workspace *state, bool locked) {
    if (!state || state->locked == locked)
        return;
    state->locked = locked;
    if (locked) {
        state->dragging = false;
        state->crossed = false;
    }
}
bool cui_dock_workspace_locked(const cui_dock_workspace *state) {
    return state && state->locked;
}
cui_rect cui_dock_workspace_bounds(const cui_dock_workspace *state) {
    return state ? state->bounds : (cui_rect){0};
}
bool cui_dock_workspace_set_edge(cui_dock_workspace *state, cui_dock_region edge, bool open,
                                 float size) {
    if (!state || edge < CUI_DOCK_LEFT || edge > CUI_DOCK_BOTTOM || !isfinite(size) || size <= 0)
        return false;
    if (!open && state->snapshot.noncollapsible[edge - 1] && state->snapshot.open[edge - 1])
        return false;
    if (edge == CUI_DOCK_BOTTOM)
        state->bottom_gesture = false;
    state->schema_present[edge - 1] = true;
    state->snapshot.open[edge - 1] = open;
    state->snapshot.size[edge - 1] = size;
    cui_resizable_clear(&state->resize[edge - 1]);
    return true;
}
bool cui_dock_workspace_set_collapsible(cui_dock_workspace *state, cui_dock_region edge,
                                        bool collapsible) {
    if (!state || edge < CUI_DOCK_LEFT || edge > CUI_DOCK_BOTTOM)
        return false;
    state->snapshot.noncollapsible[edge - 1] = !collapsible;
    if (edge == CUI_DOCK_BOTTOM)
        state->bottom_gesture = false;
    cui_resizable_clear(&state->resize[edge - 1]);
    return true;
}
bool cui_dock_workspace_save(const cui_dock_workspace *state, cui_dock_workspace_snapshot *out) {
    if (!state || !out)
        return false;
    *out = state->snapshot;
    return true;
}
bool cui_dock_workspace_restore(cui_dock_workspace *state,
                                const cui_dock_workspace_snapshot *saved) {
    if (!state || !saved)
        return false;
    for (size_t i = 0; i < 3; ++i)
        if (!isfinite(saved->size[i]) || saved->size[i] <= 0)
            return false;
    state->snapshot = *saved;
    for (size_t i = 0; i < 3; i++)
        state->schema_present[i] |= saved->open[i];
    state->bottom_gesture = false;
    state->dragging = false;
    state->pending_focus = false;
    for (size_t i = 0; i < 3; ++i)
        cui_resizable_clear(&state->resize[i]);
    return true;
}
/* Internal base transaction adds renderer-capacity preflight before publication. */
extern cui_dock_result cui_dock_private_schema_area_import_models(cui_dock *const models[4],
                                                                  const cui_dock_schema_area *,
                                                                  cui_dock_schema_key_fn, void *,
                                                                  const size_t node_limits[4],
                                                                  const size_t panel_limits[4]);
void cui_dock_workspace_set_version(cui_dock_workspace *state, bool has_version, size_t version) {
    if (state) {
        state->has_version = has_version;
        state->version = version;
    }
}
bool cui_dock_workspace_version(const cui_dock_workspace *state, size_t *version) {
    if (!state || !state->has_version)
        return false;
    if (version)
        *version = state->version;
    return true;
}
cui_dock_result cui_dock_workspace_schema_import(cui_dock_workspace *state,
                                                 const cui_dock_region_content regions[4],
                                                 const cui_dock_schema_area *area,
                                                 cui_dock_schema_key_fn key, void *user) {
    if (!state || !regions || !area)
        return CUI_DOCK_INVALID;
    cui_dock_workspace_snapshot snapshot = {.size = {200, 200, 160}};
    bool present[3] = {false};
    for (size_t i = 0; i < 3; i++) {
        if (!area->edge_present[i])
            continue;
        const cui_dock_schema_edge *edge = &area->edges[i];
        if (edge->placement < CUI_DOCK_LEFT || edge->placement > CUI_DOCK_BOTTOM ||
            !isfinite(edge->size) || edge->size <= 0 || edge->size > 1e9)
            return CUI_DOCK_INVALID;
        size_t target = (size_t)edge->placement - 1;
        snapshot.open[target] = edge->open;
        snapshot.size[target] = (float)edge->size;
        present[target] = true;
    }
    cui_dock *models[4];
    size_t node_limits[4], panel_limits[4];
    for (size_t i = 0; i < 4; i++) {
        models[i] = regions[i].model;
        node_limits[i] = regions[i].view ? regions[i].view->node_capacity : SIZE_MAX;
        panel_limits[i] = regions[i].view ? regions[i].view->panel_capacity : SIZE_MAX;
        if (regions[i].view)
            for (size_t j = 0; j < i; j++)
                if (regions[i].view == regions[j].view)
                    return CUI_DOCK_INVALID;
    }
    cui_dock_result result = cui_dock_private_schema_area_import_models(models, area, key, user,
                                                                        node_limits, panel_limits);
    if (result != CUI_DOCK_OK)
        return result;
    cui_dock_workspace_restore(state, &snapshot);
    memcpy(state->schema_present, present, sizeof present);
    state->has_version = area->has_version;
    state->version = area->version;
    for (size_t i = 0; i < 4; i++)
        if (regions[i].view)
            cui_dock_view_reset(regions[i].view);
    return CUI_DOCK_OK;
}
static void dock_schema_resolve_sizes(const cui_dock *model, const cui_dock_view *view,
                                      cui_dock_node id, const cui_dock_schema_node *node,
                                      cui_dock_schema_buffer *buffer) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(model, id, &info) || info.kind == CUI_DOCK_TABS)
        return;
    const dock_cache *measured = NULL;
    if (view && view->model == model)
        for (size_t c = 0; c < view->count; c++) {
            const dock_cache *candidate = &view->cache[c];
            if (candidate->node != id || candidate->child_capacity != info.child_count ||
                candidate->resize.count != info.child_count)
                continue;
            bool same_children = true;
            for (size_t i = 0; i < info.child_count; i++) {
                cui_dock_node child;
                same_children &= cui_dock_child_at(model, id, i, &child, NULL) &&
                                 candidate->child_geometry[i].node == child;
            }
            if (same_children)
                measured = candidate;
            break;
        }
    for (size_t i = 0; i < info.child_count; i++) {
        cui_dock_node child;
        if (!cui_dock_child_at(model, id, i, &child, NULL))
            continue;
        double size = node->sizes[i];
        if (measured) {
            double screen = measured->child_slots[i].size;
            if (isfinite(screen) && screen > 0)
                size = screen;
        }
        size_t slot = (size_t)(node->sizes - buffer->sizes) + i;
        buffer->sizes[slot] = isfinite(size) && size > 0 ? size : 100.0;
        dock_schema_resolve_sizes(model, view, child, &node->children[i], buffer);
    }
}
cui_dock_result cui_dock_workspace_schema_export(const cui_dock_workspace *state,
                                                 const cui_dock_region_content regions[4],
                                                 cui_dock_schema_dump_fn dump, void *user,
                                                 cui_dock_schema_buffer buffers[4],
                                                 cui_dock_schema_area *output) {
    if (!state || !regions)
        return CUI_DOCK_INVALID;
    const cui_dock *models[4];
    cui_dock_schema_area metadata = {.has_version = state->has_version, .version = state->version};
    for (size_t i = 0; i < 4; i++)
        models[i] = regions[i].model;
    for (size_t i = 0; i < 3; i++) {
        metadata.edge_present[i] = state->schema_present[i];
        metadata.edges[i] = (cui_dock_schema_edge){.placement = (cui_dock_region)(i + 1),
                                                   .size = state->snapshot.size[i],
                                                   .open = state->snapshot.open[i]};
    }
    cui_dock_result result =
        cui_dock_schema_area_export_models(models, &metadata, dump, user, buffers, output);
    if (result == CUI_DOCK_OK && buffers[0].nodes)
        for (size_t i = 0; i < 4; i++) {
            if (i && !metadata.edge_present[i - 1])
                continue;
            const cui_dock_schema_node *tree = i ? &output->edges[i - 1].panel : &output->center;
            dock_schema_resolve_sizes(models[i], regions[i].view, cui_dock_root(models[i]), tree,
                                      &buffers[i]);
        }
    return result;
}
cui_id cui_dock_region_id(cui_id workspace, cui_dock_region region) {
    if (region < CUI_DOCK_CENTER || region > CUI_DOCK_BOTTOM)
        return 0;
    return dock_part(workspace, (uint64_t)region, UINT64_C(0x87213591a36ef249));
}
cui_id cui_dock_edge_resize_id(cui_id workspace, cui_dock_region region) {
    if (region < CUI_DOCK_LEFT || region > CUI_DOCK_BOTTOM)
        return 0;
    cui_id group = dock_part(workspace, (uint64_t)region, UINT64_C(0x40c96f5e8193287b));
    cui_id preceding = dock_part(workspace, (uint64_t)region, UINT64_C(0x61da9f37b4f591d8));
    return cui_resizable_handle_id(group, preceding);
}
typedef struct {
    cui_context *ui;
    cui_id id;
    const cui_theme *theme;
    cui_dock_workspace *state;
    const cui_dock_region_content *regions;
    cui_dock_workspace_options options;
    cui_dock_workspace_result result;
    bool pending_transfer;
    cui_dock_transfer_request transfer;
    cui_dock_group_constraints (*host_constraints)(void *, const cui_dock *, cui_dock_node);
    void *host_constraints_user;
} dock_workspace_scene;
static cui_dock_group_constraints dock_workspace_constraints(void *user, const cui_dock *model,
                                                             cui_dock_node node) {
    const dock_workspace_scene *scene = user;
    cui_dock_group_constraints result =
        scene->host_constraints ? scene->host_constraints(scene->host_constraints_user, model, node)
                                : (cui_dock_group_constraints){0};
    result.locked = true;
    return result;
}
static const cui_dock_panel *dock_region_panel(const cui_dock_region_content *region,
                                               uint64_t key) {
    for (size_t i = 0; i < region->panel_count; ++i)
        if (region->panels[i].key == key)
            return &region->panels[i];
    return NULL;
}
static bool dock_region_ready(const cui_dock_region_content *region) {
    return region->model && region->view && (!region->panel_count || region->panels);
}
static bool dock_leaf_at(dock_workspace_scene *s, unsigned region, cui_dock_node node, float x,
                         float y, cui_dock_node *out) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(s->regions[region].model, node, &info))
        return false;
    if (info.kind != CUI_DOCK_TABS) {
        for (size_t i = 0; i < info.child_count; i++) {
            cui_dock_node child;
            if (!cui_dock_child_at(s->regions[region].model, node, i, &child, NULL))
                continue;
            if (dock_leaf_at(s, region, child, x, y, out))
                return true;
        }
        return false;
    }
    if (!dock_contains(
            cui_bounds(s->ui,
                       cui_dock_group_id(cui_dock_region_id(s->id, (cui_dock_region)region), node)),
            x, y))
        return false;
    *out = node;
    return true;
}
static cui_dock_node dock_first_leaf(const cui_dock *model, cui_dock_node node) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(model, node, &info))
        return 0;
    return info.kind == CUI_DOCK_TABS ? node : dock_first_leaf(model, info.first);
}
static bool dock_workspace_target(dock_workspace_scene *s, cui_dock_transfer_request *request) {
    cui_dock_workspace *state = s->state;
    cui_rect area = cui_bounds(s->ui, s->id);
    if (!dock_contains(area, state->pointer_x, state->pointer_y))
        return false;
    unsigned destination = 4;
    bool opens = false;
    float band = fminf(24, fminf(area.w, area.h) * .2f);
    if (!s->options.disable_edge_opening) {
        if (!state->snapshot.open[0] && state->pointer_x < area.x + band)
            destination = 1;
        else if (!state->snapshot.open[1] && state->pointer_x > area.x + area.w - band)
            destination = 2;
        else if (!state->snapshot.open[2] && state->pointer_y > area.y + area.h - band)
            destination = 3;
        opens = destination < 4;
    }
    if (destination == 4)
        for (unsigned i = 0; i < 4; ++i)
            if ((!i || state->snapshot.open[i - 1]) &&
                dock_contains(cui_bounds(s->ui, cui_dock_region_id(s->id, (cui_dock_region)i)),
                              state->pointer_x, state->pointer_y)) {
                destination = i;
                break;
            }
    if (destination == 4 || destination == (unsigned)state->source_region ||
        !dock_region_ready(&s->regions[destination]))
        return false;
    const cui_dock_region_content *source = &s->regions[state->source_region],
                                  *target = &s->regions[destination];
    const cui_dock_panel *panel = dock_region_panel(source, state->drag_key);
    if (!panel || panel->disabled || !dock_region_ready(source) ||
        !dock_panel_visible(s->options.view, source->model, state->drag_key))
        return false;
    cui_dock_node source_node, target_node;
    if (!cui_dock_panel_find(source->model, state->drag_key, &source_node, NULL))
        return false;
    if (!dock_group_draggable(s->options.view, source->model, source_node))
        return false;
    if (opens)
        target_node = dock_first_leaf(target->model, cui_dock_root(target->model));
    else if (!dock_leaf_at(s, destination, cui_dock_root(target->model), state->pointer_x,
                           state->pointer_y, &target_node))
        return false;
    *request = (cui_dock_transfer_request){state->source_region,
                                           (cui_dock_region)destination,
                                           state->drag_key,
                                           source->model,
                                           target->model,
                                           source_node,
                                           target_node,
                                           CUI_DOCK_TABS,
                                           false,
                                           opens};
    if (!opens) {
        dock_scene probe = {.ui = s->ui,
                            .id = cui_dock_region_id(s->id, (cui_dock_region)destination),
                            .dock = target->model,
                            .view = target->view,
                            .options = s->options.view};
        if (!probe.options.tab_height)
            probe.options.tab_height = 36;
        /* Derive edge placement from target geometry without requiring panel membership. */
        cui_rect b = cui_bounds(s->ui, cui_dock_group_id(probe.id, target_node));
        if (state->pointer_y >= b.y + probe.options.tab_height) {
            float dx = fminf(32, b.w * .2f), dy = fminf(32, b.h * .2f);
            if (state->pointer_x < b.x + dx || state->pointer_x > b.x + b.w - dx) {
                request->axis = CUI_DOCK_SPLIT_HORIZONTAL;
                request->before = state->pointer_x < b.x + dx;
            } else if (state->pointer_y < b.y + probe.options.tab_height + dy ||
                       state->pointer_y > b.y + b.h - dy) {
                request->axis = CUI_DOCK_SPLIT_VERTICAL;
                request->before = state->pointer_y < b.y + probe.options.tab_height + dy;
            }
        }
    }
    return true;
}
static void dock_workspace_restore_active(dock_workspace_scene *s) {
    cui_dock_workspace *state = s->state;
    cui_dock *model = s->regions[state->source_region].model;
    if (!state->has_initial_active || !model)
        return;
    cui_dock_node node;
    cui_dock_node_info info;
    if (!cui_dock_panel_find(model, state->initial_active, &node, NULL) ||
        !cui_dock_node_get(model, node, &info))
        return;
    if (info.active_panel != state->initial_active &&
        cui_dock_activate(model, state->initial_active) == CUI_DOCK_OK) {
        if (s->options.view.on_event)
            s->options.view.on_event(s->options.view.event_user, CUI_DOCK_SELECTED,
                                     state->initial_active, node);
        s->result.regions[state->source_region].selected = true;
        s->result.regions[state->source_region].selected_panel = state->initial_active;
        cui_invalidate(s->ui);
    }
    state->pending_focus = true;
    state->focus_region = state->source_region;
    state->focus_key = state->initial_active;
}
static void dock_workspace_events(dock_workspace_scene *s) {
    cui_dock_workspace *state = s->state;
    if (s->options.view.disabled || !s->options.view.allow_reorder ||
        s->options.disable_cross_region) {
        if (state->dragging && state->crossed && state->cross_region)
            dock_workspace_restore_active(s);
        state->dragging = false;
        return;
    }
    if (state->dragging) {
        cui_dock_node source;
        const cui_dock_region_content *region = &s->regions[state->source_region];
        if (!dock_panel_visible(s->options.view, region->model, state->drag_key) ||
            !cui_dock_panel_find(region->model, state->drag_key, &source, NULL) ||
            !dock_group_draggable(s->options.view, region->model, source)) {
            if (state->crossed && state->cross_region)
                dock_workspace_restore_active(s);
            state->dragging = false;
        }
    }
    if (state->dragging && state->source_region != CUI_DOCK_CENTER &&
        !state->snapshot.open[state->source_region - 1]) {
        if (state->crossed && state->cross_region)
            dock_workspace_restore_active(s);
        state->dragging = false;
    }
    size_t count;
    const cui_event *events = cui_frame_events(s->ui, &count);
    for (size_t e = 0; e < count; ++e) {
        cui_event event = events[e];
        if (event.type == CUI_EVENT_CANCEL ||
            (event.type == CUI_EVENT_KEY_DOWN && event.key == CUI_KEY_ESCAPE)) {
            if (state->dragging && state->crossed && state->cross_region)
                dock_workspace_restore_active(s);
            state->dragging = false;
        } else if (event.type == CUI_EVENT_POINTER_DOWN &&
                   event.pointer_button == CUI_POINTER_PRIMARY) {
            state->dragging = false;
            if (!cui_event_hit_within_scope(s->ui, e, s->id))
                continue;
            for (unsigned r = 0; r < 4; ++r) {
                if ((r && !state->snapshot.open[r - 1]) || !dock_region_ready(&s->regions[r]))
                    continue;
                cui_id view_id = cui_dock_region_id(s->id, (cui_dock_region)r);
                for (size_t p = 0; p < s->regions[r].panel_count; ++p) {
                    const cui_dock_panel *panel = &s->regions[r].panels[p];
                    cui_dock_node source_node;
                    if (!panel->disabled &&
                        dock_panel_visible(s->options.view, s->regions[r].model, panel->key) &&
                        cui_dock_panel_find(s->regions[r].model, panel->key, &source_node, NULL) &&
                        dock_group_draggable(s->options.view, s->regions[r].model, source_node) &&
                        dock_contains(cui_bounds(s->ui, cui_dock_tab_id(view_id, panel->key)),
                                      event.x, event.y) &&
                        dock_contains(cui_bounds(s->ui, view_id), event.x, event.y) &&
                        !dock_contains(cui_bounds(s->ui, cui_dock_close_id(view_id, panel->key)),
                                       event.x, event.y)) {
                        state->dragging = true;
                        state->crossed = false;
                        state->drag_key = panel->key;
                        state->source_region = (cui_dock_region)r;
                        cui_dock_node node;
                        cui_dock_node_info info;
                        if (!cui_dock_panel_find(s->regions[r].model, panel->key, &node, NULL) ||
                            !cui_dock_node_get(s->regions[r].model, node, &info)) {
                            state->dragging = false;
                            continue;
                        }
                        state->has_initial_active = info.has_active;
                        state->initial_active = info.active_panel;
                        state->cross_region = false;
                        state->start_x = event.x;
                        state->start_y = event.y;
                    }
                }
            }
        } else if (state->dragging &&
                   (event.type == CUI_EVENT_POINTER_MOVE || event.type == CUI_EVENT_POINTER_UP)) {
            state->pointer_x = event.x;
            state->pointer_y = event.y;
            state->cross_region =
                !dock_contains(cui_bounds(s->ui, cui_dock_region_id(s->id, state->source_region)),
                               event.x, event.y);
            if (hypotf(event.x - state->start_x, event.y - state->start_y) >= 6)
                state->crossed = true;
            cui_dock_transfer_request intent;
            if (state->crossed && dock_workspace_target(s, &intent))
                state->cross_region = true;
            if (event.type == CUI_EVENT_POINTER_UP && event.pointer_button == CUI_POINTER_PRIMARY) {
                if (state->crossed && cui_event_hit_within_scope(s->ui, e, s->id))
                    s->pending_transfer = dock_workspace_target(s, &s->transfer);
                state->dragging = false;
            }
        }
    }
}
typedef struct {
    dock_workspace_scene *scene;
    unsigned level, slot;
    bool edge;
    float cross;
} dock_workspace_child;
static void dock_workspace_preview(dock_workspace_scene *s) {
    if (!s->state->dragging || !s->state->crossed || s->options.view.disable_preview)
        return;
    cui_dock_transfer_request request;
    if (!dock_workspace_target(s, &request))
        return;
    const cui_dock_panel *panel =
        dock_region_panel(&s->regions[request.destination_region], request.panel);
    bool permitted =
        panel && !panel->disabled &&
        !cui_dock_view_zoomed(s->regions[request.destination_region].view, NULL) &&
        !dock_group_constraints(s->options.view, request.destination_model, request.destination)
             .locked;
    dock_scene permission = {
        .dock = request.source_model, .options = s->options.view, .cross_destination = true};
    if (permitted)
        permitted = dock_allowed(
            &permission, request.axis == CUI_DOCK_TABS ? CUI_DOCK_MOVED : CUI_DOCK_SPLIT,
            request.panel, request.destination, SIZE_MAX, request.axis, request.before);
    if (permitted && s->options.allow_transfer)
        permitted = s->options.allow_transfer(s->options.transfer_user, &request);
    cui_rect bounds;
    if (request.opens_edge) {
        bounds = cui_bounds(s->ui, s->id);
        unsigned index = request.destination_region - 1;
        if (request.destination_region == CUI_DOCK_BOTTOM) {
            bounds.h = fminf(bounds.h, s->state->snapshot.size[index]);
            bounds.y += s->options.view.height - bounds.h;
        } else {
            bounds.w = fminf(bounds.w, s->state->snapshot.size[index]);
            if (request.destination_region == CUI_DOCK_RIGHT)
                bounds.x += s->options.view.width - bounds.w;
        }
    } else {
        bounds = cui_bounds(s->ui,
                            cui_dock_group_id(cui_dock_region_id(s->id, request.destination_region),
                                              request.destination));
        float tab = s->options.view.tab_height > 0 ? s->options.view.tab_height : 36;
        bounds.y += tab;
        bounds.h = fmaxf(0, bounds.h - tab);
        if (request.axis == CUI_DOCK_SPLIT_HORIZONTAL) {
            bounds.w *= .5f;
            if (!request.before)
                bounds.x += bounds.w;
        }
        if (request.axis == CUI_DOCK_SPLIT_VERTICAL) {
            bounds.h *= .5f;
            if (!request.before)
                bounds.y += bounds.h;
        }
    }
    cui_color ink = s->options.view.custom_palette
                        ? (permitted ? s->options.view.drag_palette.accepted
                                     : s->options.view.drag_palette.rejected)
                        : (permitted ? s->theme->primary : s->theme->danger);
    cui_color fill = ink;
    fill.a = (uint8_t)(ink.a / 5);
    cui_begin_portal(s->ui, s->id ^ UINT64_C(0x5e928ea123ec931b),
                     (cui_box){.background = fill, .border = ink, .border_width = 2, .inert = true},
                     bounds, 93);
    cui_end_box(s->ui);
}
static void dock_workspace_layer(dock_workspace_scene *, unsigned, float, float);
static void dock_workspace_region(dock_workspace_scene *s, cui_dock_region region, float width,
                                  float height) {
    if (width <= 0 || height <= 0)
        return;
    const cui_dock_region_content *content = &s->regions[region];
    cui_dock_view_options options = s->options.view;
    options.width = width;
    options.height = height;
    if (region == s->state->source_region && s->state->crossed && s->state->cross_region &&
        (s->state->dragging || s->pending_transfer))
        options.allow_reorder = false;
    if (region == CUI_DOCK_BOTTOM && s->options.collapsible_bottom && !s->state->snapshot.open[2]) {
        options.collapsed = true;
        options.tab_height = fminf(29, height);
    }
    cui_dock_view_result result =
        cui_dock_view_render(s->ui, cui_dock_region_id(s->id, region), s->theme, content->model,
                             content->view, content->panels, content->panel_count, options);
    float min_width = content->minimum.width > 0 ? content->minimum.width : region == 0 ? 64 : 48;
    float min_height = content->minimum.height > 0 ? content->minimum.height
                       : region == 0               ? 64
                                                   : 48;
    s->result.fit_minimum &= width >= min_width && (options.collapsed || height >= min_height);
    s->result.regions[region] = result;
    s->result.fit_minimum &= options.collapsed || result.fit_minimum;
    if (result.error != CUI_DOCK_OK)
        s->result.error = result.error;
}
static void dock_workspace_render_child(cui_context *ui, void *user) {
    (void)ui;
    dock_workspace_child *child = user;
    float span = child->scene->state->slots[child->level][child->slot].size;
    bool horizontal = child->level < 2;
    float width = horizontal ? span : child->cross, height = horizontal ? child->cross : span;
    if (child->edge)
        dock_workspace_region(child->scene, (cui_dock_region)(child->level + 1), width, height);
    else
        dock_workspace_layer(child->scene, child->level + 1, width, height);
}
static bool dock_node_visible(const cui_dock *model, cui_dock_node node,
                              cui_dock_view_options options) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(model, node, &info))
        return false;
    if (info.kind == CUI_DOCK_TABS)
        return cui_dock_visible_count(model, node, options.panel_visible,
                                      options.visibility_user) != 0;
    for (size_t i = 0; i < info.child_count; i++) {
        cui_dock_node child;
        if (!cui_dock_child_at(model, node, i, &child, NULL))
            continue;
        if (dock_node_visible(model, child, options))
            return true;
    }
    return false;
}
static bool dock_workspace_visible(dock_workspace_scene *s, unsigned region) {
    return dock_node_visible(s->regions[region].model, cui_dock_root(s->regions[region].model),
                             s->options.view);
}
static cui_size dock_model_minimum(const cui_dock *model, cui_dock_node node,
                                   cui_dock_view_options options) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(model, node, &info) || !dock_node_visible(model, node, options))
        return (cui_size){0};
    if (info.kind == CUI_DOCK_TABS)
        return (cui_size){(float)info.min_width, (float)info.min_height};
    cui_size min = {0};
    for (size_t i = 0; i < info.child_count; i++) {
        cui_dock_node child;
        if (!cui_dock_child_at(model, node, i, &child, NULL))
            continue;
        cui_size c = dock_model_minimum(model, child, options);
        min.width = info.kind == CUI_DOCK_SPLIT_HORIZONTAL ? min.width + c.width
                                                           : fmaxf(min.width, c.width);
        min.height = info.kind == CUI_DOCK_SPLIT_VERTICAL ? min.height + c.height
                                                          : fmaxf(min.height, c.height);
    }
    min.width = fmaxf(min.width, (float)info.min_width);
    min.height = fmaxf(min.height, (float)info.min_height);
    return min;
}
static float dock_region_minimum(dock_workspace_scene *s, unsigned region, bool horizontal) {
    float value = horizontal ? s->regions[region].minimum.width : s->regions[region].minimum.height;
    cui_size minimum = dock_model_minimum(s->regions[region].model,
                                          cui_dock_root(s->regions[region].model), s->options.view);
    return fmaxf(value > 0     ? value
                 : region == 0 ? 64
                               : 48,
                 horizontal ? minimum.width : minimum.height);
}
static void dock_workspace_layer(dock_workspace_scene *s, unsigned level, float width,
                                 float height) {
    if (level == 3) {
        dock_workspace_region(s, CUI_DOCK_CENTER, width, height);
        return;
    }
    if (!dock_workspace_visible(s, level + 1)) {
        cui_resizable_clear(&s->state->resize[level]);
        if (level == 2)
            s->state->bottom_gesture = false;
        dock_workspace_layer(s, level + 1, width, height);
        return;
    }
    bool collapse = level == 2 && s->options.collapsible_bottom &&
                    dock_region_ready(&s->regions[CUI_DOCK_BOTTOM]);
    if (level == 2 && !collapse && s->state->bottom_gesture) {
        s->state->bottom_gesture = false;
        cui_resizable_clear(&s->state->resize[2]);
    }
    if (!s->state->snapshot.open[level] && !collapse) {
        dock_workspace_layer(s, level + 1, width, height);
        return;
    }
    bool horizontal = level < 2;
    float extent = horizontal ? width : height, cross = horizontal ? height : width;
    unsigned edge_slot = level == 0 ? 0 : 1;
    float natural_min = dock_region_minimum(s, level + 1, horizontal);
    bool can_close = !s->state->snapshot.noncollapsible[level];
    float edge_min = collapse && (can_close || !s->state->snapshot.open[2]) ? fminf(29, natural_min)
                                                                            : natural_min;
    float remaining_min = dock_region_minimum(s, 0, horizontal);
    if (horizontal && s->state->snapshot.open[2])
        remaining_min = fmaxf(remaining_min, dock_region_minimum(s, 3, true));
    if (level == 0 && s->state->snapshot.open[1] && dock_workspace_visible(s, 2))
        remaining_min += dock_region_minimum(s, 2, true);
    bool fit = edge_min + remaining_min <= extent;
    s->result.fit_minimum &= fit && cross >= dock_region_minimum(s, level + 1, !horizontal);
    float preferred =
        collapse && !s->state->snapshot.open[2] ? edge_min : s->state->snapshot.size[level];
    float edge_size =
        fit ? fminf(extent - remaining_min, fmaxf(edge_min, preferred))
            : (edge_min + remaining_min > 0 ? extent * edge_min / (edge_min + remaining_min) : 0);
    cui_resizable_state *resize = &s->state->resize[level];
    cui_resizable_slot *slots = s->state->slots[level];
    if (resize->count == 2 && !resize->dragging) {
        slots[edge_slot].size = edge_size;
        slots[1 - edge_slot].size = extent - edge_size;
    }
    dock_workspace_child children[2] = {{s, level, 0, edge_slot == 0, cross},
                                        {s, level, 1, edge_slot == 1, cross}};
    cui_resizable_panel panels[2];
    for (unsigned i = 0; i < 2; ++i) {
        panels[i] = cui_resizable_panel_default(
            dock_part(s->id, level + 1,
                      i == 0 ? UINT64_C(0x61da9f37b4f591d8) : UINT64_C(0x187d5c928a64937b)));
        panels[i].has_initial_size = true;
        panels[i].initial_size = i == edge_slot ? edge_size : extent - edge_size;
        panels[i].custom_range = true;
        panels[i].minimum = fit ? (i == edge_slot ? edge_min : remaining_min) : 0;
        panels[i].maximum = fmaxf(extent, panels[i].minimum);
        panels[i].content = dock_workspace_render_child;
        panels[i].content_user = &children[i];
        panels[i].accessibility_label =
            cui_string(i == edge_slot ? "Edge dock" : "Center workspace");
    }
    cui_resizable_options options = cui_resizable_options_default();
    options.axis = horizontal ? CUI_RESIZABLE_HORIZONTAL : CUI_RESIZABLE_VERTICAL;
    options.extent = extent;
    options.cross_size = cross;
    options.disabled = s->options.view.disabled;
    options.reduced_motion = s->options.view.reduced_motion;
    options.now_seconds = s->options.view.now_seconds;
    options.key_step = s->options.view.key_step > 0 ? s->options.view.key_step : 8;
    options.box = (cui_box){.width = width,
                            .height = height,
                            .fixed_width = width == 0,
                            .fixed_height = height == 0,
                            .clip = true};
    bool canceled = false, completed = false, keyboard = false;
    if (collapse) {
        size_t count;
        const cui_event *events = cui_frame_events(s->ui, &count);
        cui_id handle = cui_dock_edge_resize_id(s->id, CUI_DOCK_BOTTOM);
        for (size_t i = 0; i < count; ++i) {
            cui_event e = events[i];
            if (!s->options.view.disabled && e.type == CUI_EVENT_POINTER_DOWN &&
                e.pointer_button == CUI_POINTER_PRIMARY && cui_event_target(s->ui, i) == handle) {
                s->state->bottom_initial = s->state->snapshot;
                s->state->bottom_gesture = true;
            }
            canceled |= s->state->bottom_gesture &&
                        (e.type == CUI_EVENT_CANCEL ||
                         (e.type == CUI_EVENT_KEY_DOWN && e.key == CUI_KEY_ESCAPE));
            completed |= s->state->bottom_gesture && e.type == CUI_EVENT_POINTER_UP &&
                         e.pointer_button == CUI_POINTER_PRIMARY;
            keyboard |= e.type == CUI_EVENT_KEY_DOWN && cui_event_target(s->ui, i) == handle &&
                        (e.key == CUI_KEY_UP || e.key == CUI_KEY_DOWN || e.key == CUI_KEY_HOME ||
                         e.key == CUI_KEY_END);
        }
        canceled |= s->state->bottom_gesture &&
                    (s->options.view.disabled || !cui_pointer_scope_allowed(s->ui, handle));
    }
    cui_resizable_error error =
        cui_resizable(s->ui, dock_part(s->id, level + 1, UINT64_C(0x40c96f5e8193287b)), s->theme,
                      resize, panels, 2, options);
    if (error != CUI_RESIZABLE_OK && error != CUI_RESIZABLE_INFEASIBLE)
        s->result.error = CUI_DOCK_INVALID;
    float changed_size = slots[edge_slot].size;
    if (collapse && canceled) {
        s->state->snapshot = s->state->bottom_initial;
        s->state->bottom_gesture = false;
        cui_resizable_clear(resize);
        cui_invalidate(s->ui);
        return;
    }
    if (collapse && (resize->dragging || completed || keyboard)) {
        bool open = !can_close || changed_size > edge_min;
        if (!can_close)
            changed_size = fmaxf(natural_min, changed_size);
        if (completed || keyboard) {
            open = !can_close || changed_size >= (edge_min + natural_min) / 2;
            changed_size = open ? fmaxf(natural_min, changed_size) : edge_min;
            s->state->bottom_gesture = false;
        }
        if (open != s->state->snapshot.open[2]) {
            s->state->schema_present[2] = true;
            s->state->snapshot.open[2] = open;
            s->result.opened |= open;
            cui_invalidate(s->ui);
        }
        if (changed_size >= natural_min)
            s->state->snapshot.size[2] = changed_size;
        if (completed || keyboard)
            cui_resizable_clear(resize);
    }
    if (fabsf(changed_size - edge_size) > .001f || (collapse && (completed || keyboard))) {
        if (!collapse)
            s->state->snapshot.size[level] = changed_size;
        s->result.resized = true;
        if (s->options.on_resize)
            s->options.on_resize(s->options.resize_user, (cui_dock_region)(level + 1),
                                 changed_size);
        cui_invalidate(s->ui);
    }
}
cui_dock_workspace_result cui_dock_workspace_render(cui_context *ui, cui_id id,
                                                    const cui_theme *theme,
                                                    cui_dock_workspace *state,
                                                    const cui_dock_region_content regions[4],
                                                    cui_dock_workspace_options options) {
    cui_dock_workspace_result invalid = {.error = CUI_DOCK_INVALID};
    if (!ui || !id || !theme || !state || !regions || !isfinite(options.view.width) ||
        options.view.width <= 0 || !isfinite(options.view.height) || options.view.height <= 0 ||
        !isfinite(options.view.tab_height) || options.view.tab_height < 0 ||
        !isfinite(options.view.toolbar_height) || options.view.toolbar_height < 0 ||
        !isfinite(options.view.key_step) || options.view.key_step < 0 ||
        !isfinite(options.view.now_seconds) || options.view.panel_style < CUI_DOCK_PANEL_TAB_BAR ||
        options.view.panel_style > CUI_DOCK_PANEL_AUTO)
        return invalid;
    for (unsigned i = 0; i < 4; ++i) {
        const cui_dock_region_content *region = &regions[i];
        if (i && !state->snapshot.open[i - 1] && !region->model && !region->view &&
            !region->panels && !region->panel_count)
            continue;
        if (!region->model || !region->view || (region->panel_count && !region->panels) ||
            !isfinite(region->minimum.width) || region->minimum.width < 0 ||
            !isfinite(region->minimum.height) || region->minimum.height < 0)
            return invalid;
        for (unsigned j = 0; j < i; ++j)
            if (regions[j].model &&
                (regions[j].model == region->model || regions[j].view == region->view))
                return invalid;
        if (region->panel_count > CUI_DOCK_PANEL_LIMIT)
            return invalid;
        for (size_t p = 0; p < region->panel_count; ++p) {
            if (!dock_title_valid(&region->panels[p]))
                return invalid;
            if (region->panels[p].zoom_control < CUI_DOCK_ZOOM_BOTH ||
                region->panels[p].zoom_control > CUI_DOCK_ZOOM_NONE)
                return invalid;
            if (region->panels[p].title.length && !region->panels[p].title.data)
                return invalid;
            cui_id menu_ids[CUI_DOCK_CUSTOM_MENU_LIMIT];
            size_t menu_used = 0, menu_total = 0;
            if (!dock_custom_menu_valid(cui_dock_region_id(id, (cui_dock_region)i),
                                        region->panels[p].key, region->panels[p].menu_items,
                                        region->panels[p].menu_count, 0, menu_ids, &menu_used,
                                        &menu_total))
                return invalid;
            for (size_t q = 0; q < p; ++q)
                if (region->panels[p].key == region->panels[q].key)
                    return invalid;
        }
        size_t nodes = 0, panels = 0;
        cui_dock_result error =
            dock_preflight(region->model, cui_dock_root(region->model), &nodes, &panels);
        if (error != CUI_DOCK_OK)
            return (cui_dock_workspace_result){.error = error};
        if (nodes > region->view->node_capacity || panels > region->view->panel_capacity)
            return (cui_dock_workspace_result){.error = CUI_DOCK_CAPACITY};
    }
    if (state->id != id) {
        for (unsigned i = 0; i < 3; ++i)
            cui_resizable_clear(&state->resize[i]);
        state->id = id;
        state->bottom_gesture = false;
        state->dragging = false;
        state->pending_focus = false;
    }
    dock_workspace_scene scene = {.ui = ui,
                                  .id = id,
                                  .theme = theme,
                                  .state = state,
                                  .regions = regions,
                                  .options = options,
                                  .result = {.error = CUI_DOCK_OK, .fit_minimum = true}};
    if (state->locked) {
        scene.host_constraints = options.view.group_constraints;
        scene.host_constraints_user = options.view.constraints_user;
        scene.options.view.group_constraints = dock_workspace_constraints;
        scene.options.view.constraints_user = &scene;
    }
    if (!cui_begin_focus_scope(ui, id, false))
        return invalid;
    if (options.collapsible_bottom && !options.view.disabled && !state->snapshot.open[2] &&
        dock_region_ready(&regions[CUI_DOCK_BOTTOM])) {
        size_t event_count;
        const cui_event *events = cui_frame_events(ui, &event_count);
        for (size_t e = 0; e < event_count; ++e) {
            bool activates = (events[e].type == CUI_EVENT_POINTER_UP &&
                              events[e].pointer_button == CUI_POINTER_PRIMARY) ||
                             (events[e].type == CUI_EVENT_KEY_DOWN &&
                              (events[e].key == CUI_KEY_ENTER || events[e].key == CUI_KEY_SPACE));
            bool permitted = events[e].type == CUI_EVENT_KEY_DOWN
                                 ? cui_pointer_scope_allowed(ui, id)
                                 : cui_event_hit_within_scope(ui, e, id);
            if (!activates || !permitted)
                continue;
            for (size_t p = 0; p < regions[3].panel_count; ++p) {
                const cui_dock_panel *panel = &regions[3].panels[p];
                cui_id tab = cui_dock_tab_id(cui_dock_region_id(id, CUI_DOCK_BOTTOM), panel->key);
                if (!panel->disabled &&
                    dock_panel_visible(options.view, regions[3].model, panel->key) &&
                    cui_event_target(ui, e) == tab &&
                    cui_dock_panel_find(regions[3].model, panel->key, NULL, NULL)) {
                    state->schema_present[2] = true;
                    state->snapshot.open[2] = true;
                    cui_resizable_clear(&state->resize[2]);
                    scene.result.opened = true;
                    cui_invalidate(ui);
                }
            }
        }
    }
    dock_workspace_events(&scene);
    cui_begin_box(ui, id,
                  (cui_box){.width = options.view.width,
                            .height = options.view.height,
                            .clip = true,
                            .background = theme->background});
    if (!cui_track_bounds(ui, id, &state->bounds))
        scene.result.error =
            cui_last_error(ui) == CUI_ERROR_CAPACITY ? CUI_DOCK_CAPACITY : CUI_DOCK_INVALID;
    cui_interact(ui, id, CUI_ROLE_GROUP, cui_string("Dock area"), options.view.disabled, false, 0);
    cui_set_semantic_details(ui, id,
                             (cui_semantic_details){.pointer_activatable = true, .no_focus = true});
    dock_workspace_layer(&scene, 0, options.view.width, options.view.height);
    dock_workspace_preview(&scene);
    cui_end_box(ui);
    if (state->pending_focus) {
        cui_id focus =
            cui_dock_tab_id(cui_dock_region_id(id, state->focus_region), state->focus_key);
        const cui_dock_panel *focused_panel =
            dock_region_panel(&regions[state->focus_region], state->focus_key);
        if (options.view.focus_content && focused_panel && focused_panel->focus_target)
            focus = focused_panel->focus_target;
        cui_focus(ui, focus);
        state->pending_focus = false;
    }
    if (scene.pending_transfer &&
        (scene.result.error != CUI_DOCK_OK || cui_last_error(ui) != CUI_OK)) {
        dock_workspace_restore_active(&scene);
        if (scene.result.error == CUI_DOCK_OK)
            scene.result.error = CUI_DOCK_INVALID;
        scene.pending_transfer = false;
    }
    if (scene.pending_transfer) {
        cui_dock_transfer_request *request = &scene.transfer;
        const cui_dock_region_content *target = &regions[request->destination_region];
        const cui_dock_panel *panel = dock_region_panel(target, request->panel);
        size_t nodes = 0, panels = 0;
        cui_dock_result error =
            dock_preflight(target->model, cui_dock_root(target->model), &nodes, &panels);
        if (!panel)
            error = CUI_DOCK_NOT_FOUND;
        else if (panel->disabled || cui_dock_view_zoomed(target->view, NULL) ||
                 !dock_panel_visible(options.view, target->model, request->panel))
            error = CUI_DOCK_INVALID;
        else if (nodes + (request->axis == CUI_DOCK_TABS ? 0 : 2) > target->view->node_capacity ||
                 panels + 1 > target->view->panel_capacity)
            error = CUI_DOCK_CAPACITY;
        dock_scene permission = {.dock = request->source_model,
                                 .options = scene.options.view,
                                 .cross_destination = true};
        if (error == CUI_DOCK_OK &&
            dock_group_constraints(scene.options.view, target->model, request->destination).locked)
            scene.result.denied = true;
        if (error == CUI_DOCK_OK &&
            !dock_allowed(
                &permission, request->axis == CUI_DOCK_TABS ? CUI_DOCK_MOVED : CUI_DOCK_SPLIT,
                request->panel, request->destination, SIZE_MAX, request->axis, request->before))
            scene.result.denied = true;
        if (!scene.result.denied && error == CUI_DOCK_OK && options.allow_transfer &&
            !options.allow_transfer(options.transfer_user, request))
            scene.result.denied = true;
        if (!scene.result.denied && error == CUI_DOCK_OK) {
            cui_dock_node destination;
            error = cui_dock_transfer(request->source_model, request->panel,
                                      request->destination_model, request->destination, SIZE_MAX,
                                      request->axis, .5, request->before, &destination);
            if (error == CUI_DOCK_OK) {
                scene.result.transferred = true;
                scene.result.opened = request->opens_edge;
                scene.result.transferred_panel = request->panel;
                scene.result.source_region = request->source_region;
                scene.result.destination_region = request->destination_region;
                request->destination = destination;
                if (request->opens_edge)
                    state->schema_present[request->destination_region - 1] = true;
                state->snapshot.open[request->destination_region - 1] = true;
                state->pending_focus = true;
                state->focus_region = request->destination_region;
                state->focus_key = request->panel;
                if (options.on_transfer)
                    options.on_transfer(options.transfer_user, request);
                cui_invalidate(ui);
            }
        }
        if (error != CUI_DOCK_OK)
            scene.result.error = error;
        if (error != CUI_DOCK_OK || scene.result.denied)
            dock_workspace_restore_active(&scene);
    }
    cui_end_focus_scope(ui);
    return scene.result;
}

static bool dock_state_string(cui_str value) {
    return !value.length || value.data != NULL;
}
static bool dock_state_equal(cui_str a, cui_str b) {
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}
void cui_dock_panels_release(cui_dock_panel_instance *instances, size_t count) {
    if (!instances)
        return;
    for (size_t i = 0; i < count; i++) {
        if (instances[i].factory && instances[i].factory->destroy)
            instances[i].factory->destroy(instances[i].factory->user, &instances[i].panel);
        instances[i] = (cui_dock_panel_instance){0};
    }
}
static cui_dock_result dock_panels_reconstruct(const cui_dock *model,
                                               const cui_dock_panel_state *states,
                                               const cui_dock_panel_typed_state *typed,
                                               size_t count,
                                               const cui_dock_panel_factory *factories, size_t nf,
                                               cui_dock_panel_instance *out, size_t capacity) {
    if (!model || (count && (!states || !out)) || (nf && !factories) ||
        count > CUI_DOCK_PANEL_LIMIT || nf > CUI_DOCK_PANEL_LIMIT)
        return CUI_DOCK_INVALID;
    if (capacity < count)
        return CUI_DOCK_CAPACITY;
    for (size_t i = 0; i < nf; i++) {
        if (!dock_state_string(factories[i].name) || !factories[i].name.length ||
            !factories[i].build || !factories[i].destroy)
            return CUI_DOCK_INVALID;
        for (size_t j = 0; j < i; j++)
            if (dock_state_equal(factories[i].name, factories[j].name))
                return CUI_DOCK_INVALID;
    }
    for (size_t i = 0; i < count; i++) {
        if (!dock_state_string(states[i].name) || !states[i].name.length ||
            !dock_state_string(states[i].payload))
            return CUI_DOCK_INVALID;
        if (!cui_dock_panel_find(model, states[i].key, NULL, NULL))
            return CUI_DOCK_NOT_FOUND;
        for (size_t j = 0; j < i; j++)
            if (states[i].key == states[j].key)
                return CUI_DOCK_DUPLICATE_PANEL;
    }
    size_t leaf_count = 0;
    cui_dock_result result = cui_dock_layout(model, (cui_rect){0}, 0, NULL, 0, &leaf_count, NULL);
    if (result != CUI_DOCK_OK)
        return result;
    cui_dock_geometry *leaves = calloc(leaf_count, sizeof(*leaves));
    if (!leaves && leaf_count)
        return CUI_DOCK_CAPACITY;
    result = cui_dock_layout(model, (cui_rect){0}, 0, leaves, leaf_count, &leaf_count, NULL);
    size_t actual = 0;
    if (result == CUI_DOCK_OK)
        for (size_t i = 0; i < leaf_count; i++)
            actual += leaves[i].panel_count;
    free(leaves);
    if (result != CUI_DOCK_OK)
        return result;
    if (actual != count)
        return CUI_DOCK_INVALID;
    if (!count)
        return CUI_DOCK_OK;
    cui_dock_panel_instance *staged = calloc(count, sizeof(*staged));
    if (!staged)
        return CUI_DOCK_CAPACITY;
    size_t built = 0;
    for (; built < count; built++) {
        staged[built].state = states[built];
        if (typed) {
            staged[built].source = typed[built].source;
            staged[built].has_source = true;
        }
        staged[built].panel = (cui_dock_panel){
            .key = states[built].key, .title = states[built].name, .closable = true};
        const cui_dock_panel_factory *factory = NULL;
        for (size_t j = 0; j < nf; j++)
            if (dock_state_equal(states[built].name, factories[j].name)) {
                factory = &factories[j];
                break;
            }
        if (!factory)
            continue;
        cui_dock_node tab;
        if (!cui_dock_panel_find(model, states[built].key, &tab, NULL)) {
            result = CUI_DOCK_NOT_FOUND;
            break;
        }
        cui_dock_panel_build_context context = {model, tab, &states[built],
                                                typed ? &typed[built].source : NULL};
        result = factory->build(factory->user, &context, &staged[built].panel);
        if (result != CUI_DOCK_OK)
            break; /* Failed builder must release its own partial resources. */
        staged[built].factory = factory;
        if (staged[built].panel.key != states[built].key) {
            result = CUI_DOCK_INVALID;
            built++;
            break;
        }
    }
    if (result == CUI_DOCK_OK)
        memcpy(out, staged, count * sizeof(*out));
    else
        cui_dock_panels_release(staged, built);
    free(staged);
    return result;
}
cui_dock_result cui_dock_panels_reconstruct(const cui_dock *model,
                                            const cui_dock_panel_state *states, size_t count,
                                            const cui_dock_panel_factory *factories, size_t nf,
                                            cui_dock_panel_instance *out, size_t capacity) {
    return dock_panels_reconstruct(model, states, NULL, count, factories, nf, out, capacity);
}
cui_dock_result
cui_dock_panels_reconstruct_typed(const cui_dock *model, const cui_dock_panel_typed_state *typed,
                                  size_t count, const cui_dock_panel_factory *factories, size_t nf,
                                  cui_dock_panel_instance *out, size_t capacity) {
    if (count && !typed)
        return CUI_DOCK_INVALID;
    if (count > CUI_DOCK_PANEL_LIMIT)
        return CUI_DOCK_INVALID;
    cui_dock_panel_state *states = count ? calloc(count, sizeof(*states)) : NULL;
    if (count && !states)
        return CUI_DOCK_CAPACITY;
    for (size_t i = 0; i < count; i++) {
        const cui_dock_schema_node *source = &typed[i].source;
        if (source->kind != CUI_DOCK_SCHEMA_PANEL || (source->child_count && !source->children) ||
            source->size_count || source->sizes) {
            free(states);
            return CUI_DOCK_INVALID;
        }
        states[i] = (cui_dock_panel_state){typed[i].key, source->panel_name, source->payload};
    }
    cui_dock_result result =
        dock_panels_reconstruct(model, states, typed, count, factories, nf, out, capacity);
    free(states);
    return result;
}
cui_dock_result cui_dock_panel_dump(const cui_dock_panel_instance *instance,
                                    cui_dock_panel_state *out) {
    if (!instance || !out)
        return CUI_DOCK_INVALID;
    cui_dock_panel_state state = instance->state;
    if (instance->factory && instance->factory->dump) {
        cui_dock_result result =
            instance->factory->dump(instance->factory->user, &instance->panel, &state.payload);
        if (result != CUI_DOCK_OK)
            return result;
        if (!dock_state_string(state.payload))
            return CUI_DOCK_INVALID;
    }
    *out = state;
    return CUI_DOCK_OK;
}
cui_dock_result cui_dock_panel_dump_schema(const cui_dock_panel_instance *instance,
                                           cui_dock_schema_node *out) {
    if (!instance || !out || !instance->has_source)
        return CUI_DOCK_INVALID;
    cui_dock_schema_node source = instance->source;
    if (instance->factory && instance->factory->dump_schema) {
        cui_dock_result result =
            instance->factory->dump_schema(instance->factory->user, &instance->panel, &source);
        if (result != CUI_DOCK_OK)
            return result;
    } else if (instance->factory && instance->factory->dump) {
        cui_dock_result result =
            instance->factory->dump(instance->factory->user, &instance->panel, &source.payload);
        if (result != CUI_DOCK_OK)
            return result;
    }
    if (source.kind != CUI_DOCK_SCHEMA_PANEL || !dock_state_string(source.panel_name) ||
        !source.panel_name.length || !dock_state_string(source.payload) ||
        (source.child_count && !source.children) || source.size_count || source.sizes)
        return CUI_DOCK_INVALID;
    *out = source;
    return CUI_DOCK_OK;
}

bool cui_dock_lifecycle_init(cui_dock_lifecycle *state, cui_dock_lifecycle_snapshot *current,
                             cui_dock_lifecycle_snapshot *pending, size_t capacity) {
    if (!state || !current || !pending || current == pending || !capacity ||
        capacity > CUI_DOCK_PANEL_LIMIT)
        return false;
    *state = (cui_dock_lifecycle){.current = current, .pending = pending, .capacity = capacity};
    return true;
}
static size_t dock_lifecycle_count(const cui_dock *model, cui_dock_node node) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(model, node, &info))
        return 0;
    if (info.kind == CUI_DOCK_TABS)
        return info.panel_count;
    size_t count = 0;
    for (size_t i = 0; i < info.child_count; i++) {
        cui_dock_node child;
        if (!cui_dock_child_at(model, node, i, &child, NULL))
            continue;
        count += dock_lifecycle_count(model, child);
    }
    return count;
}
static bool dock_lifecycle_unique(const cui_dock_lifecycle_region *regions, size_t region,
                                  cui_dock_node node) {
    const cui_dock *model = regions[region].model;
    cui_dock_node_info info;
    if (!cui_dock_node_get(model, node, &info))
        return false;
    if (info.kind != CUI_DOCK_TABS) {
        for (size_t i = 0; i < info.child_count; i++) {
            cui_dock_node child;
            if (!cui_dock_child_at(model, node, i, &child, NULL))
                return false;
            if (!dock_lifecycle_unique(regions, region, child))
                return false;
        }
        return true;
    }
    for (size_t i = 0; i < info.panel_count; i++) {
        uint64_t key;
        if (!cui_dock_panel_at(model, node, i, &key))
            return false;
        for (size_t j = 0; j < region; j++)
            if (cui_dock_panel_find(regions[j].model, key, NULL, NULL))
                return false;
    }
    return true;
}
static bool dock_lifecycle_fill(cui_dock_lifecycle *state, const cui_dock_lifecycle_region *region,
                                cui_dock_node node, cui_dock_node zoom_group,
                                const cui_dock_panel *panels, size_t panel_count, size_t *index) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(region->model, node, &info))
        return false;
    if (info.kind != CUI_DOCK_TABS) {
        for (size_t i = 0; i < info.child_count; i++) {
            cui_dock_node child;
            if (!cui_dock_child_at(region->model, node, i, &child, NULL) ||
                !dock_lifecycle_fill(state, region, child, zoom_group, panels, panel_count, index))
                return false;
        }
        return true;
    }
    bool collapsed = region->collapsed ||
                     (region->group_collapsed &&
                      region->group_collapsed(region->constraints_user, region->model, node));
    uint64_t displayed = 0;
    bool has_displayed = cui_dock_displayed_panel(region->model, node, region->panel_visible,
                                                  region->visibility_user, &displayed);
    cui_dock_node_info current;
    if (!cui_dock_node_get(region->model, node, &current) || current.kind != info.kind ||
        current.panel_count != info.panel_count || current.has_active != info.has_active ||
        current.active_panel != info.active_panel)
        return false;
    for (size_t i = 0; i < info.panel_count; i++) {
        uint64_t key;
        if (!cui_dock_panel_at(region->model, node, i, &key) || *index >= state->capacity)
            return false;
        cui_dock_panel panel = {.key = key};
        for (size_t j = 0; j < panel_count; j++)
            if (panels[j].key == key) {
                panel = panels[j];
                break;
            }
        state->pending[(*index)++] = (cui_dock_lifecycle_snapshot){
            .panel = panel,
            .model = region->model,
            .tab = node,
            .active = !collapsed && info.has_active && info.active_panel == key,
            .displayed = has_displayed && displayed == key,
            .group_zoomed = zoom_group == node};
    }
    return true;
}
cui_dock_result cui_dock_lifecycle_capture(cui_dock_lifecycle *state,
                                           const cui_dock_lifecycle_region *regions, size_t nr,
                                           const cui_dock_panel *panels, size_t np) {
    if (!state || !state->current || !state->pending || state->dispatching || (nr && !regions) ||
        (np && !panels) || nr > CUI_DOCK_NODE_LIMIT || np > CUI_DOCK_PANEL_LIMIT)
        return CUI_DOCK_INVALID;
    for (size_t i = 0; i < np; i++)
        for (size_t j = 0; j < i; j++)
            if (panels[i].key == panels[j].key)
                return CUI_DOCK_DUPLICATE_PANEL;
    size_t total = 0;
    for (size_t i = 0; i < nr; i++) {
        if (!regions[i].model)
            return CUI_DOCK_INVALID;
        for (size_t j = 0; j < i; j++)
            if (regions[i].model == regions[j].model)
                return CUI_DOCK_INVALID;
        size_t count = dock_lifecycle_count(regions[i].model, cui_dock_root(regions[i].model));
        if (count > state->capacity - total)
            return CUI_DOCK_CAPACITY;
        total += count;
        if (!dock_lifecycle_unique(regions, i, cui_dock_root(regions[i].model)))
            return CUI_DOCK_DUPLICATE_PANEL;
    }
    cui_dock_lifecycle_snapshot *pending = total ? malloc(total * sizeof *pending) : NULL;
    if (total && !pending)
        return CUI_DOCK_CAPACITY;
    cui_dock_lifecycle staged = *state;
    staged.pending = pending;
    staged.capacity = total;
    size_t index = 0;
    state->dispatching = true;
    for (size_t i = 0; i < nr; i++) {
        uint64_t zoom_key;
        cui_dock_node zoom_group = 0;
        if (regions[i].view && cui_dock_view_zoomed(regions[i].view, &zoom_key))
            cui_dock_panel_find(regions[i].model, zoom_key, &zoom_group, NULL);
        if (!dock_lifecycle_fill(&staged, &regions[i], cui_dock_root(regions[i].model), zoom_group,
                                 panels, np, &index)) {
            state->dispatching = false;
            free(pending);
            return CUI_DOCK_INVALID;
        }
    }
    state->dispatching = false;
    if (index)
        memcpy(state->pending, pending, index * sizeof *pending);
    free(pending);
    state->pending_count = index;
    state->has_pending = true;
    return CUI_DOCK_OK;
}
static const cui_dock_lifecycle_snapshot *
dock_lifecycle_find(const cui_dock_lifecycle_snapshot *items, size_t count, uint64_t key) {
    for (size_t i = 0; i < count; i++)
        if (items[i].panel.key == key)
            return &items[i];
    return NULL;
}
bool cui_dock_lifecycle_dispatch(cui_dock_lifecycle *state) {
    if (!state || !state->current || !state->pending || state->dispatching)
        return false;
    if (!state->has_pending)
        return true;
    cui_dock_lifecycle_snapshot *old = state->current;
    size_t old_count = state->current_count;
    state->current = state->pending;
    state->current_count = state->pending_count;
    state->pending = old;
    state->pending_count = 0;
    state->has_pending = false;
    state->dispatching = true;
    for (size_t i = 0; i < state->current_count; i++) {
        const cui_dock_lifecycle_snapshot *now = &state->current[i];
        const cui_dock_lifecycle_snapshot *was =
            dock_lifecycle_find(old, old_count, now->panel.key);
        if (was && was->panel.lifecycle_identity != now->panel.lifecycle_identity)
            was = NULL;
        if ((!was || was->model != now->model || was->tab != now->tab) && now->panel.on_added)
            now->panel.on_added(now->panel.user, now->panel.key, now->model, now->tab);
    }
    for (size_t i = 0; i < old_count; i++) {
        const cui_dock_lifecycle_snapshot *now =
            dock_lifecycle_find(state->current, state->current_count, old[i].panel.key);
        if ((!now || now->panel.lifecycle_identity != old[i].panel.lifecycle_identity) &&
            old[i].panel.on_removed)
            old[i].panel.on_removed(old[i].panel.user, old[i].panel.key);
    }
    /* All deactivations precede activations; removals never receive false. */
    for (unsigned phase = 0; phase < 2; phase++)
        for (size_t i = 0; i < state->current_count; i++) {
            const cui_dock_lifecycle_snapshot *now = &state->current[i];
            if (now->active != (phase != 0))
                continue;
            const cui_dock_lifecycle_snapshot *was =
                dock_lifecycle_find(old, old_count, now->panel.key);
            if (was && was->panel.lifecycle_identity != now->panel.lifecycle_identity)
                was = NULL;
            if ((!was ? now->active : was->active != now->active) && now->panel.set_active)
                now->panel.set_active(now->panel.user, now->panel.key, now->active);
        }
    /* Group zoom edges notify only the current visible displayed member. */
    for (size_t i = 0; i < state->current_count; i++) {
        const cui_dock_lifecycle_snapshot *now = &state->current[i];
        if (!now->displayed || !now->panel.set_zoomed)
            continue;
        bool old_zoom = false;
        for (size_t j = 0; j < old_count; j++)
            if (old[j].model == now->model && old[j].tab == now->tab) {
                old_zoom = old[j].group_zoomed;
                break;
            }
        if (old_zoom != now->group_zoomed)
            now->panel.set_zoomed(now->panel.user, now->panel.key, now->group_zoomed);
    }
    state->dispatching = false;
    return true;
}

typedef struct {
    uint64_t key;
    cui_dock_region region;
    cui_dock_node tab;
    size_t descriptor;
    bool active;
} dock_area_location;
typedef struct {
    size_t descriptor;
    bool zoomed;
} dock_area_zoom_event;
struct cui_dock_area {
    cui_dock *models[4];
    cui_dock_view *views[4];
    cui_dock_workspace *workspace;
    cui_dock_area_panel *descriptors;
    cui_dock_panel *panels;
    dock_area_location *current, *scratch;
    dock_area_zoom_event *zoom_events;
    bool *notified, *notified_active;
    size_t capacity, descriptor_count, current_count, zoom_capacity, zoom_count;
    bool delivering;
};
static size_t dock_area_descriptor(const cui_dock_area *area, uint64_t key) {
    for (size_t i = 0; i < area->descriptor_count; i++)
        if (area->panels[i].key == key)
            return i;
    return SIZE_MAX;
}
static const dock_area_location *dock_area_find(const dock_area_location *items, size_t count,
                                                uint64_t key) {
    for (size_t i = 0; i < count; i++)
        if (items[i].key == key)
            return &items[i];
    return NULL;
}
static cui_dock_result dock_area_fill(cui_dock_area *area, size_t region, cui_dock_node node,
                                      dock_area_location *out, size_t *count, bool open) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(area->models[region], node, &info))
        return CUI_DOCK_INVALID;
    if (info.kind != CUI_DOCK_TABS) {
        for (size_t i = 0; i < info.child_count; i++) {
            cui_dock_node child;
            if (!cui_dock_child_at(area->models[region], node, i, &child, NULL))
                return CUI_DOCK_INVALID;
            cui_dock_result result = dock_area_fill(area, region, child, out, count, open);
            if (result != CUI_DOCK_OK)
                return result;
        }
        return CUI_DOCK_OK;
    }
    for (size_t i = 0; i < info.panel_count; i++) {
        uint64_t key;
        if (!cui_dock_panel_at(area->models[region], node, i, &key))
            return CUI_DOCK_INVALID;
        if (*count == area->capacity)
            return CUI_DOCK_CAPACITY;
        if (dock_area_find(out, *count, key))
            return CUI_DOCK_DUPLICATE_PANEL;
        size_t descriptor = dock_area_descriptor(area, key);
        if (descriptor == SIZE_MAX)
            return CUI_DOCK_NOT_FOUND;
        out[(*count)++] = (dock_area_location){key, (cui_dock_region)region, node, descriptor,
                                               open && info.has_active && info.active_panel == key};
    }
    return CUI_DOCK_OK;
}
cui_dock_area *cui_dock_area_create(cui_dock_config config, size_t panel_capacity,
                                    size_t zoom_event_capacity) {
    if (!panel_capacity)
        panel_capacity = 256;
    if (!zoom_event_capacity)
        zoom_event_capacity = panel_capacity;
    if (panel_capacity > CUI_DOCK_PANEL_LIMIT || zoom_event_capacity > CUI_DOCK_PANEL_LIMIT)
        return NULL;
    cui_dock_area *area = calloc(1, sizeof(*area));
    if (!area)
        return NULL;
    area->capacity = panel_capacity;
    area->zoom_capacity = zoom_event_capacity;
    area->descriptors = calloc(panel_capacity, sizeof(*area->descriptors));
    area->panels = calloc(panel_capacity, sizeof(*area->panels));
    area->current = calloc(panel_capacity, sizeof(*area->current));
    area->scratch = calloc(panel_capacity, sizeof(*area->scratch));
    area->zoom_events = calloc(zoom_event_capacity, sizeof(*area->zoom_events));
    area->notified = calloc(panel_capacity, sizeof(*area->notified));
    area->notified_active = calloc(panel_capacity, sizeof(*area->notified_active));
    area->workspace = cui_dock_workspace_create();
    for (size_t i = 0; i < 4; i++) {
        area->models[i] =
            cui_dock_create_with_root(config, i ? CUI_DOCK_ROOT_ANY : CUI_DOCK_ROOT_SPLIT);
        area->views[i] = cui_dock_view_create(config.node_capacity, panel_capacity);
    }
    if (!area->descriptors || !area->panels || !area->current || !area->scratch ||
        !area->zoom_events || !area->notified || !area->notified_active || !area->workspace ||
        !area->models[0] || !area->models[1] || !area->models[2] || !area->models[3] ||
        !area->views[0] || !area->views[1] || !area->views[2] || !area->views[3]) {
        cui_dock_area_destroy(area);
        return NULL;
    }
    return area;
}
void cui_dock_area_destroy(cui_dock_area *area) {
    if (!area)
        return;
    for (size_t i = 0; i < area->descriptor_count; i++)
        if (area->descriptors[i].release)
            area->descriptors[i].release(area->panels[i].user);
    for (size_t i = 0; i < 4; i++) {
        cui_dock_view_destroy(area->views[i]);
        cui_dock_destroy(area->models[i]);
    }
    cui_dock_workspace_destroy(area->workspace);
    free(area->descriptors);
    free(area->panels);
    free(area->current);
    free(area->scratch);
    free(area->zoom_events);
    free(area->notified);
    free(area->notified_active);
    free(area);
}
cui_dock *cui_dock_area_model(cui_dock_area *area, cui_dock_region region) {
    return area && region >= CUI_DOCK_CENTER && region <= CUI_DOCK_BOTTOM ? area->models[region]
                                                                          : NULL;
}
cui_dock_view *cui_dock_area_view(cui_dock_area *area, cui_dock_region region) {
    return area && region >= CUI_DOCK_CENTER && region <= CUI_DOCK_BOTTOM ? area->views[region]
                                                                          : NULL;
}
cui_dock_workspace *cui_dock_area_workspace(cui_dock_area *area) {
    return area ? area->workspace : NULL;
}
bool cui_dock_area_regions(cui_dock_area *area, cui_dock_region_content out[4]) {
    if (!area || !out)
        return false;
    for (size_t i = 0; i < 4; i++)
        out[i] = (cui_dock_region_content){.model = area->models[i],
                                           .view = area->views[i],
                                           .panels = area->panels,
                                           .panel_count = area->descriptor_count};
    return true;
}
bool cui_dock_area_has_dock(const cui_dock_area *area, cui_dock_region region) {
    return area && region >= CUI_DOCK_LEFT && region <= CUI_DOCK_BOTTOM &&
           area->workspace->schema_present[region - 1];
}
bool cui_dock_area_dock_open(const cui_dock_area *area, cui_dock_region region) {
    return cui_dock_area_has_dock(area, region) && area->workspace->snapshot.open[region - 1];
}
bool cui_dock_area_dock_collapsible(const cui_dock_area *area, cui_dock_region region) {
    return cui_dock_area_has_dock(area, region) &&
           !area->workspace->snapshot.noncollapsible[region - 1];
}
bool cui_dock_area_dock_size(const cui_dock_area *area, cui_dock_region region, float *size) {
    if (!size || !cui_dock_area_has_dock(area, region))
        return false;
    *size = area->workspace->snapshot.size[region - 1];
    return true;
}
cui_dock_result cui_dock_area_toggle_dock(cui_dock_area *area, cui_dock_region region) {
    if (!area || area->delivering || region < CUI_DOCK_LEFT || region > CUI_DOCK_BOTTOM)
        return CUI_DOCK_INVALID;
    if (!cui_dock_area_has_dock(area, region))
        return CUI_DOCK_NOT_FOUND;
    size_t edge = region - 1;
    bool open = area->workspace->snapshot.open[edge];
    if (open && area->workspace->snapshot.noncollapsible[edge])
        return CUI_DOCK_OK;
    if (!cui_dock_workspace_set_edge(area->workspace, region, !open,
                                     area->workspace->snapshot.size[edge]))
        return CUI_DOCK_INVALID;
    return cui_dock_area_reconcile(area);
}
cui_dock_result cui_dock_area_set_dock_collapsible(cui_dock_area *area, cui_dock_region region,
                                                   bool collapsible) {
    if (!area || area->delivering || region < CUI_DOCK_LEFT || region > CUI_DOCK_BOTTOM)
        return CUI_DOCK_INVALID;
    if (!cui_dock_area_has_dock(area, region))
        return CUI_DOCK_NOT_FOUND;
    return cui_dock_workspace_set_collapsible(area->workspace, region, collapsible)
               ? CUI_DOCK_OK
               : CUI_DOCK_INVALID;
}
cui_dock_result cui_dock_area_set_dock_size(cui_dock_area *area, cui_dock_region region,
                                            float size) {
    if (!area || area->delivering || region < CUI_DOCK_LEFT || region > CUI_DOCK_BOTTOM ||
        !isfinite(size) || size <= 0 || size > 1e9f)
        return CUI_DOCK_INVALID;
    if (!cui_dock_area_has_dock(area, region))
        return CUI_DOCK_NOT_FOUND;
    return cui_dock_workspace_set_edge(area->workspace, region,
                                       area->workspace->snapshot.open[region - 1], size)
               ? CUI_DOCK_OK
               : CUI_DOCK_INVALID;
}
cui_dock_result cui_dock_area_remove_dock(cui_dock_area *area, cui_dock_region region) {
    if (!area || area->delivering || region < CUI_DOCK_LEFT || region > CUI_DOCK_BOTTOM)
        return CUI_DOCK_INVALID;
    if (!cui_dock_area_has_dock(area, region))
        return CUI_DOCK_OK;
    uint64_t zoom_key = 0;
    bool zoomed = cui_dock_view_zoomed(area->views[region], &zoom_key);
    size_t zoom_descriptor = zoomed ? dock_area_descriptor(area, zoom_key) : SIZE_MAX;
    if (zoom_descriptor != SIZE_MAX && area->zoom_count == area->zoom_capacity)
        return CUI_DOCK_CAPACITY;
    cui_dock_schema_node empty = {.kind = CUI_DOCK_SCHEMA_TABS};
    cui_dock_result result = cui_dock_schema_import(area->models[region], &empty, NULL, NULL);
    if (result != CUI_DOCK_OK)
        return result;
    if (zoom_descriptor != SIZE_MAX)
        area->zoom_events[area->zoom_count++] = (dock_area_zoom_event){zoom_descriptor, false};
    cui_dock_view_reset(area->views[region]);
    area->workspace->schema_present[region - 1] = false;
    area->workspace->snapshot.open[region - 1] = false;
    return cui_dock_area_reconcile(area);
}
cui_dock_result cui_dock_area_register(cui_dock_area *area, cui_dock_area_panel descriptor) {
    if (!area || area->delivering || (descriptor.retain == NULL) != (descriptor.release == NULL) ||
        dock_area_descriptor(area, descriptor.panel.key) != SIZE_MAX)
        return CUI_DOCK_INVALID;
    if (area->descriptor_count == area->capacity)
        return CUI_DOCK_CAPACITY;
    size_t slot = area->descriptor_count++;
    area->descriptors[slot] = descriptor;
    area->panels[slot] = descriptor.panel;
    if (descriptor.retain)
        descriptor.retain(descriptor.panel.user);
    return CUI_DOCK_OK;
}
cui_dock_result cui_dock_area_reconcile(cui_dock_area *area) {
    if (!area || area->delivering)
        return CUI_DOCK_INVALID;
    cui_dock_workspace_snapshot workspace;
    if (!cui_dock_workspace_save(area->workspace, &workspace))
        return CUI_DOCK_INVALID;
    size_t count = 0;
    for (size_t i = 0; i < 4; i++) {
        cui_dock_result result =
            dock_area_fill(area, i, cui_dock_root(area->models[i]), area->scratch, &count,
                           i == 0 || workspace.open[i - 1]);
        if (result != CUI_DOCK_OK)
            return result;
    }
    dock_area_location *old = area->current;
    size_t old_count = area->current_count;
    area->current = area->scratch;
    area->scratch = old;
    area->current_count = count;
    area->delivering = true;
    for (size_t i = 0; i < count; i++) {
        dock_area_location now = area->current[i];
        const dock_area_location *was = dock_area_find(old, old_count, now.key);
        cui_dock_panel *panel = &area->panels[now.descriptor];
        if ((!was || was->region != now.region || was->tab != now.tab) && panel->on_added)
            panel->on_added(panel->user, now.key, area->models[now.region], now.tab);
    }
    for (size_t i = 0; i < old_count; i++) {
        dock_area_location was = old[i];
        if (dock_area_find(area->current, count, was.key))
            continue;
        cui_dock_panel *panel = &area->panels[was.descriptor];
        if (panel->on_removed)
            panel->on_removed(panel->user, was.key);
        area->notified[was.descriptor] = false;
    }
    area->delivering = false;
    return CUI_DOCK_OK;
}
static bool dock_area_region_of(const cui_dock_area *area, uint64_t key, size_t *region) {
    for (size_t i = 0; i < 4; i++)
        if (cui_dock_panel_find(area->models[i], key, NULL, NULL)) {
            *region = i;
            return true;
        }
    return false;
}
cui_dock_result cui_dock_area_insert(cui_dock_area *area, cui_dock_region region, cui_dock_node tab,
                                     uint64_t key, size_t index) {
    if (!area || area->delivering || region < CUI_DOCK_CENTER || region > CUI_DOCK_BOTTOM ||
        dock_area_descriptor(area, key) == SIZE_MAX)
        return CUI_DOCK_INVALID;
    size_t existing;
    if (dock_area_region_of(area, key, &existing))
        return CUI_DOCK_DUPLICATE_PANEL;
    if (area->current_count == area->capacity)
        return CUI_DOCK_CAPACITY;
    cui_dock_result result = cui_dock_insert(area->models[region], tab, key, index);
    return result == CUI_DOCK_OK ? cui_dock_area_reconcile(area) : result;
}
cui_dock_result cui_dock_area_remove(cui_dock_area *area, uint64_t key) {
    if (!area || area->delivering)
        return CUI_DOCK_INVALID;
    size_t region;
    if (!dock_area_region_of(area, key, &region))
        return CUI_DOCK_NOT_FOUND;
    cui_dock_result result = cui_dock_remove(area->models[region], key);
    return result == CUI_DOCK_OK ? cui_dock_area_reconcile(area) : result;
}
cui_dock_result cui_dock_area_move(cui_dock_area *area, uint64_t key, cui_dock_region destination,
                                   cui_dock_node target, size_t index, cui_dock_kind axis,
                                   double ratio, bool before, cui_dock_node *new_tab) {
    if (!area || area->delivering || destination < CUI_DOCK_CENTER || destination > CUI_DOCK_BOTTOM)
        return CUI_DOCK_INVALID;
    size_t source;
    if (!dock_area_region_of(area, key, &source))
        return CUI_DOCK_NOT_FOUND;
    cui_dock_result result;
    if (source != (size_t)destination)
        result = cui_dock_transfer(area->models[source], key, area->models[destination], target,
                                   index, axis, ratio, before, new_tab);
    else if (axis == CUI_DOCK_TABS)
        result = cui_dock_move(area->models[source], key, target, index);
    else
        result =
            cui_dock_move_split(area->models[source], key, target, axis, ratio, before, new_tab);
    return result == CUI_DOCK_OK ? cui_dock_area_reconcile(area) : result;
}
cui_dock_result cui_dock_area_activate(cui_dock_area *area, uint64_t key) {
    if (!area || area->delivering)
        return CUI_DOCK_INVALID;
    size_t region;
    if (!dock_area_region_of(area, key, &region))
        return CUI_DOCK_NOT_FOUND;
    cui_dock_result result = cui_dock_activate(area->models[region], key);
    return result == CUI_DOCK_OK ? cui_dock_area_reconcile(area) : result;
}
cui_dock_result cui_dock_area_set_zoom(cui_dock_area *area, uint64_t key, bool zoomed) {
    if (!area || area->delivering)
        return CUI_DOCK_INVALID;
    size_t region;
    if (!dock_area_region_of(area, key, &region))
        return CUI_DOCK_NOT_FOUND;
    const dock_area_location *location = dock_area_find(area->current, area->current_count, key);
    if (!location ||
        (zoomed && (!location->active || !area->panels[location->descriptor].zoomable)))
        return CUI_DOCK_INVALID;
    size_t previous_region = SIZE_MAX;
    uint64_t previous_key = 0;
    for (size_t i = 0; i < 4; i++)
        if (cui_dock_view_zoomed(area->views[i], &previous_key)) {
            previous_region = i;
            break;
        }
    if ((!zoomed && previous_region == SIZE_MAX) ||
        (zoomed && previous_region == region && previous_key == key))
        return CUI_DOCK_OK;
    size_t needed = (previous_region != SIZE_MAX) + (zoomed ? 1u : 0u);
    if (needed > area->zoom_capacity - area->zoom_count)
        return CUI_DOCK_CAPACITY;
    if (previous_region != SIZE_MAX) {
        size_t previous_descriptor = dock_area_descriptor(area, previous_key);
        cui_dock_view_set_zoom(area->views[previous_region], area->models[previous_region],
                               previous_key, false);
        if (previous_descriptor != SIZE_MAX)
            area->zoom_events[area->zoom_count++] =
                (dock_area_zoom_event){previous_descriptor, false};
    }
    if (zoomed) {
        if (!cui_dock_view_set_zoom(area->views[region], area->models[region], key, true))
            return CUI_DOCK_INVALID;
        area->zoom_events[area->zoom_count++] = (dock_area_zoom_event){location->descriptor, true};
    }
    return cui_dock_area_reconcile(area);
}
typedef struct {
    cui_dock_area *area;
    cui_dock_schema_key_fn key;
    void *user;
} dock_area_import_key;
static bool dock_area_schema_key(void *user, const cui_dock_schema_node *source, uint64_t *key) {
    dock_area_import_key *context = user;
    return context->key && context->key(context->user, source, key) &&
           dock_area_descriptor(context->area, *key) != SIZE_MAX;
}
cui_dock_result cui_dock_area_schema_import(cui_dock_area *area, const cui_dock_schema_area *schema,
                                            cui_dock_schema_key_fn key, void *user) {
    if (!area || area->delivering || !schema || !key)
        return CUI_DOCK_INVALID;
    cui_dock_region_content regions[4];
    cui_dock_area_regions(area, regions);
    dock_area_import_key context = {area, key, user};
    cui_dock_result result = cui_dock_workspace_schema_import(area->workspace, regions, schema,
                                                              dock_area_schema_key, &context);
    return result == CUI_DOCK_OK ? cui_dock_area_reconcile(area) : result;
}
bool cui_dock_area_tick(cui_dock_area *area) {
    if (!area || area->delivering)
        return false;
    area->delivering = true;
    for (unsigned phase = 0; phase < 2; phase++)
        for (size_t i = 0; i < area->current_count; i++) {
            dock_area_location location = area->current[i];
            size_t descriptor = location.descriptor;
            if (location.active != (phase != 0))
                continue;
            if (area->notified[descriptor] && area->notified_active[descriptor] == location.active)
                continue;
            bool was_active = area->notified[descriptor] && area->notified_active[descriptor];
            area->notified[descriptor] = true;
            area->notified_active[descriptor] = location.active;
            cui_dock_panel *panel = &area->panels[descriptor];
            if ((location.active || was_active) && panel->set_active)
                panel->set_active(panel->user, location.key, location.active);
        }
    for (size_t i = 0; i < area->zoom_count; i++) {
        dock_area_zoom_event event = area->zoom_events[i];
        cui_dock_panel *panel = &area->panels[event.descriptor];
        if (panel->set_zoomed)
            panel->set_zoomed(panel->user, panel->key, event.zoomed);
    }
    area->zoom_count = 0;
    area->delivering = false;
    return true;
}
