/* SPDX-License-Identifier: MIT */
#ifndef CUI_PLOT_H
#define CUI_PLOT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { CUI_PLOT_LINEAR, CUI_PLOT_LOG10 } cui_plot_scale_kind;
typedef enum {
    CUI_PLOT_OK,
    CUI_PLOT_INVALID,
    CUI_PLOT_DEGENERATE,
    CUI_PLOT_OVERFLOW,
    CUI_PLOT_CAPACITY,
    CUI_PLOT_EMPTY
} cui_plot_result;
typedef struct {
    cui_plot_scale_kind kind;
    double domain_min, domain_max, range_start, range_end;
    bool clamp;
} cui_plot_scale;
/* Source-compatible Linear tick model, separate from strict navigation scales.
 * Extent constructor copies finite unordered values (<=4096); no borrowed storage.
 * Empty/single-valued domains have no tick (EMPTY, output unchanged). Equal pixel
 * endpoints are valid, mapping every finite value to the same pixel. No clamp,
 * inverse, ticks or implicit nice domain; extrapolation is intentional. */
typedef struct {
    double domain_min, domain_max, range_start, range_end;
} cui_plot_linear_extent;
cui_plot_result cui_plot_linear_extent_init(cui_plot_linear_extent *, const double *domain,
                                            size_t count, double range_start, double range_end);
cui_plot_result cui_plot_linear_extent_tick(const cui_plot_linear_extent *, double value,
                                            double *pixel);
typedef struct {
    double x, y;
} cui_plot_point;
typedef struct {
    double x_min, y_min, x_max, y_max;
} cui_plot_rect;
typedef struct {
    cui_plot_point start, end;
} cui_plot_line;
/* Allocation-free categorical scale models, using application-defined keys.
 * Point/Ordinal borrow arrays; Band copies a first-occurrence deduplicated
 * domain into caller storage. All buffers must outlive their models. */
#define CUI_PLOT_CATEGORY_LIMIT 4096u
#define CUI_PLOT_ORDINAL_BYTE_LIMIT 1048576u
typedef struct {
    double range_start, range_end, padding_inner, padding_outer, max_band_width;
    size_t band_count;
    bool has_max_band_width;
} cui_plot_band_options;
typedef struct {
    uint64_t *domain;
    size_t count, capacity;
    cui_plot_band_options options;
} cui_plot_band_scale;
typedef struct {
    double start, step, band_width;
    size_t band_count;
} cui_plot_band_geometry;
/* On CAPACITY, required reports distinct key count; scale/storage are untouched.
 * NULL storage with zero capacity queries required storage. Initialization is
 * transactional, including when input keys alias destination storage. */
cui_plot_result cui_plot_band_init(cui_plot_band_scale *, uint64_t *storage, size_t capacity,
                                   const uint64_t *domain, size_t count, cui_plot_band_options,
                                   size_t *required);
/* Band order always starts at the lower endpoint, including reversed ranges.
 * Inner padding is 0..1; outer padding is nonnegative and at most half the
 * effective band count. Width cap affects widths, not ticks/steps. */
cui_plot_result cui_plot_band_layout(const cui_plot_band_scale *, cui_plot_band_geometry *);
cui_plot_result cui_plot_band_map(const cui_plot_band_scale *, uint64_t key, double *pixel);
cui_plot_result cui_plot_band_at(const cui_plot_band_scale *, size_t index, double *pixel);
/* May identify reserved empty bands beyond domain.count. Empty/zero-step scales
 * return defined clamped indices; see PLOT_MODEL.md. */
cui_plot_result cui_plot_band_nearest(const cui_plot_band_scale *, double pixel, size_t *index);
typedef struct {
    const uint64_t *domain;
    size_t count;
    double range_start, range_end;
} cui_plot_point_scale;
/* Point preserves duplicate keys and range direction. Key lookup finds the
 * first occurrence; at() maps each entry, including later duplicates. */
cui_plot_result cui_plot_point_init(cui_plot_point_scale *, const uint64_t *domain, size_t count,
                                    double range_start, double range_end);
cui_plot_result cui_plot_point_map(const cui_plot_point_scale *, uint64_t key, double *pixel);
cui_plot_result cui_plot_point_at(const cui_plot_point_scale *, size_t index, double *pixel);
cui_plot_result cui_plot_point_nearest(const cui_plot_point_scale *, double pixel, size_t *index);
typedef struct {
    const uint64_t *domain;
    size_t count;
    const void *range, *unknown;
    size_t range_count, element_size;
    bool has_unknown;
} cui_plot_ordinal_scale;
/* Generic range elements are copied with memmove, preserving their C type's
 * bytes. Range and optional unknown value are borrowed. No implicit domain
 * insertion. Known keys use their first index modulo range_count; a known key
 * with empty range stays EMPTY even when unknown is configured. */
cui_plot_result cui_plot_ordinal_init(cui_plot_ordinal_scale *, const uint64_t *domain,
                                      size_t count, const void *range, size_t range_count,
                                      size_t element_size, const void *unknown);
