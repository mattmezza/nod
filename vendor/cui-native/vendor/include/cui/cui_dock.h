/* SPDX-License-Identifier: MIT */
#ifndef CUI_DOCK_H
#define CUI_DOCK_H
#include "cui.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
#define CUI_DOCK_NODE_LIMIT 4095
#define CUI_DOCK_PANEL_LIMIT 4096
#define CUI_DOCK_SERIALIZED_LIMIT 1048576
typedef struct cui_dock cui_dock;
typedef uint64_t cui_dock_node;
typedef enum {
    CUI_DOCK_OK,
    CUI_DOCK_INVALID,
    CUI_DOCK_CAPACITY,
    CUI_DOCK_DUPLICATE_PANEL,
    CUI_DOCK_NOT_FOUND
} cui_dock_result;
typedef enum { CUI_DOCK_TABS, CUI_DOCK_SPLIT_HORIZONTAL, CUI_DOCK_SPLIT_VERTICAL } cui_dock_kind;
typedef struct {
    size_t node_capacity, panel_capacity, max_depth, max_serialized_bytes;
} cui_dock_config;
typedef struct {
    cui_dock_node id, parent, first, second;
    cui_dock_kind kind;
    double ratio, min_width, min_height;
    size_t panel_count;
    bool has_active;
    uint64_t active_panel;
    size_t child_count;
    bool nary; /* Canonical optional-size split; false for legacy ratio nodes. */
} cui_dock_node_info;
typedef struct {
    cui_dock_node node;
    cui_rect bounds;
    size_t panel_count;
    bool has_active;
    uint64_t active_panel;
} cui_dock_geometry;
cui_dock *cui_dock_create(cui_dock_config config);
/* Explicit optional-size/n-ary edit mode; legacy constructor remains binary. */
cui_dock *cui_dock_create_nary(cui_dock_config config);
typedef enum { CUI_DOCK_ROOT_ANY, CUI_DOCK_ROOT_SPLIT } cui_dock_root_kind;
/* Canonical source-style constructor: initially an empty horizontal split. */
cui_dock *cui_dock_create_with_root(cui_dock_config, cui_dock_root_kind);
cui_dock_root_kind cui_dock_root_policy(const cui_dock *);
void cui_dock_destroy(cui_dock *dock);
cui_dock_node cui_dock_root(const cui_dock *dock);
size_t cui_dock_memory_bytes(const cui_dock *dock);
/* Pure host visibility; hidden members remain in the serialized model. */
typedef bool (*cui_dock_visibility_fn)(void *, const cui_dock *, uint64_t panel);
size_t cui_dock_visible_count(const cui_dock *, cui_dock_node, cui_dock_visibility_fn, void *);
bool cui_dock_displayed_panel(const cui_dock *, cui_dock_node, cui_dock_visibility_fn, void *,
                              uint64_t *panel);
bool cui_dock_node_get(const cui_dock *dock, cui_dock_node node, cui_dock_node_info *info);
bool cui_dock_panel_at(const cui_dock *dock, cui_dock_node tab, size_t index, uint64_t *panel);
bool cui_dock_panel_find(const cui_dock *dock, uint64_t panel, cui_dock_node *tab, size_t *index);
cui_dock_result cui_dock_insert(cui_dock *dock, cui_dock_node tab, uint64_t panel, size_t index);
cui_dock_result cui_dock_split(cui_dock *dock, cui_dock_node tab, cui_dock_kind axis, double ratio,
                               uint64_t new_panel, bool before, cui_dock_node *new_tab);
cui_dock_result cui_dock_move(cui_dock *dock, uint64_t panel, cui_dock_node destination,
                              size_t index);
/* Atomically move an existing panel into a new split beside destination.
 * Rejection preserves the model and output. A sole panel cannot split itself. */
cui_dock_result cui_dock_move_split(cui_dock *, uint64_t panel, cui_dock_node destination,
                                    cui_dock_kind axis, double ratio, bool before,
                                    cui_dock_node *new_tab);
/* Atomic two-model transfer. TABS inserts at index; split axes create a new leaf.
 * Both models and output are unchanged on rejection. Models must be distinct. */
cui_dock_result cui_dock_transfer(cui_dock *source, uint64_t panel, cui_dock *destination,
                                  cui_dock_node target, size_t index, cui_dock_kind axis,
                                  double ratio, bool before, cui_dock_node *new_tab);
