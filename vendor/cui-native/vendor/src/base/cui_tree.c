/* SPDX-License-Identifier: MIT */
#include "cui/cui_tree_events.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    size_t *parent, *end, *visible, *position, *indices, *ordinal, *children;
    size_t roots;
    cui_id *keys;
} cui_tree_bank;
struct cui_tree_model {
    size_t capacity, slots, count, visible_count, bytes;
    unsigned active;
    cui_tree_bank banks[2];
    const cui_tree_node *nodes;
    bool *expanded;
};
static uint64_t cui_tree_hash(uint64_t x) {
    x ^= x >> 30;
    x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27;
    x *= UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}
cui_tree_model *cui_tree_create(size_t capacity) {
    if (!capacity || capacity > 100000)
        return NULL;
    cui_tree_model *m = calloc(1, sizeof(*m));
    if (!m)
        return NULL;
    m->capacity = capacity;
    m->slots = 1;
    while (m->slots < capacity * 2)
        m->slots *= 2;
    m->bytes = sizeof(*m) +
               2 * (capacity * 6 * sizeof(size_t) + m->slots * (sizeof(size_t) + sizeof(cui_id)));
    for (unsigned i = 0; i < 2; ++i) {
        cui_tree_bank *b = &m->banks[i];
        b->parent = malloc(capacity * sizeof(size_t));
        b->end = malloc(capacity * sizeof(size_t));
        b->visible = malloc(capacity * sizeof(size_t));
        b->position = malloc(capacity * sizeof(size_t));
        b->ordinal = malloc(capacity * sizeof(size_t));
        b->children = malloc(capacity * sizeof(size_t));
        b->indices = malloc(m->slots * sizeof(size_t));
        b->keys = calloc(m->slots, sizeof(cui_id));
        if (!b->parent || !b->end || !b->visible || !b->position || !b->indices || !b->keys ||
            !b->ordinal || !b->children) {
            cui_tree_destroy(m);
            return NULL;
        }
    }
    return m;
}
void cui_tree_destroy(cui_tree_model *m) {
    if (!m)
        return;
    for (unsigned i = 0; i < 2; ++i) {
        free(m->banks[i].parent);
        free(m->banks[i].end);
        free(m->banks[i].visible);
        free(m->banks[i].position);
        free(m->banks[i].indices);
        free(m->banks[i].keys);
        free(m->banks[i].ordinal);
        free(m->banks[i].children);
    }
    free(m);
}
cui_tree_error cui_tree_rebuild(cui_tree_model *m, const cui_tree_node *nodes, bool *expanded,
                                size_t count) {
    if (!m || ((!nodes || !expanded) && count))
        return CUI_TREE_INVALID;
    if (count > m->capacity)
        return CUI_TREE_CAPACITY;
    cui_tree_bank *b = &m->banks[1 - m->active];
    memset(b->keys, 0, m->slots * sizeof(cui_id));
    memset(b->children, 0, count * sizeof(size_t));
    b->roots = 0;
    size_t stack[64], depth = 0, visible_count = 0;
    bool shown[64] = {0};
    for (size_t i = 0; i < count; ++i) {
        if (!nodes[i].key || (!nodes[i].label.data && nodes[i].label.length) ||
            nodes[i].depth > 63 || (!i && nodes[i].depth) ||
            (i && nodes[i].depth > nodes[i - 1].depth + 1))
            return CUI_TREE_INVALID;
        size_t slot = (size_t)cui_tree_hash(nodes[i].key) & (m->slots - 1);
        while (b->keys[slot] && b->keys[slot] != nodes[i].key)
            slot = (slot + 1) & (m->slots - 1);
        if (b->keys[slot])
            return CUI_TREE_DUPLICATE_KEY;
        b->keys[slot] = nodes[i].key;
        b->indices[slot] = i;
        while (depth > nodes[i].depth)
            b->end[stack[--depth]] = i;
        b->parent[i] = nodes[i].depth ? stack[nodes[i].depth - 1] : SIZE_MAX;
        b->ordinal[i] = b->parent[i] == SIZE_MAX ? ++b->roots : ++b->children[b->parent[i]];
        bool visible = !nodes[i].depth || (shown[nodes[i].depth - 1] && expanded[b->parent[i]]);
        shown[nodes[i].depth] = visible;
        stack[nodes[i].depth] = i;
        depth = nodes[i].depth + 1;
        b->position[i] = SIZE_MAX;
        if (visible) {
            b->position[i] = visible_count;
            b->visible[visible_count++] = i;
        }
    }
    while (depth)
        b->end[stack[--depth]] = count;
    m->active = 1 - m->active;
    m->count = count;
    m->visible_count = visible_count;
    m->nodes = nodes;
    m->expanded = expanded;
    return CUI_TREE_OK;
}
size_t cui_tree_count(const cui_tree_model *m) {
    return m ? m->count : 0;
}
size_t cui_tree_visible_count(const cui_tree_model *m) {
    return m ? m->visible_count : 0;
}
size_t cui_tree_memory_bytes(const cui_tree_model *m) {
    return m ? m->bytes : 0;
}
bool cui_tree_get_node(const cui_tree_model *m, size_t index, cui_tree_node *node) {
    if (!m || !node || index >= m->count)
        return false;
    *node = m->nodes[index];
    return true;
}
bool cui_tree_expanded(const cui_tree_model *m, size_t index) {
    return m && index < m->count && m->expanded[index];
}
bool cui_tree_visible_node(const cui_tree_model *m, size_t visible, size_t *index) {
    if (!m || !index || visible >= m->visible_count)
        return false;
    *index = m->banks[m->active].visible[visible];
    return true;
}
bool cui_tree_node_index(const cui_tree_model *m, cui_id key, size_t *index) {
    if (!m || !key || !index)
        return false;
    const cui_tree_bank *b = &m->banks[m->active];
    size_t slot = (size_t)cui_tree_hash(key) & (m->slots - 1);
    while (b->keys[slot]) {
        if (b->keys[slot] == key) {
            *index = b->indices[slot];
            return true;
        }
        slot = (slot + 1) & (m->slots - 1);
    }
    return false;
}
bool cui_tree_parent(const cui_tree_model *m, size_t index, size_t *parent) {
    if (!m || !parent || index >= m->count || m->banks[m->active].parent[index] == SIZE_MAX)
        return false;
    *parent = m->banks[m->active].parent[index];
    return true;
}
bool cui_tree_is_branch(const cui_tree_model *m, size_t index) {
    return m && index < m->count && m->banks[m->active].end[index] > index + 1;
}
bool cui_tree_set_expanded(cui_tree_model *m, cui_id key, bool expanded) {
    size_t index;
    if (!cui_tree_node_index(m, key, &index) || !cui_tree_is_branch(m, index) ||
        m->expanded[index] == expanded)
        return false;
    m->expanded[index] = expanded;
    return cui_tree_rebuild(m, m->nodes, m->expanded, m->count) == CUI_TREE_OK;
}
cui_tree_view cui_tree_viewport(const cui_tree_model *m, float scroll, float height, float row,
                                size_t overscan) {
    cui_tree_view view = {0};
    if (!m)
        return view;
    if (!isfinite(row) || row <= 0)
        row = 32;
    if (row > 4096)
        row = 4096;
    if (!isfinite(height) || height < 0)
        height = 0;
    view.total = (float)m->visible_count * row;
    if (!isfinite(view.total))
        view.total = 0;
    view.scroll = !isfinite(scroll) ? 0 : fmaxf(0, fminf(scroll, fmaxf(0, view.total - height)));
    size_t first = (size_t)(view.scroll / row), last = first;
    if (row > 0) {
        double raw = ceil(((double)view.scroll + height) / row);
        last = raw > (double)m->visible_count ? m->visible_count : (size_t)raw;
    }
    view.begin = first > overscan ? first - overscan : 0;
    view.end = overscan > m->visible_count - last ? m->visible_count : last + overscan;
    view.before = (float)view.begin * row;
    view.after = (float)(m->visible_count - view.end) * row;
    return view;
}
cui_id cui_tree_row_id(cui_id root, cui_id key) {
    return root ^ cui_tree_hash(key + UINT64_C(0x3977b83c42e1d193));
}
cui_id cui_tree_toggle_id(cui_id root, cui_id key) {
    return cui_tree_row_id(root, key) ^ UINT64_C(0xb092ae86e31fc8f9);
}
static uint64_t cui_tree_unhash(uint64_t x) {
    x ^= (x >> 31) ^ (x >> 62);
    x *= UINT64_C(0x319642b2d24d8ec3);
    x ^= (x >> 27) ^ (x >> 54);
    x *= UINT64_C(0x96de1b173f119089);
    return x ^ (x >> 30) ^ (x >> 60);
}
static bool cui_tree_hit(const cui_tree_model *m, cui_id root, cui_id target, size_t *index) {
    cui_id key = cui_tree_unhash(target ^ root) - UINT64_C(0x3977b83c42e1d193);
    if (cui_tree_node_index(m, key, index) && m->banks[m->active].position[*index] != SIZE_MAX)
        return true;
    key = cui_tree_unhash(target ^ root ^ UINT64_C(0xb092ae86e31fc8f9)) -
          UINT64_C(0x3977b83c42e1d193);
    return cui_tree_node_index(m, key, index) && m->banks[m->active].position[*index] != SIZE_MAX;
}
static unsigned char cui_tree_lower(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + 'a' - 'A') : c;
}
static bool cui_tree_prefix(cui_str text, const char *prefix, size_t length) {
    if (!text.data || text.length < length)
        return false;
    for (size_t i = 0; i < length; ++i)
        if (cui_tree_lower((unsigned char)text.data[i]) != cui_tree_lower((unsigned char)prefix[i]))
            return false;
    return true;
}
static size_t cui_tree_enabled_position(const cui_tree_model *m, size_t position, int direction) {
    const cui_tree_bank *b = &m->banks[m->active];
    for (size_t n = 0; n < m->visible_count; ++n) {
        if (direction < 0) {
            if (!position)
                return SIZE_MAX;
            --position;
        } else {
            if (position + 1 >= m->visible_count)
                return SIZE_MAX;
            ++position;
        }
        if (!m->nodes[b->visible[position]].disabled)
            return position;
    }
    return SIZE_MAX;
}
static bool cui_tree_event_evidence(cui_context *ui, cui_id id, size_t index, cui_event_type type,
                                    cui_component_event_metadata *out) {
    if (!cui_frame_paint_enabled(ui) || !cui_frame_sequence(ui))
        return false;
    cui_activation_snapshot snapshot;
    if (cui_event_activation_snapshot(ui, index, &snapshot) &&
        cui_component_event_from_activation(out, CUI_COMPONENT_EVENT_CHANGE, &snapshot)) {
        out->target = id;
        return true;
    }
    cui_component_event_source source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN;
    switch (type) {
    case CUI_EVENT_POINTER_UP:
        source = CUI_COMPONENT_EVENT_SOURCE_POINTER;
        break;
    case CUI_EVENT_KEY_DOWN:
    case CUI_EVENT_KEY_UP:
        source = CUI_COMPONENT_EVENT_SOURCE_KEYBOARD;
        break;
    case CUI_EVENT_ACCESSIBILITY_ACTION:
        source = CUI_COMPONENT_EVENT_SOURCE_ACCESSIBILITY;
        break;
    default:
        break;
    }
    return cui_component_event_init(out, CUI_COMPONENT_EVENT_CHANGE, id, source) &&
           cui_component_event_set_frame_index(out, cui_frame_sequence(ui), index);
}
static void cui_tree_selection_notify(cui_tree_event_result *out, cui_tree_handler handler,
                                      cui_component_event_metadata evidence, cui_id previous,
                                      cui_id value, size_t index) {
    if (!handler.on_selection_change || previous == value)
        return;
    cui_tree_selection_change change = {evidence, previous, value, index};
    handler.on_selection_change(handler.user, &change);
    if (out->selection_count == SIZE_MAX)
        out->count_overflow = true;
    else
        ++out->selection_count;
}
static bool cui_tree_expand_observed(cui_tree_model *model, size_t index, bool expanded,
                                     cui_tree_handler handler, cui_tree_event_result *out,
                                     const cui_component_event_metadata *evidence) {
    cui_id key = model->nodes[index].key;
    bool previous = model->expanded[index];
    bool changed = cui_tree_set_expanded(model, key, expanded);
    if (evidence && handler.on_expansion_change && previous != model->expanded[index]) {
        cui_tree_expansion_change event = {*evidence, key, index, previous, model->expanded[index]};
        handler.on_expansion_change(handler.user, &event);
        if (out->expansion_count == SIZE_MAX)
            out->count_overflow = true;
        else
            ++out->expansion_count;
    }
    return changed;
}
static void cui_tree_activation_notify(cui_tree_event_result *out, cui_tree_handler handler,
                                       cui_component_event_metadata evidence, cui_id key,
                                       size_t index) {
    if (!handler.on_activate)
        return;
    evidence.kind = CUI_COMPONENT_EVENT_ACTIVATE;
    cui_tree_activation event = {evidence, key, index};
    handler.on_activate(handler.user, &event);
    if (out->activation_count == SIZE_MAX)
        out->count_overflow = true;
    else
        ++out->activation_count;
}
static cui_tree_result cui_tree_reduce_common(cui_context *ui, cui_id id, cui_tree_model *m,
                                              cui_tree_state *s, cui_id *selected,
                                              cui_tree_options o, cui_tree_handler handler,
                                              cui_tree_event_result *out) {
    cui_tree_result result = {0};
    if (!ui || !m || !s || !selected) {
        result.error = CUI_TREE_INVALID;
        return result;
    }
    cui_id before = *selected;
    if (s->search_length >= sizeof(s->search))
        s->search_length = 0;
    if (!isfinite(s->search_time) || s->search_time < 0)
        s->search_time = 0;
    if (isfinite(o.dt) && o.dt > 0)
        s->search_time = o.dt > .75f ? .76f : s->search_time + o.dt;
    if (s->search_time > .75f)
        s->search_length = 0;
    size_t cursor;
    if (!cui_tree_node_index(m, s->cursor, &cursor) ||
        m->banks[m->active].position[cursor] == SIZE_MAX || m->nodes[cursor].disabled) {
        bool found = false;
        if (cui_tree_node_index(m, *selected, &cursor) &&
            m->banks[m->active].position[cursor] != SIZE_MAX && !m->nodes[cursor].disabled)
            found = true;
        if (!found)
            for (size_t p = 0; p < m->visible_count; ++p)
                if (!m->nodes[m->banks[m->active].visible[p]].disabled) {
                    cursor = m->banks[m->active].visible[p];
                    found = true;
                    break;
                }
        s->cursor = found ? m->nodes[cursor].key : 0;
    }
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    cui_id source = 0;
    bool reveal = false;
    for (size_t e = 0; e < count && !o.disabled; ++e) {
        if (cui_event_consumed(ui, e))
            continue;
        cui_id activation = cui_event_activation(ui, e), target = cui_event_target(ui, e);
        size_t hit = SIZE_MAX;
        if (!cui_tree_hit(m, id, target, &hit))
            continue;
        if (hit == SIZE_MAX || m->nodes[hit].disabled)
            continue;
        cui_component_event_metadata evidence;
        bool observe =
            out &&
            (handler.on_selection_change || handler.on_expansion_change || handler.on_activate) &&
            cui_tree_event_evidence(ui, id, e, events[e].type, &evidence);
        if (activation == cui_tree_toggle_id(id, m->nodes[hit].key)) {
            if (cui_tree_expand_observed(m, hit, !m->expanded[hit], handler, out,
                                         observe ? &evidence : NULL))
                result.expansion_changed = true;
            s->cursor = m->nodes[hit].key;
            cui_focus(ui, cui_tree_row_id(id, s->cursor));
            continue;
        }
        if (activation == cui_tree_row_id(id, m->nodes[hit].key)) {
            cui_id previous = *selected;
            *selected = m->nodes[hit].key;
            if (observe)
                cui_tree_selection_notify(out, handler, evidence, previous, *selected, hit);
            s->cursor = *selected;
            result.activated = *selected;
            if (observe)
                cui_tree_activation_notify(out, handler, evidence, *selected, hit);
        }
        if (source != target && (source || s->cursor != m->nodes[hit].key))
            s->cursor = m->nodes[hit].key;
        source = target;
        if (!cui_tree_node_index(m, s->cursor, &cursor))
            continue;
        size_t position = m->banks[m->active].position[cursor], next = SIZE_MAX;
        bool was_expanded = m->expanded[cursor];
        if (events[e].type == CUI_EVENT_KEY_DOWN) {
            cui_key key = events[e].key;
            if (key == CUI_KEY_UP || key == CUI_KEY_DOWN)
                next = cui_tree_enabled_position(m, position, key == CUI_KEY_UP ? -1 : 1);
            else if (key == CUI_KEY_HOME || key == CUI_KEY_END) {
                for (size_t p = 0; p < m->visible_count; ++p) {
                    size_t candidate = key == CUI_KEY_HOME ? p : m->visible_count - 1 - p;
                    if (!m->nodes[m->banks[m->active].visible[candidate]].disabled) {
                        next = candidate;
                        break;
                    }
                }
            } else if (key == CUI_KEY_LEFT) {
                if (cui_tree_is_branch(m, cursor) && m->expanded[cursor])
                    result.expansion_changed |= cui_tree_expand_observed(
                        m, cursor, false, handler, out, observe ? &evidence : NULL);
                else {
                    size_t parent = cursor;
                    while (cui_tree_parent(m, parent, &parent))
                        if (!m->nodes[parent].disabled) {
                            next = m->banks[m->active].position[parent];
                            break;
                        }
                }
            } else if (key == CUI_KEY_RIGHT && cui_tree_is_branch(m, cursor)) {
                if (!m->expanded[cursor])
                    result.expansion_changed |= cui_tree_expand_observed(
                        m, cursor, true, handler, out, observe ? &evidence : NULL);
                else {
                    for (size_t candidate = position + 1; candidate < m->visible_count;
                         ++candidate) {
                        size_t child = m->banks[m->active].visible[candidate];
                        if (m->nodes[child].depth <= m->nodes[cursor].depth)
                            break;
                        if (m->banks[m->active].parent[child] == cursor &&
                            !m->nodes[child].disabled) {
                            next = candidate;
                            break;
                        }
                    }
                }
            }
        } else if (events[e].type == CUI_EVENT_TEXT) {
            bool repeat = s->search_length == 1 && events[e].text[0] && !events[e].text[1] &&
                          cui_tree_lower((unsigned char)s->search[0]) ==
                              cui_tree_lower((unsigned char)events[e].text[0]);
            for (size_t i = 0; i < sizeof(events[e].text) && events[e].text[i]; ++i) {
                unsigned char c = (unsigned char)events[e].text[i];
                if (c < 32 || c >= 127)
                    continue;
                if (!repeat && s->search_length + 1 < sizeof(s->search))
                    s->search[s->search_length++] = (char)c;
            }
            s->search[s->search_length] = 0;
            s->search_time = 0;
            if (s->search_length)
                for (size_t p = 1; p <= m->visible_count; ++p) {
                    size_t candidate = (position + p) % m->visible_count,
                           index = m->banks[m->active].visible[candidate];
                    if (!m->nodes[index].disabled &&
                        cui_tree_prefix(m->nodes[index].label, s->search, s->search_length)) {
                        next = candidate;
                        break;
                    }
                }
        }
        if (events[e].type == CUI_EVENT_KEY_DOWN &&
            (events[e].key == CUI_KEY_LEFT || events[e].key == CUI_KEY_RIGHT) &&
            (m->expanded[cursor] != was_expanded || (next != SIZE_MAX && next != position)))
            cui_consume_event(ui, e);
        if (next != SIZE_MAX && next < m->visible_count) {
            reveal = true;
            cursor = m->banks[m->active].visible[next];
            s->cursor = m->nodes[cursor].key;
            if (!o.manual_selection) {
                cui_id previous = *selected;
                *selected = s->cursor;
                if (observe)
                    cui_tree_selection_notify(out, handler, evidence, previous, *selected, cursor);
            }
            cui_focus(ui, cui_tree_row_id(id, s->cursor));
        }
    }
    float row = isfinite(o.row_height) && o.row_height > 0 ? fminf(o.row_height, 4096) : 32,
          height = isfinite(o.height) && o.height > 0 ? o.height : 240;
    if (cui_tree_node_index(m, s->cursor, &cursor)) {
        size_t position = m->banks[m->active].position[cursor];
        float top = (float)position * row;
        if (reveal && position != SIZE_MAX && cui_focused(ui) == cui_tree_row_id(id, s->cursor)) {
            if (top < s->scroll)
                s->scroll = top;
            else if (top + row > s->scroll + height)
                s->scroll = top + row - height;
        }
    }
    for (size_t e = 0; e < count && !o.disabled; ++e)
        if (events[e].type == CUI_EVENT_SCROLL) {
            bool inside = cui_event_target(ui, e) == id;
            if (inside && isfinite(events[e].dy))
                s->scroll -= events[e].dy * (events[e].scroll_precise ? 1 : row);
        }
    cui_tree_view view = cui_tree_viewport(m, s->scroll, height, row, o.overscan);
    s->scroll = view.scroll;
    result.selection_changed = *selected != before;
    if (result.selection_changed || result.expansion_changed || result.activated)
        cui_invalidate(ui);
    return result;
}

