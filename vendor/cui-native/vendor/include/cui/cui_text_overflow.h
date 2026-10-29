/* SPDX-License-Identifier: MIT */
#ifndef CUI_TEXT_OVERFLOW_H
#define CUI_TEXT_OVERFLOW_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float width; /* Positive fixed width; negative grows in the parent. Zero invalid. */
    float font_size;
    cui_color color;
    cui_text_style text_style;
} cui_text_ellipsis_options;
/* Single-line text, sized in the current layout pass and painted with a trailing
 * ellipsis at extended grapheme boundaries. Full text remains the semantic name.
 * Requires a unique nonzero ID. Copies text into frame storage. Newlines rejected.
 * Prefix and ellipsis are measured/painted separately; no paragraph reflow or
 * contextual shaping across the ellipsis boundary is promised. Height is natural.
 * Use fixed-width siblings for trailing controls in a constrained row. */
bool cui_text_ellipsis(cui_context *, cui_id, cui_str, cui_text_ellipsis_options);
#ifdef __cplusplus
}
#endif
#endif