cui_dock_result cui_dock_reorder(cui_dock *dock, uint64_t panel, size_t index);
/* Atomic bulk removal; missing/duplicate keys reject the entire operation. */
cui_dock_result cui_dock_remove_many(cui_dock *, const uint64_t *panels, size_t count);
cui_dock_result cui_dock_remove(cui_dock *dock, uint64_t panel);
cui_dock_result cui_dock_activate(cui_dock *dock, uint64_t panel);
cui_dock_result cui_dock_set_ratio(cui_dock *dock, cui_dock_node split, double ratio);
cui_dock_result cui_dock_balance(cui_dock *, cui_dock_node split);
/* Flatten only descendants sharing split's axis, in visual order. NULL slots
 * queries count. Insufficient capacity leaves output untouched. */
cui_dock_result cui_dock_split_slots(const cui_dock *, cui_dock_node split, cui_dock_node *slots,
                                     size_t capacity, size_t *count);
/* Adopt concrete positive slot extents and gap into existing binary ratios.
 * Count must match split_slots. Atomic; IDs/topology/v1 format stay unchanged.
 * Exact extents apply at sum(sizes)+(count-1)*gap, subject to minima. */
cui_dock_result cui_dock_set_slot_sizes(cui_dock *, cui_dock_node split, const double *sizes,
                                        size_t count, double gap);
cui_dock_result cui_dock_set_minimum(cui_dock *dock, cui_dock_node node, double width,
                                     double height);
/* One rectangle per tab node. NULL output queries count. Minima that cannot
   fit are proportionally compressed and fit_minimum becomes false. */
cui_dock_result cui_dock_layout(const cui_dock *dock, cui_rect bounds, float gap,
                                cui_dock_geometry *output, size_t capacity, size_t *count,
                                bool *fit_minimum);
/* Derived split rectangles use the same ratio/minimum compression as layout.
 * Child minima are aggregate subtree minima (before proportional compression). */
typedef struct {
    cui_rect first, second;
    cui_size first_minimum, second_minimum;
    bool fit_minimum;
} cui_dock_split_geometry;
cui_dock_result cui_dock_split_layout(const cui_dock *, cui_dock_node split, cui_rect, float gap,
                                      cui_dock_split_geometry *);
/* Filtered geometry omits all-hidden leaves/subtrees and their gaps/minima.
 * Durable ratios/IDs are unchanged. Geometry metadata describes visible panels. */
cui_dock_result cui_dock_layout_visible(const cui_dock *, cui_rect, float gap,
                                        cui_dock_visibility_fn, void *, cui_dock_geometry *,
                                        size_t capacity, size_t *count, bool *fit_minimum);
typedef struct {
    cui_dock_split_geometry geometry;
    bool first_visible, second_visible;
} cui_dock_visible_split_geometry;
cui_dock_result cui_dock_split_layout_visible(const cui_dock *, cui_dock_node, cui_rect, float gap,
                                              cui_dock_visibility_fn, void *,
                                              cui_dock_visible_split_geometry *);
typedef struct {
    bool has_size;
    double size;
} cui_dock_slot_size;
bool cui_dock_child_at(const cui_dock *, cui_dock_node split, size_t index, cui_dock_node *child,
                       cui_dock_slot_size *size);
/* Exact immediate-child count; canonical splits only. Rejection is atomic. */
cui_dock_result cui_dock_set_sizes(cui_dock *, cui_dock_node split, const cui_dock_slot_size *,
                                   size_t count);
typedef struct {
    cui_dock_node node;
    cui_rect bounds;
    cui_size minimum;
    bool visible;
} cui_dock_child_geometry;
/* Arbitrary immediate children; hidden slots have visible=false. */
cui_dock_result cui_dock_children_layout(const cui_dock *, cui_dock_node split, cui_rect, float gap,
                                         cui_dock_visibility_fn, void *, cui_dock_child_geometry *,
                                         size_t capacity, size_t *count, bool *fit_minimum);
/* Required serialized length excludes NUL. Insufficient output is untouched.
   Restore is bounded and transactional; successful restore invalidates IDs. */
cui_dock_result cui_dock_serialize(const cui_dock *dock, char *output, size_t capacity,
                                   size_t *length);
cui_dock_result cui_dock_restore(cui_dock *dock, cui_str serialized);
/* Frozen PanelState/PanelInfo shape; names, payload and input children are borrowed.
 * JSON parsing is host-owned. Payload represents the Panel variant's opaque value. */
