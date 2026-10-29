/* SPDX-License-Identifier: MIT */
#include "cui/cui_theme.h"
#include <math.h>
#include <string.h>
bool cui_design_measure(cui_design_size size, float density, cui_design_metrics *output) {
    if (!output || size <= CUI_DESIGN_INHERIT || size > CUI_DESIGN_9XL || !isfinite(density) ||
        density <= 0 || density > 32)
        return false;
    static const float font[] = {0, 12, 14, 16, 16, 18, 20, 24, 30, 36, 48, 60, 72, 96, 128};
    static const float line[] = {0, 16, 20, 24, 24, 28, 28, 32, 36, 40, 48, 60, 72, 96, 128};
    static const float height[] = {0, 20, 24, 32, 32, 40, 48, 56, 64, 80, 104, 128, 152, 200, 264};
    static const float padding[] = {0, 4, 8, 10, 10, 12, 16, 20, 24, 28, 32, 40, 48, 56, 64};
    static const float gap[] = {0, 4, 4, 6, 6, 8, 8, 12, 12, 16, 20, 24, 28, 32, 40};
    *output = (cui_design_metrics){.font_size = font[size] * density,
                                   .line_height = line[size] * density,
                                   .control_height = height[size] * density,
                                   .padding_x = padding[size] * density,
                                   .padding_y = (height[size] - line[size]) * .5f * density,
                                   .gap = gap[size] * density,
                                   .indicator_size = font[size] * density};
    return true;
}
float cui_space(float units) {
    return isfinite(units) && fabsf(units) <= 250000 ? units * 4 : 0;
}
float cui_radius(cui_radius_size size, float density) {
    static const float radii[] = {0, 2, 4, 6, 8, 12, 16, 24, 9999};
    return size >= CUI_RADIUS_NONE && size <= CUI_RADIUS_FULL && isfinite(density) && density > 0 &&
                   density <= 32
               ? radii[size] * density
               : 0;
}
#define CUI_THEME_RGB(r, g, b) {r, g, b, 255}
static cui_color cui_theme_scale(cui_color c, float factor) {
    c.r = (uint8_t)(c.r * factor);
    c.g = (uint8_t)(c.g * factor);
    c.b = (uint8_t)(c.b * factor);
    return c;
}
static cui_color cui_theme_opacity(cui_color c, float opacity) {
    c.a = (uint8_t)(c.a * opacity);
    return c;
}
static cui_theme cui_theme_control_defaults(cui_theme t) {
    t.chart_1 = t.primary;
    t.chart_2 = t.primary;
    t.chart_3 = t.primary;
    t.chart_4 = t.primary;
    t.chart_5 = t.primary;
    t.chart_bullish = t.success;
    t.chart_bearish = t.danger;
    t.chart_grid = cui_theme_opacity(t.border, .6f);
    cui_color secondary = {(uint8_t)((3 * t.surface.r + t.border.r) / 4),
                           (uint8_t)((3 * t.surface.g + t.border.g) / 4),
                           (uint8_t)((3 * t.surface.b + t.border.b) / 4), 255};
    cui_color backgrounds[] = {t.surface, t.primary, secondary, t.danger,
                               t.warning, t.success, t.info};
    cui_color foregrounds[] = {t.text,       t.on_primary, t.text,   t.on_danger,
                               t.on_warning, t.on_success, t.on_info};
    for (size_t i = 0; i < CUI_THEME_BUTTON_COUNT; i++) {
        cui_color normal = backgrounds[i], hover = cui_theme_scale(normal, .9f),
                  active = cui_theme_scale(normal, .75f);
        t.buttons[i] = (cui_theme_button_colors){
            .normal = normal,
            .hover = hover,
            .active = active,
            .foreground = foregrounds[i],
            .outline_normal = cui_theme_opacity(normal, .1f),
            .outline_hover = cui_theme_opacity(hover, .2f),
            .outline_active = cui_theme_opacity(active, .4f),
            .border = normal,
            .outline_border = normal,
            .outline_foreground =
                i == CUI_THEME_BUTTON_DEFAULT || i == CUI_THEME_BUTTON_SECONDARY ? t.text : normal};
    }
    t.input = t.border;
    t.input_background = t.surface;
    t.buttons[CUI_THEME_BUTTON_DEFAULT].border = t.input;
    t.buttons[CUI_THEME_BUTTON_DEFAULT].outline_border = t.input;
    t.buttons[CUI_THEME_BUTTON_DEFAULT].outline_normal = t.input_background;
    t.buttons[CUI_THEME_BUTTON_DEFAULT].outline_hover = cui_theme_opacity(t.input, .5f);
    t.buttons[CUI_THEME_BUTTON_DEFAULT].outline_active = cui_theme_opacity(t.input, .7f);
    t.link = t.primary;
    t.link_hover = cui_theme_scale(t.primary, .9f);
    t.link_active = cui_theme_scale(t.primary, .75f);
    t.secondary_foreground = t.text;
    t.accent = t.border;
    t.accent_foreground = t.text;
    t.spring_control =
        (cui_spring_settings){.response = .18f, .damping = 1, .epsilon = .001f, .travel = true};
    t.popover = t.surface;
    t.secondary = secondary;
    t.muted_background = secondary;
    t.radius_lg = t.radius == 0 ? 0 : t.radius + 2;
    t.secondary_hover = cui_theme_scale(t.secondary, .9f);
    t.tab_foreground = t.muted;
    t.tab_active_foreground = t.text;
    t.tab_active = t.surface;
    t.tab_bar = t.background;
    t.tab_bar_segmented = t.secondary;
    t.switch_background = t.buttons[CUI_THEME_BUTTON_SECONDARY].active;
    t.switch_thumb = t.background;
    t.popover_foreground = t.text;
    t.list_hover = cui_theme_opacity(t.accent, .6f);
    t.list_active = (cui_color){(uint8_t)((9u * t.background.r + t.primary.r) / 10u),
                                (uint8_t)((9u * t.background.g + t.primary.g) / 10u),
                                (uint8_t)((9u * t.background.b + t.primary.b) / 10u), 51};
    t.selection = cui_theme_opacity(t.primary, .3f);
    t.caret = t.text;
    unsigned group_alpha = t.background.r + t.background.g + t.background.b < 384 ? 3 : 4;
    t.group_box = (cui_color){
        (uint8_t)(((10 - group_alpha) * t.background.r + group_alpha * t.secondary.r) / 10),
        (uint8_t)(((10 - group_alpha) * t.background.g + group_alpha * t.secondary.g) / 10),
        (uint8_t)(((10 - group_alpha) * t.background.b + group_alpha * t.secondary.b) / 10), 255};
    t.group_box_foreground = t.text;
    t.list_active_highlight = true;
    t.description_list_label = (cui_color){(uint8_t)((4u * t.background.r + t.border.r) / 5u),
                                           (uint8_t)((4u * t.background.g + t.border.g) / 5u),
                                           (uint8_t)((4u * t.background.b + t.border.b) / 5u), 255};
    t.description_list_label_foreground = t.muted;
    t.title_bar = t.background;
    t.title_bar_border = t.border;
    t.status_bar = t.title_bar;
    t.status_bar_border = t.border;
    t.sidebar = (cui_color){(uint8_t)((85u * t.background.r + 15u * t.border.r) / 100u),
                            (uint8_t)((85u * t.background.g + 15u * t.border.g) / 100u),
                            (uint8_t)((85u * t.background.b + 15u * t.border.b) / 100u), 255};
    t.sidebar_foreground = t.text;
    t.sidebar_border = t.border;
    t.sidebar_accent = t.accent;
    t.sidebar_accent_foreground = t.accent_foreground;
    t.spring_move =
        (cui_spring_settings){.response = .28f, .damping = .85f, .epsilon = .1f, .travel = true};
    return t;
}
cui_theme cui_theme_light(void) {
    return cui_theme_control_defaults((cui_theme){CUI_THEME_RGB(246, 248, 250),
                                                  CUI_THEME_RGB(255, 255, 255),
                                                  CUI_THEME_RGB(24, 32, 42),
                                                  CUI_THEME_RGB(87, 99, 112),
                                                  CUI_THEME_RGB(23, 91, 169),
                                                  CUI_THEME_RGB(255, 255, 255),
                                                  CUI_THEME_RGB(181, 191, 204),
                                                  CUI_THEME_RGB(179, 35, 39),
                                                  CUI_THEME_RGB(23, 91, 169),
                                                  16,
                                                  10,
                                                  8,
                                                  6,
                                                  42,
                                                  CUI_THEME_RGB(133, 77, 14),
                                                  CUI_THEME_RGB(255, 255, 255),
                                                  CUI_THEME_RGB(22, 101, 52),
                                                  CUI_THEME_RGB(255, 255, 255),
                                                  CUI_THEME_RGB(7, 89, 133),
                                                  CUI_THEME_RGB(255, 255, 255),
                                                  CUI_THEME_RGB(255, 255, 255),
                                                  .spring_control = {0}});
}
cui_theme cui_theme_dark(void) {
    return cui_theme_control_defaults((cui_theme){CUI_THEME_RGB(18, 23, 30),
                                                  CUI_THEME_RGB(30, 38, 48),
                                                  CUI_THEME_RGB(235, 240, 246),
                                                  CUI_THEME_RGB(164, 176, 190),
                                                  CUI_THEME_RGB(116, 178, 250),
                                                  CUI_THEME_RGB(14, 24, 38),
                                                  CUI_THEME_RGB(102, 118, 137),
                                                  CUI_THEME_RGB(255, 137, 142),
                                                  CUI_THEME_RGB(164, 209, 255),
                                                  16,
                                                  10,
                                                  8,
                                                  6,
                                                  42,
                                                  CUI_THEME_RGB(250, 204, 21),
                                                  CUI_THEME_RGB(14, 24, 38),
                                                  CUI_THEME_RGB(74, 222, 128),
                                                  CUI_THEME_RGB(14, 24, 38),
                                                  CUI_THEME_RGB(56, 189, 248),
                                                  CUI_THEME_RGB(14, 24, 38),
                                                  CUI_THEME_RGB(14, 24, 38),
                                                  .spring_control = {0},
                                                  .dark = true});
}
cui_theme cui_theme_high_contrast(void) {
    return cui_theme_control_defaults((cui_theme){CUI_THEME_RGB(0, 0, 0),
                                                  CUI_THEME_RGB(0, 0, 0),
                                                  CUI_THEME_RGB(255, 255, 255),
                                                  CUI_THEME_RGB(255, 255, 255),
                                                  CUI_THEME_RGB(255, 255, 0),
                                                  CUI_THEME_RGB(0, 0, 0),
                                                  CUI_THEME_RGB(255, 255, 255),
                                                  CUI_THEME_RGB(255, 120, 120),
                                                  CUI_THEME_RGB(0, 255, 255),
                                                  18,
                                                  10,
                                                  10,
                                                  0,
                                                  46,
                                                  CUI_THEME_RGB(255, 255, 0),
                                                  CUI_THEME_RGB(0, 0, 0),
                                                  CUI_THEME_RGB(0, 255, 128),
                                                  CUI_THEME_RGB(0, 0, 0),
                                                  CUI_THEME_RGB(0, 255, 255),
                                                  CUI_THEME_RGB(0, 0, 0),
                                                  CUI_THEME_RGB(0, 0, 0),
                                                  .spring_control = {0},
                                                  .dark = true});
}
cui_theme cui_theme_terminal(void) {
    return cui_theme_control_defaults((cui_theme){CUI_THEME_RGB(31, 29, 23),
                                                  CUI_THEME_RGB(45, 42, 33),
                                                  CUI_THEME_RGB(255, 227, 164),
                                                  CUI_THEME_RGB(205, 182, 140),
                                                  CUI_THEME_RGB(255, 192, 77),
                                                  CUI_THEME_RGB(31, 29, 23),
                                                  CUI_THEME_RGB(148, 126, 84),
                                                  CUI_THEME_RGB(255, 137, 103),
                                                  CUI_THEME_RGB(255, 227, 164),
                                                  16,
                                                  12,
                                                  6,
                                                  0,
                                                  44,
                                                  CUI_THEME_RGB(255, 192, 77),
                                                  CUI_THEME_RGB(31, 29, 23),
                                                  CUI_THEME_RGB(168, 212, 128),
                                                  CUI_THEME_RGB(31, 29, 23),
                                                  CUI_THEME_RGB(145, 205, 232),
                                                  CUI_THEME_RGB(31, 29, 23),
                                                  CUI_THEME_RGB(31, 29, 23),
                                                  .spring_control = {0},
                                                  .dark = true});
}