/* EMPTY/INVALID/OVERFLOW/CAPACITY leave output unchanged. NULL output/zero
 * capacity queries required element capacity for a resolvable key. */
cui_plot_result cui_plot_ordinal_map(const cui_plot_ordinal_scale *, uint64_t key, void *output,
                                     size_t output_capacity);
/* Pure geometry: coordinates are already projected by the application.
 * Output/input/count pointers must not overlap. Two-pass builders never write
 * partial output on error; required is published only for OK/CAPACITY.
 * At most CUI_PLOT_CATEGORY_LIMIT samples/stack cells are accepted. */
typedef struct {
    double x, y0, y1;
    bool defined;
} cui_plot_sample;
typedef enum {
    CUI_PLOT_CURVE_LINEAR,
    CUI_PLOT_CURVE_NATURAL,
    CUI_PLOT_CURVE_STEP_AFTER
} cui_plot_curve;
typedef enum { CUI_PLOT_GAPS_SKIP, CUI_PLOT_GAPS_SPLIT } cui_plot_gap_policy;
typedef enum {
    CUI_PLOT_PATH_MOVE,
    CUI_PLOT_PATH_LINE,
    CUI_PLOT_PATH_CUBIC,
    CUI_PLOT_PATH_CLOSE,
    CUI_PLOT_PATH_ARC
} cui_plot_path_kind;
typedef struct {
    cui_plot_path_kind kind;
    cui_plot_point point, control1, control2;
    double radius; /* ARC: circular endpoint arc, no axis rotation */
    bool large_arc, clockwise;
} cui_plot_path_command;
/* Line uses y1. Area follows y1 then reverses y0 and closes each run.
 * Undefined samples' numbers are ignored. SKIP bridges them (frozen GPUI
 * behavior); SPLIT starts a new run. StepAfter omits the final vertical leg,
 * matching the source line. Area always closes even single-point runs. */
cui_plot_result cui_plot_path(const cui_plot_sample *, size_t count, cui_plot_curve,
                              cui_plot_gap_policy, bool area, cui_plot_path_command *,
                              size_t capacity, size_t *required);
/* Frozen Area's independent accessor missingness and optional constant
 * baseline. This is distinct from the extended closed per-point area path. */
typedef struct {
    double x, y;
    bool has_x, has_y;
} cui_plot_area_sample;
typedef struct {
    cui_plot_path_command *fill, *stroke;
    size_t fill_capacity, stroke_capacity, fill_count, stroke_count;
} cui_plot_area_buffers;
/* SKIP missing x/y pairs. Fill and stroke share the upper curve. Fill closes
 * only when >1 pair remains, baseline is present, and original first/last x
 * exist; closure uses those original x values even if their y is absent.
 * Singleton emits Move only, ignoring baseline (source early return).
 * Without closure the fill path is open; renderer fill semantics decide its
 * implicit closure. INVALID/OVERFLOW preserve counts and both command buffers;
 * CAPACITY publishes both counts without partial output. All buffers disjoint
 * and stable; count <=4096. Baseline is ignored when has_baseline=false. */
cui_plot_result cui_plot_area_paths(const cui_plot_area_sample *, size_t count, cui_plot_curve,
                                    bool has_baseline, double baseline, cui_plot_area_buffers *);
typedef struct {
    double value;
    bool defined;
} cui_plot_value;
typedef struct {
    double y0, y1;
} cui_plot_stack_point;
/* Series-major cells: [series * point_count + point]. Natural series order,
 * shared signed cumulative baseline; missing values are zero (not divergent).
 * cells = series_count * point_count, with no data copies or allocations. */
cui_plot_result cui_plot_stack(const cui_plot_value *, size_t series_count, size_t point_count,
                               cui_plot_stack_point *, size_t capacity, size_t *required);
typedef struct {
    size_t index;
    double value, start_angle, end_angle, pad_angle;
} cui_plot_arc;
/* Positive defined values only, original indices retained; no sorting or pad
 * redistribution. Angles may reverse; finite nonnegative pad is metadata.
 * radians: zero at 12 o'clock, clockwise positive. */
cui_plot_result cui_plot_pie(const cui_plot_value *, size_t count, double start_angle,
                             double end_angle, double pad_angle, cui_plot_arc *, size_t capacity,
                             size_t *required);
/* Radii must be finite, 0 <= inner <= outer. Centroid is relative to center.
 * contains uses normalized [0, 2pi) screen angle and the source half-open
 * [start_angle,end_angle) test, deliberately ignoring pad_angle. Consequently
 * reversed and unnormalized angle intervals do not wrap automatically. */
cui_plot_result cui_plot_arc_centroid(const cui_plot_arc *, double inner_radius,
                                      double outer_radius, cui_plot_point *);
cui_plot_result cui_plot_arc_contains(const cui_plot_arc *, double inner_radius,
                                      double outer_radius, cui_plot_point relative_position,
                                      bool *contains);
/* Closed annular sector, centered at (0,0). Positive spans up to 2pi only;
 * reversed/zero/fully padded spans produce an empty successful path.
 * Full turns use two semicircles without the source's artificial seam.
 * Smaller sectors use source pad-width policy; pad is disabled for full turns.
 * Geometry errors/capacity are transactional; buffers must be disjoint. */
