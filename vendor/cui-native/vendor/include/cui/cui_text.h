/* SPDX-License-Identifier: MIT */
#ifndef CUI_TEXT_H
#define CUI_TEXT_H
#include "cui.h"
#include "cui_bidi.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct cui_font cui_font;
typedef void (*cui_glyph_fn)(void *user, int x, int y, int width, int height, int pitch,
                             const unsigned char *coverage);
/* The font file remains caller-owned. Face operations are UI-thread confined.
 */
cui_font *cui_font_open(const char *path);
/* Open face zero from borrowed, immutable font bytes without copying or file I/O.
 * data must remain valid until the face is actually released, including layouts
 * retaining it after font_close. Static embedded arrays satisfy this lifetime.
 * NULL, empty, oversized or invalid font data returns NULL. UI-thread confined. */
cui_font *cui_font_open_memory(const void *data, size_t size);
/* Release the caller/owner reference; live layouts may retain the actual face.
 * A closed caller handle must not be used again. NULL is accepted. */
void cui_font_close(cui_font *font);
/* Borrow an actual face opened by the caller; pass NULL to remove a variant.
 * Keep variants alive through measurement/drawing. Registrations are keyed by
 * family ID, weight, italic and monospace. Nonzero families may register a
 * regular face; family zero's regular face is the one passed to font_open.
 * Fallback: exact family/style, named family regular, default family/style,
 * then the opened regular face. A family-only registration is not an OS name
 * lookup. Remove styles independently; clearing regular retains other styles. */
bool cui_font_set_style(cui_font *font, cui_text_style style, cui_font *variant);
/* Convenience path registration owns the opened face. Failure preserves the
 * previous registration; replacement and font_close release owned faces. */
bool cui_font_register_style(cui_font *font, cui_text_style style, const char *path);
enum { CUI_FONT_FALLBACK_MAX = 32 };
/* Explicit, ordered, nonrecursive fallback chain; at most 32 entries.
 * Borrowed fonts must remain alive until chain replacement/clear or owner close.
 * No NULL/self/duplicate entries. Failure preserves the complete previous chain.
 * A zero count clears it. Each entry uses existing style selection rules.
 * Current owned chain entries cannot be reinstalled as borrowed entries. */
bool cui_font_set_fallbacks(cui_font *font, cui_font *const *fallbacks, size_t count);
/* Opens and owns every path transactionally. Replacement/close release owned
 * references, while existing layouts retain their actual faces independently. */
bool cui_font_register_fallbacks(cui_font *font, const char *const *paths, size_t count);
/* Selected-face metrics suitable for cui_config.font_metrics. Failure preserves out. */
bool cui_font_get_metrics(void *font, float size, cui_text_style style, cui_font_metrics *out);
cui_size cui_font_measure(void *font, cui_str text, float size);
/* Capability bits for cui_config.text_capabilities; tracking requires Bidi build. */
uint32_t cui_font_text_capabilities(void);
cui_size cui_font_measure_styled(void *font, cui_str text, float size, cui_text_style style);
/* Optional owned visual session factory for cui_config.text_geometry; shares font user. */
cui_text_layout_status cui_font_text_geometry(void *font, cui_str text, float size,
                                              cui_text_style style, cui_text_geometry *out);
/* Contextual factory queries use original paragraph-relative byte offsets. */
cui_text_layout_status cui_font_text_geometry_line(void *, cui_text_line_source, float,
                                                   cui_text_style, cui_text_geometry *);
/* Zero source retains legacy drawing; contextual ranges retain bidi paragraph analysis. */
bool cui_font_draw_context_line(cui_font *, cui_str, cui_text_line_source, float, cui_text_style,
                                float, float, cui_glyph_fn, void *);
typedef void (*cui_color_glyph_fn)(void *, int, int, int, int, int, const unsigned char *,
                                   cui_color);
/* Colors change glyph coverage only after full-line shaping; cluster-start wins.
 * Requires native bidi support; disabled builds return false without emission. */
bool cui_font_draw_context_line_colored(cui_font *, cui_text_line_source, float, cui_text_style,
                                        float, float, cui_color, const cui_text_color_span *,
                                        size_t, cui_color_glyph_fn, void *);
/* Density-aware paint keeps shaping, metrics and input coordinates in logical units.
 * Callback coordinates and bitmap dimensions are in pixels (logical * scale).
 * scale must be finite, positive and <= 16. Existing draw APIs use scale 1. */
bool cui_font_draw_context_line_scaled(cui_font *, cui_str, cui_text_line_source, float,
                                       cui_text_style, float, float, float, cui_glyph_fn, void *);
bool cui_font_draw_context_line_colored_scaled(cui_font *, cui_text_line_source, float,
                                               cui_text_style, float, float, cui_color,
                                               const cui_text_color_span *, size_t, float,
                                               cui_color_glyph_fn, void *);
bool cui_font_draw(cui_font *font, cui_str text, float size, float x, float y, cui_glyph_fn emit,
                   void *user);
