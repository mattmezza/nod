/* SPDX-License-Identifier: MIT */
#ifndef CUI_TOOLBAR_H
#define CUI_TOOLBAR_H
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
#define CUI_TOOLBAR_ITEM_LIMIT 1024u
typedef enum {
    CUI_TOOLBAR_SMALL,
    CUI_TOOLBAR_XSMALL,
    CUI_TOOLBAR_MEDIUM,
    CUI_TOOLBAR_LARGE,
    CUI_TOOLBAR_CUSTOM
} cui_toolbar_size;
typedef struct {
    cui_toolbar_size size;
    float custom_size;
    float height, padding, gap, font_size, control_height;
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
} cui_toolbar_metrics;
/* Large resolves to Medium. Custom preserves its value for hosted controls,
 * but uses the source's Medium bar and Small wrapper height. */
bool cui_toolbar_measure(cui_toolbar_size, float custom_size, cui_toolbar_metrics *);
typedef struct {
    cui_toolbar_metrics metrics;
    bool prepared; /* Sized item: apply the control's prepare_for_toolbar hook. */
} cui_toolbar_presentation;
typedef void (*cui_toolbar_render_fn)(cui_context *, const cui_theme *,
                                      const cui_toolbar_presentation *, void *);
typedef struct {
    cui_id id; /* Required for sized wrapper; zero allowed for unsized content. */
    bool sized;
    cui_toolbar_render_fn render;
    void *user;
} cui_toolbar_item;
typedef struct {
    cui_toolbar_size size;
    float custom_size;
    bool disabled; /* Suppresses navigation only; owner disables child controls. */
    cui_str label;
    bool custom_box;
    cui_box box;                 /* Complete layout/style replacement when custom_box is true. */
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
} cui_toolbar_options;
cui_toolbar_options cui_toolbar_default(void);
/* Sized-command renderer. Accessible name is independent of the visible label.
 * An icon-only command must supply an accessible name; host tooltip delivery
 * remains the application's responsibility. */
typedef struct {
    cui_id id;
    cui_str label, accessible_name;
    bool disabled, selected;
    void (*icon)(cui_context *, float size, cui_color, void *);
    void *icon_user;
    void (*on_activate)(void *);
    void *activate_user;
} cui_toolbar_command;
void cui_toolbar_command_render(cui_context *, const cui_theme *, const cui_toolbar_presentation *,
                                void *command);

/* Base containers preserve caller layout. Pair each begin with end only when
 * begin returns true. Toolbar end handles unconsumed arrows after children. */
bool cui_base_toolbar_begin(cui_context *, cui_id, cui_str label, cui_box);
void cui_base_toolbar_end(cui_context *, cui_id, bool disabled);
bool cui_base_toolbar_group_begin(cui_context *, cui_id, cui_str label, cui_box);
void cui_base_toolbar_group_end(cui_context *);
/* Callbacks and arrays are borrowed for this call only, in source order.
 * Unsized callback gets prepared=false and has no wrapper. Sized callbacks
 * must honor metrics and prepare their control (command buttons: ghost and
 * compact). Group callbacks can pass presentation.metrics to their group. */
bool cui_toolbar(cui_context *, cui_id, const cui_theme *, const cui_toolbar_item *, size_t,
                 cui_toolbar_options);
bool cui_toolbar_group(cui_context *, cui_id, const cui_theme *, const cui_toolbar_item *, size_t,
                       cui_toolbar_options);
#ifdef __cplusplus
}
#endif
#endif