typedef enum {
    CUI_DOCK_SCHEMA_STACK,
    CUI_DOCK_SCHEMA_TABS,
    CUI_DOCK_SCHEMA_PANEL
} cui_dock_schema_kind;
typedef struct cui_dock_schema_node {
    cui_str panel_name, payload;
    cui_dock_schema_kind kind;
    const struct cui_dock_schema_node *children;
    size_t child_count;
    const double *sizes;
    size_t size_count, axis, active_index;
} cui_dock_schema_node;
/* Pure key mapping: do not create live panels or mutate the model in this callback. */
typedef bool (*cui_dock_schema_key_fn)(void *, const cui_dock_schema_node *, uint64_t *key);
cui_dock_result cui_dock_schema_import(cui_dock *, const cui_dock_schema_node *,
                                       cui_dock_schema_key_fn, void *);
/* Source supplies complete leaf states; returned borrowed data must outlive output. */
typedef bool (*cui_dock_schema_dump_fn)(void *, uint64_t key, cui_dock_schema_node *);
/* Output root is nodes[0]. Query with nodes=NULL. Capacity/callback failure leaves
 * buffers untouched; counts report required capacities only after valid traversal. */
cui_dock_result cui_dock_schema_export(const cui_dock *, cui_dock_schema_dump_fn, void *,
                                       cui_dock_schema_node *nodes, size_t node_capacity,
                                       double *sizes, size_t size_capacity, size_t *node_count,
                                       size_t *size_count);
/* Styled adapter; the domain model above remains independently linkable. */
typedef struct cui_dock_view cui_dock_view;
struct cui_menu_entry;
#define CUI_DOCK_CUSTOM_MENU_LIMIT 32
typedef enum {
    CUI_DOCK_MENU_CLOSE,
    CUI_DOCK_MENU_CLOSE_OTHERS,
    CUI_DOCK_MENU_CLOSE_ALL,
    CUI_DOCK_MENU_ZOOM,
    CUI_DOCK_MENU_SPLIT_HORIZONTAL,
    CUI_DOCK_MENU_SPLIT_VERTICAL,
    CUI_DOCK_MENU_BALANCE
} cui_dock_menu_action;
/* Zero preserves the original adapter's both-controls default. */
typedef enum {
    CUI_DOCK_ZOOM_BOTH,
    CUI_DOCK_ZOOM_MENU,
    CUI_DOCK_ZOOM_TOOLBAR,
    CUI_DOCK_ZOOM_NONE
} cui_dock_zoom_control;
typedef enum { CUI_DOCK_PANEL_TAB_BAR, CUI_DOCK_PANEL_AUTO } cui_dock_panel_style;
typedef struct {
    cui_color background, foreground;
} cui_dock_title_style;
typedef struct {
    uint64_t key;
    cui_str title;
    bool closable, disabled;
    void (*render)(cui_context *, uint64_t panel, void *user);
    void *user;
    cui_id focus_target; /* Optional content control; used when focus_content is enabled. */
    bool zoomable;
    void (*toolbar)(cui_context *, uint64_t panel, void *user);
    const struct cui_menu_entry *menu_items;
    size_t menu_count;
    void (*menu_action)(void *user, uint64_t panel, cui_id action);
    cui_dock_zoom_control zoom_control;
    /* Optional borrowed title composition; positive widths reserve clipped slots. */
    void (*title_render)(cui_context *, uint64_t panel, void *user);
    void (*title_suffix)(cui_context *, uint64_t panel, void *user);
    float title_width, title_suffix_width;
    bool hide_title_bar; /* Auto single-panel mode; multiple/collapsed tabs remain reachable. */
    bool omit_inner_padding;
    void (*on_added)(void *, uint64_t panel, const cui_dock *, cui_dock_node tab);
    void (*on_removed)(void *, uint64_t panel);
    void (*set_active)(void *, uint64_t panel, bool);
    void (*set_zoomed)(void *, uint64_t panel, bool);
    const void *lifecycle_identity; /* Optional token distinguishes replacement at the same key. */
    /* Borrowed palette for Auto single-panel titles; custom scenes choose their own ink. */
    const cui_dock_title_style *title_style;

} cui_dock_panel;
/* Caller-owned reconstruction seam; strings/payloads are borrowed opaque bytes. */
typedef struct {
    uint64_t key;
    cui_str name, payload;
} cui_dock_panel_state;
typedef struct {
    const cui_dock *model;
    cui_dock_node tab;
    const cui_dock_panel_state *state;
    const cui_dock_schema_node
        *source; /* Full borrowed leaf for typed reconstruction; else NULL. */
} cui_dock_panel_build_context;
typedef struct cui_dock_panel_factory {
    cui_str name;
    void *user;
    cui_dock_result (*build)(void *, const cui_dock_panel_build_context *, cui_dock_panel *);
    void (*destroy)(void *, cui_dock_panel *);
    cui_dock_result (*dump)(void *, const cui_dock_panel *, cui_str *payload);
    cui_dock_result (*dump_schema)(void *, const cui_dock_panel *, cui_dock_schema_node *);
} cui_dock_panel_factory;
typedef struct {
    uint64_t key;
    cui_dock_schema_node source; /* Borrowed full PanelState leaf, including children. */
} cui_dock_panel_typed_state;
typedef struct {
    cui_dock_panel_state state;
    cui_dock_panel panel;
    const cui_dock_panel_factory *factory; /* NULL: unknown-name placeholder. */
    cui_dock_schema_node source;
    bool has_source;
} cui_dock_panel_instance;
/* Exact key coverage required. On failure output/model are unchanged and staged
 * successful builds are destroyed. Output must not contain live instances. */
