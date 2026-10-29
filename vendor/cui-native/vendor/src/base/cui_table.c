/* SPDX-License-Identifier: MIT */
#include "cui/cui_table.h"
#include "cui/cui_table_events.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct cui_table_model {
    size_t capacity, slots, count, columns, active;
    const cui_table_row *rows;
    cui_table_column *cols;
    cui_id *keys[2];
    size_t *indices[2];
};
cui_table_column cui_table_column_default(cui_id key, cui_str label) {
    return (cui_table_column){.key = key,
                              .label = label,
                              .width = 100,
                              .minimum = 20,
                              .resizable = true,
                              .movable = true};
}
cui_table_options cui_table_options_default(void) {
    return (cui_table_options){
        .bordered = true, .scrollbars = true, .selection_mode = CUI_TABLE_SELECT_ROW};
}
static uint64_t mix(uint64_t x) {
    x ^= x >> 30;
    x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27;
    x *= UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}
cui_id cui_table_row_id(cui_id id, cui_id row) {
    return id ^ mix(row) ^ UINT64_C(0x42a275ee7923b113);
}
cui_id cui_table_header_id(cui_id id, cui_id col) {
    return id ^ mix(col) ^ UINT64_C(0x82a275ee7923b113);
}
cui_id cui_table_resize_id(cui_id id, cui_id col) {
    return cui_table_header_id(id, col) ^ UINT64_C(0x22a275ee7923b113);
}
cui_id cui_table_cell_id(cui_id id, cui_id row, cui_id col) {
    return cui_table_row_id(id, row) ^ mix(col ^ UINT64_C(0xa3aa29835af2341d));
}
cui_id cui_table_body_id(cui_id id) {
    return id ^ UINT64_C(0x49c42a1399ffe201);
}
cui_id cui_table_group_header_id(cui_id id, size_t level, size_t group) {
    return id ^ mix((uint64_t)level + UINT64_C(0x6a09e667f3bcc909)) ^
           mix((uint64_t)group + UINT64_C(0xbb67ae8584caa73b));
}
cui_id cui_table_trailing_id(cui_id id, bool header, cui_id row) {
    return id ^ mix(row + UINT64_C(0x3c6ef372fe94f82b)) ^
           (header ? UINT64_C(0xa54ff53a5f1d36f1) : UINT64_C(0x510e527fade682d1));
}
cui_table_model *cui_table_create(size_t capacity) {
    if (!capacity || capacity > 1000000)
        return NULL;
    cui_table_model *m = calloc(1, sizeof(*m));
    if (!m)
        return NULL;
    m->capacity = capacity;
    m->slots = 1;
    while (m->slots < capacity * 2)
        m->slots *= 2;
    for (size_t b = 0; b < 2; b++) {
        m->keys[b] = calloc(m->slots, sizeof(cui_id));
        m->indices[b] = calloc(m->slots, sizeof(size_t));
        if (!m->keys[b] || !m->indices[b]) {
            cui_table_destroy(m);
            return NULL;
        }
    }
    return m;
}
void cui_table_destroy(cui_table_model *m) {
    if (m) {
        for (size_t b = 0; b < 2; b++) {
            free(m->keys[b]);
            free(m->indices[b]);
        }
        free(m);
    }
}
cui_table_error cui_table_bind(cui_table_model *m, const cui_table_row *rows, size_t n,
                               cui_table_column *cols, size_t nc) {
    if (!m || (n && !rows) || !cols || !nc || nc > 128)
        return CUI_TABLE_INVALID;
    if (n > m->capacity)
        return CUI_TABLE_CAPACITY;
    bool unpinned = false;
    for (size_t c = 0; c < nc; c++) {
        if (cols[c].fixed_left && unpinned)
            return CUI_TABLE_INVALID;
        if (!cols[c].fixed_left)
            unpinned = true;
        if (!cols[c].key || !isfinite(cols[c].width) || cols[c].width < 16 ||
            cols[c].width > 65536 || !isfinite(cols[c].minimum) || !isfinite(cols[c].maximum) ||
            cols[c].minimum < 0 || cols[c].maximum < 0 ||
            (cols[c].maximum && cols[c].maximum < fmaxf(16, cols[c].minimum)) ||
            (cols[c].label.length && !cols[c].label.data))
            return CUI_TABLE_INVALID;
        for (size_t p = 0; p < c; p++)
            if (cols[p].key == cols[c].key)
                return CUI_TABLE_DUPLICATE_KEY;
    }
    size_t b = 1 - m->active;
    memset(m->keys[b], 0, m->slots * sizeof(cui_id));
    for (size_t r = 0; r < n; r++) {
        if (!rows[r].key || (rows[r].label.length && !rows[r].label.data))
            return CUI_TABLE_INVALID;
        size_t slot = (size_t)mix(rows[r].key) & (m->slots - 1);
        while (m->keys[b][slot]) {
            if (m->keys[b][slot] == rows[r].key)
                return CUI_TABLE_DUPLICATE_KEY;
            slot = (slot + 1) & (m->slots - 1);
        }
        m->keys[b][slot] = rows[r].key;
        m->indices[b][slot] = r;
    }
    m->active = b;
    m->rows = rows;
    m->count = n;
    m->cols = cols;
    m->columns = nc;
    return CUI_TABLE_OK;
}
size_t cui_table_rows(const cui_table_model *m) {
    return m ? m->count : 0;
}
size_t cui_table_columns(const cui_table_model *m) {
    return m ? m->columns : 0;
}
size_t cui_table_fixed_columns(const cui_table_model *m) {
    if (!m)
        return 0;
    size_t c = 0;
    while (c < m->columns && m->cols[c].fixed_left)
        c++;
    return c;
}
float cui_table_fixed_width(const cui_table_model *m) {
    float width = 0;
    size_t n = cui_table_fixed_columns(m);
    for (size_t c = 0; c < n; c++)
        width += m->cols[c].width;
    return width;
}
bool cui_table_get_row(const cui_table_model *m, size_t r, cui_table_row *out) {
    if (!m || !out || r >= m->count)
        return false;
    *out = m->rows[r];
    return true;
}
bool cui_table_get_column(const cui_table_model *m, size_t c, cui_table_column *out) {
    if (!m || !out || c >= m->columns)
        return false;
    *out = m->cols[c];
    return true;
}
bool cui_table_row_index(const cui_table_model *m, cui_id key, size_t *out) {
    if (!m || !key || !out)
        return false;
    size_t slot = (size_t)mix(key) & (m->slots - 1);
    while (m->keys[m->active][slot]) {
        if (m->keys[m->active][slot] == key) {
            *out = m->indices[m->active][slot];
            return true;
        }
        slot = (slot + 1) & (m->slots - 1);
    }
    return false;
}
static size_t column(const cui_table_model *m, cui_id key) {
    for (size_t c = 0; c < m->columns; c++)
        if (m->cols[c].key == key)
            return c;
    return SIZE_MAX;
}
bool cui_table_resize(cui_table_model *m, cui_id key, float width) {
    if (!m || !isfinite(width))
        return false;
    size_t c = column(m, key);
    if (c == SIZE_MAX || !m->cols[c].resizable || m->cols[c].disabled)
        return false;
    float lo = fmaxf(16, m->cols[c].minimum),
          hi = m->cols[c].maximum ? fminf(65536, m->cols[c].maximum) : 65536;
    width = fminf(hi, fmaxf(lo, width));
    if (width == m->cols[c].width)
        return false;
    m->cols[c].width = width;
    return true;
}
bool cui_table_move_column(cui_table_model *m, cui_id key, size_t dest) {
    if (!m || dest >= m->columns)
        return false;
    size_t c = column(m, key);
    if (c == SIZE_MAX || c == dest || m->cols[c].fixed_left != m->cols[dest].fixed_left ||
        !m->cols[c].movable || m->cols[c].disabled)
        return false;
    for (size_t i = c < dest ? c : dest; i <= (c > dest ? c : dest); i++)
        if (!m->cols[i].movable)
            return false;
    cui_table_column save = m->cols[c];
    if (c < dest)
        memmove(m->cols + c, m->cols + c + 1, (dest - c) * sizeof(save));
    else
        memmove(m->cols + dest + 1, m->cols + dest, (c - dest) * sizeof(save));
    m->cols[dest] = save;
    return true;
}
float cui_table_row_header_width(cui_table_options o) {
    return o.row_header ? (isfinite(o.row_header_width) && o.row_header_width > 0
                               ? fmaxf(16, fminf(256, o.row_header_width))
                               : 40)
                        : 0;
}
cui_id cui_table_row_header_id(cui_id id, cui_id row) {
    return cui_table_row_id(id, row) ^ UINT64_C(0x2356019da84bcff1);
}
static float viewport_width(cui_table_options o) {
    float width = isfinite(o.width) && o.width > 0 ? o.width : 480;
    return fmaxf(1, width - cui_table_row_header_width(o));
}
static float row_height(cui_table_options o) {
    return isfinite(o.row_height) && o.row_height > 0 ? fminf(o.row_height, 4096) : 32;
}
static float height(cui_table_options o) {
    return isfinite(o.height) && o.height > 0 ? o.height : 240;
}
static float trailing_width(cui_table_options o) {
    if (!o.render_trailing)
        return 0;
    return isfinite(o.trailing_width) && o.trailing_width > 0 ? fminf(o.trailing_width, 4096) : 12;
}
bool cui_table_group_headers_valid(const cui_table_model *m, cui_table_options o) {
    if (!m || o.group_header_count > 16 || (!o.group_headers && o.group_header_count))
        return false;
    size_t fixed = cui_table_fixed_columns(m);
    for (size_t level = 0; level < o.group_header_count; level++) {
        const cui_table_group_row *row = o.group_headers + level;
        if (!row->groups || !row->count || row->count > m->columns)
            return false;
        size_t column = 0;
        for (size_t group = 0; group < row->count; group++) {
            size_t span = row->groups[group].span;
            if (!span || span > m->columns - column ||
                (row->groups[group].label.length && !row->groups[group].label.data))
                return false;
            if (column < fixed && column + span > fixed)
                return false;
            column += span;
        }
        if (column != m->columns)
            return false;
    }
    return true;
}
cui_scroll_geometry cui_table_scroll_layout(const cui_table_model *m, cui_table_options o) {
    float width = isfinite(o.width) && o.width > 0 ? o.width : 480, h = height(o);
    if (!m || !o.scrollbars ||
        (o.scrollbar_style && o.scrollbar_style->visibility == CUI_SCROLLBAR_HIDDEN))
        return (cui_scroll_geometry){.viewport = {width, h}};
    float bar = isfinite(o.bar_thickness) && o.bar_thickness > 0 ? o.bar_thickness : 10;
    if (o.scrollbar_style && isfinite(o.scrollbar_style->thickness) &&
        o.scrollbar_style->thickness > 0)
        bar = o.scrollbar_style->thickness;
    bar = fminf(bar, fminf(width, h) * .5f);
    float columns = cui_table_row_header_width(o);
    for (size_t c = 0; c < m->columns; c++)
        columns += m->cols[c].width;
    columns += trailing_width(o);
    float rows = (float)m->count * row_height(o);
    bool x = false, y = false;
    for (unsigned i = 0; i < 3; i++) {
        x = m->columns > cui_table_fixed_columns(m) && columns > width - (y ? bar : 0);
        y = rows > h - (x ? bar : 0);
    }
    cui_scroll_geometry g =
        cui_scroll_layout((cui_scroll_options){.width = width,
                                               .height = h,
                                               .content_width = columns,
                                               .content_height = rows,
                                               .bar_thickness = bar,
                                               .scrollbar_style = o.scrollbar_style,
                                               .horizontal = x,
                                               .vertical = y});
    cui_table_view v =
        cui_table_column_viewport(m, 0, g.viewport.width - cui_table_row_header_width(o), 0);
    g.maximum_x = fmaxf(0, v.total + trailing_width(o) -
                               fmaxf(1, g.viewport.width - cui_table_fixed_width(m) -
                                            cui_table_row_header_width(o)));
    return g;
}
static cui_table_options view_options(const cui_table_model *m, cui_table_options o) {
    if (o.scrollbars) {
        cui_scroll_geometry g = cui_table_scroll_layout(m, o);
        o.width = g.viewport.width;
        o.height = g.viewport.height;
        o.scrollbars = false;
    }
    return o;
}
cui_table_view cui_table_viewport(const cui_table_model *m, float scroll, float h, float row,
                                  size_t over) {
    cui_table_view v = {0};
    if (!m)
        return v;
    row = isfinite(row) && row > 0 ? fminf(row, 4096) : 32;
    h = isfinite(h) && h > 0 ? h : 240;
    v.total = (float)m->count * row;
    v.scroll = isfinite(scroll) ? fmaxf(0, fminf(scroll, fmaxf(0, v.total - h))) : 0;
    size_t first = (size_t)floorf(v.scroll / row), last = (size_t)ceilf((v.scroll + h) / row);
    if (last > m->count)
        last = m->count;
    v.begin = first > over ? first - over : 0;
    v.end = over > m->count - last ? m->count : last + over;
    v.before = (float)v.begin * row;
    v.after = (float)(m->count - v.end) * row;
    return v;
}
cui_table_view cui_table_column_viewport(const cui_table_model *m, float scroll, float width,
                                         size_t over) {
    cui_table_view v = {0};
    if (!m)
        return v;
    size_t fixed = cui_table_fixed_columns(m);
    width = isfinite(width) && width > 0 ? width : 480;
    width = fmaxf(1, width - cui_table_fixed_width(m));
    for (size_t c = fixed; c < m->columns; c++)
        v.total += m->cols[c].width;
    v.scroll = isfinite(scroll) ? fmaxf(0, fminf(scroll, fmaxf(0, v.total - width))) : 0;
    size_t first = fixed, last = fixed;
    float x = 0;
    for (size_t c = fixed; c < m->columns; c++) {
        float end = x + m->cols[c].width;
        if (end <= v.scroll)
            first = c + 1;
        if (x < v.scroll + width)
            last = c + 1;
        x = end;
    }
    v.begin = first - fixed > over ? first - over : fixed;
    v.end = over > m->columns - last ? m->columns : last + over;
    for (size_t c = fixed; c < m->columns; c++) {
        if (c < v.begin)
            v.before += m->cols[c].width;
        if (c >= v.end)
            v.after += m->cols[c].width;
    }
    return v;
}
static bool selection_mode_enabled(cui_table_options o) {
    return o.selection_mode == CUI_TABLE_SELECT_ROW ? !o.row_selection_disabled
           : o.selection_mode == CUI_TABLE_SELECT_COLUMN
               ? !o.column_selection_disabled
               : o.selection_mode == CUI_TABLE_SELECT_CELL && !o.cell_selection_disabled;
}
static bool selection_allowed(const cui_table_model *m, cui_table_options o, cui_id col) {
    if (!selection_mode_enabled(o))
        return false;
    if (o.selection_mode == CUI_TABLE_SELECT_ROW)
        return true;
    size_t c = column(m, col);
    return c != SIZE_MAX && !m->cols[c].nonselectable;
}
bool cui_table_is_selected(const cui_table_state *s, cui_table_options o, cui_id row, cui_id col) {
    if (!s || !selection_mode_enabled(o))
        return false;
    row = o.selection_mode == CUI_TABLE_SELECT_COLUMN ? 0 : row;
    col = o.selection_mode == CUI_TABLE_SELECT_ROW ? 0 : col;
    if (o.multiple_selection) {
        if (!s->selection || s->selection_count > s->selection_capacity)
            return false;
        for (size_t i = 0; i < s->selection_count; i++)
            if (s->selection[i].row == row && s->selection[i].column == col)
                return true;
        return false;
    }
    return s->row == row && s->column == col;
}
static uint64_t selection_hash(const cui_table_state *s) {
    uint64_t h = 0;
    for (size_t i = 0; i < s->selection_count; i++)
        h ^= mix(s->selection[i].row) ^ mix(s->selection[i].column ^ UINT64_C(0x23761889fc4a6bd2));
    return h;
}
static cui_table_error select(cui_table_model *m, cui_table_state *s, cui_table_options o,
                              cui_id row, cui_id col, unsigned mods) {
    cui_id normalized_row = o.selection_mode == CUI_TABLE_SELECT_COLUMN ? 0 : row,
           normalized_col = o.selection_mode == CUI_TABLE_SELECT_ROW ? 0 : col;
    if (!o.multiple_selection) {
        s->row = normalized_row;
        s->column = normalized_col;
        return CUI_TABLE_OK;
    }
    size_t anchor_index = 0;
    bool valid_anchor = column(m, s->anchor_column) != SIZE_MAX &&
                        (o.selection_mode == CUI_TABLE_SELECT_COLUMN ||
                         cui_table_row_index(m, s->anchor_row, &anchor_index));
    if ((mods & CUI_MOD_SHIFT) && valid_anchor) {
        size_t r = 0, ar = 0, c = column(m, col), ac = column(m, s->anchor_column);
        if (o.selection_mode != CUI_TABLE_SELECT_COLUMN &&
            (!cui_table_row_index(m, row, &r) || !cui_table_row_index(m, s->anchor_row, &ar)))
            return CUI_TABLE_INVALID;
        if (o.selection_mode == CUI_TABLE_SELECT_ROW)
            c = ac = 0;
        if (c == SIZE_MAX || ac == SIZE_MAX)
            return CUI_TABLE_INVALID;
        size_t r0 = r < ar ? r : ar, r1 = r > ar ? r : ar, c0 = c < ac ? c : ac,
               c1 = c > ac ? c : ac, needed = 0;
        for (size_t ri = r0; ri <= r1; ri++)
            for (size_t ci = c0; ci <= c1; ci++) {
                if ((o.selection_mode != CUI_TABLE_SELECT_COLUMN && m->rows[ri].disabled) ||
                    (o.selection_mode != CUI_TABLE_SELECT_ROW &&
                     (m->cols[ci].disabled || m->cols[ci].nonselectable)))
                    continue;
                if (++needed > s->selection_capacity)
                    return CUI_TABLE_CAPACITY;
            }
        s->selection_count = 0;
        for (size_t ri = r0; ri <= r1; ri++)
            for (size_t ci = c0; ci <= c1; ci++) {
                if ((o.selection_mode != CUI_TABLE_SELECT_COLUMN && m->rows[ri].disabled) ||
                    (o.selection_mode != CUI_TABLE_SELECT_ROW &&
                     (m->cols[ci].disabled || m->cols[ci].nonselectable)))
                    continue;
                s->selection[s->selection_count++] = (cui_table_selection){
                    o.selection_mode == CUI_TABLE_SELECT_COLUMN ? 0 : m->rows[ri].key,
                    o.selection_mode == CUI_TABLE_SELECT_ROW ? 0 : m->cols[ci].key};
            }
    } else if (mods & (CUI_MOD_CTRL | CUI_MOD_META)) {
        size_t found = SIZE_MAX;
        for (size_t i = 0; i < s->selection_count; i++)
            if (s->selection[i].row == normalized_row && s->selection[i].column == normalized_col) {
                found = i;
                break;
            }
        if (found != SIZE_MAX) {
            memmove(s->selection + found, s->selection + found + 1,
                    (s->selection_count - found - 1) * sizeof(*s->selection));
            s->selection_count--;
        } else {
            if (s->selection_count == s->selection_capacity)
                return CUI_TABLE_CAPACITY;
            s->selection[s->selection_count++] =
                (cui_table_selection){normalized_row, normalized_col};
        }
        s->anchor_row = row;
        s->anchor_column = col;
    } else {
        if (!s->selection_capacity)
            return CUI_TABLE_CAPACITY;
        s->selection[0] = (cui_table_selection){normalized_row, normalized_col};
        s->selection_count = 1;
        s->anchor_row = row;
        s->anchor_column = col;
    }
    s->row = normalized_row;
    s->column = normalized_col;
    return CUI_TABLE_OK;
}
static cui_component_event_metadata cui_table_event_metadata(cui_context *, cui_id, size_t,
                                                             cui_event);
