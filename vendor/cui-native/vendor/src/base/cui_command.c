/* SPDX-License-Identifier: MIT */
#include "cui_command_events_private.h"
#include <ctype.h>
#include <math.h>
#include <string.h>
static bool cui_command_str_valid(cui_str s) {
    return !s.length || s.data;
}
static cui_str cui_command_trim(cui_str q) {
    while (q.length && isspace((unsigned char)*q.data)) {
        ++q.data;
        --q.length;
    }
    while (q.length && isspace((unsigned char)q.data[q.length - 1]))
        --q.length;
    return q;
}
static unsigned char cui_command_lower(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}
static bool cui_command_contains(cui_str s, cui_str q) {
    if (!q.length)
        return true;
    if (!cui_command_str_valid(s) || s.length < q.length)
        return false;
    for (size_t i = 0; i <= s.length - q.length; ++i) {
        size_t j = 0;
        while (j < q.length && cui_command_lower((unsigned char)s.data[i + j]) ==
                                   cui_command_lower((unsigned char)q.data[j]))
            ++j;
        if (j == q.length)
            return true;
    }
    return false;
}
bool cui_command_matches(const cui_command_item *i, cui_str q) {
    if (!i || !cui_command_str_valid(q))
        return false;
    q = cui_command_trim(q);
    if (cui_command_contains(i->label, q))
        return true;
    if (i->keyword_count && !i->keywords)
        return false;
    for (size_t k = 0; k < i->keyword_count; ++k)
        if (cui_command_contains(i->keywords[k], q))
            return true;
    return false;
}
cui_command_options cui_command_default(void) {
    return (cui_command_options){
        .searchable = true, .filterable = true, .bordered = true, .width = 380, .max_height = 300};
}
bool cui_command_init(cui_command_state *s, cui_command_row *rows, size_t n, cui_edit *q) {
    if (!s || !rows || !n || n > CUI_COMMAND_LIMIT)
        return false;
    *s = (cui_command_state){.rows = rows, .capacity = n, .query = q, .selected = SIZE_MAX};
    return true;
}
bool cui_command_index_equal(cui_command_index a, cui_command_index b) {
    return a.section == b.section && a.row == b.row;
}
bool cui_command_selected(const cui_command_state *s, cui_command_index *out) {
    if (!s || s->selected >= s->row_count || s->rows[s->selected].kind != CUI_COMMAND_ROW_ITEM)
        return false;
    if (out)
        *out = s->rows[s->selected].index;
    return true;
}
static bool cui_command_item_valid(const cui_command_item *i) {
    if (!cui_command_str_valid(i->label) || !cui_command_str_valid(i->icon) ||
        i->keyword_count > CUI_COMMAND_LIMIT || (i->keyword_count && !i->keywords))
        return false;
    for (size_t n = 0; n < i->keyword_count; ++n)
        if (!cui_command_str_valid(i->keywords[n]))
            return false;
    return true;
}
static bool cui_command_match(const cui_command_item *i, cui_str q, cui_command_options o) {
    return !o.searchable || !o.filterable ||
           (o.matches ? o.matches(i, q, o.match_user) : cui_command_matches(i, q));
}
static cui_component_event_metadata
cui_command_events_private_metadata(const cui_command_events_private_observer *observer,
                                    cui_component_event_kind kind) {
    cui_component_event_metadata event = {0};
    if (observer->has_cause) {
        event = observer->cause;
        event.kind = kind;
    } else
        (void)cui_component_event_init(&event, kind, observer->target,
                                       CUI_COMPONENT_EVENT_SOURCE_PROGRAMMATIC);
    return event;
}
static void cui_command_events_private_count(size_t *count, bool *overflow) {
    if (*count == SIZE_MAX)
        *overflow = true;
    else
        ++*count;
}
static void
cui_command_events_private_notify_selection(cui_command_events_private_observer *observer,
                                            const cui_command_selection_change *change) {
    if (!observer || observer->suppress || !observer->handler.on_selection)
        return;
    cui_command_events_private_count(&observer->result.selection_count,
                                     &observer->result.count_overflow);
    observer->handler.on_selection(observer->handler.user, change);
}
bool cui_command_events_private_rebuild(cui_command_state *s, const cui_command_entry *entries,
                                        size_t n, cui_str q, bool changed, cui_command_options o,
                                        cui_command_events_private_observer *observer) {
    if (!s || !s->rows || !s->capacity || s->capacity > CUI_COMMAND_LIMIT ||
        n > CUI_COMMAND_LIMIT || (n && !entries) || !cui_command_str_valid(q))
        return false;
    size_t required = 0, total = 0;
    bool ungrouped = false;
    /* Reserve worst-case rows before writing, including filtered entries: strict bounded contract.
     */
    for (size_t e = 0; e < n; ++e) {
        const cui_command_entry *v = &entries[e];
        if (v->kind == CUI_COMMAND_ITEM) {
            if (!cui_command_item_valid(&v->item))
                return false;
            ++required;
            ++total;
            ungrouped = true;
        } else if (v->kind == CUI_COMMAND_GROUP) {
            if (v->item_count > CUI_COMMAND_LIMIT || (v->item_count && !v->items) ||
                !cui_command_str_valid(v->heading))
                return false;
            if (required > CUI_COMMAND_LIMIT - v->item_count)
                return false;
            required += v->item_count + (v->heading.length ? 1 : 0);
            total += v->item_count;
            for (size_t i = 0; i < v->item_count; ++i)
                if (!cui_command_item_valid(&v->items[i]))
                    return false;
        } else if (v->kind == CUI_COMMAND_SEPARATOR)
            ++required;
        else
            return false;
        if (required > s->capacity || total > CUI_COMMAND_LIMIT)
            return false;
    }
    cui_command_index old = {0};
    bool had = cui_command_selected(s, &old);
    size_t count = 0, matched = 0, group = ungrouped ? 1 : 0, item = 0;
    bool separator = false;
    cui_str raw_query = q;
    q = cui_command_trim(q);
    for (size_t e = 0; e < n; ++e) {
        const cui_command_entry *v = &entries[e];
        if (v->kind == CUI_COMMAND_SEPARATOR) {
            separator = count > 0;
            continue;
        }
        size_t section = v->kind == CUI_COMMAND_ITEM ? 0 : group++;
        size_t length = v->kind == CUI_COMMAND_ITEM ? 1 : v->item_count;
        bool heading = false;
        for (size_t i = 0; i < length; ++i) {
            const cui_command_item *it = v->kind == CUI_COMMAND_ITEM ? &v->item : &v->items[i];
            size_t row = v->kind == CUI_COMMAND_ITEM ? item++ : i;
            if (!cui_command_match(it, q, o))
                continue;
            if (separator) {
                s->rows[count++] = (cui_command_row){.kind = CUI_COMMAND_ROW_SEPARATOR};
                separator = false;
            }
            if (v->kind == CUI_COMMAND_GROUP && !heading) {
                heading = true;
                if (v->heading.length)
                    s->rows[count++] =
                        (cui_command_row){.kind = CUI_COMMAND_ROW_HEADING, .heading = v->heading};
            }
            s->rows[count++] = (cui_command_row){
                .kind = CUI_COMMAND_ROW_ITEM, .index = {section, row}, .item = it};
            ++matched;
        }
    }
    s->row_count = count;
    s->matched_count = matched;
    s->selected = SIZE_MAX;
    if (!changed && had)
        for (size_t i = 0; i < count; ++i)
            if (s->rows[i].item && !s->rows[i].item->disabled &&
                cui_command_index_equal(old, s->rows[i].index)) {
                s->selected = i;
                break;
            }
    if (s->selected == SIZE_MAX && (changed || !s->preserve_no_selection)) {
        for (size_t i = 0; i < count; ++i)
            if (s->rows[i].item && !s->rows[i].item->disabled) {
                s->selected = i;
                break;
            }
        s->pending_scroll = true;
    }
    if (changed)
        s->preserve_no_selection = false;
    s->initialized = true;
    cui_command_index now = {0};
    bool selection_changed =
        changed && cui_command_selected(s, &now) && (!had || !cui_command_index_equal(old, now));
    cui_command_selection_change selection = {0};
    if (observer) {
        selection.event = cui_command_events_private_metadata(observer, CUI_COMPONENT_EVENT_CHANGE);
        selection.has_previous = had;
        selection.previous = old;
        selection.value = now;
        /* Borrowed query must be observed before either legacy hook can replace it. */
        if (changed && o.searchable && !observer->suppress && observer->handler.on_query) {
            cui_command_query_change query = {
                cui_command_events_private_metadata(observer, CUI_COMPONENT_EVENT_CHANGE),
                raw_query};
            cui_command_events_private_count(&observer->result.query_count,
                                             &observer->result.count_overflow);
            observer->handler.on_query(observer->handler.user, &query);
        }
    }
    if (selection_changed) {
        if (o.on_select)
            o.on_select(now, o.user);
        cui_command_events_private_notify_selection(observer, &selection);
    }
    if (changed && o.searchable && o.on_query)
        o.on_query(raw_query, o.user);
    return true;
}
bool cui_command_events_private_select(cui_command_state *s, cui_command_index index, bool scroll,
                                       cui_command_options o,
                                       cui_command_events_private_observer *observer) {
    if (!s)
        return false;
    for (size_t i = 0; i < s->row_count; ++i)
        if (s->rows[i].item && !s->rows[i].item->disabled &&
            cui_command_index_equal(index, s->rows[i].index)) {
            if (s->selected == i)
                return false;
            cui_command_selection_change change = {0};
            if (observer) {
                change.event =
                    cui_command_events_private_metadata(observer, CUI_COMPONENT_EVENT_CHANGE);
                change.has_previous = cui_command_selected(s, &change.previous);
                change.value = index;
            }
            s->selected = i;
            s->preserve_no_selection = false;
            s->pending_scroll = scroll;
            if (o.on_select)
                o.on_select(index, o.user);
            cui_command_events_private_notify_selection(observer, &change);
            return true;
        }
    return false;
}
void cui_command_clear_selection(cui_command_state *s) {
    if (s) {
        s->selected = SIZE_MAX;
        s->preserve_no_selection = true;
        s->pending_scroll = false;
    }
}
bool cui_command_events_private_move(cui_command_state *s, bool forward, cui_command_options o,
                                     cui_command_events_private_observer *observer) {
    if (!s || !s->row_count)
        return false;
    size_t i = s->selected < s->row_count ? s->selected : forward ? s->row_count - 1 : 0;
    for (size_t k = 0; k < s->row_count; ++k) {
        i = forward ? (i + 1) % s->row_count : (i + s->row_count - 1) % s->row_count;
        if (s->rows[i].item && !s->rows[i].item->disabled)
            return cui_command_events_private_select(s, s->rows[i].index, true, o, observer);
    }
    return false;
}
bool cui_command_events_private_confirm(cui_command_state *s, cui_command_options o,
                                        cui_command_events_private_observer *observer) {
    if (!s || o.disabled || s->selected >= s->row_count)
        return false;
    cui_command_row row = s->rows[s->selected];
    if (!row.item || row.item->disabled)
        return false;
    void (*confirm)(cui_command_index, void *) = o.on_confirm;
    void *user = o.user;
    cui_command_activation activation = {0};
    if (observer) {
        activation.event =
            cui_command_events_private_metadata(observer, CUI_COMPONENT_EVENT_ACTIVATE);
        activation.value = row.index;
    }
    if (row.item->action)
        row.item->action(row.item->action_user);
    if (confirm)
        confirm(row.index, user);
    if (observer && !observer->suppress && observer->handler.on_activate) {
        cui_command_events_private_count(&observer->result.activation_count,
                                         &observer->result.count_overflow);
        observer->handler.on_activate(observer->handler.user, &activation);
    }
    return true;
}