cui_dock_result cui_dock_panels_reconstruct(const cui_dock *, const cui_dock_panel_state *,
                                            size_t state_count, const cui_dock_panel_factory *,
                                            size_t factory_count, cui_dock_panel_instance *,
                                            size_t capacity);
/* Typed leaves are borrowed through release; builders see their complete source. */
cui_dock_result
cui_dock_panels_reconstruct_typed(const cui_dock *, const cui_dock_panel_typed_state *,
                                  size_t state_count, const cui_dock_panel_factory *,
                                  size_t factory_count, cui_dock_panel_instance *, size_t capacity);
/* Dump borrows the factory payload; failure leaves output unchanged. */
cui_dock_result cui_dock_panel_dump(const cui_dock_panel_instance *, cui_dock_panel_state *);
/* Preserves the complete unknown leaf, or delegates to factory dump_schema. */
cui_dock_result cui_dock_panel_dump_schema(const cui_dock_panel_instance *, cui_dock_schema_node *);
/* Calls factory destroy once, then clears instances. No lifecycle notification. */
void cui_dock_panels_release(cui_dock_panel_instance *, size_t count);
/* Lifecycle captures the complete host workspace, including closed regions. */
typedef struct {
    const cui_dock *model;
    const cui_dock_view *view; /* Optional, supplies group zoom state. */
    cui_dock_visibility_fn panel_visible;
    void *visibility_user;
    bool collapsed;
    bool (*group_collapsed)(void *, const cui_dock *, cui_dock_node);
    void *constraints_user;
} cui_dock_lifecycle_region;
typedef struct {
    cui_dock_panel panel;
    const cui_dock *model;
    cui_dock_node tab;
    bool active, displayed, group_zoomed;
} cui_dock_lifecycle_snapshot;
typedef struct {
    cui_dock_lifecycle_snapshot *current, *pending;
    size_t capacity, current_count, pending_count;
    bool has_pending, dispatching;
} cui_dock_lifecycle;
/* Storage must be distinct, disjoint arrays retained for tracker's lifetime. */
bool cui_dock_lifecycle_init(cui_dock_lifecycle *, cui_dock_lifecycle_snapshot *current,
                             cui_dock_lifecycle_snapshot *pending, size_t capacity);
/* Capture after committed edits; repeated captures coalesce before dispatch.
 * Failure leaves both committed and pending notifications unchanged. */
cui_dock_result cui_dock_lifecycle_capture(cui_dock_lifecycle *, const cui_dock_lifecycle_region *,
                                           size_t region_count, const cui_dock_panel *,
                                           size_t panel_count);
/* Call on the next host tick. Postcommit callbacks; recursive use is rejected.
 * Returns false for invalid/reentrant calls, true even when nothing is pending. */
