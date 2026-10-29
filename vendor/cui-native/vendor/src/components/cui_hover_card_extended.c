/* SPDX-License-Identifier: MIT */
#include "cui/cui_hover_card_extended.h"
#include <math.h>
#include <string.h>
/* Private shared placement seam, defined in cui_content.c. */
bool cui_content_begin_hover_card_model(cui_context *, cui_id, const cui_theme *, cui_hover_state *,
                                        cui_str, cui_hover_options, bool);
static bool cui_hover_card_ext_rect_valid(cui_rect r) {
    return isfinite(r.x) && isfinite(r.y) && isfinite(r.w) && isfinite(r.h) && r.w > 0 && r.h > 0;
}
static bool cui_hover_card_ext_contains(cui_rect r, float x, float y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}
static bool cui_hover_card_ext_same_contact(const cui_hover_card_contact *c, cui_event e) {
    return c->type == e.pointer_type && c->contact == e.pointer_id && c->device == e.pointer_device;
}
static bool cui_hover_card_ext_outside_release(cui_context *ui, cui_id id, cui_id trigger,
                                               cui_hover_card_contact *c, const cui_event *events,
                                               size_t n) {
    const cui_rect panel = cui_bounds(ui, id), anchor = cui_bounds(ui, trigger);
    bool dismissed = false;
    for (size_t i = 0; i < n; ++i) {
        const cui_event e = events[i];
        if (e.type == CUI_EVENT_CANCEL) {
            if (e.pointer_type != CUI_POINTER_TOUCH || !c->active ||
                cui_hover_card_ext_same_contact(c, e)) {
                c->active = c->pending = c->ambiguous = false;
            }
            continue;
        }
        if (e.type == CUI_EVENT_SCROLL || e.type == CUI_EVENT_GESTURE) {
            c->pending = false;
            continue;
        }
        if (e.type != CUI_EVENT_POINTER_DOWN && e.type != CUI_EVENT_POINTER_MOVE &&
            e.type != CUI_EVENT_POINTER_UP)
            continue;
        if (e.type == CUI_EVENT_POINTER_DOWN) {
            if (c->active) {
                c->ambiguous = true;
                c->pending = false;
                continue;
            }
            if (c->ambiguous || e.pointer_button != CUI_POINTER_PRIMARY)
                continue;
            c->active = true;
            c->type = e.pointer_type;
            c->contact = e.pointer_id;
            c->device = e.pointer_device;
            c->start_x = e.x;
            c->start_y = e.y;
            c->pending =
                !cui_event_consumed(ui, i) && isfinite(e.x) && isfinite(e.y) &&
                cui_hover_card_ext_rect_valid(panel) && cui_hover_card_ext_rect_valid(anchor) &&
                !cui_hover_card_ext_contains(panel, e.x, e.y) &&
                !cui_hover_card_ext_contains(anchor, e.x, e.y) && cui_pointer_scope_allowed(ui, id);
            continue;
        }
        if (!c->active || !cui_hover_card_ext_same_contact(c, e))
            continue;
        double dx = (double)e.x - c->start_x, dy = (double)e.y - c->start_y;
        if (!isfinite(e.x) || !isfinite(e.y) || dx * dx + dy * dy >= 64 ||
            cui_event_consumed(ui, i))
            c->pending = false;
        if (e.type == CUI_EVENT_POINTER_UP) {
            if (c->pending && !c->ambiguous && e.pointer_button == CUI_POINTER_PRIMARY &&
                cui_hover_card_ext_rect_valid(panel) && cui_hover_card_ext_rect_valid(anchor) &&
                !cui_hover_card_ext_contains(panel, e.x, e.y) &&
                !cui_hover_card_ext_contains(anchor, e.x, e.y) && cui_pointer_scope_allowed(ui, id))
                dismissed = true;
            c->active = c->pending = false;
        }
    }
    return dismissed;
}
static bool cui_hover_card_ext_relationship(cui_context *ui, cui_id trigger, cui_id card,
                                            bool admitted) {
    size_t n = 0;
    const cui_semantic *semantics = cui_semantics(ui, &n);
    for (size_t i = 0; i < n; ++i) {
        if (semantics[i].id != trigger)
            continue;
        cui_semantic_details details = semantics[i].details;
        details.controls = admitted ? card : 0;
        details.expanded = admitted;
        cui_set_semantic_details(ui, trigger, details);
        return true;
    }
    return false;
}
cui_hover_card_extended_result
cui_begin_hover_card_extended(cui_context *ui, cui_id id, const cui_theme *theme,
                              cui_hover_state *state, cui_str name, cui_hover_options options,
                              const cui_hover_card_policy *policy, cui_hover_card_contact *contact,
                              cui_hover_handler handler) {
    cui_hover_card_extended_result result;
    memset(&result, 0, sizeof(result));
    cui_hover_card_policy p = {0};
    if (policy)
        p = *policy;
    if (!ui || !id || !theme || !state || (name.length && !name.data) ||
        p.mode < CUI_HOVER_CARD_DESKTOP_HOVER || p.mode > CUI_HOVER_CARD_TAP_TOGGLE ||
        (p.outside_tap && (p.mode != CUI_HOVER_CARD_TAP_TOGGLE || !contact)))
        return result;
    const cui_hover_handler observer = handler;
    const uint64_t frame = cui_frame_sequence(ui);
    const bool previous = state->open;
    result.lifecycle.valid = true;
    if (p.mode == CUI_HOVER_CARD_DESKTOP_HOVER) {
        if (contact)
            memset(contact, 0, sizeof(*contact));
        result.lifecycle.control_result = cui_begin_hover_card(ui, id, theme, state, name, options);
    } else {
        if (contact && (!frame || contact->context != ui || contact->card != id ||
                        contact->frame == UINT64_MAX || contact->frame + 1 != frame || !previous))
            memset(contact, 0, sizeof(*contact));
        size_t n = 0;
        const cui_event *events = cui_frame_events(ui, &n);
        bool escape = false, outside = false;
        for (size_t i = 0; i < n; ++i)
            if (events[i].type == CUI_EVENT_KEY_DOWN && events[i].key == CUI_KEY_ESCAPE)
                escape = true;
        if (frame && previous && p.outside_tap && !options.disabled)
            outside =
                cui_hover_card_ext_outside_release(ui, id, options.trigger, contact, events, n);
        /* Protected work does not accept caller asserted input or outside taps. */
        bool open = previous;
        if (frame && outside)
            open = false;
        if (frame && p.accepted_trigger_activation)
            open = !previous;
        if (options.disabled || escape)
            open = false;
        *state = (cui_hover_state){
            .open = open, .target = open, .suppressed = escape || (state->suppressed && !open)};
        if (contact) {
            if (!open || options.disabled || escape || !p.outside_tap)
                memset(contact, 0, sizeof(*contact));
            contact->context = ui;
            contact->card = id;
            contact->frame = frame;
        }
        if (previous != open)
            cui_invalidate(ui);
        result.lifecycle.control_result =
            cui_content_begin_hover_card_model(ui, id, theme, state, name, options, false);
        if (contact && !state->open)
            memset(contact, 0, sizeof(*contact));
    }
    if (p.relationship && frame && frame == cui_frame_sequence(ui))
        result.relationship_applied = cui_hover_card_ext_relationship(
            ui, options.trigger, id, result.lifecycle.control_result);
    result.lifecycle.changed = previous != state->open;
    if (result.lifecycle.changed && frame && frame == cui_frame_sequence(ui) &&
        observer.on_change) {
        cui_hover_lifecycle_event event;
        memset(&event, 0, sizeof(event));
        (void)cui_component_event_init(&event.event, CUI_COMPONENT_EVENT_LIFECYCLE, id,
                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
        event.event.flags = CUI_COMPONENT_EVENT_HAS_FRAME;
        event.event.frame_sequence = frame;
        event.previous_open = previous;
        event.open = state->open;
        result.lifecycle.change_count = 1;
        observer.on_change(observer.user, &event);
    }
    return result;
}