cui_plot_result cui_plot_arc_path(const cui_plot_arc *, double inner_radius, double outer_radius,
                                  cui_plot_path_command *, size_t capacity, size_t *required);
/* Polygonal tessellation of the same sector, with outer+reversed inner rim.
 * segments_per_turn: 4..1024; each rim uses ceil(span*segments/2pi) chords.
 * Returns Move/Line/Close only. Maximum sagitta is r*(1-cos(pi/segments)). */
cui_plot_result cui_plot_arc_flatten(const cui_plot_arc *, double inner_radius, double outer_radius,
                                     size_t segments_per_turn, cui_plot_path_command *,
                                     size_t capacity, size_t *required);
/* Explicit frozen Arc padding mode. SOURCE_PADDING overrides metadata pad to
 * .0001 radians whenever positive span>=pi, including full circles; its single
 * endpoint arc leaves the source seam. Also suppresses radius/span below1e-12.
 * Legacy functions use SEAMLESS. Finite ordered radii and span<=2pi remain CUI
 * bounds; this does not reproduce source f32 rounding or renderer tessellation. */
typedef enum { CUI_PLOT_ARC_SEAMLESS, CUI_PLOT_ARC_SOURCE_PADDING } cui_plot_arc_policy;
cui_plot_result cui_plot_arc_path_ex(const cui_plot_arc *, double inner_radius, double outer_radius,
                                     cui_plot_arc_policy, cui_plot_path_command *, size_t capacity,
                                     size_t *required);
cui_plot_result cui_plot_arc_flatten_ex(const cui_plot_arc *, double inner_radius,
                                        double outer_radius, size_t segments_per_turn,
                                        cui_plot_arc_policy, cui_plot_path_command *,
                                        size_t capacity, size_t *required);
typedef struct {
    double angle, radius0, radius1;
    bool defined;
} cui_plot_radial_sample;
/* Linear radial polygon relative to (0,0). Undefined samples are skipped or
 * split. closed stroke requires >=3 points; fill requires >=3 and always
 * closes (frozen RadialLine). area follows radius1 then reverse radius0;
 * radial area is a CUI extension, not a frozen GPUI RadialArea API.
 * Radii must be finite/nonnegative. area ignores closed/fill. */
cui_plot_result cui_plot_radial_path(const cui_plot_radial_sample *, size_t count,
                                     cui_plot_gap_policy, bool closed, bool fill, bool area,
                                     cui_plot_path_command *, size_t capacity, size_t *required);
typedef struct {
    cui_plot_rect frame;
    double radius, border_width;
} cui_plot_dot_geometry;
/* Source Line/RadialLine marker: square centered at point, radius=size/2,
 * one-pixel border. Finite nonnegative size; zero produces a valid degenerate
 * marker. Errors preserve output; source unchecked negative size is rejected. */
cui_plot_result cui_plot_dot_layout(cui_plot_point center, double size, cui_plot_dot_geometry *);
typedef enum {
    CUI_PLOT_BAR_BOTTOM,
    CUI_PLOT_BAR_TOP,
    CUI_PLOT_BAR_LEFT,
    CUI_PLOT_BAR_RIGHT
} cui_plot_bar_alignment;
typedef struct {
    cui_plot_rect frame;
    cui_plot_point label_origin;
    double gradient_angle;
} cui_plot_bar_geometry;
/* Already projected coordinates: cross is the leading band edge, not center.
 * Source value-end label origin uses 10px text/2px gap; no clipping or text
 * measurement. Zero width/height still returns valid degenerate geometry.
 * All orientations support values on either side of baseline. */
cui_plot_result cui_plot_bar_layout(cui_plot_bar_alignment, double cross, double band_width,
                                    double baseline, double value, cui_plot_bar_geometry *);
/* Pure path representation of source Bar's independently rounded quad.
 * Radii are top-left/top-right/bottom-right/bottom-left, finite and nonnegative;
 * each clamps to half the shorter frame side, matching CUI styled adapters.
 * Quarter circles use one cubic (kappa approximation), not renderer tessellation.
 * Always 10 commands for a nondegenerate frame, including zero-radius cubics.
 * Degenerate bars return EMPTY/required=0. Errors preserve commands; CAPACITY
 * reports required=10. NULL output/zero capacity queries storage. */
cui_plot_result cui_plot_bar_path(cui_plot_bar_alignment, double cross, double band_width,
                                  double baseline, double value, const double radii[4],
                                  cui_plot_path_command *, size_t capacity, size_t *required);
typedef struct {
    double width, height;
    const double *x, *y, *dashes;
    size_t x_count, y_count, dash_count;
} cui_plot_grid_options;
/* Source 1px centered grid quads, vertical lines then horizontal lines.
 * SVG alternating dash/gap; odd arrays repeat twice, negative lengths clamp
 * to zero, all-nonpositive/empty patterns draw solid. Extents/positions finite
 * within +/-16384 (positive extents); positions are not silently clipped.
 * At most256 lines per axis/64 dash entries/4096 output quads. If the bounded
 * generator exceeds4096 quads CAPACITY reports4097 as a lower bound, even for
 * a size query. Other capacity errors report exact count. Tiny nonadvancing
 * dash steps return DEGENERATE. All errors leave geometry unchanged; only
 * OK/CAPACITY publish required. Buffers must be disjoint/stable. */