bool cui_dock_lifecycle_dispatch(cui_dock_lifecycle *);
typedef enum {
    CUI_DOCK_SELECTED,
    CUI_DOCK_CLOSED,
    CUI_DOCK_REORDERED,
    CUI_DOCK_RESIZED,
    CUI_DOCK_MOVED,
    CUI_DOCK_SPLIT,
    CUI_DOCK_ZOOMED,
    CUI_DOCK_RESTORED
} cui_dock_event;
typedef struct {
    cui_dock_event operation; /* CLOSED, MOVED, SPLIT, REORDERED or ZOOMED */
    uint64_t panel;
    cui_dock_node source, destination;
    size_t index;
    cui_dock_kind axis;
    bool before;
} cui_dock_request;
typedef struct {
    cui_color accepted, rejected, background, text;
} cui_dock_drag_palette;
typedef struct {
    bool locked, unclosable, preserve_last, collapsed;
} cui_dock_group_constraints;
cui_dock_group_constraints cui_dock_group_sealed(void);
cui_dock_group_constraints cui_dock_group_in_split(bool alone);
typedef struct {
    float width, height, tab_height, key_step;
    bool disabled, reduced_motion, allow_reorder, focus_content;
    double now_seconds;
    void (*on_event)(void *user, cui_dock_event, uint64_t panel, cui_dock_node node);
    void *event_user;
    /* Pure callback: queried for preview and again before commit; NULL allows. */
    bool (*allow_operation)(void *user, const cui_dock_request *);
    void *policy_user;
    bool disable_preview, custom_palette;
    cui_dock_drag_palette drag_palette;
    float toolbar_height; /* Zero defaults to 32 when a toolbar/control is present. */
    bool hide_menu;
    bool collapsed;                   /* Tab strip only: no close/zoom/content or drag actions. */
    cui_dock_panel_style panel_style; /* Zero preserves the original always-tabs presentation. */
    bool inner_padding; /* Source 8px top spacing in multi-panel groups; panel may omit it. */
    /* Pure, borrowed host policy; no model/view mutation during evaluation. */
    cui_dock_group_constraints (*group_constraints)(void *, const cui_dock *, cui_dock_node);
    void *constraints_user;
    cui_dock_visibility_fn panel_visible;
    void *visibility_user;
} cui_dock_view_options;
typedef struct {
    cui_dock_result error;
    bool selected, closed, reordered, resized, fit_minimum;
    bool moved, split, denied;
    uint64_t moved_panel;
    uint64_t selected_panel, closed_panel, reordered_panel;
    bool zoomed, restored;
    size_t closed_count;
} cui_dock_view_result;
/* Bounded retained interaction storage, no per-frame allocation. Defaults 128/256. */
cui_dock_view *cui_dock_view_create(size_t node_capacity, size_t panel_capacity);
void cui_dock_view_destroy(cui_dock_view *);
void cui_dock_view_reset(cui_dock_view *);
cui_dock_view_options cui_dock_view_options_default(void);
/* View-only zoom leaves model serialization unchanged. Programmatic setters bypass policies. */
bool cui_dock_view_set_zoom(cui_dock_view *, cui_dock *, uint64_t panel, bool zoomed);
bool cui_dock_view_zoomed(const cui_dock_view *, uint64_t *panel);
cui_id cui_dock_menu_id(cui_id view, uint64_t panel);
cui_id cui_dock_menu_action_id(cui_id view, uint64_t panel, cui_dock_menu_action);
cui_id cui_dock_zoom_id(cui_id view, uint64_t panel);
cui_id cui_dock_tab_id(cui_id view, uint64_t panel);
cui_id cui_dock_close_id(cui_id view, uint64_t panel);
cui_id cui_dock_group_id(cui_id view, cui_dock_node node);
cui_id cui_dock_resize_id(cui_id view, const cui_dock *, cui_dock_node split);
cui_id cui_dock_resize_id_at(cui_id view, const cui_dock *, cui_dock_node split,
                             size_t preceding_child);
/* Submit within a frame. width/height must be positive finite logical pixels.
 * Registry strings/user data and view must survive renderer completion.
 * Model may not be mutated/destroyed by render or event callbacks.
 * Selection is immediate; close/reorder commit after scene submission and
 * invalidate a following frame. Missing registry keys paint a placeholder.
 * Resize uses the existing Resizable adapter and updates model ratios. */
cui_dock_view_result cui_dock_view_render(cui_context *, cui_id view_id, const cui_theme *,
                                          cui_dock *, cui_dock_view *, const cui_dock_panel *,
                                          size_t panel_count, cui_dock_view_options);
