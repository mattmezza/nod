/* SPDX-License-Identifier: MIT */
#include "cui/cui_icon.h"
#include "../base/cui_design_metrics.h"
#include "../base/cui_icon_layout.h"
#include <math.h>
#include <string.h>
static bool cui_icon_options_valid(cui_icon_options o) {
    if (o.size < CUI_ICON_INHERITED || o.size > CUI_ICON_CUSTOM ||
        !cui_design_valid(o.design_size) || !isfinite(o.rem_size) || o.rem_size < 0 ||
        o.rem_size > 4096 || (!o.decorative && o.accessible_name.length && !o.accessible_name.data))
        return false;
    if (o.style) {
        const cui_box *b = o.style;
        float nonnegative[] = {b->padding,
                               b->gap,
                               b->radius,
                               b->border_width,
                               b->max_width,
                               b->max_height,
                               b->corner_radii.top_left,
                               b->corner_radii.top_right,
                               b->corner_radii.bottom_left,
                               b->corner_radii.bottom_right,
                               b->border_edges.left,
                               b->border_edges.right,
                               b->border_edges.top,
                               b->border_edges.bottom};
        for (size_t i = 0; i < sizeof(nonnegative) / sizeof(nonnegative[0]); ++i)
            if (!isfinite(nonnegative[i]) || nonnegative[i] < 0)
                return false;
        if (!isfinite(b->offset_x) || !isfinite(b->offset_y) || !isfinite(b->opacity) ||
            b->align_x < CUI_ALIGN_START || b->align_x > CUI_ALIGN_END ||
            b->align_y < CUI_ALIGN_START || b->align_y > CUI_ALIGN_END)
            return false;
    }
    float width = 0, height = 0;
    return cui_icon_layout_resolve(o, &width, &height);
}
cui_icon_error cui_icon(cui_context *ui, cui_id id, const cui_theme *theme, cui_icon_state *state,
                        cui_icon_options options) {
    if (!ui || !id || !theme || !state || !cui_icon_options_valid(options))
        return CUI_ICON_INVALID;
    float size = options.size == CUI_ICON_XSMALL   ? 12
                 : options.size == CUI_ICON_SMALL  ? 14
                 : options.size == CUI_ICON_MEDIUM ? 16
                 : options.size == CUI_ICON_LARGE  ? 24
                 : options.size == CUI_ICON_CUSTOM ? options.custom_size
                 : options.inherited_text_size > 0 ? options.inherited_text_size
                                                   : theme->font_size;
    if (options.size >= CUI_ICON_XSMALL && options.size <= CUI_ICON_LARGE &&
        ((isfinite(theme->font_size) && theme->font_size > 0) || options.rem_size > 0))
        size *= (options.rem_size > 0 ? options.rem_size : theme->font_size) / 16;
    if (options.design_size) {
        cui_design_metrics d;
        if (!cui_design_resolve(options.design_size, theme->font_size / 16, &d))
            return CUI_ICON_INVALID;
        size = d.indicator_size;
    }
    bool styled = options.custom_width || options.custom_height;
    float width = options.size != CUI_ICON_INHERITED || !styled ? size
                  : options.custom_width                        ? options.width
                                                                : 0;
    float height = options.size != CUI_ICON_INHERITED || !styled ? size
                   : options.custom_height                       ? options.height
                                                                 : 0;
    if (!isfinite(width) || !isfinite(height) || width < 0 || height < 0 || width > 4096 ||
        height > 4096)
        return CUI_ICON_INVALID;
    if (!cui_icon_layout_resolve(options, &width, &height))
        return CUI_ICON_INVALID;
    cui_color color = options.custom_color             ? options.color
                      : options.custom_inherited_color ? options.inherited_color
                                                       : theme->text;
    cui_icon_error result = cui_icon_prepare(
        state, width, height, options.custom_transform ? options.transform : cui_icon_identity());
    cui_box box = options.style ? *options.style : (cui_box){0};
    box.width = width;
    box.height = height;
    box.fixed_width = box.fixed_height = true;
    box.width_percent = box.height_percent = false;
    box.max_width = box.max_height = 0;
    /* SVG is a leaf painted over its full border box, not its content inset. */
    box.padding = 0;
    box.custom_padding = false;
    cui_begin_box(ui, id, box);
    if (!options.decorative && options.accessible_name.length) {
        cui_interact(ui, id, CUI_ROLE_IMAGE, options.accessible_name, false, false, 0);
        cui_set_tab_stop(ui, id, false);
        cui_set_semantic_details(ui, id, (cui_semantic_details){.no_focus = true});
    }
    cui_bitmap bitmap = cui_icon_state_bitmap(state);
    if (result == CUI_ICON_OK && cui_bitmap_valid(bitmap)) {
        cui_rect paint = cui_icon_state_paint_bounds(state);
        cui_begin_absolute_box(ui, id ^ UINT64_C(0xbed8b36147394a2c), (cui_box){0}, paint);
        float opacity = options.custom_opacity ? options.opacity : 1;
        if (!isfinite(opacity))
            opacity = 0;
        opacity = fminf(1, fmaxf(0, opacity));
        cui_paint_bitmap(
            ui, id ^ UINT64_C(0x5c136a8492ef704d),
            (cui_bitmap_paint){
                .bitmap = bitmap, .opacity = opacity, .custom_tint = true, .tint = color},
            (cui_box){
                .width = paint.w, .height = paint.h, .fixed_width = true, .fixed_height = true});
        cui_end_box(ui);
    }
    if (result == CUI_ICON_NOT_FOUND && options.debug_placeholder) {
        float opacity = options.custom_opacity ? options.opacity : 1;
        opacity = isfinite(opacity) ? fminf(1, fmaxf(0, opacity)) : 0;
        color.a = (uint8_t)(color.a * opacity);
        cui_begin_absolute_box(ui, id ^ UINT64_C(0x334af02cb941785d),
                               (cui_box){.border = color, .border_width = 1, .clip = true},
                               (cui_rect){0, 0, width, height});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return result;
}

typedef struct {
    cui_icon_state state;
    size_t catalog_index;
    double started;
    bool initialized;
    cui_theme_icon_loader_fn loader;
    void *loader_user;
    uint64_t revision;
} cui_icon_resource;
static void cui_icon_resource_collect(void *payload) {
    cui_icon_state_collect(&((cui_icon_resource *)payload)->state);
}
static void cui_icon_resource_destroy(void *payload) {
    cui_icon_state_destroy(&((cui_icon_resource *)payload)->state);
}
static cui_icon_resource *cui_icon_resource_get(cui_context *ui, cui_id id, cui_id namespace_id) {
    cui_icon_resource *resource =
        cui_resource_get(ui, id, namespace_id, sizeof(*resource), cui_icon_resource_collect,
                         cui_icon_resource_destroy);
    if (resource && !resource->initialized) {
        cui_icon_state_init(&resource->state);
        if (!cui_icon_set_limits(&resource->state, (cui_icon_limits){.max_cache_bytes = 1024 * 1024,
                                                                     .max_versions = 4}))
            return NULL;
        resource->catalog_index = SIZE_MAX;
        resource->started = cui_frame_time(ui);
        resource->initialized = true;
    }
    return resource;
}
cui_icon_error cui_icon_named(cui_context *ui, cui_id id, const cui_theme *theme, cui_str name,
                              cui_icon_options options) {
    if (!ui || !id || !theme || !name.data || !name.length || !cui_icon_options_valid(options) ||
        (options.design_size &&
         (!isfinite(theme->font_size) || theme->font_size <= 0 || theme->font_size / 16 > 32)))
        return CUI_ICON_INVALID;
    size_t index = cui_icon_catalog_index_internal(name);
    if (index == SIZE_MAX) {
        if (!options.debug_placeholder)
            return CUI_ICON_NOT_FOUND;
        cui_icon_state missing = {0};
        if (!cui_icon_set_path(&missing, cui_string("__cui_missing_debug_placeholder__"), NULL,
                               NULL)) {
            cui_icon_error error = missing.error;
            cui_icon_state_destroy(&missing);
            return error;
        }
        cui_icon_error error = cui_icon(ui, id, theme, &missing, options);
        cui_icon_state_destroy(&missing);
        return error == CUI_ICON_OK ? CUI_ICON_NOT_FOUND : error;
    }
    cui_icon_resource *resource = cui_icon_resource_get(ui, id, UINT64_C(0xe8e2ea151d9d14b4));
    if (!resource)
        return CUI_ICON_ALLOCATION;
    if (resource->catalog_index != index || resource->loader != theme->icon_loader ||
        resource->loader_user != theme->icon_loader_user ||
        resource->revision != theme->icon_revision) {
        if (!cui_icon_set_name(&resource->state, cui_icon_catalog_name(index), theme->icon_loader,
                               theme->icon_loader_user))
            return resource->state.error;
        resource->catalog_index = index;
        resource->loader = theme->icon_loader;
        resource->loader_user = theme->icon_loader_user;
        resource->revision = theme->icon_revision;
    }
    return cui_icon(ui, id, theme, &resource->state, options);
}
cui_icon_error cui_icon_spinner(cui_context *ui, cui_id id, const cui_theme *theme,
                                cui_icon_options options) {
    if (!ui || !id || !theme || !cui_icon_options_valid(options) ||
        (options.design_size &&
         (!isfinite(theme->font_size) || theme->font_size <= 0 || theme->font_size / 16 > 32)))
        return CUI_ICON_INVALID;
    cui_icon_resource *resource = cui_icon_resource_get(ui, id, UINT64_C(0x81f69ad695af247d));
    if (!resource)
        return CUI_ICON_ALLOCATION;
    if (resource->catalog_index == SIZE_MAX || resource->loader != theme->icon_loader ||
        resource->loader_user != theme->icon_loader_user ||
        resource->revision != theme->icon_revision) {
        if (!cui_icon_set_name(&resource->state, cui_string("loader"), theme->icon_loader,
                               theme->icon_loader_user))
            return resource->state.error;
        resource->catalog_index = 0;
        resource->loader = theme->icon_loader;
        resource->loader_user = theme->icon_loader_user;
        resource->revision = theme->icon_revision;
    }
    float phase =
        cui_reduced_motion(ui) ? 0 : (float)fmod((cui_frame_time(ui) - resource->started) / .8, 1);
    float eased = phase < .5f ? 2 * phase * phase : 1 - (-2 * phase + 2) * (-2 * phase + 2) / 2;
    options.custom_transform = true;
    options.transform = cui_icon_rotation(eased * 6.2831853071795864769f);
    if (options.size == CUI_ICON_INHERITED)
        options.size = CUI_ICON_MEDIUM;
    cui_icon_error error = cui_icon(ui, id, theme, &resource->state, options);
    if (error == CUI_ICON_OK && !cui_reduced_motion(ui))
        cui_invalidate(ui);
    return error;
}
