/* SPDX-License-Identifier: MIT */
#ifndef CUI_QUESTIONNAIRE_GEOMETRY_H
#define CUI_QUESTIONNAIRE_GEOMETRY_H
#include "cui_questionnaire_keyboard.h"
#include "cui_questionnaire_presentation.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Value-only skin snapshot. No automatic numeric inheritance. All lengths in
 * logical pixels; natural role line boxes are zero. */
typedef struct {
    float root_gap;
    float item_gap;
    float choices_gap;
    float answer_gap;
    float content_gap;
    float padding_x;
    float padding_y;
    float minimum_height;
    float card_radius;
    float indicator_size;
    float mark_size;
    float check_size;
    float indicator_radius;
    float shortcut_size;
    float shortcut_font;
    float shortcut_line;
    float shortcut_radius;
    float answer_font;
    float answer_line;
    float secondary_font;
    float secondary_line;
    float title_font;
    float title_line;
    float progress_font;
    float progress_line;
} cui_questionnaire_geometry_metrics;
enum {
    CUI_QUESTIONNAIRE_METRIC_ROOT_GAP = 1u << 0,
    CUI_QUESTIONNAIRE_METRIC_ITEM_GAP = 1u << 1,
    CUI_QUESTIONNAIRE_METRIC_CHOICES_GAP = 1u << 2,
    CUI_QUESTIONNAIRE_METRIC_ANSWER_GAP = 1u << 3,
    CUI_QUESTIONNAIRE_METRIC_CONTENT_GAP = 1u << 4,
    CUI_QUESTIONNAIRE_METRIC_PADDING_X = 1u << 5,
    CUI_QUESTIONNAIRE_METRIC_PADDING_Y = 1u << 6,
    CUI_QUESTIONNAIRE_METRIC_MINIMUM_HEIGHT = 1u << 7,
    CUI_QUESTIONNAIRE_METRIC_CARD_RADIUS = 1u << 8,
    CUI_QUESTIONNAIRE_METRIC_INDICATOR_SIZE = 1u << 9,
    CUI_QUESTIONNAIRE_METRIC_MARK_SIZE = 1u << 10,
    CUI_QUESTIONNAIRE_METRIC_CHECK_SIZE = 1u << 11,
    CUI_QUESTIONNAIRE_METRIC_INDICATOR_RADIUS = 1u << 12,
    CUI_QUESTIONNAIRE_METRIC_SHORTCUT_SIZE = 1u << 13,
    CUI_QUESTIONNAIRE_METRIC_SHORTCUT_FONT = 1u << 14,
    CUI_QUESTIONNAIRE_METRIC_SHORTCUT_LINE = 1u << 15,
    CUI_QUESTIONNAIRE_METRIC_SHORTCUT_RADIUS = 1u << 16,
    CUI_QUESTIONNAIRE_METRIC_ANSWER_FONT = 1u << 17,
    CUI_QUESTIONNAIRE_METRIC_ANSWER_LINE = 1u << 18,
    CUI_QUESTIONNAIRE_METRIC_SECONDARY_FONT = 1u << 19,
    CUI_QUESTIONNAIRE_METRIC_SECONDARY_LINE = 1u << 20,
    CUI_QUESTIONNAIRE_METRIC_TITLE_FONT = 1u << 21,
    CUI_QUESTIONNAIRE_METRIC_TITLE_LINE = 1u << 22,
    CUI_QUESTIONNAIRE_METRIC_PROGRESS_FONT = 1u << 23,
    CUI_QUESTIONNAIRE_METRIC_PROGRESS_LINE = 1u << 24
};
enum {
    CUI_QUESTIONNAIRE_BOX_WIDTH = 1u << 0,
    CUI_QUESTIONNAIRE_BOX_PADDING = 1u << 1,
    CUI_QUESTIONNAIRE_BOX_GAP = 1u << 2,
    CUI_QUESTIONNAIRE_BOX_RADIUS = 1u << 3,
    CUI_QUESTIONNAIRE_BOX_BACKGROUND = 1u << 4,
    CUI_QUESTIONNAIRE_BOX_BORDER = 1u << 5,
    CUI_QUESTIONNAIRE_BOX_ALIGN = 1u << 6,
    CUI_QUESTIONNAIRE_BOX_OPACITY = 1u << 7
};
typedef struct {
    uint32_t fields;
    float width; /* -1 grow, 0 FIT; fixed_zero_width changes zero to fixed zero. */
    bool fixed_zero_width;
    cui_border_edges padding;
    float gap, radius, border_width, opacity;
    cui_color background, border;
    cui_align align_x, align_y;
} cui_questionnaire_box_refinement;
typedef struct {
    cui_questionnaire_choice_view legacy;
    cui_questionnaire_geometry_metrics metrics;
    cui_questionnaire_text_view answer, secondary, shortcut;
} cui_questionnaire_geometry_view;
/* Synchronous borrowed snapshot; no model mutation/input dispatch/retention.
 * Balance children; same derived namespaces as old callbacks. New wins over old.
 * Text bytes survive completed-frame consumption; caller avoids ID collisions. */
