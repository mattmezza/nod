/* SPDX-License-Identifier: MIT */
#ifndef CUI_THEME_REGISTRY_H
#define CUI_THEME_REGISTRY_H
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
#define CUI_THEME_NAME_CAPACITY 128
#define CUI_THEME_SHADOW_CAPACITY 8
#define CUI_THEME_REGISTRY_CAPACITY 256
#define CUI_THEME_JSON_MAX_BYTES 65536
/* Fixed owned snapshots: no imported string/pixel pointers are retained. */
typedef struct {
    cui_color background, foreground, surface, surface_foreground, primary, primary_foreground,
        secondary, secondary_foreground, muted, muted_foreground, accent, accent_foreground,
        destructive, destructive_foreground, border, input, ring, selection;
} cui_theme_semantic_colors;
typedef struct {
    float none, sm, md, lg, xl, full;
} cui_theme_semantic_radius;
typedef struct {
    float xxs, xs, sm, md, lg, xl, xxl;
} cui_theme_semantic_spacing;
typedef struct {
    float size, line_height, weight;
} cui_theme_text_token;
typedef struct {
    char sans[CUI_THEME_NAME_CAPACITY], mono[CUI_THEME_NAME_CAPACITY];
    cui_theme_text_token xs, sm, md, lg, xl, mono_md;
} cui_theme_semantic_typography;
typedef struct {
    cui_color color;
    float offset_x, offset_y, blur_radius, spread_radius;
    bool inset;
} cui_theme_shadow;
typedef struct {
    cui_theme_shadow values[CUI_THEME_SHADOW_CAPACITY];
    size_t count;
} cui_theme_shadows;
typedef struct {
    cui_theme_shadows sm, md, lg;
} cui_theme_semantic_shadow;
typedef struct {
    cui_theme_semantic_colors colors;
    cui_theme_semantic_radius radius;
    cui_theme_semantic_spacing spacing;
    cui_theme_semantic_typography typography;
    cui_theme_semantic_shadow shadow;
} cui_theme_semantic_tokens;
typedef enum {
    CUI_THEME_REGISTRY_OK,
    CUI_THEME_REGISTRY_INVALID,
    CUI_THEME_REGISTRY_CAPACITY_ERROR,
    CUI_THEME_REGISTRY_UNSUPPORTED,
    CUI_THEME_REGISTRY_MISSING
} cui_theme_registry_error;
typedef struct {
    cui_theme_registry_error error;
    size_t byte_offset, added, replaced, skipped, unsupported_fields;
    bool partial;
} cui_theme_import_result;
bool cui_theme_semantic_snapshot(const cui_theme *, cui_theme_semantic_tokens *);
/* Generic projection only: specialized button/component colors stay unchanged.
 * Fonts, shadow arrays and scale-only tokens remain available in the snapshot. */
bool cui_theme_semantic_apply(cui_theme *, const cui_theme_semantic_tokens *);
/* Exact standalone source wrapper {"tokens":{...}}, not legacy ThemeSet JSON.
 * Strict validation rejects unknown keys and invalid source values atomically. */
cui_theme_registry_error cui_theme_semantic_import(cui_str json, const cui_theme *,
                                                   cui_theme_semantic_tokens *, cui_theme *,
                                                   cui_theme_import_result *);
bool cui_theme_color_parse(cui_str, cui_color *);
typedef struct {
    char name[CUI_THEME_NAME_CAPACITY];
    bool is_default;
    cui_theme theme;
    cui_theme_semantic_tokens semantic;
    cui_theme_motion_tokens motion;
} cui_theme_registry_entry;
typedef struct {
    cui_theme_registry_entry *entries;
    size_t count, capacity;
    uint64_t revision;
    bool seed_presets;
} cui_theme_registry;
typedef struct {
    bool replace_existing, allow_unsupported;
} cui_theme_import_options;
/* Caller arrays must outlive registry; no allocations. Reset requires capacity
 * for all 38 presets when seeded. Entry copies own names/fonts/shadows; theme
 * icon callbacks/userdata remain borrowed from their original host. */
bool cui_theme_registry_init(cui_theme_registry *, cui_theme_registry_entry *, size_t capacity,
                             bool seed_presets);
bool cui_theme_registry_reset(cui_theme_registry *);
const cui_theme_registry_entry *cui_theme_registry_find(const cui_theme_registry *, cui_str name);
/* Sorted index: defaults first, light before dark, then ASCII-folded UTF8 byte
 * order (not Unicode lowercasing). Borrow valid until registry mutation/reset. */
const cui_theme_registry_entry *cui_theme_registry_sorted(const cui_theme_registry *, size_t index);
/* Put treats .theme as authoritative for shared generic colors, md/lg radius,
 * and md font size; config-only semantic fields (including selection) survive.
 * Use semantic import/projector, or update both snapshots, for semantic edits. */
cui_theme_registry_error cui_theme_registry_put(cui_theme_registry *,
                                                const cui_theme_registry_entry *,
                                                bool replace_existing);
/* Transactional legacy ThemeSet import. Scratch, live entries and JSON bytes
 * must be mutually disjoint; report must not overlap these or registry metadata.
 * Scratch must fit the complete resulting registry; never retain its contents.
 * Source skip-existing policy is default; unknown/unrepresented fields require
 * allow_unsupported and return an observable partial report. */
cui_theme_registry_error cui_theme_registry_import(cui_theme_registry *, cui_str,
                                                   cui_theme_import_options,
                                                   cui_theme_registry_entry *scratch,
                                                   size_t scratch_capacity,
                                                   cui_theme_import_result *);
typedef enum {
    CUI_THEME_FOLLOW_SYSTEM,
    CUI_THEME_FORCE_LIGHT,
    CUI_THEME_FORCE_DARK
} cui_theme_mode;
typedef struct {
    bool dark, reduced_motion, high_contrast, auto_hide_scrollbars;
    uint64_t revision; /* Host watcher generation, not a Core frame clock. */
} cui_theme_host_preferences;
typedef struct {
    cui_theme_mode mode;
    cui_str light_name, dark_name;
} cui_theme_selection;
typedef struct {
    bool ready;
    uint64_t registry_revision;
    cui_theme_host_preferences preferences;
    cui_theme theme;
    cui_theme_semantic_tokens semantic;
    cui_theme_motion_tokens motion; /* Distances projected to pixels. */
    bool changed, metrics_changed, motion_changed;
} cui_theme_host_state;
/* Pure host event seam. High contrast explicitly chooses CUI's existing high
 * contrast palette (a CUI policy extension), not automatic platform detection.
 * Caller performs invalidation/metrics/Core preference wiring using flags. */
cui_theme_registry_error cui_theme_host_update(cui_theme_host_state *, const cui_theme_registry *,
                                               cui_theme_selection, cui_theme_host_preferences,
                                               float rem_pixels);
#ifdef __cplusplus
}
#endif
#endif