#undef CUI_THEME_RGB

#include "cui_theme_presets.inc"
size_t cui_theme_preset_count(void) {
    return sizeof cui_theme_presets / sizeof cui_theme_presets[0];
}
cui_str cui_theme_preset_name(size_t index) {
    if (index >= cui_theme_preset_count())
        return (cui_str){0};
    const char *name = cui_theme_presets[index].name;
    return (cui_str){name, strlen(name)};
}
bool cui_theme_preset_is_dark(size_t index) {
    return index < cui_theme_preset_count() && cui_theme_presets[index].dark;
}
bool cui_theme_preset_get(size_t index, cui_theme *output) {
    if (!output || index >= cui_theme_preset_count())
        return false;
    const cui_theme_preset_record *record = &cui_theme_presets[index];
    cui_theme theme = record->dark ? cui_theme_dark() : cui_theme_light();
    for (size_t i = record->begin; i < record->begin + record->count; ++i)
        cui_theme_preset_assign(&theme, cui_theme_preset_colors[i].field,
                                cui_theme_preset_colors[i].color);
    const cui_color *chart = cui_theme_chart_presets[index];
    theme.chart_1 = chart[0];
    theme.chart_2 = chart[1];
    theme.chart_3 = chart[2];
    theme.chart_4 = chart[3];
    theme.chart_5 = chart[4];
    theme.chart_bullish = chart[5];
    theme.chart_bearish = chart[6];
    theme.chart_grid = chart[7];
    *output = theme;
    return true;
}

bool cui_theme_popover_shadows(const cui_theme *theme, float strength, cui_drop_shadow out[3]) {
    if (!theme || !out || !isfinite(strength))
        return false;
    unsigned char alpha = (unsigned char)lroundf(25.5f * fminf(1, fmaxf(0, strength)));
    cui_drop_shadow next[3] = {
        {.spread = 1, .color = {theme->text.r, theme->text.g, theme->text.b, alpha}},
        {.offset_y = 4, .sigma = 3, .spread = -1, .color = {0, 0, 0, alpha}},
        {.offset_y = 2, .sigma = 2, .spread = -2, .color = {0, 0, 0, alpha}}};
    memcpy(out, next, sizeof(next));
    return true;
}