typedef void (*cui_questionnaire_geometry_render_fn)(cui_context *, cui_id,
                                                     const cui_questionnaire_geometry_view *,
                                                     void *);
enum { CUI_QUESTIONNAIRE_CARD_FIXED_HEIGHT = 1u << 0 };
typedef struct {
    cui_questionnaire_box_refinement root, item, choices, actions, card;
    /* custom_scalar: zero omitted, otherwise finite (0,256], used only with
     * design_size INHERIT. Shared sizes win. Metric masks then override either. */
    float custom_scalar;
    uint32_t metric_fields;
    cui_questionnaire_geometry_metrics metrics;
    uint32_t card_fields;
    float card_height; /* active fixed height >= resolved minimum, <=4096 */
    cui_questionnaire_text_refinement answer, secondary, shortcut;
    cui_questionnaire_geometry_render_fn render_indicator, render_content, render_shortcut;
    void *render_user;
} cui_questionnaire_geometry_options;
/* Checked resolution; no writes on failure. Existing size/density baseline,
 * scalar (INHERIT only), masked values, text, box patches. Unknown masks or active
 * nonfinite/out-of-range values reject. Lengths[0,4096], font sizes(0,512].
 * Descriptor values copied at entry; no retained callback or metric cache.
 * All-zero geometry and NULL take exact legacy path. No legacy ABI extension.
 * Instance paint overrides state paint, never reenables disabled interaction.
 * Fixed-height content/ancestor clipping obey ordinary Core overflow rules.
 * Root publishes only existing size/density, never these opt-ins. */
bool cui_questionnaire_geometry_resolve(const cui_theme *, cui_questionnaire_options,
                                        const cui_questionnaire_geometry_options *,
                                        cui_questionnaire_geometry_metrics *out);
bool cui_questionnaire_begin_ex(cui_context *, cui_id, const cui_theme *, cui_questionnaire_state *,
                                cui_questionnaire_options,
                                const cui_questionnaire_keyboard_options *,
                                const cui_questionnaire_geometry_options *);
bool cui_questionnaire_item_begin_ex(cui_context *, cui_id, const cui_questionnaire_state *, size_t,
                                     cui_questionnaire_options,
                                     const cui_questionnaire_geometry_options *);
bool cui_questionnaire_choices_begin_ex(cui_context *, cui_id, const cui_questionnaire_state *,
                                        size_t, cui_questionnaire_options,
                                        const cui_questionnaire_geometry_options *);
bool cui_questionnaire_actions_begin_ex(cui_context *, cui_id, const cui_questionnaire_state *,
                                        cui_questionnaire_options,
                                        const cui_questionnaire_geometry_options *);
bool cui_questionnaire_choice_control_ex(cui_context *, cui_id, const cui_theme *,
                                         cui_questionnaire_state *, size_t, size_t,
                                         cui_questionnaire_options,
                                         const cui_questionnaire_keyboard_options *,
                                         const cui_questionnaire_geometry_options *);
#ifdef __cplusplus
}
#endif
#endif
