/* SPDX-License-Identifier: MIT */
#ifndef CUI_PLOT_VIEW_H
#define CUI_PLOT_VIEW_H
#include "cui.h"
#include "cui_plot.h"
#include "cui_plot_motion.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Source Text/PlotLabel: caller-owned labels relative to this fixed surface.
 * Size zero selects10px; explicit colors (including transparent) are preserved.
 * Source font_weight is ignored by PlotLabel.paint, so this contract uses normal.
 * Borrowed text remains valid through renderer completion. <=256 labels. */
typedef struct {
    cui_str text;
    cui_plot_point origin;
    float font_size;
    cui_color color;
    cui_align align;
} cui_plot_label;
cui_plot_result cui_plot_labels_layout(cui_context *, const cui_plot_label *, size_t count,
                                       cui_rect *, size_t capacity, size_t *required);
cui_plot_result cui_plot_labels(cui_context *, cui_id, cui_size extent, const cui_plot_label *,
                                size_t count);
/* Copy source-style measured UTF-8 truncation into caller storage, NUL terminated.
 * <=4096 input bytes, strict UTF-8, zero size defaults10. max_width<=0 keeps text;
 * when truncated, returns at least ellipsis even if that exceeds the budget.
 * required includes NUL; errors preserve output, CAPACITY sets required.
 * Input may alias output. No grapheme clustering/locale or automatic layout. */
cui_plot_result cui_plot_truncate_text(cui_context *, cui_str, float font_size, float max_width,
                                       char *output, size_t capacity, size_t *required);
#define CUI_PLOT_AXIS_LIMIT 256u
/* Borrowed labels; font size zero selects 10px. START/CENTER/END alignment is
 * applied about tick (source left/center/right). Alpha zero uses theme muted
 * unless axis options select explicit_colors. */
typedef struct {
    cui_str text;
    float tick, font_size;
    cui_align align;
    cui_color color;
} cui_plot_axis_text;
typedef enum { CUI_PLOT_AXIS_END, CUI_PLOT_AXIS_START } cui_plot_axis_side;
typedef struct {
    float width, height, x, y;
    bool has_x, has_y, x_line, y_line;
    cui_plot_axis_side x_side, y_side;
    const cui_plot_axis_text *x_labels, *y_labels;
    size_t x_count, y_count;
    cui_color stroke;
    bool source_offsets, explicit_colors;
} cui_plot_axis_options;
typedef struct {
    cui_rect bounds;
    size_t index;
    bool horizontal;
} cui_plot_axis_label_geometry;
/* Default x line shown, y line hidden; axes absent until has_x/has_y is set. */
cui_plot_axis_options cui_plot_axis_defaults(float width, float height);
/* Frozen base paint contract: fixed10px-derived offsets at every font size,
 * explicit colors (including alpha zero) rather than theme fallback. */
cui_plot_axis_options cui_plot_axis_source_defaults(float width, float height);
float cui_plot_axis_gutter(float font_size);
/* At default 10px, source offsets match; larger fonts scale the upper gap
 * and vertical centering: x end +6/start -(font+4), y end +2/start -2, vertically
 * centered by font/2. source_offsets instead uses fixed x start=-14/y center=-5
 * at every font size. Geometry uses Core measurement and is transactional. */
cui_plot_result cui_plot_axis_layout(cui_context *, const cui_plot_axis_options *,
                                     cui_plot_axis_label_geometry *, size_t capacity,
                                     size_t *required);
/* Submits a fixed-size axis surface with theme-resolved lines/text. */
cui_plot_result cui_plot_axis(cui_context *, cui_id, const cui_theme *,
                              const cui_plot_axis_options *);
/* Caller storage for scale-generated numeric tick labels; remains live through
 * end_frame. No formatter callback, locales, category labels or auto-gutters. */
typedef struct {
    char text[32];
    double value;
    cui_plot_axis_text label;
} cui_plot_numeric_tick;
cui_plot_result cui_plot_axis_ticks(const cui_plot_scale *, size_t desired, cui_plot_numeric_tick *,
                                    size_t capacity, size_t *required);
