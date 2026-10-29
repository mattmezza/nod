/* SPDX-License-Identifier: MIT */
#include "cui/cui_text_view.h"
#include "cui/cui_components.h"
#include <math.h>
#include <string.h>
cui_text_view_style cui_text_view_style_default(void) {
    return (cui_text_view_style){.foreground = {24, 24, 27, 255},
                                 .muted = {113, 113, 122, 255},
                                 .link = {37, 99, 235, 255},
                                 .selection = {59, 130, 246, 80},
                                 .code_background = {244, 244, 245, 255},
                                 .border = {228, 228, 231, 255},
                                 .mark = {255, 220, 0, 100},
                                 .table_background = {255, 255, 255, 255},
                                 .font_size = 14,
                                 .line_height = 20,
                                 .paragraph_gap = 16};
}
cui_text_view_style cui_text_view_style_from_theme(const cui_theme *t) {
    if (!t)
        return cui_text_view_style_default();
    return (cui_text_view_style){.foreground = t->text,
                                 .muted = t->muted,
                                 .link = t->primary,
                                 .selection = t->selection,
                                 .code_background = t->accent,
                                 .border = t->border,
                                 .mark = t->warning,
                                 .table_background = t->surface,
                                 .font_size = t->font_size * .875f,
                                 .line_height = t->font_size * 1.25f,
                                 .paragraph_gap = t->font_size};
}
cui_text_view_options cui_text_view_default(void) {
    return (cui_text_view_options){.width = -1,
                                   .height = -1,
                                   .selectable = true,
                                   .code_actions_height = 32,
                                   .table_actions_height = 32};
}
static cui_text_style cui_text_view_text_style(unsigned flags) {
    return (cui_text_style){.weight = flags & CUI_TEXT_VIEW_BOLD       ? CUI_TEXT_WEIGHT_BOLD
                                      : flags & CUI_TEXT_VIEW_SEMIBOLD ? CUI_TEXT_WEIGHT_SEMIBOLD
                                      : flags & CUI_TEXT_VIEW_MEDIUM   ? CUI_TEXT_WEIGHT_MEDIUM
                                                                       : CUI_TEXT_WEIGHT_NORMAL,
                            .italic = (flags & CUI_TEXT_VIEW_ITALIC) != 0,
                            .monospace = (flags & CUI_TEXT_VIEW_CODE) != 0};
}
static cui_size cui_text_view_core_measure(void *ui, cui_str text, float font, unsigned flags) {
    return cui_measure_text_ex(ui, text, font, cui_text_view_text_style(flags));
}
static bool cui_text_view_core_font_metrics(void *ui, float font, unsigned flags,
                                            cui_font_metrics *out) {
    return cui_measure_font_metrics(ui, font, cui_text_view_text_style(flags), out);
}
static cui_str cui_text_view_slice(cui_str s, cui_text_view_range r) {
    return r.begin <= r.end && r.end <= s.length ? (cui_str){s.data + r.begin, r.end - r.begin}
                                                 : (cui_str){0};
}
static bool cui_text_view_overlap(cui_text_view_range a, cui_text_view_range b) {
    return a.begin < b.end && a.end > b.begin;
}
static size_t cui_text_view_previous(cui_str text, size_t at) {
    if (at) {
        --at;
        while (at && ((unsigned char)text.data[at] & 0xc0) == 0x80)
            --at;
    }
    return at;
}
static size_t cui_text_view_next(cui_str text, size_t at) {
    if (at < text.length) {
        ++at;
        while (at < text.length && ((unsigned char)text.data[at] & 0xc0) == 0x80)
            ++at;
    }
    return at;
}
static size_t cui_text_view_pointer_hit(const cui_text_view_state *s, float x, float y,
                                        cui_text_view_table_mode mode) {
    if (mode != CUI_TEXT_VIEW_TABLE_WRAP)
        for (size_t i = 0; i < s->block_count; ++i) {
            const cui_text_view_block *b = &s->workspace.blocks[i];
            if (b->kind != CUI_TEXT_VIEW_TABLE_CELL ||
                (i && s->workspace.blocks[i - 1].kind == CUI_TEXT_VIEW_TABLE_CELL &&
                 s->workspace.blocks[i - 1].table == b->table))
                continue;
            if (y >= b->table_frame.y && y < b->table_frame.y + b->table_frame.h) {
                x += b->table_scroll.x;
                break;
            }
        }
    return cui_text_view_hit(s, x, y);
}
static bool cui_text_view_selection_target(const cui_text_view_state *s, cui_id root,
                                           cui_id target) {
    if (target == root || target == (root ^ UINT64_C(0x891dd9c052ad9f04)))
        return true;
    for (size_t i = 0; i < s->block_count; ++i)
        if (s->workspace.blocks[i].kind == CUI_TEXT_VIEW_TABLE_CELL &&
            target == (root ^ UINT64_C(0x9c7a1d9300000000) ^ (uint64_t)i))
            return true;
    for (size_t i = 0; i < s->glyph_count; ++i) {
        const cui_text_view_glyph *g = &s->workspace.glyphs[i];
        const cui_text_view_span *span = &s->workspace.spans[g->span];
        if (span->link.begin == span->link.end)
            continue;
        if (i) {
            const cui_text_view_glyph *prev = &s->workspace.glyphs[i - 1];
            const cui_text_view_span *old = &s->workspace.spans[prev->span];
            if (prev->bounds.y == g->bounds.y && old->link.begin == span->link.begin &&
                old->link.end == span->link.end)
                continue;
        }
        cui_id link = root ^ UINT64_C(0x951f5d7100000000) ^ (uint64_t)span->link.begin ^
                      ((uint64_t)g->text.begin << 24);
        if (target == link)
            return true;
    }
    return false;
}
static bool cui_text_view_multi_click(cui_text_view_state *s, cui_text_view_table_mode mode,
                                      const cui_event *event) {
    size_t at = cui_text_view_pointer_hit(s, event->x - s->viewport.x,
                                          event->y - s->viewport.y + s->scroll.y, mode);
    if (at == SIZE_MAX || event->click_count < 2)
        return false;
    size_t hit = SIZE_MAX;
    float x = event->x - s->viewport.x, y = event->y - s->viewport.y + s->scroll.y;
    for (size_t i = 0; i < s->glyph_count; ++i) {
        const cui_text_view_glyph *g = &s->workspace.glyphs[i];
        float gx = g->bounds.x;
        if (mode != CUI_TEXT_VIEW_TABLE_WRAP &&
            s->workspace.blocks[g->block].kind == CUI_TEXT_VIEW_TABLE_CELL)
            gx -= s->workspace.blocks[g->block].table_scroll.x;
        if (x >= gx && x < gx + g->bounds.w && y >= g->bounds.y && y < g->bounds.y + g->bounds.h) {
            hit = i;
            break;
        }
    }
    if (hit == SIZE_MAX || (s->workspace.glyphs[hit].flags & CUI_TEXT_VIEW_INLINE_IMAGE))
        return false;
    const cui_text_view_glyph *g = &s->workspace.glyphs[hit];
    const cui_text_view_block *block = &s->workspace.blocks[g->block];
    bool flow = false;
    for (size_t i = 0; i < s->span_count; ++i)
        if ((s->workspace.spans[i].flags & CUI_TEXT_VIEW_INLINE_IMAGE) &&
            s->workspace.spans[i].text.begin >= block->text.begin &&
            s->workspace.spans[i].text.end <= block->text.end)
            flow = true;
    cui_text_view_range range;
    if (event->click_count == 3 && flow) {
        size_t first = SIZE_MAX, last = 0;
        for (size_t i = 0; i < s->glyph_count; ++i) {
            const cui_text_view_glyph *other = &s->workspace.glyphs[i];
            if (other->block == g->block && other->line_top == g->line_top &&
                !(other->flags & CUI_TEXT_VIEW_INLINE_IMAGE)) {
                if (first == SIZE_MAX)
                    first = other->text.begin;
                last = other->text.end;
            }
        }
        if (first == SIZE_MAX)
            return false;
        (void)cui_text_view_select(s, first, last);
        for (size_t i = 0; i < s->block_count; ++i)
            s->workspace.blocks[i].projected_selection = (cui_text_view_range){0};
        s->workspace.blocks[g->block].projected_selection = (cui_text_view_range){first, last};
        s->selection_projected = true;
    } else {
        if (event->click_count == 3)
            range = block->text;
        else if (!cui_text_view_selection_range_at(s, at, event->click_count, &range))
            return false;
        (void)cui_text_view_select(s, range.begin, range.end);
        s->selection_override = event->click_count <= 3;
    }
    s->selecting = s->dragged = false;
    s->multi_click_release = true;
    return true;
}
bool cui_text_view_selection_update(cui_context *ui, cui_text_view_selection *m,
                                    cui_text_view_options o) {
    if (!ui || !m)
        return false;
    bool changed = false;
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count; ++i) {
        const cui_event *e = &events[i];
        if (e->type == CUI_EVENT_CANCEL) {
            m->dragging = m->dragged = false;
            continue;
        }
        if (cui_event_consumed(ui, i)) {
            if (e->type == CUI_EVENT_POINTER_UP && e->pointer_button == CUI_POINTER_PRIMARY)
                m->dragging = false;
            continue;
        }
        if (e->type == CUI_EVENT_POINTER_DOWN && e->pointer_button == CUI_POINTER_PRIMARY) {
            for (size_t j = 0; j < m->count; ++j) {
                cui_text_view_selection_member *p = &m->members[j];
                if (!p->enabled || !p->state->glyph_count || p->state->viewport.w <= 0 ||
                    p->state->viewport.h <= 0 || !cui_event_hit_within_scope(ui, i, p->scope) ||
                    !cui_text_view_selection_target(p->state, p->id, cui_event_target(ui, i)))
                    continue;
                cui_text_view_selection_clear(m);
                changed = true;
                m->owner = p->id;
                if (e->click_count >= 2) {
                    m->scope = p->scope;
                    m->active = cui_text_view_multi_click(p->state, p->table_mode, e);
                    m->dragging = false;
                    if (m->active)
                        cui_focus(ui, p->id);
                    break;
                }
                changed |= cui_text_view_selection_project(m, p->scope, e->x, e->y, e->x, e->y);
                m->dragging = true;
                cui_focus(ui, p->id);
                break;
            }
        } else if (m->dragging && (e->type == CUI_EVENT_POINTER_MOVE ||
                                   (e->type == CUI_EVENT_POINTER_UP &&
                                    e->pointer_button == CUI_POINTER_PRIMARY))) {
            if (!cui_pointer_scope_allowed(ui, m->scope)) {
                m->dragging = false;
                continue;
            }
            if (fabsf(e->x - m->anchor_x) > 2 || fabsf(e->y - m->anchor_y) > 2)
                m->dragged = true;
            if (m->dragged) {
                changed |= cui_text_view_selection_project(m, m->scope, m->anchor_x, m->anchor_y,
                                                           e->x, e->y);
                (void)cui_cancel_event_activation(ui, i);
                (void)cui_consume_event(ui, i);
            }
            if (e->type == CUI_EVENT_POINTER_UP)
                m->dragging = false;
        } else if (e->type == CUI_EVENT_KEY_DOWN && m->active && cui_focused(ui) == m->owner &&
                   cui_focus_within(ui, m->scope)) {
            bool handled = false;
            if ((e->modifiers & (CUI_MOD_CTRL | CUI_MOD_META)) && e->key == CUI_KEY_C) {
                char scratch[CUI_TEXT_VIEW_BYTE_LIMIT];
                char *buffer = o.copy_buffer ? o.copy_buffer : scratch;
                size_t capacity = o.copy_buffer ? o.copy_capacity : sizeof scratch, length;
                if (o.clipboard && (o.clipboard->capabilities & CUI_PLATFORM_CLIPBOARD) &&
                    o.clipboard->clipboard_set &&
                    cui_text_view_selection_copy(m, o.copy_format, buffer, capacity, &length) ==
                        CUI_TEXT_VIEW_OK &&
                    length)
                    (void)o.clipboard->clipboard_set(o.clipboard->user, (cui_str){buffer, length});
                handled = true;
            } else if ((e->modifiers & (CUI_MOD_CTRL | CUI_MOD_META)) && e->key == CUI_KEY_A) {
                for (size_t j = 0; j < m->count; ++j)
                    if (m->members[j].enabled && m->members[j].scope == m->scope)
                        changed |= cui_text_view_select(m->members[j].state, 0,
                                                        m->members[j].state->text_length);
                handled = true;
            } else if (e->key == CUI_KEY_ESCAPE) {
                cui_text_view_selection_clear(m);
                changed = handled = true;
            }
            if (handled)
                (void)cui_consume_event(ui, i);
        }
    }
    if (changed)
        cui_invalidate(ui);
    return changed;
}
static void cui_text_view_project_pointer(cui_id id, cui_text_view_state *s,
                                          cui_text_view_table_mode mode, float x, float y) {
    cui_text_view_selection_member member = {
        .state = s, .id = id, .scope = id, .table_mode = mode, .enabled = true};
    cui_text_view_selection manager = {.members = &member, .count = 1, .capacity = 1};
    (void)cui_text_view_selection_project(&manager, id, s->selection_x, s->selection_y, x, y);
}
static void cui_text_view_reduce(cui_context *ui, cui_id id, cui_text_view_state *s,
                                 cui_text_view_options o) {
    if (!o.selectable || o.disabled) {
        s->selecting = false;
        return;
    }
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    for (size_t i = 0; i < count; ++i) {
        const cui_event *e = &events[i];
        if (e->type == CUI_EVENT_CANCEL) {
            s->selecting = false;
            s->dragged = false;
            continue;
        }
        if (cui_event_consumed(ui, i)) {
            if (e->type == CUI_EVENT_POINTER_UP && e->pointer_button == CUI_POINTER_PRIMARY)
                s->selecting = false;
            continue;
        }
        if (e->type == CUI_EVENT_POINTER_UP && e->pointer_button == CUI_POINTER_PRIMARY &&
            s->multi_click_release && cui_pointer_scope_allowed(ui, id)) {
            s->multi_click_release = false;
            (void)cui_cancel_event_activation(ui, i);
            (void)cui_consume_event(ui, i);
            continue;
        }
        bool pointer = e->type == CUI_EVENT_POINTER_DOWN || e->type == CUI_EVENT_POINTER_MOVE ||
                       e->type == CUI_EVENT_POINTER_UP;
        bool owned = pointer && cui_event_within_scope(ui, i, id) &&
                     cui_text_view_selection_target(s, id, cui_event_target(ui, i));
        if (!o.shared_selection && pointer &&
            ((e->type == CUI_EVENT_POINTER_DOWN && owned) ||
             (s->selecting && cui_pointer_scope_allowed(ui, id)))) {
            size_t at = cui_text_view_pointer_hit(s, e->x - s->viewport.x,
                                                  e->y - s->viewport.y + s->scroll.y, o.table_mode);
            if (at == SIZE_MAX)
                continue;
            if (e->type == CUI_EVENT_POINTER_DOWN && e->pointer_button == CUI_POINTER_PRIMARY) {
                if (e->click_count >= 2) {
                    (void)cui_text_view_select(s, at, at);
                    if (cui_text_view_multi_click(s, o.table_mode, e))
                        cui_focus(ui, id);
                    cui_invalidate(ui);
                    continue;
                }
                s->selecting = true;
                s->dragged = false;
                s->pointer_x = s->press_x = e->x;
                s->pointer_y = s->press_y = e->y;
                s->selection_x = e->x;
                s->selection_y = e->y;
                if (e->modifiers & CUI_MOD_SHIFT)
                    for (size_t j = 0; j < s->glyph_count; ++j) {
                        const cui_text_view_glyph *g = &s->workspace.glyphs[j];
                        if (!(g->flags & CUI_TEXT_VIEW_INLINE_IMAGE) &&
                            s->anchor >= g->text.begin && s->anchor <= g->text.end) {
                            s->selection_x = s->viewport.x + g->bounds.x +
                                             (s->anchor == g->text.end ? g->bounds.w : 0);
                            s->selection_y = s->viewport.y + g->bounds.y - s->scroll.y + 1;
                            break;
                        }
                    }
                (void)cui_text_view_select(s, e->modifiers & CUI_MOD_SHIFT ? s->anchor : at, at);
                cui_text_view_project_pointer(id, s, o.table_mode, e->x, e->y);
            } else if (s->selecting && (e->type != CUI_EVENT_POINTER_UP ||
                                        e->pointer_button == CUI_POINTER_PRIMARY)) {
                if (fabsf(e->x - s->press_x) > 2 || fabsf(e->y - s->press_y) > 2)
                    s->dragged = true;
                s->pointer_x = e->x;
                s->pointer_y = e->y;
                if (s->dragged) {
                    cui_text_view_project_pointer(id, s, o.table_mode, e->x, e->y);
                    cui_focus(ui, id);
                    (void)cui_cancel_event_activation(ui, i);
                    (void)cui_consume_event(ui, i);
                }
                if (e->type == CUI_EVENT_POINTER_UP)
                    s->selecting = false;
            }
            cui_invalidate(ui);
        }
        if (e->type == CUI_EVENT_KEY_DOWN && cui_focused(ui) == id) {
            cui_str text = cui_text_view_rendered_text(s);
            bool handled = false;
            if ((e->modifiers & (CUI_MOD_CTRL | CUI_MOD_META)) && e->key == CUI_KEY_A) {
                (void)cui_text_view_select(s, 0, text.length);
                handled = true;
            } else if ((e->modifiers & (CUI_MOD_CTRL | CUI_MOD_META)) && e->key == CUI_KEY_C &&
                       s->anchor != s->cursor) {
                if (o.clipboard && (o.clipboard->capabilities & CUI_PLATFORM_CLIPBOARD) &&
                    o.clipboard->clipboard_set) {
                    if (o.copy_format == CUI_TEXT_VIEW_COPY_SOURCE && !o.scrollable &&
                        s->format == CUI_TEXT_VIEW_MARKDOWN) {
                        size_t length;
                        if (cui_text_view_copy_document_selection(s, o.copy_format, o.copy_buffer,
                                                                  o.copy_capacity,
                                                                  &length) == CUI_TEXT_VIEW_OK)
                            (void)o.clipboard->clipboard_set(o.clipboard->user,
                                                             (cui_str){o.copy_buffer, length});
                    } else {
                        char scratch[CUI_TEXT_VIEW_BYTE_LIMIT];
                        char *buffer = o.copy_buffer ? o.copy_buffer : scratch;
                        size_t capacity = o.copy_buffer ? o.copy_capacity : sizeof scratch;
                        size_t length;
                        if (cui_text_view_copy_document_selection(s, CUI_TEXT_VIEW_COPY_PLAIN,
                                                                  buffer, capacity,
                                                                  &length) == CUI_TEXT_VIEW_OK &&
                            length)
                            (void)o.clipboard->clipboard_set(o.clipboard->user,
                                                             (cui_str){buffer, length});
                    }
                }
                handled = true;
            } else if (!(e->modifiers & (CUI_MOD_CTRL | CUI_MOD_META | CUI_MOD_ALT)) &&
                       (e->key == CUI_KEY_LEFT || e->key == CUI_KEY_RIGHT ||
                        e->key == CUI_KEY_HOME || e->key == CUI_KEY_END)) {
                size_t at = e->key == CUI_KEY_HOME   ? 0
                            : e->key == CUI_KEY_END  ? text.length
                            : e->key == CUI_KEY_LEFT ? cui_text_view_previous(text, s->cursor)
                                                     : cui_text_view_next(text, s->cursor);
                (void)cui_text_view_select(s, e->modifiers & CUI_MOD_SHIFT ? s->anchor : at, at);
                handled = true;
            } else if (e->key == CUI_KEY_ESCAPE && s->anchor != s->cursor) {
                (void)cui_text_view_select(s, s->cursor, s->cursor);
                handled = true;
            }
            if (handled) {
                (void)cui_consume_event(ui, i);
                cui_invalidate(ui);
            }
        }
    }
}
static bool cui_text_view_glyph_selected(const cui_text_view_state *s, const cui_text_view_glyph *g,
                                         cui_text_view_range selection) {
    if (!s->selection_projected)
        return cui_text_view_overlap(g->text, selection);
    if (g->flags & CUI_TEXT_VIEW_INLINE_IMAGE)
        return false;
    return cui_text_view_overlap(g->text, s->workspace.blocks[g->block].projected_selection);
}
static cui_color cui_text_view_foreground(const cui_text_view_state *s,
                                          const cui_text_view_glyph *g, cui_text_view_style style,
                                          cui_text_view_options o) {
    cui_text_view_span *span = &s->workspace.spans[g->span];
    cui_text_view_block *block = &s->workspace.blocks[g->block];
    cui_color color = span->link.end > span->link.begin    ? style.link
                      : block->kind == CUI_TEXT_VIEW_QUOTE ? style.muted
                                                           : style.foreground;
    if (o.highlighter && block->kind == CUI_TEXT_VIEW_CODE_BLOCK) {
        cui_str code = cui_text_view_slice(cui_text_view_rendered_text(s), block->text),
                lang = cui_text_view_slice(cui_text_view_source(s), block->resource);
        cui_color resolved;
        if (o.highlighter(code, lang, g->text.begin - block->text.begin, &resolved,
                          o.highlighter_user))
            color = resolved;
    }
    return color;
}
static bool cui_text_view_color_equal(cui_color a, cui_color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
static bool cui_text_view_visible(const cui_text_view_state *s, cui_rect r, float scroll,
                                  float viewport) {
    if (r.y + r.h <= scroll || r.y >= scroll + viewport)
        return false;
    /* Fit clamping removes a straddling normal line whole. An oversized line keeps its visible
     * part. */
    if (s->clamped && r.y + r.h > s->height && r.h <= s->height)
        return false;
    return true;
}
static void cui_text_view_quad(cui_context *ui, cui_id id, cui_rect r, cui_color color) {
    if (!color.a || r.w <= 0 || r.h <= 0)
        return;
    cui_begin_absolute_box(ui, id, (cui_box){.background = color}, r);
    cui_end_box(ui);
}
static void cui_text_view_link(cui_context *ui, cui_id id, cui_text_view_state *s,
                               cui_text_view_range uri, bool activated, cui_text_view_options o) {
    size_t count;
    const cui_event *events = cui_frame_events(ui, &count);
    char decoded[CUI_TEXT_VIEW_BYTE_LIMIT];
    cui_str url = {0};
    for (size_t i = 0; i < count; ++i) {
        if (cui_event_consumed(ui, i))
            continue;
        const cui_event *e = &events[i];
        bool click = activated && cui_event_activation(ui, i) == id;
        if (e->type == CUI_EVENT_POINTER_DOWN && e->pointer_button == CUI_POINTER_SECONDARY &&
            cui_event_target(ui, i) == id)
            s->secondary_link = id;
        if (e->type == CUI_EVENT_POINTER_UP && e->pointer_button == CUI_POINTER_SECONDARY &&
            s->secondary_link == id) {
            s->secondary_link = 0;
            click = cui_event_target(ui, i) == id;
        }
        if (!click)
            continue;
        if (!url.data) {
            size_t length;
            if (cui_text_view_decode_resource(s, uri, decoded, sizeof decoded, &length) !=
                CUI_TEXT_VIEW_OK)
                continue;
            url = (cui_str){decoded, length};
        }
        if (o.on_link) {
            o.on_link(url, e, o.link_user);
            (void)cui_consume_event(ui, i);
        } else if (o.open_url && e->pointer_button == CUI_POINTER_PRIMARY) {
            (void)o.open_url(url, o.link_user);
            (void)cui_consume_event(ui, i);
        }
    }
}
static void cui_text_view_paint_list_prefix(cui_context *ui, cui_id part,
                                            const cui_text_view_block *b, cui_rect r,
                                            float prefix_width, cui_text_view_style style) {
    if (b->task) {
        cui_begin_absolute_box(
            ui, part,
            (cui_box){.width = 14,
                      .height = 14,
                      .border_width = 1,
                      .border = style.foreground,
                      .background = b->checked ? style.foreground : (cui_color){0},
                      .padding = 1,
                      .align_x = CUI_ALIGN_CENTER,
                      .align_y = CUI_ALIGN_CENTER},
            (cui_rect){r.x, r.y + fmaxf(0, (style.line_height - 14) * .5f), 14, 14});
        if (b->checked) {
            float brightness = style.foreground.r * .2126f + style.foreground.g * .7152f +
                               style.foreground.b * .0722f;
            cui_checkmark(ui, 10,
                          brightness > 128 ? (cui_color){0, 0, 0, 255}
                                           : (cui_color){255, 255, 255, 255});
        }
        cui_end_box(ui);
    } else {
        cui_begin_absolute_box(ui, part, (cui_box){0},
                               (cui_rect){r.x, r.y, prefix_width, style.line_height});
        cui_text_line(ui, (cui_str){b->prefix_text, strlen(b->prefix_text)}, style.font_size,
                      style.foreground, style.line_height, (cui_text_style){0});
        cui_end_box(ui);
    }
}
static void cui_text_view_paint_range(cui_context *ui, cui_id id, const cui_theme *theme,
                                      cui_text_view_state *s, cui_text_view_style style,
                                      cui_text_view_options o, float viewport, size_t begin,
                                      size_t finish, float origin_x, float origin_y,
                                      bool actions_only) {
    float scroll = o.scrollable ? s->scroll.y : 0;
    for (size_t i = begin; i < finish; ++i) {
        cui_text_view_block *b = &s->workspace.blocks[i];
        if (actions_only)
            continue;
        cui_rect r = b->bounds;
        cui_rect line = b->kind == CUI_TEXT_VIEW_IMAGE && b->image_ready ? b->image_line : r;
        if (line.y + line.h <= scroll || line.y >= scroll + viewport)
            continue;
        r.x -= origin_x;
        r.y -= origin_y;
        cui_id part = id ^ UINT64_C(0x451ddbcb00000000) ^ (uint64_t)i;
        if (b->list_item && b->kind != CUI_TEXT_VIEW_LIST_ITEM &&
            b->kind != CUI_TEXT_VIEW_TABLE_CELL) {
            cui_rect prefix = r;
            if (b->kind == CUI_TEXT_VIEW_IMAGE && b->image_ready)
                prefix.y = b->image_line.y - origin_y;
            prefix.x -= b->list_prefix_width;
            cui_text_view_paint_list_prefix(ui, part ^ UINT64_C(0x70666978), b, prefix,
                                            b->list_prefix_width, style);
        }
        if (b->kind == CUI_TEXT_VIEW_CODE_BLOCK) {
            cui_text_view_quad(ui, part, r, style.code_background);
        } else if (b->kind == CUI_TEXT_VIEW_TABLE_CELL) {
            if (s->clamped && r.y + 4 + style.line_height > s->height &&
                style.line_height <= s->height)
                continue;
            cui_begin_absolute_box(
                ui, part,
                (cui_box){
                    .background = b->table_header ? style.code_background : style.table_background,
                    .border = style.border,
                    .custom_border_edges = true,
                    .border_edges =
                        {.left = o.table_mode == CUI_TEXT_VIEW_TABLE_WRAP && b->column == 0 ? 1 : 0,
                         .right = o.table_mode == CUI_TEXT_VIEW_TABLE_WRAP ||
                                          b->column + 1 < b->table_columns
                                      ? 1
                                      : 0,
                         .top = o.table_mode == CUI_TEXT_VIEW_TABLE_WRAP && b->row == 0 ? 1 : 0,
                         .bottom = o.table_mode == CUI_TEXT_VIEW_TABLE_WRAP ||
                                           b->row < s->workspace.blocks[finish - 1].row
                                       ? 1
                                       : 0}},
                r);
            cui_end_box(ui);
        } else if (b->kind == CUI_TEXT_VIEW_QUOTE)
            cui_text_view_quad(ui, part, (cui_rect){r.x, r.y, 3, r.h}, style.border);
        else if (b->kind == CUI_TEXT_VIEW_RULE)
            cui_text_view_quad(ui, part, r, style.border);
        else if (b->kind == CUI_TEXT_VIEW_LIST_ITEM || b->list_repeat_marker) {
            cui_text_view_paint_list_prefix(ui, part, b, r, b->text_inset, style);
        } else if (b->kind == CUI_TEXT_VIEW_IMAGE && b->image_ready) {
            cui_begin_absolute_box(ui, part, (cui_box){0}, r);
            (void)cui_paint_bitmap(ui, part ^ UINT64_C(0x696d616765000000),
                                   (cui_bitmap_paint){.bitmap = b->image, .opacity = 1},
                                   (cui_box){.width = r.w, .height = r.h});
            cui_end_box(ui);
        }
    }
    /* Link hit regions follow the full laid-out link line, independently of
     * selection/highlight/fade paint fragmentation. */
    for (size_t i = 0; i < s->glyph_count;) {
        cui_text_view_glyph *first = &s->workspace.glyphs[i];
        cui_text_view_span *span = &s->workspace.spans[first->span];
        if (actions_only || first->block < begin || first->block >= finish) {
            ++i;
            continue;
        }
        if (span->link.begin == span->link.end ||
            !cui_text_view_visible(s, first->bounds, scroll, viewport)) {
            ++i;
            continue;
        }
        size_t end = i + 1;
        while (end < s->glyph_count) {
            cui_text_view_glyph *g = &s->workspace.glyphs[end];
            cui_text_view_span *next = &s->workspace.spans[g->span];
            if (g->bounds.y != first->bounds.y || next->link.begin != span->link.begin ||
                next->link.end != span->link.end)
                break;
            ++end;
        }
        cui_text_view_glyph *last = &s->workspace.glyphs[end - 1];
        cui_rect r = first->bounds;
        r.w = last->bounds.x + last->bounds.w - r.x;
        cui_id link_id = id ^ UINT64_C(0x951f5d7100000000) ^ (uint64_t)span->link.begin ^
                         ((uint64_t)first->text.begin << 24);
        cui_text_view_block *block = &s->workspace.blocks[first->block];
        bool cell = block->kind == CUI_TEXT_VIEW_TABLE_CELL;
        r.x -= origin_x;
        r.y -= origin_y;
        cui_rect cell_bounds = block->bounds;
        cell_bounds.x -= origin_x;
        cell_bounds.y -= origin_y;
        if (cell) {
            cui_begin_absolute_box(ui, link_id ^ UINT64_C(0x63a471cf00000000),
                                   (cui_box){.clip = true}, cell_bounds);
            r.x -= cell_bounds.x;
            r.y -= cell_bounds.y;
        }
        cui_str label =
            cui_text_view_slice(cui_text_view_rendered_text(s),
                                (cui_text_view_range){first->text.begin, last->text.end});
        if (end == i + 1 && (span->flags & CUI_TEXT_VIEW_INLINE_IMAGE))
            label = cui_text_view_slice(cui_text_view_source(s), span->alt);
        cui_interaction interaction =
            cui_interact(ui, link_id, CUI_ROLE_LINK, label, o.disabled, false, 0);
        cui_set_semantic_details(ui, link_id, (cui_semantic_details){.cursor = CUI_CURSOR_POINTER});
        cui_begin_absolute_box(ui, link_id, (cui_box){0}, r);
        cui_end_box(ui);
        cui_text_view_link(ui, link_id, s, span->link, interaction.activated, o);
        if (cell)
            cui_end_box(ui);
        i = end;
    }
    cui_text_view_range selection = {s->anchor < s->cursor ? s->anchor : s->cursor,
                                     s->anchor > s->cursor ? s->anchor : s->cursor};
    for (size_t i = 0; i < s->glyph_count;) {
        cui_text_view_glyph *first = &s->workspace.glyphs[i];
        if (actions_only || first->block < begin || first->block >= finish ||
            !cui_text_view_visible(s, first->bounds, scroll, viewport)) {
            ++i;
            continue;
        }
        cui_color color = cui_text_view_foreground(s, first, style, o);
        bool selected = o.selectable && cui_text_view_glyph_selected(s, first, selection);
        float opacity = cui_text_view_stream_opacity(s, first->text.begin);
        size_t end = i + 1;
        while (end < s->glyph_count) {
            cui_text_view_glyph *g = &s->workspace.glyphs[end],
                                *previous = &s->workspace.glyphs[end - 1];
            if (g->span != first->span || g->bounds.y != first->bounds.y ||
                g->text.begin != previous->text.end ||
                cui_text_view_glyph_selected(s, g, selection) != selected ||
                cui_text_view_stream_opacity(s, g->text.begin) != opacity ||
                !cui_text_view_color_equal(cui_text_view_foreground(s, g, style, o), color))
                break;
            bool same = true;
            for (size_t h = 0; h < s->highlight_count; ++h)
                if (cui_text_view_overlap(g->text, s->workspace.highlights[h].range) !=
                    cui_text_view_overlap(first->text, s->workspace.highlights[h].range)) {
                    same = false;
                    break;
                }
            if (!same)
                break;
            ++end;
        }
        cui_text_view_glyph *last = &s->workspace.glyphs[end - 1];
        cui_rect r = first->bounds;
        r.w = last->bounds.x + last->bounds.w - r.x;
        cui_id leaf = id ^ UINT64_C(0x164b82d900000000) ^ (uint64_t)first->text.begin;
        cui_text_view_span *span = &s->workspace.spans[first->span];
        bool is_link = span->link.end > span->link.begin;
        cui_text_view_block *block = &s->workspace.blocks[first->block];
        bool cell = block->kind == CUI_TEXT_VIEW_TABLE_CELL;
        r.x -= origin_x;
        r.y -= origin_y;
        cui_rect cell_bounds = block->bounds;
        cell_bounds.x -= origin_x;
        cell_bounds.y -= origin_y;
        if (cell) {
            cui_begin_absolute_box(ui, leaf ^ UINT64_C(0x63a471cf00000000), (cui_box){.clip = true},
                                   cell_bounds);
            r.x -= cell_bounds.x;
            r.y -= cell_bounds.y;
        }
        if (first->flags & CUI_TEXT_VIEW_CODE)
            cui_text_view_quad(ui, leaf ^ UINT64_C(0x719ccbbb), r, style.code_background);
        for (size_t h = 0; h < s->highlight_count; ++h)
            if (cui_text_view_overlap(first->text, s->workspace.highlights[h].range))
                cui_text_view_quad(ui, leaf ^ UINT64_C(0x9000000000000000) ^ (uint64_t)h, r,
                                   s->workspace.highlights[h].background);
        if (first->flags & CUI_TEXT_VIEW_MARK)
            cui_text_view_quad(ui, leaf ^ UINT64_C(0x714c1d09), r, style.mark);
        if (selected)
            cui_text_view_quad(ui, leaf ^ UINT64_C(0x521e8438), r, style.selection);
        cui_begin_absolute_box(ui, leaf, (cui_box){.custom_opacity = true, .opacity = opacity}, r);
        if (span->flags & CUI_TEXT_VIEW_INLINE_IMAGE) {
            if (span->image_ready && span->image_bounds.w > 0 && span->image_bounds.h > 0) {
                cui_rect image = span->image_bounds;
                image.x -= first->bounds.x;
                image.y -= first->bounds.y;
                cui_begin_absolute_box(ui, leaf ^ UINT64_C(0x696d616765000000), (cui_box){0},
                                       image);
                cui_paint_bitmap(ui, leaf ^ UINT64_C(0x6269746d61700000),
                                 (cui_bitmap_paint){.bitmap = span->image, .opacity = 1},
                                 (cui_box){.width = image.w, .height = image.h});
                cui_end_box(ui);
            }
        } else
            cui_text_line(
                ui,
                cui_text_view_slice(cui_text_view_rendered_text(s),
                                    (cui_text_view_range){first->text.begin, last->text.end}),
                first->font_size, color, first->line_height,
                cui_text_view_text_style(first->flags));
        if (is_link && !(span->flags & CUI_TEXT_VIEW_INLINE_IMAGE))
            cui_text_view_quad(ui, leaf ^ UINT64_C(0x756e646572000000),
                               (cui_rect){0, r.h - 2, r.w, 1}, color);
        if ((first->flags & CUI_TEXT_VIEW_STRIKE) && !(span->flags & CUI_TEXT_VIEW_INLINE_IMAGE))
            cui_text_view_quad(ui, leaf ^ UINT64_C(0x737472696b000000),
                               (cui_rect){0, r.h * .5f, r.w, 1}, color);
        cui_end_box(ui);
        if (cell)
            cui_end_box(ui);

        i = end;
    }
    /* Actions paint after document glyphs so the code overlay covers underlying text. */
    for (size_t i = begin; i < finish; ++i) {
        cui_text_view_block *b = &s->workspace.blocks[i];
        cui_id part = id ^ UINT64_C(0x451ddbcb00000000) ^ (uint64_t)i;
        if (b->actions_height > 0 && ((b->kind == CUI_TEXT_VIEW_CODE_BLOCK && o.code_actions) ||
                                      (b->kind == CUI_TEXT_VIEW_TABLE_CELL && o.table_actions))) {
            bool code = b->kind == CUI_TEXT_VIEW_CODE_BLOCK;
            float y = code ? b->bounds.y + 8
                           : b->bounds.y + b->bounds.h + 4 +
                                 (o.table_mode != CUI_TEXT_VIEW_TABLE_WRAP ? 1 : 0);
            cui_rect action_bounds = {code ? 8 : 0, y, code ? fmaxf(1, s->width - 16) : s->width,
                                      b->actions_height};
            if (!code) {
                size_t first = i;
                while (first > begin &&
                       s->workspace.blocks[first - 1].kind == CUI_TEXT_VIEW_TABLE_CELL &&
                       s->workspace.blocks[first - 1].table == b->table)
                    --first;
                action_bounds.x = s->workspace.blocks[first].table_frame.x;
                action_bounds.w = s->workspace.blocks[first].table_frame.w;
            }
            if (action_bounds.y + action_bounds.h <= scroll || action_bounds.y >= scroll + viewport)
                continue;
            action_bounds.x -= origin_x;
            action_bounds.y -= origin_y;
            cui_id action_id = part ^ UINT64_C(0x513b5e17);
            cui_begin_absolute_box(ui, action_id,
                                   (cui_box){.align_x = code ? CUI_ALIGN_END : CUI_ALIGN_START},
                                   action_bounds);
            if (code)
                cui_begin_box(
                    ui, action_id ^ UINT64_C(0x813b17),
                    (cui_box){.background = style.code_background, .radius = theme->radius});
            if (b->kind == CUI_TEXT_VIEW_CODE_BLOCK)
                o.code_actions(ui, action_id, theme,
                               cui_text_view_slice(cui_text_view_rendered_text(s), b->text),
                               cui_text_view_slice(cui_text_view_source(s), b->resource),
                               o.actions_user);
            else
                o.table_actions(ui, action_id, theme, s, b->table, o.actions_user);
            if (code)
                cui_end_box(ui);
            cui_end_box(ui);
        }
    }
}
static cui_id cui_text_view_table_scroll_id(cui_id id, size_t first) {
    return id ^ UINT64_C(0x9c7a1d9300000000) ^ (uint64_t)first;
}
static void cui_text_view_paint(cui_context *ui, cui_id id, const cui_theme *theme,
                                cui_text_view_state *s, cui_text_view_style style,
                                cui_text_view_options o, float viewport) {
    for (size_t i = 0; i < s->block_count; ++i) {
        const cui_text_view_block *block = &s->workspace.blocks[i];
        if (block->kind != CUI_TEXT_VIEW_TABLE_CELL || !block->list_item)
            continue;
        cui_rect prefix = block->table_frame;
        prefix.x -= block->list_prefix_width;
        cui_id part = id ^ UINT64_C(0x451ddbcb00000000) ^ (uint64_t)i ^ UINT64_C(0x70666978);
        cui_text_view_paint_list_prefix(ui, part, block, prefix, block->list_prefix_width, style);
    }
    if (o.table_mode == CUI_TEXT_VIEW_TABLE_WRAP) {
        cui_text_view_paint_range(ui, id, theme, s, style, o, viewport, 0, s->block_count, 0, 0,
                                  false);
        return;
    }
    for (size_t first = 0; first < s->block_count;) {
        size_t end = first + 1;
        cui_text_view_block *block = &s->workspace.blocks[first];
        bool table = block->kind == CUI_TEXT_VIEW_TABLE_CELL;
        while (end < s->block_count &&
               (table ? s->workspace.blocks[end].kind == CUI_TEXT_VIEW_TABLE_CELL &&
                            s->workspace.blocks[end].table == block->table
                      : s->workspace.blocks[end].kind != CUI_TEXT_VIEW_TABLE_CELL))
            ++end;
        if (!table) {
            cui_text_view_paint_range(ui, id, theme, s, style, o, viewport, first, end, 0, 0,
                                      false);
        } else {
            cui_rect frame = block->table_frame;
            cui_id scroll_id = cui_text_view_table_scroll_id(id, first);
            cui_begin_absolute_box(ui, scroll_id ^ UINT64_C(0x26ad123),
                                   (cui_box){.clip = true,
                                             .background = style.table_background,
                                             .border = style.border,
                                             .border_width = 1,
                                             .padding = 1,
                                             .radius = theme->radius},
                                   frame);
            cui_scroll_options scroll = {.width = fmaxf(1, frame.w - 2),
                                         .height = fmaxf(1, frame.h - 2),
                                         .content_width = fmaxf(1, block->table_track_width - 2),
                                         .content_height = fmaxf(1, frame.h - 2),
                                         .horizontal = true,
                                         .trap_horizontal = true,
                                         .hide_scrollbars = true,
                                         .axis_lock = true,
                                         .gesture_lock = true,
                                         .disabled = o.disabled};
            cui_theme scroll_theme = *theme;
            scroll_theme.surface = style.table_background;
            cui_scroll_options bars = cui_scroll_resolve(
                theme, (cui_scroll_options){.horizontal = true,
                                            .disabled = o.disabled,
                                            .scrollbar_style = o.scrollbar_style});
            cui_scrollbar_style skin = *bars.scrollbar_style;
            skin.reduced_motion |= o.reduced_motion;
            bars.scrollbar_style = &skin;
            cui_rect bar_viewport = {0, 0, scroll.width, scroll.height};
            cui_size bar_content = {scroll.content_width, scroll.height};
            cui_base_scrollbar_overlay(ui, scroll_id, &block->table_scroll.x,
                                       &block->table_scroll.y, bar_viewport, bar_content, bars);
            if (cui_scrollable_begin(ui, scroll_id, &scroll_theme, &block->table_scroll, scroll)) {
                cui_text_view_options body = o;
                body.table_actions = NULL;
                cui_text_view_paint_range(ui, id, theme, s, style, body, viewport, first, end,
                                          frame.x + 1, frame.y + 1, false);
                cui_base_scrollable_finish(ui, scroll_id, &block->table_scroll, scroll);
                cui_set_box_scroll_offset(ui, scroll_id ^ UINT64_C(0x75b21b4d3cba0f89),
                                          -block->table_scroll.x, -block->table_scroll.y);
                bar_viewport.x = block->table_scroll.x;
                bar_viewport.y = block->table_scroll.y;
                cui_scrollbar_overlay_paint(ui, scroll_id, &scroll_theme, block->table_scroll.x,
                                            block->table_scroll.y, bar_viewport, bar_content, bars);
                cui_scrollable_end(ui, scroll_id, &scroll_theme, &block->table_scroll, scroll);
            }
            cui_end_box(ui);
            cui_text_view_paint_range(ui, id, theme, s, style, o, viewport, first, end, 0, 0, true);
        }
        first = end;
    }
}
bool cui_text_view(cui_context *ui, cui_id id, const cui_theme *theme, cui_text_view_state *s,
                   cui_text_view_options o) {
    if (!ui || !id || !theme || !s || !isfinite(o.width) || !isfinite(o.height) ||
        !isfinite(o.dt) || o.dt < 0 || !isfinite(o.code_actions_height) ||
        o.code_actions_height < 0 || !isfinite(o.table_actions_height) ||
        o.table_actions_height < 0)
        return false;
    if (o.custom_motion && !cui_text_view_set_motion(s, o.motion))
        return false;
    cui_text_view_style style = o.custom_style ? o.style : cui_text_view_style_from_theme(theme);
    float width = o.width > 0 ? o.width : s->viewport.w > 0 ? s->viewport.w : 320;
    float viewport = o.height > 0 ? o.height : s->viewport.h > 0 ? s->viewport.h : 240;
    cui_scroll_options body_scroll =
        cui_scroll_resolve(theme, (cui_scroll_options){.width = width,
                                                       .height = viewport,
                                                       .vertical = true,
                                                       .scrollbar_style = o.scrollbar_style});
    float content_width = o.scrollable ? cui_scroll_layout(body_scroll).viewport.width : width;
    s->stream_fade = o.stream_fade;
    s->reduced_motion = o.reduced_motion;
    char resource[CUI_TEXT_VIEW_BYTE_LIMIT];
    for (size_t i = 0; i < s->block_count; ++i)
        if (s->workspace.blocks[i].kind == CUI_TEXT_VIEW_IMAGE) {
            cui_text_view_block *b = &s->workspace.blocks[i];
            cui_bitmap bitmap = {0};
            size_t length;
            b->image_ready =
                o.image &&
                cui_text_view_decode_resource(s, b->resource, resource, sizeof resource, &length) ==
                    CUI_TEXT_VIEW_OK &&
                o.image((cui_str){resource, length}, &bitmap, o.image_user) &&
                cui_bitmap_valid(bitmap);
            b->image = bitmap;
        }
    for (size_t i = 0; i < s->span_count; ++i) {
        cui_text_view_span *span = &s->workspace.spans[i];
        if (!(span->flags & CUI_TEXT_VIEW_INLINE_IMAGE))
            continue;
        cui_bitmap bitmap = {0};
        size_t length;
        span->image_ready =
            o.image &&
            cui_text_view_decode_resource(s, span->resource, resource, sizeof resource, &length) ==
                CUI_TEXT_VIEW_OK &&
            o.image((cui_str){resource, length}, &bitmap, o.image_user) && bitmap.pixels &&
            bitmap.width && bitmap.height && bitmap.stride / 4 >= bitmap.width;
        span->image = bitmap;
    }
    if (cui_text_view_layout(
            s,
            (cui_text_view_layout_options){
                .width = content_width,
                .font_size = style.font_size,
                .line_height = style.line_height,
                .paragraph_gap = style.paragraph_gap,
                .max_lines = o.max_lines,
                .scrollable = o.scrollable,
                .table_mode = o.table_mode,
                .font_metrics = cui_text_view_core_font_metrics,
                .font_metrics_user = ui,
                .code_actions_height = o.code_actions ? o.code_actions_height : 0,
                .table_actions_height = o.table_actions ? o.table_actions_height : 0},
            cui_text_view_core_measure, ui) != CUI_TEXT_VIEW_OK)
        return false;
    if (!cui_begin_focus_scope(ui, id, false))
        return false;
    (void)cui_interact(ui, id, CUI_ROLE_GROUP, cui_text_view_rendered_text(s), o.disabled, false,
                       0);
    cui_set_semantic_details(
        ui, id,
        (cui_semantic_details){.focusable = o.selectable,
                               .pointer_activatable = o.selectable,
                               .readonly = true,
                               .cursor = o.selectable ? CUI_CURSOR_TEXT : CUI_CURSOR_DEFAULT});
    cui_set_tab_stop(ui, id, false);
    cui_text_view_reduce(ui, id, s, o);
    if (!o.scrollable) {
        viewport = s->height;
        s->scroll.y = 0;
    }
    if (s->reveal_pending) {
        cui_rect r = {0};
        bool found = false;
        for (size_t i = 0; i < s->glyph_count; ++i) {
            cui_text_view_glyph *g = &s->workspace.glyphs[i];
            if ((s->reveal.begin >= g->text.begin && s->reveal.begin < g->text.end) ||
                (s->reveal.begin == s->text_length && i + 1 == s->glyph_count)) {
                r = g->bounds;
                found = true;
                break;
            }
        }
        if (!found)
            for (size_t i = 0; i < s->block_count; ++i) {
                cui_text_view_block *b = &s->workspace.blocks[i];
                if (s->reveal.begin >= b->text.begin && s->reveal.begin <= b->text.end) {
                    r = b->bounds;
                    found = true;
                    break;
                }
            }
        if (found && o.table_mode != CUI_TEXT_VIEW_TABLE_WRAP)
            for (size_t first = 0; first < s->block_count;) {
                cui_text_view_block *b = &s->workspace.blocks[first];
                if (b->kind != CUI_TEXT_VIEW_TABLE_CELL) {
                    ++first;
                    continue;
                }
                size_t end = first + 1;
                while (end < s->block_count &&
                       s->workspace.blocks[end].kind == CUI_TEXT_VIEW_TABLE_CELL &&
                       s->workspace.blocks[end].table == b->table)
                    ++end;
                if (s->reveal.begin >= b->text.begin &&
                    s->reveal.begin <= s->workspace.blocks[end - 1].text.end) {
                    float x = r.x - b->table_frame.x - 1,
                          viewport_width = fmaxf(1, b->table_frame.w - 2);
                    if (x < b->table_scroll.x)
                        b->table_scroll.x = x;
                    else if (x + r.w > b->table_scroll.x + viewport_width)
                        b->table_scroll.x = x + r.w - viewport_width;
                    b->table_scroll.x = fminf(fmaxf(0, b->table_scroll.x),
                                              fmaxf(0, b->table_track_width - b->table_frame.w));
                    r.x -= b->table_scroll.x;
                    break;
                }
                first = end;
            }
        if (found && o.scrollable) {
            if (r.y < s->scroll.y)
                s->scroll.y = r.y;
            else if (r.y + r.h > s->scroll.y + viewport)
                s->scroll.y = r.y + r.h - viewport;
            s->reveal_pending = false;
        } else if (found) {
            if (o.on_reveal)
                o.on_reveal((cui_rect){s->viewport.x + r.x, s->viewport.y + r.y, r.w, r.h},
                            o.reveal_user);
            s->reveal_pending = false;
        }
    }
    if (s->selecting && o.scrollable && s->viewport.h > 0) {
        float speed = s->pointer_y < s->viewport.y + 12                   ? -120
                      : s->pointer_y > s->viewport.y + s->viewport.h - 12 ? 120
                                                                          : 0;
        float before = s->scroll.y;
        s->scroll.y = fminf(fmaxf(0, s->scroll.y + speed * o.dt), fmaxf(0, s->height - viewport));
        if (before != s->scroll.y) {
            cui_text_view_project_pointer(id, s, o.table_mode, s->pointer_x, s->pointer_y);
            cui_invalidate(ui);
        }
    }
    cui_begin_box(ui, id,
                  (cui_box){.width = o.width,
                            .height = o.scrollable ? o.height : s->height,
                            .fixed_height = !o.scrollable,
                            .clip = true,
                            .inert = o.disabled});
    (void)cui_track_bounds(ui, id, &s->viewport);
    if (o.scrollable) {
        cui_theme scroll_theme = *theme;
        scroll_theme.surface = (cui_color){0};
        scroll_theme.scrollbar = *body_scroll.scrollbar_style;
        scroll_theme.scrollbar.reduced_motion |= o.reduced_motion;
        cui_id scroll_id = id ^ UINT64_C(0x891dd9c052ad9f04);
        cui_scroll_options scroll = {.width = width,
                                     .height = viewport,
                                     .content_height = s->height,
                                     .vertical = true,
                                     .disabled = o.disabled,
                                     .scrollbar_style = &scroll_theme.scrollbar};
        if (cui_scrollable_begin(ui, scroll_id, &scroll_theme, &s->scroll, scroll)) {
            cui_text_view_paint(ui, id, theme, s, style, o, viewport);
            cui_scrollable_end_ex(ui, scroll_id, &scroll_theme, &s->scroll, scroll);
        }
    } else
        cui_text_view_paint(ui, id, theme, s, style, o, viewport);
    cui_end_box(ui);
    cui_end_focus_scope(ui);
    if (cui_text_view_advance(s, o.dt))
        cui_invalidate(ui);
    return true;
}