typedef struct {
    cui_context *ui;
    cui_id table;
    size_t index;
    cui_event input;
    const cui_table_event_handler *handler;
    cui_table_event_result *events;
    const cui_table_set_event_handler *set_handler;
    cui_table_selection_workspace workspace;
    size_t previous_count;
} cui_table_selection_observer;
/* Iterative heapsort of scratch only: bounded indices, O(n log n), O(1) extra. */
static bool cui_table_selection_less(cui_table_selection a, cui_table_selection b) {
    return a.row < b.row || (a.row == b.row && a.column < b.column);
}
static void cui_table_selection_sift(cui_table_selection *items, size_t root, size_t count) {
    while (root < count / 2) {
        size_t child = root * 2 + 1;
        if (child + 1 < count && cui_table_selection_less(items[child], items[child + 1]))
            child++;
        if (!cui_table_selection_less(items[root], items[child]))
            break;
        cui_table_selection temp = items[root];
        items[root] = items[child];
        items[child] = temp;
        root = child;
    }
}
static void cui_table_selection_sort(cui_table_selection *items, size_t count) {
    for (size_t i = count / 2; i > 0; i--)
        cui_table_selection_sift(items, i - 1, count);
    for (size_t end = count; end > 1; end--) {
        cui_table_selection temp = items[0];
        items[0] = items[end - 1];
        items[end - 1] = temp;
        cui_table_selection_sift(items, 0, end - 1);
    }
}
static void cui_table_snapshot_selection(cui_table_selection_observer *observer,
                                         const cui_table_state *state) {
    if (!observer || !observer->set_handler)
        return;
    observer->previous_count = state->selection_count;
    if (state->selection_count)
        memcpy(observer->workspace.previous, state->selection,
               state->selection_count * sizeof(*state->selection));
}