cui_plot_result cui_plot_grid_layout(const cui_plot_grid_options *, cui_plot_rect *,
                                     size_t capacity, size_t *required);
/* Domain must ascend; pixel ranges may reverse. All endpoints finite. */
cui_plot_result cui_plot_validate(const cui_plot_scale *);
cui_plot_result cui_plot_map(const cui_plot_scale *, double value, double *pixel);
cui_plot_result cui_plot_inverse(const cui_plot_scale *, double pixel, double *value);
/* Desired count is advisory, 2..1000. Count is required capacity; no partial
 * output on capacity failure. NULL/zero output performs a size query. */
cui_plot_result cui_plot_ticks(const cui_plot_scale *, size_t desired_count, double *ticks,
                               size_t capacity, size_t *count);
cui_plot_result cui_plot_nice(cui_plot_scale *, size_t desired_count);
/* Positive pixel delta shifts plotted data toward increasing pixels.
 * Factor >1 zooms in, keeping the data at anchor_pixel fixed. */
cui_plot_result cui_plot_pan(cui_plot_scale *, double delta_pixels);
cui_plot_result cui_plot_zoom(cui_plot_scale *, double factor, double anchor_pixel);
/* Explicit caller-owned viewport. Limits are transformed domain units (log10
 * for log scales); zero span limits are unconstrained. Both axes commit together. */
typedef struct {
    double minimum, maximum, min_span, max_span;
    bool bounded;
} cui_plot_viewport_limit;
typedef struct {
    bool enabled, disabled, pan_x, pan_y, zoom_x, zoom_y;
    cui_plot_viewport_limit x_limit, y_limit;
} cui_plot_viewport_options;
typedef struct {
    cui_plot_scale x, y, initial_x, initial_y;
    double pointer_x, pointer_y;
    bool initialized, dragging;
} cui_plot_viewport;
cui_plot_viewport_options cui_plot_viewport_defaults(void);
cui_plot_result cui_plot_viewport_init(cui_plot_viewport *, cui_plot_scale x, cui_plot_scale y);
cui_plot_result cui_plot_viewport_reset(cui_plot_viewport *);
/* Pan precedes zoom at the new center; a pinch preserves data under the old
 * center at the moved center. Positive factor >1 zooms in. */
cui_plot_result cui_plot_viewport_transform(cui_plot_viewport *, cui_plot_viewport_options,
                                            double dx, double dy, double factor,
                                            cui_plot_point anchor);
struct cui_context;
/* Reduce before projecting, bind after submitting cui_plot_view. Shift-primary
 * mouse drag, Ctrl-scroll (precise pixels, legacy steps*32), Core touch gestures.
 * Hosts preserve modifiers and deliver CANCEL on capture loss. */
cui_plot_result cui_plot_viewport_reduce(struct cui_context *, uint64_t id, cui_plot_viewport *,
                                         cui_plot_viewport_options);
bool cui_plot_viewport_bind(struct cui_context *, uint64_t id, const cui_plot_viewport *,
                            cui_plot_viewport_options);
cui_plot_result cui_plot_clip_line(cui_plot_line, cui_plot_rect viewport, cui_plot_line *out);
cui_plot_result cui_plot_project_line(const cui_plot_scale *x, const cui_plot_scale *y,
                                      cui_plot_point a, cui_plot_point b, cui_plot_rect viewport,
                                      cui_plot_line *out);
cui_plot_result cui_plot_project_bar(const cui_plot_scale *x, const cui_plot_scale *y,
                                     double center, double width_pixels, double value,
                                     double baseline, cui_plot_rect viewport, cui_plot_rect *out);
#define CUI_PLOT_SANKEY_NODE_LIMIT 256u
#define CUI_PLOT_SANKEY_LINK_LIMIT 1024u
typedef enum {
    CUI_PLOT_SANKEY_JUSTIFY,
    CUI_PLOT_SANKEY_LEFT,
    CUI_PLOT_SANKEY_RIGHT,
    CUI_PLOT_SANKEY_CENTER
} cui_plot_sankey_align;
typedef enum { CUI_PLOT_SANKEY_LINEAR, CUI_PLOT_SANKEY_SQRT } cui_plot_sankey_value_scale;
typedef struct {
    size_t source, target;
    double value;
} cui_plot_sankey_link;
typedef struct {
    size_t index, depth, height, layer, source_count, target_count;
    double value, x0, y0, x1, y1;
} cui_plot_sankey_node;
typedef struct {
    size_t index, source, target;
    double value, y0, y1, width, source_width, target_width;
} cui_plot_sankey_link_layout;
typedef struct {
    cui_plot_sankey_node *nodes;
    cui_plot_sankey_link_layout *links;
    size_t node_count, link_count, node_capacity, link_capacity;
} cui_plot_sankey_graph;
typedef struct {
    cui_plot_rect extent;
    double node_width, node_padding;
    unsigned iterations;
    cui_plot_sankey_align align;
    cui_plot_sankey_value_scale value_scale;
    bool legacy_value_alignment; /* Chart compatibility: zero-flow edges do not anchor alignment. */
} cui_plot_sankey_options;
typedef enum {
    CUI_PLOT_SANKEY_NO_ERROR,
    CUI_PLOT_SANKEY_MISSING_NODE,
    CUI_PLOT_SANKEY_CIRCULAR_LINK
} cui_plot_sankey_error_kind;
typedef struct {
    cui_plot_sankey_error_kind kind;
    size_t node;
} cui_plot_sankey_error;
/* Source defaults24px width,8px padding,Justify,6 iterations,Linear,[0,0,1,1].
 * Explicit zero width/padding/iterations are honored. Bounds finite with positive
 * height/nonnegative width, coordinates<=16384; iterations<=100. */
