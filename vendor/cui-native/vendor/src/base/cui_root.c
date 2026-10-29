/* SPDX-License-Identifier: MIT */
#include "cui/cui_root.h"
#include <math.h>
#include <string.h>
bool cui_root_registry_init(cui_root_registry *registry, cui_root_plugin *entries,
                            size_t capacity) {
    if (!registry || (capacity && !entries))
        return false;
    *registry = (cui_root_registry){.entries = entries, .capacity = capacity};
    return true;
}
bool cui_root_register(cui_root_registry *registry, cui_root_plugin plugin) {
    if (!registry || registry->count > registry->capacity ||
        (registry->capacity && !registry->entries) || !plugin.key || !plugin.build ||
        (!!plugin.decorate_begin != !!plugin.decorate_end))
        return false;
    for (size_t i = 0; i < registry->count; i++)
        if (registry->entries[i].key == plugin.key) {
            registry->entries[i] = plugin;
            return true;
        }
    if (registry->count == registry->capacity)
        return false;
    registry->entries[registry->count++] = plugin;
    return true;
}
bool cui_root_init(cui_root *root, cui_root_plugin_instance *instances, size_t capacity,
                   const cui_root_registry *registry, void *window_user) {
    if (!root || root->initialized || (capacity && !instances) ||
        (registry && (registry->count > registry->capacity ||
                      (registry->capacity && !registry->entries) || registry->count > capacity)))
        return false;
    size_t count = registry ? registry->count : 0;
    for (size_t i = 0; i < count; i++) {
        cui_root_plugin plugin = registry->entries[i];
        if (!plugin.key || !plugin.build || (!!plugin.decorate_begin != !!plugin.decorate_end))
            return false;
    }
    for (size_t i = 0; i < count; i++) {
        cui_root_plugin plugin = registry->entries[i];
        void *state = plugin.build(plugin.registration_user, window_user);
        if (!state) {
            for (size_t j = i; j > 0; j--) {
                cui_root_plugin_instance *prior = &instances[j - 1];
                if (prior->plugin.destroy)
                    prior->plugin.destroy(prior->plugin.registration_user, prior->state);
                *prior = (cui_root_plugin_instance){0};
            }
            return false;
        }
        instances[i] = (cui_root_plugin_instance){plugin, state};
    }
    *root = (cui_root){.plugins = instances,
                       .plugin_count = count,
                       .plugin_capacity = capacity,
                       .initialized = true};
    return true;
}
void cui_root_dispose(cui_root *root) {
    if (!root || !root->initialized)
        return;
    for (size_t i = root->plugin_count; i > 0; i--) {
        cui_root_plugin_instance *plugin = &root->plugins[i - 1];
        if (plugin->plugin.destroy)
            plugin->plugin.destroy(plugin->plugin.registration_user, plugin->state);
        *plugin = (cui_root_plugin_instance){0};
    }
    *root = (cui_root){0};
}
void *cui_root_plugin_state(const cui_root *root, uint64_t key) {
    if (!root || !root->initialized || !key)
        return NULL;
    for (size_t i = 0; i < root->plugin_count; i++)
        if (root->plugins[i].plugin.key == key)
            return root->plugins[i].state;
    return NULL;
}
bool cui_root_bind_facilities(cui_root *root, cui_root_surface *dialogs, size_t dialog_capacity,
                              cui_notification_state *notifications) {
    if (!root || !root->initialized || (dialog_capacity && !dialogs) || dialog_capacity > 8 ||
        root->dialog_count)
        return false;
    root->dialogs = dialogs;
    root->dialog_capacity = dialog_capacity;
    root->notifications = notifications;
    return true;
}
static void cui_root_compact_dialogs(cui_root *root) {
    size_t output = 0;
    for (size_t i = 0; i < root->dialog_count; i++)
        if (root->dialogs[i].surface && root->dialogs[i].surface->overlay.open)
            root->dialogs[output++] = root->dialogs[i];
    root->dialog_count = output;
}
bool cui_root_dialog_open(cui_root *root, cui_context *ui, cui_root_surface dialog,
                          cui_id trigger) {
    if (!root || !root->initialized || !ui || !dialog.surface || !dialog.surface->overlay.id ||
        (dialog.surface->kind != CUI_SURFACE_DIALOG && dialog.surface->kind != CUI_SURFACE_ALERT) ||
        (dialog.title.length && !dialog.title.data) ||
        (dialog.description.length && !dialog.description.data) || !dialog.content)
        return false;
    cui_root_compact_dialogs(root);
    for (size_t i = 0; i < root->dialog_count; i++)
        if (root->dialogs[i].surface == dialog.surface ||
            root->dialogs[i].surface->overlay.id == dialog.surface->overlay.id)
            return false;
    if (root->dialog_count == root->dialog_capacity)
        return false;
    root->dialogs[root->dialog_count++] = dialog;
    cui_surface_open(ui, dialog.surface, trigger);
    return true;
}
bool cui_root_sheet_open(cui_root *root, cui_context *ui, cui_root_surface sheet, cui_id trigger) {
    if (!root || !root->initialized || !ui || !sheet.surface || !sheet.surface->overlay.id ||
        sheet.surface->kind != CUI_SURFACE_SHEET || (sheet.title.length && !sheet.title.data) ||
        (sheet.description.length && !sheet.description.data) || !sheet.content)
        return false;
    if (root->sheet.surface && root->sheet.surface != sheet.surface &&
        root->sheet.surface->overlay.open)
        cui_surface_close(ui, root->sheet.surface, CUI_SURFACE_CLOSED);
    root->sheet = sheet;
    cui_surface_open(ui, sheet.surface, trigger);
    return true;
}
bool cui_root_notification_push(cui_root *root, cui_notification note, double now_seconds) {
    return root && root->initialized && root->notifications &&
           cui_notification_push(root->notifications, note, now_seconds);
}
bool cui_root_set_copy_hook(cui_root *root, cui_root_copy_fn copy, void *user) {
    if (!root || !root->initialized)
        return false;
    root->copy_selection = copy;
    root->copy_user = user;
    return true;
}
bool cui_base_root_render(cui_context *ui, cui_id id, cui_root *root, cui_rect viewport,
                          cui_root_style defaults, const cui_root_style_override *instance_override,
                          cui_root_content_fn content, void *content_user,
                          cui_root_content_fn facilities, void *facilities_user) {
    if (!ui || !id || !root || !root->initialized || !content || !isfinite(viewport.x) ||
        !isfinite(viewport.y) || !isfinite(viewport.w) || !isfinite(viewport.h) ||
        viewport.x != 0 || viewport.y != 0 || viewport.w <= 0 || viewport.h <= 0 ||
        !isfinite(defaults.padding) || defaults.padding < 0 ||
        (instance_override && instance_override->padding &&
         (!isfinite(instance_override->value.padding) || instance_override->value.padding < 0)))
        return false;
    for (size_t i = 0; i < root->plugin_count; i++) {
        cui_root_plugin_instance *plugin = &root->plugins[i];
        if (plugin->plugin.prepare)
            plugin->plugin.prepare(ui, plugin->state);
    }
    cui_root_style style = defaults;
    for (size_t i = 0; i < root->plugin_count; i++) {
        cui_root_plugin_instance *plugin = &root->plugins[i];
        if (plugin->plugin.style)
            plugin->plugin.style(ui, plugin->state, &style);
    }
    if (instance_override) {
        if (instance_override->background)
            style.background = instance_override->value.background;
        if (instance_override->foreground)
            style.foreground = instance_override->value.foreground;
        if (instance_override->padding)
            style.padding = instance_override->value.padding;
    }
    if (!isfinite(style.padding) || style.padding < 0)
        return false;
    for (size_t i = root->plugin_count; i > 0; i--) {
        cui_root_plugin_instance *plugin = &root->plugins[i - 1];
        if (plugin->plugin.decorate_begin)
            plugin->plugin.decorate_begin(ui, plugin->state, style);
    }
    cui_begin_box(ui, id,
                  (cui_box){.width = viewport.w,
                            .height = viewport.h,
                            .padding = style.padding,
                            .background = style.background});
    content(ui, content_user);
    cui_begin_absolute_box(ui, id ^ UINT64_C(0x6c182a75db0394ef), (cui_box){0},
                           (cui_rect){0, 0, -1, -1});
    for (size_t i = 0; i < root->plugin_count; i++) {
        cui_root_plugin_instance *plugin = &root->plugins[i];
        if (plugin->plugin.overlay)
            plugin->plugin.overlay(ui, plugin->state, viewport);
    }
    cui_end_box(ui);
    if (facilities)
        facilities(ui, facilities_user);
    if (root->copy_selection) {
        size_t event_count = 0;
        const cui_event *events = cui_frame_events(ui, &event_count);
        for (size_t i = 0; i < event_count; i++)
            if (!cui_event_consumed(ui, i) && events[i].type == CUI_EVENT_KEY_DOWN &&
                events[i].key == CUI_KEY_C &&
                (events[i].modifiers & (CUI_MOD_CTRL | CUI_MOD_META)) &&
                root->copy_selection(ui, root->copy_user))
                cui_consume_event(ui, i);
    }
    cui_end_box(ui);
    for (size_t i = 0; i < root->plugin_count; i++) {
        cui_root_plugin_instance *plugin = &root->plugins[i];
        if (plugin->plugin.decorate_end)
            plugin->plugin.decorate_end(ui, plugin->state);
    }
    return true;
}
