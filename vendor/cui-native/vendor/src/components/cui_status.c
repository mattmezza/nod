/* SPDX-License-Identifier: MIT */
#include "cui/cui_status.h"
#include "../base/cui_design_metrics.h"
#include "cui/cui_icon.h"
#include <math.h>
static cui_color cui_status_alpha(cui_color c, float opacity) {
    c.a = (uint8_t)(c.a * opacity);
    return c;
}
bool cui_tag_resolve(const cui_theme *t, cui_tag_options o, bool hovered, cui_tag_style *out) {
    if (!t || !out || !cui_design_valid(o.design_size) || o.variant > CUI_TAG_PALETTE ||
        o.variant < CUI_TAG_SECONDARY || o.size > CUI_STATUS_LARGE || o.size < CUI_STATUS_MEDIUM ||
        !isfinite(o.radius) || o.radius < 0 || !isfinite(t->radius) || t->radius < 0 ||
        !isfinite(t->font_size) || t->font_size <= 0)
        return false;
    cui_color bg = t->secondary, fg = o.outline ? t->muted : t->secondary_foreground,
              border = t->border;
    switch (o.variant) {
    case CUI_TAG_PRIMARY:
        bg = border = t->primary;
        fg = o.outline ? bg : t->on_primary;
        break;
    case CUI_TAG_DANGER:
        bg = border = t->danger;
        fg = o.outline ? bg : t->on_danger;
        break;
    case CUI_TAG_SUCCESS:
        bg = border = t->success;
        fg = o.outline ? bg : t->on_success;
        break;
    case CUI_TAG_WARNING:
        bg = border = t->warning;
        fg = o.outline ? bg : t->on_warning;
        break;
    case CUI_TAG_INFO:
        bg = border = t->info;
        fg = o.outline ? bg : t->on_info;
        break;
    case CUI_TAG_CUSTOM:
        bg = o.background;
        fg = o.foreground;
        border = o.border;
        break;
    case CUI_TAG_PALETTE:
        bg = o.palette.dark ? cui_status_alpha(o.palette.scale950, .5f) : o.palette.scale50;
        border = o.palette.dark ? cui_status_alpha(o.palette.scale800, .5f) : o.palette.scale200;
        fg = o.palette.dark ? o.palette.scale300 : o.palette.scale600;
        break;
    default:
        break;
    }
    bool small = o.size == CUI_STATUS_SMALL || o.size == CUI_STATUS_XSMALL;
    /* Reference rem typography: 10/12/14px at the standard 16px root. */
    float root = t->font_size / 0.875f;
    cui_tag_style s = {.background = o.outline ? (cui_color){0} : bg,
                       .foreground = fg,
                       .border = border,
                       .font_size = root * (small                        ? .625f
                                            : o.size == CUI_STATUS_LARGE ? .875f
                                                                         : .75f),
                       .padding_x = small ? 6 : 10,
                       .padding_y = small ? 2 : 4,
                       .radius = o.pill            ? 100000
                                 : o.custom_radius ? o.radius
                                 : small           ? t->radius / 2
                                                   : t->radius};
    if (o.design_size) {
        cui_design_metrics d;
        if (!cui_design_resolve(o.design_size, t->font_size / 16, &d))
            return false;
        s.font_size = d.font_size;
        s.padding_y = d.padding_y;
        s.padding_x = fmaxf(d.padding_x, d.padding_y);
    }
    if (!isfinite(s.font_size) || !isfinite(s.radius))
        return false;
    if (hovered) {
        s.background = cui_status_alpha(s.background, .9f);
        s.foreground = cui_status_alpha(s.foreground, .9f);
        s.border = cui_status_alpha(s.border, .9f);
    }
    *out = s;
    return true;
}