typedef struct {
    const cui_plot_path_command *commands;
    size_t count;
    cui_color fill, stroke;
    float stroke_width;
} cui_plot_shape;
/* Portable cached tessellation, not renderer-native stroke triangles. */
typedef struct {
    cui_plot_line line;
    bool closed;
} cui_plot_path_segment;
typedef struct {
    cui_plot_path_segment *storage;
    size_t count, capacity;
    uint64_t key;
    bool warm;
} cui_plot_path_cache;
typedef struct {
    const cui_plot_path_segment *segments;
    size_t count;
    cui_plot_point origin;
} cui_plot_cached_path;
typedef cui_plot_result (*cui_plot_path_build_fn)(void *user, cui_plot_shape *relative_shape);
/* Caller storage <=4096 segments. Build once on changed key, never on hit;
 * callback returns relative commands (borrowed until get returns), or EMPTY.
 * EMPTY is cached and warm, returning EMPTY with an empty output path.
 * Colors/stroke width in built shape are ignored; key must include every
 * geometry-affecting input. Callback must not mutate cache/storage. Invalid,
 * capacity and overflow errors preserve cache/storage/output. Origin translates
 * geometry at paint time; output borrows storage until next successful miss. */
cui_plot_result cui_plot_path_cache_init(cui_plot_path_cache *, cui_plot_path_segment *,
                                         size_t capacity);
bool cui_plot_path_cache_is_warm(const cui_plot_path_cache *);
cui_plot_result cui_plot_path_cache_get(cui_plot_path_cache *, uint64_t key, cui_plot_point origin,
                                        cui_plot_path_build_fn, void *user, cui_plot_cached_path *);
typedef struct {
    cui_plot_cached_path path;
    cui_color fill, stroke;
    float stroke_width;
} cui_plot_cached_shape;
/* Same raster bounds/style/sampling as cui_plot_raster. Reuses cached segments
 * without command rebuilding or cubic flattening; pixels are still rasterized.
 * Cache/pixel storage must be disjoint and stable throughout the call. */
cui_plot_result cui_plot_raster_cached(unsigned char *pixels, unsigned width, unsigned height,
                                       size_t stride, size_t capacity_bytes,
                                       const cui_plot_cached_shape *, size_t count,
                                       cui_color clear);
/* Portable source Background draw routes: solid, linear, slash/checkerboard.
 * Native renderer triangles and exact GPU/Hsla conversion remain separate.
 * Angle0 points up,90 right; caller angle is never changed for bar orientation. */
typedef enum {
    CUI_PLOT_BACKGROUND_SOLID,
    CUI_PLOT_BACKGROUND_LINEAR,
    CUI_PLOT_BACKGROUND_SLASH,
    CUI_PLOT_BACKGROUND_CHECKERBOARD
} cui_plot_background_kind;
typedef enum { CUI_PLOT_COLOR_SRGB, CUI_PLOT_COLOR_OKLAB } cui_plot_color_space;
typedef struct {
    cui_color color;
    double position;
} cui_plot_color_stop;
typedef struct {
    cui_plot_background_kind kind;
    cui_color solid;
    double angle;
    cui_plot_color_stop from, to;
    cui_plot_color_space color_space;
    bool source_projection; /* source short-axis scaling/dominant-axis gradient */
    double pattern_width, pattern_interval, pattern_size;
} cui_plot_background;
cui_plot_background cui_plot_solid_background(cui_color);
cui_plot_background cui_plot_linear_background(double angle, cui_plot_color_stop from,
                                               cui_plot_color_stop to);
/* Source gradient projection, preserving old diagonal constructor behavior. */
cui_plot_background cui_plot_linear_background_source(double angle, cui_plot_color_stop from,
                                                      cui_plot_color_stop to, cui_plot_color_space);
/* Source f32 1/255 slash packing, checked before sampling. Width/interval finite
 * nonnegative, packed u32 fits and interval*255<65535; decoded period must be>0.
 * Checker size finite1/255..16384. Patterns anchor at frame origin, parameters in
 * raster pixel units (no implicit device scale/offset). Slash has source1px AA. */
cui_plot_background cui_plot_pattern_slash(cui_color, double width, double interval);
cui_plot_background cui_plot_checkerboard(cui_color, double size);
/* Explicit positive finite relative frame/position within +/-16384. Stops0..1
 * may be reversed; equal stops make a right-continuous hard edge. Projection
 * clamps beyond stops. sRGB interpolates straight encoded RGBA8; Oklab decodes
 * sRGB, mixes standard Lab/alpha and gamut-clamps encoded output. Source pattern
 * phase/coverage rules are retained, not byte-identical GPU/Hsla output. */
