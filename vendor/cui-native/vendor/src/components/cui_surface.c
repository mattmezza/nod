/* SPDX-License-Identifier: MIT */
#include "cui/cui_surface.h"
#include "cui/cui_components.h"
#include "cui/cui_scroll.h"
#include "cui_popover_paint.h"
#include <math.h>
#include <string.h>
bool cui_popover_style_resolve(const cui_theme *t, cui_popover_appearance appearance,
                               cui_popover_refinement r, cui_popover_style *out) {
    if (!t || !out || !cui_popover_refinement_valid(appearance, r))
        return false;
    cui_popover_style next = {.foreground = t->text};
    if (appearance == CUI_POPOVER_APPEARANCE_LEGACY)
        next = (cui_popover_style){.background = t->surface,
                                   .foreground = t->text,
                                   .border = t->border,
                                   .border_width = 1,
                                   .radius = t->radius,
                                   .padding = t->padding};
    else if (appearance == CUI_POPOVER_APPEARANCE_SOURCE) {
        next = (cui_popover_style){.background = t->popover,
                                   .foreground = t->popover_foreground,
                                   .radius = t->radius,
                                   .padding = 12,
                                   .shadow_count = 3};
        cui_drop_shadow layers[3];
        if (!cui_theme_popover_shadows(t, 1, layers))
            return false;
        for (size_t i = 0; i < 3; ++i)
            next.shadows[i] = (cui_box_shadow){.layer = layers[i]};
    }
    if (r.flags & CUI_POPOVER_REFINE_BACKGROUND)
        next.background = r.background;
    if (r.flags & CUI_POPOVER_REFINE_FOREGROUND)
        next.foreground = r.foreground;
    if (r.flags & CUI_POPOVER_REFINE_RADIUS)
        next.radius = r.radius;
    if (r.flags & CUI_POPOVER_REFINE_PADDING)
        next.padding = r.padding;
    if (r.flags & CUI_POPOVER_REFINE_BORDER) {
        next.border = r.border;
        next.border_width = r.border_width;
    }
    if (r.flags & CUI_POPOVER_REFINE_SHADOWS) {
        next.shadow_count = r.shadow_count;
        for (size_t i = 0; i < r.shadow_count; ++i)
            next.shadows[i] = r.shadows[i];
    }
    if (!isfinite(next.padding) || next.padding < 0 || next.padding > 4096 ||
        !isfinite(next.radius) || next.radius < 0 || next.radius > 4096)
        return false;
    *out = next;
    return true;
}
#define CUI_SURFACE_SKIN_OWNER UINT64_C(0x1a45de91b7956712)
#define CUI_SURFACE_SKIN_NAMESPACE UINT64_C(0xac3286f910fdcc31)
typedef struct {
    cui_surface_state *state;
    cui_theme theme;
    bool active, footer;
} cui_surface_skin_entry;
typedef struct {
    size_t depth;
    cui_surface_skin_entry entries[64];
} cui_surface_skin_stack;
static void cui_surface_skin_collect(void *payload) {
    ((cui_surface_skin_stack *)payload)->depth = 0;
}
static cui_surface_skin_stack *cui_surface_skin_find(cui_context *ui) {
    return cui_resource_find(ui, CUI_SURFACE_SKIN_OWNER, CUI_SURFACE_SKIN_NAMESPACE);
}
static void cui_surface_skin_paint(cui_context *ui, cui_surface_skin_entry *entry) {
    if (!entry || !entry->active)
        return;
    cui_surface_state *s = entry->state;
    cui_rect viewport = {0, 0, s->body_viewport.w, s->body_viewport.h};
    cui_scroll_options options = {.vertical = true, .scrollbar_style = &entry->theme.scrollbar};
    cui_scrollbar_overlay_paint(ui, cui_surface_part_id(s->overlay.id, 6), &entry->theme, 0,
                                s->body_scroll, viewport,
                                (cui_size){viewport.w, viewport.h + s->body_maximum}, options);
}
static void cui_surface_body_begin(cui_context *ui, const cui_theme *t, cui_surface_state *s,
                                   bool centered, float width) {
    cui_begin_box(
        ui, cui_surface_part_id(s->overlay.id, 6),
        (cui_box){.width = -1,
                  .height = -1,
                  .clip = true,
                  .align_y = centered && s->body_maximum <= 0 ? CUI_ALIGN_CENTER : CUI_ALIGN_START,
                  .offset_y = -s->body_scroll});
    cui_base_surface_submit_body(ui, s);
    cui_surface_skin_stack *stack = cui_surface_skin_find(ui);
    if (stack && stack->depth && stack->entries[stack->depth - 1].active) {
        cui_surface_skin_entry *entry = &stack->entries[stack->depth - 1];
        float x = 0;
        cui_rect viewport = {0, 0, s->body_viewport.w, s->body_viewport.h};
        if (cui_base_scrollbar_overlay(
                ui, cui_surface_part_id(s->overlay.id, 6), &x, &s->body_scroll, viewport,
                (cui_size){viewport.w, viewport.h + s->body_maximum},
                (cui_scroll_options){.vertical = true, .scrollbar_style = &entry->theme.scrollbar}))
            cui_set_box_scroll_offset(ui, cui_surface_part_id(s->overlay.id, 6), 0,
                                      -s->body_scroll);
    }
    if (centered && s->body_content.h <= 0)
        cui_invalidate(ui);
    /* Clipped viewports allow horizontal overflow in Clay. Bound the content
     * explicitly so wrapping text cannot expand the action row past its hit clip. */
    cui_begin_box(ui, cui_surface_part_id(s->overlay.id, 7),
                  (cui_box){.width = width, .fixed_width = true, .gap = t->gap});
}
static bool cui_surface_begin_internal(cui_context *ui, const cui_theme *t, cui_surface_state *s,
                                       cui_str title, cui_str description, cui_surface_options o,
                                       const cui_rect *positioned, const cui_popover_paint *arrow) {
    cui_rect bounds = positioned ? *positioned
                      : s        ? cui_surface_geometry(s->kind, o)
                                 : (cui_rect){0};
    if (!t || bounds.w <= 0 || bounds.h <= 0)
        return false;
    cui_popover_style style = {.background = t->surface,
                               .foreground = t->text,
                               .border = t->border,
                               .border_width = 1,
                               .radius = t->radius,
                               .padding = t->padding};
    if (s->kind == CUI_SURFACE_POPOVER &&
        !cui_popover_style_resolve(t, o.popover_appearance, o.popover_refinement, &style))
        return false;
    const cui_scrollbar_style *skin = o.scrollbar_style ? o.scrollbar_style : &t->scrollbar;
    bool active =
        skin->visibility == CUI_SCROLLBAR_VISIBLE || skin->visibility == CUI_SCROLLBAR_AUTO_HIDE;
    cui_surface_skin_stack *stack = cui_surface_skin_find(ui);
    if (active && !stack)
        stack = cui_resource_get(ui, CUI_SURFACE_SKIN_OWNER, CUI_SURFACE_SKIN_NAMESPACE,
                                 sizeof(*stack), cui_surface_skin_collect, NULL);
    if ((active && !stack) || (stack && stack->depth == 64) ||
        !cui_base_surface_begin(ui, s, title, description, o))
        return false;
    if (stack) {
        cui_surface_skin_entry *entry = &stack->entries[stack->depth++];
        *entry = (cui_surface_skin_entry){.state = s, .theme = *t, .active = active};
        entry->theme.scrollbar = *skin;
        entry->theme.scrollbar.reduced_motion |= o.reduced_motion;
    }
    bounds = positioned ? *positioned : cui_surface_current_geometry(s, o);
    int16_t layer = o.layer > 0 && o.layer < 32766 ? o.layer : 100;
    cui_rect handle = cui_surface_resize_bounds(s, o);
    if (handle.w > 0 && handle.h > 0) {
        cui_id resize = cui_surface_part_id(s->overlay.id, 5);
        cui_begin_portal(ui, resize,
                         (cui_box){.background = cui_focused(ui) == resize ? t->focus : t->border},
                         handle, (int16_t)(layer + 2));
        cui_base_surface_submit_resize(ui, s, o);
        cui_end_box(ui);
    }
    if (s->overlay.modal) {
        cui_begin_portal(ui, s->overlay.id ^ UINT64_C(0x86a3e3d76bc1d909),
                         (cui_box){.background = {0, 0, 0, 100}}, o.viewport, layer);
        cui_end_box(ui);
    }
    cui_begin_portal(ui, s->overlay.id,
                     (cui_box){.padding = style.padding,
                               .gap = t->gap,
                               .radius = style.radius,
                               .background = style.background,
                               .border = style.border,
                               .border_width = style.border_width},
                     bounds, (int16_t)(layer + 1));
    if (arrow) {
        /* A detached paint-only portal keeps the arrow outside the panel clip.
         * Local prepaint remains visible when inert or behind another modal;
         * deferred streams intentionally require modal input eligibility. */
        cui_begin_portal(ui, cui_surface_part_id(s->overlay.id, 8), (cui_box){.inert = true},
                         o.viewport, (int16_t)(layer + 1));
        bool painted =
            cui_set_prepaint(ui, (cui_prepaint_options){.callback = cui_popover_paint_callback,
                                                        .payload = arrow,
                                                        .payload_bytes = sizeof(*arrow),
                                                        .command_budget = 5});
        cui_end_box(ui);
        if (!painted) {
            cui_end_box(ui);
            cui_base_surface_end(ui);
            if (stack)
                stack->depth--;
            return false;
        }
    }
    if (style.shadow_count)
        cui_set_box_shadows(
            ui, style.shadows, style.shadow_count,
            (cui_corner_radii){style.radius, style.radius, style.radius, style.radius});
    bool centered = s->kind == CUI_SURFACE_ALERT;
    float body_width = fmaxf(0, bounds.w - 2 * fmaxf(0, style.padding));
    if (centered)
        cui_surface_body_begin(ui, t, s, true, body_width);
    cui_begin_box(ui, 0, (cui_box){.width = -1, .row = true, .gap = t->gap});
    cui_begin_box(ui, cui_surface_part_id(s->overlay.id, 4), (cui_box){.width = -1});
    cui_text(ui, title, t->font_size * 1.1f, style.foreground);
    cui_end_box(ui);
    if (!o.hide_close && (s->kind != CUI_SURFACE_ALERT || o.show_alert_close))
        cui_button_ex(ui, cui_surface_part_id(s->overlay.id, 1), t, cui_string("Close"),
                      (cui_button_options){.variant = CUI_BUTTON_GHOST,
                                           .size = CUI_CONTROL_SMALL,
                                           .icon = {"close", 5},
                                           .icon_only = true});
    cui_end_box(ui);
    if (description.length)
        cui_text(ui, description, t->font_size, t->muted);
    if (!centered)
        cui_surface_body_begin(ui, t, s, false, body_width);
    cui_overlay_set_bounds(&s->overlay, bounds);
    return true;
}
bool cui_surface_begin(cui_context *ui, const cui_theme *t, cui_surface_state *s, cui_str title,
                       cui_str description, cui_surface_options o) {
    return cui_surface_begin_internal(ui, t, s, title, description, o, NULL, NULL);
}
bool cui_popover_begin(cui_context *ui, const cui_theme *t, cui_surface_state *s, cui_str title,
                       cui_str description, cui_surface_options o, cui_popover_position p) {
    if (!s || s->kind != CUI_SURFACE_POPOVER)
        return false;
    if (p.mode == CUI_POPOVER_POSITION_LEGACY)
        return cui_surface_begin(ui, t, s, title, description, o);
    cui_rect bounds;
    cui_popover_style style;
    if (!t || !cui_popover_surface_geometry(o, p, &bounds) ||
        !cui_popover_style_resolve(t, o.popover_appearance, o.popover_refinement, &style))
        return false;
    cui_popover_paint paint;
    if (p.arrow && !cui_popover_paint_build(bounds, o.anchor, o.viewport, p, t, style, o, &paint))
        return false;
    return cui_surface_begin_internal(ui, t, s, title, description, o, &bounds,
                                      p.arrow ? &paint : NULL);
}
void cui_surface_footer_begin(cui_context *ui, const cui_theme *t) {
    if (!ui || !t)
        return;
    cui_end_box(ui); /* body content */
    cui_surface_skin_stack *stack = cui_surface_skin_find(ui);
    if (stack && stack->depth) {
        cui_surface_skin_entry *entry = &stack->entries[stack->depth - 1];
        cui_surface_skin_paint(ui, entry);
        entry->footer = true;
    }
    cui_end_box(ui); /* body viewport */
    cui_begin_box(ui, 0, (cui_box){.width = -1});
    cui_begin_box(ui, 0,
                  (cui_box){.width = -1, .row = true, .gap = t->gap, .align_x = CUI_ALIGN_END});
}
void cui_surface_end(cui_context *ui) {
    cui_end_box(ui);
    cui_surface_skin_stack *stack = cui_surface_skin_find(ui);
    if (stack && stack->depth) {
        cui_surface_skin_entry *entry = &stack->entries[stack->depth - 1];
        if (!entry->footer)
            cui_surface_skin_paint(ui, entry);
        stack->depth--;
    }
    cui_end_box(ui);
    cui_end_box(ui);
    cui_base_surface_end(ui);
}
void cui_alert_dialog(cui_context *ui, const cui_theme *t, cui_surface_state *s, cui_str title,
                      cui_str description, cui_str confirm, cui_str cancel, cui_surface_options o) {
    if (!s || s->kind != CUI_SURFACE_ALERT)
        return;
    /* An unspecified height follows measured wrapped title, description and
     * actions; explicit heights retain the viewport/scroll contract. */
    if (t && isfinite(o.height) && o.height <= 0 && isfinite(s->body_content.h) &&
        s->body_content.h > 0)
        o.height = s->body_content.h + 2 * fmaxf(0, t->padding);
    if (!cui_surface_begin(ui, t, s, title, description, o))
        return;
    cui_begin_box(ui, 0,
                  (cui_box){.width = -1, .row = true, .gap = t->gap, .align_x = CUI_ALIGN_END});
    cui_button_ex(ui, cui_surface_part_id(s->overlay.id, 3), t, cancel,
                  (cui_button_options){.variant = CUI_BUTTON_OUTLINE});
    cui_button_ex(ui, cui_surface_part_id(s->overlay.id, 2), t, confirm,
                  (cui_button_options){.variant = CUI_BUTTON_DANGER});
    cui_end_box(ui);
    cui_surface_end(ui);
}
