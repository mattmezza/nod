/* SPDX-License-Identifier: MIT */
#ifndef CUI_TYPOGRAPHY_H
#define CUI_TYPOGRAPHY_H
#include "cui.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Value-owned authoring tokens. No font resources or mutable context state.
 * All dimensions are logical pixels. Zero line_height selects natural metrics. */
typedef struct {
    cui_text_style style;
    float size, line_height;
    cui_text_align align;
    bool wrap;
} cui_typography_token;
typedef enum {
    CUI_TYPOGRAPHY_BODY,
    CUI_TYPOGRAPHY_HEADING,
    CUI_TYPOGRAPHY_CODE
} cui_typography_role;
typedef struct {
    cui_typography_token body, heading, code;
} cui_typography_roles;
static inline bool cui_typography_token_valid(cui_typography_token token) {
    return isfinite(token.size) && token.size > 0 && token.size <= 65535 &&
           isfinite(token.line_height) && token.line_height >= 0 && token.line_height <= 65535 &&
           token.style.weight >= CUI_TEXT_WEIGHT_NORMAL &&
           token.style.weight <= CUI_TEXT_WEIGHT_BOLD && token.align >= CUI_TEXT_ALIGN_LEFT &&
           token.align <= CUI_TEXT_ALIGN_RIGHT;
}
/* Copy a validated role token. Failure preserves *out. Only the selected role
 * must be configured; zero-initialized unused roles remain invalid. */
static inline bool cui_typography_resolve(const cui_typography_roles *roles,
                                          cui_typography_role role, cui_typography_token *out) {
    if (!roles || !out)
        return false;
    cui_typography_token token;
    switch (role) {
    case CUI_TYPOGRAPHY_BODY:
        token = roles->body;
        break;
    case CUI_TYPOGRAPHY_HEADING:
        token = roles->heading;
        break;
    case CUI_TYPOGRAPHY_CODE:
        token = roles->code;
        break;
    default:
        return false;
    }
    if (!cui_typography_token_valid(token))
        return false;
    *out = token;
    return true;
}
/* Full strict UTF-8 paragraph; original-byte spans retain Core's contextual
 * cluster-start color semantics. Same capacity/error contract as paragraph API.
 * Tokens are copied by value; the helper does not retain the token or role map. */
static inline bool cui_typography_text_colored(cui_context *ui, cui_str text,
                                               cui_typography_token token, cui_color color,
                                               const cui_text_color_span *spans, size_t count) {
    if (!cui_typography_token_valid(token))
        return false;
    return cui_text_paragraph_colored_aligned(ui, text, token.size, color, token.line_height,
                                              token.style, token.wrap, spans, count, token.align);
}
static inline bool cui_typography_text(cui_context *ui, cui_str text, cui_typography_token token,
                                       cui_color color) {
    return cui_typography_text_colored(ui, text, token, color, NULL, 0);
}
static inline bool cui_typography_text_painted(cui_context *ui, cui_str text,
                                               cui_typography_token token, cui_text_paint paint) {
    if (!cui_typography_token_valid(token))
        return false;
    return cui_text_paragraph_painted(ui, text, token.size, token.line_height, token.style,
                                      token.wrap, token.align, paint);
}
#ifdef __cplusplus
}
#endif
#endif