cui_plot_result cui_plot_background_sample(const cui_plot_background *, cui_plot_rect frame,
                                           cui_plot_point position, cui_color *out);
typedef struct {
    cui_plot_cached_path path;
    cui_plot_rect background_frame; /* origin-relative; paint colors share this frame */
    cui_plot_background fill, stroke;
    float stroke_width;
} cui_plot_background_shape;
/* Cached geometry with per-pixel fill/stroke background. Same raster budgets,
 * sampling and transaction/lifetime contract as raster_cached. Origin translates
 * frame+geometry together; changing paint backgrounds never tessellates again. */
cui_plot_result cui_plot_raster_backgrounds(unsigned char *pixels, unsigned width, unsigned height,
                                            size_t stride, size_t capacity_bytes,
                                            const cui_plot_background_shape *, size_t count,
                                            cui_color clear);
/* Shader-source color seam, distinct from encoded cui_color bytes. Hsla inputs
 * are normalized0..1; source conversion interprets their RGB as linear. */
typedef struct {
    float h, s, l, a;
} cui_plot_hsla;
typedef struct {
    float r, g, b, a;
} cui_plot_linear_rgba;
typedef struct {
    cui_plot_hsla color;
    double position;
} cui_plot_hsla_stop;
typedef struct {
    cui_plot_background_kind kind;
    cui_plot_color_space color_space;
    cui_plot_hsla solid;
    double angle;
    cui_plot_hsla_stop from, to;
    double pattern_width, pattern_interval, pattern_size;
} cui_plot_hsla_background;
/* Source short-axis projection, pattern rules and checked bounds as above.
 * Coverage0..1 multiplies alpha. Output is linear premultiplied RGB, preserving
 * shader out-of-gamut values without byte encoding/clamping. Not bit-identical
 * GPU floating arithmetic. Invalid inputs preserve output. */
cui_plot_result cui_plot_hsla_background_sample(const cui_plot_hsla_background *,
                                                cui_plot_rect frame, cui_plot_point position,
                                                double coverage, cui_plot_linear_rgba *out);
typedef struct {
    cui_plot_cached_path path;
    cui_plot_rect background_frame;
    cui_plot_hsla_background fill, stroke;
    float stroke_width;
} cui_plot_hsla_shape;
/* Caller linear premultiplied float pixels, stride/capacity in RGBA pixels.
 * Same geometry/work bounds as raster_backgrounds, fill then stroke source-over.
 * Clear is already premultiplied, finite RGB0..alpha and alpha0..1. No encoding,
 * display transfer, byte rounding, retained rendering or triangle-edge AA.
 * Buffers stable/disjoint; errors preserve all pixels. */
cui_plot_result cui_plot_raster_hsla(cui_plot_linear_rgba *pixels, unsigned width, unsigned height,
                                     size_t stride, size_t capacity, const cui_plot_hsla_shape *,
                                     size_t count, cui_plot_linear_rgba clear);
/* Caller RGBA8 storage, straight alpha. Rasterizer accepts Move/Line/Cubic/Close;
 * use arc_flatten for ARC commands. Cubics use 16 uniform parameter chords.
 * Each shape <=4096 flattened segments, <=64 shapes, dimensions1..1024,
 * storage requires stride*height bytes; total pixel*segment work <=64 million. Even-odd fill of
 * closed runs, round segment strokes, pixel-center sampling, no anti-aliasing. Disjoint stable
 * buffers required; invalid/capacity errors preserve all pixels. Coordinates finite and within
 * +/-16384; shape colors are explicit. */
cui_plot_result cui_plot_raster(unsigned char *pixels, unsigned width, unsigned height,
                                size_t stride, size_t capacity_bytes, const cui_plot_shape *,
                                size_t count, cui_color clear);
/* Fixed-size themed Grid, borrowing pure geometry options for this call.
 * Alpha-zero stroke selects theme border. Preflight failures submit nothing. */
cui_plot_result cui_plot_grid(cui_context *, cui_id, const cui_theme *,
                              const cui_plot_grid_options *, cui_color stroke);
