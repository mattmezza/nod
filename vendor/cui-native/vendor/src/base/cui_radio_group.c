/* SPDX-License-Identifier: MIT */
#include "cui/cui_radio_group.h"
#include <math.h>
static size_t cui_radio_group_find(const cui_radio_group_item *items, size_t count, cui_id id) {
    for (size_t i = 0; i < count; i++)
        if (items[i].id == id)
            return i;
    return count;
}
static size_t cui_radio_group_enabled(const cui_radio_group_item *items, size_t count, size_t from,
                                      int direction) {
    for (size_t n = 0; n < count; n++) {
        from = direction > 0 ? (from + 1) % count : (from + count - 1) % count;
        if (!items[from].disabled)
            return from;
    }
    return count;
}
static cui_radio_group_result
cui_radio_group_reduce(cui_context *ui, cui_id id, const cui_radio_group_item *items, size_t count,
                       cui_id *selected, cui_radio_group_state *state, cui_radio_group_options o,
                       cui_selection_handler handler, cui_selection_event_result *observed) {
    cui_radio_group_result r = {0};
    if (!ui || !id || !selected || !state || count > CUI_RADIO_GROUP_MAX || (count && !items) ||
        !isfinite(o.gap) || o.gap < 0 || o.size > CUI_CONTROL_XSMALL ||
        o.size < CUI_CONTROL_MEDIUM || o.design_size < CUI_DESIGN_INHERIT ||
        o.design_size > CUI_DESIGN_9XL || (o.name.length && !o.name.data))
        return r;
    for (size_t i = 0; i < count; i++) {
        if (!items[i].id || items[i].id == id || (items[i].label.length && !items[i].label.data))
            return r;
        for (size_t j = 0; j < i; j++)
            if (items[i].id == items[j].id)
                return r;
    }
    r.valid = true;
    bool submitting = cui_frame_is_submitting(ui) && cui_frame_sequence(ui) != 0;
    if (observed) {
        observed->valid = true;
        observed->submitted = submitting;
    }
    size_t first = count;
    for (size_t i = 0; i < count; i++)
        if (!items[i].disabled) {
            first = i;
            break;
        }
    cui_id focused = cui_focused(ui);
    size_t active = cui_radio_group_find(items, count, focused),
           chosen = cui_radio_group_find(items, count, *selected);
    size_t tab = chosen < count && !items[chosen].disabled ? chosen : first;
    if (!o.disabled && first < count &&
        (active < count || (state->had_focus && (!focused || focused == state->focused)))) {
        if (active == count || items[active].disabled) {
            active = tab;
            cui_focus(ui, items[active].id);
        }
        tab = active;
    } else if ((o.disabled && active < count) ||
               (first == count && state->had_focus && focused == state->focused))
        cui_focus(ui, 0);
    size_t event_count;
    const cui_event *events = cui_frame_events(ui, &event_count);
    for (size_t e = 0; e < event_count; e++) {
        if (cui_event_consumed(ui, e))
            continue;
        size_t target = cui_radio_group_find(items, count, cui_event_target(ui, e));
        size_t activate = cui_radio_group_find(items, count, cui_event_activation(ui, e));
        size_t request = count;
        cui_component_event_metadata metadata;
        memset(&metadata, 0, sizeof(metadata));
        bool has_metadata = false;
        if (observed && handler.on_change && submitting) {
            /* Legacy RadioGroup registers semantics after reduction. Accepted
             * activation snapshots are unavailable at this seam; preserve that
             * timing and report only genuine event type/frame/index facts. */
            cui_component_event_source source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN;
            if (events[e].type == CUI_EVENT_POINTER_UP)
                source = CUI_COMPONENT_EVENT_SOURCE_POINTER;
            else if (events[e].type == CUI_EVENT_KEY_DOWN || events[e].type == CUI_EVENT_KEY_UP)
                source = CUI_COMPONENT_EVENT_SOURCE_KEYBOARD;
            else if (events[e].type == CUI_EVENT_ACCESSIBILITY_ACTION)
                source = CUI_COMPONENT_EVENT_SOURCE_ACCESSIBILITY;
            has_metadata =
                cui_component_event_init(&metadata, CUI_COMPONENT_EVENT_CHANGE, id, source) &&
                cui_component_event_set_frame_index(&metadata, cui_frame_sequence(ui), e);
        }
        if (activate < count) {
            cui_cancel_event_activation(ui, e);
            cui_consume_event(ui, e);
            if (!o.disabled && !items[activate].disabled)
                request = activate;
        } else if (!o.disabled && target < count && !items[target].disabled &&
                   events[e].type == CUI_EVENT_KEY_DOWN) {
            size_t current = cui_radio_group_find(items, count, cui_focused(ui));
            if (current < count && !items[current].disabled)
                target = current;
            switch (events[e].key) {
            case CUI_KEY_RIGHT:
            case CUI_KEY_DOWN:
                request = cui_radio_group_enabled(items, count, target, 1);
                break;
            case CUI_KEY_LEFT:
            case CUI_KEY_UP:
                request = cui_radio_group_enabled(items, count, target, -1);
                break;
            case CUI_KEY_HOME:
                request = first;
                break;
            case CUI_KEY_END:
                for (size_t i = count; i > 0; i--)
                    if (!items[i - 1].disabled) {
                        request = i - 1;
                        break;
                    }
                break;
            default:
                break;
            }
            if (request < count)
                cui_consume_event(ui, e);
        }
        if (request < count) {
            cui_focus(ui, items[request].id);
            tab = request;
            if (*selected != items[request].id) {
                cui_id before = *selected, attempted = items[request].id;
                r.requested = true;
                if (o.on_change)
                    o.on_change(o.change_user, items[request].id);
                else
                    *selected = items[request].id;
                cui_id after = *selected;
                r.changed |= before != after;
                if (before != after && observed && handler.on_change && submitting) {
                    if (!has_metadata)
                        (void)cui_component_event_init(&metadata, CUI_COMPONENT_EVENT_CHANGE, id,
                                                       CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
                    cui_selection_change change = {metadata, before, after, attempted,
                                                   activate < count
                                                       ? CUI_SELECTION_REASON_ACTIVATION
                                                       : CUI_SELECTION_REASON_NAVIGATION};
                    if (observed->change_count == SIZE_MAX)
                        observed->count_overflow = true;
                    else
                        observed->change_count++;
                    handler.on_change(handler.user, &change);
                }
            }
        }
    }
    if (!o.disabled && !o.skip_tab && tab < count)
        r.tab_stop = items[tab].id;
    state->focused = cui_focused(ui);
    state->had_focus = cui_radio_group_find(items, count, state->focused) < count;
    cui_interact(ui, id, CUI_ROLE_RADIOGROUP, o.name, o.disabled, false, 0);
    cui_set_tab_stop(ui, id, false);
    cui_set_semantic_details(ui, id,
                             (cui_semantic_details){.orientation = o.vertical
                                                                       ? CUI_ORIENTATION_VERTICAL
                                                                       : CUI_ORIENTATION_HORIZONTAL,
                                                    .required = o.required,
                                                    .invalid = o.invalid});
    for (size_t i = 0; i < count; i++) {
        cui_interact(ui, items[i].id, CUI_ROLE_RADIO, items[i].label,
                     o.disabled || items[i].disabled, *selected == items[i].id, (float)i);
        cui_set_tab_stop(ui, items[i].id, items[i].id == r.tab_stop);
        cui_set_semantic_details(ui, items[i].id,
                                 (cui_semantic_details){.parent = id,
                                                        .pos_in_set = (unsigned)i + 1,
                                                        .set_size = (unsigned)count,
                                                        .required = o.required,
                                                        .invalid = o.invalid});
    }
    if (r.changed)
        cui_invalidate(ui);
    return r;
}

cui_radio_group_result cui_base_radio_group(cui_context *ui, cui_id id,
                                            const cui_radio_group_item *items, size_t count,
                                            cui_id *selected, cui_radio_group_state *state,
                                            cui_radio_group_options options) {
    return cui_radio_group_reduce(ui, id, items, count, selected, state, options,
                                  (cui_selection_handler){0}, NULL);
}
cui_radio_group_event_result
cui_base_radio_group_events(cui_context *ui, cui_id id, const cui_radio_group_item *items,
                            size_t count, cui_id *selected, cui_radio_group_state *state,
                            cui_radio_group_options options, cui_selection_handler handler) {
    cui_radio_group_event_result result;
    memset(&result, 0, sizeof(result));
    result.legacy = cui_radio_group_reduce(ui, id, items, count, selected, state, options, handler,
                                           &result.events);
    return result;
}