/* Independently retained edge docks around an ordinary center model/view. */
typedef enum { CUI_DOCK_CENTER, CUI_DOCK_LEFT, CUI_DOCK_RIGHT, CUI_DOCK_BOTTOM } cui_dock_region;
typedef struct {
    cui_dock_schema_node panel;
    cui_dock_region placement;
    double size;
    bool open;
} cui_dock_schema_edge;
typedef struct {
    bool has_version;
    size_t version;
    cui_dock_schema_node center;
    bool edge_present[3]; /* Optional left_dock, right_dock, bottom_dock fields. */
    cui_dock_schema_edge edges[3];
} cui_dock_schema_area;
typedef struct {
    cui_dock_schema_node *nodes;
    size_t node_capacity;
    double *sizes;
    size_t size_capacity, node_count, size_count;
} cui_dock_schema_buffer;
cui_dock_schema_area cui_dock_schema_area_default(void);
/* All four model pointers must be distinct/non-null. Missing edges are cleared.
 * Center is pinned; edges are unconstrained. Pure callbacks run before publication. */
cui_dock_result cui_dock_schema_area_import_models(cui_dock *const models[4],
                                                   const cui_dock_schema_area *,
                                                   cui_dock_schema_key_fn, void *);
/* metadata supplies version/presence/placement/size/open, not panel trees.
 * All buffer writes and output publication are atomic; counts publish on valid query. */
cui_dock_result cui_dock_schema_area_export_models(const cui_dock *const models[4],
                                                   const cui_dock_schema_area *metadata,
                                                   cui_dock_schema_dump_fn, void *,
                                                   cui_dock_schema_buffer buffers[4],
                                                   cui_dock_schema_area *output);
typedef struct cui_dock_workspace cui_dock_workspace;
typedef struct {
    cui_dock *model;
    cui_dock_view *view;
    const cui_dock_panel *panels;
    size_t panel_count;
    cui_size minimum; /* Zero dimensions default to 64 center / 48 edge. */
} cui_dock_region_content;
typedef struct {
    bool open[3];           /* left, right, bottom */
    float size[3];          /* preferred logical pixels, retained when closed/constrained */
    bool noncollapsible[3]; /* Zero preserves collapsible edges in existing snapshots. */
} cui_dock_workspace_snapshot;
typedef struct {
    cui_dock_region source_region, destination_region;
    uint64_t panel;
    cui_dock *source_model, *destination_model;
    cui_dock_node source, destination;
    cui_dock_kind axis;
    bool before, opens_edge;
} cui_dock_transfer_request;
typedef struct {
    cui_dock_view_options view;
    void (*on_resize)(void *, cui_dock_region, float size);
    void *resize_user;
    bool disable_cross_region, disable_edge_opening;
    /* Pure policy and post-commit notification; model/registry storage remains host-owned. */
    bool (*allow_transfer)(void *, const cui_dock_transfer_request *);
    void (*on_transfer)(void *, const cui_dock_transfer_request *);
    void *transfer_user;
    bool collapsible_bottom; /* Opt in to the frozen 29px closed bottom strip. */
} cui_dock_workspace_options;
typedef struct {
    cui_dock_result error;
    bool fit_minimum, resized;
    cui_dock_view_result regions[4];
    bool transferred, opened, denied;
    uint64_t transferred_panel;
    cui_dock_region source_region, destination_region;
} cui_dock_workspace_result;
cui_dock_workspace *cui_dock_workspace_create(void);
void cui_dock_workspace_destroy(cui_dock_workspace *);
/* Layout lock blocks user close/reorder/move/split; selection and resize remain available. */
void cui_dock_workspace_set_locked(cui_dock_workspace *, bool locked);
bool cui_dock_workspace_locked(const cui_dock_workspace *);
/* Last completed frame's actual laid-out workspace rectangle; zero before first render. */
cui_rect cui_dock_workspace_bounds(const cui_dock_workspace *);
bool cui_dock_workspace_set_edge(cui_dock_workspace *, cui_dock_region, bool open, float size);
/* Preserve open/size state; an open noncollapsible edge cannot be closed. */
bool cui_dock_workspace_set_collapsible(cui_dock_workspace *, cui_dock_region, bool collapsible);
bool cui_dock_workspace_save(const cui_dock_workspace *, cui_dock_workspace_snapshot *);
bool cui_dock_workspace_restore(cui_dock_workspace *, const cui_dock_workspace_snapshot *);
/* Version/presence belong to typed area persistence, separate from legacy snapshots. */
void cui_dock_workspace_set_version(cui_dock_workspace *, bool has_version, size_t version);
bool cui_dock_workspace_version(const cui_dock_workspace *, size_t *version);
cui_dock_result cui_dock_workspace_schema_import(cui_dock_workspace *,
                                                 const cui_dock_region_content regions[4],
                                                 const cui_dock_schema_area *,
                                                 cui_dock_schema_key_fn, void *);
