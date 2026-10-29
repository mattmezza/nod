/* SPDX-License-Identifier: MIT */
#include "cui/cui_button_group.h"
#include <string.h>

static bool same_frame(cui_context *ui, cui_button_group_workspace *w) {
    return w && w->active && !w->dispatching && w->context == ui && w->frame_sequence &&
           w->frame_sequence == cui_frame_sequence(ui);
}
static bool string_valid(cui_str s) {
    return !s.length || s.data;
}
bool cui_base_button_group_begin(cui_context *ui, cui_id id, const cui_button_group_item *items,
                                 bool *selected, size_t count, cui_button_group_workspace *w,
                                 cui_button_group_options o) {
    if (!ui || !id || !w || w->active || !cui_frame_sequence(ui) ||
        count > CUI_BUTTON_GROUP_MAX_ITEMS || (count && (!items || !selected)) ||
        o.activation_policy < CUI_BUTTON_GROUP_SOURCE_POINTER_CALLBACKS ||
        o.activation_policy > CUI_BUTTON_GROUP_ALL_ACTIVATIONS ||
        !string_valid(o.accessibility_label))
        return false;
    uintptr_t start = (uintptr_t)w, end = start + sizeof *w;
    if (count && (uintptr_t)selected < end &&
        (uintptr_t)selected + count * sizeof *selected > start)
        return false;
    for (size_t i = 0; i < count; i++) {
        if (!items[i].id || items[i].id == id || !string_valid(items[i].label) ||
            (items[i].role != CUI_ROLE_NONE && items[i].role != CUI_ROLE_BUTTON &&
             items[i].role != CUI_ROLE_LINK) ||
            (items[i].custom_cursor &&
             (items[i].cursor < CUI_CURSOR_DEFAULT || items[i].cursor > CUI_CURSOR_HELP)))
            return false;
        for (size_t j = 0; j < i; j++)
            if (items[i].id == items[j].id)
                return false;
    }
    /* Copy before clearing: a styled wrapper may supply workspace-owned items. */
    if (items != w->items && count)
        memmove(w->items, items, count * sizeof *items);
    memset(w->registered, 0, sizeof w->registered);
    memset(w->interactions, 0, sizeof w->interactions);
    w->context = ui;
    w->frame_sequence = cui_frame_sequence(ui);
    w->id = id;
    w->selected = selected;
    w->count = count;
    w->registered_count = 0;
    w->options = o;
    w->active = true;
    w->failed = w->dispatching = false;
    cui_interact(ui, id, CUI_ROLE_TOOLBAR, o.accessibility_label, o.disabled, false, 0);
    cui_set_tab_stop(ui, id, false);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.no_focus = true,
                               .pointer_activatable = o.on_change && !o.disabled,
                               .orientation = o.vertical ? CUI_ORIENTATION_VERTICAL
                                                         : CUI_ORIENTATION_HORIZONTAL});
    w->failed = cui_last_error(ui) != CUI_OK;
    return !w->failed;
}
cui_interaction cui_base_button_group_item(cui_context *ui, cui_button_group_workspace *w,
                                           size_t index) {
    if (!same_frame(ui, w) || w->failed || index >= w->count || w->registered[index]) {
        if (w && w->active)
            w->failed = true;
        return (cui_interaction){0};
    }
    cui_button_group_item item = w->items[index];
    if (item.role == CUI_ROLE_NONE)
        item.role = CUI_ROLE_BUTTON;
    bool inert = w->options.disabled || item.disabled || item.loading;
    cui_interaction result = cui_interact(ui, item.id, item.role, item.label, inert,
                                          w->selected[index], w->selected[index] ? 1 : 0);
    cui_set_semantic_details(
        ui, item.id,
        (cui_semantic_details){.busy = item.loading,
                               .description = item.loading ? cui_string("Loading") : (cui_str){0},
                               .no_pointer_focus = item.prevent_pointer_focus,
                               .pointer_activatable = item.prevent_pointer_focus,
                               .blocks_pointer_activation = inert});
    cui_set_tab_stop(ui, item.id, !item.skip_tab);
    if (item.custom_cursor)
        cui_set_cursor(ui, item.id, item.cursor);
    w->registered[index] = true;
    ++w->registered_count;
    w->interactions[index] = result;
    w->failed = cui_last_error(ui) != CUI_OK;
    return result;
}
cui_button_group_result cui_base_button_group_end(cui_context *ui, cui_button_group_workspace *w) {
    cui_button_group_result result = {0};
    if (!same_frame(ui, w) || w->failed || w->registered_count != w->count ||
        cui_last_error(ui) != CUI_OK) {
        if (w && w->active && !w->dispatching) {
            w->active = false;
            w->failed = true;
        }
        return result;
    }
    result.valid = true;
    w->dispatching = true;
    size_t count = 0;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t e = 0; e < count; e++) {
        cui_id accepted = cui_event_accepted_activation(ui, e);
        if (!accepted)
            continue;
        size_t index = SIZE_MAX;
        for (size_t i = 0; i < w->count; i++)
            if (w->items[i].id == accepted)
                index = i;
        if (index == SIZE_MAX && accepted != w->id)
            continue;
        ++result.accepted_activations;
        if (!w->options.on_change) {
            if (index != SIZE_MAX && w->items[index].prepare_click_event) {
                cui_activation_snapshot snapshot;
                if (cui_event_activation_snapshot(ui, e, &snapshot))
                    w->items[index].prepare_click_event(w->items[index].event_user, ui, &snapshot);
            }
            if (index != SIZE_MAX && w->items[index].on_click) {
                w->items[index].on_click(w->items[index].action_user);
                ++result.callbacks;
            }
            if (index != SIZE_MAX && w->items[index].on_click_event) {
                w->items[index].on_click_event(w->items[index].event_user, ui, e);
                ++result.callbacks;
            }
            continue;
        }
        bool pointer = events[e].type == CUI_EVENT_POINTER_UP;
        if (!pointer && w->options.activation_policy == CUI_BUTTON_GROUP_SOURCE_POINTER_CALLBACKS)
            continue;
        size_t n = 0;
        for (size_t i = 0; i < w->count; i++)
            if (w->selected[i])
                w->requested_indices[n++] = i;
        if (index != SIZE_MAX) {
            if (!w->options.multiple) {
                w->requested_indices[0] = index;
                n = 1;
            } else if (w->selected[index]) {
                size_t pos = 0;
                while (pos < n && w->requested_indices[pos] != index)
                    ++pos;
                if (pos < n) {
                    memmove(w->requested_indices + pos, w->requested_indices + pos + 1,
                            (n - pos - 1) * sizeof(size_t));
                    --n;
                }
            } else
                w->requested_indices[n++] = index;
        }
        cui_button_group_request request = {accepted, index, e, w->requested_indices, n};
        w->options.on_change(w->options.change_user, &request);
        ++result.callbacks;
    }
    for (size_t i = 0; i < w->count; i++)
        cui_update_semantic(ui, w->items[i].id, w->selected[i], w->selected[i] ? 1 : 0);
    w->dispatching = w->active = false;
    return result;
}
