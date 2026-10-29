/* SPDX-License-Identifier: MIT */
#include "cui/cui_settings_events.h"
#include <math.h>
#include <string.h>
static bool valid_string(cui_str s) {
    return !s.length || s.data;
}
static unsigned char lower(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}
static bool contains(cui_str text, cui_str query) {
    if (!query.length)
        return true;
    if (query.length > text.length)
        return false;
    for (size_t i = 0; i <= text.length - query.length; i++) {
        size_t j = 0;
        while (j < query.length &&
               lower((unsigned char)text.data[i + j]) == lower((unsigned char)query.data[j]))
            j++;
        if (j == query.length)
            return true;
    }
    return false;
}
bool cui_setting_item_matches(const cui_setting_item *item, cui_str query) {
    if (!item || !valid_string(query) || !valid_string(item->title) ||
        !valid_string(item->description) || (item->keyword_count && !item->keywords))
        return false;
    if (item->matches)
        return item->matches(query, item->user);
    if (!query.length)
        return true;
    if (!item->custom_item && (contains(item->title, query) || contains(item->description, query)))
        return true;
    for (size_t i = 0; i < item->keyword_count; i++)
        if (valid_string(item->keywords[i]) && contains(item->keywords[i], query))
            return true;
    return false;
}
bool cui_setting_group_matches(const cui_setting_group *group, cui_str query) {
    if (!group || (group->count && !group->items))
        return false;
    for (size_t i = 0; i < group->count; i++)
        if (cui_setting_item_matches(&group->items[i], query))
            return true;
    return false;
}
bool cui_settings_state_init(cui_settings_state *state, cui_settings_page_state *pages,
                             size_t capacity, cui_edit *search) {
    if (!state || !pages || !capacity || capacity > 4096 ||
        (search && cui_edit_get_state(search).multiline))
        return false;
    memset(state, 0, sizeof(*state));
    memset(pages, 0, capacity * sizeof(*pages));
    state->pages = pages;
    state->capacity = capacity;
    state->search = search;
    state->selected_group = CUI_SETTINGS_NO_GROUP;
    return true;
}
cui_settings_options cui_settings_default(void) {
    return (cui_settings_options){.sidebar_width = 250,
                                  .sidebar_minimum = 160,
                                  .sidebar_maximum = 360,
                                  .default_group = CUI_SETTINGS_NO_GROUP,
                                  .search_placeholder = {"Search settings", 15},
                                  .reset_label = {"Reset All", 9}};
}
cui_settings_result cui_settings_filter(const cui_setting_page *pages, size_t count,
                                        cui_settings_state *state, cui_settings_options options) {
    cui_settings_result result = {0};
    cui_str query = state && state->search ? cui_edit_get_text(state->search) : options.query;
    if (!state || !state->pages || count > state->capacity || (count && !pages) ||
        !valid_string(query) || count > 4096) {
        result.invalid = true;
        return result;
    }
    for (size_t p = 0; p < count; p++) {
        if (!valid_string(pages[p].title) || !valid_string(pages[p].description) ||
            !valid_string(pages[p].icon) || (pages[p].count && !pages[p].groups) ||
            pages[p].count > 4096) {
            result.invalid = true;
            return result;
        }
        for (size_t g = 0; g < pages[p].count; g++) {
            const cui_setting_group *group = &pages[p].groups[g];
            if ((group->custom_variant &&
                 (group->variant < CUI_SETTINGS_NORMAL || group->variant > CUI_SETTINGS_OUTLINE)) ||
                (group->count && !group->items) || group->count > 65536 ||
                !valid_string(group->title) || !valid_string(group->description)) {
                result.invalid = true;
                return result;
            }
            for (size_t i = 0; i < group->count; i++) {
                const cui_setting_item *item = &group->items[i];
                if (item->kind < CUI_SETTING_CUSTOM || item->kind > CUI_SETTING_NUMBER ||
                    !valid_string(item->default_text) || !valid_string(item->title) ||
                    !valid_string(item->description) || (item->keyword_count && !item->keywords) ||
                    item->keyword_count > 4096) {
                    result.invalid = true;
                    return result;
                }
                if (!item->render) {
                    bool valid = false;
                    switch (item->kind) {
                    case CUI_SETTING_SWITCH:
                    case CUI_SETTING_CHECKBOX:
                        valid = item->boolean != NULL;
                        break;
                    case CUI_SETTING_INPUT:
                        valid = item->edit && item->input_state &&
                                !cui_edit_get_state(item->edit).multiline;
                        break;
                    case CUI_SETTING_NUMBER:
                        valid = item->number && isfinite(*item->number) && item->number_state &&
                                item->number_state->edit &&
                                !cui_edit_get_state(item->number_state->edit).multiline;
                        break;
                    case CUI_SETTING_DROPDOWN:
                        valid = item->choice && (!item->choice_count || item->choices) &&
                                item->choice_count <= 4096;
                        if (item->dropdown_state) {
                            const cui_settings_dropdown_state *dropdown = item->dropdown_state;
                            valid =
                                valid && dropdown->menu.overlay.id &&
                                dropdown->capacity >= item->choice_count &&
                                (!item->choice_count || (dropdown->entries && dropdown->checked));
                        } else {
                            valid = valid && item->popup && item->popup->overlay.id;
                        }
                        break;
                    default:
                        break;
                    }
                    if (item->custom_item || !valid) {
                        result.invalid = true;
                        return result;
                    }
                }
                for (size_t k = 0; k < item->keyword_count; k++)
                    if (!valid_string(item->keywords[k])) {
                        result.invalid = true;
                        return result;
                    }
            }
        }
    }
    if (!state->initialized) {
        state->selected_page = options.default_page;
        state->selected_group =
            options.default_group_selected ? options.default_group : CUI_SETTINGS_NO_GROUP;
        state->sidebar_width = options.sidebar_width > 0 ? options.sidebar_width : 250;
        state->initialized = true;
    }
    size_t previous_page = state->selected_page, previous_group = state->selected_group,
           first = CUI_SETTINGS_NO_GROUP;
    bool selected_visible = false, group_visible = false;
    for (size_t p = 0; p < count; p++) {
        bool visible = false;
        for (size_t g = 0; g < pages[p].count; g++)
            if (cui_setting_group_matches(&pages[p].groups[g], query)) {
                visible = true;
                result.visible_groups++;
                if (p == previous_page && g == previous_group)
                    group_visible = true;
                for (size_t i = 0; i < pages[p].groups[g].count; i++)
                    result.visible_items +=
                        cui_setting_item_matches(&pages[p].groups[g].items[i], query);
            }
        if (visible) {
            result.visible_pages++;
            if (first == CUI_SETTINGS_NO_GROUP)
                first = p;
            if (p == previous_page)
                selected_visible = true;
        }
    }
    if (result.visible_pages) {
        if (!selected_visible)
            state->selected_page = first;
        if (!selected_visible || !group_visible)
            state->selected_group = CUI_SETTINGS_NO_GROUP;
    }
    result.selection_changed =
        previous_page != state->selected_page || previous_group != state->selected_group;
    if (result.selection_changed)
        state->pending_group_scroll = false;
    return result;
}
bool cui_setting_item_dirty(const cui_setting_item *item) {
    if (!item)
        return false;
    if (item->dirty)
        return item->dirty(item->user);
    if (!item->has_default || item->custom_item || item->kind == CUI_SETTING_CUSTOM)
        return false;
    switch (item->kind) {
    case CUI_SETTING_SWITCH:
    case CUI_SETTING_CHECKBOX:
        return item->boolean && *item->boolean != item->default_boolean;
    case CUI_SETTING_NUMBER:
        return item->number && *item->number != item->default_number;
    case CUI_SETTING_DROPDOWN:
        return item->choice && *item->choice != item->default_choice;
    case CUI_SETTING_INPUT: {
        if (!item->edit || !valid_string(item->default_text))
            return false;
        cui_str text = cui_edit_get_text(item->edit);
        return text.length != item->default_text.length ||
               (text.length && memcmp(text.data, item->default_text.data, text.length));
    }
    default:
        return false;
    }
}
static bool reset_item(const cui_setting_item *item) {
    if (item->reset) {
        item->reset(item->user);
        return true;
    }
    if (!item->has_default || item->custom_item)
        return false;
    bool done = false;
    switch (item->kind) {
    case CUI_SETTING_SWITCH:
    case CUI_SETTING_CHECKBOX:
        if (item->boolean) {
            *item->boolean = item->default_boolean;
            done = true;
        }
        break;
    case CUI_SETTING_NUMBER:
        if (item->number) {
            *item->number = item->default_number;
            done = true;
        }
        break;
    case CUI_SETTING_DROPDOWN:
        if (item->choice) {
            *item->choice = item->default_choice;
            done = true;
        }
        break;
    case CUI_SETTING_INPUT:
        if (item->edit) {
            done = cui_edit_replace_range(item->edit, 0, cui_edit_get_state(item->edit).length,
                                          item->default_text) == CUI_EDIT_OK;
        }
        break;
    default:
        break;
    }
    if (done && item->on_change)
        item->on_change(item->user);
    return done;
}
/* Internal cross-layer seam: no input dispatch, allocation or renderer dependency.
 * The components layer freezes genuine UI evidence before calling this operation. */