/* explicit_color=true preserves source stroke verbatim, including transparent;
 * false retains legacy alpha-zero theme fallback. Geometry unchanged. */
cui_plot_result cui_plot_grid_ex(cui_context *, cui_id, const cui_theme *,
                                 const cui_plot_grid_options *, cui_color stroke,
                                 bool explicit_color);
typedef struct {
    double size;
    cui_color fill, stroke;
    bool use_theme_fill, custom_stroke;
} cui_plot_dot_options;
/* Styled default size4, theme primary fill; source base itself defaults to
 * transparent fill. Without custom_stroke the border uses resolved solid fill. */
cui_plot_dot_options cui_plot_dot_defaults(void);
/* Fixed extent; skips undefined samples and paints markers after preflight.
 * Cartesian uses x/y1. Radial uses angle/radius1 around extent center.
 * <=4096 samples, size0..1024, coordinates within +/-16384. No dot semantics
 * or hit model; stable borrowed arrays. Pure validation errors submit nothing. */
cui_plot_result cui_plot_dots(cui_context *, cui_id, const cui_theme *, cui_size extent,
                              const cui_plot_sample *, size_t count, cui_plot_dot_options);
cui_plot_result cui_plot_radial_dots(cui_context *, cui_id, const cui_theme *, cui_size extent,
                                     const cui_plot_radial_sample *, size_t count,
                                     cui_plot_dot_options);
typedef struct {
    double cross, baseline, value;
    bool has_cross, has_value;
} cui_plot_bar_sample;
typedef cui_color (*cui_plot_bar_fill_fn)(const cui_plot_bar_sample *, size_t index,
                                          cui_plot_rect frame, cui_plot_bar_alignment, void *user);
typedef struct {
    cui_plot_bar_alignment alignment;
    double band_width;
    cui_color fill;
    bool use_theme_fill;
    cui_corner_radii corners;
    cui_plot_bar_fill_fn fill_for;
    void *user;
} cui_plot_bar_options;
/* Styled bottom/zero-width/themed solid fill defaults; caller sets width. */
cui_plot_bar_options cui_plot_bar_defaults(void);
/* Callback resolves a solid fill synchronously once per eligible datum, with
 * source-relative frame and alignment. It must not submit UI or mutate borrowed
 * samples/options; called during frame composition, not renderer execution.
 * Per-corner rounding uses existing Core renderer clamping. No gradient or
 * generic label-content driver. Missing cross/value skip callback and geometry.
 * Fixed extent, <=4096 samples, band width0..16384, finite coordinates within
 * +/-16384, corner sizes0..1024; preflight failures submit nothing/call nobody. */
cui_plot_result cui_plot_bars(cui_context *, cui_id, const cui_theme *, cui_size extent,
                              const cui_plot_bar_sample *, size_t count,
                              const cui_plot_bar_options *);
typedef void (*cui_plot_activate_fn)(cui_context *, cui_id, cui_plot_point bitmap_position,
                                     void *user);
typedef struct {
    cui_bitmap bitmap;
    const cui_plot_axis_options *axis; /* same pixel extent as bitmap */
    cui_str name;
    bool disabled, interactive;
    cui_plot_activate_fn on_activate;
    void *user;
} cui_plot_view_options;
/* Immediate fixed-size bitmap + axis composition. Bitmap/labels borrowed
 * through renderer completion. Validated primary-pointer release invokes the
 * callback synchronously with bitmap coordinates; no keyboard activation,
 * implicit zoom/pan or per-shape hit testing. */
cui_plot_result cui_plot_view(cui_context *, cui_id, const cui_theme *,
                              const cui_plot_view_options *);
/* Portable PlotElement composition seam. Legacy hosts supply current global
 * bounds/pointer/hover. Explicit current-frame paint uses Core after layout. */
typedef struct {
    cui_id owner;
    cui_plot_cursor_memory cursor;
    cui_plot_hover_memory hover;
    cui_plot_transition_clock transition;
    bool request_frame;
    cui_plot_transition_clock appear_clock;
    cui_plot_appear_value appearance;
    uint64_t generation, last_frame, scope_identity;
    cui_plot_appear_registry *scope;
    bool has_frame, has_generation;
    cui_rect current_bounds; /* last successful current-frame paint; activation coordinates */
} cui_plot_element_state;
typedef cui_plot_result (*cui_plot_resolve_fn)(cui_plot_point relative_cursor,
                                               cui_plot_rect current_bounds,
                                               cui_plot_tooltip_state *out, void *user);
