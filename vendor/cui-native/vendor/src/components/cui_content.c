/* SPDX-License-Identifier: MIT */
#include "cui/cui_content.h"
#include "cui/cui_avatar_events.h"
#include "cui/cui_components.h"
#include "cui/cui_hover_events.h"
#include "cui/cui_icon.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static float cui_content_positive(float value, float fallback) {
    return isfinite(value) && value > 0 ? value : fallback;
}
static cui_color cui_content_blend(cui_color a, cui_color b, float amount) {
    return (cui_color){(uint8_t)(a.r + (b.r - a.r) * amount), (uint8_t)(a.g + (b.g - a.g) * amount),
                       (uint8_t)(a.b + (b.b - a.b) * amount),
                       (uint8_t)(a.a + (b.a - a.a) * amount)};
}
static void cui_content_semantic(cui_context *ui, cui_id id, cui_role role, cui_str name,
                                 bool disabled, bool checked, float value) {
    cui_interact(ui, id, role, name, disabled, checked, value);
    cui_set_tab_stop(ui, id, false);
}
static cui_id cui_content_child(cui_id id, unsigned index) {
    return id ^ (UINT64_C(0x874e65fa19c80721) + UINT64_C(0x9e3779b97f4a7c15) * index);
}
bool cui_alert(cui_context *ui, cui_id id, const cui_theme *t, cui_alert_options o) {
    if (!ui || !t || o.kind < CUI_ALERT_INFO || o.kind > CUI_ALERT_DEFAULT ||
        (o.visible && !*o.visible))
        return false;
    float scale = cui_content_positive(o.scale, 1);
    cui_color accent = o.kind == CUI_ALERT_ERROR     ? t->danger
                       : o.kind == CUI_ALERT_SUCCESS ? t->success
                       : o.kind == CUI_ALERT_WARNING ? t->warning
                       : o.kind == CUI_ALERT_DEFAULT ? t->text
                                                     : t->info;
    const char *kind = o.kind == CUI_ALERT_SUCCESS   ? "Success"
                       : o.kind == CUI_ALERT_WARNING ? "Warning"
                       : o.kind == CUI_ALERT_ERROR   ? "Error"
                                                     : "Information";
    cui_begin_box(ui, id,
                  (cui_box){.width = o.width > 0 ? o.width : 0,
                            .padding = t->padding * scale,
                            .gap = t->gap * scale,
                            .radius = o.banner ? 0 : t->radius,
                            .border = o.kind == CUI_ALERT_DEFAULT ? t->border : accent,
                            .border_width = o.banner ? 0 : 1,
                            .background = o.kind == CUI_ALERT_DEFAULT
                                              ? t->background
                                              : cui_content_blend(t->surface, accent, .1f)});
    cui_content_semantic(ui, id, CUI_ROLE_ALERT,
                         o.accessible_name.length ? o.accessible_name
                         : o.message.length       ? o.message
                                                  : o.title,
                         false, false, 0);
    cui_begin_box(ui, cui_content_child(id, 2),
                  (cui_box){.row = true, .align_y = CUI_ALIGN_CENTER, .gap = t->gap * scale});
    if (o.icon.length)
        cui_text(ui, o.icon, t->font_size * .8f * scale, accent);
    else {
        const char *name = o.kind == CUI_ALERT_SUCCESS   ? "circle-check"
                           : o.kind == CUI_ALERT_WARNING ? "triangle-alert"
                           : o.kind == CUI_ALERT_ERROR   ? "circle-alert"
                                                         : "info";
        cui_icon_named(ui, cui_content_child(id, 3), t,
                       o.icon_name.length ? o.icon_name : cui_string(name),
                       (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                          .custom_size = t->font_size * scale,
                                          .custom_color = true,
                                          .color = accent});
    }
    if (o.kind != CUI_ALERT_DEFAULT)
        cui_text(ui, cui_string(kind), t->font_size * .8f * scale, accent);
    cui_end_box(ui);
    if (!o.banner) {
        if (o.title_content)
            o.title_content(ui, t, accent, o.user);
        else if (o.title.length)
            cui_text(ui, o.title, t->font_size * scale, accent);
    }
    if (o.content)
        o.content(ui, t, accent, o.user);
    else if (o.message.length)
        cui_text(ui, o.message, t->font_size * scale, accent);
    if (o.action)
        o.action(ui, t, accent, o.user);
    bool closed = false;
    if (o.dismissible) {
        closed = cui_button(ui, cui_content_child(id, 1), t, cui_string("Dismiss"), false);
        if (closed && o.visible)
            *o.visible = false;
    }
    cui_end_box(ui);
    return closed;
}
void cui_avatar(cui_context *ui, cui_id id, const cui_theme *t, cui_str name, float size) {
    if (!ui || !t)
        return;
    if (!id) {
        cui_diagnostic diagnostic = {.error = CUI_ERROR_INVALID_ACTION,
                                     .detail = CUI_DIAGNOSTIC_ZERO_ID,
                                     .operation = "cui_avatar",
                                     .id = id};
        cui_report_diagnostic(ui, &diagnostic);
        return;
    }
    size = cui_content_positive(size, 48);
    char initials[9];
    size_t n = cui_avatar_initials(name, initials);
    cui_begin_box(ui, id,
                  (cui_box){.width = size,
                            .height = size,
                            .radius = size / 2,
                            .align_x = CUI_ALIGN_CENTER,
                            .align_y = CUI_ALIGN_CENTER,
                            .background = t->primary,
                            .border = t->border,
                            .border_width = 1});
    cui_content_semantic(ui, id, CUI_ROLE_IMAGE, name.length ? name : cui_string("Anonymous"),
                         false, false, 0);
    cui_text(ui, (cui_str){initials, n}, size * .38f, t->on_primary);
    cui_end_box(ui);
}
bool cui_avatar_image_init(cui_avatar_image *s, unsigned char *storage, size_t capacity,
                           unsigned diameter) {
    if (!s || !storage || !diameter || diameter > 512 || capacity < (size_t)diameter * diameter * 4)
        return false;
    *s = (cui_avatar_image){.storage = storage, .capacity = capacity, .diameter = diameter};
    return true;
}
bool cui_avatar_image_prepare(cui_avatar_image *s, cui_bitmap source) {
    if (!s || !s->storage || !s->diameter || s->diameter > 512 ||
        s->capacity < (size_t)s->diameter * s->diameter * 4 || !cui_bitmap_valid(source) ||
        source.width > 16384 || source.height > 16384)
        return false;
    size_t source_bytes = source.stride * source.height;
    uintptr_t a = (uintptr_t)s->storage, b = (uintptr_t)source.pixels;
    size_t bytes = (size_t)s->diameter * s->diameter * 4;
    if ((a >= b && a - b < source_bytes) || (b >= a && b - a < bytes))
        return false;
    if (s->ready && s->source.pixels == source.pixels && s->source.width == source.width &&
        s->source.height == source.height && s->source.stride == source.stride &&
        s->source.revision == source.revision)
        return true;
    double extent = fmin(source.width, source.height), scale = extent / s->diameter;
    double radius = s->diameter * .5;
    for (unsigned y = 0; y < s->diameter; y++)
        for (unsigned x = 0; x < s->diameter; x++) {
            double sx = fmax(
                0, fmin(source.width - 1, (x + .5) * scale + (source.width - extent) * .5 - .5));
            double sy = fmax(
                0, fmin(source.height - 1, (y + .5) * scale + (source.height - extent) * .5 - .5));
            unsigned x0 = (unsigned)sx, y0 = (unsigned)sy;
            unsigned x1 = x0 + 1 < source.width ? x0 + 1 : x0,
                     y1 = y0 + 1 < source.height ? y0 + 1 : y0;
            double fx = sx - x0, fy = sy - y0;
            double coverage =
                fmax(0, fmin(1, radius + .5 - hypot(x + .5 - radius, y + .5 - radius)));
            unsigned char *out = s->storage + ((size_t)y * s->diameter + x) * 4;
            for (unsigned c = 0; c < 4; c++) {
                double top = source.pixels[(size_t)y0 * source.stride + x0 * 4 + c] * (1 - fx) +
                             source.pixels[(size_t)y0 * source.stride + x1 * 4 + c] * fx;
                double bottom = source.pixels[(size_t)y1 * source.stride + x0 * 4 + c] * (1 - fx) +
                                source.pixels[(size_t)y1 * source.stride + x1 * 4 + c] * fx;
                out[c] = (unsigned char)floor(
                    (top * (1 - fy) + bottom * fy) * (c == 3 ? coverage : 1) + .5);
            }
        }
    uint64_t revision = s->bitmap.revision + 1;
    s->bitmap = (cui_bitmap){s->storage, s->diameter, s->diameter, s->diameter * 4,
                             revision ? revision : 1};
    s->source = source;
    s->ready = true;
    return true;
}
bool cui_avatar_measure(cui_design_size size, float rem, cui_avatar_metrics *out) {
    cui_design_metrics shared;
    if (!out || !isfinite(rem) || rem <= 0 || rem > 512 || !cui_design_measure(size, 1, &shared))
        return false;
    float scale = rem / 16;
    float diameter = size == CUI_DESIGN_XS                              ? 16
                     : size == CUI_DESIGN_SM                            ? 24
                     : size == CUI_DESIGN_BASE || size == CUI_DESIGN_MD ? 48
                     : size == CUI_DESIGN_LG                            ? 80
                                                                        : shared.control_height * 2;
    float text = size == CUI_DESIGN_XS                              ? 10.4f
                 : size == CUI_DESIGN_SM                            ? 12
                 : size == CUI_DESIGN_BASE || size == CUI_DESIGN_MD ? 14
                 : size == CUI_DESIGN_LG ? 30
                                         : shared.font_size * (30.f / 18.f);
    scale = fminf(scale, 512 / diameter);
    *out = (cui_avatar_metrics){.diameter = fminf(diameter * scale, 512),
                                .font_size = text * scale,
                                .text_style = {.weight = size >= CUI_DESIGN_LG
                                                             ? CUI_TEXT_WEIGHT_SEMIBOLD
                                                             : CUI_TEXT_WEIGHT_NORMAL}};
    return true;
}
static uint8_t cui_avatar_channel(double linear) {
    double srgb = linear <= .0031308 ? linear * 12.92 : 1.055 * pow(linear, 1 / 2.4) - .055;
    return (uint8_t)floor(fmax(0, fmin(1, srgb)) * 255 + .5);
}
static cui_color cui_avatar_oklch(double light, double chroma, unsigned hue) {
    double angle = hue * (3.14159265358979323846 / 6);
    double a = chroma * cos(angle), b = chroma * sin(angle);
    double l = light + .3963377774 * a + .2158037573 * b;
    double m = light - .1055613458 * a - .0638541728 * b;
    double n = light - .0894841775 * a - 1.2914855480 * b;
    l = l * l * l;
    m = m * m * m;
    n = n * n * n;
    return (cui_color){cui_avatar_channel(4.0767416621 * l - 3.3077115913 * m + .2309699292 * n),
                       cui_avatar_channel(-1.2684380046 * l + 2.6097574011 * m - .3413193965 * n),
                       cui_avatar_channel(-.0041960863 * l - .7034186147 * m + 1.7076147010 * n),
                       255};
}
bool cui_avatar_identity(cui_str name, bool dark, bool custom_hue, unsigned hue,
                         cui_avatar_palette *out) {
    if (!out || (custom_hue && hue >= 12))
        return false;
    if (!custom_hue) {
        char initials[9];
        size_t length = cui_avatar_initials(name, initials);
        uint32_t hash = UINT32_C(2166136261);
        for (size_t i = 0; i < length; i++)
            hash = (hash ^ (unsigned char)initials[i]) * UINT32_C(16777619);
        hue = hash % 12;
    }
    *out = dark ? (cui_avatar_palette){cui_avatar_oklch(.30, .05, hue),
                                       cui_avatar_oklch(.82, .11, hue),
                                       cui_avatar_oklch(.36, .06, hue)}
                : (cui_avatar_palette){cui_avatar_oklch(.97, .032, hue),
                                       cui_avatar_oklch(.50, .145, hue),
                                       cui_avatar_oklch(.89, .05, hue)};
    return true;
}
static bool cui_avatar_private_submit(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                                      cui_avatar_options o,
                                      cui_component_activation_handler handler,
                                      cui_component_action_result *result) {
    if (!ui || !t)
        return false;
    if (!id) {
        cui_diagnostic diagnostic = {.error = CUI_ERROR_INVALID_ACTION,
                                     .detail = CUI_DIAGNOSTIC_ZERO_ID,
                                     .operation = "cui_avatar_ex",
                                     .id = id};
        cui_report_diagnostic(ui, &diagnostic);
        return false;
    }
    cui_avatar_metrics metrics = {0};
    bool variant = cui_avatar_measure(o.design_size, o.rem_size == 0 ? 16 : o.rem_size, &metrics);
    float size = variant ? metrics.diameter : fminf(cui_content_positive(o.size, 48), 512);
    float ring = isfinite(o.ring_width) ? fmaxf(0, fminf(o.ring_width, size * .25f)) : 0;
    bool photo = o.image_state && cui_avatar_image_prepare(o.image_state, o.image);
    cui_avatar_palette palette = {0};
    bool identity =
        !photo && name.length && o.identity_colors &&
        cui_avatar_identity(name, t->dark, o.custom_identity_hue, o.identity_hue, &palette);
    cui_color background = o.custom_background ? o.background
                           : identity          ? palette.background
                                               : t->primary;
    cui_color foreground = o.custom_foreground ? o.foreground
                           : identity          ? palette.foreground
                                               : t->on_primary;
    cui_begin_box(
        ui, id,
        (cui_box){.width = size,
                  .height = size,
                  .fixed_width = true,
                  .fixed_height = true,
                  .radius = size * .5f,
                  .padding = photo ? ring : 0,
                  .align_x = CUI_ALIGN_CENTER,
                  .align_y = CUI_ALIGN_CENTER,
                  .background = photo ? (o.custom_ring ? o.ring : t->surface) : background,
                  .border = cui_focused(ui) == id ? t->primary
                            : identity            ? palette.border
                                                  : t->border,
                  .border_width = photo ? 0 : 1});
    cui_interaction interaction =
        cui_interact(ui, id, o.interactive ? CUI_ROLE_BUTTON : CUI_ROLE_IMAGE,
                     name.length ? name : cui_string("Anonymous"), o.disabled, false, 0);
    if (interaction.focus_visible)
        cui_set_outline(ui, 2, t->primary,
                        (cui_corner_radii){size / 2, size / 2, size / 2, size / 2});
    if (photo)
        cui_paint_bitmap(ui, cui_content_child(id, 31),
                         (cui_bitmap_paint){.bitmap = o.image_state->bitmap, .opacity = 1},
                         (cui_box){.width = size - 2 * ring,
                                   .height = size - 2 * ring,
                                   .fixed_width = true,
                                   .fixed_height = true});
    else if (!name.length && o.anonymous_icon) {
        cui_icon_options icon = {.size = CUI_ICON_CUSTOM,
                                 .custom_size = size * .6f,
                                 .custom_color = true,
                                 .color = foreground,
                                 .decorative = true};
        if (o.placeholder)
            cui_icon(ui, cui_content_child(id, 32), t, o.placeholder, icon);
        else
            cui_icon_named(ui, cui_content_child(id, 32), t,
                           o.placeholder_name.length ? o.placeholder_name : cui_string("user"),
                           icon);
    } else {
        char initials[9];
        size_t n = cui_avatar_initials(name, initials);
        cui_text_ex(ui, (cui_str){initials, n}, variant ? metrics.font_size : size * .38f,
                    foreground, 0, metrics.text_style);
    }
    cui_end_box(ui);
    size_t count;
    const cui_semantic *items = cui_semantics(ui, &count);
    for (size_t i = 0; i < count; i++)
        if (items[i].id == id) {
            cui_semantic_details d = items[i].details;
            d.pointer_activatable = o.interactive;
            d.focusable = o.interactive;
            d.no_focus = !o.interactive;
            cui_set_semantic_details(ui, id, d);
            cui_set_tab_stop(ui, id, o.interactive);
            if (o.interactive)
                cui_set_cursor(ui, id, o.disabled ? CUI_CURSOR_NOT_ALLOWED : CUI_CURSOR_POINTER);
            break;
        }
    bool activated = false;
    size_t n;
    cui_frame_events(ui, &n);
    for (size_t i = 0; i < n; i++)
        if (o.interactive && !o.disabled && !cui_event_consumed(ui, i) &&
            cui_event_activation(ui, i) == id) {
            cui_component_event_metadata event;
            bool observe = result && result->submitted;
            if (observe) {
                cui_activation_snapshot snapshot;
                if (!cui_event_activation_snapshot(ui, i, &snapshot) || snapshot.target != id ||
                    !cui_component_event_from_activation(&event, CUI_COMPONENT_EVENT_ACTIVATE,
                                                         &snapshot)) {
                    (void)cui_component_event_init(&event, CUI_COMPONENT_EVENT_ACTIVATE, id,
                                                   CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
                    event.frame_sequence = cui_frame_sequence(ui);
                    event.flags = CUI_COMPONENT_EVENT_HAS_FRAME;
                }
            }
            cui_consume_event(ui, i);
            activated = true;
            if (observe) {
                cui_control_events_private_increment(&result->activation_count,
                                                     &result->count_overflow);
                if (handler.callback)
                    handler.callback(handler.user, &event);
            }
        }
    return activated;
}
bool cui_avatar_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_str name,
                   cui_avatar_options options) {
    return cui_avatar_private_submit(ui, id, t, name, options,
                                     (cui_component_activation_handler){0}, NULL);
}
cui_component_action_result cui_avatar_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                              cui_str name, cui_avatar_options options,
                                              cui_component_activation_handler handler) {
    cui_component_action_result result = {0};
    if (!ui || !id || !theme || (name.length && !name.data))
        return result;
    result.valid = true;
    result.submitted = cui_frame_paint_enabled(ui) && cui_frame_sequence(ui) != 0;
    (void)cui_avatar_private_submit(ui, id, theme, name, options, handler, &result);
    return result;
}
typedef struct {
    cui_id id;
    size_t index;
    uint64_t key;
    cui_avatar_stack_handler handler;
} cui_avatar_private_stack_forward;
static void cui_avatar_private_stack_activate(void *user,
                                              const cui_component_event_metadata *event) {
    cui_avatar_private_stack_forward *forward = user;
    cui_avatar_stack_activation activation = {
        .event = *event, .index = forward->index, .key = forward->key};
    activation.event.target = forward->id;
    forward->handler.callback(forward->handler.user, &activation);
}
static size_t cui_avatar_private_stack_submit(cui_context *ui, cui_id id, const cui_theme *t,
                                              const cui_avatar_item *items, size_t count,
                                              cui_avatar_stack_options o,
                                              cui_avatar_stack_handler handler,
                                              cui_component_action_result *result) {
    if (!ui || !id || !t || !count || count > 64 || !items)
        return SIZE_MAX;
    if (isfinite(o.width) && o.width > 0 && o.width < 1)
        return SIZE_MAX;
    float size = fminf(cui_content_positive(o.size, 48), 512);
    float overlap = cui_content_positive(o.overlap, size * .25f);
    overlap = fminf(overlap, size * .75f);
    float step = size - overlap;
    size_t visible = o.max_visible ? o.max_visible : 3;
    if (visible > count)
        visible = count;
    if (isfinite(o.width) && o.width > 0) {
        size = fminf(size, o.width);
        overlap = fminf(overlap, size * .75f);
        step = size - overlap;
        size_t slots = 1 + (size_t)fminf(64, fmaxf(0, floorf((o.width - size) / step)));
        if (visible + (visible < count ? 1 : 0) > slots)
            visible = count <= slots ? count : slots - 1;
    }
    size_t slots = visible + (visible < count ? 1 : 0);
    float width = size + step * (float)(slots - 1);
    cui_begin_box(
        ui, id,
        (cui_box){.width = width, .height = size, .fixed_width = true, .fixed_height = true});
    cui_content_semantic(ui, id, CUI_ROLE_GROUP, o.name.length ? o.name : cui_string("People"),
                         false, false, 0);
    size_t activated = SIZE_MAX;
    for (size_t i = 0; i < visible; i++) {
        cui_id child = id ^ (UINT64_C(0x7396dacf16000000) +
                             (items[i].key ? items[i].key : i + 1) * UINT64_C(0x9e3779b97f4a7c15));
        cui_begin_absolute_box(ui, cui_content_child(child, 32), (cui_box){0},
                               (cui_rect){step * (float)i, 0, size, size});
        cui_avatar_private_stack_forward forward = {id, i, items[i].key, handler};
        if (cui_avatar_private_submit(
                ui, child, t, items[i].name,
                (cui_avatar_options){.size = size,
                                     .ring_width = o.ring_width,
                                     .image = items[i].image,
                                     .image_state = items[i].image_state,
                                     .custom_ring = true,
                                     .ring = t->background,
                                     .interactive = o.interactive,
                                     .disabled = o.disabled},
                (cui_component_activation_handler){
                    handler.callback ? cui_avatar_private_stack_activate : NULL, &forward},
                result))
            activated = i;
        cui_end_box(ui);
    }
    if (visible < count) {
        char label[48], text[32];
        snprintf(label, sizeof label, "%zu more people", count - visible);
        snprintf(text, sizeof text, "+%zu", count - visible);
        cui_id child = cui_content_child(id, 99);
        cui_begin_absolute_box(ui, child,
                               (cui_box){.radius = size * .5f,
                                         .background = t->secondary,
                                         .border = t->background,
                                         .border_width = o.ring_width,
                                         .align_x = CUI_ALIGN_CENTER,
                                         .align_y = CUI_ALIGN_CENTER},
                               (cui_rect){step * (float)visible, 0, size, size});
        cui_content_semantic(ui, child, CUI_ROLE_IMAGE, cui_string(label), false, false, 0);
        cui_text(ui, cui_string(text), size * .28f, t->text);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return activated;
}
size_t cui_avatar_stack(cui_context *ui, cui_id id, const cui_theme *theme,
                        const cui_avatar_item *items, size_t count,
                        cui_avatar_stack_options options) {
    return cui_avatar_private_stack_submit(ui, id, theme, items, count, options,
                                           (cui_avatar_stack_handler){0}, NULL);
}
cui_avatar_stack_event_result cui_avatar_stack_events(cui_context *ui, cui_id id,
                                                      const cui_theme *theme,
                                                      const cui_avatar_item *items, size_t count,
                                                      cui_avatar_stack_options options,
                                                      cui_avatar_stack_handler handler) {
    cui_avatar_stack_event_result result = {.control = SIZE_MAX};
    if (!ui || !id || !theme || !items || !count || count > 64 ||
        (isfinite(options.width) && options.width > 0 && options.width < 1) ||
        (options.name.length && !options.name.data))
        return result;
    for (size_t i = 0; i < count; i++)
        if (items[i].name.length && !items[i].name.data)
            return result;
    result.action.valid = true;
    result.action.submitted = cui_frame_paint_enabled(ui) && cui_frame_sequence(ui) != 0;
    result.control = cui_avatar_private_stack_submit(ui, id, theme, items, count, options, handler,
                                                     &result.action);
    return result;
}
void cui_begin_bubble(cui_context *ui, cui_id id, const cui_theme *t, cui_bubble_variant v,
                      float width) {
    if (!ui || !t)
        return;
    cui_color bg = t->surface;
    if (v == CUI_BUBBLE_FILLED)
        bg = t->primary;
    else if (v == CUI_BUBBLE_SECONDARY || v == CUI_BUBBLE_MUTED)
        bg = cui_content_blend(t->surface, t->border, .5f);
    else if (v == CUI_BUBBLE_TINTED)
        bg = cui_content_blend(t->surface, t->primary, .15f);
    else if (v == CUI_BUBBLE_DESTRUCTIVE)
        bg = cui_content_blend(t->surface, t->danger, .15f);
    else if (v >= CUI_BUBBLE_INFO && v <= CUI_BUBBLE_ERROR) {
        cui_color accent = v == CUI_BUBBLE_SUCCESS   ? t->success
                           : v == CUI_BUBBLE_WARNING ? t->warning
                           : v == CUI_BUBBLE_ERROR   ? t->danger
                                                     : t->info;
        bg = cui_content_blend(t->surface, accent, .15f);
    } else if (v == CUI_BUBBLE_GHOST)
        bg.a = 0;
    else if (v == CUI_BUBBLE_OUTLINE)
        bg = t->background;
    cui_begin_box(ui, id,
                  (cui_box){.width = width > 0 ? width : 0,
                            .padding = v == CUI_BUBBLE_GHOST ? 0 : t->padding,
                            .gap = t->gap,
                            .radius = v == CUI_BUBBLE_GHOST ? 0 : t->radius * 2,
                            .background = bg,
                            .border = t->border,
                            .border_width = v == CUI_BUBBLE_OUTLINE ? 1 : 0});
}
void cui_end_bubble(cui_context *ui) {
    cui_end_box(ui);
}
cui_color cui_bubble_foreground(const cui_theme *t, cui_bubble_variant v) {
    if (!t)
        return (cui_color){0};
    switch (v) {
    case CUI_BUBBLE_FILLED:
        return t->on_primary;
    case CUI_BUBBLE_DESTRUCTIVE:
    case CUI_BUBBLE_ERROR:
        return t->danger;
    case CUI_BUBBLE_INFO:
        return t->info;
    case CUI_BUBBLE_SUCCESS:
        return t->success;
    case CUI_BUBBLE_WARNING:
        return t->warning;
    default:
        return t->text;
    }
}
static void cui_bubble_reactions(cui_context *ui, cui_id id, const cui_theme *t,
                                 cui_bubble_options o) {
    bool floating = o.state && o.state->measured;
    if (!floating)
        cui_begin_box(ui, cui_content_child(id, 4),
                      (cui_box){.width = 1,
                                .width_percent = true,
                                .align_x = o.reaction_alignment == CUI_BUBBLE_OUTGOING
                                               ? CUI_ALIGN_END
                                               : CUI_ALIGN_START});
    cui_box pill = {.row = true,
                    .gap = t->gap,
                    .radius = t->radius * 2,
                    .padding = t->padding * .25f,
                    .background = cui_content_blend(t->surface, t->border, .5f),
                    .border = t->background,
                    .border_width = 2};
    if (floating) {
        cui_bubble_state *state = o.state;
        float inset = fminf(t->padding, state->surface.w * .25f);
        float x = state->surface.x - state->row.x + inset;
        if (o.reaction_alignment == CUI_BUBBLE_OUTGOING)
            x = state->surface.x - state->row.x + state->surface.w - state->reactions.w - inset;
        float y = state->surface.y - state->row.y;
        y += o.reaction_side == CUI_BUBBLE_REACTIONS_TOP
                 ? -state->reactions.h * .75f
                 : state->surface.h - state->reactions.h * .25f;
        cui_begin_absolute_box(ui, cui_content_child(id, 5), pill,
                               (cui_rect){x, y, state->reactions.w, state->reactions.h});
    } else
        cui_begin_box(ui, cui_content_child(id, 5), pill);
    o.reactions(ui, t, t->text, o.user);
    cui_end_box(ui);
    if (!floating)
        cui_end_box(ui);
}
void cui_bubble(cui_context *ui, cui_id id, const cui_theme *t, cui_bubble_options o) {
    if (!ui || !t || o.variant < CUI_BUBBLE_FILLED || o.variant > CUI_BUBBLE_ERROR ||
        o.alignment < CUI_BUBBLE_INCOMING || o.alignment > CUI_BUBBLE_OUTGOING ||
        o.reaction_alignment < CUI_BUBBLE_INCOMING || o.reaction_alignment > CUI_BUBBLE_OUTGOING ||
        o.reaction_side < CUI_BUBBLE_REACTIONS_BOTTOM ||
        o.reaction_side > CUI_BUBBLE_REACTIONS_TOP || o.announcement < CUI_BUBBLE_ANNOUNCE_NONE ||
        o.announcement > CUI_BUBBLE_ANNOUNCE_ASSERTIVE)
        return;
    float row_width = cui_content_positive(o.width, 0);
    if (o.state) {
        float font_size = isfinite(t->font_size) ? t->font_size : 0;
        float padding = isfinite(t->padding) ? t->padding : 0;
        unsigned configuration = (unsigned)o.variant | ((unsigned)o.alignment << 4) |
                                 ((unsigned)o.reaction_side << 5) |
                                 ((unsigned)o.reaction_alignment << 6) | (o.reactions ? 128u : 0u);
        float width = row_width;
        bool reset = o.state->revision != o.content_revision || o.state->width != width ||
                     o.state->configuration != configuration || o.state->font_size != font_size ||
                     o.state->padding != padding;
        cui_rect surface = cui_bounds(ui, cui_content_child(id, 3));
        cui_rect reactions = cui_bounds(ui, cui_content_child(id, 5));
        cui_rect row = cui_bounds(ui, id);
        if (o.state->measured && row.w != o.state->row.w)
            reset = true;
        bool measured = !reset && o.reactions && surface.w > 0 && surface.h > 0 &&
                        reactions.w > 0 && reactions.h > 0;
        if (reset || measured != o.state->measured ||
            memcmp(&surface, &o.state->surface, sizeof surface) ||
            memcmp(&reactions, &o.state->reactions, sizeof reactions) ||
            memcmp(&row, &o.state->row, sizeof row)) {
            *o.state = (cui_bubble_state){.surface = surface,
                                          .reactions = reactions,
                                          .row = row,
                                          .measured = measured,
                                          .revision = o.content_revision,
                                          .width = width,
                                          .configuration = configuration,
                                          .font_size = font_size,
                                          .padding = padding};
            cui_invalidate(ui);
        }
    }
    cui_begin_box(
        ui, id,
        (cui_box){.width = row_width > 0 ? row_width : 1,
                  .width_percent = !(row_width > 0),
                  .gap = t->gap * .5f,
                  .align_x = o.alignment == CUI_BUBBLE_OUTGOING ? CUI_ALIGN_END : CUI_ALIGN_START});
    cui_role role = o.announcement == CUI_BUBBLE_ANNOUNCE_ASSERTIVE ? CUI_ROLE_ALERT
                    : o.announcement == CUI_BUBBLE_ANNOUNCE_POLITE  ? CUI_ROLE_STATUS
                                                                    : CUI_ROLE_GROUP;
    cui_content_semantic(ui, id, role, o.accessible_name.length ? o.accessible_name : o.message,
                         false, false, 0);
    if (o.reactions && o.reaction_side == CUI_BUBBLE_REACTIONS_TOP) {
        if (o.state && o.state->measured) {
            cui_begin_box(ui, cui_content_child(id, 6),
                          (cui_box){.height = o.state->reactions.h * .75f, .fixed_height = true});
            cui_end_box(ui);
        }
        cui_bubble_reactions(ui, id, t, o);
    }
    cui_begin_box(
        ui, cui_content_child(id, 2),
        (cui_box){.width = o.variant == CUI_BUBBLE_GHOST ? 1 : .8f,
                  .width_percent = true,
                  .align_x = o.alignment == CUI_BUBBLE_OUTGOING ? CUI_ALIGN_END : CUI_ALIGN_START});
    cui_begin_bubble(ui, cui_content_child(id, 3), t, o.variant, 0);
    cui_color fg = cui_bubble_foreground(t, o.variant);
    if (o.content)
        o.content(ui, t, fg, o.user);
    else if (o.message.length)
        cui_text(ui, o.message, t->font_size, fg);
    cui_end_bubble(ui);
    cui_end_box(ui);
    if (o.reactions && o.reaction_side != CUI_BUBBLE_REACTIONS_TOP) {
        cui_bubble_reactions(ui, id, t, o);
        if (o.state && o.state->measured) {
            cui_begin_box(ui, cui_content_child(id, 6),
                          (cui_box){.height = o.state->reactions.h * .75f, .fixed_height = true});
            cui_end_box(ui);
        }
    }
    cui_end_box(ui);
}
cui_empty_style cui_empty_default_style(const cui_theme *t, cui_empty_part part,
                                        cui_empty_media_variant variant, float rem) {
    cui_empty_style s = {0};
    if (!t)
        return s;
    if (!isfinite(rem) || rem <= 0)
        rem = 16;
    s.foreground = t->text;
    s.font_size = rem * .875f;
    s.box.align_x = CUI_ALIGN_CENTER;
    if (part == CUI_EMPTY_ROOT) {
        s.box.width = -1;
        s.box.height = -1;
        s.box.padding = rem * 1.5f;
        s.box.gap = rem;
        s.box.radius = t->radius_lg;
        s.box.border = t->border;
        s.box.align_y = CUI_ALIGN_CENTER;
    } else if (part == CUI_EMPTY_HEADER || part == CUI_EMPTY_CONTENT) {
        s.box.width = -1;
        s.box.max_width = rem * 24;
        s.box.gap = rem * (part == CUI_EMPTY_HEADER ? .5f : .625f);
    } else if (part == CUI_EMPTY_MEDIA) {
        s.font_size = rem;
        s.box.align_y = CUI_ALIGN_CENTER;
        if (variant == CUI_EMPTY_MEDIA_ICON) {
            s.box.width = s.box.height = rem * 2;
            s.box.fixed_width = s.box.fixed_height = true;
            s.box.background = t->secondary;
            s.box.radius = t->radius_lg;
        }
    } else if (part == CUI_EMPTY_TITLE) {
        s.text.weight = CUI_TEXT_WEIGHT_MEDIUM;
    } else if (part == CUI_EMPTY_DESCRIPTION) {
        s.box.width = -1;
        s.foreground = t->muted;
        s.line_height = s.font_size * 1.625f;
    }
    return s;
}
static bool cui_empty_style_valid(const cui_empty_style *s) {
    if (!s)
        return true;
    const cui_box *b = &s->box;
    float values[] = {b->padding,
                      b->gap,
                      b->radius,
                      b->border_width,
                      b->max_width,
                      b->max_height,
                      s->font_size,
                      s->line_height,
                      b->corner_radii.top_left,
                      b->corner_radii.top_right,
                      b->corner_radii.bottom_left,
                      b->corner_radii.bottom_right,
                      b->border_edges.left,
                      b->border_edges.right,
                      b->border_edges.top,
                      b->border_edges.bottom};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        if (!isfinite(values[i]) || values[i] < 0)
            return false;
    return isfinite(b->width) && b->width >= -1 && isfinite(b->height) && b->height >= -1 &&
           isfinite(b->offset_x) && isfinite(b->offset_y) && isfinite(b->opacity) &&
           s->font_size > 0 && b->align_x <= CUI_ALIGN_END && b->align_x >= CUI_ALIGN_START &&
           b->align_y <= CUI_ALIGN_END && b->align_y >= CUI_ALIGN_START &&
           s->text.weight >= CUI_TEXT_WEIGHT_NORMAL && s->text.weight <= CUI_TEXT_WEIGHT_BOLD;
}
static bool cui_empty_span_valid(cui_str s) {
    return !s.length || s.data;
}
static bool cui_empty_slot_valid(const cui_empty_slot *s) {
    return !s || (cui_empty_style_valid(s->style) && cui_empty_span_valid(s->text));
}
static void cui_empty_render_slot(cui_context *ui, cui_id id, const cui_theme *t,
                                  const cui_empty_slot *slot, cui_empty_style style) {
    if (!slot)
        return;
    if (slot->style)
        style = *slot->style;
    cui_begin_box(ui, id, style.box);
    if (slot->text.length)
        cui_text_ex(ui, slot->text, style.font_size, style.foreground, style.line_height,
                    style.text);
    if (slot->children) {
        cui_theme inherited = *t;
        inherited.font_size = style.font_size;
        inherited.text = style.foreground;
        slot->children(ui, &inherited, style.foreground, slot->user);
    }
    cui_end_box(ui);
}
bool cui_empty_ex(cui_context *ui, cui_id id, const cui_theme *t, cui_empty_options o) {
    const cui_empty_header *h = o.header;
    if (!ui || !id || !t || !isfinite(o.rem) || o.rem < 0 || o.rem > 4096 ||
        !cui_empty_style_valid(o.style) || !cui_empty_span_valid(o.accessible_name) ||
        !cui_empty_slot_valid(o.content) ||
        (h &&
         (!cui_empty_style_valid(h->style) || !cui_empty_slot_valid(h->media) ||
          !cui_empty_slot_valid(h->title) || !cui_empty_slot_valid(h->description) ||
          h->media_variant < CUI_EMPTY_MEDIA_DEFAULT || h->media_variant > CUI_EMPTY_MEDIA_ICON)))
        return false;
    float rem = o.rem ? o.rem : 16;
    cui_empty_style root =
        o.style ? *o.style
                : cui_empty_default_style(t, CUI_EMPTY_ROOT, CUI_EMPTY_MEDIA_DEFAULT, rem);
    cui_begin_box(ui, id, root.box);
    cui_content_semantic(ui, id, CUI_ROLE_GROUP, o.accessible_name, false, false, 0);
    if (h) {
        cui_empty_style header =
            h->style ? *h->style
                     : cui_empty_default_style(t, CUI_EMPTY_HEADER, h->media_variant, rem);
        if (!h->style)
            header.foreground = root.foreground;
        cui_theme header_theme = *t;
        header_theme.text = header.foreground;
        cui_begin_box(ui, cui_content_child(id, 1), header.box);
        cui_empty_render_slot(
            ui, cui_content_child(id, 2), t, h->media,
            cui_empty_default_style(&header_theme, CUI_EMPTY_MEDIA, h->media_variant, rem));
        /* Media's mb_2 is separate from the header's gap_2. */
        if (h->media) {
            cui_begin_box(ui, cui_content_child(id, 7),
                          (cui_box){.height = 0, .fixed_height = true});
            cui_end_box(ui);
        }
        cui_empty_render_slot(
            ui, cui_content_child(id, 3), t, h->title,
            cui_empty_default_style(&header_theme, CUI_EMPTY_TITLE, h->media_variant, rem));
        cui_empty_render_slot(
            ui, cui_content_child(id, 4), t, h->description,
            cui_empty_default_style(t, CUI_EMPTY_DESCRIPTION, h->media_variant, rem));
        cui_end_box(ui);
    }
    cui_theme root_theme = *t;
    root_theme.text = root.foreground;
    cui_empty_render_slot(
        ui, cui_content_child(id, 5), t, o.content,
        cui_empty_default_style(&root_theme, CUI_EMPTY_CONTENT, CUI_EMPTY_MEDIA_DEFAULT, rem));
    if (o.children)
        o.children(ui, t, root.foreground, o.user);
    cui_end_box(ui);
    return true;
}
void cui_empty(cui_context *ui, cui_id id, const cui_theme *t, cui_str title, cui_str description,
               float width) {
    if (!ui || !t)
        return;
    cui_begin_box(ui, id,
                  (cui_box){.width = width > 0 ? width : 0,
                            .padding = t->padding * 2,
                            .gap = t->gap,
                            .border = t->border,
                            .border_width = 1,
                            .radius = t->radius});
    cui_content_semantic(ui, id, CUI_ROLE_GROUP, title, false, false, 0);
    if (title.length)
        cui_text(ui, title, t->font_size * 1.2f, t->text);
    if (description.length)
        cui_text(ui, description, t->font_size, t->muted);
    cui_end_box(ui);
}
void cui_begin_group_box(cui_context *ui, cui_id id, const cui_theme *t, cui_str title,
                         float width) {
    if (!ui || !t)
        return;
    cui_begin_box(ui, id,
                  (cui_box){.width = width > 0 ? width : 0,
                            .padding = t->padding,
                            .gap = t->gap,
                            .border = t->border,
                            .border_width = 1,
                            .radius = t->radius,
                            .background = t->surface});
    cui_content_semantic(ui, id, CUI_ROLE_GROUP, title, false, false, 0);
    if (title.length)
        cui_text(ui, title, t->font_size, t->text);
}
void cui_end_group_box(cui_context *ui) {
    cui_end_box(ui);
}
void cui_kbd(cui_context *ui, cui_id id, const cui_theme *t, cui_str chord) {
    if (!ui || !t)
        return;
    cui_begin_box(ui, id,
                  (cui_box){.padding = 4,
                            .radius = t->radius * .5f,
                            .background = t->background,
                            .border = t->border,
                            .border_width = 1});
    cui_content_semantic(ui, id, CUI_ROLE_LABEL, chord, false, false, 0);
    cui_text(ui, chord, t->font_size * .8f, t->text);
    cui_end_box(ui);
}
void cui_spinner(cui_context *ui, cui_id id, const cui_theme *t, float size, float phase,
                 bool reduced) {
    if (!ui || !t)
        return;
    size = cui_content_positive(size, 24);
    phase = isfinite(phase) ? phase - floorf(phase) : 0;
    unsigned head = reduced ? 0 : (unsigned)(phase * 8);
    const unsigned ring[9] = {0, 1, 2, 7, 8, 3, 6, 5, 4};
    cui_begin_box(ui, id, (cui_box){.width = size, .height = size});
    cui_content_semantic(ui, id, CUI_ROLE_STATUS, cui_string("Loading"), false, false, 0);
    for (unsigned y = 0; y < 3; ++y) {
        cui_begin_box(ui, cui_content_child(id, 1 + y),
                      (cui_box){.row = true, .width = size, .height = size / 3});
        for (unsigned x = 0; x < 3; ++x) {
            unsigned index = ring[y * 3 + x];
            cui_color color = t->primary;
            if (index == 8)
                color.a = 0;
            else
                color.a =
                    (uint8_t)(color.a * (.25f + .75f * (float)(8 - ((index + 8 - head) % 8)) / 8));
            cui_begin_box(ui, cui_content_child(id, 5 + y * 3 + x),
                          (cui_box){.width = size / 3, .height = size / 3, .padding = size / 12});
            cui_begin_box(ui, cui_content_child(id, 20 + y * 3 + x),
                          (cui_box){.width = size / 6,
                                    .height = size / 6,
                                    .radius = size / 12,
                                    .background = color});
            cui_end_box(ui);
            cui_end_box(ui);
        }
        cui_end_box(ui);
    }
    cui_end_box(ui);
    if (!reduced)
        cui_invalidate(ui);
}
void cui_skeleton(cui_context *ui, cui_id id, const cui_theme *t, float width, float height,
                  float radius, float phase, bool secondary, bool reduced) {
    if (!ui || !t)
        return;
    phase = isfinite(phase) ? phase - floorf(phase) : 0;
    float alpha = reduced ? 1 : .75f + .25f * cosf(phase * 6.283185307f);
    cui_color color = t->border;
    color.a = (uint8_t)(color.a * alpha * (secondary ? .5f : 1));
    cui_begin_box(ui, id,
                  (cui_box){.width = cui_content_positive(width, 160),
                            .height = cui_content_positive(height, 16),
                            .radius = isfinite(radius) && radius >= 0 ? radius : t->radius,
                            .background = color});
    cui_end_box(ui);
    if (!reduced)
        cui_invalidate(ui);
}
static bool cui_spinner_resolve_size(cui_spinner_options o, float *size) {
    float value = o.size;
    if (o.sizing) {
        cui_spinner_sizing sizing = *o.sizing;
        if (sizing.size < CUI_SPINNER_CUSTOM || sizing.size > CUI_SPINNER_LARGE)
            return false;
        if (sizing.size == CUI_SPINNER_CUSTOM)
            value = sizing.pixels;
        else {
            if (!isfinite(sizing.rem_size) || sizing.rem_size < 0 || sizing.rem_size > 4096)
                return false;
            const float ratios[] = {0, .75f, .875f, 1, 1.5f};
            value = (sizing.rem_size ? sizing.rem_size : 16) * ratios[sizing.size];
        }
    }
    if (!isfinite(value) || value <= 0 || value > 4096)
        return false;
    *size = value;
    return true;
}
/* Preserve the shared sampler's clock validation/default quadratic behavior,
 * but do not impose Skeleton's bounded opacity policy on Spinner rotation. */
static bool cui_spinner_sample(cui_loading_motion *state, double now, float period, bool reduced,
                               cui_loading_ease_fn ease, void *user,
                               cui_loading_motion_result *out) {
    cui_loading_motion next = *state;
    cui_loading_motion_result result;
    if (!cui_loading_motion_sample(&next, now, period, reduced, false, NULL, NULL, &result))
        return false;
    if (ease) {
        float phase = reduced ? 0 : (float)(fmod(next.last_time - next.started, period) / period);
        float turns = ease(phase, user);
        if (!isfinite(turns))
            return false;
        result.phase = turns;
    }
    *state = next;
    *out = result;
    return true;
}
bool cui_spinner_ex(cui_context *ui, cui_id id, const cui_theme *theme, cui_loading_motion *state,
                    cui_spinner_options o, cui_loading_motion_result *out) {
    float size;
    if (!ui || !id || !theme || !state || !out || !cui_spinner_resolve_size(o, &size) ||
        !isfinite(o.duration) || o.duration < 0 || o.duration > 60)
        return false;
    cui_loading_motion next = *state;
    cui_loading_motion_result result;
    if (!cui_spinner_sample(&next, o.now_seconds, o.duration ? o.duration : .8f,
                            o.reduced_motion || cui_reduced_motion(ui), o.ease, o.ease_user,
                            &result))
        return false;
    cui_color color = o.custom_color ? o.color : theme->text;
    /* Reduce in double before multiplication: even FLT_MAX turns stay finite. */
    double turns = fmod((double)result.phase, 1.0);
    if (turns < 0)
        turns += 1;
    float radians = (float)(turns * 6.2831853071795864769);
    bool painted =
        o.paint ? o.paint(ui, id, theme, size, color, radians, o.paint_user)
                : cui_icon_named(ui, id, theme, cui_string("loader"),
                                 (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                                    .custom_size = size,
                                                    .custom_color = true,
                                                    .color = color,
                                                    .custom_transform = true,
                                                    .transform = cui_icon_rotation(radians)}) ==
                      CUI_ICON_OK;
    if (!painted)
        return false;
    *state = next;
    *out = result;
    return true;
}
bool cui_skeleton_ex(cui_context *ui, cui_id id, const cui_theme *theme, cui_loading_motion *state,
                     cui_skeleton_options o, cui_loading_motion_result *out) {
    if (!ui || !id || !theme || !state || !out || !isfinite(o.width) || o.width <= 0 ||
        o.width > 4096 || !isfinite(o.height) || o.height <= 0 || o.height > 4096 ||
        !isfinite(o.radius) || o.radius < 0 || o.radius > 4096 || !isfinite(o.duration) ||
        o.duration < 0 || o.duration > 60)
        return false;
    cui_loading_motion next = *state;
    cui_loading_motion_result result;
    if (!cui_loading_motion_sample(&next, o.now_seconds, o.duration ? o.duration : 2,
                                   o.reduced_motion || cui_reduced_motion(ui), true, o.ease,
                                   o.ease_user, &result))
        return false;
    /* The reference has no loading boolean, status role, or busy relationship. */
    cui_color color = theme->secondary;
    color.a = (uint8_t)(color.a * result.opacity * (o.secondary ? .5f : 1));
    cui_begin_box(
        ui, id,
        (cui_box){.width = o.width, .height = o.height, .radius = o.radius, .background = color});
    cui_end_box(ui);
    *state = next;
    *out = result;
    return true;
}
void cui_divider(cui_context *ui, cui_id id, const cui_theme *t, float length, bool vertical) {
    if (!ui || !t)
        return;
    length = cui_content_positive(length, 160);
    cui_begin_box(ui, id,
                  (cui_box){.width = vertical ? 1 : length,
                            .height = vertical ? length : 1,
                            .background = t->border});
    cui_end_box(ui);
}
typedef struct {
    cui_rect content;
    float width, placed_height, maximum, font_size, padding, gap;
    uint64_t revision;
    bool measured;
} cui_hover_card_measurement;
static bool cui_content_hover_panel(cui_context *ui, cui_id id, const cui_theme *t,
                                    cui_hover_state *state, cui_str name, cui_hover_options o,
                                    bool card, bool update_model) {
    if (!ui || !t || !state)
        return false;
    if (update_model) {
        size_t n = 0;
        const cui_event *events = cui_frame_events(ui, &n);
        bool escape = false;
        for (size_t i = 0; i < n; ++i)
            if (events[i].type == CUI_EVENT_KEY_DOWN && events[i].key == CUI_KEY_ESCAPE)
                escape = true;
        cui_id focused = cui_focused(ui);
        cui_rect previous = cui_bounds(ui, id), focus_bounds = cui_bounds(ui, focused);
        bool inside_focus = card && state->open && focused && focused != o.trigger &&
                            focus_bounds.w > 0 && focus_bounds.h > 0 &&
                            focus_bounds.x >= previous.x && focus_bounds.y >= previous.y &&
                            focus_bounds.x + focus_bounds.w <= previous.x + previous.w &&
                            focus_bounds.y + focus_bounds.h <= previous.y + previous.h;
        bool changed = cui_hover_update(
            state, cui_hovered(ui, o.trigger), card && (cui_hovered(ui, id) || inside_focus),
            focused == o.trigger, escape, o.disabled, o.dt, o.open_delay, o.close_delay);
        if (changed || state->pending)
            cui_invalidate(ui);
    }
    if (!state->open)
        return false;
    cui_rect anchor = cui_bounds(ui, o.trigger);
    if (!isfinite(o.viewport.x) || !isfinite(o.viewport.y) || !isfinite(o.viewport.w) ||
        !isfinite(o.viewport.h) || o.viewport.w <= 0 || o.viewport.h <= 0 || anchor.w <= 0 ||
        anchor.h <= 0) {
        state->open = false;
        return false;
    }
    float width = cui_content_positive(o.width, card ? 240 : 200);
    bool intrinsic = card && (!isfinite(o.height) || o.height <= 0);
    cui_hover_card_measurement *measurement = NULL;
    float max_height = o.viewport.h;
    if (intrinsic && isfinite(o.max_height) && o.max_height > 0)
        max_height = fminf(max_height, o.max_height);
    float height =
        cui_content_positive(o.height, card ? 120 : t->font_size * 1.5f + t->padding * 2);
    if (intrinsic) {
        measurement = cui_resource_get(ui, id, UINT64_C(0x29ca7264138d9517), sizeof(*measurement),
                                       NULL, NULL);
        if (!measurement)
            return false;
        height = measurement->measured ? measurement->content.h + 2 * t->padding
                                       : 2 * t->padding + t->font_size;
        height = fminf(max_height, fmaxf(0, height));
        if (!measurement->measured || measurement->width != width ||
            measurement->maximum != max_height || measurement->font_size != t->font_size ||
            measurement->padding != t->padding || measurement->gap != t->gap ||
            measurement->revision != o.content_revision ||
            fabsf(measurement->placed_height - height) > .01f)
            cui_invalidate(ui);
        measurement->width = width;
        measurement->maximum = max_height;
        measurement->font_size = t->font_size;
        measurement->padding = t->padding;
        measurement->gap = t->gap;
        measurement->revision = o.content_revision;
        measurement->placed_height = height;
        measurement->measured = true;
    }
    cui_popup_result placement =
        cui_popup_place(anchor, (cui_size){width, height}, o.viewport, o.side, o.gap);
    if (intrinsic) {
        /* The first content measurement is not known at begin. Keep that frame
         * within the viewport while retaining its actual fit height for the
         * scheduled collision-placement redraw. */
        max_height = fminf(max_height, fmaxf(0, o.viewport.y + o.viewport.h - placement.bounds.y));
        placement.bounds.h = 0;
    }
    float padding =
        card ? t->padding
             : fminf(t->padding,
                     fmaxf(0, (height - cui_measure_text(ui, name, t->font_size).height) / 2));
    cui_begin_portal(ui, id,
                     (cui_box){.padding = padding,
                               .max_height = intrinsic ? max_height : 0,
                               .gap = t->gap,
                               .align_x = card ? CUI_ALIGN_START : CUI_ALIGN_CENTER,
                               .align_y = card ? CUI_ALIGN_START : CUI_ALIGN_CENTER,
                               .radius = t->radius,
                               .background = t->surface,
                               .border = t->border,
                               .border_width = 1,
                               .clip = true},
                     placement.bounds, card ? 110 : 120);
    cui_content_semantic(ui, id, card ? CUI_ROLE_GROUP : CUI_ROLE_LABEL, name, false, false, 0);
    cui_set_semantic_details(ui, id, (cui_semantic_details){.parent = o.trigger});
    if (!card) {
        size_t semantic_count = 0;
        const cui_semantic *semantics = cui_semantics(ui, &semantic_count);
        for (size_t i = 0; i < semantic_count; ++i) {
            if (semantics[i].id == o.trigger) {
                cui_semantic_details details = semantics[i].details;
                details.description = name;
                cui_set_semantic_details(ui, o.trigger, details);
                break;
            }
        }
    }
    return true;
}
bool cui_tooltip(cui_context *ui, cui_id id, const cui_theme *t, cui_hover_state *state,
                 cui_str text, cui_hover_options options) {
    if (!cui_content_hover_panel(ui, id, t, state, text, options, false, true))
        return false;
    cui_text(ui, text, t->font_size, t->text);
    cui_end_box(ui);
    return true;
}
/* Private shared placement seam; not part of the public API. */
bool cui_content_begin_hover_card_model(cui_context *, cui_id, const cui_theme *, cui_hover_state *,
                                        cui_str, cui_hover_options, bool);
bool cui_content_begin_hover_card_model(cui_context *ui, cui_id id, const cui_theme *t,
                                        cui_hover_state *state, cui_str name,
                                        cui_hover_options options, bool update_model) {
    if (!cui_content_hover_panel(ui, id, t, state, name, options, true, update_model))
        return false;
    /* A clipping container permits horizontal overflow in Clay. Constrain a
     * non-scrolling inner content box so text actually reflows at card width. */
    float width = fminf(cui_content_positive(options.width, 240), options.viewport.w);
    cui_begin_box(
        ui, cui_content_child(id, 17),
        (cui_box){.width = fmaxf(0, width - 2 * t->padding), .fixed_width = true, .gap = t->gap});
    if (!isfinite(options.height) || options.height <= 0) {
        cui_hover_card_measurement *measurement = cui_resource_get(
            ui, id, UINT64_C(0x29ca7264138d9517), sizeof(*measurement), NULL, NULL);
        if (measurement)
            cui_track_bounds(ui, cui_content_child(id, 17), &measurement->content);
    }
    return true;
}
bool cui_begin_hover_card(cui_context *ui, cui_id id, const cui_theme *t, cui_hover_state *state,
                          cui_str name, cui_hover_options options) {
    return cui_content_begin_hover_card_model(ui, id, t, state, name, options, true);
}
void cui_end_hover_card(cui_context *ui) {
    cui_end_box(ui);
    cui_end_box(ui);
}

static cui_hover_event_result cui_content_hover_events(cui_context *ui, cui_id id,
                                                       const cui_theme *theme,
                                                       cui_hover_state *state, cui_str name,
                                                       cui_hover_options options,
                                                       cui_hover_handler handler, bool card) {
    cui_hover_event_result result;
    memset(&result, 0, sizeof(result));
    if (!ui || !id || !theme || !state)
        return result;
    bool previous = state->open;
    uint64_t frame = cui_frame_sequence(ui);
    result.valid = true;
    result.control_result = card ? cui_begin_hover_card(ui, id, theme, state, name, options)
                                 : cui_tooltip(ui, id, theme, state, name, options);
    result.changed = previous != state->open;
    if (result.changed && frame && frame == cui_frame_sequence(ui) && handler.on_change) {
        cui_hover_lifecycle_event event;
        memset(&event, 0, sizeof(event));
        (void)cui_component_event_init(&event.event, CUI_COMPONENT_EVENT_LIFECYCLE, id,
                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
        event.event.flags = CUI_COMPONENT_EVENT_HAS_FRAME;
        event.event.frame_sequence = frame;
        event.previous_open = previous;
        event.open = state->open;
        result.change_count = 1;
        handler.on_change(handler.user, &event);
    }
    return result;
}
cui_hover_event_result cui_tooltip_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                          cui_hover_state *state, cui_str text,
                                          cui_hover_options options, cui_hover_handler handler) {
    return cui_content_hover_events(ui, id, theme, state, text, options, handler, false);
}
cui_hover_event_result cui_begin_hover_card_events(cui_context *ui, cui_id id,
                                                   const cui_theme *theme, cui_hover_state *state,
                                                   cui_str name, cui_hover_options options,
                                                   cui_hover_handler handler) {
    return cui_content_hover_events(ui, id, theme, state, name, options, handler, true);
}
