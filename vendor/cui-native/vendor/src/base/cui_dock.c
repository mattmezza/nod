/* SPDX-License-Identifier: MIT */
#include "cui/cui_dock.h"
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CUI_DOCK_PRIVATE_NONE SIZE_MAX
#define CUI_DOCK_PRIVATE_MAX_DEPTH 64
#define CUI_DOCK_PRIVATE_MAX_DIMENSION 1000000000.0
typedef struct {
    bool used;
    uint32_t generation;
    cui_dock_kind kind;
    size_t parent, a, b, first, last, active, count;
    double ratio, minw, minh;
    bool canonical, has_size;
    size_t sibling_prev, sibling_next, child_count;
    double size;
} cui_dock_private_node;
typedef struct {
    bool used;
    uint64_t key;
    size_t node, prev, next;
} cui_dock_private_panel;
typedef struct {
    cui_dock_private_node *nodes;
    cui_dock_private_panel *panels;
    size_t root;
    bool root_split;
} cui_dock_private_state;
struct cui_dock {
    cui_dock_config config;
    cui_dock_private_state state, scratch;
    uint32_t generation;
    size_t memory_bytes;
    bool nary_edits;
};
static size_t cui_dock_private_node_index(const cui_dock *d, cui_dock_node id) {
    size_t n = (size_t)(uint32_t)id;
    if (!n || n > d->config.node_capacity)
        return SIZE_MAX;
    n--;
    return d->state.nodes[n].used && d->state.nodes[n].generation == (uint32_t)(id >> 32)
               ? n
               : SIZE_MAX;
}
static cui_dock_node cui_dock_private_id(const cui_dock_private_state *s, size_t n) {
    return n == SIZE_MAX ? 0 : ((uint64_t)s->nodes[n].generation << 32) | (uint64_t)(n + 1);
}
static size_t cui_dock_private_find_panel(const cui_dock *d, const cui_dock_private_state *s,
                                          uint64_t key) {
    for (size_t i = 0; i < d->config.panel_capacity; i++)
        if (s->panels[i].used && s->panels[i].key == key)
            return i;
    return SIZE_MAX;
}
static size_t cui_dock_private_free_node(const cui_dock *d, const cui_dock_private_state *s) {
    for (size_t i = 0; i < d->config.node_capacity; i++)
        if (!s->nodes[i].used)
            return i;
    return SIZE_MAX;
}
static size_t cui_dock_private_free_panel(const cui_dock *d, const cui_dock_private_state *s) {
    for (size_t i = 0; i < d->config.panel_capacity; i++)
        if (!s->panels[i].used)
            return i;
    return SIZE_MAX;
}
static void cui_dock_private_init_node(cui_dock_private_state *s, size_t n, uint32_t gen,
                                       size_t parent) {
    s->nodes[n] = (cui_dock_private_node){.used = true,
                                          .generation = gen,
                                          .kind = CUI_DOCK_TABS,
                                          .parent = parent,
                                          .a = SIZE_MAX,
                                          .b = SIZE_MAX,
                                          .first = SIZE_MAX,
                                          .last = SIZE_MAX,
                                          .active = SIZE_MAX,
                                          .ratio = 0.5,
                                          .sibling_prev = SIZE_MAX,
                                          .sibling_next = SIZE_MAX};
}
static size_t cui_dock_private_child_count(const cui_dock_private_node *n) {
    return n->kind == CUI_DOCK_TABS ? 0 : n->canonical ? n->child_count : 2;
}
static size_t cui_dock_private_next_child(const cui_dock_private_state *s, size_t parent,
                                          size_t child) {
    const cui_dock_private_node *n = &s->nodes[parent];
    return n->canonical ? s->nodes[child].sibling_next : child == n->a ? n->b : SIZE_MAX;
}
static void cui_dock_private_replace_child(cui_dock_private_state *s, size_t parent, size_t old,
                                           size_t replacement) {
    if (parent == SIZE_MAX) {
        s->root = replacement;
        s->nodes[replacement].parent = SIZE_MAX;
        return;
    }
    cui_dock_private_node *p = &s->nodes[parent], *r = &s->nodes[replacement], *o = &s->nodes[old];
    r->parent = parent;
    if (!p->canonical) {
        if (p->a == old)
            p->a = replacement;
        else
            p->b = replacement;
        return;
    }
    r->sibling_prev = o->sibling_prev;
    r->sibling_next = o->sibling_next;
    r->has_size = o->has_size;
    r->size = o->size;
    if (o->sibling_prev != SIZE_MAX)
        s->nodes[o->sibling_prev].sibling_next = replacement;
    else
        p->a = replacement;
    if (o->sibling_next != SIZE_MAX)
        s->nodes[o->sibling_next].sibling_prev = replacement;
    else
        p->b = replacement;
}
static void cui_dock_private_insert_child(cui_dock_private_state *s, size_t parent, size_t child,
                                          size_t before) {
    cui_dock_private_node *p = &s->nodes[parent], *c = &s->nodes[child];
    size_t previous = before == SIZE_MAX ? p->b : s->nodes[before].sibling_prev;
    c->parent = parent;
    c->sibling_prev = previous;
    c->sibling_next = before;
    if (previous == SIZE_MAX)
        p->a = child;
    else
        s->nodes[previous].sibling_next = child;
    if (before == SIZE_MAX)
        p->b = child;
    else
        s->nodes[before].sibling_prev = child;
    p->child_count++;
}
static void cui_dock_private_remove_child(cui_dock_private_state *s, size_t parent, size_t child) {
    cui_dock_private_node *p = &s->nodes[parent], *c = &s->nodes[child];
    if (c->sibling_prev == SIZE_MAX)
        p->a = c->sibling_next;
    else
        s->nodes[c->sibling_prev].sibling_next = c->sibling_next;
    if (c->sibling_next == SIZE_MAX)
        p->b = c->sibling_prev;
    else
        s->nodes[c->sibling_next].sibling_prev = c->sibling_prev;
    p->child_count--;
    c->sibling_prev = c->sibling_next = SIZE_MAX;
}
static bool cui_dock_private_dimension(double x) {
    return isfinite(x) && x >= 0 && x <= CUI_DOCK_PRIVATE_MAX_DIMENSION;
}
static bool cui_dock_private_ratio(double x) {
    return isfinite(x) && x > 0 && x < 1;
}
cui_dock *cui_dock_create(cui_dock_config c) {
    if (!c.node_capacity)
        c.node_capacity = 128;
    if (!c.panel_capacity)
        c.panel_capacity = 256;
    if (!c.max_depth)
        c.max_depth = 32;
    if (!c.max_serialized_bytes)
        c.max_serialized_bytes = 65536;
    if (c.node_capacity > CUI_DOCK_NODE_LIMIT || c.panel_capacity > CUI_DOCK_PANEL_LIMIT ||
        c.max_serialized_bytes > CUI_DOCK_SERIALIZED_LIMIT ||
        c.max_depth > CUI_DOCK_PRIVATE_MAX_DEPTH || c.node_capacity > UINT32_MAX ||
        c.node_capacity > (size_t)PTRDIFF_MAX / (2 * sizeof(cui_dock_private_node)) ||
        c.panel_capacity > (size_t)PTRDIFF_MAX / (2 * sizeof(cui_dock_private_panel)))
        return NULL;
    size_t nb = c.node_capacity * 2 * sizeof(cui_dock_private_node),
           pb = c.panel_capacity * 2 * sizeof(cui_dock_private_panel);
    if (nb > SIZE_MAX - sizeof(cui_dock) || pb > SIZE_MAX - sizeof(cui_dock) - nb)
        return NULL;
    cui_dock *d = calloc(1, sizeof(*d));
    if (!d)
        return NULL;
    d->config = c;
    d->memory_bytes = sizeof(*d) + nb + pb;
    d->state.nodes = calloc(c.node_capacity, sizeof(*d->state.nodes));
    d->scratch.nodes = calloc(c.node_capacity, sizeof(*d->scratch.nodes));
    d->state.panels = calloc(c.panel_capacity, sizeof(*d->state.panels));
    d->scratch.panels = calloc(c.panel_capacity, sizeof(*d->scratch.panels));
    if (!d->state.nodes || !d->scratch.nodes || !d->state.panels || !d->scratch.panels) {
        cui_dock_destroy(d);
        return NULL;
    }
    d->generation = 1;
    cui_dock_private_init_node(&d->state, 0, 1, SIZE_MAX);
    d->state.root = 0;
    return d;
}
cui_dock *cui_dock_create_nary(cui_dock_config config) {
    cui_dock *d = cui_dock_create(config);
    if (d)
        d->nary_edits = true;
    return d;
}
cui_dock *cui_dock_create_with_root(cui_dock_config config, cui_dock_root_kind kind) {
    if (kind != CUI_DOCK_ROOT_ANY && kind != CUI_DOCK_ROOT_SPLIT)
        return NULL;
    cui_dock *d = cui_dock_create_nary(config);
    if (d) {
        d->state.root_split = kind == CUI_DOCK_ROOT_SPLIT;
        d->state.nodes[d->state.root].kind = CUI_DOCK_SPLIT_HORIZONTAL;
        d->state.nodes[d->state.root].canonical = true;
    }
    return d;
}
cui_dock_root_kind cui_dock_root_policy(const cui_dock *d) {
    return d && d->state.root_split ? CUI_DOCK_ROOT_SPLIT : CUI_DOCK_ROOT_ANY;
}
void cui_dock_destroy(cui_dock *d) {
    if (d) {
        free(d->state.nodes);
        free(d->state.panels);
        free(d->scratch.nodes);
        free(d->scratch.panels);
        free(d);
    }
}
cui_dock_node cui_dock_root(const cui_dock *d) {
    return d ? cui_dock_private_id(&d->state, d->state.root) : 0;
}
size_t cui_dock_memory_bytes(const cui_dock *d) {
    return d ? d->memory_bytes : 0;
}
bool cui_dock_node_get(const cui_dock *d, cui_dock_node id, cui_dock_node_info *out) {
    if (!d || !out)
        return false;
    size_t i = cui_dock_private_node_index(d, id);
    if (i == SIZE_MAX)
        return false;
    const cui_dock_private_node *n = &d->state.nodes[i];
    *out = (cui_dock_node_info){
        id,
        cui_dock_private_id(&d->state, n->parent),
        cui_dock_private_id(&d->state, n->a),
        cui_dock_private_id(
            &d->state, n->canonical && n->a != SIZE_MAX ? d->state.nodes[n->a].sibling_next : n->b),
        n->kind,
        n->ratio,
        n->minw,
        n->minh,
        n->count,
        n->active != SIZE_MAX,
        n->active != SIZE_MAX ? d->state.panels[n->active].key : 0,
        cui_dock_private_child_count(n),
        n->canonical};
    return true;
}
bool cui_dock_panel_at(const cui_dock *d, cui_dock_node id, size_t index, uint64_t *key) {
    if (!d || !key)
        return false;
    size_t n = cui_dock_private_node_index(d, id);
    if (n == SIZE_MAX || d->state.nodes[n].kind != CUI_DOCK_TABS ||
        index >= d->state.nodes[n].count)
        return false;
    size_t p = d->state.nodes[n].first;
    while (index--)
        p = d->state.panels[p].next;
    *key = d->state.panels[p].key;
    return true;
}
bool cui_dock_panel_find(const cui_dock *d, uint64_t key, cui_dock_node *tab, size_t *index) {
    if (!d)
        return false;
    size_t p = cui_dock_private_find_panel(d, &d->state, key);
    if (p == SIZE_MAX)
        return false;
    size_t n = d->state.panels[p].node;
    if (tab)
        *tab = cui_dock_private_id(&d->state, n);
    if (index) {
        *index = 0;
        for (size_t x = d->state.nodes[n].first; x != p; x = d->state.panels[x].next)
            (*index)++;
    }
    return true;
}
static void cui_dock_private_attach(cui_dock_private_state *s, size_t p, size_t n, size_t index) {
    cui_dock_private_node *t = &s->nodes[n];
    size_t after = t->first;
    while (index-- && after != SIZE_MAX)
        after = s->panels[after].next;
    size_t before = after == SIZE_MAX ? t->last : s->panels[after].prev;
    s->panels[p].node = n;
    s->panels[p].prev = before;
    s->panels[p].next = after;
    if (before != SIZE_MAX)
        s->panels[before].next = p;
    else
        t->first = p;
    if (after != SIZE_MAX)
        s->panels[after].prev = p;
    else
        t->last = p;
    if (!t->count)
        t->active = p;
    t->count++;
}
static void cui_dock_private_detach(cui_dock_private_state *s, size_t p) {
    cui_dock_private_panel *x = &s->panels[p];
    cui_dock_private_node *t = &s->nodes[x->node];
    if (x->prev != SIZE_MAX)
        s->panels[x->prev].next = x->next;
    else
        t->first = x->next;
    if (x->next != SIZE_MAX)
        s->panels[x->next].prev = x->prev;
    else
        t->last = x->prev;
    if (t->active == p)
        t->active = x->next != SIZE_MAX ? x->next : x->prev;
    t->count--;
}
static void cui_dock_private_collapse(cui_dock_private_state *s, size_t n) {
    cui_dock_private_node *t = &s->nodes[n];
    if (t->count || n == s->root)
        return;
    size_t parent = t->parent;
    if (s->nodes[parent].canonical) {
        cui_dock_private_remove_child(s, parent, n);
        t->used = false;
        if (s->nodes[parent].child_count == 1 && !(parent == s->root && s->root_split)) {
            size_t only = s->nodes[parent].a;
            s->nodes[only].minw = fmax(s->nodes[only].minw, s->nodes[parent].minw);
            s->nodes[only].minh = fmax(s->nodes[only].minh, s->nodes[parent].minh);
            cui_dock_private_replace_child(s, s->nodes[parent].parent, parent, only);
            s->nodes[parent].used = false;
        }
        return;
    }
    size_t sibling = s->nodes[parent].a == n ? s->nodes[parent].b : s->nodes[parent].a;
    if (parent == s->root && s->root_split) {
        s->nodes[parent].canonical = true;
        s->nodes[parent].a = s->nodes[parent].b = SIZE_MAX;
        s->nodes[parent].child_count = 0;
        s->nodes[sibling].sibling_prev = s->nodes[sibling].sibling_next = SIZE_MAX;
        cui_dock_private_insert_child(s, parent, sibling, SIZE_MAX);
        t->used = false;
        return;
    }
    cui_dock_private_replace_child(s, s->nodes[parent].parent, parent, sibling);
    t->used = false;
    s->nodes[parent].used = false;
}
cui_dock_result cui_dock_insert(cui_dock *d, cui_dock_node id, uint64_t key, size_t index) {
    if (!d)
        return CUI_DOCK_INVALID;
    size_t n = cui_dock_private_node_index(d, id);
    if (n == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    if (d->state.nodes[n].kind != CUI_DOCK_TABS) {
        cui_dock_private_node *root = &d->state.nodes[n];
        if (n != d->state.root || !root->canonical || root->child_count > 1)
            return CUI_DOCK_INVALID;
        if (root->child_count == 1)
            return cui_dock_insert(d, cui_dock_private_id(&d->state, root->a), key, index);
        if (index != 0 && index != SIZE_MAX)
            return CUI_DOCK_INVALID;
        if (cui_dock_private_find_panel(d, &d->state, key) != SIZE_MAX)
            return CUI_DOCK_DUPLICATE_PANEL;
        size_t child = cui_dock_private_free_node(d, &d->state);
        size_t panel = cui_dock_private_free_panel(d, &d->state);
        if (child == SIZE_MAX || panel == SIZE_MAX || d->config.max_depth < 2 ||
            d->generation == UINT32_MAX)
            return CUI_DOCK_CAPACITY;
        cui_dock_private_init_node(&d->state, child, ++d->generation, n);
        cui_dock_private_insert_child(&d->state, n, child, SIZE_MAX);
        d->state.panels[panel] = (cui_dock_private_panel){.used = true, .key = key};
        cui_dock_private_attach(&d->state, panel, child, 0);
        if (!d->state.root_split) {
            cui_dock_private_replace_child(&d->state, SIZE_MAX, n, child);
            root->used = false;
        }
        return CUI_DOCK_OK;
    }
    if (index == SIZE_MAX)
        index = d->state.nodes[n].count;
    if (index > d->state.nodes[n].count)
        return CUI_DOCK_INVALID;
    if (cui_dock_private_find_panel(d, &d->state, key) != SIZE_MAX)
        return CUI_DOCK_DUPLICATE_PANEL;
    size_t p = cui_dock_private_free_panel(d, &d->state);
    if (p == SIZE_MAX)
        return CUI_DOCK_CAPACITY;
    d->state.panels[p] = (cui_dock_private_panel){.used = true, .key = key};
    cui_dock_private_attach(&d->state, p, n, index);
    return CUI_DOCK_OK;
}
static void cui_dock_private_normalize(cui_dock_private_state *s, size_t node) {
    cui_dock_private_node *n = &s->nodes[node];
    if (n->kind == CUI_DOCK_TABS)
        return;
    for (size_t child = n->a; child != SIZE_MAX;
         child = cui_dock_private_next_child(s, node, child))
        cui_dock_private_normalize(s, child);
    if (!n->canonical)
        return;
    size_t child = n->a;
    while (child != SIZE_MAX) {
        cui_dock_private_node *c = &s->nodes[child];
        size_t next = c->sibling_next;
        if (c->canonical && c->kind == n->kind && !c->minw && !c->minh) {
            bool all_known = true;
            double total = 0;
            for (size_t inner = c->a; inner != SIZE_MAX; inner = s->nodes[inner].sibling_next) {
                all_known &= s->nodes[inner].has_size;
                total += s->nodes[inner].size;
            }
            size_t inner = c->a;
            while (inner != SIZE_MAX) {
                size_t inner_next = s->nodes[inner].sibling_next;
                if (c->has_size && all_known && total > 0)
                    s->nodes[inner].size *= c->size / total;
                cui_dock_private_insert_child(s, node, inner, child);
                inner = inner_next;
            }
            cui_dock_private_remove_child(s, node, child);
            c->used = false;
        }
        child = next;
    }
}
cui_dock_result cui_dock_split(cui_dock *d, cui_dock_node id, cui_dock_kind axis, double ratio,
                               uint64_t key, bool before, cui_dock_node *out) {
    if (!d || !cui_dock_private_ratio(ratio) ||
        (axis != CUI_DOCK_SPLIT_HORIZONTAL && axis != CUI_DOCK_SPLIT_VERTICAL))
        return CUI_DOCK_INVALID;
    size_t n = cui_dock_private_node_index(d, id);
    if (n == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    if (d->state.nodes[n].kind != CUI_DOCK_TABS || !d->state.nodes[n].count)
        return CUI_DOCK_INVALID;
    if (cui_dock_private_find_panel(d, &d->state, key) != SIZE_MAX)
        return CUI_DOCK_DUPLICATE_PANEL;
    size_t existing_parent = d->state.nodes[n].parent;
    if (d->nary_edits && existing_parent != SIZE_MAX && d->state.nodes[existing_parent].canonical &&
        d->state.nodes[existing_parent].kind == axis) {
        size_t child = cui_dock_private_free_node(d, &d->state),
               panel = cui_dock_private_free_panel(d, &d->state);
        if (child == SIZE_MAX || panel == SIZE_MAX || d->generation == UINT32_MAX)
            return CUI_DOCK_CAPACITY;
        cui_dock_private_init_node(&d->state, child, ++d->generation, existing_parent);
        double old_size = d->state.nodes[n].size;
        d->state.nodes[child].has_size = d->state.nodes[n].has_size;
        d->state.nodes[child].size = old_size * (before ? ratio : 1 - ratio);
        d->state.nodes[n].size = old_size * (before ? 1 - ratio : ratio);
        size_t before_node = before ? n : d->state.nodes[n].sibling_next;
        cui_dock_private_insert_child(&d->state, existing_parent, child, before_node);
        d->state.panels[panel] = (cui_dock_private_panel){.used = true, .key = key};
        cui_dock_private_attach(&d->state, panel, child, 0);
        cui_dock_private_normalize(&d->state, d->state.root);
        if (out)
            *out = cui_dock_private_id(&d->state, child);
        return CUI_DOCK_OK;
    }
    size_t depth = 1;
    for (size_t p = d->state.nodes[n].parent; p != SIZE_MAX; p = d->state.nodes[p].parent)
        depth++;
    if (depth >= d->config.max_depth || d->generation > UINT32_MAX - 2)
        return CUI_DOCK_CAPACITY;
    size_t a = cui_dock_private_free_node(d, &d->state), b = SIZE_MAX,
           p = cui_dock_private_free_panel(d, &d->state);
    if (a != SIZE_MAX)
        for (size_t i = a + 1; i < d->config.node_capacity; i++)
            if (!d->state.nodes[i].used) {
                b = i;
                break;
            }
    if (a == SIZE_MAX || b == SIZE_MAX || p == SIZE_MAX)
        return CUI_DOCK_CAPACITY;
    size_t parent = d->state.nodes[n].parent;
    cui_dock_private_init_node(&d->state, a, ++d->generation, parent);
    cui_dock_private_init_node(&d->state, b, ++d->generation, a);
    d->state.nodes[a].kind = axis;
    d->state.nodes[a].ratio = ratio;
    cui_dock_private_replace_child(&d->state, parent, n, a);
    d->state.nodes[a].canonical = d->nary_edits;
    if (d->nary_edits) {
        d->state.nodes[a].a = d->state.nodes[a].b = SIZE_MAX;
        cui_dock_private_insert_child(&d->state, a, before ? b : n, SIZE_MAX);
        cui_dock_private_insert_child(&d->state, a, before ? n : b, SIZE_MAX);
        d->state.nodes[n].has_size = d->state.nodes[b].has_size = true;
        d->state.nodes[n].size = before ? 1 - ratio : ratio;
        d->state.nodes[b].size = before ? ratio : 1 - ratio;
    } else {
        d->state.nodes[a].a = before ? b : n;
        d->state.nodes[a].b = before ? n : b;
        d->state.nodes[n].parent = a;
    }
    d->state.panels[p] = (cui_dock_private_panel){.used = true, .key = key};
    cui_dock_private_attach(&d->state, p, b, 0);
    if (d->nary_edits)
        cui_dock_private_normalize(&d->state, d->state.root);
    if (out)
        *out = cui_dock_private_id(&d->state, b);
    return CUI_DOCK_OK;
}
cui_dock_result cui_dock_move(cui_dock *d, uint64_t key, cui_dock_node id, size_t index) {
    if (!d)
        return CUI_DOCK_INVALID;
    size_t n = cui_dock_private_node_index(d, id),
           p = cui_dock_private_find_panel(d, &d->state, key);
    if (n == SIZE_MAX || p == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    if (d->state.nodes[n].kind != CUI_DOCK_TABS)
        return CUI_DOCK_INVALID;
    size_t old = d->state.panels[p].node, newcount = d->state.nodes[n].count - (old == n ? 1 : 0);
    if (index == SIZE_MAX)
        index = newcount;
    if (index > newcount)
        return CUI_DOCK_INVALID;
    bool was_active = d->state.nodes[old].active == p;
    cui_dock_private_detach(&d->state, p);
    cui_dock_private_attach(&d->state, p, n, index);
    if (old != n || was_active)
        d->state.nodes[n].active = p;
    if (old != n)
        cui_dock_private_collapse(&d->state, old);
    if (d->nary_edits)
        cui_dock_private_normalize(&d->state, d->state.root);
    return CUI_DOCK_OK;
}
cui_dock_result cui_dock_move_split(cui_dock *d, uint64_t key, cui_dock_node destination,
                                    cui_dock_kind axis, double ratio, bool before,
                                    cui_dock_node *out) {
    if (!d)
        return CUI_DOCK_INVALID;
    cui_dock_node source;
    cui_dock_node_info source_info;
    if (!cui_dock_panel_find(d, key, &source, NULL) || !cui_dock_node_get(d, source, &source_info))
        return CUI_DOCK_NOT_FOUND;
    cui_dock trial = *d;
    trial.state = d->scratch;
    trial.state.root = d->state.root;
    trial.state.root_split = d->state.root_split;
    memcpy(trial.state.nodes, d->state.nodes, d->config.node_capacity * sizeof(*d->state.nodes));
    memcpy(trial.state.panels, d->state.panels,
           d->config.panel_capacity * sizeof(*d->state.panels));
    cui_dock_result result = cui_dock_remove(&trial, key);
    cui_dock_node node;
    if (result == CUI_DOCK_OK)
        result = cui_dock_split(&trial, destination, axis, ratio, key, before, &node);
    if (result != CUI_DOCK_OK)
        return result;
    cui_dock_set_minimum(&trial, node, source_info.min_width, source_info.min_height);
    cui_dock_private_state old = d->state;
    d->state = trial.state;
    d->scratch = old;
    d->generation = trial.generation;
    if (out)
        *out = node;
    return CUI_DOCK_OK;
}
static cui_dock cui_dock_private_stage(cui_dock *model) {
    cui_dock trial = *model;
    trial.state = model->scratch;
    trial.state.root = model->state.root;
    trial.state.root_split = model->state.root_split;
    memcpy(trial.state.nodes, model->state.nodes,
           model->config.node_capacity * sizeof(*model->state.nodes));
    memcpy(trial.state.panels, model->state.panels,
           model->config.panel_capacity * sizeof(*model->state.panels));
    return trial;
}
static void cui_dock_private_commit(cui_dock *model, cui_dock *trial) {
    cui_dock_private_state old = model->state;
    model->state = trial->state;
    model->scratch = old;
    model->generation = trial->generation;
}
cui_dock_result cui_dock_transfer(cui_dock *source, uint64_t key, cui_dock *destination,
                                  cui_dock_node target, size_t index, cui_dock_kind axis,
                                  double ratio, bool before, cui_dock_node *out) {
    if (!source || !destination || source == destination ||
        (axis != CUI_DOCK_TABS && axis != CUI_DOCK_SPLIT_HORIZONTAL &&
         axis != CUI_DOCK_SPLIT_VERTICAL))
        return CUI_DOCK_INVALID;
    cui_dock_node source_node;
    cui_dock_node_info info;
    if (!cui_dock_panel_find(source, key, &source_node, NULL) ||
        !cui_dock_node_get(source, source_node, &info))
        return CUI_DOCK_NOT_FOUND;
    cui_dock a = cui_dock_private_stage(source), b = cui_dock_private_stage(destination);
    cui_dock_node leaf = target;
    cui_dock_result result = cui_dock_remove(&a, key);
    if (result == CUI_DOCK_OK) {
        if (axis == CUI_DOCK_TABS) {
            result = cui_dock_insert(&b, target, key, index);
            if (result == CUI_DOCK_OK) {
                cui_dock_activate(&b, key);
                cui_dock_panel_find(&b, key, &leaf, NULL);
            }
        } else {
            result = cui_dock_split(&b, target, axis, ratio, key, before, &leaf);
            if (result == CUI_DOCK_OK)
                cui_dock_set_minimum(&b, leaf, info.min_width, info.min_height);
        }
    }
    if (result != CUI_DOCK_OK)
        return result;
    cui_dock_private_commit(source, &a);
    cui_dock_private_commit(destination, &b);
    if (out)
        *out = leaf;
    return CUI_DOCK_OK;
}
cui_dock_result cui_dock_remove_many(cui_dock *model, const uint64_t *keys, size_t count) {
    if (!model || (count && !keys) || count > model->config.panel_capacity)
        return CUI_DOCK_INVALID;
    cui_dock trial = cui_dock_private_stage(model);
    for (size_t i = 0; i < count; ++i) {
        cui_dock_result result = cui_dock_remove(&trial, keys[i]);
        if (result != CUI_DOCK_OK)
            return result;
    }
    cui_dock_private_commit(model, &trial);
    return CUI_DOCK_OK;
}
cui_dock_result cui_dock_reorder(cui_dock *d, uint64_t key, size_t index) {
    cui_dock_node n;
    if (!cui_dock_panel_find(d, key, &n, NULL))
        return CUI_DOCK_NOT_FOUND;
    return cui_dock_move(d, key, n, index);
}
cui_dock_result cui_dock_remove(cui_dock *d, uint64_t key) {
    if (!d)
        return CUI_DOCK_INVALID;
    size_t p = cui_dock_private_find_panel(d, &d->state, key);
    if (p == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    size_t n = d->state.panels[p].node;
    cui_dock_private_detach(&d->state, p);
    d->state.panels[p].used = false;
    cui_dock_private_collapse(&d->state, n);
    if (d->nary_edits)
        cui_dock_private_normalize(&d->state, d->state.root);
    return CUI_DOCK_OK;
}
cui_dock_result cui_dock_activate(cui_dock *d, uint64_t key) {
    if (!d)
        return CUI_DOCK_INVALID;
    size_t p = cui_dock_private_find_panel(d, &d->state, key);
    if (p == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    d->state.nodes[d->state.panels[p].node].active = p;
    return CUI_DOCK_OK;
}
cui_dock_result cui_dock_set_ratio(cui_dock *d, cui_dock_node id, double ratio) {
    if (!d || !cui_dock_private_ratio(ratio))
        return CUI_DOCK_INVALID;
    size_t n = cui_dock_private_node_index(d, id);
    if (n == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    if (d->state.nodes[n].kind == CUI_DOCK_TABS)
        return CUI_DOCK_INVALID;
    if (d->state.nodes[n].canonical) {
        if (d->state.nodes[n].child_count != 2)
            return CUI_DOCK_INVALID;
        size_t a = d->state.nodes[n].a, b = d->state.nodes[n].b;
        double total =
            fmin(CUI_DOCK_PRIVATE_MAX_DIMENSION, d->state.nodes[a].size + d->state.nodes[b].size);
        if (!(total > 0))
            total = 1;
        d->state.nodes[a].has_size = d->state.nodes[b].has_size = true;
        d->state.nodes[a].size = total * ratio;
        d->state.nodes[b].size = total * (1 - ratio);
    }
    d->state.nodes[n].ratio = ratio;
    return CUI_DOCK_OK;
}
cui_dock_result cui_dock_balance(cui_dock *d, cui_dock_node id) {
    if (!d)
        return CUI_DOCK_INVALID;
    size_t n = cui_dock_private_node_index(d, id);
    if (n == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    if (d->state.nodes[n].kind == CUI_DOCK_TABS)
        return CUI_DOCK_INVALID;
    if (!d->state.nodes[n].canonical)
        return cui_dock_set_ratio(d, id, .5);
    for (size_t child = d->state.nodes[n].a; child != SIZE_MAX;
         child = d->state.nodes[child].sibling_next) {
        d->state.nodes[child].has_size = true;
        d->state.nodes[child].size = 1;
    }
    return CUI_DOCK_OK;
}
static size_t cui_dock_private_slot_count(const cui_dock_private_state *s, size_t n,
                                          cui_dock_kind axis) {
    if (s->nodes[n].kind != axis)
        return 1;
    size_t count = 0;
    for (size_t child = s->nodes[n].a; child != SIZE_MAX;
         child = cui_dock_private_next_child(s, n, child))
        count += cui_dock_private_slot_count(s, child, axis);
    return count;
}
static void cui_dock_private_slots(const cui_dock_private_state *s, size_t n, cui_dock_kind axis,
                                   cui_dock_node *out, size_t *i) {
    if (s->nodes[n].kind != axis) {
        out[(*i)++] = cui_dock_private_id(s, n);
        return;
    }
    for (size_t child = s->nodes[n].a; child != SIZE_MAX;
         child = cui_dock_private_next_child(s, n, child))
        cui_dock_private_slots(s, child, axis, out, i);
}
cui_dock_result cui_dock_split_slots(const cui_dock *d, cui_dock_node id, cui_dock_node *out,
                                     size_t capacity, size_t *count) {
    if (!d || !count)
        return CUI_DOCK_INVALID;
    size_t n = cui_dock_private_node_index(d, id);
    if (n == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    cui_dock_kind axis = d->state.nodes[n].kind;
    if (axis == CUI_DOCK_TABS)
        return CUI_DOCK_INVALID;
    size_t needed = cui_dock_private_slot_count(&d->state, n, axis);
    *count = needed;
    if (!out)
        return CUI_DOCK_OK;
    if (capacity < needed)
        return CUI_DOCK_CAPACITY;
    size_t i = 0;
    cui_dock_private_slots(&d->state, n, axis, out, &i);
    return CUI_DOCK_OK;
}
static double cui_dock_private_adopt_sizes(cui_dock_private_state *s, size_t n, cui_dock_kind axis,
                                           const double *sizes, size_t *i, double gap) {
    if (s->nodes[n].kind != axis)
        return sizes[(*i)++];
    if (s->nodes[n].canonical) {
        double total = 0;
        size_t count = 0;
        for (size_t child = s->nodes[n].a; child != SIZE_MAX;
             child = cui_dock_private_next_child(s, n, child)) {
            double span = cui_dock_private_adopt_sizes(s, child, axis, sizes, i, gap);
            s->nodes[child].has_size = true;
            s->nodes[child].size = span;
            total += span;
            count++;
        }
        return total + (count - 1) * gap;
    }
    double a = cui_dock_private_adopt_sizes(s, s->nodes[n].a, axis, sizes, i, gap);
    double b = cui_dock_private_adopt_sizes(s, s->nodes[n].b, axis, sizes, i, gap);
    s->nodes[n].ratio = a / (a + b);
    return a + b + gap;
}
cui_dock_result cui_dock_set_slot_sizes(cui_dock *d, cui_dock_node id, const double *sizes,
                                        size_t count, double gap) {
    if (!d || !sizes || !cui_dock_private_dimension(gap))
        return CUI_DOCK_INVALID;
    size_t n = cui_dock_private_node_index(d, id);
    if (n == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    cui_dock_kind axis = d->state.nodes[n].kind;
    if (axis == CUI_DOCK_TABS || count != cui_dock_private_slot_count(&d->state, n, axis))
        return CUI_DOCK_INVALID;
    for (size_t i = 0; i < count; i++)
        if (!cui_dock_private_dimension(sizes[i]) || sizes[i] <= 0)
            return CUI_DOCK_INVALID;
    cui_dock trial = cui_dock_private_stage(d);
    size_t i = 0;
    cui_dock_private_adopt_sizes(&trial.state, n, axis, sizes, &i, gap);
    /* Extremely disparate finite sizes may round a ratio to one. */
    for (size_t j = 0; j < d->config.node_capacity; j++)
        if (trial.state.nodes[j].used && trial.state.nodes[j].kind != CUI_DOCK_TABS &&
            !cui_dock_private_ratio(trial.state.nodes[j].ratio))
            return CUI_DOCK_INVALID;
    for (size_t j = 0; j < d->config.node_capacity; j++)
        if (trial.state.nodes[j].used && trial.state.nodes[j].has_size &&
            !cui_dock_private_dimension(trial.state.nodes[j].size))
            return CUI_DOCK_INVALID;
    cui_dock_private_commit(d, &trial);
    return CUI_DOCK_OK;
}
cui_dock_result cui_dock_set_minimum(cui_dock *d, cui_dock_node id, double w, double h) {
    if (!d || !cui_dock_private_dimension(w) || !cui_dock_private_dimension(h))
        return CUI_DOCK_INVALID;
    size_t n = cui_dock_private_node_index(d, id);
    if (n == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    d->state.nodes[n].minw = w == 0 ? 0 : w;
    d->state.nodes[n].minh = h == 0 ? 0 : h;
    return CUI_DOCK_OK;
}
bool cui_dock_child_at(const cui_dock *d, cui_dock_node id, size_t index, cui_dock_node *out,
                       cui_dock_slot_size *size) {
    if (!d)
        return false;
    size_t node = cui_dock_private_node_index(d, id);
    if (node == SIZE_MAX || index >= cui_dock_private_child_count(&d->state.nodes[node]))
        return false;
    size_t child = d->state.nodes[node].a;
    while (index--)
        child = cui_dock_private_next_child(&d->state, node, child);
    if (out)
        *out = cui_dock_private_id(&d->state, child);
    if (size)
        *size =
            (cui_dock_slot_size){d->state.nodes[node].canonical && d->state.nodes[child].has_size,
                                 d->state.nodes[node].canonical && d->state.nodes[child].has_size
                                     ? d->state.nodes[child].size
                                     : 0};
    return true;
}
cui_dock_result cui_dock_set_sizes(cui_dock *d, cui_dock_node id, const cui_dock_slot_size *sizes,
                                   size_t count) {
    if (!d || (!sizes && count))
        return CUI_DOCK_INVALID;
    size_t n = cui_dock_private_node_index(d, id);
    if (n == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    if (!d->state.nodes[n].canonical || count != d->state.nodes[n].child_count)
        return CUI_DOCK_INVALID;
    for (size_t i = 0; i < count; i++)
        if (sizes[i].has_size && !cui_dock_private_dimension(sizes[i].size))
            return CUI_DOCK_INVALID;
    size_t child = d->state.nodes[n].a;
    for (size_t i = 0; i < count; i++) {
        d->state.nodes[child].has_size = sizes[i].has_size;
        d->state.nodes[child].size = sizes[i].has_size ? sizes[i].size : 0;
        child = d->state.nodes[child].sibling_next;
    }
    cui_dock_private_normalize(&d->state, d->state.root);
    return CUI_DOCK_OK;
}
typedef struct {
    double w, h;
    size_t tabs;
} cui_dock_private_minimum;
static cui_dock_private_minimum cui_dock_private_min(const cui_dock_private_state *s, size_t i,
                                                     double gap) {
    const cui_dock_private_node *n = &s->nodes[i];
    if (n->kind == CUI_DOCK_TABS) {
        return (cui_dock_private_minimum){n->minw, n->minh, 1};
    }
    cui_dock_private_minimum min = {0};
    size_t count = 0;
    for (size_t child = n->a; child != SIZE_MAX; child = cui_dock_private_next_child(s, i, child)) {
        cui_dock_private_minimum c = cui_dock_private_min(s, child, gap);
        min.w = n->kind == CUI_DOCK_SPLIT_HORIZONTAL ? min.w + c.w : fmax(min.w, c.w);
        min.h = n->kind == CUI_DOCK_SPLIT_VERTICAL ? min.h + c.h : fmax(min.h, c.h);
        min.tabs += c.tabs;
        count++;
    }
    if (count > 1) {
        if (n->kind == CUI_DOCK_SPLIT_HORIZONTAL)
            min.w += (count - 1) * gap;
        else
            min.h += (count - 1) * gap;
    }
    min.w = fmax(min.w, n->minw);
    min.h = fmax(min.h, n->minh);
    return min;
}
static void cui_dock_private_children_geometry(const cui_dock *, size_t, cui_rect, double,
                                               cui_dock_visibility_fn, void *, bool,
                                               cui_dock_child_geometry *);
static cui_dock_split_geometry cui_dock_private_split(const cui_dock_private_state *s, size_t i,
                                                      cui_rect r, double gap) {
    const cui_dock_private_node *n = &s->nodes[i];
    cui_dock_private_minimum a = cui_dock_private_min(s, n->a, gap),
                             b = cui_dock_private_min(s, n->b, gap);
    bool horizontal = n->kind == CUI_DOCK_SPLIT_HORIZONTAL;
    double span = horizontal ? r.w : r.h, g = fmin(gap, span), available = fmax(0, span - g),
           ma = horizontal ? a.w : a.h, mb = horizontal ? b.w : b.h, first = available * n->ratio;
    if (ma + mb <= available)
        first = fmin(fmax(first, ma), available - mb);
    else if (ma + mb > 0)
        first = available * ma / (ma + mb);
    cui_rect ra = r, rb = r;
    if (horizontal) {
        ra.w = (float)first;
        rb.x += (float)(first + g);
        rb.w = (float)(available - first);
    } else {
        ra.h = (float)first;
        rb.y += (float)(first + g);
        rb.h = (float)(available - first);
    }
    return (cui_dock_split_geometry){.first = ra,
                                     .second = rb,
                                     .first_minimum = {(float)a.w, (float)a.h},
                                     .second_minimum = {(float)b.w, (float)b.h},
                                     .fit_minimum = ma + mb <= available};
}
static cui_dock_result cui_dock_private_geometry(const cui_dock *d, size_t i, cui_rect r,
                                                 double gap, cui_dock_geometry *out, size_t *at) {
    const cui_dock_private_state *s = &d->state;
    const cui_dock_private_node *n = &s->nodes[i];
    if (n->kind == CUI_DOCK_TABS) {
        out[(*at)++] =
            (cui_dock_geometry){cui_dock_private_id(s, i), r, n->count, n->active != SIZE_MAX,
                                n->active != SIZE_MAX ? s->panels[n->active].key : 0};
        return CUI_DOCK_OK;
    }
    size_t count = cui_dock_private_child_count(n);
    if (!count)
        return CUI_DOCK_OK;
    cui_dock_child_geometry *children = calloc(count, sizeof(*children));
    if (!children)
        return CUI_DOCK_CAPACITY;
    cui_dock_private_children_geometry(d, i, r, gap, NULL, NULL, false, children);
    size_t child = n->a;
    cui_dock_result result = CUI_DOCK_OK;
    for (size_t j = 0; j < count; j++) {
        result = cui_dock_private_geometry(d, child, children[j].bounds, gap, out, at);
        if (result != CUI_DOCK_OK)
            break;
        child = cui_dock_private_next_child(s, i, child);
    }
    free(children);
    return result;
}
cui_dock_result cui_dock_split_layout(const cui_dock *d, cui_dock_node id, cui_rect r, float gap,
                                      cui_dock_split_geometry *out) {
    if (!d || !out || !isfinite(r.x) || !isfinite(r.y) || !isfinite(r.w) || !isfinite(r.h) ||
        r.w < 0 || r.h < 0 || !isfinite(gap) || gap < 0 || fabs((double)r.x + r.w) > FLT_MAX ||
        fabs((double)r.y + r.h) > FLT_MAX)
        return CUI_DOCK_INVALID;
    size_t node = cui_dock_private_node_index(d, id);
    if (node == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    if (d->state.nodes[node].kind == CUI_DOCK_TABS)
        return CUI_DOCK_INVALID;
    if (d->state.nodes[node].canonical) {
        if (d->state.nodes[node].child_count != 2)
            return CUI_DOCK_INVALID;
        cui_dock_child_geometry children[2];
        cui_dock_private_children_geometry(d, node, r, gap, NULL, NULL, false, children);
        cui_dock_private_minimum min = cui_dock_private_min(&d->state, node, gap);
        *out =
            (cui_dock_split_geometry){children[0].bounds, children[1].bounds, children[0].minimum,
                                      children[1].minimum, r.w >= min.w && r.h >= min.h};
        return CUI_DOCK_OK;
    }
    *out = cui_dock_private_split(&d->state, node, r, gap);
    return CUI_DOCK_OK;
}
cui_dock_result cui_dock_layout(const cui_dock *d, cui_rect r, float gap, cui_dock_geometry *out,
                                size_t capacity, size_t *count, bool *fit) {
    if (!d || !isfinite(r.x) || !isfinite(r.y) || !isfinite(r.w) || !isfinite(r.h) || r.w < 0 ||
        r.h < 0 || !isfinite(gap) || gap < 0 || fabs((double)r.x + r.w) > FLT_MAX ||
        fabs((double)r.y + r.h) > FLT_MAX)
        return CUI_DOCK_INVALID;
    cui_dock_private_minimum min = cui_dock_private_min(&d->state, d->state.root, gap);
    if (count)
        *count = min.tabs;
    if (fit)
        *fit = r.w >= min.w && r.h >= min.h;
    if (!out)
        return CUI_DOCK_OK;
    if (capacity < min.tabs)
        return CUI_DOCK_CAPACITY;
    cui_dock_geometry *staged = calloc(min.tabs, sizeof(*staged));
    if (!staged && min.tabs)
        return CUI_DOCK_CAPACITY;
    size_t at = 0;
    cui_dock_result result = cui_dock_private_geometry(d, d->state.root, r, gap, staged, &at);
    if (result == CUI_DOCK_OK && min.tabs)
        memcpy(out, staged, min.tabs * sizeof(*out));
    free(staged);
    return result;
}
static cui_dock_private_minimum cui_dock_private_visible_min(const cui_dock *d, size_t i,
                                                             double gap,
                                                             cui_dock_visibility_fn visible,
                                                             void *user) {
    const cui_dock_private_state *s = &d->state;
    const cui_dock_private_node *n = &s->nodes[i];
    if (n->kind == CUI_DOCK_TABS) {
        if (!cui_dock_visible_count(d, cui_dock_private_id(s, i), visible, user))
            return (cui_dock_private_minimum){0};
        return (cui_dock_private_minimum){n->minw, n->minh, 1};
    }
    cui_dock_private_minimum min = {0};
    size_t count = 0;
    for (size_t child = n->a; child != SIZE_MAX; child = cui_dock_private_next_child(s, i, child)) {
        cui_dock_private_minimum c = cui_dock_private_visible_min(d, child, gap, visible, user);
        if (!c.tabs)
            continue;
        min.w = n->kind == CUI_DOCK_SPLIT_HORIZONTAL ? min.w + c.w : fmax(min.w, c.w);
        min.h = n->kind == CUI_DOCK_SPLIT_VERTICAL ? min.h + c.h : fmax(min.h, c.h);
        min.tabs += c.tabs;
        count++;
    }
    if (!count)
        return min;
    if (count > 1) {
        if (n->kind == CUI_DOCK_SPLIT_HORIZONTAL)
            min.w += (count - 1) * gap;
        else
            min.h += (count - 1) * gap;
    }
    min.w = fmax(min.w, n->minw);
    min.h = fmax(min.h, n->minh);
    return min;
}
static cui_dock_visible_split_geometry
cui_dock_private_visible_split(const cui_dock *d, size_t i, cui_rect r, double gap,
                               cui_dock_visibility_fn visible, void *user) {
    const cui_dock_private_node *n = &d->state.nodes[i];
    if (n->canonical) {
        cui_dock_child_geometry children[2];
        cui_dock_private_children_geometry(d, i, r, gap, visible, user, true, children);
        cui_dock_private_minimum min = cui_dock_private_visible_min(d, i, gap, visible, user);
        return (cui_dock_visible_split_geometry){
            .geometry = {children[0].bounds, children[1].bounds, children[0].minimum,
                         children[1].minimum, r.w >= min.w && r.h >= min.h},
            .first_visible = children[0].visible,
            .second_visible = children[1].visible};
    }
    cui_dock_private_minimum a = cui_dock_private_visible_min(d, n->a, gap, visible, user),
                             b = cui_dock_private_visible_min(d, n->b, gap, visible, user);
    cui_dock_visible_split_geometry result = {
        .first_visible = a.tabs != 0,
        .second_visible = b.tabs != 0,
        .geometry = {.first_minimum = {(float)a.w, (float)a.h},
                     .second_minimum = {(float)b.w, (float)b.h},
                     .fit_minimum = true}};
    if (!a.tabs || !b.tabs) {
        if (a.tabs)
            result.geometry.first = r;
        if (b.tabs)
            result.geometry.second = r;
        cui_dock_private_minimum min = cui_dock_private_visible_min(d, i, gap, visible, user);
        result.geometry.fit_minimum = r.w >= min.w && r.h >= min.h;
        return result;
    }
    bool horizontal = n->kind == CUI_DOCK_SPLIT_HORIZONTAL;
    double span = horizontal ? r.w : r.h, g = fmin(gap, span), available = fmax(0, span - g),
           ma = horizontal ? a.w : a.h, mb = horizontal ? b.w : b.h, first = available * n->ratio;
    if (ma + mb <= available)
        first = fmin(fmax(first, ma), available - mb);
    else if (ma + mb > 0)
        first = available * ma / (ma + mb);
    cui_rect ra = r, rb = r;
    if (horizontal) {
        ra.w = (float)first;
        rb.x += (float)(first + g);
        rb.w = (float)(available - first);
    } else {
        ra.h = (float)first;
        rb.y += (float)(first + g);
        rb.h = (float)(available - first);
    }
    result.geometry.first = ra;
    result.geometry.second = rb;
    result.geometry.fit_minimum = ma + mb <= available;
    return result;
}
static void cui_dock_private_children_geometry(const cui_dock *d, size_t node, cui_rect r,
                                               double gap, cui_dock_visibility_fn visible,
                                               void *user, bool filtered,
                                               cui_dock_child_geometry *out) {
    const cui_dock_private_node *n = &d->state.nodes[node];
    if (!n->canonical) {
        cui_dock_split_geometry geometry;
        bool a = true, b = true;
        if (filtered) {
            cui_dock_visible_split_geometry value =
                cui_dock_private_visible_split(d, node, r, gap, visible, user);
            geometry = value.geometry;
            a = value.first_visible;
            b = value.second_visible;
        } else
            geometry = cui_dock_private_split(&d->state, node, r, gap);
        out[0] = (cui_dock_child_geometry){cui_dock_private_id(&d->state, n->a), geometry.first,
                                           geometry.first_minimum, a};
        out[1] = (cui_dock_child_geometry){cui_dock_private_id(&d->state, n->b), geometry.second,
                                           geometry.second_minimum, b};
        return;
    }
    bool horizontal = n->kind == CUI_DOCK_SPLIT_HORIZONTAL;
    size_t nv = 0, unknown = 0, index = 0;
    double known = 0, minsum = 0;
    for (size_t child = n->a; child != SIZE_MAX; child = d->state.nodes[child].sibling_next) {
        cui_dock_private_minimum min =
            filtered ? cui_dock_private_visible_min(d, child, gap, visible, user)
                     : cui_dock_private_min(&d->state, child, gap);
        bool present = min.tabs != 0;
        out[index++] = (cui_dock_child_geometry){.node = cui_dock_private_id(&d->state, child),
                                                 .minimum = {(float)min.w, (float)min.h},
                                                 .visible = present};
        if (present) {
            nv++;
            minsum += horizontal ? min.w : min.h;
            if (d->state.nodes[child].has_size)
                known += d->state.nodes[child].size;
            else
                unknown++;
        }
    }
    double extent = horizontal ? r.w : r.h;
    double g = nv > 1 ? fmin(gap, extent / (nv - 1)) : 0;
    double available = fmax(0, extent - (nv ? nv - 1 : 0) * g);
    double desired_total = 0;
    index = 0;
    for (size_t child = n->a; child != SIZE_MAX;
         child = d->state.nodes[child].sibling_next, index++) {
        if (!out[index].visible)
            continue;
        double min = horizontal ? out[index].minimum.width : out[index].minimum.height;
        double desired;
        if (nv == 1)
            desired = available;
        else if (!unknown)
            desired = known > 0 ? available * d->state.nodes[child].size / known : available / nv;
        else if (d->state.nodes[child].has_size)
            desired = known > available ? available * d->state.nodes[child].size / known
                                        : d->state.nodes[child].size;
        else
            desired = fmax(0, available - known) / unknown;
        desired = fmax(desired, min);
        out[index].bounds.w = (float)desired; /* Temporary axis extent. */
        desired_total += desired;
    }
    double position = horizontal ? r.x : r.y;
    for (size_t j = 0; j < n->child_count; j++) {
        if (!out[j].visible)
            continue;
        double min = horizontal ? out[j].minimum.width : out[j].minimum.height;
        double span = out[j].bounds.w;
        if (minsum > available && minsum > 0)
            span = available * min / minsum;
        else if (desired_total > available && desired_total > minsum)
            span = min + fmax(0, span - min) * (available - minsum) / (desired_total - minsum);
        out[j].bounds = r;
        if (horizontal) {
            out[j].bounds.x = (float)position;
            out[j].bounds.w = (float)span;
        } else {
            out[j].bounds.y = (float)position;
            out[j].bounds.h = (float)span;
        }
        position += span + g;
    }
}
static bool cui_dock_private_bounds(cui_rect r, float gap) {
    return isfinite(r.x) && isfinite(r.y) && isfinite(r.w) && isfinite(r.h) && r.w >= 0 &&
           r.h >= 0 && isfinite(gap) && gap >= 0 && fabs((double)r.x + r.w) <= FLT_MAX &&
           fabs((double)r.y + r.h) <= FLT_MAX;
}
cui_dock_result cui_dock_split_layout_visible(const cui_dock *d, cui_dock_node id, cui_rect r,
                                              float gap, cui_dock_visibility_fn visible, void *user,
                                              cui_dock_visible_split_geometry *out) {
    if (!d || !out || !cui_dock_private_bounds(r, gap))
        return CUI_DOCK_INVALID;
    size_t node = cui_dock_private_node_index(d, id);
    if (node == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    if (d->state.nodes[node].kind == CUI_DOCK_TABS)
        return CUI_DOCK_INVALID;
    if (d->state.nodes[node].canonical && d->state.nodes[node].child_count != 2)
        return CUI_DOCK_INVALID;
    *out = cui_dock_private_visible_split(d, node, r, gap, visible, user);
    return CUI_DOCK_OK;
}
static cui_dock_result cui_dock_private_visible_geometry(const cui_dock *d, size_t i, cui_rect r,
                                                         float gap, cui_dock_visibility_fn visible,
                                                         void *user, cui_dock_geometry *out,
                                                         size_t *at) {
    const cui_dock_private_node *n = &d->state.nodes[i];
    if (n->kind == CUI_DOCK_TABS) {
        uint64_t displayed;
        cui_dock_node id = cui_dock_private_id(&d->state, i);
        if (cui_dock_displayed_panel(d, id, visible, user, &displayed))
            out[(*at)++] = (cui_dock_geometry){id, r, cui_dock_visible_count(d, id, visible, user),
                                               true, displayed};
        return CUI_DOCK_OK;
    }
    size_t count = cui_dock_private_child_count(n);
    if (!count)
        return CUI_DOCK_OK;
    cui_dock_child_geometry *children = calloc(count, sizeof(*children));
    if (!children)
        return CUI_DOCK_CAPACITY;
    cui_dock_private_children_geometry(d, i, r, gap, visible, user, true, children);
    cui_dock_result result = CUI_DOCK_OK;
    size_t child = n->a;
    for (size_t j = 0; j < count; j++) {
        if (children[j].visible)
            result = cui_dock_private_visible_geometry(d, child, children[j].bounds, gap, visible,
                                                       user, out, at);
        if (result != CUI_DOCK_OK)
            break;
        child = cui_dock_private_next_child(&d->state, i, child);
    }
    free(children);
    return result;
}
cui_dock_result cui_dock_layout_visible(const cui_dock *d, cui_rect r, float gap,
                                        cui_dock_visibility_fn visible, void *user,
                                        cui_dock_geometry *out, size_t capacity, size_t *count,
                                        bool *fit) {
    if (!d || !cui_dock_private_bounds(r, gap))
        return CUI_DOCK_INVALID;
    cui_dock_private_minimum min =
        cui_dock_private_visible_min(d, d->state.root, gap, visible, user);
    if (count)
        *count = min.tabs;
    if (fit)
        *fit = r.w >= min.w && r.h >= min.h;
    if (!out)
        return CUI_DOCK_OK;
    if (capacity < min.tabs)
        return CUI_DOCK_CAPACITY;
    if (!min.tabs)
        return CUI_DOCK_OK;
    cui_dock_geometry *staged = calloc(min.tabs, sizeof(*staged));
    if (!staged)
        return CUI_DOCK_CAPACITY;
    size_t at = 0;
    cui_dock_result result =
        cui_dock_private_visible_geometry(d, d->state.root, r, gap, visible, user, staged, &at);
    if (result == CUI_DOCK_OK && min.tabs)
        memcpy(out, staged, min.tabs * sizeof(*out));
    free(staged);
    return result;
}
cui_dock_result cui_dock_children_layout(const cui_dock *d, cui_dock_node id, cui_rect r, float gap,
                                         cui_dock_visibility_fn visible, void *user,
                                         cui_dock_child_geometry *out, size_t capacity,
                                         size_t *count, bool *fit) {
    if (!d || !cui_dock_private_bounds(r, gap))
        return CUI_DOCK_INVALID;
    size_t node = cui_dock_private_node_index(d, id);
    if (node == SIZE_MAX)
        return CUI_DOCK_NOT_FOUND;
    size_t needed = cui_dock_private_child_count(&d->state.nodes[node]);
    if (!needed)
        return CUI_DOCK_INVALID;
    cui_dock_private_minimum min = cui_dock_private_visible_min(d, node, gap, visible, user);
    if (count)
        *count = needed;
    if (fit)
        *fit = r.w >= min.w && r.h >= min.h;
    if (!out)
        return CUI_DOCK_OK;
    if (capacity < needed)
        return CUI_DOCK_CAPACITY;
    cui_dock_private_children_geometry(d, node, r, gap, visible, user, true, out);
    return CUI_DOCK_OK;
}
typedef struct {
    char *out;
    size_t used;
    bool failed;
} cui_dock_private_writer;
static void cui_dock_private_write(cui_dock_private_writer *w, const char *text) {
    size_t n = strlen(text);
    if (n > SIZE_MAX - w->used) {
        w->failed = true;
        return;
    }
    if (w->out)
        memcpy(w->out + w->used, text, n);
    w->used += n;
}
static void cui_dock_private_uint(cui_dock_private_writer *w, uint64_t value) {
    char text[32];
    (void)snprintf(text, sizeof(text), "%llu ", (unsigned long long)value);
    cui_dock_private_write(w, text);
}
static void cui_dock_private_number(cui_dock_private_writer *w, double value) {
    char text[96];
    (void)snprintf(text, sizeof(text), "%.17g", value);
    const char *point = localeconv()->decimal_point;
    if (point && point[0] && strcmp(point, ".")) {
        char *p = strstr(text, point);
        if (p) {
            size_t n = strlen(point);
            *p = '.';
            memmove(p + 1, p + n, strlen(p + n) + 1);
        }
    }
    cui_dock_private_write(w, text);
    cui_dock_private_write(w, " ");
}
static void cui_dock_private_write_node(const cui_dock_private_state *s, size_t i,
                                        cui_dock_private_writer *w) {
    const cui_dock_private_node *n = &s->nodes[i];
    cui_dock_private_write(w, n->kind == CUI_DOCK_TABS ? "T "
                              : n->canonical
                                  ? (n->kind == CUI_DOCK_SPLIT_HORIZONTAL ? "X " : "Y ")
                                  : (n->kind == CUI_DOCK_SPLIT_HORIZONTAL ? "H " : "V "));
    if (n->kind != CUI_DOCK_TABS && !n->canonical)
        cui_dock_private_number(w, n->ratio);
    cui_dock_private_number(w, n->minw);
    cui_dock_private_number(w, n->minh);
    if (n->kind == CUI_DOCK_TABS) {
        size_t active = 0;
        for (size_t p = n->first; p != SIZE_MAX && p != n->active; p = s->panels[p].next)
            active++;
        cui_dock_private_uint(w, active);
        cui_dock_private_uint(w, n->count);
        for (size_t p = n->first; p != SIZE_MAX; p = s->panels[p].next)
            cui_dock_private_uint(w, s->panels[p].key);
    } else if (n->canonical) {
        cui_dock_private_uint(w, n->child_count);
        for (size_t child = n->a; child != SIZE_MAX; child = s->nodes[child].sibling_next) {
            cui_dock_private_uint(w, s->nodes[child].has_size);
            if (s->nodes[child].has_size)
                cui_dock_private_number(w, s->nodes[child].size);
            cui_dock_private_write_node(s, child, w);
        }
    } else {
        cui_dock_private_write_node(s, n->a, w);
        cui_dock_private_write_node(s, n->b, w);
    }
}
cui_dock_result cui_dock_serialize(const cui_dock *d, char *output, size_t capacity,
                                   size_t *length) {
    if (!d)
        return CUI_DOCK_INVALID;
    cui_dock_private_writer w = {0};
    cui_dock_private_write(&w, d->nary_edits ? "CUIDOCK 2 " : "CUIDOCK 1 ");
    cui_dock_private_write_node(&d->state, d->state.root, &w);
    cui_dock_private_write(&w, "\n");
    if (length)
        *length = w.used;
    if (w.failed || w.used > d->config.max_serialized_bytes)
        return CUI_DOCK_CAPACITY;
    if (!output)
        return CUI_DOCK_OK;
    if (capacity <= w.used)
        return CUI_DOCK_CAPACITY;
    w = (cui_dock_private_writer){.out = output};
    cui_dock_private_write(&w, d->nary_edits ? "CUIDOCK 2 " : "CUIDOCK 1 ");
    cui_dock_private_write_node(&d->state, d->state.root, &w);
    cui_dock_private_write(&w, "\n");
    output[w.used] = 0;
    return CUI_DOCK_OK;
}
typedef struct {
    cui_dock *dock;
    cui_str text;
    size_t pos;
    uint32_t generation;
    cui_dock_result error;
    uint64_t version;
} cui_dock_private_parser;
static bool cui_dock_private_space(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}
static bool cui_dock_private_token(cui_dock_private_parser *p, char token[64]) {
    while (p->pos < p->text.length && cui_dock_private_space(p->text.data[p->pos]))
        p->pos++;
    size_t start = p->pos;
    while (p->pos < p->text.length && !cui_dock_private_space(p->text.data[p->pos]))
        p->pos++;
    size_t n = p->pos - start;
    if (!n || n >= 64 || memchr(p->text.data + start, 0, n)) {
        p->error = CUI_DOCK_INVALID;
        return false;
    }
    memcpy(token, p->text.data + start, n);
    token[n] = 0;
    return true;
}
static bool cui_dock_private_read_uint(cui_dock_private_parser *p, uint64_t *value) {
    char token[64];
    if (!cui_dock_private_token(p, token))
        return false;
    uint64_t n = 0;
    for (size_t i = 0; token[i]; i++) {
        if (token[i] < '0' || token[i] > '9') {
            p->error = CUI_DOCK_INVALID;
            return false;
        }
        unsigned digit = (unsigned)(token[i] - '0');
        if (n > (UINT64_MAX - digit) / 10) {
            p->error = CUI_DOCK_INVALID;
            return false;
        }
        n = n * 10 + digit;
    }
    *value = n;
    return true;
}
static bool cui_dock_private_read_number(cui_dock_private_parser *p, double *out) {
    char token[64];
    if (!cui_dock_private_token(p, token))
        return false;
    size_t i = 0, digits = 0;
    int exponent = 0;
    while (token[i] >= '0' && token[i] <= '9') {
        i++;
        digits++;
    }
    if (token[i] == '.') {
        i++;
        while (token[i] >= '0' && token[i] <= '9') {
            i++;
            digits++;
        }
    }
    if (!digits || digits > 32) {
        p->error = CUI_DOCK_INVALID;
        return false;
    }
    if (token[i] == 'e' || token[i] == 'E') {
        i++;
        if (token[i] == '+' || token[i] == '-')
            i++;
        size_t start = i;
        while (token[i] >= '0' && token[i] <= '9') {
            if (exponent > 400) {
                p->error = CUI_DOCK_INVALID;
                return false;
            }
            exponent = exponent * 10 + (token[i++] - '0');
        }
        if (i == start || exponent > 400) {
            p->error = CUI_DOCK_INVALID;
            return false;
        }
    }
    if (token[i]) {
        p->error = CUI_DOCK_INVALID;
        return false;
    }
    /* Accumulating decimal digits and multiplying by a rounded power of ten
       can round twice when long double is only double (e.g. macOS ARM).
       Delegate conversion once to strtod after validating the ASCII grammar.
       Adapt only its radix to the current locale, without changing locale. */
    char localized[128];
    const char *dot = strchr(token, '.');
    if (dot) {
        const char *point = localeconv()->decimal_point;
        size_t point_length = 0;
        if (point)
            while (point_length < 32 && point[point_length])
                point_length++;
        if (!point_length || point_length == 32) {
            p->error = CUI_DOCK_INVALID;
            return false;
        }
        size_t prefix = (size_t)(dot - token), suffix = strlen(dot + 1);
        memcpy(localized, token, prefix);
        memcpy(localized + prefix, point, point_length);
        memcpy(localized + prefix + point_length, dot + 1, suffix + 1);
    } else {
        memcpy(localized, token, strlen(token) + 1);
    }
    char *end;
    *out = strtod(localized, &end);
    if (*end) {
        p->error = CUI_DOCK_INVALID;
        return false;
    }
    if (!isfinite(*out)) {
        p->error = CUI_DOCK_INVALID;
        return false;
    }
    return true;
}
static size_t cui_dock_private_read_node(cui_dock_private_parser *p, size_t parent, size_t depth) {
    cui_dock *d = p->dock;
    cui_dock_private_state *s = &d->scratch;
    if (depth > d->config.max_depth || p->generation == UINT32_MAX) {
        p->error = CUI_DOCK_CAPACITY;
        return SIZE_MAX;
    }
    size_t n = cui_dock_private_free_node(d, s);
    if (n == SIZE_MAX) {
        p->error = CUI_DOCK_CAPACITY;
        return SIZE_MAX;
    }
    char kind[64];
    if (!cui_dock_private_token(p, kind))
        return SIZE_MAX;
    bool canonical = !strcmp(kind, "X") || !strcmp(kind, "Y");
    if ((canonical && p->version != 2) ||
        (!canonical && strcmp(kind, "T") && strcmp(kind, "H") && strcmp(kind, "V"))) {
        p->error = CUI_DOCK_INVALID;
        return SIZE_MAX;
    }
    cui_dock_private_init_node(s, n, ++p->generation, parent);
    cui_dock_private_node *node = &s->nodes[n];
    node->canonical = canonical;
    node->kind = kind[0] == 'T'                     ? CUI_DOCK_TABS
                 : kind[0] == 'H' || kind[0] == 'X' ? CUI_DOCK_SPLIT_HORIZONTAL
                                                    : CUI_DOCK_SPLIT_VERTICAL;
    if (node->kind != CUI_DOCK_TABS && !canonical &&
        (!cui_dock_private_read_number(p, &node->ratio) || !cui_dock_private_ratio(node->ratio))) {
        p->error = CUI_DOCK_INVALID;
        return SIZE_MAX;
    }
    if (!cui_dock_private_read_number(p, &node->minw) ||
        !cui_dock_private_read_number(p, &node->minh) || !cui_dock_private_dimension(node->minw) ||
        !cui_dock_private_dimension(node->minh)) {
        p->error = CUI_DOCK_INVALID;
        return SIZE_MAX;
    }
    if (node->kind == CUI_DOCK_TABS) {
        uint64_t active, count;
        if (!cui_dock_private_read_uint(p, &active) || !cui_dock_private_read_uint(p, &count))
            return SIZE_MAX;
        if (count > d->config.panel_capacity) {
            p->error = CUI_DOCK_CAPACITY;
            return SIZE_MAX;
        }
        if ((!count && (active || parent != SIZE_MAX)) || (count && active >= count)) {
            p->error = CUI_DOCK_INVALID;
            return SIZE_MAX;
        }
        for (size_t i = 0; i < (size_t)count; i++) {
            uint64_t key;
            if (!cui_dock_private_read_uint(p, &key))
                return SIZE_MAX;
            if (cui_dock_private_find_panel(d, s, key) != SIZE_MAX) {
                p->error = CUI_DOCK_DUPLICATE_PANEL;
                return SIZE_MAX;
            }
            size_t slot = cui_dock_private_free_panel(d, s);
            if (slot == SIZE_MAX) {
                p->error = CUI_DOCK_CAPACITY;
                return SIZE_MAX;
            }
            s->panels[slot] = (cui_dock_private_panel){.used = true, .key = key};
            cui_dock_private_attach(s, slot, n, i);
            if (i == (size_t)active)
                node->active = slot;
        }
    } else if (canonical) {
        uint64_t count;
        if (!cui_dock_private_read_uint(p, &count))
            return SIZE_MAX;
        if (count < 2 && parent != SIZE_MAX) {
            p->error = CUI_DOCK_INVALID;
            return SIZE_MAX;
        }
        if (count > d->config.node_capacity) {
            p->error = CUI_DOCK_CAPACITY;
            return SIZE_MAX;
        }
        for (size_t i = 0; i < count; i++) {
            uint64_t has_size;
            double size = 0;
            if (!cui_dock_private_read_uint(p, &has_size))
                return SIZE_MAX;
            if (has_size > 1 || (has_size && (!cui_dock_private_read_number(p, &size) ||
                                              !cui_dock_private_dimension(size)))) {
                p->error = CUI_DOCK_INVALID;
                return SIZE_MAX;
            }
            size_t child = cui_dock_private_read_node(p, n, depth + 1);
            if (child == SIZE_MAX)
                return SIZE_MAX;
            s->nodes[child].has_size = has_size != 0;
            s->nodes[child].size = size;
            cui_dock_private_insert_child(s, n, child, SIZE_MAX);
        }
    } else {
        node->a = cui_dock_private_read_node(p, n, depth + 1);
        if (node->a == SIZE_MAX)
            return SIZE_MAX;
        node->b = cui_dock_private_read_node(p, n, depth + 1);
        if (node->b == SIZE_MAX)
            return SIZE_MAX;
    }
    return n;
}
cui_dock_result cui_dock_restore(cui_dock *d, cui_str text) {
    if (!d || !text.data || !text.length)
        return CUI_DOCK_INVALID;
    if (text.length > d->config.max_serialized_bytes)
        return CUI_DOCK_CAPACITY;
    memset(d->scratch.nodes, 0, d->config.node_capacity * sizeof(*d->scratch.nodes));
    memset(d->scratch.panels, 0, d->config.panel_capacity * sizeof(*d->scratch.panels));
    cui_dock_private_parser p = {
        .dock = d, .text = text, .generation = d->generation, .error = CUI_DOCK_OK};
    char magic[64];
    uint64_t version;
    if (!cui_dock_private_token(&p, magic) || strcmp(magic, "CUIDOCK") ||
        !cui_dock_private_read_uint(&p, &version) || (version != 1 && version != 2))
        return CUI_DOCK_INVALID;
    p.version = version;
    size_t root = cui_dock_private_read_node(&p, SIZE_MAX, 1);
    if (root == SIZE_MAX)
        return p.error == CUI_DOCK_OK ? CUI_DOCK_INVALID : p.error;
    while (p.pos < text.length && cui_dock_private_space(text.data[p.pos]))
        p.pos++;
    if (p.pos != text.length)
        return CUI_DOCK_INVALID;
    d->scratch.root = root;
    d->scratch.root_split = d->state.root_split;
    cui_dock_private_node *loaded = &d->scratch.nodes[root];
    if (d->scratch.root_split && loaded->kind == CUI_DOCK_TABS) {
        if (!loaded->count) {
            loaded->kind = CUI_DOCK_SPLIT_HORIZONTAL;
            loaded->canonical = true;
        } else {
            size_t wrapper = cui_dock_private_free_node(d, &d->scratch);
            if (wrapper == SIZE_MAX || p.generation == UINT32_MAX || d->config.max_depth < 2)
                return CUI_DOCK_CAPACITY;
            cui_dock_private_init_node(&d->scratch, wrapper, ++p.generation, SIZE_MAX);
            d->scratch.nodes[wrapper].kind = CUI_DOCK_SPLIT_HORIZONTAL;
            d->scratch.nodes[wrapper].canonical = true;
            cui_dock_private_insert_child(&d->scratch, wrapper, root, SIZE_MAX);
            d->scratch.root = wrapper;
        }
    } else if (!d->scratch.root_split && loaded->canonical && loaded->child_count == 1) {
        size_t child = loaded->a;
        d->scratch.nodes[child].minw = fmax(d->scratch.nodes[child].minw, loaded->minw);
        d->scratch.nodes[child].minh = fmax(d->scratch.nodes[child].minh, loaded->minh);
        cui_dock_private_replace_child(&d->scratch, SIZE_MAX, root, child);
        loaded->used = false;
    }
    cui_dock_private_state old = d->state;
    d->state = d->scratch;
    d->scratch = old;
    d->generation = p.generation;
    d->nary_edits = version == 2 || d->state.root_split;
    return CUI_DOCK_OK;
}

cui_dock_group_constraints cui_dock_group_sealed(void) {
    return (cui_dock_group_constraints){.locked = true, .unclosable = true, .preserve_last = true};
}
cui_dock_group_constraints cui_dock_group_in_split(bool alone) {
    return (cui_dock_group_constraints){.preserve_last = alone};
}

size_t cui_dock_visible_count(const cui_dock *model, cui_dock_node node,
                              cui_dock_visibility_fn visible, void *user) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(model, node, &info) || info.kind != CUI_DOCK_TABS)
        return 0;
    size_t count = 0;
    for (size_t i = 0; i < info.panel_count; ++i) {
        uint64_t key;
        if (cui_dock_panel_at(model, node, i, &key) && (!visible || visible(user, model, key)))
            ++count;
    }
    return count;
}
bool cui_dock_displayed_panel(const cui_dock *model, cui_dock_node node,
                              cui_dock_visibility_fn visible, void *user, uint64_t *panel) {
    cui_dock_node_info info;
    if (!panel || !cui_dock_node_get(model, node, &info) || info.kind != CUI_DOCK_TABS ||
        !info.has_active)
        return false;
    cui_dock_node owner;
    if ((!visible || visible(user, model, info.active_panel)) &&
        cui_dock_panel_find(model, info.active_panel, &owner, NULL) && owner == node) {
        *panel = info.active_panel;
        return true;
    }
    for (size_t i = 0; i < info.panel_count; ++i) {
        uint64_t key;
        if (visible && cui_dock_panel_at(model, node, i, &key) && visible(user, model, key) &&
            cui_dock_panel_find(model, key, &owner, NULL) && owner == node) {
            *panel = key;
            return true;
        }
    }
    return false;
}

/* Typed frozen state conversion: schema text and live panel ownership stay outside base. */
typedef struct {
    cui_dock *model;
    cui_dock_schema_key_fn key;
    void *user;
    size_t visited;
    cui_dock_result error;
} cui_dock_private_schema_reader;
static bool cui_dock_private_schema_valid(const cui_dock_schema_node *state) {
    return state && state->kind >= CUI_DOCK_SCHEMA_STACK && state->kind <= CUI_DOCK_SCHEMA_PANEL &&
           (!state->panel_name.length || state->panel_name.data) &&
           (!state->payload.length || state->payload.data) &&
           (!state->child_count || state->children) && (!state->size_count || state->sizes);
}
static bool cui_dock_private_schema_visit(cui_dock_private_schema_reader *reader,
                                          const cui_dock_schema_node *state, size_t depth) {
    if (!cui_dock_private_schema_valid(state)) {
        reader->error = CUI_DOCK_INVALID;
        return false;
    }
    size_t bound = reader->model->config.node_capacity + reader->model->config.panel_capacity;
    if (depth > CUI_DOCK_PRIVATE_MAX_DEPTH || state->child_count > bound ||
        state->size_count > bound ||
        ++reader->visited >
            reader->model->config.node_capacity + reader->model->config.panel_capacity) {
        reader->error = CUI_DOCK_CAPACITY;
        return false;
    }
    return true;
}
static bool cui_dock_private_schema_empty_tab(const cui_dock_schema_node *state) {
    return state->kind == CUI_DOCK_SCHEMA_PANEL && state->panel_name.length == 8 &&
           !memcmp(state->panel_name.data, "TabPanel", 8);
}
static bool cui_dock_private_schema_panels(cui_dock_private_schema_reader *reader,
                                           const cui_dock_schema_node *state, size_t tab,
                                           size_t depth, bool visited) {
    if (!visited && !cui_dock_private_schema_visit(reader, state, depth))
        return false;
    if (state->kind == CUI_DOCK_SCHEMA_TABS) {
        for (size_t i = 0; i < state->child_count; i++)
            if (!cui_dock_private_schema_panels(reader, &state->children[i], tab, depth + 1, false))
                return false;
        return true;
    }
    if (cui_dock_private_schema_empty_tab(state))
        return true;
    uint64_t key;
    if (!reader->key || !reader->key(reader->user, state, &key)) {
        reader->error = CUI_DOCK_INVALID;
        return false;
    }
    reader->error = cui_dock_insert(reader->model, cui_dock_private_id(&reader->model->state, tab),
                                    key, SIZE_MAX);
    return reader->error == CUI_DOCK_OK;
}
static size_t cui_dock_private_schema_build(cui_dock_private_schema_reader *reader,
                                            const cui_dock_schema_node *state, size_t parent,
                                            size_t depth) {
    cui_dock *model = reader->model;
    if (!cui_dock_private_schema_visit(reader, state, depth))
        return SIZE_MAX;
    size_t node = cui_dock_private_free_node(model, &model->state);
    if (node == SIZE_MAX || model->generation == UINT32_MAX || depth > model->config.max_depth) {
        reader->error = CUI_DOCK_CAPACITY;
        return SIZE_MAX;
    }
    cui_dock_private_init_node(&model->state, node, ++model->generation, parent);
    if (state->kind != CUI_DOCK_SCHEMA_STACK) {
        if (!cui_dock_private_schema_panels(reader, state, node, depth, true))
            return SIZE_MAX;
        size_t active = model->state.nodes[node].count
                            ? (state->active_index < model->state.nodes[node].count
                                   ? state->active_index
                                   : model->state.nodes[node].count - 1)
                            : 0;
        size_t panel = model->state.nodes[node].first;
        while (active--)
            panel = model->state.panels[panel].next;
        model->state.nodes[node].active = panel;
        return node;
    }
    model->state.nodes[node].canonical = true;
    model->state.nodes[node].kind =
        state->axis == 0 ? CUI_DOCK_SPLIT_HORIZONTAL : CUI_DOCK_SPLIT_VERTICAL;
    for (size_t i = 0; i < state->size_count; i++)
        if (!isfinite(state->sizes[i]) || fabs(state->sizes[i]) > CUI_DOCK_PRIVATE_MAX_DIMENSION) {
            reader->error = CUI_DOCK_INVALID;
            return SIZE_MAX;
        }
    for (size_t i = 0; i < state->child_count; i++) {
        size_t child = cui_dock_private_schema_build(reader, &state->children[i], node, depth + 1);
        if (child == SIZE_MAX)
            return SIZE_MAX;
        model->state.nodes[child].has_size = i < state->size_count && state->sizes[i] > 0;
        model->state.nodes[child].size = model->state.nodes[child].has_size ? state->sizes[i] : 0;
        cui_dock_private_insert_child(&model->state, node, child, SIZE_MAX);
    }
    return node;
}
static void cui_dock_private_schema_normalize(cui_dock_private_state *state, size_t node) {
    cui_dock_private_node *container = &state->nodes[node];
    if (container->kind == CUI_DOCK_TABS)
        return;
    size_t child = container->a;
    while (child != SIZE_MAX) {
        size_t next = state->nodes[child].sibling_next;
        cui_dock_private_schema_normalize(state, child);
        cui_dock_private_node *inner = &state->nodes[child];
        bool empty = inner->kind == CUI_DOCK_TABS ? !inner->count : !inner->child_count;
        if (empty) {
            cui_dock_private_remove_child(state, node, child);
            inner->used = false;
        } else if (inner->canonical && inner->child_count == 1) {
            size_t only = inner->a;
            cui_dock_private_replace_child(state, node, child, only);
            inner->used = false;
        }
        child = next;
    }
    cui_dock_private_normalize(state, node);
}
cui_dock_result cui_dock_schema_import(cui_dock *model, const cui_dock_schema_node *input,
                                       cui_dock_schema_key_fn key, void *user) {
    if (!model || !input)
        return CUI_DOCK_INVALID;
    cui_dock trial = cui_dock_private_stage(model);
    memset(trial.state.nodes, 0, model->config.node_capacity * sizeof(*trial.state.nodes));
    memset(trial.state.panels, 0, model->config.panel_capacity * sizeof(*trial.state.panels));
    trial.nary_edits = true;
    cui_dock_private_schema_reader reader = {&trial, key, user, 0, CUI_DOCK_OK};
    size_t root = cui_dock_private_schema_build(&reader, input, SIZE_MAX, 1);
    if (root == SIZE_MAX)
        return reader.error;
    trial.state.root = root;
    if (trial.state.root_split && trial.state.nodes[root].kind == CUI_DOCK_TABS) {
        size_t wrapper = cui_dock_private_free_node(&trial, &trial.state);
        if (wrapper == SIZE_MAX || trial.generation == UINT32_MAX || trial.config.max_depth < 2)
            return CUI_DOCK_CAPACITY;
        cui_dock_private_init_node(&trial.state, wrapper, ++trial.generation, SIZE_MAX);
        trial.state.nodes[wrapper].kind = CUI_DOCK_SPLIT_HORIZONTAL;
        trial.state.nodes[wrapper].canonical = true;
        cui_dock_private_insert_child(&trial.state, wrapper, root, SIZE_MAX);
        trial.state.root = wrapper;
    }
    cui_dock_private_schema_normalize(&trial.state, trial.state.root);
    root = trial.state.root;
    if (!trial.state.root_split && trial.state.nodes[root].canonical &&
        trial.state.nodes[root].child_count == 1) {
        size_t only = trial.state.nodes[root].a;
        cui_dock_private_replace_child(&trial.state, SIZE_MAX, root, only);
        trial.state.nodes[root].used = false;
    }
    cui_dock_private_commit(model, &trial);
    model->nary_edits = true;
    return CUI_DOCK_OK;
}
typedef struct {
    const cui_dock *model;
    cui_dock_schema_dump_fn dump;
    void *user;
    cui_dock_schema_node *nodes;
    bool *owned;
    double *sizes;
    size_t node_count, size_count;
    cui_dock_result error;
} cui_dock_private_schema_writer;
static void cui_dock_private_schema_write(cui_dock_private_schema_writer *writer, cui_dock_node id,
                                          size_t slot) {
    cui_dock_node_info info;
    if (!cui_dock_node_get(writer->model, id, &info)) {
        writer->error = CUI_DOCK_INVALID;
        return;
    }
    cui_dock_schema_node *out = &writer->nodes[slot];
    writer->owned[slot] = true;
    bool tabs = info.kind == CUI_DOCK_TABS;
    size_t count = tabs ? info.panel_count : info.child_count;
    *out = (cui_dock_schema_node){.panel_name = {(tabs ? "TabPanel" : "StackPanel"), tabs ? 8 : 10},
                                  .kind = tabs ? CUI_DOCK_SCHEMA_TABS : CUI_DOCK_SCHEMA_STACK,
                                  .child_count = count,
                                  .axis = info.kind == CUI_DOCK_SPLIT_VERTICAL ? 1 : 0};
    size_t children = writer->node_count;
    writer->node_count += count;
    out->children = count ? &writer->nodes[children] : NULL;
    if (!tabs) {
        out->sizes = count ? &writer->sizes[writer->size_count] : NULL;
        out->size_count = count;
        writer->size_count += count;
    }
    for (size_t i = 0; i < count; i++) {
        if (tabs) {
            uint64_t key;
            if (!cui_dock_panel_at(writer->model, id, i, &key)) {
                writer->error = CUI_DOCK_INVALID;
                return;
            }
            if (info.has_active && key == info.active_panel)
                out->active_index = i;
            if (!writer->dump || !writer->dump(writer->user, key, &writer->nodes[children + i]) ||
                !cui_dock_private_schema_valid(&writer->nodes[children + i])) {
                writer->error = CUI_DOCK_INVALID;
                return;
            }
        } else {
            cui_dock_node child;
            cui_dock_slot_size size;
            if (!cui_dock_child_at(writer->model, id, i, &child, &size)) {
                writer->error = CUI_DOCK_INVALID;
                return;
            }
            writer->sizes[(size_t)(out->sizes - writer->sizes) + i] = size.has_size ? size.size : 0;
            cui_dock_private_schema_write(writer, child, children + i);
            if (writer->error != CUI_DOCK_OK)
                return;
        }
    }
}
cui_dock_result cui_dock_schema_export(const cui_dock *model, cui_dock_schema_dump_fn dump,
                                       void *user, cui_dock_schema_node *nodes, size_t capacity,
                                       double *sizes, size_t size_capacity, size_t *node_count,
                                       size_t *size_count) {
    if (!model || !node_count || !size_count)
        return CUI_DOCK_INVALID;
    size_t bound = model->config.node_capacity + model->config.panel_capacity;
    cui_dock_private_schema_writer writer = {
        .model = model,
        .dump = dump,
        .user = user,
        .nodes = calloc(bound, sizeof(*writer.nodes)),
        .owned = calloc(bound, sizeof(*writer.owned)),
        .sizes = calloc(model->config.node_capacity, sizeof(*writer.sizes)),
        .node_count = 1,
        .error = CUI_DOCK_OK};
    if (!writer.nodes || !writer.owned || !writer.sizes)
        writer.error = CUI_DOCK_CAPACITY;
    else
        cui_dock_private_schema_write(&writer, cui_dock_root(model), 0);
    if (writer.error == CUI_DOCK_OK) {
        *node_count = writer.node_count;
        *size_count = writer.size_count;
        if (nodes && (capacity < writer.node_count || size_capacity < writer.size_count ||
                      (writer.size_count && !sizes)))
            writer.error = CUI_DOCK_CAPACITY;
        else if (nodes) {
            for (size_t i = 0; i < writer.node_count; i++) {
                cui_dock_schema_node value = writer.nodes[i];
                if (writer.owned[i]) {
                    if (value.children)
                        value.children = &nodes[value.children - writer.nodes];
                    if (value.sizes)
                        value.sizes = &sizes[value.sizes - writer.sizes];
                }
                nodes[i] = value;
            }
            if (writer.size_count)
                memcpy(sizes, writer.sizes, writer.size_count * sizeof(*sizes));
        }
    }
    free(writer.nodes);
    free(writer.owned);
    free(writer.sizes);
    return writer.error;
}

cui_dock_schema_area cui_dock_schema_area_default(void) {
    return (cui_dock_schema_area){
        .center = {.kind = CUI_DOCK_SCHEMA_PANEL, .payload = {"null", 4}}};
}
static cui_dock_result cui_dock_private_area_routes(const cui_dock_schema_area *area,
                                                    const cui_dock_schema_node *nodes[4],
                                                    const cui_dock_schema_edge *edges[3]) {
    if (!area)
        return CUI_DOCK_INVALID;
    nodes[0] = &area->center;
    for (size_t i = 0; i < 3; i++) {
        nodes[i + 1] = NULL;
        edges[i] = NULL;
    }
    for (size_t i = 0; i < 3; i++) {
        if (!area->edge_present[i])
            continue;
        const cui_dock_schema_edge *edge = &area->edges[i];
        if (edge->placement < CUI_DOCK_LEFT || edge->placement > CUI_DOCK_BOTTOM ||
            !isfinite(edge->size) || edge->size <= 0 || edge->size > CUI_DOCK_PRIVATE_MAX_DIMENSION)
            return CUI_DOCK_INVALID;
        size_t index = (size_t)edge->placement;
        nodes[index] = &edge->panel;
        edges[index - 1] = edge;
    }
    return CUI_DOCK_OK;
}
static bool cui_dock_private_area_distinct(const cui_dock *const models[4]) {
    if (!models)
        return false;
    for (size_t i = 0; i < 4; i++) {
        if (!models[i])
            return false;
        for (size_t j = 0; j < i; j++)
            if (models[i] == models[j])
                return false;
    }
    return true;
}
static cui_dock_result cui_dock_private_area_keys(const cui_dock *const models[4],
                                                  const bool present[4]) {
    for (size_t i = 0; i < 4; i++) {
        if (!present[i])
            continue;
        for (size_t p = 0; p < models[i]->config.panel_capacity; p++) {
            if (!models[i]->state.panels[p].used)
                continue;
            for (size_t j = 0; j < i; j++)
                if (present[j] &&
                    cui_dock_private_find_panel(models[j], &models[j]->state,
                                                models[i]->state.panels[p].key) != SIZE_MAX)
                    return CUI_DOCK_DUPLICATE_PANEL;
        }
    }
    return CUI_DOCK_OK;
}
cui_dock_result cui_dock_private_schema_area_import_models(cui_dock *const models[4],
                                                           const cui_dock_schema_area *area,
                                                           cui_dock_schema_key_fn key, void *user,
                                                           const size_t node_limits[4],
                                                           const size_t panel_limits[4]) {
    if (!cui_dock_private_area_distinct((const cui_dock *const *)models))
        return CUI_DOCK_INVALID;
    const cui_dock_schema_node *nodes[4];
    const cui_dock_schema_edge *edges[3];
    cui_dock_result result = cui_dock_private_area_routes(area, nodes, edges);
    if (result != CUI_DOCK_OK)
        return result;
    cui_dock *staged[4] = {0};
    bool present[4] = {true, true, true, true};
    for (size_t i = 0; i < 4; i++) {
        staged[i] = cui_dock_create_with_root(models[i]->config,
                                              i ? CUI_DOCK_ROOT_ANY : CUI_DOCK_ROOT_SPLIT);
        if (!staged[i]) {
            result = CUI_DOCK_CAPACITY;
            break;
        }
        staged[i]->generation = models[i]->generation;
        if (nodes[i]) {
            result = cui_dock_schema_import(staged[i], nodes[i], key, user);
            if (result != CUI_DOCK_OK)
                break;
        } else {
            if (staged[i]->generation == UINT32_MAX) {
                result = CUI_DOCK_CAPACITY;
                break;
            }
            staged[i]->state.nodes[staged[i]->state.root].generation = ++staged[i]->generation;
        }
    }
    if (result == CUI_DOCK_OK && node_limits && panel_limits)
        for (size_t i = 0; i < 4; i++) {
            size_t nodes_used = 0, panels_used = 0;
            for (size_t n = 0; n < staged[i]->config.node_capacity; n++)
                nodes_used += staged[i]->state.nodes[n].used;
            for (size_t p = 0; p < staged[i]->config.panel_capacity; p++)
                panels_used += staged[i]->state.panels[p].used;
            if (nodes_used > node_limits[i] || panels_used > panel_limits[i]) {
                result = CUI_DOCK_CAPACITY;
                break;
            }
        }
    if (result == CUI_DOCK_OK)
        result = cui_dock_private_area_keys((const cui_dock *const *)staged, present);
    if (result == CUI_DOCK_OK)
        for (size_t i = 0; i < 4; i++) {
            cui_dock_private_state old = models[i]->state;
            models[i]->state = staged[i]->state;
            staged[i]->state = old;
            models[i]->generation = staged[i]->generation;
            models[i]->nary_edits = true;
        }
    for (size_t i = 0; i < 4; i++)
        cui_dock_destroy(staged[i]);
    return result;
}
cui_dock_result cui_dock_schema_area_import_models(cui_dock *const models[4],
                                                   const cui_dock_schema_area *area,
                                                   cui_dock_schema_key_fn key, void *user) {
    return cui_dock_private_schema_area_import_models(models, area, key, user, NULL, NULL);
}
cui_dock_result cui_dock_schema_area_export_models(const cui_dock *const models[4],
                                                   const cui_dock_schema_area *metadata,
                                                   cui_dock_schema_dump_fn dump, void *user,
                                                   cui_dock_schema_buffer buffers[4],
                                                   cui_dock_schema_area *output) {
    if (!cui_dock_private_area_distinct(models) || !metadata || !buffers || !output)
        return CUI_DOCK_INVALID;
    const cui_dock_schema_node *ignored[4];
    const cui_dock_schema_edge *edges[3];
    cui_dock_result result = cui_dock_private_area_routes(metadata, ignored, edges);
    if (result != CUI_DOCK_OK)
        return result;
    bool present[4] = {true, edges[0] != NULL, edges[1] != NULL, edges[2] != NULL};
    result = cui_dock_private_area_keys(models, present);
    if (result != CUI_DOCK_OK)
        return result;
    cui_dock_schema_buffer staged[4] = {0};
    bool query = buffers[0].nodes == NULL;
    for (size_t i = 0; i < 4 && result == CUI_DOCK_OK; i++) {
        if (!present[i])
            continue;
        if ((buffers[i].nodes == NULL) != query) {
            result = CUI_DOCK_INVALID;
            break;
        }
        staged[i].node_capacity =
            models[i]->config.node_capacity + models[i]->config.panel_capacity;
        staged[i].size_capacity = models[i]->config.node_capacity;
        staged[i].nodes = calloc(staged[i].node_capacity, sizeof(*staged[i].nodes));
        staged[i].sizes = calloc(staged[i].size_capacity, sizeof(*staged[i].sizes));
        if (!staged[i].nodes || !staged[i].sizes) {
            result = CUI_DOCK_CAPACITY;
            break;
        }
        result = cui_dock_schema_export(
            models[i], dump, user, staged[i].nodes, staged[i].node_capacity, staged[i].sizes,
            staged[i].size_capacity, &staged[i].node_count, &staged[i].size_count);
    }
    bool short_output = false;
    if (result == CUI_DOCK_OK) {
        for (size_t i = 0; i < 4; i++) {
            buffers[i].node_count = staged[i].node_count;
            buffers[i].size_count = staged[i].size_count;
            if (present[i] && !query &&
                (buffers[i].node_capacity < staged[i].node_count ||
                 buffers[i].size_capacity < staged[i].size_count ||
                 (staged[i].size_count && !buffers[i].sizes)))
                short_output = true;
        }
        if (short_output)
            result = CUI_DOCK_CAPACITY;
        else if (!query) {
            cui_dock_schema_area value = {.has_version = metadata->has_version,
                                          .version = metadata->version};
            for (size_t i = 0; i < 4; i++) {
                if (!present[i])
                    continue;
                uintptr_t start = (uintptr_t)staged[i].nodes;
                uintptr_t end = start + staged[i].node_count * sizeof(*staged[i].nodes);
                for (size_t j = 0; j < staged[i].node_count; j++) {
                    cui_dock_schema_node node = staged[i].nodes[j];
                    uintptr_t pointer = (uintptr_t)node.children;
                    if (pointer >= start && pointer < end)
                        node.children =
                            &buffers[i].nodes[(pointer - start) / sizeof(*staged[i].nodes)];
                    start = (uintptr_t)staged[i].sizes;
                    end = start + staged[i].size_count * sizeof(*staged[i].sizes);
                    pointer = (uintptr_t)node.sizes;
                    if (pointer >= start && pointer < end)
                        node.sizes =
                            &buffers[i].sizes[(pointer - start) / sizeof(*staged[i].sizes)];
                    buffers[i].nodes[j] = node;
                    start = (uintptr_t)staged[i].nodes;
                    end = start + staged[i].node_count * sizeof(*staged[i].nodes);
                }
                if (staged[i].size_count)
                    memcpy(buffers[i].sizes, staged[i].sizes,
                           staged[i].size_count * sizeof(*buffers[i].sizes));
                if (!i)
                    value.center = buffers[i].nodes[0];
                else {
                    value.edge_present[i - 1] = true;
                    value.edges[i - 1] = *edges[i - 1];
                    value.edges[i - 1].panel = buffers[i].nodes[0];
                }
            }
            *output = value;
        }
    }
    for (size_t i = 0; i < 4; i++) {
        free(staged[i].nodes);
        free(staged[i].sizes);
    }
    return result;
}
