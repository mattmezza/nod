/* SPDX-License-Identifier: MIT */
#ifndef CUI_RESIZABLE_H
#define CUI_RESIZABLE_H
#include "cui.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
#define CUI_RESIZABLE_PANEL_LIMIT 1024
typedef enum { CUI_RESIZABLE_HORIZONTAL, CUI_RESIZABLE_VERTICAL } cui_resizable_axis;
typedef enum { CUI_RESIZE_STRADDLE, CUI_RESIZE_LEADING, CUI_RESIZE_TRAILING } cui_resize_edge;
typedef enum {
    CUI_RESIZE_IDLE,
    CUI_RESIZE_HOVERED,
    CUI_RESIZE_PRESSED,
    CUI_RESIZE_DRAGGING
} cui_resize_phase;
typedef enum {
    CUI_RESIZABLE_OK,
    CUI_RESIZABLE_INVALID,
    CUI_RESIZABLE_CAPACITY,
    CUI_RESIZABLE_INFEASIBLE
} cui_resizable_error;
typedef struct {
    cui_id id;
    cui_str accessibility_label;
    float initial_size, minimum, maximum;
    bool has_initial_size, custom_range, hidden;
    cui_box box;
    void (*content)(cui_context *, void *);
    void *content_user;
} cui_resizable_panel;
typedef struct {
    cui_id id;
    float size, minimum, maximum, scratch;
    bool initialized, hidden;
    cui_rect bounds;
    cui_resize_phase phase;
    double indicator_length, indicator_opacity, start_length, start_opacity;
    double target_length, target_opacity, transition_start;
    bool indicator_initialized;
} cui_resizable_slot;
typedef struct cui_resizable_state cui_resizable_state;
typedef void (*cui_resizable_resize_fn)(void *, const cui_resizable_state *);
struct cui_resizable_state {
    cui_resizable_slot *slots;
    size_t count, capacity;
    cui_resizable_axis axis;
    float extent;
    cui_rect bounds;
    cui_id active_handle;
    size_t active_index;
    bool dragging, fit_constraints, awaiting_measure;
    cui_resizable_resize_fn on_resize;
    void *resize_user;
};
cui_resizable_panel cui_resizable_panel_default(cui_id);
bool cui_resizable_panel_valid(cui_resizable_panel);
bool cui_resizable_state_init(cui_resizable_state *, cui_resizable_slot *, size_t capacity);
/* Sync stable IDs, initial preferences, constraints, visibility and container extent.
 * INFEASIBLE still yields minimum-preserving overflow or maximum-limited unused space.
 * A responsive split needs enough combined maximum capacity for its extent;
 * capping every panel can leave no room to transfer during a drag. */
cui_resizable_error cui_resizable_sync(cui_resizable_state *, const cui_resizable_panel *, size_t,
                                       cui_resizable_axis, float extent);
bool cui_resizable_resize_panel(cui_resizable_state *, size_t index, float size);
cui_resizable_error cui_resizable_insert(cui_resizable_state *, size_t index, cui_resizable_panel);
bool cui_resizable_remove(cui_resizable_state *, size_t index);
bool cui_resizable_reset(cui_resizable_state *, size_t index);
void cui_resizable_clear(cui_resizable_state *);
/* Save/restore sizes in current stable-ID order. Restore validates the complete
 * input before mutation, applies constraints, and emits one completed resize. */
bool cui_resizable_save(const cui_resizable_state *, float *, size_t capacity);
cui_resizable_error cui_resizable_restore(cui_resizable_state *, const float *, size_t count);
/* Source nine-pixel straddling or five-pixel inside-edge band, and its one-pixel line. */
bool cui_resize_handle_rectangles(cui_resizable_axis, cui_resize_edge, cui_size container,
                                  cui_rect *band, cui_rect *line);
cui_id cui_resizable_handle_id(cui_id group, cui_id preceding_panel);
typedef struct {
    bool changed, completed;
    cui_resize_phase phase;
} cui_resize_result;
/* Pure geometry/model stays theme-free; this headless behavior submits only
 * handle semantics. Consumers supply its hit-band box at the returned ID. */
cui_resize_result cui_base_resize_handle(cui_context *, cui_id, cui_resizable_state *,
                                         size_t preceding_index, bool disabled, float key_step);
typedef struct {
    cui_resizable_axis axis;
    float extent, cross_size;
    bool disabled, reduced_motion;
    double now_seconds, duration_seconds;
    float key_step;
    cui_box box;
    /* Returning false retains the default styled hairline and pill. */
    bool (*appearance)(cui_context *, cui_id, cui_resizable_axis, cui_resize_phase, cui_rect band,
                       double length, double opacity, void *);
    void *appearance_user;
} cui_resizable_options;
/* Paint within a caller's open handle band; motion sampling remains caller-owned.
 * Leading/trailing pills obey the caller's ancestor clip. */
void cui_resize_handle_appearance(cui_context *, const cui_theme *, cui_resizable_axis,
                                  cui_resize_edge, double length, double opacity);
cui_resizable_options cui_resizable_options_default(void);
cui_resizable_error cui_resizable(cui_context *, cui_id, const cui_theme *, cui_resizable_state *,
                                  const cui_resizable_panel *, size_t, cui_resizable_options);
#ifdef __cplusplus
}
#endif
#endif