cui_plot_sankey_options cui_plot_sankey_defaults(void);
/* Caller supplies output arrays/capacities; count metadata set only on success.
 * No borrowed input after topology. Values finite/nonnegative; topology scales
 * once, preserving scaled values in graph. Missing/cycle errors populate detail;
 * every failure preserves graph and arrays. CAPACITY when bounded counts or
 * caller storage insufficient. Empty graphs supported. */
cui_plot_result cui_plot_sankey_topology(cui_plot_sankey_graph *, size_t node_count,
                                         const cui_plot_sankey_link *, size_t link_count,
                                         const cui_plot_sankey_options *, cui_plot_sankey_error *);
/* Complete geometry from unmodified topology, reusing ranks/scaled values.
 * value_scale is ignored here, as source layout_from does not rescale graph.
 * Relayout may change extent/alignment/width/padding/iterations. Transactional. */
cui_plot_result cui_plot_sankey_layout_from(cui_plot_sankey_graph *,
                                            const cui_plot_sankey_options *);
cui_plot_result cui_plot_sankey_layout(cui_plot_sankey_graph *, size_t node_count,
                                       const cui_plot_sankey_link *, size_t link_count,
                                       const cui_plot_sankey_options *, cui_plot_sankey_error *);
/* Shared end-breadth calculation for adapters with caller-positioned nodes.
 * Updates link y0/y1/source_width/target_width from current node y0/y1 and
 * scaled values; nominal width and topology remain unchanged. Transactional.
 * Requires only finite ordered node y coordinates, valid endpoint indices and
 * finite nonnegative scaled link values; ranks/topology are not recomputed.
 * Models source compute_link_breadths, not an additional frozen public export. */
cui_plot_result cui_plot_sankey_link_breadths(cui_plot_sankey_graph *);
size_t cui_plot_sankey_layer_count(const cui_plot_sankey_graph *);
/* Query adjacency in opposite-node y0 order with input-index ties, matching
 * layout link ordering; topology coordinates y0=0 preserve input order. */
cui_plot_result cui_plot_sankey_node_links(const cui_plot_sankey_graph *, size_t node,
                                           bool outgoing, size_t *, size_t capacity,
                                           size_t *required);
/* Geometry consumed by the frozen sankey_link_path function. source_x is the
 * source node's right edge, target_x the target node's left edge. End widths
 * are independent (tapered ribbons); y coordinates are their centerlines. */
typedef struct {
    double source_x, target_x, source_y, target_y, source_width, target_width;
} cui_plot_sankey_ribbon;
/* Move/Cubic/Line/Cubic/Close, midpoint-X controls, explicit origin offset.
 * Each end width=max(width,min_width), all widths finite/nonnegative. Zero
 * widths still emit a closed degenerate contour. Exactly5 commands; errors
 * preserve output, capacity queries report5. No topology/layout solver. */
cui_plot_result cui_plot_sankey_link_path(const cui_plot_sankey_ribbon *, double min_width,
                                          cui_plot_point origin, cui_plot_path_command *,
                                          size_t capacity, size_t *required);
/* PlotElement's cursor visibility gate, separated from platform event routing.
 * Zero initialize. Global positions are finite; bounds are a positive finite
 * rectangle. Application supplies occlusion-aware hovered/default-prevented
 * decisions and detected long-press phases. Desktop refreshes each frame;
 * touch tracking begins only on accepted long press and ends on lift/cancel. */
typedef struct {
    cui_plot_point relative;
    bool visible;
} cui_plot_cursor_memory;
typedef struct {
    bool changed, request_frame, prevent_default;
} cui_plot_cursor_update;
typedef enum {
    CUI_PLOT_LONG_PRESS_STARTED,
    CUI_PLOT_LONG_PRESS_MOVED,
    CUI_PLOT_LONG_PRESS_ENDED,
    CUI_PLOT_LONG_PRESS_CANCELLED
} cui_plot_long_press_phase;
cui_plot_result cui_plot_cursor_mouse(cui_plot_cursor_memory *, cui_plot_rect bounds,
                                      cui_plot_point global_position, bool hovered,
                                      cui_plot_cursor_update *);
