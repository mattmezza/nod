/* SPDX-License-Identifier: MIT */
#ifndef CUI_QUESTIONNAIRE_PRESENTATION_H
#define CUI_QUESTIONNAIRE_PRESENTATION_H
#include "cui_questionnaire_progress.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Independent opt-in; no root cascade or legacy options/state ABI extension.
 * LEGACY extracts the corresponding heading/error paint unchanged. TOKENS uses
 * frozen default role tiers for legacy sizes; shared design sizes keep their
 * resolved scale and use proportional role tiers (a CUI extension). */
typedef enum {
    CUI_QUESTIONNAIRE_TEXT_LEGACY,
    CUI_QUESTIONNAIRE_TEXT_TOKENS
} cui_questionnaire_text_policy;
typedef struct {
    cui_str text; /* resolved visible fallback, borrowed */
    float font_size, line_height;
    cui_color color;
    cui_text_style style;
} cui_questionnaire_text_view;
/* Synchronous once, replacing visible text. Balance children; do not close the
 * wrapper, mutate state, dispatch input or retain view. Child namespace is
 * derived from owner ID; caller avoids collisions with application IDs.
 * Prefer decorative content: Error already owns its one Alert semantic node. */
typedef void (*cui_questionnaire_text_render_fn)(cui_context *, cui_id,
                                                 const cui_questionnaire_text_view *, void *);
enum { CUI_QUESTIONNAIRE_TEXT_VISIBLE = 1u << 0 };
typedef struct {
    cui_questionnaire_text_policy policy;
    uint32_t fields;
    /* Masked valid UTF-8 without NUL, including {NULL,0} explicit empty.
     * Replacement text affects visible text only; Error's accessible name
     * remains the localized model error. Custom child wins over visible text. */
    cui_str visible;
    cui_questionnaire_text_refinement text;
    cui_questionnaire_text_render_fn render_child;
    void *render_user;
} cui_questionnaire_text_options;
/* Title/Description require a known schema item, not current/enabled status.
 * Enclose in successful item_begin to gate inactive items. Title falls back to
 * label/name; Description omits its wrapper when schema description is empty
 * and neither explicit visible nor child exists. Zero extracts legacy heading
 * text metrics; it does not claim two separate parts reproduce heading's gap.
 * TOKENS Title adds an item-gap spacer when schema description is empty.
 * ChoiceDescription has explicit content, no state or schema fallback.
 * Strings (including custom child's draws) survive completed-frame consumption;
 * descriptors copied at entry, functions/user borrowed only during submission.
 * Unknown policies/masks/invalid active refinements reject before any output or
 * callback. True means accepted (including omission), not child/layout success.
 * These parts do not validate, flush editors, consume events or emit events. */
bool cui_questionnaire_title_ex(cui_context *, cui_id, const cui_theme *,
                                const cui_questionnaire_state *, size_t item,
                                cui_questionnaire_options, cui_questionnaire_text_options);
bool cui_questionnaire_description_ex(cui_context *, cui_id, const cui_theme *,
                                      const cui_questionnaire_state *, size_t item,
                                      cui_questionnaire_options, cui_questionnaire_text_options);
bool cui_questionnaire_choice_description_ex(cui_context *, cui_id, const cui_theme *, cui_str,
                                             cui_questionnaire_options,
                                             cui_questionnaire_text_options);
/* Zero retains legacy current-item/error gating and Alert name. No separate
 * schema-error visibility policy in this slice. TOKENS adds secondary line box
 * and an 8px top spacer; explicit refinement wins. Custom child never replaces
 * model error/Alert semantics. Valid/no-current-error omits output and callback. */
bool cui_questionnaire_error_ex(cui_context *, cui_id, const cui_theme *,
                                const cui_questionnaire_state *, size_t item,
                                cui_questionnaire_options, cui_questionnaire_text_options);
#ifdef __cplusplus
}
#endif
#endif
