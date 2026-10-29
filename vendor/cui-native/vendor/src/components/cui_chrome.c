/* SPDX-License-Identifier: MIT */
#include "cui/cui_chrome.h"
#include <math.h>
static cui_id cui_chrome_part(cui_id id, unsigned part) {
    return id ^ (UINT64_C(0x1cb2ea6581472f93) * (part + 1));
}
static bool cui_chrome_regions_valid(cui_id id, const cui_chrome_region regions[3]) {
    for (unsigned r = 0; r < 3; ++r) {
        if (regions[r].count && !regions[r].items)
            return false;
        for (size_t i = 0; i < regions[r].count; ++i) {
            cui_id item = regions[r].items[i].id;
            if (!item || item == id)
                return false;
            for (unsigned p = 0; p <= r; ++p)
                for (size_t j = 0; j < (p == r ? i : regions[p].count); ++j)
                    if (regions[p].items[j].id == item)
                        return false;
        }
    }
    return true;
}
static void cui_chrome_region_draw(cui_context *ui, cui_id id, const cui_theme *theme,
                                   cui_chrome_region region, float width, cui_align alignment,
                                   float gap) {
    cui_begin_box(ui, id,
                  (cui_box){.width = width,
                            .row = true,
                            .align_x = alignment,
                            .align_y = CUI_ALIGN_CENTER,
                            .gap = gap});
    for (size_t i = 0; i < region.count; ++i) {
        const cui_chrome_item *item = &region.items[i];
        cui_begin_box(ui, item->id, (cui_box){0});
        if (item->render)
            item->render(ui, theme, item->id, item->user);
        else {
            cui_interact(ui, item->id, CUI_ROLE_LABEL, item->text, false, false, 0);
            cui_set_tab_stop(ui, item->id, false);
            cui_text_with_line_height(ui, item->text, theme->font_size, theme->text,
                                      theme->font_size * 1.618034f);
        }
        cui_end_box(ui);
    }
    cui_end_box(ui);
}
bool cui_status_bar(cui_context *ui, cui_id id, const cui_theme *theme, cui_chrome_region left,
                    cui_chrome_region center, cui_chrome_region right, cui_status_bar_options o) {
    cui_chrome_region regions[] = {left, center, right};
    if (!ui || !id || !theme || !isfinite(theme->font_size) || theme->font_size <= 0 ||
        !isfinite(o.width) || o.width < -1 || !cui_chrome_regions_valid(id, regions))
        return false;
    float density = theme->font_size / 16;
    cui_box box = {.width = o.width > 0 ? o.width : -1,
                   .background = theme->status_bar,
                   .border = theme->status_bar_border,
                   .custom_border_edges = true,
                   .border_edges = {.top = 1},
                   .clip = true};
    if (o.style)
        box = *o.style;
    cui_begin_box(ui, id, box);
    cui_interact(ui, id, CUI_ROLE_GROUP, o.name, false, false, 0);
    cui_set_tab_stop(ui, id, false);
    cui_begin_box(ui, cui_chrome_part(id, 0), (cui_box){.height = 4 * density + (o.style ? 0 : 1)});
    cui_end_box(ui);
    cui_begin_box(ui, cui_chrome_part(id, 1), (cui_box){.width = -1, .row = true});
    cui_begin_box(ui, cui_chrome_part(id, 2), (cui_box){.width = 8 * density});
    cui_end_box(ui);
    cui_begin_box(
        ui, cui_chrome_part(id, 3),
        (cui_box){.width = -1, .row = true, .align_y = CUI_ALIGN_CENTER, .gap = 8 * density});
    cui_theme content = *theme;
    content.font_size *= .75f;
    content.text = theme->muted;
    if (left.count)
        cui_chrome_region_draw(ui, cui_chrome_part(id, 4), &content, left, 0, CUI_ALIGN_START,
                               8 * density);
    cui_chrome_region_draw(ui, cui_chrome_part(id, 5), &content, center, -1,
                           cui_status_bar_center_alignment(left.count != 0, right.count != 0),
                           8 * density);
    if (right.count)
        cui_chrome_region_draw(ui, cui_chrome_part(id, 6), &content, right, 0, CUI_ALIGN_START,
                               8 * density);
    cui_end_box(ui);
    cui_begin_box(ui, cui_chrome_part(id, 7), (cui_box){.width = 8 * density});
    cui_end_box(ui);
    cui_end_box(ui);
    cui_begin_box(ui, cui_chrome_part(id, 8), (cui_box){.height = 4 * density});
    cui_end_box(ui);
    cui_end_box(ui);
    return true;
}

