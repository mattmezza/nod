/* SPDX-License-Identifier: MIT */
#include "cui/cui_theme_registry.h"
#include "cui_theme_json.h"
#include "cui_theme_palette.inc"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
static bool cui_theme_private_memory_overlap(const void *a, size_t an, const void *b, size_t bn) {
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    if (an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y)
        return true;
    return x < y + bn && y < x + an;
}
static bool cui_theme_private_fixed_string(const char *s, size_t capacity, bool empty) {
    const char *end = memchr(s, 0, capacity);
    return end && (empty || end != s) && cui_theme_private_theme_utf8(s, (size_t)(end - s));
}
static bool cui_theme_private_bounded(float value, float minimum, float maximum) {
    return isfinite(value) && value >= minimum && value <= maximum;
}
static size_t cui_theme_private_trim_space(char *text, size_t length) {
    while (length) {
        size_t begin = length - 1;
        while (begin && ((unsigned char)text[begin] & 192) == 128)
            begin--;
        unsigned c = (unsigned char)text[begin], scalar = c;
        if (c >= 0xc0) {
            scalar = c & (c < 0xe0 ? 31 : c < 0xf0 ? 15 : 7);
            for (size_t i = begin + 1; i < length; i++)
                scalar = (scalar << 6) | ((unsigned char)text[i] & 63);
        }
        bool space = (scalar >= 9 && scalar <= 13) || scalar == 32 || scalar == 0x85 ||
                     scalar == 0xa0 || scalar == 0x1680 || (scalar >= 0x2000 && scalar <= 0x200a) ||
                     scalar == 0x2028 || scalar == 0x2029 || scalar == 0x202f || scalar == 0x205f ||
                     scalar == 0x3000;
        if (!space)
            break;
        length = begin;
        text[length] = 0;
    }
    return length;
}
bool cui_theme_color_parse(cui_str value, cui_color *out) {
    if (!out || !value.data || !value.length || value.length >= 128 ||
        !cui_theme_private_theme_utf8(value.data, value.length))
        return false;
    char text[128];
    memcpy(text, value.data, value.length);
    text[value.length] = 0;
    cui_color c = {0, 0, 0, 255};
    if (text[0] == '#') {
        size_t length = cui_theme_private_trim_space(text, value.length);
        size_t n = length - 1;
        if (n != 3 && n != 4 && n != 6 && n != 8)
            return false;
        unsigned char channels[4] = {0, 0, 0, 255};
        for (size_t i = 0; i < (n == 3 || n == 4 ? n : n / 2); i++) {
            int a = cui_theme_private_theme_hex((unsigned char)text[1 + (n <= 4 ? i : i * 2)]);
            if (a < 0)
                return false;
            int b = n <= 4 ? a : cui_theme_private_theme_hex((unsigned char)text[2 + i * 2]);
            if (b < 0)
                return false;
            channels[i] = (unsigned char)(a * 16 + b);
        }
        c = (cui_color){channels[0], channels[1], channels[2], channels[3]};
    } else {
        char *opacity = strchr(text, '/');
        float alpha = 100;
        if (opacity) {
            *opacity++ = 0;
            double v;
            if (!cui_theme_private_theme_decimal(opacity, strlen(opacity), &v) || v < 0 || v > 100)
                return false;
            alpha = (float)v;
        }
        char *scale = strchr(text, '-');
        unsigned index = 500;
        if (scale) {
            *scale++ = 0;
            char *end;
            unsigned long n = strtoul(scale, &end, 10);
            if (!*scale || *end || n > 1000)
                return false;
            index = (unsigned)n;
        }
        if (!strcmp(text, "black"))
            c = (cui_color){0, 0, 0, 255};
        else if (!strcmp(text, "white"))
            c = (cui_color){255, 255, 255, 255};
        else {
            bool found = false;
            for (size_t i = 0; i < sizeof cui_theme_private_theme_palette /
                                       sizeof *cui_theme_private_theme_palette;
                 i++)
                if (cui_theme_private_theme_palette[i].scale == index &&
                    !strcmp(cui_theme_private_theme_palette[i].name, text)) {
                    c = cui_theme_private_theme_palette[i].color;
                    found = true;
                    break;
                }
            if (!found)
                return false;
        }
        c.a = (uint8_t)lroundf(alpha * 2.55f);
    }
    *out = c;
    return true;
}
static cui_theme_semantic_shadow cui_theme_private_source_shadows(void) {
    cui_theme_semantic_shadow result = {0};
    cui_theme_shadows *s[] = {&result.sm, &result.md, &result.lg};
    const float y[] = {1, 4, 12}, blur[] = {2, 8, 24}, spread[] = {0, -2, -4};
    for (size_t i = 0; i < 3; i++) {
        s[i]->count = 1;
        s[i]->values[0] = (cui_theme_shadow){.color = {0, 0, 0, 46},
                                             .offset_y = y[i],
                                             .blur_radius = blur[i],
                                             .spread_radius = spread[i]};
    }
    return result;
}
static cui_theme_text_token cui_theme_private_text_token(float size, float line) {
    return (cui_theme_text_token){size, line, 400};
}
static cui_theme_semantic_colors cui_theme_private_colors_of(const cui_theme *t) {
    return (cui_theme_semantic_colors){t->background,
                                       t->text,
                                       t->popover,
                                       t->popover_foreground,
                                       t->primary,
                                       t->on_primary,
                                       t->secondary,
                                       t->secondary_foreground,
                                       t->muted_background,
                                       t->muted,
                                       t->accent,
                                       t->accent_foreground,
                                       t->danger,
                                       t->on_danger,
                                       t->border,
                                       t->input,
                                       t->focus,
                                       t->selection};
}
bool cui_theme_semantic_snapshot(const cui_theme *t, cui_theme_semantic_tokens *out) {
    if (!t || !out || !cui_theme_private_bounded(t->font_size, .01f, 4096) ||
        !cui_theme_private_bounded(t->radius, 0, 500000) ||
        !cui_theme_private_bounded(t->radius_lg, 0, 1e6f))
        return false;
    cui_theme_semantic_tokens s = {
        .colors = cui_theme_private_colors_of(t),
        .radius = {0, t->radius / 2, t->radius, t->radius_lg, t->radius * 2, t->radius ? 9999 : 0},
        .spacing = {2, 4, 8, 12, 16, 24, 32}};
    s.shadow = cui_theme_private_source_shadows();
    strcpy(s.typography.sans, ".SystemUIFont");
    /* Portable snapshot names a logical mono family; actual platform discovery
     * is a host responsibility, unlike the reference platform resolver. */
    strcpy(s.typography.mono, "monospace");
    s.typography.xs = cui_theme_private_text_token(12, 16);
    s.typography.sm = cui_theme_private_text_token(14, 20);
    s.typography.md = cui_theme_private_text_token(t->font_size, 24);
    s.typography.lg = cui_theme_private_text_token(18, 28);
    s.typography.xl = cui_theme_private_text_token(20, 28);
    s.typography.mono_md = cui_theme_private_text_token(13, 20);
    *out = s;
    return true;
}
static bool cui_theme_private_tokens_valid(const cui_theme_semantic_tokens *s) {
    if (!s ||
        !cui_theme_private_fixed_string(s->typography.sans, sizeof s->typography.sans, false) ||
        !cui_theme_private_fixed_string(s->typography.mono, sizeof s->typography.mono, false))
        return false;
    const float r[] = {s->radius.none, s->radius.sm,   s->radius.md,  s->radius.lg,  s->radius.xl,
                       s->radius.full, s->spacing.xxs, s->spacing.xs, s->spacing.sm, s->spacing.md,
                       s->spacing.lg,  s->spacing.xl,  s->spacing.xxl};
    for (size_t i = 0; i < sizeof r / sizeof *r; i++)
        if (!cui_theme_private_bounded(r[i], 0, 1e6f))
            return false;
    const cui_theme_text_token styles[] = {s->typography.xs, s->typography.sm,
                                           s->typography.md, s->typography.lg,
                                           s->typography.xl, s->typography.mono_md};
    for (size_t i = 0; i < 6; i++)
        if (!cui_theme_private_bounded(styles[i].size, .01f, 4096) ||
            !cui_theme_private_bounded(styles[i].line_height, 0, 4096) ||
            !cui_theme_private_bounded(styles[i].weight, 1, 1000))
            return false;
    const cui_theme_shadows *shadows[] = {&s->shadow.sm, &s->shadow.md, &s->shadow.lg};
    for (size_t i = 0; i < 3; i++) {
        if (shadows[i]->count > CUI_THEME_SHADOW_CAPACITY)
            return false;
        for (size_t k = 0; k < shadows[i]->count; k++) {
            cui_theme_shadow b = shadows[i]->values[k];
            if (!cui_theme_private_bounded(b.offset_x, -1e6f, 1e6f) ||
                !cui_theme_private_bounded(b.offset_y, -1e6f, 1e6f) ||
                !cui_theme_private_bounded(b.blur_radius, 0, 1e6f) ||
                !cui_theme_private_bounded(b.spread_radius, -1e6f, 1e6f))
                return false;
        }
    }
    return true;
}
bool cui_theme_semantic_apply(cui_theme *t, const cui_theme_semantic_tokens *s) {
    if (!t || !cui_theme_private_tokens_valid(s))
        return false;
    cui_theme n = *t;
    n.background = s->colors.background;
    n.text = s->colors.foreground;
    n.popover = s->colors.surface;
    n.popover_foreground = s->colors.surface_foreground;
    n.primary = s->colors.primary;
    n.on_primary = s->colors.primary_foreground;
    n.secondary = s->colors.secondary;
    n.secondary_foreground = s->colors.secondary_foreground;
    n.muted_background = s->colors.muted;
    n.muted = s->colors.muted_foreground;
    n.accent = s->colors.accent;
    n.accent_foreground = s->colors.accent_foreground;
    n.danger = s->colors.destructive;
    n.on_danger = s->colors.destructive_foreground;
    n.border = s->colors.border;
    n.input = s->colors.input;
    n.focus = s->colors.ring;
    n.radius = s->radius.md;
    n.radius_lg = s->radius.lg;
    n.font_size = s->typography.md.size;
    *t = n;
    return true;
}
typedef struct {
    const char *name;
    size_t offset;
} cui_theme_private_theme_field;
#define CUI_THEME_PRIVATE_FIELD(type, name) {#name, offsetof(type, name)}
static const cui_theme_private_theme_field cui_theme_private_semantic_colors[] = {
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, background),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, foreground),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, surface),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, surface_foreground),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, primary),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, primary_foreground),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, secondary),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, secondary_foreground),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, muted),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, muted_foreground),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, accent),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, accent_foreground),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, destructive),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, destructive_foreground),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, border),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, input),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_colors, ring)};
static const cui_theme_private_theme_field cui_theme_private_semantic_radius[] = {
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_radius, none),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_radius, sm),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_radius, md),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_radius, lg),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_radius, xl),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_radius, full)};
static const cui_theme_private_theme_field cui_theme_private_semantic_spacing[] = {
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_spacing, xxs),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_spacing, xs),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_spacing, sm),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_spacing, md),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_spacing, lg),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_spacing, xl),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_spacing, xxl)};
static const cui_theme_private_theme_field cui_theme_private_semantic_styles[] = {
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_typography, xs),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_typography, sm),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_typography, md),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_typography, lg),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_typography, xl),
    CUI_THEME_PRIVATE_FIELD(cui_theme_semantic_typography, mono_md)};