cui_tree_result cui_tree_reduce(cui_context *ui, cui_id id, cui_tree_model *m, cui_tree_state *s,
                                cui_id *selected, cui_tree_options options) {
    return cui_tree_reduce_common(ui, id, m, s, selected, options, (cui_tree_handler){0}, NULL);
}
cui_tree_event_result cui_tree_reduce_events(cui_context *ui, cui_id id, cui_tree_model *m,
                                             cui_tree_state *s, cui_id *selected,
                                             cui_tree_options options, cui_tree_handler handler) {
    cui_tree_event_result out = {0};
    if (!ui || !id || !m || !s || !selected) {
        out.result.error = CUI_TREE_INVALID;
        return out;
    }
    out.result = cui_tree_reduce_common(ui, id, m, s, selected, options, handler, &out);
    return out;
}

void cui_tree_submit(cui_context *ui, cui_id id, cui_tree_model *m, cui_tree_state *s,
                     cui_id selected, cui_tree_options o) {
    if (!ui || !m || !s)
        return;
    float row = isfinite(o.row_height) && o.row_height > 0 ? fminf(o.row_height, 4096) : 32;
    float height = isfinite(o.height) && o.height > 0 ? o.height : 240;
    cui_tree_view view = cui_tree_viewport(m, s->scroll, height, row, o.overscan);
    cui_interact(ui, id, CUI_ROLE_TREE, cui_string("Tree"), o.disabled, false,
                 (float)m->visible_count);
    cui_set_tab_stop(ui, id, false);
    cui_set_semantic_details(ui, id,
                             (cui_semantic_details){.scrollable = true,
                                                    .scroll_y = s->scroll,
                                                    .scroll_max_y = fmaxf(0, view.total - height)});
    for (size_t p = view.begin; p < view.end; ++p) {
        size_t i = m->banks[m->active].visible[p], parent = m->banks[m->active].parent[i];
        cui_id key = m->nodes[i].key, row_id = cui_tree_row_id(id, key);
        cui_interact(ui, row_id, CUI_ROLE_TREEITEM, m->nodes[i].label,
                     o.disabled || m->nodes[i].disabled, selected == key, (float)m->nodes[i].depth);
        cui_set_semantic_details(
            ui, row_id,
            (cui_semantic_details){
                .selected = selected == key,
                .level = m->nodes[i].depth + 1,
                .pos_in_set = (unsigned)m->banks[m->active].ordinal[i],
                .set_size = (unsigned)(parent == SIZE_MAX ? m->banks[m->active].roots
                                                          : m->banks[m->active].children[parent]),
                .expanded = cui_tree_is_branch(m, i) && m->expanded[i],
                .parent = parent == SIZE_MAX ? id : cui_tree_row_id(id, m->nodes[parent].key)});
        cui_set_tab_stop(ui, row_id, key == s->cursor);
        if (cui_tree_is_branch(m, i)) {
            cui_id toggle = cui_tree_toggle_id(id, key);
            cui_interact(ui, toggle, CUI_ROLE_BUTTON,
                         m->expanded[i] ? cui_string("Collapse") : cui_string("Expand"),
                         o.disabled || m->nodes[i].disabled, false, 0);
            cui_set_semantic_details(
                ui, toggle, (cui_semantic_details){.parent = row_id, .expanded = m->expanded[i]});
            cui_set_tab_stop(ui, toggle, false);
        }
    }
}
cui_tree_result cui_base_tree(cui_context *ui, cui_id id, cui_tree_model *m, cui_tree_state *s,
                              cui_id *selected, cui_tree_options o) {
    cui_tree_result result = cui_tree_reduce(ui, id, m, s, selected, o);
    if (selected)
        cui_tree_submit(ui, id, m, s, *selected, o);
    return result;
}

cui_tree_event_result cui_base_tree_events(cui_context *ui, cui_id id, cui_tree_model *m,
                                           cui_tree_state *s, cui_id *selected,
                                           cui_tree_options options, cui_tree_handler handler) {
    cui_tree_event_result out = cui_tree_reduce_events(ui, id, m, s, selected, options, handler);
    if (ui && id && m && s && selected)
        cui_tree_submit(ui, id, m, s, *selected, options);
    return out;
}