cui_plot_result cui_plot_cursor_long_press(cui_plot_cursor_memory *, cui_plot_rect bounds,
                                           cui_plot_point global_position,
                                           cui_plot_long_press_phase, bool hovered,
                                           bool default_prevented, cui_plot_cursor_update *);
/* Resolve from LIVE global position and CURRENT bounds, using memory only as
 * visibility gate, as source prepaint does. Suppress during appearance. On no
 * cursor, resolved=false and relative output unchanged; invalid leaves all
 * outputs untouched. Bounds contain left/top, exclude right/bottom (Core rule). */
cui_plot_result cui_plot_cursor_resolve(const cui_plot_cursor_memory *, cui_plot_rect bounds,
                                        cui_plot_point live_global_position, bool appearing,
                                        cui_plot_point *relative, bool *resolved);
/* Shape identity helper: extras then source-style f32 point/value bits, with
 * original FNV-1a hashing. Keys are opaque process-local values, not compatible
 * with Rust DefaultHasher or persistent cross-platform serialization. Include
 * curve/stroke/dash/radii inputs; exclude origin and paint-only color. */
typedef struct {
    uint64_t value;
} cui_plot_shape_key;
cui_plot_shape_key cui_plot_shape_key_init(uint64_t extra);
cui_plot_result cui_plot_shape_key_float(cui_plot_shape_key *, float value);
cui_plot_result cui_plot_shape_key_point(cui_plot_shape_key *, cui_plot_point);
/* Projected scalar transition clock used by Plot enter/exit policies. Seconds
 * are caller-supplied monotonic timestamps; backward time saturates elapsed to
 * zero. Zero-initialize each independently keyed channel. First target is
 * adopted immediately. NULL easing uses source cubic-out; callback is synchronous
 * and must return a finite sample. Invalid inputs preserve state and output. */
typedef double (*cui_plot_easing_fn)(double progress, void *user);
typedef struct {
    double duration, delay;
    cui_plot_easing_fn easing;
    void *user;
} cui_plot_transition_policy;
typedef struct {
    double from, target, started_at, reversing_factor, duration;
    bool initialized;
} cui_plot_transition_clock;
typedef enum {
    CUI_PLOT_MOTION_IDLE,
    CUI_PLOT_MOTION_DELAYED,
    CUI_PLOT_MOTION_RUNNING,
    CUI_PLOT_MOTION_FINISHED
} cui_plot_motion_status;
typedef struct {
    double value;
    cui_plot_motion_status status;
    bool request_frame;
} cui_plot_motion_value;
cui_plot_result cui_plot_transition_step(cui_plot_transition_clock *, double target, double now,
                                         cui_plot_transition_policy, bool reduced_motion,
                                         cui_plot_motion_value *);
/* First-paint Presence clock: unlike transition_step, initializes 0->1.
 * Linear time is kept independently of easing, so stagger samples local time
 * before easing. Complete samples bypass easing. Policy/user are borrowed. */
typedef struct {
    double time, progress;
    cui_plot_easing_fn easing;
    void *user;
    bool appearing, request_frame;
} cui_plot_appear_value;
cui_plot_result cui_plot_appear_step(cui_plot_transition_clock *, double now,
                                     cui_plot_transition_policy, bool reduced_motion,
                                     cui_plot_appear_value *);
cui_plot_result cui_plot_appear_value_stagger(const cui_plot_appear_value *, size_t index,
                                              size_t count, double spread, double *out);
/* Scalar pointer glide: source PlotHover.glide targets one Pixels channel.
 * The host owns one zero-initialized state per channel and supplies seconds.
 * Elapsed time advances toward the PREVIOUS target before retargeting. */
typedef struct {
    double response, damping, epsilon;
    bool travel;
} cui_plot_spring_policy;
typedef struct {
    double position, velocity, target, updated_at;
    bool initialized;
} cui_plot_spring_clock;
typedef struct {
    double value, velocity;
    bool request_frame;
} cui_plot_spring_value;
/* Source default damping=1, epsilon=.001, travel=true; response zero snaps. */
cui_plot_spring_policy cui_plot_spring_defaults(double response);
/* entering implements PlotHover.is_entering's temporary travel=false policy.
 * Invalid inputs/state and numeric overflow preserve clock and output. */
cui_plot_result cui_plot_spring_step(cui_plot_spring_clock *, double target, double now,
                                     cui_plot_spring_policy, bool reduced_motion, bool entering,
                                     cui_plot_spring_value *);
/* Caller-owned hover lifecycle. Dots are borrowed until replaced or exit completes;
 * live requires cursor. Progress is supplied by the caller's motion policy in [0,1].
 * Zero initialize. Invalid input leaves memory unchanged. */
typedef struct {
    size_t index;
    cui_plot_point cross_line;
    const cui_plot_point *dots;
    size_t dot_count;
} cui_plot_tooltip_state;
typedef struct {
    cui_plot_tooltip_state state;
    cui_plot_point cursor;
    double progress;
    bool has_state, hovered, entering;
} cui_plot_hover_memory;
cui_plot_result cui_plot_hover_track(cui_plot_hover_memory *, const cui_plot_tooltip_state *live,
                                     const cui_plot_point *cursor, double progress);