static void cui_table_notify_selection(const cui_table_selection_observer *observer,
                                       cui_table_selection previous, cui_table_selection value,
                                       cui_table_selection_mode mode,
                                       cui_component_event_metadata metadata,
                                       const cui_table_state *state) {
    if (!observer)
        return;
    if (observer->set_handler) {
        cui_table_selection_workspace w = observer->workspace;
        size_t before = observer->previous_count, after = state->selection_count;
        if (after)
            memcpy(w.value, state->selection, after * sizeof(*state->selection));
        cui_table_selection_sort(w.previous, before);
        cui_table_selection_sort(w.value, after);
        bool changed =
            previous.row != value.row || previous.column != value.column || before != after;
        for (size_t i = 0; !changed && i < before; i++)
            changed =
                w.previous[i].row != w.value[i].row || w.previous[i].column != w.value[i].column;
        if (!changed)
            return;
        cui_table_selection_set_change change = {
            metadata, previous, value, mode, {w.previous, before}, {w.value, after}};
        if (observer->events->selection_count == SIZE_MAX)
            observer->events->count_overflow = true;
        else
            observer->events->selection_count++;
        observer->set_handler->on_selection(observer->set_handler->user, &change);
        return;
    }
    if (previous.row == value.row && previous.column == value.column)
        return;
    cui_table_selection_change change = {metadata, previous, value, mode};
    if (observer->events->selection_count == SIZE_MAX)
        observer->events->count_overflow = true;
    else
        observer->events->selection_count++;
    observer->handler->on_selection(observer->handler->user, &change);
}
static void apply_selection(cui_table_model *m, cui_table_state *s, cui_table_options o, cui_id row,
                            cui_id col, unsigned mods, cui_table_result *out,
                            cui_table_selection_observer *observer) {
    if (!selection_allowed(m, o, col))
        return;
    cui_table_selection previous = {s->row, s->column};
    cui_component_event_metadata metadata = {0};
    if (observer)
        metadata = cui_table_event_metadata(observer->ui, observer->table, observer->index,
                                            observer->input);
    cui_table_snapshot_selection(observer, s);
    cui_table_error error = select(m, s, o, row, col, mods);
    if (error != CUI_TABLE_OK)
        out->error = error;
    cui_table_notify_selection(observer, previous, (cui_table_selection){s->row, s->column},
                               o.selection_mode, metadata, s);
}
static size_t enabled_row(const cui_table_model *m, size_t start, int direction, bool loop) {
    if (!m->count)
        return SIZE_MAX;
    size_t p = start;
    for (size_t i = 0; i < m->count; i++) {
        if (direction < 0) {
            if (!p) {
                if (!loop)
                    return SIZE_MAX;
                p = m->count;
            }
            --p;
        } else {
            if (p + 1 >= m->count) {
                if (!loop)
                    return SIZE_MAX;
                p = 0;
            } else
                ++p;
        }
        if (!m->rows[p].disabled)
            return p;
    }
    return SIZE_MAX;
}
static size_t enabled_col(const cui_table_model *m, size_t start, int direction, bool loop) {
    size_t p = start;
    for (size_t i = 0; i < m->columns; i++) {
        if (direction < 0) {
            if (!p) {
                if (!loop)
                    return SIZE_MAX;
                p = m->columns;
            }
            --p;
        } else {
            if (p + 1 >= m->columns) {
                if (!loop)
                    return SIZE_MAX;
                p = 0;
            } else
                ++p;
        }
        if (!m->cols[p].disabled)
            return p;
    }
    return SIZE_MAX;
}
typedef struct {
    cui_context *ui;
    cui_id table;
    size_t index;
    cui_event input;
    const cui_table_event_handler *handler;
    cui_table_event_result *events;
} cui_table_input_observer;
static bool cui_table_prepare_activation(const cui_table_input_observer *observer,
                                         cui_table_action_area area, cui_id row, cui_id column_key,
                                         cui_table_activation *change) {
    if (!observer->handler || !observer->handler->on_activate ||
        cui_frame_sequence(observer->ui) == 0)
        return false;
    change->event =
        cui_table_event_metadata(observer->ui, observer->table, observer->index, observer->input);
    change->event.kind = CUI_COMPONENT_EVENT_ACTIVATE;
    change->area = area;
    change->row = row;
    change->column = column_key;
    return true;
}
static void cui_table_notify_activation(const cui_table_input_observer *observer,
                                        const cui_table_activation *change) {
    if (observer->events->activation_count == SIZE_MAX)
        observer->events->count_overflow = true;
    else
        observer->events->activation_count++;
    observer->handler->on_activate(observer->handler->user, change);
}
static bool cui_table_prepare_mode(const cui_table_input_observer *observer,
                                   cui_table_selection_mode current, cui_id row,
                                   cui_table_mode_request_reason reason,
                                   cui_table_mode_request *request) {
    if (!observer->handler || !observer->handler->on_mode_request ||
        cui_frame_sequence(observer->ui) == 0)
        return false;
    request->event =
        cui_table_event_metadata(observer->ui, observer->table, observer->index, observer->input);
    request->event.kind = CUI_COMPONENT_EVENT_REQUEST;
    request->current = current;
    request->requested = CUI_TABLE_SELECT_ROW;
    request->row = row;
    request->reason = reason;
    return true;
}
static void cui_table_notify_mode(const cui_table_input_observer *observer,
                                  const cui_table_mode_request *request) {
    if (observer->events->mode_request_count == SIZE_MAX)
        observer->events->count_overflow = true;
    else
        observer->events->mode_request_count++;
    observer->handler->on_mode_request(observer->handler->user, request);
}
static void cui_table_notify_context(const cui_table_input_observer *observer,
                                     cui_table_context_area area, cui_id row, cui_id column_key) {
    if (!observer->handler || !observer->handler->on_context_request ||
        cui_frame_sequence(observer->ui) == 0)
        return;
    cui_table_context_request request = {0};
    request.event =
        cui_table_event_metadata(observer->ui, observer->table, observer->index, observer->input);
    request.event.kind = CUI_COMPONENT_EVENT_REQUEST;
    request.area = area;
    request.row = row;
    request.column = column_key;
    if (observer->events->context_request_count == SIZE_MAX)
        observer->events->count_overflow = true;
    else
        observer->events->context_request_count++;
    observer->handler->on_context_request(observer->handler->user, &request);
}
/* Call only from reducer operations, never from the public programmatic helpers. */
static bool cui_table_resize_observed(cui_table_model *model, cui_id key, float width,
                                      const cui_table_input_observer *observer) {
    if (!observer->handler || !observer->handler->on_column_width ||
        cui_frame_sequence(observer->ui) == 0)
        return cui_table_resize(model, key, width);
    size_t before_index = column(model, key);
    bool notify = before_index != SIZE_MAX;
    float previous_width = before_index != SIZE_MAX ? model->cols[before_index].width : 0;
    cui_component_event_metadata metadata = {0};
    if (notify)
        metadata = cui_table_event_metadata(observer->ui, observer->table, observer->index,
                                            observer->input);
    bool changed = cui_table_resize(model, key, width);
    if (changed && notify && previous_width != model->cols[before_index].width) {
        cui_table_column_width_change change = {metadata, key, previous_width,
                                                model->cols[before_index].width};
        if (observer->events->column_width_count == SIZE_MAX)
            observer->events->count_overflow = true;
        else
            observer->events->column_width_count++;
        observer->handler->on_column_width(observer->handler->user, &change);
    }
    return changed;
}
static bool cui_table_move_observed(cui_table_model *model, cui_id key, size_t destination,
                                    const cui_table_input_observer *observer) {
    if (!observer->handler || !observer->handler->on_column_move ||
        cui_frame_sequence(observer->ui) == 0)
        return cui_table_move_column(model, key, destination);
    size_t previous_index = column(model, key);
    bool notify = previous_index != SIZE_MAX;
    cui_component_event_metadata metadata = {0};
    if (notify)
        metadata = cui_table_event_metadata(observer->ui, observer->table, observer->index,
                                            observer->input);
    bool changed = cui_table_move_column(model, key, destination);
    if (changed && notify) {
        size_t index = column(model, key);
        if (index != previous_index) {
            cui_table_column_move_change change = {metadata, key, previous_index, index};
            if (observer->events->column_move_count == SIZE_MAX)
                observer->events->count_overflow = true;
            else
                observer->events->column_move_count++;
            observer->handler->on_column_move(observer->handler->user, &change);
        }
    }
    return changed;
}
static bool drop_column(cui_context *ui, cui_id id, cui_table_model *m, cui_table_state *s,
                        cui_table_options o, const cui_event *event,
                        const cui_table_input_observer *observer) {
    size_t from = column(m, s->dragging);
    if (from == SIZE_MAX || !cui_hovered(ui, id) || !isfinite(event->x) || !isfinite(event->y))
        return false;
    cui_rect table = cui_bounds(ui, id),
             head = cui_bounds(ui, cui_table_header_id(id, s->dragging));
    if (event->x < table.x + cui_table_row_header_width(o) || event->x >= table.x + table.w ||
        event->y < head.y || event->y >= head.y + head.h)
        return false;
    size_t fixed = cui_table_fixed_columns(m);
    bool pinned = event->x < table.x + cui_table_fixed_width(m) + cui_table_row_header_width(o);
    if (pinned != (from < fixed))
        return false;
    cui_table_view columns =
        cui_table_column_viewport(m, s->scroll_x, viewport_width(o), o.column_overscan);
    size_t begin = pinned ? 0 : columns.begin, end = pinned ? fixed : columns.end, gap = begin;
    for (size_t c = begin; c < end; c++) {
        cui_rect bounds = cui_bounds(ui, cui_table_header_id(id, m->cols[c].key));
        if (event->x < bounds.x + bounds.w / 2)
            break;
        gap = c + 1;
    }
    if (gap == from || gap == from + 1)
        return false;
    return cui_table_move_observed(m, s->dragging, gap > from ? gap - 1 : gap, observer);
}
static bool row_header_event(cui_context *ui, cui_id id, cui_table_model *m, cui_table_state *s,
                             cui_table_options o, const cui_event *event, cui_id target,
                             cui_id activation, cui_id *previous, bool *reveal,
                             cui_table_result *out, cui_table_selection_observer *observer,
                             const cui_table_input_observer *input_observer) {
    if (!o.row_header)
        return false;
    cui_table_view view = cui_table_viewport(m, s->scroll_y, height(o), row_height(o), o.overscan);
    size_t r = SIZE_MAX;
    for (size_t p = view.begin; p < view.end; p++)
        if (target == cui_table_row_header_id(id, m->rows[p].key)) {
            r = p;
            break;
        }
    if (r == SIZE_MAX)
        return false;
    if (m->rows[r].disabled)
        return true;
    if (*previous != target)
        s->cursor_row = m->rows[r].key;
    *previous = target;
    if (event->type == CUI_EVENT_POINTER_DOWN && event->pointer_button == CUI_POINTER_SECONDARY) {
        out->context_requested = true;
        out->context_row = m->rows[r].key;
        out->context_column = 0;
        cui_table_notify_context(input_observer, CUI_TABLE_CONTEXT_ROW_HEADER, out->context_row, 0);
    }
    cui_table_options row_options = o;
    row_options.selection_mode = CUI_TABLE_SELECT_ROW;
    unsigned mods = o.selection_mode == CUI_TABLE_SELECT_ROW ? event->modifiers : 0;
    if (activation == target) {
        cui_table_activation change = {0};
        bool notify = cui_table_prepare_activation(input_observer, CUI_TABLE_ACTION_ROW_HEADER,
                                                   s->cursor_row, 0, &change);
        cui_table_mode_request request = {0};
        bool notify_mode = !o.row_selection_disabled &&
                           cui_table_prepare_mode(input_observer, o.selection_mode, s->cursor_row,
                                                  CUI_TABLE_MODE_FROM_ACTIVATION, &request);
        apply_selection(m, s, row_options, s->cursor_row, s->cursor_column, mods, out, observer);
        out->activated_row = s->cursor_row;
        if (notify)
            cui_table_notify_activation(input_observer, &change);
        out->selection_mode_requested = !o.row_selection_disabled;
        out->requested_selection_mode = CUI_TABLE_SELECT_ROW;
        if (notify_mode)
            cui_table_notify_mode(input_observer, &request);
        *reveal = true;
    }
    if (event->type == CUI_EVENT_KEY_DOWN) {
        size_t current;
        if (!cui_table_row_index(m, s->cursor_row, &current))
            return true;
        size_t next = SIZE_MAX;
        if (event->key == CUI_KEY_UP || event->key == CUI_KEY_DOWN)
            next = enabled_row(m, current, event->key == CUI_KEY_UP ? -1 : 1, o.loop_selection);
        else if (event->key == CUI_KEY_HOME || event->key == CUI_KEY_END) {
            for (size_t p = 0; p < m->count; p++) {
                size_t candidate = event->key == CUI_KEY_HOME ? p : m->count - 1 - p;
                if (!m->rows[candidate].disabled) {
                    next = candidate;
                    break;
                }
            }
        }
        if (next != SIZE_MAX) {
            s->cursor_row = m->rows[next].key;
            if (!o.manual_selection && !o.row_selection_disabled) {
                cui_table_mode_request request = {0};
                bool notify_mode =
                    cui_table_prepare_mode(input_observer, o.selection_mode, s->cursor_row,
                                           CUI_TABLE_MODE_FROM_NAVIGATION, &request);
                apply_selection(m, s, row_options, s->cursor_row, s->cursor_column, mods, out,
                                observer);
                out->selection_mode_requested = true;
                out->requested_selection_mode = CUI_TABLE_SELECT_ROW;
                if (notify_mode)
                    cui_table_notify_mode(input_observer, &request);
            }
            cui_focus(ui, cui_table_row_header_id(id, s->cursor_row));
            *reveal = true;
        }
    }
    return true;
}
static cui_component_event_metadata cui_table_event_metadata(cui_context *ui, cui_id id,
                                                             size_t index, cui_event input) {
    cui_component_event_metadata metadata = {0};
    cui_activation_snapshot snapshot;
    if (!cui_event_activation_snapshot(ui, index, &snapshot) ||
        !cui_component_event_from_activation(&metadata, CUI_COMPONENT_EVENT_CHANGE, &snapshot)) {
        cui_component_event_source source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN;
        if (input.type == CUI_EVENT_POINTER_DOWN || input.type == CUI_EVENT_POINTER_UP ||
            input.type == CUI_EVENT_POINTER_MOVE)
            source = CUI_COMPONENT_EVENT_SOURCE_POINTER;
        else if (input.type == CUI_EVENT_KEY_DOWN || input.type == CUI_EVENT_KEY_UP)
            source = CUI_COMPONENT_EVENT_SOURCE_KEYBOARD;
        else if (input.type == CUI_EVENT_ACCESSIBILITY_ACTION)
            source = CUI_COMPONENT_EVENT_SOURCE_ACCESSIBILITY;
        (void)cui_component_event_init(&metadata, CUI_COMPONENT_EVENT_CHANGE, id, source);
        (void)cui_component_event_set_frame_index(&metadata, cui_frame_sequence(ui), index);
    }
    metadata.target = id;
    return metadata;
}
static cui_table_result cui_table_reduce_common(cui_context *ui, cui_id id, cui_table_model *m,
                                                cui_table_state *s, cui_table_options o,
                                                const cui_table_event_handler *handler,
                                                cui_table_event_result *events,
                                                const cui_table_set_event_handler *set_handler,
                                                cui_table_selection_workspace workspace) {
    cui_table_result out = {0};
    if (!ui || !m || !s || !m->columns ||
        (o.selection_mode != CUI_TABLE_SELECT_ROW && o.selection_mode != CUI_TABLE_SELECT_COLUMN &&
         o.selection_mode != CUI_TABLE_SELECT_CELL) ||
        (o.sort_cycle != CUI_TABLE_SORT_CYCLE_LEGACY &&
         o.sort_cycle != CUI_TABLE_SORT_CYCLE_SOURCE) ||
        (o.accessible_name.length && !o.accessible_name.data) ||
        (s->sort != CUI_TABLE_SORT_DEFAULT && s->sort != CUI_TABLE_SORT_ASCENDING &&
         s->sort != CUI_TABLE_SORT_DESCENDING)) {
        out.error = CUI_TABLE_INVALID;
        return out;
    }
    cui_table_options original = o;
    if (o.scrollbar_style && o.scrollbar_style->visibility == CUI_SCROLLBAR_HIDDEN)
        original.scrollbars = o.scrollbars = false;
    if (!o.scrollbars)
        s->scrollbar.dragging_x = s->scrollbar.dragging_y = false;
    cui_scroll_geometry geometry = cui_table_scroll_layout(m, o);
    o = view_options(m, o);
    if (o.multiple_selection &&
        ((!s->selection && s->selection_capacity) || s->selection_capacity > 4096 ||
         s->selection_count > s->selection_capacity)) {
        out.error = CUI_TABLE_INVALID;
        return out;
    }
    if (events) {
        events->valid = true;
        events->submitted = cui_frame_sequence(ui) != 0;
    }
    if (o.disabled || o.loading) {
        s->resizing = 0;
        s->dragging = 0;
        s->drag_active = false;
    }
    size_t oldcount = o.multiple_selection ? s->selection_count : 0;
    uint64_t oldhash = o.multiple_selection ? selection_hash(s) : 0;
    cui_id oldrow = s->row, oldcol = s->column;
    size_t r = 0, c = column(m, s->cursor_column);
    if (!cui_table_row_index(m, s->cursor_row, &r) || m->rows[r].disabled) {
        s->cursor_row = 0;
        for (r = 0; r < m->count; r++)
            if (!m->rows[r].disabled) {
                s->cursor_row = m->rows[r].key;
                break;
            }
    }
    if (c == SIZE_MAX || m->cols[c].disabled) {
        s->cursor_column = 0;
        for (c = 0; c < m->columns; c++)
            if (!m->cols[c].disabled) {
                s->cursor_column = m->cols[c].key;
                break;
            }
    }
    size_t header_column = column(m, s->header_cursor);
    if (header_column == SIZE_MAX || m->cols[header_column].disabled) {
        s->header_cursor = 0;
        for (size_t p = 0; p < m->columns; p++)
            if (!m->cols[p].disabled) {
                s->header_cursor = m->cols[p].key;
                break;
            }
    }
    bool reveal_header = false;
    size_t count;
    const cui_event *ev = cui_frame_events(ui, &count);
    cui_id previous = 0;
    bool reveal = false;
    for (size_t e = 0; e < count && !o.disabled && !o.loading; e++) {
        cui_id target = cui_event_target(ui, e), activation = cui_event_activation(ui, e);
        cui_table_input_observer input_observer = {ui, id, e, ev[e], handler, events};
        cui_table_selection_observer selection_observer = {
            ui, id, e, ev[e], handler, events, set_handler, workspace, 0};
        cui_table_selection_observer *observer =
            ((handler && handler->on_selection) || (set_handler && set_handler->on_selection)) &&
                    cui_frame_sequence(ui) != 0
                ? &selection_observer
                : NULL;
        if (ev[e].type == CUI_EVENT_CANCEL) {
            s->resizing = 0;
            s->dragging = 0;
            s->drag_active = false;
            continue;
        }
        if (s->resizing) {
            if (ev[e].type == CUI_EVENT_POINTER_MOVE && isfinite(ev[e].x))
                out.columns_changed |= cui_table_resize_observed(
                    m, s->resizing, s->resize_width + ev[e].x - s->resize_x, &input_observer);
            if (ev[e].type == CUI_EVENT_POINTER_UP && ev[e].pointer_button == CUI_POINTER_PRIMARY)
                s->resizing = 0;
        }
        bool was_drag = s->drag_active;
        if (s->dragging) {
            if (ev[e].type == CUI_EVENT_POINTER_MOVE && isfinite(ev[e].x) &&
                fabsf(ev[e].x - s->drag_x) > 4)
                s->drag_active = true;
            was_drag = s->drag_active;
            if (ev[e].type == CUI_EVENT_POINTER_UP && ev[e].pointer_button == CUI_POINTER_PRIMARY) {
                if (s->drag_active)
                    out.columns_changed |= drop_column(ui, id, m, s, o, &ev[e], &input_observer);
                s->dragging = 0;
                s->drag_active = false;
            }
            if (ev[e].type == CUI_EVENT_KEY_DOWN && ev[e].key == CUI_KEY_ESCAPE) {
                s->dragging = 0;
                s->drag_active = false;
            }
        }
        if (was_drag)
            activation = 0;
        if (ev[e].type == CUI_EVENT_KEY_DOWN && ev[e].key == CUI_KEY_ESCAPE) {
            bool owned = target == id || target == cui_table_body_id(id) ||
                         target == cui_table_row_header_id(id, s->cursor_row) ||
                         target == cui_table_cell_id(id, s->cursor_row, s->cursor_column) ||
                         target == cui_table_header_id(id, s->header_cursor);
            if (owned) {
                cui_table_selection previous_selection = {s->row, s->column};
                cui_component_event_metadata metadata = {0};
                if (observer)
                    metadata = cui_table_event_metadata(ui, id, e, ev[e]);
                out.selection_changed |= s->row != 0 || s->column != 0 || s->selection_count != 0;
                cui_table_snapshot_selection(observer, s);
                s->row = s->column = s->anchor_row = s->anchor_column = 0;
                s->selection_count = 0;
                cui_table_notify_selection(observer, previous_selection, (cui_table_selection){0},
                                           o.selection_mode, metadata, s);
                s->resizing = 0;
                continue;
            }
        }
        if (row_header_event(ui, id, m, s, o, &ev[e], target, activation, &previous, &reveal, &out,
                             observer, &input_observer))
            continue;
        bool header = false, hit = false;
        size_t hc = SIZE_MAX, hr = SIZE_MAX;
        for (size_t p = 0; p < m->columns; p++) {
            if (target == cui_table_resize_id(id, m->cols[p].key)) {
                if (ev[e].type == CUI_EVENT_POINTER_DOWN &&
                    ev[e].pointer_button == CUI_POINTER_PRIMARY && m->cols[p].resizable &&
                    !m->cols[p].disabled) {
                    s->resizing = m->cols[p].key;
                    s->resize_x = ev[e].x;
                    s->resize_width = m->cols[p].width;
                }
                if (ev[e].type == CUI_EVENT_KEY_DOWN &&
                    (ev[e].key == CUI_KEY_LEFT || ev[e].key == CUI_KEY_RIGHT))
                    out.columns_changed |= cui_table_resize_observed(
                        m, m->cols[p].key, m->cols[p].width + (ev[e].key == CUI_KEY_LEFT ? -8 : 8),
                        &input_observer);
            }
            if (target == cui_table_header_id(id, m->cols[p].key)) {
                header = hit = true;
                hc = p;
                break;
            }
        }
        if (!hit) {
            cui_table_view view =
                cui_table_viewport(m, s->scroll_y, height(o), row_height(o), o.overscan);
            for (size_t p = view.begin; p < view.end && !hit; p++)
                for (size_t q = 0; q < m->columns; q++)
                    if (target == cui_table_cell_id(id, m->rows[p].key, m->cols[q].key)) {
                        hit = true;
                        hr = p;
                        hc = q;
                        break;
                    }
        }
        if (hit && hc < m->columns && !m->cols[hc].disabled) {
            if (ev[e].type == CUI_EVENT_POINTER_DOWN &&
                ev[e].pointer_button == CUI_POINTER_SECONDARY &&
                (header || !m->rows[hr].disabled)) {
                out.context_requested = true;
                out.context_column = m->cols[hc].key;
                out.context_row = header ? 0 : m->rows[hr].key;
                cui_table_notify_context(&input_observer,
                                         header ? CUI_TABLE_CONTEXT_COLUMN_HEADER
                                                : CUI_TABLE_CONTEXT_CELL,
                                         out.context_row, out.context_column);
            }
            if (header) {
                if (ev[e].type == CUI_EVENT_POINTER_DOWN &&
                    ev[e].pointer_button == CUI_POINTER_PRIMARY && m->cols[hc].movable &&
                    isfinite(ev[e].x)) {
                    s->dragging = m->cols[hc].key;
                    s->drag_x = ev[e].x;
                    s->drag_active = false;
                }
                if (previous != target)
                    s->header_cursor = m->cols[hc].key;
                previous = target;
                if (ev[e].type == CUI_EVENT_KEY_DOWN && !(ev[e].modifiers & CUI_MOD_CTRL)) {
                    size_t current = column(m, s->header_cursor), next = SIZE_MAX;
                    if (ev[e].key == CUI_KEY_LEFT || ev[e].key == CUI_KEY_RIGHT)
                        next = enabled_col(m, current, ev[e].key == CUI_KEY_LEFT ? -1 : 1,
                                           o.loop_selection);
                    else if (ev[e].key == CUI_KEY_HOME || ev[e].key == CUI_KEY_END) {
                        for (size_t p = 0; p < m->columns; p++) {
                            size_t n = ev[e].key == CUI_KEY_HOME ? p : m->columns - 1 - p;
                            if (!m->cols[n].disabled) {
                                next = n;
                                break;
                            }
                        }
                    }
                    if (next != SIZE_MAX) {
                        s->header_cursor = m->cols[next].key;
                        reveal_header = true;
                        cui_focus(ui, cui_table_header_id(id, s->header_cursor));
                        if (!o.manual_selection && o.selection_mode == CUI_TABLE_SELECT_COLUMN)
                            apply_selection(m, s, o, 0, s->header_cursor, ev[e].modifiers, &out,
                                            observer);
                    }
                }
                if (activation == target &&
                    (ev[e].type == CUI_EVENT_KEY_DOWN || ev[e].type == CUI_EVENT_KEY_UP))
                    hc = column(m, s->header_cursor);
                if (activation == target && o.selection_mode == CUI_TABLE_SELECT_COLUMN)
                    apply_selection(m, s, o, 0, m->cols[hc].key, ev[e].modifiers, &out, observer);
                if (activation == target && m->cols[hc].sortable) {
                    cui_id previous_sort_column = s->sort_column;
                    cui_table_sort previous_sort = s->sort;
                    bool notify = handler && handler->on_sort && cui_frame_sequence(ui) != 0;
                    cui_component_event_metadata metadata = {0};
                    if (notify)
                        metadata = cui_table_event_metadata(ui, id, e, ev[e]);
                    if (o.sort_cycle == CUI_TABLE_SORT_CYCLE_SOURCE) {
                        cui_table_sort current =
                            s->sort_column == m->cols[hc].key ? s->sort : CUI_TABLE_SORT_DEFAULT;
                        s->sort = current == CUI_TABLE_SORT_DEFAULT      ? CUI_TABLE_SORT_DESCENDING
                                  : current == CUI_TABLE_SORT_DESCENDING ? CUI_TABLE_SORT_ASCENDING
                                                                         : CUI_TABLE_SORT_DEFAULT;
                    } else
                        s->sort = s->sort_column == m->cols[hc].key
                                      ? (cui_table_sort)((s->sort + 1) % 3)
                                      : CUI_TABLE_SORT_ASCENDING;
                    s->sort_column = m->cols[hc].key;
                    out.sort_changed = true;
                    if (notify &&
                        (previous_sort_column != s->sort_column || previous_sort != s->sort)) {
                        cui_table_sort_change change = {metadata, previous_sort_column,
                                                        s->sort_column, previous_sort, s->sort};
                        if (events->sort_count == SIZE_MAX)
                            events->count_overflow = true;
                        else
                            events->sort_count++;
                        handler->on_sort(handler->user, &change);
                    }
                }
                if (ev[e].type == CUI_EVENT_KEY_DOWN && (ev[e].modifiers & CUI_MOD_CTRL) &&
                    (ev[e].key == CUI_KEY_LEFT || ev[e].key == CUI_KEY_RIGHT)) {
                    size_t dest = ev[e].key == CUI_KEY_LEFT ? (hc ? hc - 1 : hc) : hc + 1;
                    out.columns_changed |=
                        cui_table_move_observed(m, m->cols[hc].key, dest, &input_observer);
                }
            } else if (!m->rows[hr].disabled) {
                if (previous != target) {
                    s->cursor_row = m->rows[hr].key;
                    s->cursor_column = m->cols[hc].key;
                }
                previous = target;
                if (activation == target) {
                    cui_table_activation change = {0};
                    bool notify =
                        cui_table_prepare_activation(&input_observer, CUI_TABLE_ACTION_CELL,
                                                     s->cursor_row, s->cursor_column, &change);
                    apply_selection(m, s, o, s->cursor_row, s->cursor_column, ev[e].modifiers, &out,
                                    observer);
                    reveal = true;
                    out.activated_row = s->cursor_row;
                    out.activated_column = s->cursor_column;
                    if (notify)
                        cui_table_notify_activation(&input_observer, &change);
                }
                if (ev[e].type == CUI_EVENT_KEY_DOWN) {
                    cui_table_row_index(m, s->cursor_row, &r);
                    c = column(m, s->cursor_column);
                    size_t nr = r, nc = c;
                    if (ev[e].key == CUI_KEY_UP || ev[e].key == CUI_KEY_DOWN) {
                        size_t n =
                            enabled_row(m, r, ev[e].key == CUI_KEY_UP ? -1 : 1, o.loop_selection);
                        if (n != SIZE_MAX)
                            nr = n;
                    }
                    if (ev[e].key == CUI_KEY_PAGEUP || ev[e].key == CUI_KEY_PAGEDOWN) {
                        size_t page = (size_t)fmaxf(
                            1, fminf((float)m->count, floorf(height(o) / row_height(o))));
                        nr = ev[e].key == CUI_KEY_PAGEUP
                                 ? (r > page ? r - page : 0)
                                 : (page > m->count - 1 - r ? m->count - 1 : r + page);
                        if (m->rows[nr].disabled) {
                            size_t n =
                                enabled_row(m, nr, ev[e].key == CUI_KEY_PAGEUP ? -1 : 1, false);
                            if (n == SIZE_MAX)
                                n = enabled_row(m, nr, ev[e].key == CUI_KEY_PAGEUP ? 1 : -1, false);
                            nr = n == SIZE_MAX ? r : n;
                        }
                    }
                    if (ev[e].key == CUI_KEY_LEFT || ev[e].key == CUI_KEY_RIGHT) {
                        size_t n =
                            enabled_col(m, c, ev[e].key == CUI_KEY_LEFT ? -1 : 1, o.loop_selection);
                        if (n != SIZE_MAX)
                            nc = n;
                    }
                    if (ev[e].key == CUI_KEY_HOME || ev[e].key == CUI_KEY_END) {
                        bool by_column = o.selection_mode != CUI_TABLE_SELECT_ROW;
                        size_t length = by_column ? m->columns : m->count;
                        for (size_t p = 0; p < length; p++) {
                            size_t n = ev[e].key == CUI_KEY_HOME ? p : length - 1 - p;
                            if (by_column ? !m->cols[n].disabled : !m->rows[n].disabled) {
                                if (by_column)
                                    nc = n;
                                else
                                    nr = n;
                                break;
                            }
                        }
                    }
                    if (nr != r || nc != c) {
                        reveal = true;
                        s->cursor_row = m->rows[nr].key;
                        s->cursor_column = m->cols[nc].key;
                        if (!o.manual_selection)
                            apply_selection(m, s, o, s->cursor_row, s->cursor_column,
                                            ev[e].modifiers, &out, observer);
                        cui_focus(ui, cui_table_cell_id(id, s->cursor_row, s->cursor_column));
                    }
                }
            }
        }
        if (ev[e].type == CUI_EVENT_SCROLL && isfinite(ev[e].dy) &&
            target == cui_table_body_id(id)) {
            s->scroll_y -= ev[e].dy;
            if (isfinite(ev[e].dx))
                s->scroll_x -= ev[e].dx;
        }
    }
    float total = 0;
    for (size_t e = 0; e < count && !o.disabled && !o.loading; e++)
        if (ev[e].type == CUI_EVENT_SCROLL && cui_event_target(ui, e) == id && isfinite(ev[e].dx))
            s->scroll_x -= ev[e].dx;
    size_t fixed = cui_table_fixed_columns(m);
    for (size_t p = fixed; p < m->columns; p++)
        total += m->cols[p].width;
    total += trailing_width(o);
    float width = isfinite(o.width) && o.width > 0 ? o.width : 480;
    float available = fmaxf(1, width - cui_table_fixed_width(m) - cui_table_row_header_width(o));
    s->scroll_x =
        isfinite(s->scroll_x) ? fmaxf(0, fminf(s->scroll_x, fmaxf(0, total - available))) : 0;
    if (reveal && cui_table_row_index(m, s->cursor_row, &r) &&
        (cui_focused(ui) == cui_table_cell_id(id, s->cursor_row, s->cursor_column) ||
         cui_focused(ui) == cui_table_row_header_id(id, s->cursor_row))) {
        float top = (float)r * row_height(o);
        if (top < s->scroll_y)
            s->scroll_y = top;
        else if (top + row_height(o) > s->scroll_y + height(o))
            s->scroll_y = top + row_height(o) - height(o);
        float left = 0;
        c = column(m, s->cursor_column);
        for (size_t p = fixed; p < c && p < m->columns; p++)
            left += m->cols[p].width;
        if (c >= fixed && c < m->columns) {
            if (left < s->scroll_x)
                s->scroll_x = left;
            else if (left + m->cols[c].width > s->scroll_x + available)
                s->scroll_x = left + m->cols[c].width - available;
        }
    }
    if (reveal_header) {
        size_t hc = column(m, s->header_cursor);
        float left = 0;
        for (size_t p = fixed; p < hc && p < m->columns; p++)
            left += m->cols[p].width;
        if (hc >= fixed && hc < m->columns) {
            if (left < s->scroll_x)
                s->scroll_x = left;
            else if (left + m->cols[hc].width > s->scroll_x + available)
                s->scroll_x = left + m->cols[hc].width - available;
        }
    }
    s->scroll_y = cui_table_viewport(m, s->scroll_y, height(o), row_height(o), o.overscan).scroll;
    if (original.scrollbars) {
        s->scrollbar.x = s->scroll_x;
        s->scrollbar.y = s->scroll_y;
        cui_scroll_options bars = {
            .width = isfinite(original.width) && original.width > 0 ? original.width : 480,
            .height = height(original),
            .bar_thickness = original.bar_thickness,
            .scrollbar_style = original.scrollbar_style,
            .wheel_step = row_height(o),
            .disabled = o.disabled || o.loading};
        if (geometry.maximum_x > 0)
            cui_base_scrollbar(ui, id, &s->scrollbar, geometry, bars, false);
        if (geometry.maximum_y > 0)
            cui_base_scrollbar(ui, id, &s->scrollbar, geometry, bars, true);
        s->scroll_x = s->scrollbar.x;
        s->scroll_y = s->scrollbar.y;
    }
    out.rows = cui_table_viewport(m, s->scroll_y, height(o), row_height(o), 0);
    out.columns =
        cui_table_column_viewport(m, s->scroll_x, width - cui_table_row_header_width(o), 0);
    out.visible_rows_changed = !s->viewport_initialized || s->visible_begin != out.rows.begin ||
                               s->visible_end != out.rows.end;
    out.visible_columns_changed = !s->viewport_initialized ||
                                  s->column_begin != out.columns.begin ||
                                  s->column_end != out.columns.end;
    bool near = m->count - out.rows.end <= o.load_more_threshold;
    if (!near || s->previous_row_count != m->count || !o.load_more)
        s->load_pending = false;
    out.load_more = o.load_more && !o.disabled && !o.loading && near && !s->load_pending;
    if (out.load_more)
        s->load_pending = true;
    s->viewport_initialized = true;
    s->visible_begin = out.rows.begin;
    s->visible_end = out.rows.end;
    s->column_begin = out.columns.begin;
    s->column_end = out.columns.end;
    s->previous_row_count = m->count;
    out.selection_changed =
        oldrow != s->row || oldcol != s->column ||
        (o.multiple_selection && (oldcount != s->selection_count || oldhash != selection_hash(s)));
    return out;
}
cui_table_result cui_table_reduce(cui_context *ui, cui_id id, cui_table_model *model,
                                  cui_table_state *state, cui_table_options options) {
    return cui_table_reduce_common(ui, id, model, state, options, NULL, NULL, NULL,
                                   (cui_table_selection_workspace){0});
}
cui_table_event_result cui_table_reduce_events(cui_context *ui, cui_id id, cui_table_model *model,
                                               cui_table_state *state, cui_table_options options,
                                               cui_table_event_handler handler) {
    cui_table_event_result result = {0};
    if (!ui || !id || (handler.on_selection && options.multiple_selection)) {
        result.control.error = CUI_TABLE_INVALID;
        return result;
    }
    result.control = cui_table_reduce_common(ui, id, model, state, options, &handler, &result, NULL,
                                             (cui_table_selection_workspace){0});
    return result;
}
/* Checked half-open integer intervals; never relationally compare C pointers. */
typedef struct {
    uintptr_t begin, end;
} cui_table_memory_range;
static bool cui_table_memory(const void *pointer, size_t count, size_t size, size_t alignment,
                             cui_table_memory_range *range) {
    range->begin = range->end = 0;
    if (!count)
        return true;
    if (!pointer || count > SIZE_MAX / size)
        return false;
    uintptr_t begin = (uintptr_t)pointer;
    size_t bytes = count * size;
    if (begin % alignment || bytes > UINTPTR_MAX - begin)
        return false;
    range->begin = begin;
    range->end = begin + bytes;
    return true;
}
static bool cui_table_memory_overlaps(cui_table_memory_range a, cui_table_memory_range b) {
    return a.begin < a.end && b.begin < b.end && a.begin < b.end && b.begin < a.end;
}
static cui_table_set_preflight cui_table_set_preflight_check(
    cui_context *ui, cui_id id, const cui_table_model *m, const cui_table_state *s,
    cui_table_options o, cui_table_selection_workspace w, cui_table_set_event_handler handler) {
    if (!ui || !id || !m || !s || !m->columns ||
        (o.selection_mode != CUI_TABLE_SELECT_ROW && o.selection_mode != CUI_TABLE_SELECT_COLUMN &&
         o.selection_mode != CUI_TABLE_SELECT_CELL) ||
        (o.sort_cycle != CUI_TABLE_SORT_CYCLE_LEGACY &&
         o.sort_cycle != CUI_TABLE_SORT_CYCLE_SOURCE) ||
        (o.accessible_name.length && !o.accessible_name.data) ||
        (s->sort != CUI_TABLE_SORT_DEFAULT && s->sort != CUI_TABLE_SORT_ASCENDING &&
         s->sort != CUI_TABLE_SORT_DESCENDING) ||
        (o.multiple_selection &&
         ((!s->selection && s->selection_capacity) || s->selection_capacity > 4096 ||
          s->selection_count > s->selection_capacity)) ||
        (handler.on_selection && !o.multiple_selection))
        return CUI_TABLE_SET_PREFLIGHT_INVALID;
    if (!handler.on_selection)
        return CUI_TABLE_SET_PREFLIGHT_OK;
    cui_table_memory_range scratch[2], owned[5];
    if (w.capacity > 4096 ||
        !cui_table_memory(w.previous, w.capacity, sizeof(*w.previous),
                          _Alignof(cui_table_selection), &scratch[0]) ||
        !cui_table_memory(w.value, w.capacity, sizeof(*w.value), _Alignof(cui_table_selection),
                          &scratch[1]) ||
        !cui_table_memory(s, 1, sizeof(*s), _Alignof(cui_table_state), &owned[0]) ||
        !cui_table_memory(s->selection, s->selection_capacity, sizeof(*s->selection),
                          _Alignof(cui_table_selection), &owned[1]) ||
        !cui_table_memory(m, 1, sizeof(*m), _Alignof(cui_table_model), &owned[2]) ||
        !cui_table_memory(m->rows, m->count, sizeof(*m->rows), _Alignof(cui_table_row),
                          &owned[3]) ||
        !cui_table_memory(m->cols, m->columns, sizeof(*m->cols), _Alignof(cui_table_column),
                          &owned[4]))
        return CUI_TABLE_SET_PREFLIGHT_INVALID;
    if (w.capacity < s->selection_capacity)
        return CUI_TABLE_SET_PREFLIGHT_WORKSPACE_CAPACITY;
    if (cui_table_memory_overlaps(scratch[0], scratch[1]))
        return CUI_TABLE_SET_PREFLIGHT_OVERLAP;
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < 5; j++)
            if (cui_table_memory_overlaps(scratch[i], owned[j]))
                return CUI_TABLE_SET_PREFLIGHT_OVERLAP;
    return CUI_TABLE_SET_PREFLIGHT_OK;
}
cui_table_set_event_result cui_table_reduce_set_events(cui_context *ui, cui_id id,
                                                       cui_table_model *model,
                                                       cui_table_state *state,
                                                       cui_table_options options,
                                                       cui_table_selection_workspace workspace,
                                                       cui_table_set_event_handler handler) {
    cui_table_set_event_result result = {0};
    result.preflight =
        cui_table_set_preflight_check(ui, id, model, state, options, workspace, handler);
    if (result.preflight != CUI_TABLE_SET_PREFLIGHT_OK) {
        result.events.control.error = CUI_TABLE_INVALID;
        return result;
    }
    cui_table_event_handler scalar = {.on_sort = handler.on_sort,
                                      .on_column_width = handler.on_column_width,
                                      .on_column_move = handler.on_column_move,
                                      .on_activate = handler.on_activate,
                                      .on_context_request = handler.on_context_request,
                                      .on_mode_request = handler.on_mode_request,
                                      .user = handler.user};
    result.events.control =
        cui_table_reduce_common(ui, id, model, state, options, &scalar, &result.events,
                                handler.on_selection ? &handler : NULL, workspace);
    return result;
}
void cui_table_submit_root(cui_context *ui, cui_id id, cui_table_model *m, cui_table_state *s,
                           cui_table_options o) {
    o = view_options(m, o);
    if (!ui || !m || !s)
        return;
    cui_table_view columns = cui_table_column_viewport(m, s->scroll_x, viewport_width(o), 0);
    float width = isfinite(o.width) && o.width > 0 ? o.width : 480;
    cui_interact(ui, id, CUI_ROLE_GRID,
                 o.accessible_name.length ? o.accessible_name : cui_string("Data table"),
                 o.disabled || o.loading, false, 0);
    cui_set_tab_stop(ui, id, false);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.row_count = (unsigned)m->count,
                               .column_count = (unsigned)m->columns,
                               .multiselectable = o.multiple_selection,
                               .busy = o.loading,
                               .scrollable = true,
                               .scroll_x = s->scroll_x,
                               .scroll_max_x =
                                   fmaxf(0, columns.total + trailing_width(o) -
                                                fmaxf(1, width - cui_table_fixed_width(m) -
                                                             cui_table_row_header_width(o)))});
}
void cui_table_submit_header_column(cui_context *ui, cui_id id, cui_table_model *m,
                                    cui_table_state *s, cui_table_options o, size_t c) {
    o = view_options(m, o);
    if (!ui || !m || !s || c >= m->columns)
        return;
    cui_table_view columns =
        cui_table_column_viewport(m, s->scroll_x, viewport_width(o), o.column_overscan);
    size_t tab = column(m, s->header_cursor);
    if (tab == SIZE_MAX ||
        (!m->cols[tab].fixed_left && (tab < columns.begin || tab >= columns.end)) ||
        m->cols[tab].disabled) {
        tab = SIZE_MAX;
        for (size_t p = 0; p < m->columns; p++)
            if (!m->cols[p].disabled &&
                (m->cols[p].fixed_left || (p >= columns.begin && p < columns.end))) {
                tab = p;
                break;
            }
    }
    cui_id cid = cui_table_header_id(id, m->cols[c].key);
    bool disabled = o.disabled || o.loading || m->cols[c].disabled;
    cui_interact(ui, cid, CUI_ROLE_COLUMNHEADER, m->cols[c].label, disabled, false,
                 s->sort_column == m->cols[c].key ? (float)s->sort : 0);
    cui_set_tab_stop(ui, cid, c == tab);
    cui_set_semantic_details(
        ui, cid,
        (cui_semantic_details){.parent = id,
                               .column_index = (unsigned)c + 1,
                               .cursor = disabled ? CUI_CURSOR_NOT_ALLOWED : CUI_CURSOR_POINTER,
                               .selected = !m->cols[c].nonselectable &&
                                           o.selection_mode == CUI_TABLE_SELECT_COLUMN &&
                                           cui_table_is_selected(s, o, 0, m->cols[c].key)});
    if (m->cols[c].resizable) {
        cui_id grip = cui_table_resize_id(id, m->cols[c].key);
        cui_interact(ui, grip, CUI_ROLE_SLIDER, cui_string("Column width"), disabled, false,
                     m->cols[c].width);
        cui_set_tab_stop(ui, grip, false);
        cui_set_semantic_details(
            ui, grip,
            (cui_semantic_details){.parent = cid,
                                   .minimum = fmaxf(16, m->cols[c].minimum),
                                   .maximum = m->cols[c].maximum ? m->cols[c].maximum : 65536});
    }
}
void cui_table_submit_header(cui_context *ui, cui_id id, cui_table_model *m, cui_table_state *s,
                             cui_table_options o) {
    o = view_options(m, o);
    if (!ui || !m || !s)
        return;
    cui_table_view columns =
        cui_table_column_viewport(m, s->scroll_x, viewport_width(o), o.column_overscan);
    for (size_t c = 0; c < cui_table_fixed_columns(m); c++)
        cui_table_submit_header_column(ui, id, m, s, o, c);
    for (size_t c = columns.begin; c < columns.end; c++)
        cui_table_submit_header_column(ui, id, m, s, o, c);
}
void cui_table_submit_body_root(cui_context *ui, cui_id id, cui_table_model *m, cui_table_state *s,
                                cui_table_options o) {
    o = view_options(m, o);
    if (!ui || !m || !s)
        return;
    cui_id body = cui_table_body_id(id);
    cui_interact(ui, body, CUI_ROLE_ROWGROUP, cui_string("Rows"), o.disabled || o.loading, false,
                 0);
    cui_set_tab_stop(ui, body, false);
    cui_table_view allrows = cui_table_viewport(m, s->scroll_y, height(o), row_height(o), 0);
    cui_table_view allcols = cui_table_column_viewport(m, s->scroll_x, viewport_width(o), 0);
    float width = isfinite(o.width) && o.width > 0 ? o.width : 480;
    cui_set_semantic_details(
        ui, body,
        (cui_semantic_details){.parent = id,
                               .scrollable = true,
                               .scroll_y = s->scroll_y,
                               .scroll_x = s->scroll_x,
                               .scroll_max_y = fmaxf(0, allrows.total - height(o)),
                               .scroll_max_x =
                                   fmaxf(0, allcols.total + trailing_width(o) -
                                                fmaxf(1, width - cui_table_fixed_width(m) -
                                                             cui_table_row_header_width(o)))});
}
void cui_table_submit_row(cui_context *ui, cui_id id, cui_table_model *m, cui_table_state *s,
                          cui_table_options o, size_t r) {
    o = view_options(m, o);
    if (!ui || !m || !s || r >= m->count)
        return;
    cui_id body = cui_table_body_id(id);
    cui_id rid = cui_table_row_id(id, m->rows[r].key);
    cui_interact(ui, rid, CUI_ROLE_ROW, m->rows[r].label, o.disabled || m->rows[r].disabled, false,
                 (float)r);
    cui_set_tab_stop(ui, rid, false);
    cui_set_semantic_details(
        ui, rid,
        (cui_semantic_details){.parent = body,
                               .row_index = (unsigned)r + 1,
                               .selected = o.selection_mode == CUI_TABLE_SELECT_ROW &&
                                           cui_table_is_selected(s, o, m->rows[r].key, 0)});
    if (o.row_header) {
        cui_id header = cui_table_row_header_id(id, m->rows[r].key);
        cui_interact(ui, header, CUI_ROLE_ROWHEADER,
                     m->rows[r].label.length ? m->rows[r].label : cui_string("Select row"),
                     o.disabled || m->rows[r].disabled, false, (float)r + 1);
        cui_set_tab_stop(ui, header, false);
        cui_set_semantic_details(
            ui, header,
            (cui_semantic_details){.parent = rid,
                                   .readonly = o.row_selection_disabled,
                                   .row_index = (unsigned)r + 1,
                                   .selected = o.selection_mode == CUI_TABLE_SELECT_ROW &&
                                               cui_table_is_selected(s, o, m->rows[r].key, 0)});
    }
}
void cui_table_submit_cell(cui_context *ui, cui_id id, cui_table_model *m, cui_table_state *s,
                           cui_table_options o, size_t r, size_t c, cui_table_label_fn label,
                           void *data) {
    o = view_options(m, o);
    if (!ui || !m || !s || r >= m->count || c >= m->columns)
        return;
    cui_id rid = cui_table_row_id(id, m->rows[r].key);
    cui_id cid = cui_table_cell_id(id, m->rows[r].key, m->cols[c].key);
    bool selected = selection_allowed(m, o, m->cols[c].key) &&
                    cui_table_is_selected(s, o, m->rows[r].key, m->cols[c].key);
    cui_interact(ui, cid, CUI_ROLE_GRIDCELL,
                 label ? label(data, r, m->cols[c].key) : m->rows[r].label,
                 o.disabled || m->rows[r].disabled || m->cols[c].disabled, false, 0);
    cui_set_tab_stop(ui, cid,
                     s->cursor_row == m->rows[r].key && s->cursor_column == m->cols[c].key);
    cui_set_semantic_details(
        ui, cid,
        (cui_semantic_details){.parent = rid,
                               .selected = selected,
                               .readonly = !selection_allowed(m, o, m->cols[c].key),
                               .row_index = (unsigned)r + 1,
                               .column_index = (unsigned)c + 1});
}
void cui_table_submit_body(cui_context *ui, cui_id id, cui_table_model *m, cui_table_state *s,
                           cui_table_options o, cui_table_label_fn label, void *data) {
    o = view_options(m, o);
    if (!ui || !m || !s)
        return;
    cui_table_submit_body_root(ui, id, m, s, o);
    if (o.loading || !m->count) {
        cui_id placeholder = cui_table_body_id(id) ^ UINT64_C(0x790416281c39bdd1);
        cui_interact(ui, placeholder, CUI_ROLE_STATUS,
                     cui_string(o.loading ? "Loading rows" : "No rows"), false, false, 0);
        cui_set_tab_stop(ui, placeholder, false);
        cui_set_semantic_details(
            ui, placeholder,
            (cui_semantic_details){.parent = cui_table_body_id(id), .busy = o.loading});
        return;
    }
    cui_table_view rows = cui_table_viewport(m, s->scroll_y, height(o), row_height(o), o.overscan),
                   cols = cui_table_column_viewport(m, s->scroll_x, viewport_width(o),
                                                    o.column_overscan);
    for (size_t r = rows.begin; r < rows.end; r++) {
        cui_table_submit_row(ui, id, m, s, o, r);
        for (size_t c = 0; c < cui_table_fixed_columns(m); c++)
            cui_table_submit_cell(ui, id, m, s, o, r, c, label, data);
        for (size_t c = cols.begin; c < cols.end; c++)
            cui_table_submit_cell(ui, id, m, s, o, r, c, label, data);
    }
}
cui_table_result cui_base_data_table(cui_context *ui, cui_id id, cui_table_model *m,
                                     cui_table_state *s, cui_table_options o) {
    cui_table_result result = cui_table_reduce(ui, id, m, s, o);
    if (result.error == CUI_TABLE_OK || result.error == CUI_TABLE_CAPACITY) {
        cui_table_submit_root(ui, id, m, s, o);
        cui_table_submit_header(ui, id, m, s, o);
        cui_table_submit_body(ui, id, m, s, o, NULL, NULL);
    }
    return result;
}