/* Six required scales from frozen component/theme/default-colors.json. */
bool cui_tag_named_palette(cui_status_color name, bool dark, cui_tag_palette *out) {
    static const cui_color colors[21][6] = {{{255, 255, 255, 255},
                                             {255, 255, 255, 255},
                                             {255, 255, 255, 255},
                                             {255, 255, 255, 255},
                                             {255, 255, 255, 255},
                                             {255, 255, 255, 255}},
                                            {{0, 0, 0, 255},
                                             {0, 0, 0, 255},
                                             {0, 0, 0, 255},
                                             {0, 0, 0, 255},
                                             {0, 0, 0, 255},
                                             {0, 0, 0, 255}},
                                            {{250, 250, 250, 255},
                                             {229, 229, 229, 255},
                                             {212, 212, 212, 255},
                                             {82, 82, 82, 255},
                                             {38, 38, 38, 255},
                                             {10, 10, 10, 255}},
                                            {{249, 250, 251, 255},
                                             {229, 231, 235, 255},
                                             {209, 213, 219, 255},
                                             {75, 85, 99, 255},
                                             {31, 41, 55, 255},
                                             {3, 7, 18, 255}},
                                            {{254, 242, 242, 255},
                                             {254, 202, 202, 255},
                                             {252, 165, 165, 255},
                                             {220, 38, 38, 255},
                                             {153, 27, 27, 255},
                                             {69, 10, 10, 255}},
                                            {{255, 247, 237, 255},
                                             {254, 215, 170, 255},
                                             {253, 186, 116, 255},
                                             {234, 88, 12, 255},
                                             {154, 52, 18, 255},
                                             {67, 20, 7, 255}},
                                            {{255, 251, 235, 255},
                                             {253, 230, 138, 255},
                                             {252, 211, 77, 255},
                                             {217, 119, 6, 255},
                                             {146, 64, 14, 255},
                                             {69, 26, 3, 255}},
                                            {{254, 252, 232, 255},
                                             {254, 240, 138, 255},
                                             {253, 224, 71, 255},
                                             {202, 138, 4, 255},
                                             {133, 77, 14, 255},
                                             {66, 32, 6, 255}},
                                            {{247, 254, 231, 255},
                                             {217, 249, 157, 255},
                                             {190, 242, 100, 255},
                                             {101, 163, 13, 255},
                                             {63, 98, 18, 255},
                                             {26, 46, 5, 255}},
                                            {{240, 253, 244, 255},
                                             {187, 247, 208, 255},
                                             {134, 239, 172, 255},
                                             {22, 163, 74, 255},
                                             {22, 101, 52, 255},
                                             {5, 46, 22, 255}},
                                            {{236, 253, 245, 255},
                                             {167, 243, 208, 255},
                                             {110, 231, 183, 255},
                                             {5, 150, 105, 255},
                                             {6, 95, 70, 255},
                                             {2, 44, 34, 255}},
                                            {{240, 253, 250, 255},
                                             {153, 246, 228, 255},
                                             {94, 234, 212, 255},
                                             {13, 148, 136, 255},
                                             {17, 94, 89, 255},
                                             {4, 47, 46, 255}},
                                            {{236, 254, 255, 255},
                                             {165, 243, 252, 255},
                                             {103, 232, 249, 255},
                                             {8, 145, 178, 255},
                                             {21, 94, 117, 255},
                                             {8, 51, 68, 255}},
                                            {{240, 249, 255, 255},
                                             {186, 230, 253, 255},
                                             {125, 211, 252, 255},
                                             {2, 132, 199, 255},
                                             {7, 89, 133, 255},
                                             {8, 47, 73, 255}},
                                            {{239, 246, 255, 255},
                                             {191, 219, 254, 255},
                                             {147, 197, 253, 255},
                                             {37, 99, 235, 255},
                                             {30, 64, 175, 255},
                                             {23, 37, 84, 255}},
                                            {{238, 242, 255, 255},
                                             {199, 210, 254, 255},
                                             {165, 180, 252, 255},
                                             {79, 70, 229, 255},
                                             {55, 48, 163, 255},
                                             {30, 27, 75, 255}},
                                            {{245, 243, 255, 255},
                                             {221, 214, 254, 255},
                                             {196, 181, 253, 255},
                                             {124, 58, 237, 255},
                                             {91, 33, 182, 255},
                                             {30, 27, 75, 255}},
                                            {{250, 245, 255, 255},
                                             {233, 213, 255, 255},
                                             {216, 180, 254, 255},
                                             {147, 51, 234, 255},
                                             {107, 33, 168, 255},
                                             {59, 7, 100, 255}},
                                            {{253, 244, 255, 255},
                                             {245, 208, 254, 255},
                                             {240, 171, 252, 255},
                                             {192, 38, 211, 255},
                                             {134, 25, 143, 255},
                                             {74, 4, 78, 255}},
                                            {{253, 242, 248, 255},
                                             {251, 207, 232, 255},
                                             {249, 168, 212, 255},
                                             {219, 39, 119, 255},
                                             {157, 23, 77, 255},
                                             {80, 7, 36, 255}},
                                            {{255, 241, 242, 255},
                                             {254, 205, 211, 255},
                                             {253, 164, 175, 255},
                                             {225, 29, 72, 255},
                                             {159, 18, 57, 255},
                                             {76, 5, 25, 255}}};
    if (!out || name < CUI_STATUS_COLOR_WHITE || name > CUI_STATUS_COLOR_ROSE)
        return false;
    *out = (cui_tag_palette){colors[name][0],
                             colors[name][1],
                             colors[name][2],
                             colors[name][3],
                             colors[name][4],
                             colors[name][5],
                             dark};
    return true;
}