/* Explicit scope entry: one slot per stable application identity. Only completed
 * generations persist across disappearance; unfinished appearances restart.
 * An identity change clears the slot. No allocation or implicit frame tracking. */
typedef struct {
    uint64_t identity, completed_generation;
    bool has_completed;
} cui_plot_appear_scope;
bool cui_plot_appear_complete(cui_plot_appear_scope *, uint64_t identity, uint64_t generation);
cui_plot_result cui_plot_appear_record(cui_plot_appear_scope *, uint64_t identity,
                                       uint64_t generation, double linear_progress);
/* Bounded model of source PlotAppearScope's completed-generation map. Storage
 * belongs to caller and stays live. Stable plot identities must be unique within
 * a scope. Frame serials are supplied by the host; repeated same-frame begin is
 * idempotent, a skipped/backward frame or scope identity change clears entries.
 * Begin/end explicitly delimit access; nested composition passes the innermost
 * registry. No retained element tree, implicit frame hooks or motion clock. */
typedef struct {
    cui_plot_appear_scope *entries;
    size_t count, capacity;
    uint64_t identity, last_frame;
    bool initialized, active;
} cui_plot_appear_registry;
cui_plot_result cui_plot_appear_registry_init(cui_plot_appear_registry *,
                                              cui_plot_appear_scope *storage, size_t capacity);
cui_plot_result cui_plot_appear_registry_begin(cui_plot_appear_registry *, uint64_t scope_identity,
                                               uint64_t frame_serial);
cui_plot_result cui_plot_appear_registry_end(cui_plot_appear_registry *);
cui_plot_result cui_plot_appear_registry_complete(const cui_plot_appear_registry *,
                                                  uint64_t plot_identity, uint64_t generation,
                                                  bool *complete);
/* Only progress==1 inserts/replaces a completed generation. Partial appearance
 * never consumes storage or evicts the last completed generation. CAPACITY and
 * invalid inputs preserve registry/storage; explicit init discards all memory. */
cui_plot_result cui_plot_appear_registry_record(cui_plot_appear_registry *, uint64_t plot_identity,
                                                uint64_t generation, double linear_progress);
/* Stagger the linear clock before applying the caller's existing easing policy.
 * Spread clamps to [0,.95], index clamps to count-1. count<=1 is unstaggered. */
cui_plot_result cui_plot_appear_stagger(double linear_progress, size_t index, size_t count,
                                        double spread, double *local_progress);
/* Native path preparation: non-indexed triangle list, relative XY and unscaled
 * shader ST coordinates. Host owns clip/background/origin and mesh retention. */
typedef struct {
    cui_plot_point xy, st;
} cui_plot_mesh_vertex;
/* Single simple implicitly closed contour, 3..256 distinct points within
 * +/-16384. Concave contours and either winding are supported; collinear
 * vertices, holes, touching/self-crossing edges are rejected. Original bounded
 * ear clipping, not the source Lyon tessellator. Each triangle has ST=(0,1).
 * Query with out=NULL/capacity=0; CAPACITY sets required, preserves vertices and
 * bounds. Other errors preserve all outputs. Output must not alias inputs. */
cui_plot_result cui_plot_fill_mesh(const cui_plot_point *contour, size_t count,
                                   cui_plot_mesh_vertex *out, size_t capacity, size_t *required,
                                   cui_plot_rect *bounds);
/* Multi-contour polygon fill, implicit closure, total 3..256 points. Supports
 * holes, overlap, coincident edges and self crossings. No curve commands: flatten
 * curves first. Finite coordinates within +/-16384. Contours need >=3 points;
 * duplicate/collinear points are accepted. Scan bands split at edge crossings,
 * using long-double predicates; topology is not guaranteed for nearly coincident
 * geometry and does not reproduce Lyon triangle topology or analytic curve ST.
 * Workspace is scratch (may change on any result), and all buffers must be
 * disjoint. events_capacity must be >=n+n*(n-1)/2, order_capacity >=n where n
 * is total point count. Smaller workspace returns CAPACITY without publishing
 * required. Output CAPACITY publishes exact vertex count and preserves mesh/
 * bounds. Other errors preserve output/count/bounds. Empty fill succeeds with
 * required=0 and zero bounds. Each emitted triangle has ST=(0,1). */
typedef enum { CUI_PLOT_FILL_EVEN_ODD, CUI_PLOT_FILL_NON_ZERO } cui_plot_fill_rule;
typedef struct {
    const cui_plot_point *points;
    size_t count;
} cui_plot_contour;
typedef struct {
    long double *events;
    size_t events_capacity;
    size_t *order;
    size_t order_capacity;
} cui_plot_fill_workspace;
cui_plot_result cui_plot_fill_mesh_contours(const cui_plot_contour *, size_t contour_count,
                                            cui_plot_fill_rule, cui_plot_fill_workspace *,
                                            cui_plot_mesh_vertex *, size_t capacity,
                                            size_t *required, cui_plot_rect *bounds);