cui_dock_result cui_dock_workspace_schema_export(const cui_dock_workspace *,
                                                 const cui_dock_region_content regions[4],
                                                 cui_dock_schema_dump_fn, void *,
                                                 cui_dock_schema_buffer buffers[4],
                                                 cui_dock_schema_area *output);
cui_id cui_dock_region_id(cui_id workspace, cui_dock_region);
cui_id cui_dock_edge_resize_id(cui_id workspace, cui_dock_region);
/* regions are CENTER, LEFT, RIGHT, BOTTOM; configured regions require distinct models/views.
 * Model persistence remains separate from the portable edge-size/open snapshot. */
cui_dock_workspace_result cui_dock_workspace_render(cui_context *, cui_id, const cui_theme *,
                                                    cui_dock_workspace *,
                                                    const cui_dock_region_content regions[4],
                                                    cui_dock_workspace_options);
/* Optional owned DockArea-style coordinator. Register descriptors before insertion.
 * It owns four models/views and a workspace; panel callback user data is retained
 * through release when hooks are provided. Model access is low-level: call
 * reconcile after direct edits; aggregate edit operations reconcile themselves. */
typedef struct cui_dock_area cui_dock_area;
typedef struct {
    cui_dock_panel panel;
    void (*retain)(void *user);
    void (*release)(void *user);
} cui_dock_area_panel;
cui_dock_area *cui_dock_area_create(cui_dock_config, size_t panel_capacity,
                                    size_t zoom_event_capacity);
void cui_dock_area_destroy(cui_dock_area *);
cui_dock *cui_dock_area_model(cui_dock_area *, cui_dock_region);
cui_dock_view *cui_dock_area_view(cui_dock_area *, cui_dock_region);
cui_dock_workspace *cui_dock_area_workspace(cui_dock_area *);
bool cui_dock_area_regions(cui_dock_area *, cui_dock_region_content out[4]);
bool cui_dock_area_has_dock(const cui_dock_area *, cui_dock_region);
bool cui_dock_area_dock_open(const cui_dock_area *, cui_dock_region);
bool cui_dock_area_dock_collapsible(const cui_dock_area *, cui_dock_region);
bool cui_dock_area_dock_size(const cui_dock_area *, cui_dock_region, float *size);
cui_dock_result cui_dock_area_toggle_dock(cui_dock_area *, cui_dock_region);
cui_dock_result cui_dock_area_set_dock_collapsible(cui_dock_area *, cui_dock_region, bool);
cui_dock_result cui_dock_area_set_dock_size(cui_dock_area *, cui_dock_region, float);
cui_dock_result cui_dock_area_remove_dock(cui_dock_area *, cui_dock_region);
cui_dock_result cui_dock_area_register(cui_dock_area *, cui_dock_area_panel);
cui_dock_result cui_dock_area_reconcile(cui_dock_area *);
cui_dock_result cui_dock_area_insert(cui_dock_area *, cui_dock_region, cui_dock_node tab,
                                     uint64_t key, size_t index);
cui_dock_result cui_dock_area_remove(cui_dock_area *, uint64_t key);
cui_dock_result cui_dock_area_move(cui_dock_area *, uint64_t key, cui_dock_region destination,
                                   cui_dock_node target, size_t index, cui_dock_kind axis,
                                   double ratio, bool before, cui_dock_node *new_tab);
cui_dock_result cui_dock_area_activate(cui_dock_area *, uint64_t key);
cui_dock_result cui_dock_area_set_zoom(cui_dock_area *, uint64_t key, bool zoomed);
cui_dock_result cui_dock_area_schema_import(cui_dock_area *, const cui_dock_schema_area *,
                                            cui_dock_schema_key_fn, void *);
/* Delivers source-style next-tick active net edges, then every queued zoom edge. */
bool cui_dock_area_tick(cui_dock_area *);
#ifdef __cplusplus
}
#endif
#endif