bool cui_begin_tag(cui_context *ui, cui_id id, const cui_theme *t, cui_tag_options o,
                   cui_tag_style *out) {
    cui_tag_style s;
    if (ui && !id) {
        cui_diagnostic diagnostic = {.error = CUI_ERROR_INVALID_ACTION,
                                     .detail = CUI_DIAGNOSTIC_ZERO_ID,
                                     .operation = "cui_begin_tag",
                                     .id = id};
        cui_report_diagnostic(ui, &diagnostic);
        return false;
    }
    if (!ui || !cui_tag_resolve(t, o, cui_hovered(ui, id), &s))
        return false;
    /* Separate horizontal/vertical padding using inner composition, since the
     * core box exposes uniform padding. */
    cui_begin_box(ui, id,
                  (cui_box){.row = true,
                            .padding = s.padding_y,
                            .radius = s.radius,
                            .background = s.background,
                            .border = s.border,
                            .border_width = 1,
                            .align_y = CUI_ALIGN_CENTER});
    cui_begin_box(ui, id ^ UINT64_C(0xe937dbc482156a30),
                  (cui_box){.width = s.padding_x - s.padding_y, .height = 1});
    cui_end_box(ui);
    cui_begin_box(ui, id ^ UINT64_C(0xa037dbc482156a30),
                  (cui_box){.row = true, .align_y = CUI_ALIGN_CENTER});
    if (out)
        *out = s;
    return true;
}
void cui_end_tag(cui_context *ui, cui_id id, const cui_tag_style *s) {
    if (ui && s) {
        cui_end_box(ui);
        cui_begin_box(ui, id ^ UINT64_C(0xb037dbc482156a30),
                      (cui_box){.width = s->padding_x - s->padding_y, .height = 1});
        cui_end_box(ui);
        cui_end_box(ui);
    }
}
bool cui_tag(cui_context *ui, cui_id id, const cui_theme *t, cui_str text, cui_tag_options o) {
    cui_tag_style s;
    if (!cui_begin_tag(ui, id, t, o, &s))
        return false;
    cui_interact(ui, id, CUI_ROLE_LABEL, text, false, false, 0);
    cui_set_tab_stop(ui, id, false);
    cui_text(ui, text, s.font_size, s.foreground);
    cui_end_tag(ui, id, &s);
    return true;
}
#include "cui/cui_content.h"
static cui_id cui_status_child_id(cui_id id, size_t i) {
    return id ^ (UINT64_C(0x95d7e38a461bc203) + UINT64_C(0x9e3779b97f4a7c15) * (i + 1));
}
static void cui_status_spacer(cui_context *ui, cui_id id, float width) {
    cui_begin_box(ui, id, (cui_box){.width = width, .height = 1, .fixed_width = true});
    cui_end_box(ui);
}
static void cui_status_shimmer(cui_context *ui, cui_id id, cui_rect b, cui_str text, float font,
                               cui_color color, float phase, float spread, bool absolute,
                               bool dark) {
    if (b.w <= 0 || b.h <= 0)
        return;
    float half = absolute ? fmaxf(1, spread) : b.w * fmaxf(.05f, fminf(spread, 1)),
          padding = half / b.w + .05f, center = (phase * (1 + 2 * padding) - padding) * b.w;
    color.a = (uint8_t)(color.a * (1 - powf(1 - (dark ? .6f : .75f), 1.0f / 12)));
    for (size_t layer = 0; layer < 12; layer++) {
        float radius = half * (1 - (float)layer / 12), left = fmaxf(0, center - radius),
              right = fminf(b.w, center + radius);
        if (right <= left)
            continue;
        cui_begin_portal(ui, cui_status_child_id(id, layer),
                         (cui_box){.width = right - left,
                                   .height = b.h,
                                   .fixed_width = true,
                                   .fixed_height = true,
                                   .clip = true,
                                   .offset_x = -left,
                                   .inert = true},
                         (cui_rect){b.x + left, b.y, right - left, b.h}, 2);
        cui_begin_box(
            ui, cui_status_child_id(id, layer + 20),
            (cui_box){.width = b.w, .height = b.h, .fixed_width = true, .fixed_height = true});
        cui_text(ui, text, font, color);
        cui_end_box(ui);
        cui_end_box(ui);
    }
}
static float cui_status_lightness(cui_color c) {
    float lo = fminf(c.r, fminf(c.g, c.b)), hi = fmaxf(c.r, fmaxf(c.g, c.b));
    return (lo + hi) / 510;
}
static cui_color cui_status_highlight(const cui_theme *t, cui_color text, bool dark) {
    cui_color target = dark ? t->text : t->background;
    if (fabsf(cui_status_lightness(target) - cui_status_lightness(text)) < .1f)
        target = dark ? t->background : t->text;
    return cui_color_mix_oklab(text, target, .8f);
}
bool cui_shimmer_text(cui_context *ui, cui_id id, const cui_theme *t, cui_shimmer_state *state,
                      cui_str text, cui_shimmer_options o) {
    cui_shimmer_plan plan;
    if (!ui || !id || !t || !state || (text.length && !text.data) || !cui_base_shimmer(o, &plan))
        return false;
    float font = o.font_size > 0 ? o.font_size : t->font_size;
    if (!isfinite(font) || font <= 0)
        return false;
    cui_color color = o.custom_color ? o.color : t->text;
    if (plan.animated && (!state->initialized || state->last_phase != plan.phase))
        cui_invalidate(ui);
    state->initialized = true;
    state->last_phase = plan.phase;
    cui_size size = cui_measure_text(ui, text, font);
    cui_begin_box(ui, id, (cui_box){.max_width = o.max_width, .clip = o.max_width > 0});
    if (o.role != CUI_ROLE_NONE) {
        cui_interact(ui, id, o.role, text, false, false, 0);
        cui_set_tab_stop(ui, id, false);
    }
    cui_id glyphs = cui_status_child_id(id, 500);
    cui_begin_box(
        ui, glyphs,
        (cui_box){
            .width = size.width, .height = size.height, .fixed_width = true, .fixed_height = true});
    cui_text(ui, text, font, color);
    cui_track_bounds(ui, glyphs, &state->text_bounds);
    cui_end_box(ui);
    if (!o.reduced_motion && text.length) {
        bool dark = t->background.r + t->background.g + t->background.b < 384;
        cui_color highlight =
            o.custom_highlight ? o.highlight : cui_status_highlight(t, color, dark);
        cui_status_shimmer(ui, cui_status_child_id(id, 501), state->text_bounds, text, font,
                           highlight, plan.phase, o.spread, o.absolute_spread, dark);
    }
    cui_end_box(ui);
    return true;
}
bool cui_marker(cui_context *ui, cui_id id, const cui_theme *t, cui_marker_state *state,
                const cui_marker_child *children, size_t count, cui_marker_options o) {
    cui_marker_plan p;
    if (!ui || !t || !state || !cui_base_marker(o, children, count, &p))
        return false;
    if (p.animated && (!state->initialized || state->last_phase != p.phase))
        cui_invalidate(ui);
    state->initialized = true;
    state->last_phase = p.phase;
    float widths[CUI_MARKER_MAX_CHILDREN], total = p.spinner ? 16 : 0, font = t->font_size;
    size_t slots = count + (p.spinner ? 1 : 0);
    for (size_t i = 0; i < count; i++) {
        widths[i] = children[i].kind == CUI_MARKER_TEXT
                        ? cui_measure_text(ui, children[i].text, font).width
                    : children[i].width > 0               ? children[i].width
                    : children[i].kind == CUI_MARKER_ICON ? 16
                                                          : 0;
        total += widths[i];
    }
    if (slots)
        total += 8 * (slots - 1);
    float remaining = fmaxf(0, o.width - total),
          line = o.variant == CUI_MARKER_SEPARATOR
                     ? remaining / (p.alignment == CUI_MARKER_ALIGN_CENTER ? 2 : 1)
                     : 0;
    cui_begin_box(ui, id,
                  (cui_box){.width = o.width,
                            .border = t->border,
                            .custom_border_edges = o.variant == CUI_MARKER_BORDER,
                            .border_edges = {.bottom = o.variant == CUI_MARKER_BORDER ? 1 : 0}});
    cui_begin_box(ui, cui_status_child_id(id, 103),
                  (cui_box){.row = true,
                            .width = o.width,
                            .gap = 8,
                            .align_y = CUI_ALIGN_CENTER,
                            .height = slots ? 0 : 16});
    if (o.role != CUI_ROLE_NONE) {
        cui_interact(ui, id, o.role, o.name, false, false, 0);
        cui_set_tab_stop(ui, id, false);
    }
    if (o.variant == CUI_MARKER_SEPARATOR && p.alignment != CUI_MARKER_ALIGN_START) {
        cui_begin_box(ui, cui_status_child_id(id, 100),
                      (cui_box){.width = fmaxf(0, line - 8),
                                .height = 1,
                                .fixed_width = true,
                                .background = t->border});
        cui_end_box(ui);
    } else if (o.variant != CUI_MARKER_SEPARATOR && p.alignment != CUI_MARKER_ALIGN_START)
        cui_status_spacer(
            ui, cui_status_child_id(id, 100),
            fmaxf(0, remaining / (p.alignment == CUI_MARKER_ALIGN_CENTER ? 2 : 1) - 8));
    if (p.spinner)
        cui_spinner(ui, cui_status_child_id(id, 101), t, 16, p.phase, o.reduced_motion);
    for (size_t i = 0; i < count; i++) {
        const cui_marker_child *c = &children[i];
        cui_id child = cui_status_child_id(id, i);
        cui_begin_box(ui, child,
                      (cui_box){.width = widths[i],
                                .fixed_width = widths[i] > 0,
                                .height = c->kind == CUI_MARKER_ICON ? 16 : 0,
                                .row = true,
                                .align_x = p.alignment == CUI_MARKER_ALIGN_END ? CUI_ALIGN_END
                                           : p.alignment == CUI_MARKER_ALIGN_CENTER
                                               ? CUI_ALIGN_CENTER
                                               : CUI_ALIGN_START,
                                .align_y = CUI_ALIGN_CENTER});
        if (c->kind == CUI_MARKER_TEXT) {
            cui_text(ui, c->text, font, t->muted);
            cui_track_bounds(ui, child, &state->text_bounds[i]);
        } else if (c->render)
            c->render(ui, cui_status_child_id(child, 49), c->user, font, t->muted,
                      c->kind == CUI_MARKER_CONTENT ? p.rich_opacity : 1);
        else if (c->text.length)
            cui_text(ui, c->text, font, t->muted);
        cui_end_box(ui);
        if (c->kind == CUI_MARKER_TEXT && o.loading && !o.reduced_motion &&
            o.loading_style == CUI_MARKER_SHIMMER) {
            bool dark = t->background.r + t->background.g + t->background.b < 384;
            cui_color highlight =
                o.custom_highlight ? o.highlight : cui_status_highlight(t, t->muted, dark);
            cui_status_shimmer(ui, cui_status_child_id(child, 50), state->text_bounds[i], c->text,
                               font, highlight, p.phase, o.shimmer_spread, o.absolute_spread, dark);
        }
    }
    if (o.variant == CUI_MARKER_SEPARATOR && p.alignment != CUI_MARKER_ALIGN_END) {
        cui_begin_box(ui, cui_status_child_id(id, 102),
                      (cui_box){.width = fmaxf(0, line - 8),
                                .height = 1,
                                .fixed_width = true,
                                .background = t->border});
        cui_end_box(ui);
    }
    if (o.variant != CUI_MARKER_SEPARATOR && p.alignment == CUI_MARKER_ALIGN_CENTER)
        cui_status_spacer(ui, cui_status_child_id(id, 102), fmaxf(0, remaining / 2 - 8));
    cui_end_box(ui);
    if (o.variant == CUI_MARKER_BORDER) {
        cui_begin_box(ui, cui_status_child_id(id, 104), (cui_box){.height = 8, .width = 1});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return true;
}
#include "cui/cui_components.h"
typedef struct {
    const cui_theme *theme;
    cui_id id;
} cui_status_close_icon;
static void cui_status_notification_close_content(cui_context *ui, void *user) {
    const cui_status_close_icon *icon = user;
    (void)cui_icon_named(ui, icon->id, icon->theme, cui_string("close"),
                         (cui_icon_options){.size = CUI_ICON_XSMALL,
                                            .custom_color = true,
                                            .color = icon->theme->muted});
}
static cui_id cui_status_notification_element(cui_id id, cui_notification_id n) {
    return cui_status_child_id(id, (size_t)(n.type ^ (n.key + UINT64_C(0x9e3779b97f4a7c15) +
                                                      (n.type << 6) + (n.type >> 2))));
}
static bool cui_status_notification_bottom(cui_notification_anchor a) {
    return a >= CUI_NOTIFICATION_BOTTOM_LEFT && a <= CUI_NOTIFICATION_BOTTOM_RIGHT;
}
static cui_color cui_status_notification_color(cui_color c, float alpha) {
    c.a = (uint8_t)(c.a * fmaxf(0, fminf(alpha, 1)));
    return c;
}
static cui_theme cui_status_notification_theme(const cui_theme *t, float alpha) {
    cui_theme copy = *t;
    cui_color *colors[] = {&copy.background,
                           &copy.surface,
                           &copy.popover,
                           &copy.text,
                           &copy.muted,
                           &copy.border,
                           &copy.primary,
                           &copy.on_primary,
                           &copy.secondary,
                           &copy.secondary_foreground,
                           &copy.accent,
                           &copy.accent_foreground,
                           &copy.link,
                           &copy.link_hover,
                           &copy.link_active,
                           &copy.input,
                           &copy.input_background};
    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++)
        *colors[i] = cui_status_notification_color(*colors[i], alpha);
    for (size_t i = 0; i < CUI_THEME_BUTTON_COUNT; i++) {
        cui_theme_button_colors *p = &copy.buttons[i];
        cui_color *palette[] = {&p->normal,         &p->hover,
                                &p->active,         &p->foreground,
                                &p->outline_normal, &p->outline_hover,
                                &p->outline_active, &p->border,
                                &p->outline_border, &p->outline_foreground};
        for (size_t j = 0; j < sizeof(palette) / sizeof(palette[0]); j++)
            *palette[j] = cui_status_notification_color(*palette[j], alpha);
    }
    return copy;
}
static float cui_status_notification_ease(float phase) {
    float u = phase;
    for (int i = 0; i < 6; i++) {
        float x = 3 * (1 - u) * (1 - u) * u * .25f + 3 * (1 - u) * u * u * .25f + u * u * u;
        float dx =
            .75f * (1 - u) * (1 - u) + 6 * (1 - u) * u * (.25f - .25f) + 3 * u * u * (1 - .25f);
        if (dx < .00001f)
            break;
        u = fmaxf(0, fminf(1, u - (x - phase) / dx));
    }
    return 3 * (1 - u) * (1 - u) * u * .1f + 3 * (1 - u) * u * u + u * u * u;
}
static float cui_status_notification_source_ease(float phase) {
    /* Pinned gpui-base unit-Bezier solver for (.25,.1,.25,1).
     * Eight Newton steps and bounded bisection preserve finite sampling. */
    const float cx = 3 * .25f, cy = 3 * .1f;
    const float bx = 3 * (.25f - .25f) - cx, by = 3 * (1 - .1f) - cy;
    const float ax = 1 - cx - bx, ay = 1 - cy - by;
    float u = phase;
    for (int i = 0; i < 8; i++) {
        float error = ((ax * u + bx) * u + cx) * u - phase;
        if (fabsf(error) < 1e-6f)
            return fmaxf(0, fminf(1, ((ay * u + by) * u + cy) * u));
        float slope = (3 * ax * u + 2 * bx) * u + cx;
        if (fabsf(slope) < 1e-6f)
            break;
        u = fmaxf(0, fminf(1, u - error / slope));
    }
    float low = 0, high = 1;
    u = phase;
    for (int i = 0; i < 32; i++) {
        float x = ((ax * u + bx) * u + cx) * u;
        if (fabsf(x - phase) < 1e-6f)
            break;
        if (x < phase)
            low = u;
        else
            high = u;
        u = (low + high) / 2;
    }
    return fmaxf(0, fminf(1, ((ay * u + by) * u + cy) * u));
}
static void cui_status_notification_spring(float *position, float *velocity, float target,
                                           double dt, float epsilon) {
    /* Closed-form critically damped response, stable for large frame intervals. */
    float omega = 15.7079632679f, delta = *position - target, step = (float)fmin(dt, 1),
          decay = expf(-omega * step), c = *velocity + omega * delta;
    *position = target + (delta + c * step) * decay;
    *velocity = (*velocity - omega * c * step) * decay;
    if (fabsf(*position - target) < epsilon && fabsf(*velocity) < epsilon) {
        *position = target;
        *velocity = 0;
    }
}
static void cui_status_notification_shadow(cui_context *ui, cui_id id, cui_rect b, float radius,
                                           float opacity, int16_t layer) {
    for (size_t shadow = 0; shadow < 2; shadow++)
        for (size_t ring = 0; ring < 8; ring++) {
            float blur = shadow ? 3 : 7.5f, spread = shadow ? -4 : -3, offset = shadow ? 4 : 10,
                  extent = spread + blur * (1 - (float)ring / 8);
            float width = fmaxf(0, b.w + 2 * extent), height = fmaxf(0, b.h + 2 * extent);
            if (width <= 0 || height <= 0)
                continue;
            cui_color ink = {0, 0, 0, (uint8_t)(255 * .1f * opacity / 8)};
            if (!ink.a)
                continue;
            cui_begin_portal(ui, cui_status_child_id(id, shadow * 8 + ring),
                             (cui_box){.width = width,
                                       .height = height,
                                       .radius = fmaxf(0, radius + extent),
                                       .background = ink,
                                       .inert = true},
                             (cui_rect){b.x - extent, b.y - extent + offset, width, height}, layer);
            cui_end_box(ui);
        }
}
static cui_notification_advance cui_status_notifications_render(cui_context *ui, cui_id id,
                                                                const cui_theme *t,
                                                                cui_notification_state *s,
                                                                cui_rect viewport, double now,
                                                                bool *submitted) {
    cui_notification_advance result = {0};
    if (!ui || !t || !s || !isfinite(now) || now < 0 || !isfinite(viewport.x) ||
        !isfinite(viewport.y) || !isfinite(viewport.w) || viewport.w <= 0 ||
        !isfinite(viewport.h) || viewport.h <= 0 || !isfinite(viewport.x + viewport.w) ||
        !isfinite(viewport.y + viewport.h))
        return result;
    bool expanded[8] = {0}, paused = false;
    cui_id focused = cui_focused(ui);
    for (size_t anchor = 0; anchor < 8; anchor++) {
        cui_id stack = cui_status_child_id(id, anchor + 500);
        expanded[anchor] = cui_hovered(ui, stack) || cui_focus_within(ui, stack);
        for (size_t i = 0; i < s->count; i++) {
            cui_notification *n = &s->entries[i].notification;
            cui_notification_anchor a = n->custom_placement ? n->placement : s->settings.placement;
            if (a != (cui_notification_anchor)anchor)
                continue;
            cui_id card = cui_status_notification_element(id, n->id);
            expanded[anchor] |= cui_hovered(ui, card) || focused == card ||
                                focused == cui_status_child_id(card, 300) ||
                                focused == cui_status_child_id(card, 301);
        }
        paused |= expanded[anchor];
    }
    size_t event_count;
    const cui_event *events = cui_frame_events(ui, &event_count);
    for (size_t ev = 0; ev < event_count; ev++)
        if (events[ev].pointer_button == CUI_POINTER_MIDDLE) {
            if (events[ev].type == CUI_EVENT_POINTER_DOWN) {
                s->middle_pressed = false;
                for (size_t i = s->count; i > 0; i--) {
                    cui_notification_entry *e = &s->entries[i - 1];
                    cui_rect b = e->bounds;
                    if (cui_notification_visible(s, i - 1) && e->phase != CUI_NOTIFICATION_ENDING &&
                        events[ev].x >= b.x && events[ev].x < b.x + b.w && events[ev].y >= b.y &&
                        events[ev].y < b.y + b.h) {
                        s->middle_capture = e->notification.id;
                        s->middle_pressed = true;
                        break;
                    }
                }
            } else if (events[ev].type == CUI_EVENT_POINTER_UP && s->middle_pressed) {
                s->middle_pressed = false;
                for (size_t i = 0; i < s->count; i++) {
                    cui_notification_entry *e = &s->entries[i];
                    cui_rect b = e->bounds;
                    if (e->notification.id.type == s->middle_capture.type &&
                        e->notification.id.key == s->middle_capture.key &&
                        cui_notification_visible(s, i) && events[ev].x >= b.x &&
                        events[ev].x < b.x + b.w && events[ev].y >= b.y &&
                        events[ev].y < b.y + b.h) {
                        cui_notification_dismiss(s, e->notification.id, now);
                        cui_invalidate(ui);
                        break;
                    }
                }
            }
        }
    result = cui_notification_update(s, now, paused);
    bool source_reduced =
        s->motion_policy == CUI_NOTIFICATION_MOTION_SOURCE && s->settings.reduced_motion;
    if (source_reduced)
        result.next_wake_seconds = 0;
    double dt = fmax(0, now - s->last_render);
    s->last_render = fmax(now, s->last_render);
    for (size_t anchor = 0; anchor < 8; anchor++) {
        size_t indices[CUI_NOTIFICATION_MAX_ITEMS], count = 0;
        float heights[CUI_NOTIFICATION_MAX_ITEMS];
        for (size_t i = 0; i < s->count; i++) {
            cui_notification_entry *e = &s->entries[i];
            cui_notification_anchor a = e->notification.custom_placement ? e->notification.placement
                                                                         : s->settings.placement;
            if (cui_notification_visible(s, i) && a == (cui_notification_anchor)anchor) {
                indices[count] = i;
                heights[count] = e->notification.height > 0 ? e->notification.height
                                 : e->bounds.h > 0          ? e->bounds.h
                                                            : 64;
                count++;
            }
        }
        if (!count) {
            s->stack_bounds[anchor] = (cui_rect){0};
            continue;
        }
        bool bottom = cui_status_notification_bottom((cui_notification_anchor)anchor);
        cui_notification_geometry geometry;
        cui_notification_stack_geometry(heights, count, 0, bottom, &geometry);
        float target_height =
            expanded[anchor] ? geometry.expanded_height : geometry.collapsed_height;
        if (!s->stack_initialized[anchor] || s->settings.reduced_motion) {
            s->stack_height[anchor] = target_height;
            s->stack_initialized[anchor] = true;
        } else
            cui_status_notification_spring(&s->stack_height[anchor], &s->stack_velocity[anchor],
                                           target_height, dt, .1f);
        if (s->stack_height[anchor] != target_height) {
            cui_invalidate(ui);
            result.next_wake_seconds = now + 1.0 / 60;
        }
        float stack_height = s->stack_height[anchor],
              width = fminf(s->settings.width, fmaxf(1, viewport.w - s->settings.margin_left -
                                                            s->settings.margin_right));
        float x = viewport.x + s->settings.margin_left, y = viewport.y + s->settings.margin_top;
        if (anchor == CUI_NOTIFICATION_TOP_CENTER || anchor == CUI_NOTIFICATION_BOTTOM_CENTER)
            x = viewport.x + (viewport.w - width) / 2;
        else if (anchor == CUI_NOTIFICATION_TOP_RIGHT || anchor == CUI_NOTIFICATION_BOTTOM_RIGHT ||
                 anchor == CUI_NOTIFICATION_RIGHT_CENTER)
            x = viewport.x + viewport.w - s->settings.margin_right - width;
        if (bottom)
            y = viewport.y + viewport.h - s->settings.margin_bottom - stack_height;
        else if (anchor == CUI_NOTIFICATION_LEFT_CENTER || anchor == CUI_NOTIFICATION_RIGHT_CENTER)
            y = viewport.y + (viewport.h - stack_height) / 2;
        cui_id stack = cui_status_child_id(id, anchor + 500);
        cui_begin_portal(
            ui, stack,
            (cui_box){
                .width = width, .height = stack_height, .fixed_width = true, .fixed_height = true},
            (cui_rect){x, y, width, stack_height}, 20);
        cui_track_bounds(ui, stack, &s->stack_bounds[anchor]);
        cui_interact(ui, stack, CUI_ROLE_GROUP, cui_string("Notifications"), false, false, 0);
        cui_set_tab_stop(ui, stack, false);
        cui_begin_focus_scope(ui, stack, false);
        for (size_t position = 0; position < count; position++) {
            cui_notification_entry *e = &s->entries[indices[position]];
            cui_notification n = e->notification;
            cui_notification_stack_geometry(heights, count, position, bottom, &geometry);
            float target_y = expanded[anchor] ? geometry.expanded_y : geometry.collapsed_y,
                  inset =
                      expanded[anchor] ? 0 : width * (.05f * fminf((float)geometry.rank, 2) / 2),
                  target_width = width - 2 * inset;
            if (!e->motion_initialized || s->settings.reduced_motion) {
                e->motion_y = target_y;
                e->motion_width = target_width;
                e->visibility = (!expanded[anchor] && geometry.rank >= 3) ? 0 : 1;
                e->motion_initialized = true;
            } else {
                cui_status_notification_spring(&e->motion_y, &e->velocity_y, target_y, dt, .1f);
                cui_status_notification_spring(&e->motion_width, &e->velocity_width, target_width,
                                               dt, .1f);
            }
            bool hidden = !expanded[anchor] && geometry.rank >= 3;
            if (s->settings.reduced_motion)
                e->visibility = hidden ? 0 : 1;
            else
                cui_status_notification_spring(&e->visibility, &e->velocity_visibility,
                                               hidden ? 0 : 1, dt, .001f);
            bool moving = e->motion_y != target_y || e->motion_width != target_width ||
                          e->visibility != (hidden ? 0 : 1);
            float phase =
                (float)fmin(1, e->elapsed / (e->phase == CUI_NOTIFICATION_ENDING ? .2 : .4));
            float eased = s->settings.reduced_motion ? 1
                          : s->motion_policy == CUI_NOTIFICATION_MOTION_SOURCE
                              ? cui_status_notification_source_ease(phase)
                              : cui_status_notification_ease(phase);
            float distance = s->motion_policy == CUI_NOTIFICATION_MOTION_SOURCE ? 96 : 16;
            float alpha = e->phase == CUI_NOTIFICATION_STARTING ? eased
                          : e->phase == CUI_NOTIFICATION_ENDING ? 1 - eased
                                                                : 1,
                  slide = e->phase == CUI_NOTIFICATION_STARTING ? (1 - eased) * distance
                          : e->phase == CUI_NOTIFICATION_ENDING ? eased * distance
                                                                : 0;
            if (!bottom && anchor != CUI_NOTIFICATION_LEFT_CENTER &&
                anchor != CUI_NOTIFICATION_RIGHT_CENTER)
                slide = -slide;
            if (anchor == CUI_NOTIFICATION_LEFT_CENTER || anchor == CUI_NOTIFICATION_RIGHT_CENTER)
                slide = 0;
            float transition_alpha = alpha;
            alpha *= e->visibility;
            cui_theme theme = cui_status_notification_theme(t, alpha);
            cui_id card = cui_status_notification_element(id, n.id);
            if (!hidden && s->settings.shadow_policy == CUI_NOTIFICATION_SHADOW_LEGACY)
                cui_status_notification_shadow(
                    ui, cui_status_child_id(card, 320),
                    (cui_rect){x + (width - e->motion_width) / 2, y + e->motion_y + slide,
                               e->motion_width, heights[position]},
                    t->radius_lg, alpha * alpha * alpha, (int16_t)(21 + position));
            cui_begin_portal(ui, card,
                             (cui_box){.width = e->motion_width,
                                       .height = n.height,
                                       .padding = 14,
                                       .gap = 0,
                                       .row = true,
                                       .radius = t->radius_lg,
                                       .background = theme.popover,
                                       .border = theme.border,
                                       .border_width = 1,
                                       .layout_only = hidden && e->visibility == 0,
                                       .inert = hidden || e->phase == CUI_NOTIFICATION_ENDING},
                             (cui_rect){x + (width - e->motion_width) / 2, y + e->motion_y + slide,
                                        e->motion_width, n.height},
                             (int16_t)(21 + position));
            if (!hidden && s->settings.shadow_policy == CUI_NOTIFICATION_SHADOW_SOURCE) {
                /* Source strength is alpha^3; source element opacity contributes
                 * once more. This path already fades its local theme colors. */
                float shadow_alpha = fmaxf(0, fminf(transition_alpha, 1));
                float visibility = fmaxf(0, fminf(e->visibility, 1));
                unsigned char ink = (unsigned char)lroundf(
                    25.5f * shadow_alpha * shadow_alpha * shadow_alpha * shadow_alpha * visibility);
                cui_drop_shadow layers[2] = {
                    {.offset_y = 10, .sigma = 7.5f, .spread = -3, .color = {0, 0, 0, ink}},
                    {.offset_y = 4, .sigma = 3, .spread = -4, .color = {0, 0, 0, ink}}};
                float radius = t->radius_lg;
                if (!cui_set_drop_shadows(ui, layers, 2,
                                          (cui_corner_radii){radius, radius, radius, radius}))
                    *submitted = false;
            }
            cui_track_bounds(ui, card, &e->bounds);
            cui_interaction interaction =
                cui_interact(ui, card, n.on_click ? CUI_ROLE_BUTTON : CUI_ROLE_ALERT,
                             n.message.length ? n.message : n.title, false, false, 0);
            cui_set_tab_stop(ui, card, n.on_click != NULL);
            if (n.title.length)
                cui_set_semantic_details(ui, card, (cui_semantic_details){.description = n.title});
            cui_status_spacer(ui, cui_status_child_id(card, 310), 2);
            cui_begin_box(ui, cui_status_child_id(card, 311), (cui_box){.row = true, .gap = 12});
            bool has_icon = n.kind != CUI_NOTIFICATION_UNTYPED || n.icon.length;
            if (has_icon) {
                cui_color color = n.kind == CUI_NOTIFICATION_SUCCESS   ? t->success
                                  : n.kind == CUI_NOTIFICATION_WARNING ? t->warning
                                  : n.kind == CUI_NOTIFICATION_ERROR   ? t->danger
                                  : n.kind == CUI_NOTIFICATION_INFO    ? t->info
                                                                       : t->muted;
                const char *icon = n.kind == CUI_NOTIFICATION_SUCCESS   ? "circle-check"
                                   : n.kind == CUI_NOTIFICATION_WARNING ? "triangle-alert"
                                   : n.kind == CUI_NOTIFICATION_ERROR   ? "circle-x"
                                                                        : "info";
                if (n.kind == CUI_NOTIFICATION_UNTYPED)
                    cui_text(ui, n.icon, t->font_size, cui_status_notification_color(color, alpha));
                else
                    (void)cui_icon_named(
                        ui, cui_status_child_id(card, 314), t, cui_string(icon),
                        (cui_icon_options){.custom_color = true,
                                           .color = cui_status_notification_color(color, alpha)});
            }
            float action_width =
                n.action ? 80
                : n.action_label.length
                    ? cui_measure_text(ui, n.action_label, t->font_size * .8f).width +
                          2 * t->padding * .8f + 12
                    : 0;
            float content_width =
                fmaxf(1, e->motion_width - 32 - (has_icon ? 28 : 0) - action_width - 24);
            cui_begin_box(ui, cui_status_child_id(card, 302),
                          (cui_box){.width = content_width, .gap = 4, .clip = true});
            if (n.title.length)
                cui_text(ui, n.title, t->font_size, theme.text);
            if (n.message.length)
                cui_text(ui, n.message, t->font_size, theme.text);
            if (n.content)
                n.content(ui, cui_status_child_id(card, 303), n.content_user, t->font_size,
                          theme.text, alpha);
            cui_end_box(ui);
            if (n.action)
                n.action(ui, cui_status_child_id(card, 301), n.action_user, t->font_size,
                         theme.text, alpha);
            else if (n.action_label.length &&
                     cui_button_ex(ui, cui_status_child_id(card, 301), &theme, n.action_label,
                                   (cui_button_options){.size = CUI_CONTROL_SMALL,
                                                        .variant = CUI_BUTTON_PRIMARY}) &&
                     n.on_action)
                n.on_action(n.user, n.id);
            /* Keep action content clear of the independent close hit target.
             * Reserve this lane even when Close is hidden to avoid hover reflow. */
            cui_begin_box(ui, cui_status_child_id(card, 316), (cui_box){.width = 24, .height = 1});
            cui_end_box(ui);
            cui_end_box(ui);
            cui_status_spacer(ui, cui_status_child_id(card, 312), 2);
            bool close = false;
            if (cui_hovered(ui, card) || expanded[anchor]) {
                cui_begin_portal(
                    ui, cui_status_child_id(card, 313), (cui_box){.width = 20, .height = 20},
                    (cui_rect){x + (width - e->motion_width) / 2 + e->motion_width - 24,
                               y + e->motion_y + slide + 4, 20, 20},
                    (int16_t)(21 + position));
                cui_status_close_icon close_icon = {&theme, cui_status_child_id(card, 315)};
                close = cui_button_ex(
                    ui, cui_status_child_id(card, 300), &theme, cui_string("Close"),
                    (cui_button_options){.variant = CUI_BUTTON_GHOST,
                                         .size = CUI_CONTROL_XSMALL,
                                         .width = 20,
                                         .content = cui_status_notification_close_content,
                                         .content_user = &close_icon});
                cui_end_box(ui);
            }
            cui_end_box(ui);
            if (close || interaction.activated) {
                cui_notification_dismiss(s, n.id, now);
                if (interaction.activated && n.on_click)
                    n.on_click(n.user, n.id);
                cui_invalidate(ui);
                result.changed = true;
            }
            if (moving) {
                cui_invalidate(ui);
                result.next_wake_seconds = now + 1.0 / 60;
            }
        }
        cui_end_focus_scope(ui);
        cui_end_box(ui);
    }
    for (size_t i = 0; i < s->count; i++) {
        if (s->entries[i].in_app && s->entries[i].phase != CUI_NOTIFICATION_PRESENT) {
            double wake =
                now + (source_reduced
                           ? fmax(0, (s->entries[i].phase == CUI_NOTIFICATION_ENDING ? .2 : .4) -
                                         s->entries[i].elapsed)
                           : 1.0 / 60);
            if (!result.next_wake_seconds || wake < result.next_wake_seconds)
                result.next_wake_seconds = wake;
        } else if (source_reduced && s->entries[i].in_app && !paused &&
                   s->entries[i].remaining >= 0) {
            double wake = now + fmin(.05, s->entries[i].remaining);
            if (!result.next_wake_seconds || wake < result.next_wake_seconds)
                result.next_wake_seconds = wake;
        }
    }
    if (result.changed)
        cui_invalidate(ui);
    return result;
}

