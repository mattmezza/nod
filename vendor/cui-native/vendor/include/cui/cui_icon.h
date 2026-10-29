/* SPDX-License-Identifier: MIT */
#ifndef CUI_ICON_H
#define CUI_ICON_H
#include "cui/cui.h"
#include "cui/cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_ICON_OK,
    CUI_ICON_INVALID,
    CUI_ICON_NOT_FOUND,
    CUI_ICON_PARSE,
    CUI_ICON_LIMIT,
    CUI_ICON_ALLOCATION
} cui_icon_error;
typedef enum {
    CUI_ICON_INHERITED,
    CUI_ICON_XSMALL,
    CUI_ICON_SMALL,
    CUI_ICON_MEDIUM,
    CUI_ICON_LARGE,
    CUI_ICON_CUSTOM
} cui_icon_size;
/* Center-relative affine: x'=a*x+c*y+tx; y'=b*x+d*y+ty. */
typedef struct {
    float a, b, c, d, tx, ty;
} cui_icon_transform;
cui_icon_transform cui_icon_identity(void);
cui_icon_transform cui_icon_rotation(float radians);
typedef struct {
    size_t max_svg_bytes, max_pixels, max_cache_bytes, max_versions;
} cui_icon_limits;
/* Returned bytes must remain valid until prepare returns; they are then parsed
 * from an owned copy. False tries the embedded bundle. No implicit file/network I/O. */
typedef bool (*cui_icon_loader_fn)(void *user, cui_str path, cui_str *svg);
/* Caller-owned handle; do not copy an initialized state. Private storage contains
 * immutable masks, so replacing a source/size never invalidates earlier draws. */
typedef struct {
    void *internal;
    cui_icon_error error;
    size_t parse_count, raster_count, cache_bytes, cache_versions;
} cui_icon_state;
void cui_icon_state_init(cui_icon_state *);
void cui_icon_state_destroy(cui_icon_state *);
/* Call only after all previously returned draw lists have finished rendering. */
void cui_icon_state_collect(cui_icon_state *);
bool cui_icon_set_limits(cui_icon_state *, cui_icon_limits);
cui_icon_limits cui_icon_default_limits(void);
bool cui_icon_set_data(cui_icon_state *, cui_str svg);
bool cui_icon_set_path(cui_icon_state *, cui_str path, cui_icon_loader_fn, void *user);
bool cui_icon_set_name(cui_icon_state *, cui_str name, cui_icon_loader_fn, void *user);
bool cui_icon_set_empty(cui_icon_state *);
void cui_icon_reload(cui_icon_state *);
/* Complete catalog is always present; only 126 defaults embed SVG unless
 * CUI_ICON_ALL_ASSETS=1. Unknown catalog index returns an empty string. */
size_t cui_icon_catalog_count(void);
cui_str cui_icon_catalog_name(size_t);
cui_str cui_icon_catalog_path(size_t);
bool cui_icon_catalog_default(size_t);
typedef enum {
    CUI_ICON_UNKNOWN,
    CUI_ICON_CATALOG_NOT_EMBEDDED,
    CUI_ICON_EMBEDDED
} cui_icon_availability;
/* Build-local availability; recognizes the same names, paths and aliases as
 * cui_icon_asset. A custom loader may still provide a nonembedded asset. */
cui_icon_availability cui_icon_query(cui_str name_or_path);
bool cui_icon_asset(cui_str name_or_path, cui_str *svg);
/* Portable headless preparation. Width scales SVG aspect; height positions it
 * around the layout center. Transform changes paint only, including overflow. */
cui_icon_error cui_icon_prepare(cui_icon_state *, float width, float height, cui_icon_transform);
cui_bitmap cui_icon_state_bitmap(const cui_icon_state *);
cui_rect cui_icon_state_paint_bounds(const cui_icon_state *);
/* Numeric border-box constraints. Explicit maxima include zero. Minima win
 * over maxima; padding+border sums finally floor each resolved dimension. */
typedef struct {
    float min_width, min_height, max_width, max_height;
    bool custom_min_width, custom_min_height, custom_max_width, custom_max_height;
} cui_icon_constraints;
typedef struct {
    cui_icon_size size;
    float custom_size, inherited_text_size;
    float width, height;
    bool custom_width, custom_height; /* inherited-size style dimensions, including zero */
    cui_color color, inherited_color;
    bool custom_color, custom_inherited_color;
    float opacity;
    bool custom_opacity;
    cui_icon_transform transform;
    bool custom_transform;
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
    /* Zero preserves legacy preset scaling by theme font size. Positive rem
     * separates explicit preset sizes from inherited text size, as in source. */
    float rem_size;
    /* Default is decorative. A nonempty name opts into a noninteractive image
     * semantic; decorative=true suppresses it even when a name is supplied.
     * Names are borrowed for the frame and never inferred from path/SVG data. */
    cui_str accessible_name;
    bool decorative;
    /* Optional layout decoration. width/height and sizing flags remain governed
     * by options above; uniform/per-edge padding and positive maximum caps apply.
     * SVG paints the complete resolved box, without a padding inset. Other box
     * paint/clip/inert/opacity fields apply; generic flex/percentage sizing does not. */
    const cui_box *style;
    /* Opt-in outline inside the allocated box on NOT_FOUND. Keeps that error
     * result and the existing decorative/named semantic contract. */
    bool debug_placeholder;
    /* Optional synchronous numeric constraints; copied values are not retained.
     * Explicit descriptor maxima replace corresponding legacy style maxima. */
    const cui_icon_constraints *constraints;
} cui_icon_options;
/* Decorative by default, or named noninteractive image; never a focus stop. Theme supplies fallback
 * size/color; explicit inherited overrides bridge hosts with their own text scopes. */
cui_icon_error cui_icon(cui_context *, cui_id, const cui_theme *, cui_icon_state *,
                        cui_icon_options);
/* Context-owned convenience cache; accepts catalog names or paths. Its masks
 * expire at the next begin_frame, matching the core draw-list borrow lifetime. */
cui_icon_error cui_icon_named(cui_context *, cui_id, const cui_theme *, cui_str name,
                              cui_icon_options);
/* Source Loader, .8-second quadratic rotation; zero options use Medium size.
 * Reduced motion paints a static icon and does not request animation frames. */
cui_icon_error cui_icon_spinner(cui_context *, cui_id, const cui_theme *, cui_icon_options);
#ifdef __cplusplus
}
#endif
#endif
