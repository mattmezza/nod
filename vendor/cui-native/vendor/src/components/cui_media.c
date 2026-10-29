/* SPDX-License-Identifier: MIT */
#include "../base/cui_design_metrics.h"
#include "cui/cui_icon.h"
#include "cui/cui_media_events.h"
#include "cui/cui_scroll.h"
#include <math.h>
#include <stdio.h>
static cui_id cui_media_id(cui_id parent, uint64_t child) {
    uint64_t x = parent + child * UINT64_C(0x9e3779b97f4a7c15);
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x ^= x >> 31;
    return x ? x : 1;
}

cui_id cui_attachment_remove_id(cui_id id) {
    return cui_media_id(id, 1001);
}
cui_id cui_attachment_retry_id(cui_id id) {
    return cui_media_id(id, 1002);
}
cui_image_result cui_image(cui_context *ui, cui_id id, cui_image_state *state,
                           cui_image_source source, cui_image_loader_fn loader, void *user,
                           cui_image_options o) {
    cui_image_result result = {0};
    cui_image_layout layout;
    if (!ui || !id || (o.description.length && !o.description.data) || !isfinite(o.opacity) ||
        o.opacity < 0 || o.opacity > 1 || !isfinite(o.corners.top_left) ||
        !isfinite(o.corners.top_right) || !isfinite(o.corners.bottom_left) ||
        !isfinite(o.corners.bottom_right) || o.corners.top_left < 0 || o.corners.top_right < 0 ||
        o.corners.bottom_left < 0 || o.corners.bottom_right < 0 ||
        !cui_image_layout_resolve(o.geometry, 0, 0, &layout))
        return result;
    result = cui_image_update(state, source, loader, user, o.clock);
    if (!result.valid)
        return result;
    if (result.frame &&
        !cui_image_layout_resolve(o.geometry, result.frame->width, result.frame->height, &layout)) {
        result.valid = false;
        return result;
    }
    if (o.description.length)
        cui_interact(ui, id, CUI_ROLE_LABEL, o.description, false, false, 0);
    cui_begin_box(ui, id,
                  (cui_box){.width = layout.width,
                            .height = layout.height,
                            .fixed_width = true,
                            .fixed_height = true,
                            .clip = true,
                            .row = true});
    if (result.frame && layout.destination.w > 0 && layout.destination.h > 0) {
        cui_begin_box(ui, cui_media_id(id, 1),
                      (cui_box){.width = layout.destination.x,
                                .fixed_width = true,
                                .height = layout.height,
                                .fixed_height = true});
        cui_end_box(ui);
        cui_begin_box(ui, cui_media_id(id, 2),
                      (cui_box){.width = layout.destination.w,
                                .height = layout.height,
                                .fixed_width = true,
                                .fixed_height = true});
        cui_begin_box(ui, cui_media_id(id, 3),
                      (cui_box){.height = layout.destination.y,
                                .fixed_height = true,
                                .width = layout.destination.w,
                                .fixed_width = true});
        cui_end_box(ui);
        cui_image_frame f = *result.frame;
        cui_paint_bitmap(
            ui, cui_media_id(id, 4),
            (cui_bitmap_paint){.bitmap = {f.pixels, f.width, f.height, f.stride, f.revision},
                               .source = layout.source,
                               .opacity = o.opacity,
                               .grayscale = o.grayscale},
            (cui_box){.width = layout.destination.w,
                      .height = layout.destination.h,
                      .fixed_width = true,
                      .fixed_height = true,
                      .corner_radii = o.corners,
                      .custom_corners = true});
        cui_end_box(ui);
    } else if (result.loading_visible && o.loading)
        o.loading(ui, cui_media_id(id, 5), o.user, layout.width, layout.height);
    else if (result.status == CUI_IMAGE_FAILED && o.fallback)
        o.fallback(ui, cui_media_id(id, 6), o.user, layout.width, layout.height);
    cui_end_box(ui);
    if (result.changed)
        cui_invalidate(ui);
    return result;
}
typedef struct {
    cui_component_event_metadata evidence;
    unsigned matches; /* Saturates at two: unique versus aggregate ambiguity. */
    bool missing;
} cui_media_action_slot;
typedef struct {
    cui_id id;
    uint64_t frame;
    cui_attachment_handler handler;
    cui_attachment_event_result *result;
    cui_media_action_slot slots[3];
} cui_media_action_observer;
static void cui_media_action_capture(cui_context *ui, cui_media_action_observer *observer,
                                     cui_attachment_action action, cui_id control, bool activated) {
    if (!observer || !observer->frame || !activated)
        return;
    cui_media_action_slot *slot = &observer->slots[action];
    size_t count;
    (void)cui_frame_events(ui, &count);
    bool found = false;
    for (size_t e = 0; e < count; ++e) {
        if (cui_event_accepted_activation(ui, e) != control)
            continue;
        found = true;
        if (slot->matches < 2)
            ++slot->matches;
        cui_activation_snapshot snapshot;
        cui_component_event_metadata evidence;
        if (slot->matches == 1 && cui_event_activation_snapshot(ui, e, &snapshot) &&
            cui_component_event_from_activation(&evidence, CUI_COMPONENT_EVENT_ACTIVATE,
                                                &snapshot)) {
            slot->evidence = evidence;
            slot->evidence.target = observer->id;
        } else
            slot->missing = true;
    }
    if (!found)
        slot->missing = true;
}
static void cui_media_action_notify(cui_media_action_observer *observer,
                                    cui_attachment_action action) {
    if (!observer || !observer->frame)
        return;
    cui_attachment_event_fn handler =
        action == CUI_ATTACHMENT_ACTION_OPEN     ? observer->handler.on_activate
        : action == CUI_ATTACHMENT_ACTION_REMOVE ? observer->handler.on_remove
                                                 : observer->handler.on_retry;
    if (!handler)
        return;
    cui_media_action_slot slot = observer->slots[action];
    if (slot.matches != 1 || slot.missing) {
        (void)cui_component_event_init(&slot.evidence, CUI_COMPONENT_EVENT_ACTIVATE, observer->id,
                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
        slot.evidence.flags |= CUI_COMPONENT_EVENT_HAS_FRAME;
        slot.evidence.frame_sequence = observer->frame;
    }
    cui_attachment_action_event event = {slot.evidence, action};
    handler(observer->handler.user, &event);
    if (action == CUI_ATTACHMENT_ACTION_OPEN)
        ++observer->result->activation_count;
    else if (action == CUI_ATTACHMENT_ACTION_REMOVE)
        ++observer->result->remove_count;
    else
        ++observer->result->retry_count;
}
static bool cui_media_control(cui_context *ui, const cui_theme *theme, cui_id id, cui_str label,
                              cui_str glyph, cui_color fg, cui_color bg, cui_color border,
                              float size, float font, cui_media_action_observer *observer,
                              cui_attachment_action action) {
    cui_interaction interaction = cui_interact(ui, id, CUI_ROLE_BUTTON, label, false, false, 0);
    cui_media_action_capture(ui, observer, action, id, interaction.activated);
    cui_begin_box(ui, id,
                  (cui_box){.width = size,
                            .height = size,
                            .radius = size / 2,
                            .background = bg,
                            .border = border,
                            .border_width = border.a ? 1 : 0,
                            .align_x = CUI_ALIGN_CENTER,
                            .align_y = CUI_ALIGN_CENTER});
    (void)cui_icon_named(
        ui, cui_media_id(id, 89), theme, glyph,
        (cui_icon_options){
            .size = CUI_ICON_CUSTOM, .custom_size = font, .custom_color = true, .color = fg});
    cui_end_box(ui);
    return interaction.activated;
}
static void cui_media_busy(cui_context *ui, cui_id id, const cui_theme *t, cui_attachment_options o,
                           cui_attachment_plan p, float size, cui_color color) {
    if (p.progress_ring) {
        cui_interact(ui, id, CUI_ROLE_PROGRESSBAR, cui_string("Upload progress"), false, false,
                     p.progress);
        cui_set_semantic_details(ui, id, (cui_semantic_details){.minimum = 0, .maximum = 100});
        cui_begin_box(ui, id, (cui_box){.width = size, .height = size});
        cui_rect b = cui_bounds(ui, id);
        if (b.w > 0)
            for (unsigned i = 0; i < 36; i++) {
                float angle = (float)i / 36 * 6.2831853f - 1.5707963f;
                cui_color c = color;
                if (i * 100.f / 36 >= p.progress)
                    c.a = (unsigned char)(c.a * .2f);
                float dot = fmaxf(1, size / 10);
                cui_begin_portal(
                    ui, cui_media_id(id, i + 1), (cui_box){.background = c, .radius = dot / 2},
                    (cui_rect){b.x + size / 2 + cosf(angle) * (size / 2 - dot) - dot / 2,
                               b.y + size / 2 + sinf(angle) * (size / 2 - dot) - dot / 2, dot, dot},
                    1);
                cui_end_box(ui);
            }
        cui_end_box(ui);
    } else {
        cui_theme spinner_theme = *t;
        spinner_theme.primary = color;
        cui_spinner(ui, id, &spinner_theme, size, cui_loading_phase(o.now_seconds, 1),
                    o.reduced_motion);
    }
}
static void cui_media_metadata(cui_context *ui, cui_id id, const cui_theme *t,
                               cui_attachment_state *state, cui_attachment_options o,
                               cui_attachment_plan p, float width, cui_attachment_result *result,
                               cui_media_action_observer *observer) {
    cui_begin_box(ui, id, (cui_box){.width = width, .gap = 2, .clip = true});
    bool retry_used = false;
    for (size_t i = 0; i < o.child_count; i++) {
        cui_attachment_child child = o.children[i];
        cui_id child_id = cui_media_id(id, i + 1);
        if (child.kind == CUI_ATTACHMENT_CUSTOM) {
            if (child.render)
                child.render(ui, child_id, child.user, width, 0);
            continue;
        }
        bool title = child.kind == CUI_ATTACHMENT_TITLE;
        cui_attachment_status status = child.status_override ? child.status : o.model.status;
        float font = child.font_size ? child.font_size
                     : title         ? p.metrics.text
                                     : p.metrics.description;
        cui_color color = title ? t->text : status == CUI_ATTACHMENT_FAILED ? t->danger : t->muted;
        if (child.custom_color)
            color = child.color;
        bool retry = !title && p.retry_description && !retry_used;
        float suffix_width = 0;
        if (retry)
            suffix_width = fmaxf(48, cui_measure_text(ui, o.retry_label, font).width);
        else if (p.append_percentage && !title) {
            char percentage[16];
            snprintf(percentage, sizeof percentage, "· %u%%", (unsigned)floorf(p.progress + .5f));
            suffix_width = fmaxf(48, cui_measure_text(ui, cui_string(percentage), font).width);
        }
        cui_begin_box(ui, child_id, (cui_box){.width = width, .row = true, .clip = true, .gap = 4});
        float text_width = fmaxf(0, width - suffix_width - (suffix_width > 0 ? 4 : 0));
        cui_begin_box(ui, cui_media_id(child_id, 1),
                      (cui_box){.width = text_width, .fixed_width = true, .clip = true});
        if (title && cui_attachment_in_progress(status)) {
            cui_marker_child text = {.kind = CUI_MARKER_TEXT, .text = child.text};
            cui_marker_options marker =
                child.custom_shimmer ? child.shimmer : cui_marker_options_default();
            marker.width = text_width;
            marker.loading = true;
            marker.loading_style = CUI_MARKER_SHIMMER;
            marker.now_seconds = o.now_seconds;
            marker.reduced_motion |= o.reduced_motion;
            cui_theme title_theme = *t;
            title_theme.font_size = font;
            title_theme.muted = color;
            cui_marker(ui, cui_media_id(child_id, 2), &title_theme, &state->titles[i], &text, 1,
                       marker);
        } else
            cui_text(ui, child.text, font, color);
        cui_end_box(ui);
        if (retry) {
            cui_id retry_id = cui_media_id(cui_attachment_retry_id(id), i + 1);
            cui_interaction action =
                cui_interact(ui, retry_id, CUI_ROLE_LINK, o.retry_label, false, false, 0);
            cui_media_action_capture(ui, observer, CUI_ATTACHMENT_ACTION_RETRY, retry_id,
                                     action.activated);
            cui_begin_box(ui, retry_id, (cui_box){.height = font * 1.25f});
            cui_text(ui, o.retry_label, font, t->primary);
            cui_end_box(ui);
            if (action.activated)
                result->retried = true;
            retry_used = true;
        } else if (p.append_percentage && !title) {
            char text[16];
            snprintf(text, sizeof text, "· %u%%", (unsigned)floorf(p.progress + .5f));
            cui_text(ui, cui_string(text), font, color);
        }
        cui_end_box(ui);
    }
    cui_end_box(ui);
}
static void cui_media_card_treatment(cui_context *ui, cui_id id, cui_rect b, cui_attachment_plan p,
                                     float radius, cui_color color) {
    if (b.w <= 2 || b.h <= 2)
        return;
    radius = fminf(fmaxf(0, radius - 1), fminf(b.w - 2, b.h - 2) / 2);
    if (p.progress_bar && p.progress > 0) {
        float width = b.w - 2, end = width * p.progress / 100, bottom = b.y + b.h - 1;
        for (unsigned i = 0; i < 6; i++) {
            float y = bottom - 2 + (i + .5f) / 3;
            float cy = bottom - radius, dx = radius;
            if (radius > 0 && y > cy)
                dx = sqrtf(fmaxf(0, radius * radius - (y - cy) * (y - cy)));
            float left = radius - dx, right = fminf(width - radius + dx, end);
            if (right <= left)
                continue;
            cui_begin_portal(
                ui, cui_media_id(id, 2000 + i), (cui_box){.background = color},
                (cui_rect){b.x + 1 + left, bottom - 2 + i / 3.f, right - left, 1.f / 3}, 1);
            cui_end_box(ui);
        }
    }
    if (!p.dashed)
        return;
    for (unsigned side = 0; side < 4; side++) {
        float length = (side < 2 ? b.w : b.h) - 2 * radius;
        unsigned count = (unsigned)fminf(32, fmaxf(1, length / 6));
        for (unsigned i = 0; i < count; i++) {
            float at = radius + (i + .25f) * length / count;
            float dash = fminf(3, length / count * .5f);
            cui_rect d = side < 2 ? (cui_rect){b.x + at, b.y + (side ? b.h - 1 : 0), dash, 1}
                                  : (cui_rect){b.x + (side == 3 ? b.w - 1 : 0), b.y + at, 1, dash};
            cui_begin_portal(ui, cui_media_id(id, 2100 + side * 32 + i),
                             (cui_box){.background = color}, d, 1);
            cui_end_box(ui);
        }
    }
    if (radius > 0)
        for (unsigned corner = 0; corner < 4; corner++)
            for (unsigned i = 0; i < 4; i++) {
                float angle = (corner * 1.5707963f) + (i + .5f) / 4 * 1.5707963f;
                float cx = b.x + (corner == 0 || corner == 3 ? b.w - radius : radius);
                float cy = b.y + (corner < 2 ? b.h - radius : radius);
                cui_begin_portal(ui, cui_media_id(id, 2300 + corner * 4 + i),
                                 (cui_box){.background = color, .radius = .5f},
                                 (cui_rect){cx + cosf(angle) * radius - .5f,
                                            cy + sinf(angle) * radius - .5f, 1, 1},
                                 1);
                cui_end_box(ui);
            }
}
static cui_attachment_result cui_attachment_common(cui_context *ui, cui_id id, const cui_theme *t,
                                                   cui_attachment_state *state,
                                                   cui_attachment_options o,
                                                   cui_media_action_observer *observer) {
    cui_attachment_result r = {0};
    if (!ui || !id || !t || !state || !isfinite(t->radius) || t->radius < 0 ||
        (o.name.length && !o.name.data) || (o.remove_label.length && !o.remove_label.data) ||
        (o.retry_label.length && !o.retry_label.data) || (o.tooltip.length && !o.tooltip.data) ||
        o.child_count > CUI_ATTACHMENT_MAX_CHILDREN || (o.child_count && !o.children) ||
        !isfinite(o.actions_width) || o.actions_width < 0 || !isfinite(o.now_seconds) ||
        o.now_seconds < 0 || !isfinite(o.delta_seconds) || o.delta_seconds < 0)
        return r;
    for (size_t i = 0; i < o.child_count; i++) {
        cui_attachment_child c = o.children[i];
        if (c.kind < CUI_ATTACHMENT_TITLE || c.kind > CUI_ATTACHMENT_CUSTOM ||
            (c.text.length && !c.text.data) || !isfinite(c.font_size) || c.font_size < 0 ||
            (c.status_override &&
             (c.status < CUI_ATTACHMENT_PENDING || c.status > CUI_ATTACHMENT_COMPLETE)))
            return r;
        if (c.custom_shimmer) {
            cui_marker_child text = {.kind = CUI_MARKER_TEXT, .text = c.text};
            cui_marker_plan plan;
            if (!cui_base_marker(c.shimmer, &text, 1, &plan))
                return r;
        }
    }
    o.model.root_font_size = o.model.design_size ? t->font_size : t->font_size / .875f;
    o.model.content = o.child_count > 0;
    o.model.actions = o.actions != NULL;
    o.model.image = o.image_state && o.image_loader;
    o.model.media = o.model.media || o.model.image || o.media_content;
    o.model.remove = o.on_remove != NULL || (observer && observer->handler.on_remove);
    o.model.retry = o.on_retry != NULL || (observer && observer->handler.on_retry);
    o.model.hovered = cui_hover_within(ui, id);
    if (!cui_base_attachment(o.model, &r.plan))
        return r;
    if (!cui_begin_focus_scope(ui, id, false))
        return r;
    r.valid = true;
    cui_attachment_plan p = r.plan;
    if (!o.model.vertical && !o.model.content && o.actions && !o.model.width) {
        p.width += o.actions_width + (o.model.media ? p.metrics.gap : 0);
        r.plan.width = p.width;
    }
    if (!o.model.vertical && o.model.content && !o.model.width && o.model.design_size) {
        /* The design chip width must reserve metadata in addition to media/actions. */
        p.width = fmaxf(p.width, p.metrics.padding_start + p.metrics.padding_end +
                                     (o.model.media ? p.media_width + p.metrics.gap : 0) +
                                     (o.actions ? o.actions_width + p.metrics.gap : 0) +
                                     8 * p.metrics.text);
        r.plan.width = p.width;
    }
    float radius = o.model.size == CUI_MEDIA_XSMALL ? t->radius : t->radius * 1.5f;
    bool click = o.model.identified && (o.on_click || (observer && observer->handler.on_activate));
    cui_interaction root = cui_interact(ui, id, CUI_ROLE_GROUP, o.name, false, false, 0);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.pointer_activatable = click,
                               .busy = cui_attachment_in_progress(o.model.status)});
    r.clicked = click && root.activated;
    cui_media_action_capture(ui, observer, CUI_ATTACHMENT_ACTION_OPEN, id, r.clicked);
    cui_begin_box(ui, cui_media_id(id, 1), (cui_box){.width = p.width + p.overhang});
    cui_begin_box(ui, cui_media_id(id, 9), (cui_box){.height = p.overhang, .fixed_height = true});
    cui_end_box(ui);
    cui_begin_box(ui, cui_media_id(id, 10), (cui_box){.width = p.width + p.overhang, .row = true});
    cui_begin_box(
        ui, id,
        (cui_box){.width = p.width,
                  .height = p.flush ? p.height : 0,
                  .row = !o.model.vertical,
                  .gap = o.model.vertical ? p.metrics.gap : 0,
                  .align_x = o.model.vertical ? CUI_ALIGN_CENTER : CUI_ALIGN_START,
                  .padding = o.model.vertical && !p.flush ? p.metrics.card_padding : 0,
                  .radius = radius,
                  .background = root.hovered && click ? t->surface : t->background,
                  .border = o.model.status == CUI_ATTACHMENT_FAILED ? t->danger : t->border,
                  .border_width = p.dashed ? 0 : 1,
                  .align_y = CUI_ALIGN_CENTER});
    cui_track_bounds(ui, id, &state->card);
    if (!o.model.vertical) {
        /* Preserve the nominal chip minimum, but fit real metadata/action height. */
        cui_begin_box(
            ui, cui_media_id(id, 15),
            (cui_box){.width = 0, .fixed_width = true, .height = p.height, .fixed_height = true});
        cui_end_box(ui);
        cui_begin_box(ui, cui_media_id(id, 2),
                      (cui_box){.width = p.metrics.padding_start, .fixed_width = true});
        cui_end_box(ui);
    }
    cui_id media_id = cui_media_id(id, 3);
    if (o.model.media) {
        float media_radius = p.flush ? fmaxf(0, radius - 1) : t->radius;
        cui_color background =
            o.model.status == CUI_ATTACHMENT_FAILED && !o.model.image ? t->danger : t->surface;
        if (o.model.status == CUI_ATTACHMENT_FAILED && !o.model.image)
            background.a = 26;
        cui_begin_box(ui, media_id,
                      (cui_box){.width = p.media_width,
                                .height = p.media_height,
                                .radius = media_radius,
                                .background = background,
                                .clip = true,
                                .align_x = CUI_ALIGN_CENTER,
                                .align_y = CUI_ALIGN_CENTER});
        cui_track_bounds(ui, media_id, &state->media);
        if (o.model.image) {
            cui_image_options image = cui_image_options_default();
            image.geometry =
                (cui_image_geometry){p.media_width, p.media_height, 0, CUI_IMAGE_COVER};
            image.corners =
                (cui_corner_radii){media_radius, media_radius, media_radius, media_radius};
            image.clock =
                (cui_image_clock){o.model.identified, o.active, o.reduced_motion, o.now_seconds};
            cui_image(ui, cui_media_id(media_id, 1), o.image_state, o.image_source, o.image_loader,
                      o.image_user, image);
        } else if (!p.spinner && !p.progress_ring && !p.rejection && !p.retry_media &&
                   o.media_content)
            o.media_content(ui, cui_media_id(media_id, 2), o.media_user, p.media_width,
                            p.media_height);
        cui_end_box(ui);
        cui_rect b = state->media;
        if (b.w > 0 && (p.scrim || p.spinner || p.progress_ring || p.rejection || p.retry_media)) {
            cui_begin_portal(ui, cui_media_id(media_id, 3),
                             (cui_box){.radius = media_radius,
                                       .background = {0, 0, 0, (unsigned char)(p.scrim * 255)},
                                       .align_x = CUI_ALIGN_CENTER,
                                       .align_y = CUI_ALIGN_CENTER},
                             b, 1);
            cui_color color = o.model.image ? (cui_color){255, 255, 255, 255} : t->primary;
            if (p.spinner || p.progress_ring)
                cui_media_busy(ui, cui_media_id(media_id, 4), t, o, p,
                               o.model.size == CUI_MEDIA_XSMALL ? 14 : 20, color);
            else if (p.retry_media)
                r.retried |= cui_media_control(ui, t, cui_attachment_retry_id(id), o.retry_label,
                                               cui_string("refresh-cw"), t->danger, t->background,
                                               (cui_color){0}, 24, 12, observer,
                                               CUI_ATTACHMENT_ACTION_RETRY);
            else if (p.rejection)
                (void)cui_icon_named(ui, cui_media_id(media_id, 88), t, cui_string("ban"),
                                     (cui_icon_options){.size = CUI_ICON_CUSTOM,
                                                        .custom_size = p.metrics.glyph,
                                                        .custom_color = true,
                                                        .color = color});
            cui_end_box(ui);
        }
        if (b.w > 0 && o.media_overlay) {
            cui_begin_portal(ui, cui_media_id(media_id, 5),
                             (cui_box){.align_x = CUI_ALIGN_CENTER, .align_y = CUI_ALIGN_CENTER}, b,
                             2);
            o.media_overlay(ui, cui_media_id(media_id, 6), o.overlay_user, b.w, b.h);
            cui_end_box(ui);
        }
    }
    if (!o.model.vertical && o.model.media && (o.model.content || o.actions)) {
        cui_begin_box(ui, cui_media_id(id, 13),
                      (cui_box){.width = p.metrics.gap, .fixed_width = true});
        cui_end_box(ui);
    }
    float content_width = o.model.vertical
                              ? p.width - 2 * p.metrics.card_padding
                              : fmaxf(0, p.width - p.metrics.padding_start - p.metrics.padding_end -
                                             (o.model.media ? p.metrics.media + p.metrics.gap : 0) -
                                             (o.actions ? o.actions_width + p.metrics.gap : 0));
    if (o.model.content)
        cui_media_metadata(ui, cui_media_id(id, 4), t, state, o, p, content_width, &r, observer);
    if (!o.model.vertical && o.model.content && o.actions) {
        cui_begin_box(ui, cui_media_id(id, 14),
                      (cui_box){.width = p.metrics.gap, .fixed_width = true});
        cui_end_box(ui);
    }
    if (o.actions) {
        cui_id action_id = cui_media_id(id, 5);
        /* The cluster itself captures presses in the gaps, below its real controls. */
        cui_interact(ui, action_id, CUI_ROLE_GROUP, cui_string("Attachment actions"), false, false,
                     0);
        cui_set_semantic_details(ui, action_id,
                                 (cui_semantic_details){.parent = id, .pointer_activatable = true});
        if (o.model.vertical && state->card.w > 0) {
            cui_rect b = state->card;
            cui_begin_portal(
                ui, action_id, (cui_box){.row = true, .gap = 4},
                (cui_rect){b.x + b.w - o.actions_width - 12, b.y + 12, o.actions_width, 0}, 3);
            o.actions(ui, cui_media_id(id, 6), o.actions_user, o.actions_width, 24);
            cui_end_box(ui);
        } else {
            cui_begin_box(ui, action_id,
                          (cui_box){.row = true, .gap = 4, .width = o.actions_width});
            o.actions(ui, cui_media_id(id, 6), o.actions_user, o.actions_width, p.height);
            cui_end_box(ui);
        }
    }
    if (!o.model.vertical) {
        cui_begin_box(ui, cui_media_id(id, 7),
                      (cui_box){.width = p.metrics.padding_end, .fixed_width = true});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    cui_begin_box(ui, cui_media_id(id, 11), (cui_box){.width = p.overhang, .fixed_width = true});
    cui_end_box(ui);
    cui_end_box(ui);
    cui_end_box(ui);
    cui_media_card_treatment(ui, id, state->card, p, radius, p.dashed ? t->border : t->primary);
    if (p.remove_visible && state->card.w > 0) {
        cui_rect b = state->card;
        cui_begin_portal(ui, cui_media_id(id, 8), (cui_box){0},
                         (cui_rect){b.x + b.w - 14, b.y - 6, 20, 20}, 4);
        r.removed = cui_media_control(ui, t, cui_attachment_remove_id(id), o.remove_label,
                                      cui_string("close"), t->text, t->background, t->border, 20,
                                      10, observer, CUI_ATTACHMENT_ACTION_REMOVE);
        cui_end_box(ui);
    }
    if (o.model.identified && o.tooltip.length) {
        cui_hover_options tooltip = o.tooltip_options;
        tooltip.trigger = id;
        tooltip.dt = o.delta_seconds;
        cui_tooltip(ui, cui_media_id(id, 12), t, &state->tooltip, o.tooltip, tooltip);
    }
    if (r.clicked) {
        if (o.on_click)
            o.on_click(o.action_user, id);
        cui_media_action_notify(observer, CUI_ATTACHMENT_ACTION_OPEN);
    }
    if (r.removed) {
        if (o.on_remove)
            o.on_remove(o.action_user, id);
        cui_media_action_notify(observer, CUI_ATTACHMENT_ACTION_REMOVE);
    }
    if (r.retried) {
        if (o.on_retry)
            o.on_retry(o.action_user, id);
        cui_media_action_notify(observer, CUI_ATTACHMENT_ACTION_RETRY);
    }
    cui_end_focus_scope(ui);
    return r;
}