bool cui_font_draw_styled(cui_font *font, cui_str text, float size, cui_text_style style, float x,
                          float y, cui_glyph_fn emit, void *user);
/* Optional bidi/shaping layout (CUI_TEXT_BIDI). When enabled, ordinary
 * measurement/drawing also use paragraph/script analysis and hard-line layout.
 * Disabled builds retain legacy drawing. No platform/shaping types are public. */
typedef struct cui_text_layout cui_text_layout;
typedef struct {
    uint32_t glyph_id;
    size_t cluster_byte;        /* Original paragraph UTF-8 byte offset; clusters may repeat. */
    float x, y;                 /* Positioned baseline-relative origin, y grows down. */
    float x_advance, y_advance; /* Original shaped advances; tracking is in positions/clusters. */
} cui_text_glyph;
typedef struct {
    size_t byte_start, byte_length, glyph_start, glyph_count;
    uint32_t script; /* ISO 15924 four-byte tag, most significant byte first. */
    uint8_t level;
    size_t face_index; /* Index in view.faces; glyph IDs belong to this face. */
} cui_text_run;
/* Shaped cluster advance spans, not ink/grapheme bounds or ligature interiors. */
typedef struct {
    size_t byte_start, byte_end, run_index, glyph_start, glyph_count;
    float left, right;
} cui_text_cluster;
typedef struct {
    const cui_text_glyph *glyphs;
    const cui_text_run *runs;
    size_t glyph_count, run_count;
    float width, height, ascent;
    uint8_t paragraph_level;
    cui_font *const *faces; /* Borrowed layout-retained handles; do not close them. */
    size_t face_count;
    const cui_text_cluster *clusters; /* Visual left-to-right order; zero spans retained. */
    size_t cluster_count, byte_start, byte_end;
} cui_text_layout_view;
/* Analyze a full paragraph, shape one caller-selected scalar line. SIZE_MAX
 * length means through paragraph end. Empty ranges return paragraph metrics.
 * Choose complete grapheme/shaping cluster
 * boundaries. Language is a BCP47 tag or NULL for "und". Layout owns glyph/run
 * data and retains face references; input may be reused after return. Closing
 * fonts or replacing style/fallback registrations does not invalidate a layout.
 * Fallback chooses a complete script/directional subrun; absent coverage returns
 * MISSING_GLYPH, including shaped glyph zero, without publishing a layout.
 * Operations involving fonts are UI-thread confined. Failure preserves *out. */
cui_text_layout_status cui_font_layout_line(cui_font *font, cui_str paragraph, float size,
                                            cui_text_style style, cui_bidi_direction direction,
                                            size_t scalar_start, size_t scalar_length,
                                            const char *language, cui_text_layout **out);
/* View remains valid until destroy; invalid arguments preserve output. */
bool cui_text_layout_get(const cui_text_layout *layout, cui_text_layout_view *out);
void cui_text_layout_destroy(cui_text_layout *layout);
/* Paint positioned glyphs with their selected run faces and saved size. Coordinates
 * specify the line's top-left; partial raster failure may already have emitted. */
bool cui_text_layout_draw_scaled(cui_text_layout *, float, float, float, cui_glyph_fn, void *);
bool cui_text_layout_draw(cui_text_layout *layout, float x, float y, cui_glyph_fn emit, void *user);
/* Cluster-edge foundation, not full editing caret support. Original paragraph
 * byte offsets must be exact shaped-cluster boundaries; interior ligature/mark
 * offsets are INVALID. UPSTREAM attaches to preceding logical cluster's end;
 * DOWNSTREAM to following cluster's start. At bidi transitions these may have
 * distinct x positions. Empty lines have both affinities at x=0/run=SIZE_MAX.
 * NULL output with capacity zero queries required count. INVALID/CAPACITY keep
 * outputs and count unchanged; buffers/count must be disjoint. Carets sorted by
 * x then affinity, zero-width edges retained. Coordinates use the line top-left. */
cui_text_layout_status cui_text_layout_carets(const cui_text_layout *layout, size_t byte_offset,
                                              cui_text_caret *out, size_t capacity, size_t *count);
/* Finite point projected to this line, even outside its bounds. Nearest cluster
 * edge wins; equal distances prefer the requested affinity, then visual cluster
 * order. Does not hit glyph ink or provide vertical navigation. */
cui_text_layout_status cui_text_layout_hit_test(const cui_text_layout *layout, float x, float y,
                                                cui_text_affinity preferred, cui_text_caret *out);
/* Reversed logical endpoints normalize. Both must be cluster boundaries.
 * One rectangle per selected cluster (including zero-width default-ignorables),
 * in visual order; mixed bidi ranges may be disconnected. No ink bounds or
 * partial-ligature selections. NULL/0 queries count; failures are atomic. */
cui_text_layout_status cui_text_layout_selection(const cui_text_layout *layout, size_t byte_start,
                                                 size_t byte_end, cui_text_selection_rect *out,
                                                 size_t capacity, size_t *count);
#ifdef __cplusplus
}
#endif
#endif