static const cui_theme_private_theme_field *
cui_theme_private_field_find(const cui_theme_private_theme_field *fields, size_t count,
                             const char *name) {
    for (size_t i = 0; i < count; i++)
        if (!strcmp(fields[i].name, name))
            return &fields[i];
    return NULL;
}
static cui_theme_registry_error
cui_theme_private_import_fields(const cui_theme_private_theme_json *j, size_t object,
                                void *destination, const cui_theme_private_theme_field *fields,
                                size_t count, bool colors) {
    if (j->nodes[object].type != CUI_THEME_PRIVATE_J_OBJECT)
        return CUI_THEME_REGISTRY_INVALID;
    for (size_t i = object + 1; i < j->nodes[object].next;) {
        char key[256];
        if (!cui_theme_private_json_string(j, i, key, sizeof key))
            return CUI_THEME_REGISTRY_INVALID;
        size_t v = j->nodes[i].next;
        const cui_theme_private_theme_field *f = cui_theme_private_field_find(fields, count, key);
        if (!f)
            return CUI_THEME_REGISTRY_UNSUPPORTED;
        if (j->nodes[v].type != CUI_THEME_PRIVATE_J_NULL) {
            if (colors) {
                char text[128];
                cui_color c;
                if (!cui_theme_private_json_string(j, v, text, sizeof text) ||
                    !cui_theme_color_parse((cui_str){text, strlen(text)}, &c))
                    return CUI_THEME_REGISTRY_INVALID;
                memcpy((char *)destination + f->offset, &c, sizeof c);
            } else {
                float value;
                if (!cui_theme_private_json_number(j, v, &value) || value < 0)
                    return CUI_THEME_REGISTRY_INVALID;
                memcpy((char *)destination + f->offset, &value, sizeof value);
            }
        }
        i = j->nodes[v].next;
    }
    return CUI_THEME_REGISTRY_OK;
}
static cui_theme_registry_error
cui_theme_private_import_typography(const cui_theme_private_theme_json *j, size_t node,
                                    cui_theme_semantic_typography *t) {
    if (j->nodes[node].type != CUI_THEME_PRIVATE_J_OBJECT)
        return CUI_THEME_REGISTRY_INVALID;
    for (size_t i = node + 1; i < j->nodes[node].next;) {
        char key[256];
        if (!cui_theme_private_json_string(j, i, key, sizeof key))
            return CUI_THEME_REGISTRY_INVALID;
        size_t v = j->nodes[i].next;
        if (!strcmp(key, "sans") || !strcmp(key, "mono")) {
            if (j->nodes[v].type != CUI_THEME_PRIVATE_J_NULL) {
                if (j->nodes[v].type != CUI_THEME_PRIVATE_J_STRING)
                    return CUI_THEME_REGISTRY_INVALID;
                if (!cui_theme_private_json_string(j, v, !strcmp(key, "sans") ? t->sans : t->mono,
                                                   CUI_THEME_NAME_CAPACITY))
                    return CUI_THEME_REGISTRY_CAPACITY_ERROR;
            }
        } else {
            const cui_theme_private_theme_field *f =
                cui_theme_private_field_find(cui_theme_private_semantic_styles, 6, key);
            if (!f)
                return CUI_THEME_REGISTRY_UNSUPPORTED;
            if (j->nodes[v].type != CUI_THEME_PRIVATE_J_OBJECT)
                return CUI_THEME_REGISTRY_INVALID;
            static const cui_theme_private_theme_field styles[] = {
                CUI_THEME_PRIVATE_FIELD(cui_theme_text_token, size),
                CUI_THEME_PRIVATE_FIELD(cui_theme_text_token, line_height),
                CUI_THEME_PRIVATE_FIELD(cui_theme_text_token, weight)};
            cui_theme_registry_error error =
                cui_theme_private_import_fields(j, v, (char *)t + f->offset, styles, 3, false);
            if (error)
                return error;
        }
        i = j->nodes[v].next;
    }
    return CUI_THEME_REGISTRY_OK;
}
#undef CUI_THEME_PRIVATE_FIELD
static float cui_theme_private_hue(float p, float q, float h) {
    if (h < 0)
        h += 1;
    if (h > 1)
        h -= 1;
    if (h < 1.f / 6)
        return p + (q - p) * 6 * h;
    if (h < .5f)
        return q;
    if (h < 2.f / 3)
        return p + (q - p) * (2.f / 3 - h) * 6;
    return p;
}
static bool cui_theme_private_import_hsla(const cui_theme_private_theme_json *j, size_t object,
                                          cui_color *out) {
    if (j->nodes[object].type != CUI_THEME_PRIVATE_J_OBJECT || j->nodes[object].count != 8)
        return false;
    float v[4];
    const char *names[] = {"h", "s", "l", "a"};
    for (size_t i = 0; i < 4; i++) {
        size_t n = cui_theme_private_json_member(j, object, names[i]);
        if (n == SIZE_MAX || !cui_theme_private_json_number(j, n, &v[i]) ||
            !cui_theme_private_bounded(v[i], 0, 1))
            return false;
    }
    float r = v[2], g = r, b = r;
    if (v[1]) {
        float q = v[2] < .5f ? v[2] * (1 + v[1]) : v[2] + v[1] - v[2] * v[1], p = 2 * v[2] - q;
        r = cui_theme_private_hue(p, q, v[0] + 1.f / 3);
        g = cui_theme_private_hue(p, q, v[0]);
        b = cui_theme_private_hue(p, q, v[0] - 1.f / 3);
    }
    *out = (cui_color){(uint8_t)lroundf(r * 255), (uint8_t)lroundf(g * 255),
                       (uint8_t)lroundf(b * 255), (uint8_t)lroundf(v[3] * 255)};
    return true;
}
static cui_theme_registry_error
cui_theme_private_import_shadow(const cui_theme_private_theme_json *j, size_t node,
                                cui_theme_shadows *out) {
    if (j->nodes[node].type == CUI_THEME_PRIVATE_J_NULL)
        return CUI_THEME_REGISTRY_OK;
    if (j->nodes[node].type != CUI_THEME_PRIVATE_J_ARRAY)
        return CUI_THEME_REGISTRY_INVALID;
    if (j->nodes[node].count > CUI_THEME_SHADOW_CAPACITY)
        return CUI_THEME_REGISTRY_CAPACITY_ERROR;
    cui_theme_shadows s = {0};
    for (size_t i = node + 1; i < j->nodes[node].next; i = j->nodes[i].next) {
        if (j->nodes[i].type != CUI_THEME_PRIVATE_J_OBJECT || j->nodes[i].count != 10)
            return CUI_THEME_REGISTRY_INVALID;
        size_t color = cui_theme_private_json_member(j, i, "color"),
               offset = cui_theme_private_json_member(j, i, "offset"),
               blur = cui_theme_private_json_member(j, i, "blur_radius"),
               spread = cui_theme_private_json_member(j, i, "spread_radius"),
               inset = cui_theme_private_json_member(j, i, "inset");
        if (color == SIZE_MAX || offset == SIZE_MAX || blur == SIZE_MAX || spread == SIZE_MAX ||
            inset == SIZE_MAX)
            return CUI_THEME_REGISTRY_UNSUPPORTED;
        cui_theme_shadow *b = &s.values[s.count++];
        if (!cui_theme_private_import_hsla(j, color, &b->color) ||
            j->nodes[offset].type != CUI_THEME_PRIVATE_J_OBJECT || j->nodes[offset].count != 4)
            return CUI_THEME_REGISTRY_INVALID;
        size_t x = cui_theme_private_json_member(j, offset, "x"),
               y = cui_theme_private_json_member(j, offset, "y");
        if (x == SIZE_MAX || y == SIZE_MAX || !cui_theme_private_json_number(j, x, &b->offset_x) ||
            !cui_theme_private_json_number(j, y, &b->offset_y) ||
            !cui_theme_private_json_number(j, blur, &b->blur_radius) ||
            !cui_theme_private_json_number(j, spread, &b->spread_radius) ||
            !cui_theme_private_json_bool(j, inset, &b->inset))
            return CUI_THEME_REGISTRY_INVALID;
    }
    *out = s;
    return CUI_THEME_REGISTRY_OK;
}
cui_theme_registry_error cui_theme_semantic_import(cui_str source, const cui_theme *base,
                                                   cui_theme_semantic_tokens *out, cui_theme *theme,
                                                   cui_theme_import_result *report) {
    cui_theme_import_result r = {.error = CUI_THEME_REGISTRY_INVALID};
    cui_theme_private_theme_json j;
    if (!out || !theme || !cui_theme_semantic_snapshot(base, &(cui_theme_semantic_tokens){0}))
        goto done;
    if (!cui_theme_private_json_parse(&j, source)) {
        r.error = j.error;
        r.byte_offset = j.cursor;
        goto done;
    }
    if (j.nodes[0].type != CUI_THEME_PRIVATE_J_OBJECT)
        goto done;
    size_t tokens = cui_theme_private_json_member(&j, 0, "tokens");
    /* Missing tokens uses the source serde default; unrelated roots reject. */
    if (j.nodes[0].count != (tokens == SIZE_MAX ? 0 : 2)) {
        r.error = CUI_THEME_REGISTRY_UNSUPPORTED;
        goto done;
    }
    cui_theme_semantic_tokens s;
    cui_theme_semantic_snapshot(base, &s);
    if (tokens != SIZE_MAX) {
        if (j.nodes[tokens].type != CUI_THEME_PRIVATE_J_OBJECT)
            goto done;
        for (size_t i = tokens + 1; i < j.nodes[tokens].next;) {
            char key[256];
            cui_theme_private_json_string(&j, i, key, sizeof key);
            size_t v = j.nodes[i].next;
            if (!strcmp(key, "colors"))
                r.error = cui_theme_private_import_fields(
                    &j, v, &s.colors, cui_theme_private_semantic_colors, 17, true);
            else if (!strcmp(key, "radius"))
                r.error = cui_theme_private_import_fields(
                    &j, v, &s.radius, cui_theme_private_semantic_radius, 6, false);
            else if (!strcmp(key, "spacing"))
                r.error = cui_theme_private_import_fields(
                    &j, v, &s.spacing, cui_theme_private_semantic_spacing, 7, false);
            else if (!strcmp(key, "typography"))
                r.error = cui_theme_private_import_typography(&j, v, &s.typography);
            else if (!strcmp(key, "shadow")) {
                if (j.nodes[v].type != CUI_THEME_PRIVATE_J_OBJECT) {
                    r.error = CUI_THEME_REGISTRY_INVALID;
                    goto done;
                }
                r.error = CUI_THEME_REGISTRY_OK;
                for (size_t k = v + 1; k < j.nodes[v].next;) {
                    char name[256];
                    cui_theme_private_json_string(&j, k, name, sizeof name);
                    size_t value = j.nodes[k].next;
                    cui_theme_shadows *dest = !strcmp(name, "sm")   ? &s.shadow.sm
                                              : !strcmp(name, "md") ? &s.shadow.md
                                              : !strcmp(name, "lg") ? &s.shadow.lg
                                                                    : NULL;
                    if (!dest) {
                        r.error = CUI_THEME_REGISTRY_UNSUPPORTED;
                        break;
                    }
                    r.error = cui_theme_private_import_shadow(&j, value, dest);
                    if (r.error)
                        break;
                    k = j.nodes[value].next;
                }
            } else
                r.error = CUI_THEME_REGISTRY_UNSUPPORTED;
            if (r.error) {
                r.byte_offset = j.nodes[v].start;
                goto done;
            }
            i = j.nodes[v].next;
        }
    }
    cui_theme projected = *base;
    if (!cui_theme_private_tokens_valid(&s) || !cui_theme_semantic_apply(&projected, &s)) {
        r.error = CUI_THEME_REGISTRY_INVALID;
        goto done;
    }
    *out = s;
    *theme = projected;
    r.error = CUI_THEME_REGISTRY_OK;
done:
    if (report)
        *report = r;
    return r.error;
}
#include "cui_theme_equal.h"
static bool cui_theme_private_registry_valid(const cui_theme_registry *r) {
    return r && r->entries && r->capacity && r->capacity <= CUI_THEME_REGISTRY_CAPACITY &&
           r->count <= r->capacity;
}
static bool cui_theme_private_entry_valid(const cui_theme_registry_entry *e) {
    return e && cui_theme_private_fixed_string(e->name, sizeof e->name, false) &&
           cui_theme_private_tokens_valid(&e->semantic) && cui_theme_motion_valid(&e->motion) &&
           cui_theme_private_bounded(e->theme.font_size, .01f, 4096) &&
           cui_theme_private_bounded(e->theme.radius, 0, 1e6f) &&
           cui_theme_private_bounded(e->theme.radius_lg, 0, 1e6f);
}
static bool cui_theme_private_name_equal(cui_str name, const char *fixed) {
    return name.data && name.length == strlen(fixed) && !memcmp(name.data, fixed, name.length);
}
const cui_theme_registry_entry *cui_theme_registry_find(const cui_theme_registry *r, cui_str name) {
    if (!cui_theme_private_registry_valid(r) || !name.data || !name.length ||
        name.length >= CUI_THEME_NAME_CAPACITY ||
        !cui_theme_private_theme_utf8(name.data, name.length))
        return NULL;
    for (size_t i = 0; i < r->count; i++)
        if (cui_theme_private_name_equal(name, r->entries[i].name))
            return &r->entries[i];
    return NULL;
}
static unsigned cui_theme_private_ascii_fold(unsigned c) {
    return c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c;
}
static int cui_theme_private_entry_order(const cui_theme_registry_entry *a,
                                         const cui_theme_registry_entry *b) {
    if (a->is_default != b->is_default)
        return a->is_default ? -1 : 1;
    if (a->theme.dark != b->theme.dark)
        return a->theme.dark ? 1 : -1;
    const unsigned char *x = (const unsigned char *)a->name, *y = (const unsigned char *)b->name;
    while (*x && *y) {
        unsigned l = cui_theme_private_ascii_fold(*x++), r = cui_theme_private_ascii_fold(*y++);
        if (l != r)
            return l < r ? -1 : 1;
    }
    if (*x != *y)
        return *x ? 1 : -1;
    return strcmp(a->name, b->name);
}
const cui_theme_registry_entry *cui_theme_registry_sorted(const cui_theme_registry *r,
                                                          size_t index) {
    if (!cui_theme_private_registry_valid(r) || index >= r->count)
        return NULL;
    for (size_t i = 0; i < r->count; i++) {
        size_t rank = 0;
        for (size_t k = 0; k < r->count; k++)
            if (cui_theme_private_entry_order(&r->entries[k], &r->entries[i]) < 0)
                rank++;
        if (rank == index)
            return &r->entries[i];
    }
    return NULL;
}
bool cui_theme_registry_reset(cui_theme_registry *r) {
    if (!cui_theme_private_registry_valid(r) || r->revision == UINT64_MAX ||
        (r->seed_presets && r->capacity < cui_theme_preset_count()))
        return false;
    size_t count = 0;
    if (r->seed_presets)
        for (size_t i = 0; i < cui_theme_preset_count(); i++) {
            cui_theme_registry_entry entry = {0};
            cui_str name = cui_theme_preset_name(i);
            if (name.length >= sizeof entry.name)
                return false;
            memcpy(entry.name, name.data, name.length);
            entry.name[name.length] = 0;
            cui_theme_preset_get(i, &entry.theme);
            cui_theme_semantic_snapshot(&entry.theme, &entry.semantic);
            entry.motion = cui_theme_motion_default();
            entry.is_default =
                !strcmp(entry.name, "Default Light") || !strcmp(entry.name, "Default Dark");
            r->entries[count++] = entry;
        }
    r->count = count;
    r->revision++;
    return true;
}
bool cui_theme_registry_init(cui_theme_registry *out, cui_theme_registry_entry *entries,
                             size_t capacity, bool seed) {
    if (!out || !entries || !capacity || capacity > CUI_THEME_REGISTRY_CAPACITY ||
        cui_theme_private_memory_overlap(out, sizeof *out, entries, capacity * sizeof *entries) ||
        (seed && capacity < cui_theme_preset_count()))
        return false;
    cui_theme_registry r = {.entries = entries, .capacity = capacity, .seed_presets = seed};
    if (!cui_theme_registry_reset(&r))
        return false;
    *out = r;
    return true;
}
cui_theme_registry_error cui_theme_registry_put(cui_theme_registry *r,
                                                const cui_theme_registry_entry *e, bool replace) {
    if (!cui_theme_private_registry_valid(r) || !cui_theme_private_entry_valid(e) ||
        r->revision == UINT64_MAX)
        return CUI_THEME_REGISTRY_INVALID;
    const cui_theme_registry_entry *old =
        cui_theme_registry_find(r, (cui_str){e->name, strlen(e->name)});
    if (old && !replace)
        return CUI_THEME_REGISTRY_OK;
    if (!old && r->count == r->capacity)
        return CUI_THEME_REGISTRY_CAPACITY_ERROR;
    size_t index = old ? (size_t)(old - r->entries) : r->count++;
    cui_theme_registry_entry coherent = *e;
    coherent.semantic.colors = cui_theme_private_colors_of(&e->theme);
    coherent.semantic.colors.selection = e->semantic.colors.selection;
    coherent.semantic.radius.md = e->theme.radius;
    coherent.semantic.radius.lg = e->theme.radius_lg;
    coherent.semantic.typography.md.size = e->theme.font_size;
    r->entries[index] = coherent;
    r->revision++;
    return CUI_THEME_REGISTRY_OK;
}
#include "cui_theme_legacy_fields.inc"
static cui_theme_registry_error cui_theme_private_unsupported(cui_theme_import_options o,
                                                              cui_theme_import_result *r) {
    r->unsupported_fields++;
    r->partial = true;
    return o.allow_unsupported ? CUI_THEME_REGISTRY_OK : CUI_THEME_REGISTRY_UNSUPPORTED;
}
static cui_theme_registry_error
cui_theme_private_legacy_entry(const cui_theme_private_theme_json *j, size_t object,
                               cui_theme_import_options options, cui_theme_import_result *report,
                               cui_theme_registry_entry *out) {
    if (j->nodes[object].type != CUI_THEME_PRIVATE_J_OBJECT)
        return CUI_THEME_REGISTRY_INVALID;
    cui_theme_registry_entry e = {.motion = cui_theme_motion_default()};
    size_t mode = cui_theme_private_json_member(j, object, "mode");
    char value[128];
    bool dark = false;
    if (mode != SIZE_MAX) {
        if (!cui_theme_private_json_string(j, mode, value, sizeof value))
            return CUI_THEME_REGISTRY_INVALID;
        if (!strcmp(value, "dark"))
            dark = true;
        else if (strcmp(value, "light"))
            return CUI_THEME_REGISTRY_INVALID;
    }
    e.theme = dark ? cui_theme_dark() : cui_theme_light();
    /* Defaults use the existing frozen source palette adaptations. */
    for (size_t i = 0; i < cui_theme_preset_count(); i++)
        if (cui_theme_private_name_equal(
                (cui_str){dark ? "Default Dark" : "Default Light", dark ? 12 : 13},
                cui_theme_preset_name(i).data)) {
            cui_theme_preset_get(i, &e.theme);
            break;
        }
    cui_theme_semantic_snapshot(&e.theme, &e.semantic);
    size_t name = cui_theme_private_json_member(j, object, "name");
    if (name == SIZE_MAX || j->nodes[name].type != CUI_THEME_PRIVATE_J_STRING)
        return CUI_THEME_REGISTRY_INVALID;
    if (!cui_theme_private_json_string(j, name, e.name, sizeof e.name))
        return CUI_THEME_REGISTRY_CAPACITY_ERROR;
    if (!e.name[0])
        return CUI_THEME_REGISTRY_INVALID;
    for (size_t i = object + 1; i < j->nodes[object].next;) {
        char key[256];
        cui_theme_private_json_string(j, i, key, sizeof key);
        size_t v = j->nodes[i].next;
        cui_theme_registry_error error = CUI_THEME_REGISTRY_OK;
        if (!strcmp(key, "name") || !strcmp(key, "mode")) {
        } else if (!strcmp(key, "is_default")) {
            if (!cui_theme_private_json_bool(j, v, &e.is_default))
                return CUI_THEME_REGISTRY_INVALID;
        } else if (!strcmp(key, "colors")) {
            if (j->nodes[v].type != CUI_THEME_PRIVATE_J_OBJECT)
                return CUI_THEME_REGISTRY_INVALID;
            for (size_t k = v + 1; k < j->nodes[v].next;) {
                char token[256];
                cui_theme_private_json_string(j, k, token, sizeof token);
                size_t color = j->nodes[k].next;
                const cui_theme_private_theme_field *f =
                    cui_theme_private_field_find(cui_theme_private_legacy_colors,
                                                 sizeof cui_theme_private_legacy_colors /
                                                     sizeof *cui_theme_private_legacy_colors,
                                                 token);
                if (!f)
                    error = cui_theme_private_unsupported(options, report);
                else if (j->nodes[color].type != CUI_THEME_PRIVATE_J_NULL) {
                    cui_color c;
                    if (!cui_theme_private_json_string(j, color, value, sizeof value))
                        return CUI_THEME_REGISTRY_INVALID;
                    if (!cui_theme_color_parse((cui_str){value, strlen(value)}, &c)) {
                        if (!strncmp(value, "linear-gradient(", 16))
                            error = cui_theme_private_unsupported(options, report);
                        else
                            return CUI_THEME_REGISTRY_INVALID;
                    } else
                        memcpy((char *)&e.theme + f->offset, &c, sizeof c);
                }
                if (error)
                    return error;
                k = j->nodes[color].next;
            }
        } else if (!strcmp(key, "font.size") || !strcmp(key, "mono_font.size") ||
                   !strcmp(key, "radius") || !strcmp(key, "radius.lg")) {
            if (j->nodes[v].type != CUI_THEME_PRIVATE_J_NULL) {
                float n;
                if (!cui_theme_private_json_number(j, v, &n))
                    return CUI_THEME_REGISTRY_INVALID;
                if (!strcmp(key, "font.size"))
                    e.theme.font_size = n;
                else if (!strcmp(key, "mono_font.size"))
                    e.semantic.typography.mono_md.size = n;
                else {
                    if (n < 0 || floorf(n) != n)
                        return CUI_THEME_REGISTRY_INVALID;
                    if (!strcmp(key, "radius"))
                        e.theme.radius = n;
                    else
                        e.theme.radius_lg = n;
                }
            }
        } else if (!strcmp(key, "font.family") || !strcmp(key, "mono_font.family")) {
            if (j->nodes[v].type != CUI_THEME_PRIVATE_J_NULL) {
                if (j->nodes[v].type != CUI_THEME_PRIVATE_J_STRING)
                    return CUI_THEME_REGISTRY_INVALID;
                if (!cui_theme_private_json_string(j, v,
                                                   !strcmp(key, "font.family")
                                                       ? e.semantic.typography.sans
                                                       : e.semantic.typography.mono,
                                                   CUI_THEME_NAME_CAPACITY))
                    return CUI_THEME_REGISTRY_CAPACITY_ERROR;
            }
        } else if (!strcmp(key, "shadow")) {
            if (j->nodes[v].type != CUI_THEME_PRIVATE_J_NULL) {
                bool enabled;
                if (!cui_theme_private_json_bool(j, v, &enabled))
                    return CUI_THEME_REGISTRY_INVALID;
                e.semantic.shadow = (cui_theme_semantic_shadow){0};
                if (enabled)
                    e.semantic.shadow = cui_theme_private_source_shadows();
            }
        } else if (!strcmp(key, "highlight") && j->nodes[v].type == CUI_THEME_PRIVATE_J_NULL) {
        } else
            error = cui_theme_private_unsupported(options, report);
        if (error)
            return error;
        i = j->nodes[v].next;
    }
    /* Refresh projected generic tokens, retaining owned config-only details. */
    cui_theme_semantic_tokens refreshed;
    if (!cui_theme_semantic_snapshot(&e.theme, &refreshed))
        return CUI_THEME_REGISTRY_INVALID;
    refreshed.typography = e.semantic.typography;
    refreshed.typography.md.size = e.theme.font_size;
    refreshed.shadow = e.semantic.shadow;
    e.semantic = refreshed;
    if (!cui_theme_private_entry_valid(&e))
        return CUI_THEME_REGISTRY_INVALID;
    *out = e;
    return CUI_THEME_REGISTRY_OK;
}
cui_theme_registry_error cui_theme_registry_import(cui_theme_registry *r, cui_str source,
                                                   cui_theme_import_options options,
                                                   cui_theme_registry_entry *scratch,
                                                   size_t capacity,
                                                   cui_theme_import_result *report) {
    cui_theme_import_result result = {.error = CUI_THEME_REGISTRY_INVALID};
    cui_theme_private_theme_json j;
    if (report && cui_theme_private_registry_valid(r) &&
        (cui_theme_private_memory_overlap(report, sizeof *report, r, sizeof *r) ||
         cui_theme_private_memory_overlap(report, sizeof *report, r->entries,
                                          r->capacity * sizeof *r->entries) ||
         (scratch && capacity <= CUI_THEME_REGISTRY_CAPACITY &&
          cui_theme_private_memory_overlap(report, sizeof *report, scratch,
                                           capacity * sizeof *scratch)) ||
         (source.data &&
          cui_theme_private_memory_overlap(report, sizeof *report, source.data, source.length))))
        return CUI_THEME_REGISTRY_INVALID;
    if (!cui_theme_private_registry_valid(r) || !scratch || !capacity ||
        capacity > CUI_THEME_REGISTRY_CAPACITY || r->revision == UINT64_MAX ||
        cui_theme_private_memory_overlap(r, sizeof *r, scratch, capacity * sizeof *scratch) ||
        cui_theme_private_memory_overlap(r->entries, r->capacity * sizeof *r->entries, scratch,
                                         capacity * sizeof *scratch) ||
        (source.data && (cui_theme_private_memory_overlap(source.data, source.length, scratch,
                                                          capacity * sizeof *scratch) ||
                         cui_theme_private_memory_overlap(source.data, source.length, r->entries,
                                                          r->capacity * sizeof *r->entries))))
        goto done;
    if (!cui_theme_private_json_parse(&j, source)) {
        result.error = j.error;
        result.byte_offset = j.cursor;
        goto done;
    }
    if (j.nodes[0].type != CUI_THEME_PRIVATE_J_OBJECT)
        goto done;
    size_t themes = cui_theme_private_json_member(&j, 0, "themes");
    if (themes != SIZE_MAX && j.nodes[themes].type != CUI_THEME_PRIVATE_J_ARRAY)
        goto done;
    for (size_t i = 1; i < j.nodes[0].next;) {
        char name[256];
        cui_theme_private_json_string(&j, i, name, sizeof name);
        size_t v = j.nodes[i].next;
        if (strcmp(name, "themes") && strcmp(name, "name") && strcmp(name, "author") &&
            strcmp(name, "url") && strcmp(name, "$schema") && strcmp(name, "_ref")) {
            result.error = cui_theme_private_unsupported(options, &result);
            if (result.error)
                goto done;
        } else if (strcmp(name, "themes") && j.nodes[v].type != CUI_THEME_PRIVATE_J_STRING &&
                   j.nodes[v].type != CUI_THEME_PRIVATE_J_NULL) {
            result.error = CUI_THEME_REGISTRY_INVALID;
            goto done;
        }
        i = j.nodes[v].next;
    }
    if (capacity < r->count) {
        result.error = CUI_THEME_REGISTRY_CAPACITY_ERROR;
        goto done;
    }
    memcpy(scratch, r->entries, r->count * sizeof *scratch);
    cui_theme_registry temporary = *r;
    temporary.entries = scratch;
    temporary.capacity = capacity;
    size_t theme_start = themes == SIZE_MAX ? j.nodes[0].next : themes + 1,
           theme_end = themes == SIZE_MAX ? j.nodes[0].next : j.nodes[themes].next;
    for (size_t i = theme_start; i < theme_end; i = j.nodes[i].next) {
        cui_theme_registry_entry entry;
        result.error = cui_theme_private_legacy_entry(&j, i, options, &result, &entry);
        if (result.error) {
            result.byte_offset = j.nodes[i].start;
            goto done;
        }
        const cui_theme_registry_entry *old =
            cui_theme_registry_find(&temporary, (cui_str){entry.name, strlen(entry.name)});
        if (old && !options.replace_existing) {
            result.skipped++;
            continue;
        }
        if (!old && (temporary.count == r->capacity || temporary.count == capacity)) {
            result.error = CUI_THEME_REGISTRY_CAPACITY_ERROR;
            goto done;
        }
        /* Revision changes once for a complete batch; never wrap mid-parse. */
        size_t index = old ? (size_t)(old - temporary.entries) : temporary.count++;
        temporary.entries[index] = entry;
        if (old)
            result.replaced++;
        else
            result.added++;
    }
    if (result.added || result.replaced) {
        memcpy(r->entries, scratch, temporary.count * sizeof *scratch);
        r->count = temporary.count;
        r->revision++;
    }
    result.error = CUI_THEME_REGISTRY_OK;
done:
    if (result.error) {
        result.added = result.replaced = result.skipped = 0;
        result.partial = false;
    }
    if (report)
        *report = result;
    return result.error;
}
cui_theme_registry_error cui_theme_host_update(cui_theme_host_state *state,
                                               const cui_theme_registry *r,
                                               cui_theme_selection selection,
                                               cui_theme_host_preferences preferences, float rem) {
    if (!state || !cui_theme_private_registry_valid(r) ||
        selection.mode < CUI_THEME_FOLLOW_SYSTEM || selection.mode > CUI_THEME_FORCE_DARK ||
        (state->ready && preferences.revision < state->preferences.revision))
        return CUI_THEME_REGISTRY_INVALID;
    bool dark = selection.mode == CUI_THEME_FORCE_DARK ||
                (selection.mode == CUI_THEME_FOLLOW_SYSTEM && preferences.dark);
    const cui_theme_registry_entry *entry =
        cui_theme_registry_find(r, dark ? selection.dark_name : selection.light_name);
    if (!entry)
        return CUI_THEME_REGISTRY_MISSING;
    cui_theme_host_state next = {.ready = true,
                                 .registry_revision = r->revision,
                                 .preferences = preferences,
                                 .theme = entry->theme,
                                 .semantic = entry->semantic};
    if (preferences.high_contrast) {
        next.theme = cui_theme_high_contrast();
        cui_theme_semantic_snapshot(&next.theme, &next.semantic);
    }
    next.theme.scrollbar.visibility =
        preferences.auto_hide_scrollbars ? CUI_SCROLLBAR_AUTO_HIDE : CUI_SCROLLBAR_VISIBLE;
    next.theme.scrollbar.reduced_motion = preferences.reduced_motion;
    if (!cui_theme_motion_project(&entry->motion, rem, preferences.reduced_motion, &next.motion))
        return CUI_THEME_REGISTRY_INVALID;
    next.changed = !state->ready || !cui_theme_private_theme_equal(&state->theme, &next.theme) ||
                   !cui_theme_private_semantic_equal(&state->semantic, &next.semantic);
    next.metrics_changed =
        !state->ready || state->theme.font_size != next.theme.font_size ||
        !cui_theme_private_typography_equal(&state->semantic.typography, &next.semantic.typography);
    next.motion_changed =
        !state->ready || !cui_theme_private_motion_equal(&state->motion, &next.motion);
    *state = next;
    return CUI_THEME_REGISTRY_OK;
}