bool cui_notifications_checked(cui_context *ui, cui_id id, const cui_theme *theme,
                               cui_notification_state *state, cui_rect viewport, double now,
                               cui_notification_advance *out) {
    if (!ui || !theme || !state || !out || !state->entries || !state->capacity ||
        state->capacity > CUI_NOTIFICATION_MAX_ITEMS || state->count > state->capacity ||
        state->system_count > 100 || !cui_notification_settings_valid(state->settings) ||
        state->motion_policy < CUI_NOTIFICATION_MOTION_LEGACY ||
        state->motion_policy > CUI_NOTIFICATION_MOTION_SOURCE || !isfinite(now) || now < 0 ||
        !isfinite(viewport.x) || !isfinite(viewport.y) || !isfinite(viewport.w) ||
        viewport.w <= 0 || !isfinite(viewport.h) || viewport.h <= 0 ||
        !isfinite(viewport.x + viewport.w) || !isfinite(viewport.y + viewport.h) ||
        (state->settings.shadow_policy == CUI_NOTIFICATION_SHADOW_SOURCE &&
         (!isfinite(theme->radius_lg) || theme->radius_lg < 0 || theme->radius_lg > 16384)))
        return false;
    if (state->settings.shadow_policy == CUI_NOTIFICATION_SHADOW_SOURCE ||
        state->motion_policy == CUI_NOTIFICATION_MOTION_SOURCE) {
        if (!isfinite(state->last_render) || state->last_render < 0)
            return false;
        for (size_t i = 0; i < state->count; i++) {
            const cui_notification_entry *e = &state->entries[i];
            if (e->phase < CUI_NOTIFICATION_STARTING || e->phase > CUI_NOTIFICATION_ENDING ||
                !isfinite(e->elapsed) || e->elapsed < 0 || !isfinite(e->last_advance) ||
                !isfinite(e->visibility) || !isfinite(e->velocity_visibility))
                return false;
        }
    }
    bool submitted = true;
    cui_notification_advance next =
        cui_status_notifications_render(ui, id, theme, state, viewport, now, &submitted);
    if (!submitted)
        return false;
    *out = next;
    return true;
}
cui_notification_advance cui_notifications(cui_context *ui, cui_id id, const cui_theme *theme,
                                           cui_notification_state *state, cui_rect viewport,
                                           double now) {
    cui_notification_advance result = {0};
    (void)cui_notifications_checked(ui, id, theme, state, viewport, now, &result);
    return result;
}