cui_settings_reset_event_result cui_settings_reset_page_events_internal(
    cui_id root, size_t page_index, const cui_setting_page *page, cui_str query,
    cui_settings_handler handler, const cui_component_event_metadata *evidence) {
    cui_settings_reset_event_result out = {0};
    if (!page || page->no_reset || !valid_string(query) || (page->count && !page->groups))
        return out;
    for (size_t g = 0; g < page->count; g++) {
        const cui_setting_group *group = &page->groups[g];
        if (group->count && !group->items) {
            out.reset = false;
            return out;
        }
        for (size_t i = 0; i < group->count; i++) {
            const cui_setting_item *item = &group->items[i];
            if (!cui_setting_item_matches(item, query))
                continue;
            bool observe = evidence && !item->reset && item->has_default && !item->custom_item;
            bool boolean = false;
            double number = 0;
            cui_id choice = 0;
            cui_edit_state text = {0};
            if (observe) {
                if ((item->kind == CUI_SETTING_SWITCH || item->kind == CUI_SETTING_CHECKBOX) &&
                    item->boolean && handler.on_boolean)
                    boolean = *item->boolean;
                else if (item->kind == CUI_SETTING_NUMBER && item->number && handler.on_number)
                    number = *item->number;
                else if (item->kind == CUI_SETTING_DROPDOWN && item->choice && handler.on_choice)
                    choice = *item->choice;
                else if (item->kind == CUI_SETTING_INPUT && item->edit && handler.on_text)
                    text = cui_edit_get_state(item->edit);
            }
            out.reset |= reset_item(item);
            if (!observe)
                continue;
            cui_id field = cui_settings_part_id(root, page_index, g, i, 24);
            size_t *count = NULL;
            if ((item->kind == CUI_SETTING_SWITCH || item->kind == CUI_SETTING_CHECKBOX) &&
                item->boolean && handler.on_boolean && boolean != *item->boolean) {
                cui_settings_bool_change event = {
                    *evidence, field,   page_index,     g,
                    i,         boolean, *item->boolean, CUI_SETTINGS_CHANGE_RESET};
                handler.on_boolean(handler.user, &event);
                count = &out.boolean_count;
            } else if (item->kind == CUI_SETTING_NUMBER && item->number && handler.on_number &&
                       number != *item->number && !(isnan(number) && isnan(*item->number))) {
                cui_settings_number_change event = {*evidence,
                                                    field,
                                                    page_index,
                                                    g,
                                                    i,
                                                    number,
                                                    *item->number,
                                                    isfinite(number),
                                                    isfinite(*item->number),
                                                    CUI_SETTINGS_CHANGE_RESET};
                handler.on_number(handler.user, &event);
                count = &out.number_count;
            } else if (item->kind == CUI_SETTING_DROPDOWN && item->choice && handler.on_choice &&
                       choice != *item->choice) {
                cui_settings_choice_change event = {
                    *evidence, field,  page_index,    g,
                    i,         choice, *item->choice, CUI_SETTINGS_CHANGE_RESET};
                handler.on_choice(handler.user, &event);
                count = &out.choice_count;
            } else if (item->kind == CUI_SETTING_INPUT && item->edit && handler.on_text) {
                cui_edit_state after = cui_edit_get_state(item->edit);
                if (text.revision != after.revision) {
                    cui_settings_text_change event = {*evidence,
                                                      field,
                                                      page_index,
                                                      g,
                                                      i,
                                                      text.revision,
                                                      after.revision,
                                                      text.length,
                                                      after.length,
                                                      cui_edit_get_text(item->edit),
                                                      CUI_SETTINGS_CHANGE_RESET};
                    handler.on_text(handler.user, &event);
                    count = &out.text_count;
                }
            }
            if (count) {
                if (*count == SIZE_MAX)
                    out.count_overflow = true;
                else
                    ++*count;
            }
        }
    }
    return out;
}
bool cui_settings_reset_page(const cui_setting_page *page, cui_str query) {
    return cui_settings_reset_page_events_internal(0, 0, page, query, (cui_settings_handler){0},
                                                   NULL)
        .reset;
}
cui_settings_reset_event_result cui_settings_reset_page_events(cui_id root, size_t page_index,
                                                               const cui_setting_page *page,
                                                               cui_str query,
                                                               cui_settings_handler handler) {
    cui_component_event_metadata evidence;
    if (!root || !page ||
        !cui_component_event_init(&evidence, CUI_COMPONENT_EVENT_CHANGE, root,
                                  CUI_COMPONENT_EVENT_SOURCE_PROGRAMMATIC))
        return (cui_settings_reset_event_result){0};
    return cui_settings_reset_page_events_internal(root, page_index, page, query, handler,
                                                   &evidence);
}
cui_id cui_settings_part_id(cui_id root, size_t page, size_t group, size_t item, unsigned part) {
    uint64_t values[4] = {page, group, item, part}, hash = root ^ UINT64_C(0x612f695f419ab257);
    for (size_t i = 0; i < 4; i++) {
        hash ^= values[i] + UINT64_C(0x9e3779b97f4a7c15) + (hash << 6) + (hash >> 2);
    }
    return hash ? hash : 1;
}