bool cui_command_rebuild(cui_command_state *s, const cui_command_entry *entries, size_t n,
                         cui_str query, bool changed, cui_command_options options) {
    return cui_command_events_private_rebuild(s, entries, n, query, changed, options, NULL);
}
bool cui_command_select(cui_command_state *s, cui_command_index index, bool scroll,
                        cui_command_options options) {
    return cui_command_events_private_select(s, index, scroll, options, NULL);
}
bool cui_command_move(cui_command_state *s, bool forward, cui_command_options options) {
    return cui_command_events_private_move(s, forward, options, NULL);
}
bool cui_command_confirm(cui_command_state *s, cui_command_options options) {
    return cui_command_events_private_confirm(s, options, NULL);
}
cui_command_event_result cui_command_rebuild_events(cui_command_state *s, cui_id target,
                                                    const cui_command_entry *entries, size_t n,
                                                    cui_str query, bool changed,
                                                    cui_command_options options,
                                                    cui_command_event_handler handler) {
    cui_command_events_private_observer observer = {.target = target, .handler = handler};
    if (target)
        observer.result.control =
            cui_command_events_private_rebuild(s, entries, n, query, changed, options, &observer);
    return observer.result;
}
cui_command_event_result cui_command_select_events(cui_command_state *s, cui_id target,
                                                   cui_command_index index, bool scroll,
                                                   cui_command_options options,
                                                   cui_command_event_handler handler) {
    cui_command_events_private_observer observer = {.target = target, .handler = handler};
    if (target)
        observer.result.control =
            cui_command_events_private_select(s, index, scroll, options, &observer);
    return observer.result;
}
cui_command_event_result cui_command_move_events(cui_command_state *s, cui_id target, bool forward,
                                                 cui_command_options options,
                                                 cui_command_event_handler handler) {
    cui_command_events_private_observer observer = {.target = target, .handler = handler};
    if (target)
        observer.result.control = cui_command_events_private_move(s, forward, options, &observer);
    return observer.result;
}
cui_command_event_result cui_command_confirm_events(cui_command_state *s, cui_id target,
                                                    cui_command_options options,
                                                    cui_command_event_handler handler) {
    cui_command_events_private_observer observer = {.target = target, .handler = handler};
    if (target)
        observer.result.control = cui_command_events_private_confirm(s, options, &observer);
    return observer.result;
}
