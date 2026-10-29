/* SPDX-License-Identifier: MIT */
#ifndef CUI_ROOT_H
#define CUI_ROOT_H
#include "cui_status.h"
#include "cui_surface.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct cui_root cui_root;
typedef struct {
    cui_color background, foreground;
    float padding;
} cui_root_style;
typedef struct {
    cui_root_style value;
    bool background, foreground, padding; /* Refine only named fields. */
} cui_root_style_override;
typedef void (*cui_root_content_fn)(cui_context *, void *);
/* Host retrieves its selected text and writes to its clipboard explicitly. */
typedef bool (*cui_root_copy_fn)(cui_context *, void *);
typedef void *(*cui_root_plugin_build_fn)(void *registration_user, void *window_user);
typedef struct {
    uint64_t key; /* Application-owned stable type key. */
    cui_root_plugin_build_fn build;
    void (*destroy)(void *registration_user, void *state);
    void (*prepare)(cui_context *, void *state);
    void (*style)(cui_context *, void *state, cui_root_style *);
    void (*decorate_begin)(cui_context *, void *state, cui_root_style);
    void (*decorate_end)(cui_context *, void *state);
    void (*overlay)(cui_context *, void *state, cui_rect viewport);
    void *registration_user;
} cui_root_plugin;
typedef struct {
    cui_root_plugin plugin;
    void *state;
} cui_root_plugin_instance;
typedef struct {
    cui_root_plugin *entries;
    size_t count, capacity;
} cui_root_registry;
/* Replacement keeps its original order and affects only later root snapshots. */
bool cui_root_registry_init(cui_root_registry *, cui_root_plugin *, size_t capacity);
bool cui_root_register(cui_root_registry *, cui_root_plugin);
typedef struct {
    cui_surface_state *surface;
    cui_str title, description;
    cui_surface_options options;
    cui_root_content_fn content;
    void *user;
} cui_root_surface;
struct cui_root {
    cui_root_plugin_instance *plugins;
    size_t plugin_count, plugin_capacity;
    cui_root_surface *dialogs;
    size_t dialog_count, dialog_capacity;
    cui_root_surface sheet;
    cui_notification_state *notifications;
    cui_root_copy_fn copy_selection;
    void *copy_user;
    bool initialized;
};
/* Zero-initialize the root before its first init; dispose before reinitializing.
 * All arrays and plugin states are caller-owned and must outlive this root.
 * Invalid capacity is rejected before any factory is called or state is changed. */
bool cui_root_init(cui_root *, cui_root_plugin_instance *, size_t capacity,
                   const cui_root_registry *, void *window_user);
/* Calls optional plugin cleanup in reverse order; caller owns plugin memory. */
void cui_root_dispose(cui_root *);
void *cui_root_plugin_state(const cui_root *, uint64_t key);
bool cui_root_bind_facilities(cui_root *, cui_root_surface *dialogs, size_t dialog_capacity,
                              cui_notification_state *notifications);
bool cui_root_dialog_open(cui_root *, cui_context *, cui_root_surface, cui_id trigger);
bool cui_root_sheet_open(cui_root *, cui_context *, cui_root_surface, cui_id trigger);
bool cui_root_notification_push(cui_root *, cui_notification, double now_seconds);
bool cui_root_set_copy_hook(cui_root *, cui_root_copy_fn, void *user);
/* Base composition. Call during a frame; content and overlays each run once.
 * prepare -> style -> instance override -> decorators -> content -> overlays. */
bool cui_base_root_render(cui_context *, cui_id, cui_root *, cui_rect viewport,
                          cui_root_style defaults, const cui_root_style_override *instance_override,
                          cui_root_content_fn content, void *content_user,
                          cui_root_content_fn facilities, void *facilities_user);
/* Styled composition mounts bound Sheet, Dialog and Notification facilities
 * above application content without requiring the content callback to mention them. */
bool cui_root_render(cui_context *, cui_id, cui_root *, const cui_theme *, cui_rect viewport,
                     const cui_root_style_override *instance_override, cui_root_content_fn content,
                     void *content_user, double now_seconds);
#ifdef __cplusplus
}
#endif
#endif