/* A one-pixel-wide immutable strip retains the source vertical gradient while
 * using the existing bitmap renderer and per-context resource lifetime. */
#include "cui/cui_icon.h"
typedef struct {
    unsigned char pixels[34 * 4];
    bool initialized;
} cui_title_gradient;
static bool cui_title_background(cui_context *ui, cui_id id, const cui_theme *theme) {
    cui_color bottom = theme->title_bar;
    cui_color top = {(uint8_t)((55u * bottom.r + 45u * theme->background.r) / 100u),
                     (uint8_t)((55u * bottom.g + 45u * theme->background.g) / 100u),
                     (uint8_t)((55u * bottom.b + 45u * theme->background.b) / 100u),
                     (uint8_t)((55u * bottom.a + 45u * theme->background.a) / 100u)};
    const unsigned char colors[] = {top.r,    top.g,    top.b,    top.a,
                                    bottom.r, bottom.g, bottom.b, bottom.a};
    uint64_t key = UINT64_C(0x291753bac4035ef1);
    for (size_t i = 0; i < sizeof colors; ++i)
        key = (key ^ colors[i]) * UINT64_C(1099511628211);
    if (!key)
        key = 1;
    cui_title_gradient *gradient = cui_resource_get(ui, id, key, sizeof(*gradient), NULL, NULL);
    if (!gradient)
        return false;
    if (!gradient->initialized) {
        for (unsigned y = 0; y < 34; ++y)
            for (unsigned channel = 0; channel < 4; ++channel)
                gradient->pixels[y * 4 + channel] =
                    (unsigned char)((colors[channel] * (33u - y) + colors[channel + 4] * y) / 33u);
        gradient->initialized = true;
    }
    return cui_set_bitmap_underlay(
        ui, (cui_underlay_layout){.grow_width = true, .grow_height = true},
        (cui_bitmap_paint){.bitmap = {gradient->pixels, 1, 34, 4, key}, .opacity = 1},
        (cui_corner_radii){0});
}
static bool cui_title_control(cui_context *ui, cui_id id, const cui_theme *theme,
                              cui_window_info info, cui_title_bar_options o,
                              cui_window_action action, const char *name, const char *icon) {
    cui_interaction input =
        cui_interact(ui, id, CUI_ROLE_BUTTON, cui_string(name), false, false, 0);
    bool close = action == CUI_WINDOW_CLOSE;
    cui_color foreground = theme->text, background = {0};
    if (input.pressed || input.hovered) {
        foreground = close ? theme->on_danger : theme->secondary_foreground;
        background =
            close ? input.pressed ? theme->buttons[CUI_THEME_BUTTON_DANGER].active : theme->danger
            : input.pressed ? theme->buttons[CUI_THEME_BUTTON_SECONDARY].active
                            : theme->secondary_hover;
    }
    cui_begin_box(ui, id,
                  (cui_box){.width = 34,
                            .height = -1,
                            .align_x = CUI_ALIGN_CENTER,
                            .align_y = CUI_ALIGN_CENTER,
                            .background = background});
    cui_icon_named(
        ui, cui_chrome_part(id, 14), theme, cui_string(icon),
        (cui_icon_options){.size = CUI_ICON_SMALL, .color = foreground, .custom_color = true});
    cui_end_box(ui);
    if (!input.activated)
        return false;
    if (close && o.on_close) {
        o.on_close(o.close_user);
        return true;
    }
    return (info.actions & (1u << action)) && o.host && o.host->perform &&
           o.host->perform(o.host->user, action, 0, 0);
}
bool cui_title_bar(cui_context *ui, cui_id id, const cui_theme *theme, cui_title_bar_state *state,
                   const cui_chrome_item *items, size_t count, cui_title_bar_options o) {
    cui_chrome_region regions[] = {{items, count}, {0}, {0}};
    if (!ui || !id || !theme || !state || !isfinite(o.width) || o.width < -1 ||
        !cui_chrome_regions_valid(id, regions))
        return false;
    cui_window_info info = {0};
    if (o.host && o.host->query)
        info = o.host->query(o.host->user);
    bool acted = cui_base_title_bar(ui, id, state, info, o);
    cui_box box = {.width = o.width > 0 ? o.width : -1,
                   .height = 34,
                   .row = true,
                   .align_y = CUI_ALIGN_CENTER,
                   .border = theme->title_bar_border,
                   .custom_border_edges = true,
                   .border_edges = {.bottom = 1}};
    if (o.style)
        box = *o.style;
    cui_begin_box(ui, id, box);
    if (!o.style)
        cui_title_background(ui, id, theme);
    float left = info.decoration == CUI_WINDOW_NATIVE_TRAFFIC_LIGHTS ? 80 : 12;
    cui_begin_box(ui, cui_chrome_part(id, 20), (cui_box){.width = left});
    cui_end_box(ui);
    cui_begin_box(ui, cui_chrome_part(id, 21),
                  (cui_box){.width = -1, .height = -1, .row = true, .align_y = CUI_ALIGN_CENTER});
    if (info.fullscreen && info.decoration != CUI_WINDOW_BROWSER) {
        cui_begin_box(ui, cui_chrome_part(id, 22), (cui_box){.width = 12});
        cui_end_box(ui);
    }
    for (size_t i = 0; i < count; ++i) {
        const cui_chrome_item *item = &items[i];
        cui_begin_box(ui, item->id, (cui_box){0});
        if (item->render)
            item->render(ui, theme, item->id, item->user);
        else {
            cui_interact(ui, item->id, CUI_ROLE_LABEL, item->text, false, false, 0);
            cui_set_tab_stop(ui, item->id, false);
            cui_text(ui, item->text, theme->font_size, theme->text);
        }
        cui_end_box(ui);
        if (i + 1 < count) {
            cui_begin_box(ui, cui_chrome_part(item->id, 24), (cui_box){.width = -1});
            cui_end_box(ui);
        }
    }
    cui_end_box(ui);
    if (info.decoration == CUI_WINDOW_CLIENT_DECORATED) {
        if (info.actions & (1u << CUI_WINDOW_MINIMIZE))
            acted |= cui_title_control(ui, cui_title_bar_control_id(id, 0), theme, info, o,
                                       CUI_WINDOW_MINIMIZE, "Minimize window", "window-minimize");
        cui_window_action zoom = info.maximized ? CUI_WINDOW_RESTORE : CUI_WINDOW_MAXIMIZE;
        if (info.actions & (1u << zoom))
            acted |= cui_title_control(ui, cui_title_bar_control_id(id, 1), theme, info, o, zoom,
                                       info.maximized ? "Restore window" : "Maximize window",
                                       info.maximized ? "window-restore" : "window-maximize");
        if ((info.actions & (1u << CUI_WINDOW_CLOSE)) || o.on_close)
            acted |= cui_title_control(ui, cui_title_bar_control_id(id, 2), theme, info, o,
                                       CUI_WINDOW_CLOSE, "Close window", "window-close");
    }
    cui_end_box(ui);
    return acted;
}