/* Adaptive cubic polyline (includes start/end), original conservative
 * de Casteljau subdivision, max4097 points/depth24. Coordinates+controls within
 * +/-16384, finite tolerance1e-6..16384. Every accepted chord is within tolerance
 * of its control hull. Source Lyon default tolerance is0.1; sample positions
 * differ from Lyon's quadratic approximation. Query NULL/0 sets exact required;
 * caller CAPACITY preserves points, internal budget CAPACITY preserves required
 * too. Other errors preserve outputs. Input/output must be disjoint. */
cui_plot_result cui_plot_cubic_flatten(cui_plot_point start, cui_plot_point control1,
                                       cui_plot_point control2, cui_plot_point end,
                                       double tolerance, cui_plot_point *out, size_t capacity,
                                       size_t *required);
/* Open-polyline stroke, source default Butt caps/Miter joins. Width0 returns
 * EMPTY with required0/bounds0; width>0..1024, miter_limit1..64 (source default4).
 * 2..64 points, offset length/width beyond miter limit bevels the outer edge. Self-overlap,
 * reversal and touching outlines are rejected; closed/dashed/round strokes are
 * not supplied. Successful mesh has ST=(0,1). Same transaction/query contract
 * as fill_mesh; computed coordinates beyond +/-16384 return OVERFLOW. */
cui_plot_result cui_plot_stroke_mesh(const cui_plot_point *, size_t count, double width,
                                     double miter_limit, cui_plot_mesh_vertex *, size_t capacity,
                                     size_t *required, cui_plot_rect *bounds);
/* Extended strokes are a non-overlapping union of segment, join and cap
 * polygons, tessellated by fill_mesh_contours. 2..64 points (closed >=3);
 * repeated closing endpoint accepted. Width0: EMPTY/count0/bounds0. Default
 * Butt/Miter retains the old width/miter policy; the old API remains unchanged.
 * Round circles use round_segments_per_turn (4..64) inscribed chords. Closed
 * strokes ignore caps. MiterClip uses pinned Lyon's clipping distance policy.
 * Generated polygon vertices are bounded to256; exceeding that internal budget
 * returns CAPACITY with required unchanged. Workspace follows fill_mesh_contours
 * (reserve32896 events/256 indices to cover any accepted generated stroke).
 * Output capacity errors publish exact required and preserve mesh/bounds.
 * All other errors preserve output/count/bounds. Inputs/output/workspace must
 * be disjoint. Coordinates within +/-16384, width0..1024, miter_limit1..64;
 * generated coordinates outside range return OVERFLOW. No automatic curve or
 * dash flattening; no exact Lyon topology/variable-width attributes. */
typedef enum { CUI_PLOT_CAP_BUTT, CUI_PLOT_CAP_SQUARE, CUI_PLOT_CAP_ROUND } cui_plot_line_cap;
typedef enum {
    CUI_PLOT_JOIN_MITER,
    CUI_PLOT_JOIN_BEVEL,
    CUI_PLOT_JOIN_ROUND,
    CUI_PLOT_JOIN_MITER_CLIP
} cui_plot_line_join;
typedef struct {
    double width, miter_limit;
    cui_plot_line_cap start_cap, end_cap;
    cui_plot_line_join join;
    size_t round_segments_per_turn;
    bool closed;
} cui_plot_stroke_options;
cui_plot_stroke_options cui_plot_stroke_defaults(void);
cui_plot_result cui_plot_stroke_mesh_ex(const cui_plot_point *, size_t count,
                                        cui_plot_stroke_options, cui_plot_fill_workspace *,
                                        cui_plot_mesh_vertex *, size_t capacity, size_t *required,
                                        cui_plot_rect *bounds);
/* Pinned GPUI dash-array policy for already flattened polylines: alternating
 * draw/gap distances, odd arrays repeated twice, no phase offset. Closed paths
 * include their closing segment; each drawn run has the selected start/end
 * caps, including runs at the closed seam (no automatic seam merge). Patterns
 * have1..64 finite strictly positive entries; zero/negative entries are rejected
 * to prevent the source's nonadvancing measurement loop. Maximum1024 splitting
 * steps/64 points per run/256 generated primitive vertices; internal CAPACITY
 * preserves required. Nonadvancing positive distances return DEGENERATE.
 * Other contracts match stroke_mesh_ex. Lengths are exact linear-segment lengths,
 * not source curved-path measurements at tolerance0.01. */
cui_plot_result cui_plot_stroke_mesh_dashed(const cui_plot_point *, size_t count,
                                            cui_plot_stroke_options, const double *dashes,
                                            size_t dash_count, cui_plot_fill_workspace *,
                                            cui_plot_mesh_vertex *, size_t capacity,
                                            size_t *required, cui_plot_rect *bounds);
/* Pinned path fragment coverage: ST and its screen-space derivatives; output
 * preserved on invalid/overflow. Constant S derivatives (<.001 norm) are solid.
 * This is analytic quadratic coverage, not generic triangle-edge antialiasing. */
cui_plot_result cui_plot_mesh_coverage(cui_plot_point st, cui_plot_point dstdx,
                                       cui_plot_point dstdy, double *coverage);
#ifdef __cplusplus
}
#endif
#endif
