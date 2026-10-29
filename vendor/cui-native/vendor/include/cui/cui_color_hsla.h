/* SPDX-License-Identifier: MIT */
#ifndef CUI_COLOR_HSLA_H
#define CUI_COLOR_HSLA_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Normalized finite HSL (lightness, not HSV brightness). Directly accepted
 * components retain float precision and hue endpoints; no implicit clamp/wrap.
 * These encoded RGB conversions do not apply an sRGB transfer function. */
typedef struct {
    float h, s, l, a;
} cui_color_hsla;
/* Encoded, unpremultiplied channels; explicitly not Plot linear RGBA. */
typedef struct {
    float r, g, b, a;
} cui_color_encoded_rgba;
typedef enum {
    CUI_COLOR_HSLA_OK,
    CUI_COLOR_HSLA_INVALID_ARGUMENT,
    CUI_COLOR_HSLA_CAPACITY
} cui_color_hsla_status;
typedef enum {
    CUI_COLOR_HSLA_HEX_INVALID,
    CUI_COLOR_HSLA_HEX_INCOMPLETE,
    CUI_COLOR_HSLA_HEX_COMPLETE
} cui_color_hsla_hex_status;
typedef struct {
    cui_color_hsla_status status;
    size_t required; /* Payload bytes, excluding NUL; 7 or 9 for valid color. */
} cui_color_hsla_format_result;
bool cui_color_hsla_valid(cui_color_hsla);
/* Nonnull output required; failure leaves output untouched. Disjoint storage. */
cui_color_hsla_status cui_color_hsla_from_rgba(cui_color_encoded_rgba, cui_color_hsla *out);
cui_color_hsla_status cui_color_hsla_to_rgba(cui_color_hsla, cui_color_encoded_rgba *out);
cui_color_hsla_status cui_color_hsla_from_color(cui_color, cui_color_hsla *out);
/* Explicit source-style truncating export, never updates a retained model. */
cui_color_hsla_status cui_color_hsla_to_color(cui_color_hsla, cui_color *out);
/* Optional #, 3/4/6/8 ASCII hex digits. Empty/hex prefixes with other widths
 * <=8 are incomplete. No CSS grammar. Only COMPLETE writes out; NULL validates.
 * Byte channels divide by 255 then convert to HSL; no byte model is retained. */
cui_color_hsla_hex_status cui_color_hsla_parse_hex(cui_str, cui_color_hsla *out);
/* Uppercase, alpha omitted only when opaque, source-style channel truncation.
 * Capacity includes NUL; insufficient output is atomic, required excludes NUL.
 * NULL/0 is a length probe (CAPACITY); NULL/nonzero is invalid. Invalid arguments
 * return required=0. No promise of byte-exact round trips at float boundaries. */
cui_color_hsla_format_result cui_color_hsla_format_hex(cui_color_hsla, char *out, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