cui_attachment_result cui_attachment(cui_context *ui, cui_id id, const cui_theme *theme,
                                     cui_attachment_state *state, cui_attachment_options options) {
    return cui_attachment_common(ui, id, theme, state, options, NULL);
}
cui_attachment_event_result cui_attachment_events(cui_context *ui, cui_id id,
                                                  const cui_theme *theme,
                                                  cui_attachment_state *state,
                                                  cui_attachment_options options,
                                                  cui_attachment_handler handler) {
    cui_attachment_event_result result = {0};
    if (!handler.on_activate && !handler.on_remove && !handler.on_retry) {
        result.control = cui_attachment(ui, id, theme, state, options);
        return result;
    }
    cui_media_action_observer observer = {.id = id, .handler = handler, .result = &result};
    observer.frame = cui_frame_paint_enabled(ui) ? cui_frame_sequence(ui) : 0;
    result.control = cui_attachment_common(ui, id, theme, state, options, &observer);
    return result;
}

static float cui_media_transition(float value, float *start, float *target, double *epoch,
                                  float next, double now, bool reduced) {
    float phase = (float)fmin(1, fmax(0, now - *epoch) / .2);
    phase = 1 - powf(1 - phase, 3);
    value = *start + (*target - *start) * phase;
    if (next != *target) {
        *start = value;
        *target = next;
        *epoch = now;
    }
    return reduced ? next : value;
}
cui_attachment_group_result cui_attachment_group(cui_context *ui, cui_id id,
                                                 cui_attachment_group_state *s,
                                                 cui_attachment_group_options o) {
    cui_attachment_group_result r = {0};
    if (!ui || !id || !s || !o.children || !isfinite(o.width) || o.width <= 0 ||
        !isfinite(o.height) || o.height < 0 || !isfinite(o.gap) || o.gap < 0 ||
        !isfinite(o.wheel_step) || o.wheel_step <= 0 || !isfinite(o.now_seconds) ||
        o.now_seconds < 0)
        return r;
    r.valid = true;
    cui_scroll_state *scroll = o.external_scroll ? o.external_scroll : &s->scroll;
    float height = o.height ? o.height : s->content.h + 8;
    cui_scroll_options options = {.width = o.width,
                                  .height = height,
                                  .content_width = s->content.w,
                                  .content_height = height,
                                  .horizontal = true,
                                  .hide_scrollbars = true,
                                  .wheel_step = o.wheel_step};
    cui_scroll_geometry geometry = cui_scroll_layout(options);
    r.changed = cui_base_scrollable(ui, id, scroll, options);
    r.maximum = geometry.maximum_x;
    cui_set_semantic_details(ui, id,
                             (cui_semantic_details){.scrollable = true,
                                                    .orientation = CUI_ORIENTATION_HORIZONTAL,
                                                    .scroll_axis_lock = true,
                                                    .scroll_x = scroll->x,
                                                    .scroll_max_x = r.maximum});
    cui_theme scrollbar_theme = o.scrollbar_theme ? *o.scrollbar_theme : cui_theme_light();
    cui_scroll_options bars = cui_scroll_resolve(
        &scrollbar_theme,
        (cui_scroll_options){.horizontal = true, .scrollbar_style = o.scrollbar_style});
    cui_scrollbar_style skin = *bars.scrollbar_style;
    skin.reduced_motion |= o.reduced_motion;
    bars.scrollbar_style = &skin;
    r.changed |= cui_base_scrollbar_overlay(ui, id, &scroll->x, &scroll->y,
                                            (cui_rect){0, 0, o.width, height},
                                            (cui_size){s->content.w, height}, bars);
    cui_begin_box(
        ui, id,
        (cui_box){.width = o.width, .height = o.height, .clip = true, .offset_x = -scroll->x});
    cui_track_bounds(ui, id, &s->viewport);
    cui_begin_box(ui, cui_media_id(id, 2), (cui_box){.height = 4, .fixed_height = true});
    cui_end_box(ui);
    cui_begin_box(ui, cui_media_id(id, 1), (cui_box){.row = true, .gap = o.gap});
    cui_track_bounds(ui, cui_media_id(id, 1), &s->content);
    o.children(ui, cui_media_id(id, 3), o.user, 0, height);
    cui_end_box(ui);
    cui_begin_box(ui, cui_media_id(id, 4), (cui_box){.height = 4, .fixed_height = true});
    cui_end_box(ui);
    cui_scrollbar_overlay_paint(ui, id, &scrollbar_theme, scroll->x, 0,
                                (cui_rect){0, 0, o.width, height}, (cui_size){s->content.w, height},
                                bars);
    cui_end_box(ui);
    float leading = o.edge_fade && r.maximum > 1 && scroll->x > 1 ? 1 : 0;
    float trailing = o.edge_fade && r.maximum > 1 && scroll->x < r.maximum - 1 ? 1 : 0;
    if (!s->initialized) {
        s->leading = s->leading_start = s->leading_target = leading;
        s->trailing = s->trailing_start = s->trailing_target = trailing;
        s->leading_epoch = s->trailing_epoch = o.now_seconds;
        s->initialized = true;
    }
    float old_leading = s->leading, old_trailing = s->trailing;
    s->leading = cui_media_transition(s->leading, &s->leading_start, &s->leading_target,
                                      &s->leading_epoch, leading, o.now_seconds, o.reduced_motion);
    s->trailing =
        cui_media_transition(s->trailing, &s->trailing_start, &s->trailing_target,
                             &s->trailing_epoch, trailing, o.now_seconds, o.reduced_motion);
    r.changed |= old_leading != s->leading || old_trailing != s->trailing;
    r.leading_opacity = s->leading;
    r.trailing_opacity = s->trailing;
    if (s->leading != leading || s->trailing != trailing)
        r.next_wake_seconds = o.now_seconds + 1. / 60;
    cui_rect b = s->viewport;
    if (b.w > 0)
        for (unsigned side = 0; side < 2; side++) {
            float opacity = side ? s->trailing : s->leading;
            if (opacity <= 0)
                continue;
            float width = fminf(24, b.w / 2), step = width / 24;
            for (unsigned i = 0; i < 24; i++) {
                cui_color color = o.fade_color;
                color.a = (unsigned char)(color.a * opacity *
                                          (side ? (i + .5f) / 24 : 1 - (i + .5f) / 24));
                cui_begin_portal(
                    ui, cui_media_id(id, 100 + side * 24 + i), (cui_box){.background = color},
                    (cui_rect){b.x + (side ? b.w - width : 0) + i * step, b.y, step, b.h}, 2);
                cui_end_box(ui);
            }
        }
    if (r.changed)
        cui_invalidate(ui);
    return r;
}