cui_table_event_result cui_base_data_table_events(cui_context *ui, cui_id id,
                                                  cui_table_model *model, cui_table_state *state,
                                                  cui_table_options options,
                                                  cui_table_event_handler handler) {
    cui_table_event_result result = cui_table_reduce_events(ui, id, model, state, options, handler);
    if (result.control.error == CUI_TABLE_OK || result.control.error == CUI_TABLE_CAPACITY) {
        cui_table_submit_root(ui, id, model, state, options);
        cui_table_submit_header(ui, id, model, state, options);
        cui_table_submit_body(ui, id, model, state, options, NULL, NULL);
    }
    return result;
}

cui_table_set_event_result cui_base_data_table_set_events(cui_context *ui, cui_id id,
                                                          cui_table_model *model,
                                                          cui_table_state *state,
                                                          cui_table_options options,
                                                          cui_table_selection_workspace workspace,
                                                          cui_table_set_event_handler handler) {
    cui_table_set_event_result result =
        cui_table_reduce_set_events(ui, id, model, state, options, workspace, handler);
    if (result.events.control.error == CUI_TABLE_OK ||
        result.events.control.error == CUI_TABLE_CAPACITY) {
        cui_table_submit_root(ui, id, model, state, options);
        cui_table_submit_header(ui, id, model, state, options);
        cui_table_submit_body(ui, id, model, state, options, NULL, NULL);
    }
    return result;
}

bool cui_table_span_width(const float *widths, size_t count, size_t start, size_t span,
                          float *out) {
    if (!widths || !out || !count || count > 128 || start >= count || !span || span > count - start)
        return false;
    float total = 0;
    for (size_t i = start; i < start + span; i++) {
        if (!isfinite(widths[i]) || widths[i] <= 0 || widths[i] > 65536)
            return false;
        total += widths[i];
    }
    *out = total;
    return true;
}