typedef void (*cui_plot_compose_fn)(cui_context *, cui_id owner, const cui_theme *,
                                    const cui_plot_hover_memory *hover,
                                    cui_plot_rect current_bounds, void *user);
/* Immutable current-frame geometry; writer is callback-local. No layout/input
 * mutations. Return false to discard emitted output. User remains borrowed. */
typedef bool (*cui_plot_paint_fn)(const cui_prepaint_info *, cui_prepaint_writer *,
                                  const cui_theme *, const cui_plot_hover_memory *, void *user);
typedef void (*cui_plot_appear_fn)(cui_id owner, const cui_plot_appear_value *, void *user);
typedef struct {
    cui_plot_rect bounds;
    cui_plot_point pointer;
    bool hovered, appearing, touch;
    cui_plot_transition_policy enter, exit;
    cui_plot_resolve_fn resolve;
    cui_plot_compose_fn marks, children, tooltip;
    void *user;
    int16_t tooltip_layer; /* zero selects1, otherwise must be positive */
    /* Optional first-paint appearance. Host increments serial every frame,
     * including frames omitting this plot. Pass active innermost registry
     * begun with the same frame serial. */
    bool has_appearance;
    uint64_t generation, frame_serial;
    cui_plot_transition_policy appear;
    cui_plot_appear_registry *appear_scope;
    cui_plot_appear_fn on_appear; /* synchronous, no UI; may prepare bitmap/marks */
    /* Opt-in replaces enter/exit/appear. Published appearance clears temporary
     * sampler pointers; use motion_appear_stagger with explicit policy later. */
    bool custom_motion;
    cui_plot_motion_policy motion; /* pointer policy is used by host marks via follow */
    /* AUTO preserves explicit host geometry and layout callbacks. New paint-only
     * zero-bounds options select Core. CURRENT_FRAME rejects layout callbacks and
     * touch; legacy touch hosts retain their explicit cursor contract. */
    cui_plot_interaction_mode interaction_mode;
    cui_plot_paint_fn paint_marks, paint_tooltip;
    size_t paint_command_budget; /* combined custom commands1..4096 when callbacks set */
} cui_plot_element_options;
/* Zero-init state. Owner change resets it. Resolver is synchronous preflight:
 * OK supplies relative datum/dots, EMPTY supplies none; it must not compose UI
 * or mutate state/options. Dots stay borrowed until replacement/exit completes.
 * Uses Core frame time/reduced-motion; marks then ordinary children are composed
 * inside the plot after bitmap/axes, tooltip last in an inert global portal with
 * plot extent and hover opacity. Tooltip callback places its body relative to
 * that portal and may extend outside it. Text/bitmap payloads stay live through
 * renderer completion. Callbacks balance boxes and must not mutate driver state.
 * Primary activation uses existing view routing. Touch hosts update state.cursor
 * through cursor_long_press before composition; touch=true preserves that gate.
 * Driver requests redraw for clocks; host advances frames. Disabled/passive
 * plots reset hover, suppress resolver/tooltip and still compose children.
 * Appearance runs even disabled/passive, notifies on_appear during preflight
 * before resolver/bitmap paint (sample pointer lives only for that callback),
 * suppresses datum resolution while its LINEAR time<1 and combines redraw flags.
 * Generation/rekey/frame gap restarts; active scope remembers only completed
 * generations. Default appear duration0/reduced motion complete immediately.
 * CURRENT_FRAME borrows state/data/user until end_frame/renderer completion;
 * paint callbacks run after layout with immutable occlusion/clip information.
 * They emit local/deferred commands explicitly; apply hover.progress themselves.
 * User side effects and bitmap mutations are outside output rollback. No native
 * mesh cache or OS long-press detector. */
cui_plot_result cui_plot_element(cui_context *, cui_id, const cui_theme *,
                                 const cui_plot_view_options *, cui_plot_element_state *,
                                 const cui_plot_element_options *);
#ifdef __cplusplus
}
#endif
#endif
