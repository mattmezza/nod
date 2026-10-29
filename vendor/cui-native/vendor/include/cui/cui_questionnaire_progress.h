/* SPDX-License-Identifier: MIT */
#ifndef CUI_QUESTIONNAIRE_PROGRESS_H
#define CUI_QUESTIONNAIRE_PROGRESS_H
#include "cui_questionnaire.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
    CUI_QUESTIONNAIRE_TEXT_SIZE = 1u << 0,
    CUI_QUESTIONNAIRE_TEXT_LINE_HEIGHT = 1u << 1,
    CUI_QUESTIONNAIRE_TEXT_COLOR = 1u << 2,
    CUI_QUESTIONNAIRE_TEXT_STYLE = 1u << 3
};
/* Explicit masks: unmasked values are ignored. Style replaces all members,
 * including normal weight/false flags/family zero/letter spacing zero.
 * Active font size: finite (0,512]; line height: finite [0,4096], zero natural.
 * Active style: known weight and finite letter spacing. Font/shaping capabilities
 * and negative spacing's cluster-progression limits remain host/Core concerns. */
typedef struct {
    uint32_t fields;
    float font_size, line_height;
    cui_color color;
    cui_text_style style;
} cui_questionnaire_text_refinement;
typedef struct {
    size_t current, total; /* One-based enabled position, zero without current. */
    float font_size, line_height;
    cui_color color;
    cui_text_style style;
} cui_questionnaire_progress_view;
/* Runs once synchronously inside the progress wrapper, replacing default paint.
 * View/user borrowed only for this call. Balance child boxes; do not close the
 * wrapper, mutate the model or dispatch input. ID is a child namespace, not the
 * progress semantic ID. Caller avoids collisions with other application IDs,
 * as for existing child composition. No inherited text cascade: use view metrics explicitly.
 * Prefer decorative children to avoid duplicate progress semantics. */
typedef void (*cui_questionnaire_progress_render_fn)(cui_context *, cui_id,
                                                     const cui_questionnaire_progress_view *,
                                                     void *);
enum { CUI_QUESTIONNAIRE_PROGRESS_LABEL = 1u << 0, CUI_QUESTIONNAIRE_PROGRESS_VISIBLE = 1u << 1 };
typedef struct {
    uint32_t fields;
    /* Masked valid UTF-8 without embedded NUL; {NULL,0} is explicit empty.
     * LABEL changes accessible name and default visible text. VISIBLE changes
     * visible text only. A custom child replaces visible text entirely.
     * String bytes remain borrowed through completed-frame draw/semantic use. */
    cui_str label, visible;
    cui_questionnaire_text_refinement text;
    cui_questionnaire_progress_render_fn render_child;
    void *render_user;
} cui_questionnaire_progress_options;
/* Zero preserves legacy progress. Descriptors copied at entry; no retained
 * registration, model mutation, validation, editor flush or event consumption.
 * Numeric semantics remain model-derived regardless of supplied labels/children.
 * Unknown masks/invalid active fields reject before semantics/draw/callback.
 * False means invalid arguments/options; true means submission accepted, not a
 * transaction over child rendering/layout errors (use normal frame errors).
 * Independent part options; no root publication or legacy options/state ABI change. */
bool cui_questionnaire_progress_ex(cui_context *, cui_id, const cui_theme *,
                                   const cui_questionnaire_state *, cui_questionnaire_options,
                                   cui_questionnaire_progress_options);
#ifdef __cplusplus
}
#endif
#endif
