/* SPDX-License-Identifier: MIT */
#include "cui/cui_color_hsla.h"
#include <math.h>
#include <string.h>
static bool cui_color_hsla_unit(float v) {
    return isfinite(v) && v >= 0 && v <= 1;
}
bool cui_color_hsla_valid(cui_color_hsla c) {
    return cui_color_hsla_unit(c.h) && cui_color_hsla_unit(c.s) && cui_color_hsla_unit(c.l) &&
           cui_color_hsla_unit(c.a);
}
cui_color_hsla_status cui_color_hsla_to_rgba(cui_color_hsla c, cui_color_encoded_rgba *out) {
    if (!out || !cui_color_hsla_valid(c))
        return CUI_COLOR_HSLA_INVALID_ARGUMENT;
    float chroma = (1.f - fabsf(2.f * c.l - 1.f)) * c.s;
    float x = chroma * (1.f - fabsf(fmodf(c.h * 6.f, 2.f) - 1.f)), m = c.l - chroma / 2.f;
    float cm = chroma + m, xm = x + m;
    cui_color_encoded_rgba r = {cm, m, xm, c.a};
    switch ((int)floorf(c.h * 6.f)) {
    case 0:
    case 6:
        r = (cui_color_encoded_rgba){cm, xm, m, c.a};
        break;
    case 1:
        r = (cui_color_encoded_rgba){xm, cm, m, c.a};
        break;
    case 2:
        r = (cui_color_encoded_rgba){m, cm, xm, c.a};
        break;
    case 3:
        r = (cui_color_encoded_rgba){m, xm, cm, c.a};
        break;
    case 4:
        r = (cui_color_encoded_rgba){xm, m, cm, c.a};
        break;
    default:
        break;
    }
    r.r = fmaxf(0, fminf(1, r.r));
    r.g = fmaxf(0, fminf(1, r.g));
    r.b = fmaxf(0, fminf(1, r.b));
    *out = r;
    return CUI_COLOR_HSLA_OK;
}
cui_color_hsla_status cui_color_hsla_from_rgba(cui_color_encoded_rgba c, cui_color_hsla *out) {
    if (!out || !cui_color_hsla_unit(c.r) || !cui_color_hsla_unit(c.g) ||
        !cui_color_hsla_unit(c.b) || !cui_color_hsla_unit(c.a))
        return CUI_COLOR_HSLA_INVALID_ARGUMENT;
    float maximum = fmaxf(c.r, fmaxf(c.g, c.b)), minimum = fminf(c.r, fminf(c.g, c.b)),
          delta = maximum - minimum;
    float l = (maximum + minimum) / 2.f;
    float s = l == 0 || l == 1 ? 0 : l < .5f ? delta / (2.f * l) : delta / (2.f - 2.f * l);
    float h = 0;
    if (delta != 0) {
        if (maximum == c.r) {
            float sector = fmodf((c.g - c.b) / delta, 6.f);
            if (sector < 0)
                sector += 6.f;
            h = sector / 6.f;
        } else if (maximum == c.g)
            h = ((c.b - c.r) / delta + 2.f) / 6.f;
        else
            h = ((c.r - c.g) / delta + 4.f) / 6.f;
    }
    *out = (cui_color_hsla){h, s, l, c.a};
    return CUI_COLOR_HSLA_OK;
}
cui_color_hsla_status cui_color_hsla_from_color(cui_color c, cui_color_hsla *out) {
    return cui_color_hsla_from_rgba(
        (cui_color_encoded_rgba){c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f}, out);
}
cui_color_hsla_status cui_color_hsla_to_color(cui_color_hsla c, cui_color *out) {
    cui_color_encoded_rgba r;
    if (!out || cui_color_hsla_to_rgba(c, &r) != CUI_COLOR_HSLA_OK)
        return CUI_COLOR_HSLA_INVALID_ARGUMENT;
    *out = (cui_color){(uint8_t)(r.r * 255.f), (uint8_t)(r.g * 255.f), (uint8_t)(r.b * 255.f),
                       (uint8_t)(r.a * 255.f)};
    return CUI_COLOR_HSLA_OK;
}
static int cui_color_hsla_nibble(unsigned char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
cui_color_hsla_hex_status cui_color_hsla_parse_hex(cui_str text, cui_color_hsla *out) {
    if ((!text.data && text.length) || text.length > 9)
        return CUI_COLOR_HSLA_HEX_INVALID;
    size_t start = text.length && text.data[0] == '#' ? 1 : 0, n = text.length - start;
    if (n > 8)
        return CUI_COLOR_HSLA_HEX_INVALID;
    for (size_t i = start; i < text.length; ++i)
        if (cui_color_hsla_nibble((unsigned char)text.data[i]) < 0)
            return CUI_COLOR_HSLA_HEX_INVALID;
    if (n != 3 && n != 4 && n != 6 && n != 8)
        return CUI_COLOR_HSLA_HEX_INCOMPLETE;
    uint8_t v[4] = {0, 0, 0, 255};
    size_t count = n == 3 || n == 6 ? 3 : 4;
    for (size_t i = 0; i < count; ++i) {
        int a = cui_color_hsla_nibble((unsigned char)text.data[start + (n < 5 ? i : 2 * i)]);
        int b = n < 5 ? a : cui_color_hsla_nibble((unsigned char)text.data[start + 2 * i + 1]);
        v[i] = (uint8_t)(16 * a + b);
    }
    cui_color_hsla value;
    (void)cui_color_hsla_from_color((cui_color){v[0], v[1], v[2], v[3]}, &value);
    if (out)
        *out = value;
    return CUI_COLOR_HSLA_HEX_COMPLETE;
}
cui_color_hsla_format_result cui_color_hsla_format_hex(cui_color_hsla c, char *out,
                                                       size_t capacity) {
    if (!cui_color_hsla_valid(c) || (!out && capacity))
        return (cui_color_hsla_format_result){CUI_COLOR_HSLA_INVALID_ARGUMENT, 0};
    size_t required = c.a < 1 ? 9 : 7;
    if (capacity <= required)
        return (cui_color_hsla_format_result){CUI_COLOR_HSLA_CAPACITY, required};
    cui_color color = {0};
    (void)cui_color_hsla_to_color(c, &color);
    const char *digits = "0123456789ABCDEF";
    uint8_t channels[4] = {color.r, color.g, color.b, color.a};
    char text[10] = {'#'};
    for (size_t i = 0; i < (required - 1) / 2; ++i) {
        text[1 + 2 * i] = digits[channels[i] >> 4];
        text[2 + 2 * i] = digits[channels[i] & 15];
    }
    text[required] = 0;
    memcpy(out, text, required + 1);
    return (cui_color_hsla_format_result){CUI_COLOR_HSLA_OK, required};
}
