/* SPDX-License-Identifier: MIT */
#include "cui/cui_text_view.h"
#include <math.h>
#include <string.h>
static bool cui_text_view_boundary(cui_str t, size_t p) {
    return p <= t.length && (p == t.length || !((unsigned char)t.data[p] >= 0x80 &&
                                                (unsigned char)t.data[p] < 0xc0));
}
static size_t cui_text_view_scalar(cui_str s, size_t i) {
    if (i >= s.length)
        return 0;
    unsigned c = (unsigned char)s.data[i];
    size_t n = c < 0x80                 ? 1
               : c >= 0xc2 && c <= 0xdf ? 2
               : c >= 0xe0 && c <= 0xef ? 3
               : c >= 0xf0 && c <= 0xf4 ? 4
                                        : 0;
    if (!n || n > s.length - i)
        return 0;
    for (size_t k = 1; k < n; ++k)
        if (((unsigned char)s.data[i + k] & 0xc0) != 0x80)
            return 0;
    if (n >= 3 && ((c == 0xe0 && (unsigned char)s.data[i + 1] < 0xa0) ||
                   (c == 0xed && (unsigned char)s.data[i + 1] >= 0xa0)))
        return 0;
    if (n == 4 && ((c == 0xf0 && (unsigned char)s.data[i + 1] < 0x90) ||
                   (c == 0xf4 && (unsigned char)s.data[i + 1] >= 0x90)))
        return 0;
    return n;
}
static bool cui_text_view_valid(cui_str s) {
    if (s.length && !s.data)
        return false;
    for (size_t i = 0; i < s.length;) {
        size_t n = cui_text_view_scalar(s, i);
        if (!n || s.data[i] == 0)
            return false;
        i += n;
    }
    return true;
}
static bool cui_text_view_range_valid(cui_str s, cui_text_view_range r) {
    return r.begin <= r.end && cui_text_view_boundary(s, r.begin) &&
           cui_text_view_boundary(s, r.end);
}
bool cui_text_view_init(cui_text_view_state *s, cui_text_view_workspace w, cui_text_view_format f) {
    if (!s || f < CUI_TEXT_VIEW_MARKDOWN || f > CUI_TEXT_VIEW_PLAIN || !w.source || !w.rendered ||
        !w.source_capacity || !w.rendered_capacity ||
        w.source_capacity > CUI_TEXT_VIEW_BYTE_LIMIT ||
        w.rendered_capacity > CUI_TEXT_VIEW_BYTE_LIMIT || !w.mapping ||
        w.mapping_capacity < w.rendered_capacity || !w.spans || !w.span_capacity ||
        w.span_capacity > CUI_TEXT_VIEW_NODE_LIMIT || !w.blocks || !w.block_capacity ||
        w.block_capacity > CUI_TEXT_VIEW_NODE_LIMIT || !w.glyphs || !w.glyph_capacity ||
        w.glyph_capacity > CUI_TEXT_VIEW_BYTE_LIMIT || (w.highlight_capacity && !w.highlights))
        return false;
    *s = (cui_text_view_state){.workspace = w, .format = f, .fade_duration = .35f};
    w.source[0] = w.rendered[0] = 0;
    return true;
}
cui_str cui_text_view_source(const cui_text_view_state *s) {
    return s ? (cui_str){s->workspace.source, s->source_length} : (cui_str){0};
}
cui_str cui_text_view_rendered_text(const cui_text_view_state *s) {
    return s ? (cui_str){s->workspace.rendered, s->text_length} : (cui_str){0};
}
typedef struct {
    cui_text_view_state *state;
    cui_str input;
    bool write, fade;
    size_t text, spans, blocks, block;
    cui_text_view_span last;
    cui_text_view_error error;
} cui_text_view_parser;
static void cui_text_view_emit(cui_text_view_parser *p, cui_str text, cui_text_view_range source,
                               unsigned flags, cui_text_view_range link) {
    cui_text_view_workspace *w = &p->state->workspace;
    if (p->error)
        return;
    if (text.length > w->rendered_capacity - 1 - p->text) {
        p->error = CUI_TEXT_VIEW_CAPACITY;
        return;
    }
    bool mapped = source.begin != SIZE_MAX;
    if (mapped && text.length) {
        bool same = !(flags & CUI_TEXT_VIEW_INLINE_IMAGE) && p->spans &&
                    p->last.block == p->block && p->last.flags == flags &&
                    p->last.link.begin == link.begin && p->last.link.end == link.end &&
                    p->last.text.end == p->text;
        if (!same) {
            if (p->spans >= w->span_capacity) {
                p->error = CUI_TEXT_VIEW_CAPACITY;
                return;
            }
            p->last = (cui_text_view_span){.text = {p->text, p->text + text.length},
                                           .source = source,
                                           .link = link,
                                           .flags = flags,
                                           .block = p->block};
            ++p->spans;
        } else {
            p->last.text.end = p->text + text.length;
            p->last.source.end = source.end;
        }
        if (p->write)
            w->spans[p->spans - 1] = p->last;
    }
    for (size_t i = 0; i < text.length; ++i) {
        if (p->write) {
            float born = -1;
            if (p->fade && p->text < p->state->text_length &&
                w->rendered[p->text] == text.data[i] && w->mapping[p->text].flags == flags)
                born = w->mapping[p->text].born;
            else if (p->fade)
                born = -2; /* Resolved scalar-safe after the complete bounded parse. */
            w->rendered[p->text] = text.data[i];
            w->mapping[p->text] =
                (cui_text_view_mapping){.source = source, .flags = flags, .born = born};
        }
        ++p->text;
    }
}
static void cui_text_view_gap(cui_text_view_parser *p, char c) {
    cui_text_view_emit(p, (cui_str){&c, 1}, (cui_text_view_range){SIZE_MAX, SIZE_MAX}, 0,
                       (cui_text_view_range){0});
}
static bool cui_text_view_block_begin(cui_text_view_parser *p, cui_text_view_block_kind kind,
                                      size_t start, unsigned level) {
    if (p->blocks >= p->state->workspace.block_capacity) {
        p->error = CUI_TEXT_VIEW_CAPACITY;
        return false;
    }
    p->block = p->blocks++;
    if (p->write) {
        cui_scroll_state scroll = {0};
        if (kind == CUI_TEXT_VIEW_TABLE_CELL && p->block < p->state->block_count) {
            const cui_text_view_block *old = &p->state->workspace.blocks[p->block];
            if (old->kind == kind && old->source.begin == start)
                scroll = old->table_scroll;
        }
        p->state->workspace.blocks[p->block] = (cui_text_view_block){.kind = kind,
                                                                     .text = {p->text, p->text},
                                                                     .source = {start, start},
                                                                     .level = level,
                                                                     .table_scroll = scroll,
                                                                     .list_parent = SIZE_MAX};
    }
    p->last.text.end = SIZE_MAX;
    return true;
}
static void cui_text_view_block_end(cui_text_view_parser *p, size_t end, bool newline) {
    if (p->write && p->block < p->blocks) {
        p->state->workspace.blocks[p->block].text.end = p->text;
        p->state->workspace.blocks[p->block].source.end = end;
    }
    if (newline)
        cui_text_view_gap(p, '\n');
}
static size_t cui_text_view_find(cui_str s, size_t begin, size_t end, const char *needle,
                                 size_t n) {
    if (n > end - begin)
        return SIZE_MAX;
    for (size_t i = begin; i <= end - n; ++i)
        if (!memcmp(s.data + i, needle, n))
            return i;
    return SIZE_MAX;
}
static unsigned cui_text_view_encode(unsigned c, char out[4]) {
    if (c < 0x80) {
        out[0] = (char)c;
        return 1;
    }
    if (c < 0x800) {
        out[0] = (char)(0xc0 | (c >> 6));
        out[1] = (char)(0x80 | (c & 63));
        return 2;
    }
    if (c < 0x10000) {
        out[0] = (char)(0xe0 | (c >> 12));
        out[1] = (char)(0x80 | ((c >> 6) & 63));
        out[2] = (char)(0x80 | (c & 63));
        return 3;
    }
    out[0] = (char)(0xf0 | (c >> 18));
    out[1] = (char)(0x80 | ((c >> 12) & 63));
    out[2] = (char)(0x80 | ((c >> 6) & 63));
    out[3] = (char)(0x80 | (c & 63));
    return 4;
}
static bool cui_text_view_entity_value(cui_str input, size_t *at, size_t end, unsigned *value) {
    size_t a = *at, limit = end - a > 17 ? a + 17 : end,
           b = cui_text_view_find(input, a, limit, ";", 1);
    if (b == SIZE_MAX || b - a > 16)
        return false;
    cui_str name = {input.data + a + 1, b - a - 1};
    unsigned c = 0;
    if (name.length == 3 && !memcmp(name.data, "amp", 3))
        c = '&';
    else if (name.length == 2 && !memcmp(name.data, "lt", 2))
        c = '<';
    else if (name.length == 2 && !memcmp(name.data, "gt", 2))
        c = '>';
    else if (name.length == 4 && !memcmp(name.data, "quot", 4))
        c = '"';
    else if (name.length == 4 && !memcmp(name.data, "apos", 4))
        c = '\'';
    else if (name.length == 4 && !memcmp(name.data, "nbsp", 4))
        c = 0xa0;
    else if (name.length > 1 && name.data[0] == '#') {
        size_t i = 1;
        unsigned base = 10;
        if (i < name.length && (name.data[i] == 'x' || name.data[i] == 'X')) {
            base = 16;
            ++i;
        }
        if (i == name.length)
            return false;
        for (; i < name.length; ++i) {
            unsigned char ch = (unsigned char)name.data[i];
            unsigned d = ch >= '0' && ch <= '9'   ? ch - '0'
                         : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10
                         : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10
                                                  : 99;
            if (d >= base || c > (0x10ffff - d) / base)
                return false;
            c = c * base + d;
        }
    }
    if (!c || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
        return false;
    *value = c;
    *at = b + 1;
    return true;
}
static bool cui_text_view_entity(cui_text_view_parser *p, size_t *at, size_t end, unsigned flags,
                                 cui_text_view_range link) {
    size_t begin = *at;
    unsigned value;
    if (!cui_text_view_entity_value(p->input, at, end, &value))
        return false;
    char encoded[4];
    unsigned n = cui_text_view_encode(value, encoded);
    cui_text_view_emit(p, (cui_str){encoded, n}, (cui_text_view_range){begin, *at}, flags, link);
    return true;
}
cui_text_view_error cui_text_view_decode_resource(const cui_text_view_state *s,
                                                  cui_text_view_range range, char *output,
                                                  size_t capacity, size_t *length) {
    if (!s || !length || (!output && capacity) ||
        !cui_text_view_range_valid(cui_text_view_source(s), range))
        return CUI_TEXT_VIEW_INVALID;
    cui_str input = cui_text_view_source(s);
    size_t required = 0;
    for (unsigned pass = 0; pass < 2; ++pass) {
        size_t written = 0;
        for (size_t at = range.begin; at < range.end;) {
            size_t start = at, end = at;
            unsigned value = 0;
            char encoded[4];
            const char *bytes;
            size_t n;
            if (s->format != CUI_TEXT_VIEW_PLAIN && input.data[at] == '&' &&
                cui_text_view_entity_value(input, &end, range.end, &value)) {
                n = cui_text_view_encode(value, encoded);
                bytes = encoded;
                at = end;
            } else {
                if (s->format == CUI_TEXT_VIEW_MARKDOWN && input.data[at] == '\\' &&
                    at + 1 < range.end) {
                    unsigned char ch = (unsigned char)input.data[at + 1];
                    if ((ch >= 33 && ch <= 47) || (ch >= 58 && ch <= 64) ||
                        (ch >= 91 && ch <= 96) || (ch >= 123 && ch <= 126))
                        ++at;
                }
                start = at;
                n = cui_text_view_scalar(input, at);
                bytes = input.data + start;
                at += n;
            }
            if (pass)
                memcpy(output + written, bytes, n);
            written += n;
        }
        if (!pass) {
            required = written;
            *length = required;
            if (!output || capacity <= required)
                return CUI_TEXT_VIEW_CAPACITY;
        }
    }
    output[required] = 0;
    return CUI_TEXT_VIEW_OK;
}
static bool cui_text_view_name(cui_str, const char *);
static bool cui_text_view_html_space(char);
static size_t cui_text_view_html_tag_end(cui_str, size_t, size_t);
static cui_text_view_range cui_text_view_attribute(cui_str, size_t, size_t, const char *);
static cui_text_view_image_length cui_text_view_html_image_length(cui_str, size_t, size_t,
                                                                  const char *);
static bool cui_text_view_link_syntax(cui_str input, size_t opening, size_t end, size_t *label_end,
                                      size_t *destination_end) {
    if (opening >= end || input.data[opening] != '[')
        return false;
    unsigned brackets = 1;
    size_t close = opening + 1;
    for (; close < end; ++close) {
        char c = input.data[close];
        if (c == '\\' && close + 1 < end) {
            ++close;
            continue;
        }
        if (c == '[' && ++brackets > 32)
            return false;
        if (c == ']' && --brackets == 0)
            break;
    }
    if (close + 1 >= end || input.data[close + 1] != '(')
        return false;
    unsigned parentheses = 1;
    bool angle = false;
    char quote = 0;
    for (size_t at = close + 2; at < end; ++at) {
        char c = input.data[at];
        if (c == '\\' && at + 1 < end) {
            ++at;
            continue;
        }
        if (quote) {
            if (c == quote)
                quote = 0;
            continue;
        }
        if (angle) {
            if (c == '>')
                angle = false;
            continue;
        }
        if (c == '<' && at == close + 2) {
            angle = true;
            continue;
        }
        if ((c == '"' || c == '\'') && at > close + 2 &&
            (input.data[at - 1] == ' ' || input.data[at - 1] == '\t')) {
            quote = c;
            continue;
        }
        if (c == '(' && ++parentheses > 32)
            return false;
        if (c == ')' && --parentheses == 0) {
            *label_end = close;
            *destination_end = at;
            return true;
        }
    }
    return false;
}
static cui_text_view_range cui_text_view_link_uri(cui_str input, size_t begin, size_t end) {
    while (begin < end && (input.data[begin] == ' ' || input.data[begin] == '\t'))
        ++begin;
    if (begin < end && input.data[begin] == '<') {
        size_t close = cui_text_view_find(input, begin + 1, end, ">", 1);
        if (close != SIZE_MAX)
            return (cui_text_view_range){begin + 1, close};
    }
    size_t at = begin;
    while (at < end && input.data[at] != ' ' && input.data[at] != '\t') {
        if (input.data[at] == '\\' && at + 1 < end)
            ++at;
        ++at;
    }
    return (cui_text_view_range){begin, at};
}
static void cui_text_view_inline(cui_text_view_parser *p, size_t a, size_t b, unsigned flags,
                                 cui_text_view_range link, unsigned depth) {
    if (depth > 16) {
        p->error = CUI_TEXT_VIEW_INVALID;
        return;
    }
    while (a < b && !p->error) {
        char c = p->input.data[a];
        if (c == '\\' && a + 1 < b) {
            size_t n = cui_text_view_scalar(p->input, a + 1);
            cui_text_view_emit(p, (cui_str){p->input.data + a + 1, n},
                               (cui_text_view_range){a, a + 1 + n}, flags, link);
            a += 1 + n;
            continue;
        }
        if (c == '&' && cui_text_view_entity(p, &a, b, flags, link))
            continue;
        if (c == '<') {
            size_t end = cui_text_view_html_tag_end(p->input, a, b);
            if (end != SIZE_MAX) {
                size_t name_end = a + 1;
                while (name_end < end && !cui_text_view_html_space(p->input.data[name_end]) &&
                       p->input.data[name_end] != '/')
                    ++name_end;
                if (cui_text_view_name((cui_str){p->input.data + a + 1, name_end - a - 1}, "img")) {
                    cui_text_view_range src =
                        cui_text_view_attribute(p->input, name_end, end, "src");
                    if (src.begin || src.end) {
                        cui_text_view_emit(p, (cui_str){" ", 1}, (cui_text_view_range){a, end + 1},
                                           flags | CUI_TEXT_VIEW_INLINE_IMAGE, link);
                        p->last.resource = src;
                        p->last.alt = cui_text_view_attribute(p->input, name_end, end, "alt");
                        p->last.image_width =
                            cui_text_view_html_image_length(p->input, name_end, end, "width");
                        p->last.image_height =
                            cui_text_view_html_image_length(p->input, name_end, end, "height");
                        if (p->write && !p->error)
                            p->state->workspace.spans[p->spans - 1] = p->last;
                    }
                    a = end + 1;
                    continue;
                }
            }
        }
        if (c == '`') {
            size_t close = cui_text_view_find(p->input, a + 1, b, "`", 1);
            if (close != SIZE_MAX) {
                for (size_t i = a + 1; i < close;) {
                    size_t n = cui_text_view_scalar(p->input, i);
                    cui_text_view_emit(p, (cui_str){p->input.data + i, n},
                                       (cui_text_view_range){i, i + n}, flags | CUI_TEXT_VIEW_CODE,
                                       link);
                    i += n;
                }
                a = close + 1;
                continue;
            }
        }
        if (c == '!' && a + 1 < b && p->input.data[a + 1] == '[') {
            size_t close, last;
            if (cui_text_view_link_syntax(p->input, a + 1, b, &close, &last)) {
                cui_text_view_emit(p, (cui_str){" ", 1}, (cui_text_view_range){a, last + 1},
                                   flags | CUI_TEXT_VIEW_INLINE_IMAGE, link);
                p->last.resource = cui_text_view_link_uri(p->input, close + 2, last);
                p->last.alt = (cui_text_view_range){a + 2, close};
                if (p->write && !p->error)
                    p->state->workspace.spans[p->spans - 1] = p->last;
                a = last + 1;
                continue;
            }
        }
        if (c == '[') {
            size_t close, last;
            if (cui_text_view_link_syntax(p->input, a, b, &close, &last)) {
                cui_text_view_inline(p, a + 1, close, flags,
                                     cui_text_view_link_uri(p->input, close + 2, last), depth + 1);
                a = last + 1;
                continue;
            }
        }
        if (c == '*' || c == '_' || c == '~') {
            size_t n = a + 1 < b && p->input.data[a + 1] == c ? 2 : 1;
            unsigned mark = n == 2 ? (c == '~' ? CUI_TEXT_VIEW_STRIKE : CUI_TEXT_VIEW_BOLD)
                                   : CUI_TEXT_VIEW_ITALIC;
            if (c != '~' || n == 2) {
                size_t close = cui_text_view_find(p->input, a + n, b, p->input.data + a, n);
                if (close != SIZE_MAX && close > a + n) {
                    cui_text_view_inline(p, a + n, close, flags | mark, link, depth + 1);
                    a = close + n;
                    continue;
                }
            }
        }
        size_t n = cui_text_view_scalar(p->input, a);
        cui_text_view_emit(p, (cui_str){p->input.data + a, n}, (cui_text_view_range){a, a + n},
                           flags, link);
        a += n;
    }
}
static size_t cui_text_view_line_end(cui_str s, size_t a) {
    while (a < s.length && s.data[a] != '\n' && s.data[a] != '\r')
        ++a;
    return a;
}
static size_t cui_text_view_next_line(cui_str s, size_t a) {
    if (a < s.length && s.data[a] == '\r')
        ++a;
    if (a < s.length && s.data[a] == '\n')
        ++a;
    return a;
}
static bool cui_text_view_table_separator(cui_str s, size_t a, size_t b) {
    bool dash = false;
    for (size_t i = a; i < b; ++i) {
        char c = s.data[i];
        if (c == '-')
            dash = true;
        else if (c != '|' && c != ':' && c != ' ' && c != '\t')
            return false;
    }
    return dash;
}
static cui_text_view_alignment cui_text_view_table_alignment(cui_str s, size_t a, size_t b,
                                                             unsigned col) {
    while (a < b && s.data[a] == ' ')
        ++a;
    if (a < b && s.data[a] == '|')
        ++a;
    for (unsigned c = 0; a < b; ++c) {
        size_t end = cui_text_view_find(s, a, b, "|", 1);
        if (end == SIZE_MAX)
            end = b;
        if (c == col) {
            while (a < end && s.data[a] == ' ')
                ++a;
            while (end > a && s.data[end - 1] == ' ')
                --end;
            bool first = a < end && s.data[a] == ':', last = end > a && s.data[end - 1] == ':';
            return last ? first ? CUI_TEXT_VIEW_ALIGN_CENTER : CUI_TEXT_VIEW_ALIGN_RIGHT
                        : CUI_TEXT_VIEW_ALIGN_LEFT;
        }
        a = end < b ? end + 1 : b;
    }
    return CUI_TEXT_VIEW_ALIGN_LEFT;
}
static bool cui_text_view_fence_close(cui_str input, size_t a, size_t b, char marker,
                                      size_t length) {
    unsigned indent = 0;
    while (a < b && input.data[a] == ' ' && indent < 3) {
        ++a;
        ++indent;
    }
    size_t start = a;
    while (a < b && input.data[a] == marker)
        ++a;
    if (a - start < length)
        return false;
    while (a < b && (input.data[a] == ' ' || input.data[a] == '\t'))
        ++a;
    return a == b;
}
static unsigned cui_text_view_setext(cui_str source, size_t begin, size_t end) {
    unsigned indent = 0;
    while (begin < end && source.data[begin] == ' ' && indent < 3) {
        ++begin;
        ++indent;
    }
    if (begin == end || (source.data[begin] != '=' && source.data[begin] != '-'))
        return 0;
    char marker = source.data[begin];
    do {
        ++begin;
    } while (begin < end && source.data[begin] == marker);
    while (begin < end && (source.data[begin] == ' ' || source.data[begin] == '\t'))
        ++begin;
    return begin == end ? marker == '=' ? 1 : 2 : 0;
}
static size_t cui_text_view_fenced_content(cui_text_view_parser *p, size_t a, size_t b, size_t next,
                                           unsigned indent) {
    char fence = p->input.data[a];
    size_t fence_length = 3;
    while (a + fence_length < b && p->input.data[a + fence_length] == fence)
        ++fence_length;
    size_t lang = a + fence_length, lang_end = b;
    while (lang < lang_end && p->input.data[lang] == ' ')
        ++lang;
    lang_end = lang;
    while (lang_end < b && p->input.data[lang_end] != ' ' && p->input.data[lang_end] != '\t')
        ++lang_end;
    if (p->write)
        p->state->workspace.blocks[p->block].resource = (cui_text_view_range){lang, lang_end};
    size_t i = next, end = i;
    while (i < p->input.length) {
        size_t e = cui_text_view_line_end(p->input, i);
        size_t content = i;
        for (unsigned removed = 0; removed < indent && content < e && p->input.data[content] == ' ';
             ++removed)
            ++content;
        if (cui_text_view_fence_close(p->input, content, e, fence, fence_length)) {
            end = cui_text_view_next_line(p->input, e);
            break;
        }
        for (size_t j = content; j < e;) {
            size_t n = cui_text_view_scalar(p->input, j);
            cui_text_view_emit(p, (cui_str){p->input.data + j, n}, (cui_text_view_range){j, j + n},
                               CUI_TEXT_VIEW_CODE, (cui_text_view_range){0});
            j += n;
        }
        size_t following = cui_text_view_next_line(p->input, e);
        size_t following_end = cui_text_view_line_end(p->input, following);
        size_t following_content = following;
        for (unsigned removed = 0; removed < indent && following_content < following_end &&
                                   p->input.data[following_content] == ' ';
             ++removed)
            ++following_content;
        if (following < p->input.length &&
            !cui_text_view_fence_close(p->input, following_content, following_end, fence,
                                       fence_length))
            cui_text_view_emit(p, (cui_str){"\n", 1},
                               (cui_text_view_range){e, cui_text_view_next_line(p->input, e)},
                               CUI_TEXT_VIEW_CODE, (cui_text_view_range){0});
        i = cui_text_view_next_line(p->input, e);
        end = i;
    }
    cui_text_view_block_end(p, end, true);
    return end;
}
static void cui_text_view_markdown(cui_text_view_parser *p) {
    size_t a = 0;
    unsigned table = 0, list_number = 0;
    struct {
        unsigned indent, content_indent, number, start, copy_indent;
        size_t item;
        bool ordered, nested;
    } lists[CUI_TEXT_VIEW_LIST_DEPTH_LIMIT];
    unsigned list_count = 0;
    while (a < p->input.length && !p->error) {
        size_t b = cui_text_view_line_end(p->input, a), next = cui_text_view_next_line(p->input, b),
               start = a;
        unsigned indent = 0;
        while (a < b && (p->input.data[a] == ' ' || p->input.data[a] == '\t')) {
            indent = p->input.data[a] == '\t' ? (indent + 4) & ~3u : indent + 1;
            ++a;
        }
        size_t leading = a;
        if (a == b) {
            a = next;
            continue;
        }
        if (b - a >= 3 &&
            (!memcmp(p->input.data + a, "```", 3) || !memcmp(p->input.data + a, "~~~", 3))) {
            while (list_count > 1 && indent < lists[list_count - 1].content_indent)
                --list_count;
            bool nested_code = list_count && indent >= lists[list_count - 1].content_indent;
            if (!nested_code)
                list_count = 0;
            if (!cui_text_view_block_begin(p, CUI_TEXT_VIEW_CODE_BLOCK, start, 0))
                return;
            if (p->write) {
                cui_text_view_block *block = &p->state->workspace.blocks[p->block];
                if (nested_code) {
                    unsigned depth = list_count - 1;
                    const cui_text_view_block *owner =
                        &p->state->workspace.blocks[lists[depth].item];
                    block->list_continuation = true;
                    block->list_parent = lists[depth].item;
                    block->list_depth = depth;
                    block->list_source_indent =
                        owner->list_source_indent +
                        (owner->list_ordered ? (unsigned)strlen(owner->marker_text) + 1 : 2);
                }
            }
            if (nested_code)
                lists[list_count - 1].nested = false;
            a = cui_text_view_fenced_content(p, a, b, next, indent);
            continue;
        }
        size_t next_end = cui_text_view_line_end(p->input, next);
        cui_text_view_range marker = {0};
        unsigned level = 0;
        while (a + level < b && p->input.data[a + level] == '#' && level < 6)
            ++level;
        cui_text_view_block_kind kind = CUI_TEXT_VIEW_PARAGRAPH;
        if (level && a + level < b && p->input.data[a + level] == ' ') {
            kind = CUI_TEXT_VIEW_HEADING;
            a += level + 1;
        } else {
            level = 0;
            if (p->input.data[a] == '>') {
                kind = CUI_TEXT_VIEW_QUOTE;
                ++a;
                if (a < b && p->input.data[a] == ' ')
                    ++a;
            } else if (b - a >= 2 &&
                       (p->input.data[a] == '-' || p->input.data[a] == '+' ||
                        p->input.data[a] == '*') &&
                       p->input.data[a + 1] == ' ') {
                kind = CUI_TEXT_VIEW_LIST_ITEM;
                level = 0;
                a += 2;
            } else {
                size_t digit = a;
                while (digit < b && p->input.data[digit] >= '0' && p->input.data[digit] <= '9')
                    ++digit;
                if (digit > a && digit - a <= 9 && digit + 1 < b && p->input.data[digit] == '.' &&
                    p->input.data[digit + 1] == ' ') {
                    kind = CUI_TEXT_VIEW_LIST_ITEM;
                    level = 1;
                    marker = (cui_text_view_range){a, digit + 1};
                    a = digit + 2;
                }
            }
        }
        if (b - a >= 3 &&
            ((p->input.data[a] == '-' && cui_text_view_table_separator(p->input, a, b)) ||
             !memcmp(p->input.data + a, "***", 3)))
            kind = CUI_TEXT_VIEW_RULE;
        size_t marker_content = a;
        bool task = kind == CUI_TEXT_VIEW_LIST_ITEM && b - a >= 3 && p->input.data[a] == '[' &&
                    p->input.data[a + 2] == ']' &&
                    (a + 3 == b || p->input.data[a + 3] == ' ' || p->input.data[a + 3] == '\t') &&
                    (p->input.data[a + 1] == ' ' || p->input.data[a + 1] == 'x' ||
                     p->input.data[a + 1] == 'X');
        bool checked = task && p->input.data[a + 1] != ' ';
        if (task) {
            a += 3;
            if (a < b)
                ++a;
        }
        size_t image_close = SIZE_MAX, image_last = SIZE_MAX;
        if (b - a >= 4 && p->input.data[a] == '!' && p->input.data[a + 1] == '[') {
            if (!cui_text_view_link_syntax(p->input, a + 1, b, &image_close, &image_last))
                image_close = image_last = SIZE_MAX;
            if (image_last != SIZE_MAX) {
                size_t remainder = image_last + 1;
                while (remainder < b &&
                       (p->input.data[remainder] == ' ' || p->input.data[remainder] == '\t'))
                    ++remainder;
                if (remainder != b)
                    image_last = SIZE_MAX;
            }
        }
        unsigned heading_prefix = 0;
        while (heading_prefix < 6 && a + heading_prefix < b &&
               p->input.data[a + heading_prefix] == '#')
            ++heading_prefix;
        bool block_prefix =
            a < b && (p->input.data[a] == '>' || (heading_prefix && a + heading_prefix < b &&
                                                  p->input.data[a + heading_prefix] == ' '));
        bool table_candidate =
            image_last == SIZE_MAX && !block_prefix &&
            (kind == CUI_TEXT_VIEW_PARAGRAPH || kind == CUI_TEXT_VIEW_LIST_ITEM) &&
            cui_text_view_find(p->input, a, b, "|", 1) != SIZE_MAX && next < p->input.length &&
            cui_text_view_table_separator(p->input, next, next_end);
        unsigned depth = 0, list_start = 0, copy_indent = 0;
        size_t parent = SIZE_MAX;
        bool repeat_marker = false;
        if (kind == CUI_TEXT_VIEW_LIST_ITEM) {
            while (list_count && indent < lists[list_count - 1].indent)
                --list_count;
            bool fresh = false;
            if (!list_count) {
                list_count = 1;
                fresh = true;
            } else if (indent >= lists[list_count - 1].content_indent) {
                if (list_count == CUI_TEXT_VIEW_LIST_DEPTH_LIMIT) {
                    p->error = CUI_TEXT_VIEW_INVALID;
                    return;
                }
                lists[list_count - 1].nested = true;
                ++list_count;
                fresh = true;
            }
            depth = list_count - 1;
            if (!fresh && lists[depth].ordered != (level != 0))
                fresh = true;
            if (fresh) {
                list_number = 0;
                if (level)
                    for (size_t i = marker.begin; i + 1 < marker.end; ++i)
                        list_number = list_number * 10 + (unsigned)(p->input.data[i] - '0');
                lists[depth].indent = indent;
                lists[depth].ordered = level != 0;
                lists[depth].number = lists[depth].start = list_number;
            } else if (level)
                ++lists[depth].number;
            lists[depth].nested = false;
            lists[depth].content_indent = indent + (unsigned)(marker_content - leading);
            if (depth) {
                parent = lists[depth - 1].item;
                unsigned digits = 1, ordinal = lists[depth - 1].number;
                while (ordinal >= 10) {
                    ++digits;
                    ordinal /= 10;
                }
                copy_indent =
                    lists[depth - 1].copy_indent + (lists[depth - 1].ordered ? digits + 2 : 2);
            }
            lists[depth].copy_indent = copy_indent;
            list_start = lists[depth].start;
            list_number = lists[depth].number;
        } else {
            while ((kind == CUI_TEXT_VIEW_PARAGRAPH || kind == CUI_TEXT_VIEW_HEADING ||
                    kind == CUI_TEXT_VIEW_QUOTE || kind == CUI_TEXT_VIEW_IMAGE) &&
                   list_count > 1 && indent < lists[list_count - 1].content_indent)
                --list_count;
            if ((kind == CUI_TEXT_VIEW_PARAGRAPH || kind == CUI_TEXT_VIEW_HEADING ||
                 kind == CUI_TEXT_VIEW_QUOTE || kind == CUI_TEXT_VIEW_IMAGE) &&
                list_count && indent >= lists[list_count - 1].content_indent) {
                depth = list_count - 1;
                parent = lists[depth].item;
                repeat_marker =
                    kind == CUI_TEXT_VIEW_PARAGRAPH && !table_candidate && lists[depth].nested;
                lists[depth].nested = false;
                unsigned digits = 1, ordinal = lists[depth].number;
                while (ordinal >= 10) {
                    ++digits;
                    ordinal /= 10;
                }
                copy_indent = lists[depth].copy_indent + (lists[depth].ordered ? digits + 2 : 2);
            } else
                list_count = 0;
        }
        bool continuation = kind != CUI_TEXT_VIEW_LIST_ITEM && parent != SIZE_MAX;
        size_t block_start = table_candidate ? a + (p->input.data[a] == '|') : start;
        if (!cui_text_view_block_begin(p, table_candidate ? CUI_TEXT_VIEW_TABLE_CELL : kind,
                                       block_start, level))
            return;
        if (kind == CUI_TEXT_VIEW_LIST_ITEM)
            lists[depth].item = p->block;
        if (p->write) {
            cui_text_view_block *block = &p->state->workspace.blocks[p->block];
            block->list_item = kind == CUI_TEXT_VIEW_LIST_ITEM;
            block->list_ordered = kind == CUI_TEXT_VIEW_LIST_ITEM && level != 0;
            block->list_continuation = continuation;
            block->list_depth = depth;
            block->list_parent = parent;
            block->list_start = list_start;
            block->list_source_indent = copy_indent;
            block->task = task;
            block->checked = checked;
            block->list_marker = marker;
            block->list_number = list_number;
            block->list_repeat_marker = repeat_marker;
            if (repeat_marker) {
                const cui_text_view_block *owner = &p->state->workspace.blocks[parent];
                block->list_ordered = owner->list_ordered;
                block->task = owner->task;
                block->checked = owner->checked;
                block->level = owner->level;
                block->list_number = owner->list_number;
                block->list_start = owner->list_start;
                memcpy(block->marker_text, owner->marker_text, sizeof block->marker_text);
            }
            if (kind == CUI_TEXT_VIEW_LIST_ITEM && level) {
                char digits[10];
                unsigned number = list_number, count = 0;
                do {
                    digits[count++] = (char)('0' + number % 10);
                    number /= 10;
                } while (number);
                for (unsigned i = 0; i < count; ++i)
                    block->marker_text[i] = digits[count - i - 1];
                block->marker_text[count] = '.';
                block->marker_text[count + 1] = 0;
            }
        }
        if (table_candidate) {
            ++table;
            unsigned row = 0;
            size_t table_owner = p->block;
            size_t line = a, line_end = b;
            do {
                size_t cell = line;
                while (cell < line_end && p->input.data[cell] == ' ')
                    ++cell;
                if (cell < line_end && p->input.data[cell] == '|')
                    ++cell;
                unsigned col = 0;
                while (cell < line_end) {
                    size_t end = cui_text_view_find(p->input, cell, line_end, "|", 1);
                    if (end == SIZE_MAX)
                        end = line_end;
                    size_t l = cell, r = end;
                    while (l < r && p->input.data[l] == ' ')
                        ++l;
                    while (r > l && p->input.data[r - 1] == ' ')
                        --r;
                    bool first_cell = row == 0 && col == 0;
                    if (!first_cell &&
                        !cui_text_view_block_begin(p, CUI_TEXT_VIEW_TABLE_CELL, cell, 0))
                        return;
                    if (p->write) {
                        cui_text_view_block *block = &p->state->workspace.blocks[p->block];
                        block->kind = CUI_TEXT_VIEW_TABLE_CELL;
                        block->table = table;
                        if (!first_cell && (continuation || kind == CUI_TEXT_VIEW_LIST_ITEM)) {
                            const cui_text_view_block *owner =
                                &p->state->workspace.blocks[table_owner];
                            block->list_continuation = true;
                            block->list_parent =
                                kind == CUI_TEXT_VIEW_LIST_ITEM ? table_owner : parent;
                            block->list_depth = depth;
                            block->list_source_indent =
                                kind == CUI_TEXT_VIEW_LIST_ITEM
                                    ? owner->list_source_indent +
                                          (owner->list_ordered
                                               ? (unsigned)strlen(owner->marker_text) + 1
                                               : 2)
                                    : copy_indent;
                        }
                        block->row = row;
                        block->column = col;
                        block->table_header = row == 0;
                        block->alignment =
                            cui_text_view_table_alignment(p->input, next, next_end, col);
                    }
                    cui_text_view_inline(p, l, r, 0, (cui_text_view_range){0}, 0);
                    cui_text_view_block_end(p, end, false);
                    ++col;
                    if (end < line_end && end + 1 < line_end)
                        cui_text_view_gap(p, ' ');
                    cell = end < line_end ? end + 1 : line_end;
                }
                cui_text_view_gap(p, '\n');
                ++row;
                line = cui_text_view_next_line(p->input, line_end);
                if (row == 1)
                    line = cui_text_view_next_line(p->input, next_end);
                line_end = cui_text_view_line_end(p->input, line);
            } while (line < p->input.length &&
                     cui_text_view_find(p->input, line, line_end, "|", 1) != SIZE_MAX);
            a = line;
            continue;
        }
        if (image_last != SIZE_MAX) {
            if (p->write) {
                cui_text_view_block *block = &p->state->workspace.blocks[p->block];
                block->resource = cui_text_view_link_uri(p->input, image_close + 2, image_last);
            }
            cui_text_view_inline(p, a, image_last + 1, 0, (cui_text_view_range){0}, 0);
            cui_text_view_block_end(p, next, true);
            a = next;
            continue;
        }
        if (kind == CUI_TEXT_VIEW_LIST_ITEM && b - a >= 3 &&
            (!memcmp(p->input.data + a, "```", 3) || !memcmp(p->input.data + a, "~~~", 3))) {
            if (p->write)
                p->state->workspace.blocks[p->block].kind = CUI_TEXT_VIEW_CODE_BLOCK;
            a = cui_text_view_fenced_content(p, a, b, next, lists[depth].content_indent);
            continue;
        }
        if (kind == CUI_TEXT_VIEW_LIST_ITEM && a < b) {
            unsigned heading = 0;
            while (heading < 6 && a + heading < b && p->input.data[a + heading] == '#')
                ++heading;
            if (heading && a + heading < b && p->input.data[a + heading] == ' ') {
                kind = CUI_TEXT_VIEW_HEADING;
                a += heading + 1;
                if (p->write) {
                    p->state->workspace.blocks[p->block].kind = kind;
                    p->state->workspace.blocks[p->block].level = heading;
                }
            } else if (p->input.data[a] == '>') {
                kind = CUI_TEXT_VIEW_QUOTE;
                ++a;
                if (a < b && p->input.data[a] == ' ')
                    ++a;
                if (p->write)
                    p->state->workspace.blocks[p->block].kind = kind;
            }
        }
        if (kind != CUI_TEXT_VIEW_RULE) {
            size_t inline_end = b;
            if ((kind == CUI_TEXT_VIEW_PARAGRAPH || kind == CUI_TEXT_VIEW_LIST_ITEM ||
                 kind == CUI_TEXT_VIEW_QUOTE) &&
                b >= a + 2 && p->input.data[b - 1] == ' ' && p->input.data[b - 2] == ' ')
                inline_end -= 2;
            cui_text_view_inline(p, a, inline_end, 0, (cui_text_view_range){0}, 0);
        }
        if (kind == CUI_TEXT_VIEW_PARAGRAPH || kind == CUI_TEXT_VIEW_LIST_ITEM ||
            kind == CUI_TEXT_VIEW_QUOTE) {
            while (next < p->input.length) {
                size_t end = cui_text_view_line_end(p->input, next), first = next;
                unsigned next_indent = 0;
                while (first < end &&
                       (p->input.data[first] == ' ' || p->input.data[first] == '\t')) {
                    next_indent =
                        p->input.data[first] == '\t' ? (next_indent + 4) & ~3u : next_indent + 1;
                    ++first;
                }
                if ((continuation || (kind == CUI_TEXT_VIEW_QUOTE && list_count)) &&
                    next_indent < lists[depth].content_indent)
                    break;
                if (first == end)
                    break;
                if (kind == CUI_TEXT_VIEW_QUOTE) {
                    if (p->input.data[first] != '>')
                        break;
                    ++first;
                    if (first < end && p->input.data[first] == ' ')
                        ++first;
                    if (first == end)
                        break;
                }
                unsigned setext = kind == CUI_TEXT_VIEW_PARAGRAPH && !continuation
                                      ? cui_text_view_setext(p->input, next, end)
                                      : 0;
                if (setext) {
                    kind = CUI_TEXT_VIEW_HEADING;
                    if (p->write) {
                        p->state->workspace.blocks[p->block].kind = kind;
                        p->state->workspace.blocks[p->block].level = setext;
                    }
                    b = end;
                    next = cui_text_view_next_line(p->input, end);
                    break;
                }
                char lead = p->input.data[first];
                size_t digits = first;
                while (digits < end && p->input.data[digits] >= '0' && p->input.data[digits] <= '9')
                    ++digits;
                bool list = ((lead == '-' || lead == '+' || lead == '*') && first + 1 < end &&
                             p->input.data[first + 1] == ' ') ||
                            (digits > first && digits + 1 < end && p->input.data[digits] == '.' &&
                             p->input.data[digits + 1] == ' ');
                if (lead == '#' || lead == '>' || list ||
                    (end >= first + 3 && (!memcmp(p->input.data + first, "```", 3) ||
                                          !memcmp(p->input.data + first, "~~~", 3) ||
                                          !memcmp(p->input.data + first, "***", 3))) ||
                    (end >= first + 2 && !memcmp(p->input.data + first, "![", 2)))
                    break;
                size_t after = cui_text_view_next_line(p->input, end);
                if (cui_text_view_find(p->input, first, end, "|", 1) != SIZE_MAX &&
                    after < p->input.length &&
                    cui_text_view_table_separator(p->input, after,
                                                  cui_text_view_line_end(p->input, after)))
                    break;
                bool hard = b >= 2 && p->input.data[b - 1] == ' ' && p->input.data[b - 2] == ' ';
                cui_text_view_emit(p, (cui_str){hard ? "\n" : " ", 1},
                                   (cui_text_view_range){b, next}, 0, (cui_text_view_range){0});
                size_t inline_end = end;
                if (end >= first + 2 && p->input.data[end - 1] == ' ' &&
                    p->input.data[end - 2] == ' ')
                    inline_end -= 2;
                cui_text_view_inline(p, first, inline_end, 0, (cui_text_view_range){0}, 0);
                b = end;
                next = after;
            }
        }
        cui_text_view_block_end(p, next, true);
        a = next;
    }
}
static bool cui_text_view_name(cui_str s, const char *name) {
    size_t n = strlen(name);
    if (s.length != n)
        return false;
    for (size_t i = 0; i < n; ++i) {
        char c = s.data[i];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c + 32);
        if (c != name[i])
            return false;
    }
    return true;
}
static bool cui_text_view_html_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}
static size_t cui_text_view_html_tag_end(cui_str input, size_t start, size_t end) {
    char quote = 0;
    for (size_t at = start + 1; at < end; ++at) {
        char c = input.data[at];
        if (quote) {
            if (c == quote)
                quote = 0;
        } else if (c == '\'' || c == '\"')
            quote = c;
        else if (c == '>')
            return at;
    }
    return SIZE_MAX;
}
static cui_text_view_range cui_text_view_attribute(cui_str input, size_t a, size_t b,
                                                   const char *name) {
    while (a < b) {
        while (a < b && (cui_text_view_html_space(input.data[a]) || input.data[a] == '/'))
            ++a;
        size_t start = a;
        while (a < b && input.data[a] != '=' && !cui_text_view_html_space(input.data[a]))
            ++a;
        size_t end = a;
        while (a < b && cui_text_view_html_space(input.data[a]))
            ++a;
        if (a == b || input.data[a] != '=') {
            if (a == start)
                ++a;
            continue;
        }
        ++a;
        while (a < b && cui_text_view_html_space(input.data[a]))
            ++a;
        char quote = a < b && (input.data[a] == '\'' || input.data[a] == '"') ? input.data[a++] : 0;
        size_t value = a;
        while (a < b && (quote ? input.data[a] != quote : !cui_text_view_html_space(input.data[a])))
            ++a;
        if (cui_text_view_name((cui_str){input.data + start, end - start}, name))
            return (cui_text_view_range){value, a};
        if (quote && a < b)
            ++a;
    }
    return (cui_text_view_range){0};
}
static cui_text_view_image_length cui_text_view_image_length_parse(cui_str input,
                                                                   cui_text_view_range r) {
    cui_text_view_image_length result = {0};
    size_t a = r.begin, b = r.end;
    while (a < b && (input.data[a] == ' ' || input.data[a] == '\t'))
        ++a;
    while (b > a && (input.data[b - 1] == ' ' || input.data[b - 1] == '\t'))
        --b;
    if (b - a > 64 || a == b)
        return result;
    if (input.data[b - 1] == '%') {
        result.relative = true;
        --b;
    } else if (b - a >= 2 && input.data[b - 2] == 'p' && input.data[b - 1] == 'x')
        b -= 2;
    if (a < b && input.data[a] == '+')
        ++a;
    double value = 0, fraction = .1;
    bool digit = false, dot = false;
    for (; a < b; ++a) {
        char c = input.data[a];
        if (c == '.' && !dot) {
            dot = true;
            continue;
        }
        if (c < '0' || c > '9')
            break;
        digit = true;
        if (dot) {
            value += (c - '0') * fraction;
            fraction *= .1;
        } else
            value = value * 10 + c - '0';
    }
    if (!digit)
        return (cui_text_view_image_length){0};
    if (a < b && (input.data[a] == 'e' || input.data[a] == 'E')) {
        ++a;
        bool negative = a < b && input.data[a] == '-';
        if (a < b && (input.data[a] == '+' || input.data[a] == '-'))
            ++a;
        unsigned exponent = 0;
        size_t first = a;
        while (a < b && input.data[a] >= '0' && input.data[a] <= '9') {
            exponent = exponent * 10 + (unsigned)(input.data[a++] - '0');
            if (exponent > 38)
                return (cui_text_view_image_length){0};
        }
        if (a == first)
            return (cui_text_view_image_length){0};
        value *= pow(10, negative ? -(double)exponent : (double)exponent);
    }
    if (a != b || !isfinite(value) || value > 10000000)
        return (cui_text_view_image_length){0};
    result.value = (float)(result.relative ? value * .01 : value);
    result.specified = true;
    return result;
}
static cui_text_view_image_length cui_text_view_html_image_length(cui_str input, size_t a, size_t b,
                                                                  const char *name) {
    cui_text_view_image_length attribute =
        cui_text_view_image_length_parse(input, cui_text_view_attribute(input, a, b, name));
    if (attribute.specified)
        return attribute;
    cui_text_view_range style = cui_text_view_attribute(input, a, b, "style");
    cui_text_view_image_length result = {0};
    for (size_t start = style.begin; start < style.end;) {
        size_t end = cui_text_view_find(input, start, style.end, ";", 1);
        if (end == SIZE_MAX)
            end = style.end;
        size_t colon = cui_text_view_find(input, start, end, ":", 1);
        if (colon != SIZE_MAX) {
            size_t key = start, key_end = colon;
            while (key < key_end && (input.data[key] == ' ' || input.data[key] == '\t'))
                ++key;
            while (key_end > key &&
                   (input.data[key_end - 1] == ' ' || input.data[key_end - 1] == '\t'))
                --key_end;
            if (cui_text_view_name((cui_str){input.data + key, key_end - key}, name))
                result =
                    cui_text_view_image_length_parse(input, (cui_text_view_range){colon + 1, end});
        }
        start = end + (end < style.end);
    }
    return result;
}
static void cui_text_view_html_style(unsigned *flags, unsigned *depth, unsigned flag,
                                     bool closing) {
    if (closing) {
        if (*depth)
            --*depth;
    } else
        ++*depth;
    if (*depth)
        *flags |= flag;
    else
        *flags &= ~flag;
}
static size_t cui_text_view_html_skip_raw(cui_str input, size_t start, cui_str tag) {
    for (size_t i = start; i + 2 < input.length; ++i) {
        if (input.data[i] != '<' || input.data[i + 1] != '/')
            continue;
        size_t a = i + 2, end = a;
        while (end < input.length && input.data[end] != '>' && input.data[end] != ' ' &&
               input.data[end] != '\t' && input.data[end] != '\n' && input.data[end] != '\r')
            ++end;
        if (end - a != tag.length)
            continue;
        bool equal = true;
        for (size_t j = 0; j < tag.length; ++j) {
            char x = input.data[a + j], y = tag.data[j];
            if (x >= 'A' && x <= 'Z')
                x = (char)(x + 32);
            if (y >= 'A' && y <= 'Z')
                y = (char)(y + 32);
            if (x != y) {
                equal = false;
                break;
            }
        }
        if (equal) {
            size_t close = cui_text_view_find(input, end, input.length, ">", 1);
            return close == SIZE_MAX ? input.length : close + 1;
        }
    }
    return input.length;
}
static void cui_text_view_html(cui_text_view_parser *p) {
    size_t a = 0;
    unsigned flags = 0, table = 0, row = 0, col = 0, inline_containers = 0;
    unsigned bold_depth = 0, italic_depth = 0, code_depth = 0, strike_depth = 0, mark_depth = 0;
    cui_text_view_range link = {0};
    bool active = false, pre = false;
    while (a < p->input.length && !p->error) {
        if (p->input.data[a] == '<') {
            size_t end = cui_text_view_html_tag_end(p->input, a, p->input.length);
            if (end != SIZE_MAX) {
                size_t begin = a + 1;
                bool closing = begin < end && p->input.data[begin] == '/';
                if (closing)
                    ++begin;
                size_t name_end = begin;
                while (name_end < end && !cui_text_view_html_space(p->input.data[name_end]) &&
                       p->input.data[name_end] != '/')
                    ++name_end;
                cui_str name = {p->input.data + begin, name_end - begin};
                bool inline_container =
                    cui_text_view_name(name, "p") || cui_text_view_name(name, "td") ||
                    cui_text_view_name(name, "th") ||
                    (name.length == 2 && (name.data[0] == 'h' || name.data[0] == 'H') &&
                     name.data[1] >= '1' && name.data[1] <= '6');
                if (inline_container) {
                    if (closing) {
                        if (inline_containers)
                            --inline_containers;
                    } else if (++inline_containers > 32) {
                        p->error = CUI_TEXT_VIEW_INVALID;
                        return;
                    }
                }
                if (!closing &&
                    (cui_text_view_name(name, "script") || cui_text_view_name(name, "style"))) {
                    a = cui_text_view_html_skip_raw(p->input, end + 1, name);
                    continue;
                }
                if (cui_text_view_name(name, "strong") || cui_text_view_name(name, "b"))
                    cui_text_view_html_style(&flags, &bold_depth, CUI_TEXT_VIEW_BOLD, closing);
                else if (cui_text_view_name(name, "em") || cui_text_view_name(name, "i"))
                    cui_text_view_html_style(&flags, &italic_depth, CUI_TEXT_VIEW_ITALIC, closing);
                else if (cui_text_view_name(name, "code"))
                    cui_text_view_html_style(&flags, &code_depth, CUI_TEXT_VIEW_CODE, closing);
                else if (cui_text_view_name(name, "s") || cui_text_view_name(name, "del"))
                    cui_text_view_html_style(&flags, &strike_depth, CUI_TEXT_VIEW_STRIKE, closing);
                else if (cui_text_view_name(name, "mark"))
                    cui_text_view_html_style(&flags, &mark_depth, CUI_TEXT_VIEW_MARK, closing);
                else if (cui_text_view_name(name, "a"))
                    link = closing ? (cui_text_view_range){0}
                                   : cui_text_view_attribute(p->input, name_end, end, "href");
                else if (!closing && cui_text_view_name(name, "img") &&
                         (inline_containers || link.end > link.begin)) {
                    cui_text_view_range src =
                        cui_text_view_attribute(p->input, name_end, end, "src");
                    if (src.begin || src.end) {
                        if (!active) {
                            if (!cui_text_view_block_begin(p, CUI_TEXT_VIEW_PARAGRAPH, a, 0))
                                return;
                            active = true;
                        }
                        cui_text_view_emit(p, (cui_str){" ", 1}, (cui_text_view_range){a, end + 1},
                                           flags | CUI_TEXT_VIEW_INLINE_IMAGE, link);
                        p->last.resource = src;
                        p->last.alt = cui_text_view_attribute(p->input, name_end, end, "alt");
                        p->last.image_width =
                            cui_text_view_html_image_length(p->input, name_end, end, "width");
                        p->last.image_height =
                            cui_text_view_html_image_length(p->input, name_end, end, "height");
                        if (p->write && !p->error)
                            p->state->workspace.spans[p->spans - 1] = p->last;
                    }
                } else if (cui_text_view_name(name, "br")) {
                    if (active)
                        cui_text_view_emit(p, (cui_str){"\n", 1}, (cui_text_view_range){a, end + 1},
                                           flags, link);
                } else if (cui_text_view_name(name, "table")) {
                    if (!closing) {
                        ++table;
                        row = 0;
                        col = 0;
                    }
                } else if (cui_text_view_name(name, "tr")) {
                    if (closing) {
                        cui_text_view_gap(p, '\n');
                        ++row;
                    }
                    col = 0;
                } else {
                    cui_text_view_block_kind kind = CUI_TEXT_VIEW_PARAGRAPH;
                    unsigned level = 0;
                    bool block = false;
                    if (name.length == 2 && (name.data[0] == 'h' || name.data[0] == 'H') &&
                        name.data[1] >= '1' && name.data[1] <= '6') {
                        kind = CUI_TEXT_VIEW_HEADING;
                        level = (unsigned)(name.data[1] - '0');
                        block = true;
                    } else if (cui_text_view_name(name, "p") || cui_text_view_name(name, "div"))
                        block = true;
                    else if (cui_text_view_name(name, "blockquote")) {
                        kind = CUI_TEXT_VIEW_QUOTE;
                        block = true;
                    } else if (cui_text_view_name(name, "li")) {
                        kind = CUI_TEXT_VIEW_LIST_ITEM;
                        block = true;
                    } else if (cui_text_view_name(name, "pre")) {
                        kind = CUI_TEXT_VIEW_CODE_BLOCK;
                        block = true;
                        pre = !closing;
                    } else if (cui_text_view_name(name, "td") || cui_text_view_name(name, "th")) {
                        kind = CUI_TEXT_VIEW_TABLE_CELL;
                        block = true;
                    } else if (cui_text_view_name(name, "hr")) {
                        kind = CUI_TEXT_VIEW_RULE;
                        block = true;
                    } else if (cui_text_view_name(name, "img")) {
                        cui_text_view_range src =
                            cui_text_view_attribute(p->input, name_end, end, "src");
                        if (closing || (!src.begin && !src.end)) {
                            a = end + 1;
                            continue;
                        }
                        block = true;
                    }
                    if (block) {
                        if (active) {
                            cui_text_view_block_end(p, a, kind != CUI_TEXT_VIEW_TABLE_CELL);
                            active = false;
                        }
                        if (!closing) {
                            if (!cui_text_view_block_begin(p, kind, a, level))
                                return;
                            active = true;
                            if (p->write && kind == CUI_TEXT_VIEW_TABLE_CELL) {
                                cui_text_view_block *v = &p->state->workspace.blocks[p->block];
                                v->table = table;
                                v->row = row;
                                v->column = col;
                                v->table_header = cui_text_view_name(name, "th");
                            }
                            if (kind == CUI_TEXT_VIEW_TABLE_CELL)
                                ++col;
                            if (cui_text_view_name(name, "img")) {
                                cui_text_view_emit(p, (cui_str){" ", 1},
                                                   (cui_text_view_range){a, end + 1},
                                                   flags | CUI_TEXT_VIEW_INLINE_IMAGE, link);
                                p->last.resource =
                                    cui_text_view_attribute(p->input, name_end, end, "src");
                                p->last.alt =
                                    cui_text_view_attribute(p->input, name_end, end, "alt");
                                p->last.image_width = cui_text_view_html_image_length(
                                    p->input, name_end, end, "width");
                                p->last.image_height = cui_text_view_html_image_length(
                                    p->input, name_end, end, "height");
                                if (p->write && !p->error)
                                    p->state->workspace.spans[p->spans - 1] = p->last;
                                cui_text_view_block_end(p, end + 1, true);
                                active = false;
                            } else if (kind == CUI_TEXT_VIEW_RULE) {
                                cui_text_view_block_end(p, end + 1, true);
                                active = false;
                            }
                        } else if (kind == CUI_TEXT_VIEW_TABLE_CELL)
                            cui_text_view_gap(p, ' ');
                    }
                }
                a = end + 1;
                continue;
            }
        }
        if (!active) {
            if (!pre && (p->input.data[a] == ' ' || p->input.data[a] == '\n' ||
                         p->input.data[a] == '\r' || p->input.data[a] == '\t')) {
                ++a;
                continue;
            }
            if (!cui_text_view_block_begin(p, CUI_TEXT_VIEW_PARAGRAPH, a, 0))
                return;
            active = true;
        }
        if (p->input.data[a] == '&' && cui_text_view_entity(p, &a, p->input.length, flags, link))
            continue;
        size_t n = cui_text_view_scalar(p->input, a);
        char ch = p->input.data[a];
        if (!pre && (ch == '\n' || ch == '\r' || ch == '\t'))
            cui_text_view_emit(p, (cui_str){" ", 1}, (cui_text_view_range){a, a + n}, flags, link);
        else
            cui_text_view_emit(p, (cui_str){p->input.data + a, n}, (cui_text_view_range){a, a + n},
                               flags, link);
        a += n;
    }
    if (active)
        cui_text_view_block_end(p, p->input.length, true);
}
static void cui_text_view_parse(cui_text_view_parser *p) {
    if (p->state->format == CUI_TEXT_VIEW_MARKDOWN)
        cui_text_view_markdown(p);
    else if (p->state->format == CUI_TEXT_VIEW_HTML)
        cui_text_view_html(p);
    else if (p->input.length) {
        if (cui_text_view_block_begin(p, CUI_TEXT_VIEW_PARAGRAPH, 0, 0)) {
            for (size_t i = 0; i < p->input.length;) {
                size_t n = cui_text_view_scalar(p->input, i);
                cui_text_view_emit(p, (cui_str){p->input.data + i, n},
                                   (cui_text_view_range){i, i + n}, 0, (cui_text_view_range){0});
                i += n;
            }
            cui_text_view_block_end(p, p->input.length, false);
        }
    }
}
static bool cui_text_view_source_of_range(const cui_text_view_state *s, cui_text_view_range range,
                                          cui_text_view_range *out) {
    size_t a = SIZE_MAX, b = 0;
    for (size_t i = range.begin; i < range.end; ++i) {
        cui_text_view_range m = s->workspace.mapping[i].source;
        if (m.begin != SIZE_MAX) {
            if (m.begin < a)
                a = m.begin;
            if (m.end > b)
                b = m.end;
        }
    }
    if (a == SIZE_MAX)
        return false;
    *out = (cui_text_view_range){a, b};
    return true;
}
static bool cui_text_view_remap_source(cui_text_view_range *r, size_t prefix, size_t suffix,
                                       size_t old_length, size_t new_length) {
    if (r->end <= prefix)
        return true;
    if (r->begin >= old_length - suffix) {
        r->begin = new_length - (old_length - r->begin);
        r->end = new_length - (old_length - r->end);
        return true;
    }
    return false;
}
static bool cui_text_view_table_highlight_stable(const cui_text_view_state *s,
                                                 cui_text_view_range range, size_t prefix,
                                                 size_t suffix, bool append) {
    if (append)
        return true;
    for (size_t i = 0; i < s->block_count; ++i) {
        const cui_text_view_block *cell = &s->workspace.blocks[i];
        if (cell->kind != CUI_TEXT_VIEW_TABLE_CELL || cell->text.end <= range.begin ||
            cell->text.begin >= range.end)
            continue;
        size_t first = i, last = i;
        while (first && s->workspace.blocks[first - 1].kind == CUI_TEXT_VIEW_TABLE_CELL &&
               s->workspace.blocks[first - 1].table == cell->table)
            --first;
        while (last + 1 < s->block_count &&
               s->workspace.blocks[last + 1].kind == CUI_TEXT_VIEW_TABLE_CELL &&
               s->workspace.blocks[last + 1].table == cell->table)
            ++last;
        if (s->workspace.blocks[first].source.begin >= s->source_length - suffix)
            continue;
        size_t table_end = cui_text_view_next_line(
            cui_text_view_source(s),
            cui_text_view_line_end(cui_text_view_source(s), s->workspace.blocks[last].source.end));
        if (prefix >= table_end)
            continue;
        size_t row_end = cui_text_view_next_line(
            cui_text_view_source(s),
            cui_text_view_line_end(cui_text_view_source(s), cell->source.end));
        if (row_end > prefix)
            return false;
    }
    return true;
}
static unsigned cui_text_view_codepoint(cui_str, size_t);
static unsigned cui_text_view_character_kind(unsigned);
static bool cui_text_view_cjk(unsigned c) {
    return (c >= 0x3040 && c <= 0x30ff) || (c >= 0x3400 && c <= 0x4dbf) ||
           (c >= 0x4e00 && c <= 0x9fff) || (c >= 0xac00 && c <= 0xd7af) ||
           (c >= 0xf900 && c <= 0xfaff) || (c >= 0x20000 && c <= 0x2fa1f);
}
/* Two passes count units before assigning birth times: large chunks snap back
 * to a single fade, rather than an arbitrarily fast sweep. Per-block ranges
 * isolate table cells/code/paragraphs, matching the source's text leaves. */
static void cui_text_view_stagger_range(cui_text_view_state *s, size_t begin, size_t end) {
    cui_str text = cui_text_view_rendered_text(s);
    size_t units = 1;
    bool glyph = false;
    unsigned previous = 0;
    for (size_t at = begin; at < end; at += cui_text_view_scalar(text, at)) {
        unsigned c = cui_text_view_codepoint(text, at);
        bool space = cui_text_view_character_kind(c) == 2;
        if (glyph && !space &&
            (cui_text_view_cjk(c) || cui_text_view_character_kind(previous) == 2 ||
             cui_text_view_cjk(previous)))
            ++units;
        glyph |= !space;
        previous = c;
    }
    float step =
        (double)s->fade_stagger * (double)(units - 1) <= s->fade_duration ? s->fade_stagger : 0;
    size_t unit = 0;
    glyph = false;
    previous = 0;
    for (size_t at = begin; at < end;) {
        unsigned c = cui_text_view_codepoint(text, at);
        size_t n = cui_text_view_scalar(text, at);
        bool space = cui_text_view_character_kind(c) == 2;
        if (glyph && !space &&
            (cui_text_view_cjk(c) || cui_text_view_character_kind(previous) == 2 ||
             cui_text_view_cjk(previous)))
            ++unit;
        float born = s->time + (float)unit * step;
        if (!isfinite(born))
            born = s->time;
        for (size_t j = 0; j < n; ++j)
            s->workspace.mapping[at + j].born = born;
        glyph |= !space;
        previous = c;
        at += n;
    }
}
static void cui_text_view_finish_fade(cui_text_view_state *s) {
    cui_str text = cui_text_view_rendered_text(s);
    for (size_t b = 0; b < s->block_count; ++b) {
        cui_text_view_range range = s->workspace.blocks[b].text;
        size_t begin = SIZE_MAX;
        for (size_t at = range.begin; at < range.end;) {
            size_t n = cui_text_view_scalar(text, at);
            bool fresh = false;
            for (size_t j = 0; j < n; ++j)
                fresh |= s->workspace.mapping[at + j].born == -2;
            if (fresh && begin == SIZE_MAX)
                begin = at;
            if (!fresh && begin != SIZE_MAX) {
                cui_text_view_stagger_range(s, begin, at);
                begin = SIZE_MAX;
            }
            at += n;
        }
        if (begin != SIZE_MAX)
            cui_text_view_stagger_range(s, begin, range.end);
    }
    /* Block separators and non-text markers are not independent fade leaves. */
    for (size_t at = 0; at < s->text_length; ++at)
        if (s->workspace.mapping[at].born == -2)
            s->workspace.mapping[at].born = s->time;
}
cui_text_view_error cui_text_view_set_text(cui_text_view_state *s, cui_str input) {
    if (!s || !s->workspace.source || !cui_text_view_valid(input))
        return CUI_TEXT_VIEW_INVALID;
    if (input.length >= s->workspace.source_capacity)
        return CUI_TEXT_VIEW_CAPACITY;
    cui_text_view_parser dry = {.state = s, .input = input};
    cui_text_view_parse(&dry);
    if (dry.error)
        return dry.error;
    bool append = input.length >= s->source_length &&
                  (!s->source_length || !memcmp(input.data, s->workspace.source, s->source_length));
    if (input.length == s->source_length && append)
        return CUI_TEXT_VIEW_OK;
    size_t prefix = 0, suffix = 0;
    while (prefix < input.length && prefix < s->source_length &&
           input.data[prefix] == s->workspace.source[prefix])
        ++prefix;
    while (prefix && (!cui_text_view_boundary(input, prefix) ||
                      !cui_text_view_boundary(cui_text_view_source(s), prefix)))
        --prefix;
    while (suffix < input.length - prefix && suffix < s->source_length - prefix &&
           input.data[input.length - 1 - suffix] ==
               s->workspace.source[s->source_length - 1 - suffix])
        ++suffix;
    while (suffix && (!cui_text_view_boundary(input, input.length - suffix) ||
                      !cui_text_view_boundary(cui_text_view_source(s), s->source_length - suffix)))
        --suffix;
    size_t kept = 0;
    for (size_t i = 0; i < s->highlight_count; ++i) {
        cui_text_view_highlight h = s->workspace.highlights[i];
        cui_text_view_range source;
        if (!cui_text_view_table_highlight_stable(s, h.range, prefix, suffix, append) ||
            !cui_text_view_source_of_range(s, h.range, &source) ||
            !cui_text_view_remap_source(&source, prefix, suffix, s->source_length, input.length))
            continue;
        h.range = source;
        s->workspace.highlights[kept++] = h;
    }
    bool reveal = s->reveal_pending;
    cui_text_view_range reveal_source;
    if (reveal)
        reveal = cui_text_view_source_of_range(s, s->reveal, &reveal_source) &&
                 cui_text_view_remap_source(&reveal_source, prefix, suffix, s->source_length,
                                            input.length);
    if (input.length)
        memmove(s->workspace.source, input.data, input.length);
    s->workspace.source[input.length] = 0;
    cui_text_view_parser actual = {.state = s,
                                   .input = {s->workspace.source, input.length},
                                   .write = true,
                                   .fade = append && s->stream_fade && !s->reduced_motion &&
                                           s->fade_duration > 0};
    cui_text_view_parse(&actual);
    s->source_length = input.length;
    s->text_length = actual.text;
    s->span_count = actual.spans;
    s->block_count = actual.blocks;
    s->glyph_count = 0;
    s->selection_projected = s->selection_override = s->multi_click_release = false;
    s->workspace.rendered[s->text_length] = 0;
    cui_text_view_finish_fade(s);
    s->highlight_count = 0;
    for (size_t i = 0; i < kept; ++i) {
        cui_text_view_highlight h = s->workspace.highlights[i];
        cui_text_view_range mapped;
        if (cui_text_view_range_for_source(s, h.range, &mapped)) {
            h.range = mapped;
            s->workspace.highlights[s->highlight_count++] = h;
        }
    }
    s->reveal_pending = reveal && cui_text_view_range_for_source(s, reveal_source, &s->reveal);
    if (!append) {
        s->anchor = s->cursor = 0;
    } else {
        if (s->anchor > s->text_length)
            s->anchor = s->text_length;
        if (s->cursor > s->text_length)
            s->cursor = s->text_length;
    }
    return actual.error;
}
bool cui_text_view_select(cui_text_view_state *s, size_t a, size_t c) {
    if (!s || !cui_text_view_boundary(cui_text_view_rendered_text(s), a) ||
        !cui_text_view_boundary(cui_text_view_rendered_text(s), c))
        return false;
    bool changed = s->anchor != a || s->cursor != c || s->selection_projected;
    s->selection_projected = s->selection_override = s->multi_click_release = false;
    s->anchor = a;
    s->cursor = c;
    return changed;
}
static unsigned cui_text_view_codepoint(cui_str text, size_t offset) {
    size_t n = cui_text_view_scalar(text, offset);
    unsigned value = n == 1 ? (unsigned char)text.data[offset]
                            : (unsigned char)text.data[offset] & (n == 2   ? 31u
                                                                  : n == 3 ? 15u
                                                                           : 7u);
    for (size_t i = 1; i < n; ++i)
        value = (value << 6) | ((unsigned char)text.data[offset + i] & 63u);
    return value;
}
static unsigned cui_text_view_character_kind(unsigned c) {
    if (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        (c >= 0xc0 && c <= 0x24f) || (c >= 0x400 && c <= 0x4ff) || (c >= 0x1e00 && c <= 0x1eff) ||
        (c >= 0x300 && c <= 0x36f))
        return 1;
    if (c == '\n' || c == '\r')
        return 3;
    if ((c >= 9 && c <= 13) || c == 32 || c == 0x85 || c == 0xa0 || c == 0x1680 ||
        (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f ||
        c == 0x3000)
        return 2;
    return 4;
}
bool cui_text_view_selection_range_at(const cui_text_view_state *s, size_t offset, unsigned clicks,
                                      cui_text_view_range *out) {
    if (!s || clicks < 2)
        return false;
    cui_str text = cui_text_view_rendered_text(s);
    if (offset > text.length)
        offset = text.length;
    while (!cui_text_view_boundary(text, offset))
        --offset;
    size_t begin = SIZE_MAX, end = 0;
    for (size_t i = 0; i < s->block_count; ++i) {
        cui_text_view_range r = s->workspace.blocks[i].text;
        if (offset >= r.begin && offset < r.end) {
            begin = r.begin;
            end = r.end;
            break;
        }
    }
    if (begin == SIZE_MAX || (s->workspace.mapping[offset].flags & CUI_TEXT_VIEW_INLINE_IMAGE))
        return false;
    for (size_t i = begin; i < end; ++i)
        if (s->workspace.mapping[i].flags & CUI_TEXT_VIEW_INLINE_IMAGE) {
            if (i < offset)
                begin = i + 1;
            else {
                end = i;
                break;
            }
        }
    size_t a = offset, b = offset;
    if (clicks == 2) {
        size_t n = cui_text_view_scalar(text, offset);
        unsigned kind = cui_text_view_character_kind(cui_text_view_codepoint(text, offset));
        b += n;
        if (kind == 1 || kind == 2) {
            for (unsigned count = 0; count < 128 && a > begin; ++count) {
                size_t previous = a - 1;
                while (!cui_text_view_boundary(text, previous))
                    --previous;
                if (cui_text_view_character_kind(cui_text_view_codepoint(text, previous)) != kind)
                    break;
                a = previous;
            }
            for (unsigned count = 0; count < 128 && b < end; ++count) {
                if (cui_text_view_character_kind(cui_text_view_codepoint(text, b)) != kind)
                    break;
                b += cui_text_view_scalar(text, b);
            }
        }
    } else {
        while (a > begin && text.data[a - 1] != '\n')
            --a;
        while (b < end && text.data[b] != '\n')
            ++b;
    }
    if (a == b)
        return false;
    if (out)
        *out = (cui_text_view_range){a, b};
    return true;
}
cui_str cui_text_view_selected_text(const cui_text_view_state *s) {
    if (!s)
        return (cui_str){0};
    size_t a = s->anchor < s->cursor ? s->anchor : s->cursor,
           b = s->anchor > s->cursor ? s->anchor : s->cursor;
    return (cui_str){s->workspace.rendered + a, b - a};
}
/* Images at paragraph boundaries belong to the adjacent selected text run. */
static void cui_text_view_source_selection(const cui_text_view_state *s, size_t *a, size_t *b) {
    if (*a == *b)
        return;
    for (size_t i = 0; i < s->block_count; ++i) {
        cui_text_view_range r = s->workspace.blocks[i].text;
        size_t first = r.begin, last = r.end;
        while (first < last && (s->workspace.mapping[first].flags & CUI_TEXT_VIEW_INLINE_IMAGE))
            ++first;
        while (last > first && (s->workspace.mapping[last - 1].flags & CUI_TEXT_VIEW_INLINE_IMAGE))
            --last;
        if (*a == first && first < last && *b > first)
            *a = r.begin;
        if (*b == last && first < last && *a < last)
            *b = r.end;
    }
}
static cui_text_view_range cui_text_view_block_selection(const cui_text_view_state *s, size_t index,
                                                         size_t a, size_t b) {
    const cui_text_view_block *block = &s->workspace.blocks[index];
    if (s->selection_projected) {
        a = block->projected_selection.begin;
        b = block->projected_selection.end;
        if (a != b)
            cui_text_view_source_selection(s, &a, &b);
    }
    return (cui_text_view_range){a > block->text.begin ? a : block->text.begin,
                                 b < block->text.end ? b : block->text.end};
}
bool cui_text_view_selected_source_range(const cui_text_view_state *s, cui_text_view_range *out) {
    if (!s || s->format == CUI_TEXT_VIEW_HTML)
        return false;
    size_t a = s->anchor < s->cursor ? s->anchor : s->cursor,
           b = s->anchor > s->cursor ? s->anchor : s->cursor;
    cui_text_view_source_selection(s, &a, &b);
    size_t start = SIZE_MAX, end = 0;
    for (size_t i = a; i < b; ++i) {
        if (s->selection_projected) {
            bool selected = false;
            for (size_t j = 0; j < s->block_count; ++j) {
                cui_text_view_range r = cui_text_view_block_selection(s, j, a, b);
                if (i >= r.begin && i < r.end)
                    selected = true;
            }
            if (!selected)
                continue;
        }
        cui_text_view_range m = s->workspace.mapping[i].source;
        if (m.begin != SIZE_MAX) {
            if (m.begin < start)
                start = m.begin;
            if (m.end > end)
                end = m.end;
        }
    }
    if (start == SIZE_MAX)
        return false;
    if (out)
        *out = (cui_text_view_range){start, end};
    return true;
}
bool cui_text_view_range_for_source(const cui_text_view_state *s, cui_text_view_range r,
                                    cui_text_view_range *out) {
    if (!s || s->format == CUI_TEXT_VIEW_HTML || r.begin == r.end ||
        !cui_text_view_range_valid(cui_text_view_source(s), r))
        return false;
    size_t begin = SIZE_MAX, end = 0;
    for (size_t i = 0; i < s->text_length; ++i) {
        cui_text_view_range m = s->workspace.mapping[i].source;
        if (m.begin != SIZE_MAX && m.begin < r.end && m.end > r.begin) {
            if (begin == SIZE_MAX)
                begin = i;
            end = i + 1;
        }
    }
    if (begin == SIZE_MAX)
        return false;
    if (out)
        *out = (cui_text_view_range){begin, end};
    return true;
}
cui_text_view_error cui_text_view_set_highlights(cui_text_view_state *s,
                                                 const cui_text_view_highlight *h, size_t n) {
    if (!s || (n && !h))
        return CUI_TEXT_VIEW_INVALID;
    if (s->format == CUI_TEXT_VIEW_HTML)
        return CUI_TEXT_VIEW_UNSUPPORTED;
    if (n > s->workspace.highlight_capacity)
        return CUI_TEXT_VIEW_CAPACITY;
    for (size_t i = 0; i < n; ++i)
        if (!cui_text_view_range_valid(cui_text_view_rendered_text(s), h[i].range))
            return CUI_TEXT_VIEW_INVALID;
    if (n)
        memmove(s->workspace.highlights, h, n * sizeof *h);
    s->highlight_count = n;
    return CUI_TEXT_VIEW_OK;
}
cui_text_view_error cui_text_view_reveal(cui_text_view_state *s, cui_text_view_range r) {
    if (!s || !cui_text_view_range_valid(cui_text_view_rendered_text(s), r))
        return CUI_TEXT_VIEW_INVALID;
    s->reveal = r;
    s->reveal_pending = true;
    s->reveal_age = 0;
    return CUI_TEXT_VIEW_OK;
}
cui_text_view_motion cui_text_view_motion_default(void) {
    return (cui_text_view_motion){.duration = .35f, .easing = CUI_TEXT_VIEW_FADE_EASE_OUT_CUBIC};
}
bool cui_text_view_set_motion(cui_text_view_state *s, cui_text_view_motion motion) {
    if (!s || !isfinite(motion.duration) || motion.duration < 0 || !isfinite(motion.stagger) ||
        motion.stagger < 0 || motion.easing < CUI_TEXT_VIEW_FADE_EASE_OUT_CUBIC ||
        motion.easing > CUI_TEXT_VIEW_FADE_EASE_IN_OUT_CUBIC)
        return false;
    s->fade_duration = motion.duration;
    s->fade_stagger = motion.stagger;
    s->fade_easing = motion.easing;
    if (motion.duration == 0)
        for (size_t i = 0; i < s->text_length; ++i)
            s->workspace.mapping[i].born = -1;
    return true;
}
float cui_text_view_stream_opacity(const cui_text_view_state *s, size_t at) {
    if (!s || at >= s->text_length || !s->stream_fade || s->reduced_motion ||
        s->workspace.mapping[at].born < 0 || s->fade_duration <= 0)
        return 1;
    float t = fminf(fmaxf((s->time - s->workspace.mapping[at].born) / s->fade_duration, 0), 1);
    if (s->fade_easing == CUI_TEXT_VIEW_FADE_LINEAR)
        return t;
    if (s->fade_easing == CUI_TEXT_VIEW_FADE_EASE_IN_OUT_CUBIC)
        return t < .5f ? 4 * t * t * t : 1 - 4 * (1 - t) * (1 - t) * (1 - t);
    return 1 - (1 - t) * (1 - t) * (1 - t);
}
bool cui_text_view_advance(cui_text_view_state *s, float dt) {
    if (!s || !isfinite(dt) || dt < 0)
        return false;
    if (!isfinite(s->time + dt))
        return false;
    s->time += dt;
    if (s->reveal_pending) {
        s->reveal_age += dt;
        if (s->reveal_age > 1)
            s->reveal_pending = false;
    }
    if (s->reduced_motion || !s->stream_fade)
        return s->reveal_pending;
    for (size_t i = 0; i < s->text_length; ++i)
        if (s->workspace.mapping[i].born >= 0 &&
            s->time - s->workspace.mapping[i].born < s->fade_duration)
            return true;
    return s->reveal_pending;
}
static unsigned cui_text_view_block_flags(const cui_text_view_block *b) {
    return b->kind == CUI_TEXT_VIEW_HEADING      ? (b->level == 1   ? CUI_TEXT_VIEW_BOLD
                                                    : b->level == 6 ? CUI_TEXT_VIEW_MEDIUM
                                                                    : CUI_TEXT_VIEW_SEMIBOLD)
           : b->kind == CUI_TEXT_VIEW_CODE_BLOCK ? CUI_TEXT_VIEW_CODE
                                                 : 0;
}
static bool cui_text_view_prepare_list_prefix(cui_text_view_block *b, float font,
                                              cui_text_view_measure_fn measure, void *user,
                                              float *width) {
    *width = 20;
    if (!b->task) {
        if (b->list_ordered) {
            if (b->list_depth && b->list_start) {
                b->prefix_text[0] =
                    (char)((b->list_depth == 1 ? 'A' : 'a') + (b->list_number - 1) % 26);
                memcpy(b->prefix_text + 1, ". ", 3);
            } else {
                size_t length = strlen(b->marker_text);
                memcpy(b->prefix_text, b->marker_text, length);
                b->prefix_text[length] = ' ';
                b->prefix_text[length + 1] = 0;
            }
        } else {
            static const char *bullets[] = {"\xe2\x80\xa2 ", "\xe2\x97\xa6 ", "\xe2\x96\xaa ",
                                            "\xe2\x80\xa3 ", "\xe2\x81\x83 "};
            memcpy(b->prefix_text, bullets[b->list_depth < 5 ? b->list_depth : 4], 5);
        }
        cui_size marker = measure(user, (cui_str){b->prefix_text, strlen(b->prefix_text)}, font, 0);
        if (!isfinite(marker.width) || marker.width < 0 || marker.width > 10000000)
            return false;
        *width = marker.width;
    }
    return true;
}
static bool cui_text_view_metrics_valid(cui_font_metrics m) {
    return isfinite(m.ascent) && isfinite(m.descent) && isfinite(m.line_height) && m.ascent >= 0 &&
           m.descent >= 0 && m.line_height > 0 && m.line_height <= 10000000 &&
           m.ascent + m.descent > 0 && m.ascent + m.descent <= 10000000;
}
static float cui_text_view_finish_inline_line(cui_text_view_state *s, size_t first, float y,
                                              float line, cui_text_view_layout_options o) {
    float baseline =
        o.font_metrics_available
            ? (line - o.body_metrics.ascent - o.body_metrics.descent) * .5f + o.body_metrics.ascent
            : 0;
    float ascent = o.font_metrics_available ? baseline : 0;
    float descent = o.font_metrics_available ? line - baseline : line;
    bool image = false;
    for (size_t i = first; i < s->glyph_count; ++i) {
        cui_text_view_span *span = &s->workspace.spans[s->workspace.glyphs[i].span];
        if (!(span->flags & CUI_TEXT_VIEW_INLINE_IMAGE))
            continue;
        image = true;
        float h = span->image_bounds.h;
        float b =
            o.font_metrics_available ? fminf(h, fmaxf(0, h * .5f + baseline - line * .5f)) : 0;
        ascent = fmaxf(ascent, b);
        descent = fmaxf(descent, h - b);
    }
    if (!image)
        return line;
    float height = ascent + descent;
    if (!isfinite(height) || height <= 0 || height > 10000000)
        return NAN;
    for (size_t i = first; i < s->glyph_count; ++i) {
        cui_text_view_glyph *glyph = &s->workspace.glyphs[i];
        cui_text_view_span *span = &s->workspace.spans[glyph->span];
        if (span->flags & CUI_TEXT_VIEW_INLINE_IMAGE) {
            float h = span->image_bounds.h;
            float b =
                o.font_metrics_available ? fminf(h, fmaxf(0, h * .5f + baseline - line * .5f)) : 0;
            span->image_bounds.y = y + ascent - b;
            glyph->bounds.y = y;
            glyph->bounds.h = height;
        } else
            glyph->bounds.y += ascent - baseline;
    }
    return height;
}
static bool cui_text_view_layout_block(cui_text_view_state *s, size_t index, float x, float y,
                                       float width, cui_text_view_layout_options o,
                                       cui_text_view_measure_fn measure, void *user) {
    cui_text_view_block *b = &s->workspace.blocks[index];
    float scale = b->kind == CUI_TEXT_VIEW_HEADING ? (b->level == 1   ? 2
                                                      : b->level == 2 ? 1.5f
                                                      : b->level == 3 ? 1.25f
                                                      : b->level == 4 ? 1.125f
                                                                      : 1)
                                                   : 1;
    float font = o.font_size * scale, line = o.line_height * scale,
          padding = b->kind == CUI_TEXT_VIEW_CODE_BLOCK   ? 12
                    : b->kind == CUI_TEXT_VIEW_TABLE_CELL ? 4
                                                          : 0;
    if (o.font_metrics) {
        cui_font_metrics metrics = {0};
        o.font_metrics_available =
            o.font_metrics(o.font_metrics_user, font, cui_text_view_block_flags(b), &metrics) &&
            cui_text_view_metrics_valid(metrics);
        o.body_metrics = metrics;
    }
    float left = b->kind == CUI_TEXT_VIEW_QUOTE            ? 12
                 : b->kind == CUI_TEXT_VIEW_TABLE_CELL     ? 8
                 : (b->list_item || b->list_repeat_marker) ? 20
                                                           : padding;
    float content_inset = b->kind == CUI_TEXT_VIEW_QUOTE ? 12 : padding;
    if ((b->list_item || b->list_repeat_marker) && b->kind != CUI_TEXT_VIEW_TABLE_CELL &&
        !cui_text_view_prepare_list_prefix(b, o.font_size, measure, user, &left))
        return false;
    if (b->kind != CUI_TEXT_VIEW_TABLE_CELL)
        b->list_prefix_width = 0;
    if (b->list_item && b->kind != CUI_TEXT_VIEW_LIST_ITEM && b->kind != CUI_TEXT_VIEW_TABLE_CELL) {
        b->list_prefix_width = left;
        x += left;
        width = fmaxf(1, width - left);
        left = content_inset;
    }
    b->text_inset = left;
    b->image_line = (cui_rect){0};
    float available = fmaxf(1, width - left - (b->kind == CUI_TEXT_VIEW_TABLE_CELL ? 8 : padding));
    float cursor_x = 0, cursor_y = 0;
    size_t first_glyph = s->glyph_count, line_first = first_glyph;
    if (b->kind == CUI_TEXT_VIEW_RULE) {
        b->bounds = (cui_rect){x, y, width, 1};
        return true;
    }
    if (b->kind == CUI_TEXT_VIEW_IMAGE && b->image_ready && b->image.width && b->image.height) {
        float w = fminf(width, (float)b->image.width);
        float height = w * b->image.height / b->image.width;
        b->bounds = (cui_rect){x, y, w, height};
        b->image_line = b->bounds;
        if (o.font_metrics_available) {
            float baseline =
                (o.line_height - o.body_metrics.ascent - o.body_metrics.descent) * .5f +
                o.body_metrics.ascent;
            float image_baseline =
                fminf(height, fmaxf(0, height * .5f + baseline - o.line_height * .5f));
            float ascent = fmaxf(baseline, image_baseline);
            float descent = fmaxf(o.line_height - baseline, height - image_baseline);
            b->bounds.y += ascent - image_baseline;
            b->image_line.h = ascent + descent;
        }
        return true;
    }
    for (size_t span = 0; span < s->span_count; ++span) {
        cui_text_view_span *r = &s->workspace.spans[span];
        if (r->block != index)
            continue;
        unsigned flags = r->flags | cui_text_view_block_flags(b);
        float size =
            flags & CUI_TEXT_VIEW_CODE && b->kind != CUI_TEXT_VIEW_CODE_BLOCK ? font * .875f : font;
        if (r->flags & CUI_TEXT_VIEW_INLINE_IMAGE) {
            float ratio = r->image_ready && r->image.width && r->image.height
                              ? (float)r->image.width / r->image.height
                              : 1;
            cui_text_view_image_length width = r->image_width, height = r->image_height;
            if ((width.specified && (!isfinite(width.value) || width.value < 0)) ||
                (height.specified && (!isfinite(height.value) || height.value < 0)))
                return false;
            float h =
                height.specified ? height.value * (height.relative ? line : 1)
                : width.specified
                    ? (r->image_ready ? width.value * (width.relative ? line : 1) / ratio : line)
                    : fmaxf(1, line * .75f);
            float w = width.specified    ? width.value * (width.relative ? line : 1)
                      : height.specified ? h * (r->image_ready ? ratio : 1)
                                         : fmaxf(1, h * ratio);
            if (!isfinite(h) || h < 0 || h > 10000000)
                return false;
            if (!isfinite(w) || w > 10000000 || s->glyph_count >= s->workspace.glyph_capacity)
                return false;
            if (cursor_x > 0 && cursor_x + w > available &&
                !(b->kind == CUI_TEXT_VIEW_TABLE_CELL &&
                  o.table_mode == CUI_TEXT_VIEW_TABLE_NOWRAP)) {
                cursor_x = 0;
                float height = cui_text_view_finish_inline_line(s, line_first,
                                                                y + padding + cursor_y, line, o);
                if (!isfinite(height))
                    return false;
                cursor_y += height;
                line_first = s->glyph_count;
            }
            float top = 0;
            if (o.font_metrics_available) {
                float baseline = (line - o.body_metrics.ascent - o.body_metrics.descent) * .5f +
                                 o.body_metrics.ascent;
                float image_baseline = fminf(h, fmaxf(0, h * .5f + baseline - line * .5f));
                top = fmaxf(baseline, image_baseline) - image_baseline;
            }
            r->image_bounds = (cui_rect){x + left + cursor_x, y + padding + cursor_y + top, w, h};
            s->workspace.glyphs[s->glyph_count++] = (cui_text_view_glyph){
                .text = r->text,
                .span = span,
                .block = index,
                .bounds = {x + left + cursor_x, y + padding + cursor_y, w, line},
                .font_size = size,
                .line_height = line,
                .flags = flags,
                .line_top = y + padding + cursor_y};
            cursor_x += w;
            continue;
        }
        size_t segment = r->text.begin;
        float measured = 0;
        for (size_t i = r->text.begin; i < r->text.end;) {
            size_t n = cui_text_view_scalar(cui_text_view_rendered_text(s), i);
            if (!n)
                return false;
            if (s->workspace.rendered[i] == '\n') {
                cursor_x = 0;
                float height = cui_text_view_finish_inline_line(s, line_first,
                                                                y + padding + cursor_y, line, o);
                if (!isfinite(height))
                    return false;
                cursor_y += height;
                line_first = s->glyph_count;
                i += n;
                segment = i;
                measured = 0;
                continue;
            }
            bool word = s->workspace.rendered[i] != ' ' && s->workspace.rendered[i] != '\t' &&
                        (i == r->text.begin || s->workspace.rendered[i - 1] == ' ' ||
                         s->workspace.rendered[i - 1] == '\n');
            if (word && cursor_x > 0 &&
                !(b->kind == CUI_TEXT_VIEW_TABLE_CELL &&
                  o.table_mode == CUI_TEXT_VIEW_TABLE_NOWRAP)) {
                size_t end = i;
                while (end < r->text.end && s->workspace.rendered[end] != ' ' &&
                       s->workspace.rendered[end] != '\t' && s->workspace.rendered[end] != '\n')
                    end += cui_text_view_scalar(cui_text_view_rendered_text(s), end);
                cui_size word_size =
                    measure(user, (cui_str){s->workspace.rendered + i, end - i}, size, flags);
                if (!isfinite(word_size.width) || word_size.width < 0 || word_size.width > 10000000)
                    return false;
                if (word_size.width > available - cursor_x) {
                    cursor_x = 0;
                    float height = cui_text_view_finish_inline_line(
                        s, line_first, y + padding + cursor_y, line, o);
                    if (!isfinite(height))
                        return false;
                    cursor_y += height;
                    line_first = s->glyph_count;
                    segment = i;
                    measured = 0;
                }
            }
            cui_size m = measure(user, (cui_str){s->workspace.rendered + segment, i + n - segment},
                                 size, flags);
            if (!isfinite(m.width) || m.width < 0 || m.width > 10000000)
                return false;
            float advance = fmaxf(0, m.width - measured);
            if (cursor_x > 0 && cursor_x + advance > available &&
                !(b->kind == CUI_TEXT_VIEW_TABLE_CELL &&
                  o.table_mode == CUI_TEXT_VIEW_TABLE_NOWRAP)) {
                cursor_x = 0;
                float height = cui_text_view_finish_inline_line(s, line_first,
                                                                y + padding + cursor_y, line, o);
                if (!isfinite(height))
                    return false;
                cursor_y += height;
                line_first = s->glyph_count;
                segment = i;
                measured = 0;
                m = measure(user, (cui_str){s->workspace.rendered + i, n}, size, flags);
                advance = m.width;
                if (!isfinite(advance) || advance < 0 || advance > 10000000)
                    return false;
            }
            if (s->glyph_count >= s->workspace.glyph_capacity)
                return false;
            s->workspace.glyphs[s->glyph_count++] = (cui_text_view_glyph){
                .text = {i, i + n},
                .span = span,
                .block = index,
                .bounds = {x + left + cursor_x, y + padding + cursor_y, advance, line},
                .font_size = size,
                .line_height = line,
                .flags = flags,
                .line_top = y + padding + cursor_y};
            cursor_x += advance;
            measured = m.width;
            i += n;
        }
    }
    if (b->kind == CUI_TEXT_VIEW_TABLE_CELL && b->alignment != CUI_TEXT_VIEW_ALIGN_LEFT) {
        for (size_t first = first_glyph; first < s->glyph_count;) {
            size_t last = first + 1;
            while (last < s->glyph_count &&
                   s->workspace.glyphs[last].line_top == s->workspace.glyphs[first].line_top)
                ++last;
            cui_rect tail = s->workspace.glyphs[last - 1].bounds;
            float extra = fmaxf(0, available - (tail.x + tail.w - x - left));
            float shift = b->alignment == CUI_TEXT_VIEW_ALIGN_CENTER ? extra * .5f : extra;
            for (size_t j = first; j < last; ++j)
                s->workspace.glyphs[j].bounds.x += shift;
            first = last;
        }
    }
    for (size_t i = first_glyph; i < s->glyph_count; ++i) {
        cui_text_view_glyph *glyph = &s->workspace.glyphs[i];
        cui_text_view_span *span = &s->workspace.spans[glyph->span];
        if (span->flags & CUI_TEXT_VIEW_INLINE_IMAGE)
            span->image_bounds.x = glyph->bounds.x;
    }
    b->actions_height = b->kind == CUI_TEXT_VIEW_CODE_BLOCK ? o.code_actions_height : 0;
    float final_height =
        cui_text_view_finish_inline_line(s, line_first, y + padding + cursor_y, line, o);
    if (!isfinite(final_height))
        return false;
    b->bounds = (cui_rect){x, y, width, cursor_y + final_height + padding * 2};
    return true;
}
static bool cui_text_view_table_widths(cui_text_view_state *s, size_t first, size_t end,
                                       cui_text_view_layout_options o,
                                       cui_text_view_measure_fn measure, void *user) {
    float weights[CUI_TEXT_VIEW_NODE_LIMIT], widths[CUI_TEXT_VIEW_NODE_LIMIT],
        floors[CUI_TEXT_VIEW_NODE_LIMIT];
    unsigned columns = 0;
    for (size_t i = first; i < end; ++i)
        if (s->workspace.blocks[i].column + 1 > columns)
            columns = s->workspace.blocks[i].column + 1;
    bool adaptive = o.table_mode != CUI_TEXT_VIEW_TABLE_WRAP;
    for (unsigned c = 0; c < columns; ++c) {
        weights[c] = adaptive ? 48 : 5;
        widths[c] = 0;
        floors[c] = 64;
    }
    for (size_t i = first; i < end; ++i) {
        const cui_text_view_block *cell = &s->workspace.blocks[i];
        if (!adaptive) {
            unsigned count = 0;
            for (size_t j = cell->text.begin; j < cell->text.end && count < 150; ++j)
                if (((unsigned char)s->workspace.rendered[j] & 0xc0) != 0x80)
                    ++count;
            weights[cell->column] = fmaxf(weights[cell->column], (float)count);
            continue;
        }
        float maximum = 0;
        for (size_t line = cell->text.begin; line < cell->text.end;) {
            size_t end_line = line;
            while (end_line < cell->text.end && s->workspace.rendered[end_line] != '\n')
                ++end_line;
            size_t begin = line, finish = end_line;
            while (begin < finish &&
                   (s->workspace.rendered[begin] == ' ' || s->workspace.rendered[begin] == '\t'))
                ++begin;
            while (finish > begin && (s->workspace.rendered[finish - 1] == ' ' ||
                                      s->workspace.rendered[finish - 1] == '\t'))
                --finish;
            float width = 0;
            for (size_t j = 0; j < s->span_count; ++j) {
                const cui_text_view_span *span = &s->workspace.spans[j];
                if (span->block != i || span->text.end <= begin || span->text.begin >= finish)
                    continue;
                size_t start = span->text.begin > begin ? span->text.begin : begin,
                       stop = span->text.end < finish ? span->text.end : finish;
                float size = o.font_size * ((span->flags & CUI_TEXT_VIEW_CODE) ? .875f : 1);
                cui_size measured =
                    measure(user, (cui_str){s->workspace.rendered + start, stop - start}, size,
                            span->flags);
                if (!isfinite(measured.width) || measured.width < 0 || measured.width > 10000000)
                    return false;
                width += measured.width;
                if (span->flags & CUI_TEXT_VIEW_CODE)
                    width += 4;
            }
            if (!isfinite(width) || width > 10000000)
                return false;
            maximum = fmaxf(maximum, width);
            line = end_line + (end_line < cell->text.end);
        }
        weights[cell->column] =
            fmaxf(weights[cell->column], maximum + 16 + (cell->column + 1 < columns ? 1 : 0));
    }
    float total = 0, minimum = 0;
    for (unsigned c = 0; c < columns; ++c) {
        if (adaptive)
            floors[c] = o.table_mode == CUI_TEXT_VIEW_TABLE_NOWRAP
                            ? weights[c]
                            : fminf(weights[c], fminf(480, fmaxf(160, weights[c] * .5f)));
        total += weights[c];
        minimum += floors[c];
    }
    float remaining = fmaxf(adaptive ? fmaxf(0, o.width - 2) : o.width, minimum);
    if (!isfinite(remaining) || remaining > 10000000 || total > 10000000)
        return false;
    bool changed;
    do {
        changed = false;
        for (unsigned c = 0; c < columns; ++c)
            if (widths[c] == 0 && remaining * (weights[c] / total) < floors[c]) {
                widths[c] = floors[c];
                remaining -= floors[c];
                total -= weights[c];
                changed = true;
            }
    } while (changed && total > 0);
    float x = 0;
    for (unsigned c = 0; c < columns; ++c) {
        if (widths[c] == 0)
            widths[c] = remaining * (weights[c] / total);
        for (size_t i = first; i < end; ++i)
            if (s->workspace.blocks[i].column == c) {
                s->workspace.blocks[i].bounds.x = x;
                s->workspace.blocks[i].bounds.w = widths[c];
                s->workspace.blocks[i].table_columns = columns;
            }
        x += widths[c];
    }
    return true;
}
cui_text_view_error cui_text_view_layout(cui_text_view_state *s, cui_text_view_layout_options o,
                                         cui_text_view_measure_fn measure, void *user) {
    if (!s || !measure || !isfinite(o.width) || o.width <= 0 || o.width > 10000000 ||
        !isfinite(o.font_size) || o.font_size <= 0 || o.font_size > 10000000 ||
        !isfinite(o.line_height) || o.line_height <= 0 || o.line_height > 10000000 ||
        !isfinite(o.paragraph_gap) || o.paragraph_gap < 0 || o.paragraph_gap > 10000000 ||
        !isfinite(o.code_actions_height) || o.code_actions_height < 0 ||
        o.code_actions_height > 10000000 || !isfinite(o.table_actions_height) ||
        o.table_actions_height < 0 || o.table_actions_height > 10000000 ||
        o.table_mode < CUI_TEXT_VIEW_TABLE_WRAP || o.table_mode > CUI_TEXT_VIEW_TABLE_NOWRAP)
        return CUI_TEXT_VIEW_INVALID;
    if (o.font_metrics_available && !cui_text_view_metrics_valid(o.body_metrics))
        return CUI_TEXT_VIEW_INVALID;
    if (s->text_length > s->workspace.glyph_capacity)
        return CUI_TEXT_VIEW_CAPACITY;
    for (size_t i = 0; i < s->span_count; ++i) {
        cui_text_view_span *span = &s->workspace.spans[i];
        if (!(span->flags & CUI_TEXT_VIEW_INLINE_IMAGE))
            continue;
        cui_text_view_image_length lengths[] = {span->image_width, span->image_height};
        for (size_t j = 0; j < 2; ++j)
            if (lengths[j].specified && (!isfinite(lengths[j].value) || lengths[j].value < 0 ||
                                         lengths[j].value > 10000000))
                return CUI_TEXT_VIEW_INVALID;
    }
    s->glyph_count = 0;
    float y = 0;
    size_t table_first = 0;
    for (size_t b = 0; b < s->block_count;) {
        cui_text_view_block *block = &s->workspace.blocks[b];
        if (block->kind == CUI_TEXT_VIEW_TABLE_CELL) {
            unsigned table = block->table, row = block->row;
            if (b == 0 || s->workspace.blocks[b - 1].kind != CUI_TEXT_VIEW_TABLE_CELL ||
                s->workspace.blocks[b - 1].table != table) {
                table_first = b;
                float indent = block->list_continuation ? (block->list_depth + 1) * 16.f : 0;
                block->list_prefix_width = 0;
                if (block->list_item) {
                    if (!cui_text_view_prepare_list_prefix(block, o.font_size, measure, user,
                                                           &block->list_prefix_width))
                        return CUI_TEXT_VIEW_INVALID;
                    indent = block->list_depth * 16.f + block->list_prefix_width;
                }
                cui_text_view_layout_options table_options = o;
                table_options.width = fmaxf(1, o.width - indent);
                s->workspace.blocks[b].table_frame = (cui_rect){indent, y, table_options.width, 0};
                if (o.table_mode != CUI_TEXT_VIEW_TABLE_WRAP)
                    y += 1;
                size_t end = b + 1;
                while (end < s->block_count &&
                       s->workspace.blocks[end].kind == CUI_TEXT_VIEW_TABLE_CELL &&
                       s->workspace.blocks[end].table == table)
                    ++end;
                if (!cui_text_view_table_widths(s, b, end, table_options, measure, user))
                    return CUI_TEXT_VIEW_INVALID;
                for (size_t cell = b; cell < end; ++cell)
                    s->workspace.blocks[cell].bounds.x += indent;
            }
            float height = 0;
            size_t first = b;
            while (b < s->block_count && s->workspace.blocks[b].kind == CUI_TEXT_VIEW_TABLE_CELL &&
                   s->workspace.blocks[b].table == table && s->workspace.blocks[b].row == row) {
                float x = s->workspace.blocks[b].bounds.x, w = s->workspace.blocks[b].bounds.w;
                if (o.table_mode != CUI_TEXT_VIEW_TABLE_WRAP)
                    x += 1;
                if (!cui_text_view_layout_block(s, b, x, y, w, o, measure, user))
                    return CUI_TEXT_VIEW_INVALID;
                if (s->workspace.blocks[b].bounds.h > height)
                    height = s->workspace.blocks[b].bounds.h;
                ++b;
            }
            for (size_t i = first; i < b; ++i)
                s->workspace.blocks[i].bounds.h = height;
            y += height;
            if (b == s->block_count || s->workspace.blocks[b].kind != CUI_TEXT_VIEW_TABLE_CELL ||
                s->workspace.blocks[b].table != table) {
                cui_text_view_block *first_cell = &s->workspace.blocks[table_first];
                cui_text_view_block *last_cell = &s->workspace.blocks[b - 1];
                if (o.table_mode != CUI_TEXT_VIEW_TABLE_WRAP)
                    y += 1;
                first_cell->table_frame.h = y - first_cell->table_frame.y;
                float track_end = 0;
                for (size_t cell = table_first; cell < b; ++cell)
                    track_end = fmaxf(track_end, s->workspace.blocks[cell].bounds.x +
                                                     s->workspace.blocks[cell].bounds.w);
                first_cell->table_track_width = track_end - first_cell->table_frame.x +
                                                (o.table_mode != CUI_TEXT_VIEW_TABLE_WRAP ? 1 : 0);
                last_cell->actions_height = o.table_actions_height;
                bool following = b < s->block_count;
                bool list_following = following && (s->workspace.blocks[b].list_item ||
                                                    s->workspace.blocks[b].list_continuation);
                y += (following && (!(first_cell->list_item || first_cell->list_continuation) ||
                                    !list_following)
                          ? o.paragraph_gap
                          : 0) +
                     (o.table_actions_height > 0 ? o.table_actions_height + 4 : 0);
            }
        } else {
            float indent =
                block->list_item || block->list_continuation
                    ? block->list_depth * 16.f +
                          (block->list_continuation && !block->list_repeat_marker ? 16 : 0)
                    : 0;
            if (!cui_text_view_layout_block(s, b, indent, y, fmaxf(1, o.width - indent), o, measure,
                                            user))
                return CUI_TEXT_VIEW_INVALID;
            float height = block->kind == CUI_TEXT_VIEW_IMAGE && block->image_ready
                               ? block->image_line.h
                               : block->bounds.h;
            y += block->list_item ? fmaxf(height, o.line_height) : height;
            if (b + 1 < s->block_count && ((!block->list_item && !block->list_continuation) ||
                                           (!s->workspace.blocks[b + 1].list_item &&
                                            !s->workspace.blocks[b + 1].list_continuation)))
                y += block->kind == CUI_TEXT_VIEW_HEADING ? o.font_size * (16.f / 14) * .3f
                                                          : o.paragraph_gap;
            ++b;
        }
        if (!isfinite(y) || y > 10000000)
            return CUI_TEXT_VIEW_INVALID;
    }
    s->width = o.width;
    s->height = y;
    s->clamped = false;
    if (o.max_lines && !o.scrollable) {
        float cap = (float)o.max_lines * o.line_height;
        if (y > cap) {
            s->clamped = true;
            s->height = cap;
        }
        s->reveal_pending = false;
    }
    return CUI_TEXT_VIEW_OK;
}
size_t cui_text_view_hit(const cui_text_view_state *s, float x, float y) {
    if (!s || !isfinite(x) || !isfinite(y))
        return SIZE_MAX;
    float nearest = INFINITY;
    size_t offset = 0;
    for (size_t i = 0; i < s->glyph_count; ++i) {
        const cui_text_view_glyph *g = &s->workspace.glyphs[i];
        float dy = y < g->bounds.y                 ? g->bounds.y - y
                   : y > g->bounds.y + g->bounds.h ? y - g->bounds.y - g->bounds.h
                                                   : 0;
        float dx = x < g->bounds.x                 ? g->bounds.x - x
                   : x > g->bounds.x + g->bounds.w ? x - g->bounds.x - g->bounds.w
                                                   : 0;
        float distance = dy * dy * 10000 + dx * dx;
        if (distance < nearest) {
            nearest = distance;
            offset = x > g->bounds.x + g->bounds.w * .5f ? g->text.end : g->text.begin;
        }
    }
    return offset;
}

/* Two-pass writer keeps copy capacity failures transactional. */
typedef struct {
    char *output;
    size_t length;
    bool flatten, indent_pending;
    unsigned newline_indent;
    const char *newline_prefix;
} cui_text_view_copy_writer;
static void cui_text_view_copy_write(cui_text_view_copy_writer *w, cui_str t) {
    if ((w->newline_indent || w->newline_prefix) && !w->flatten) {
        for (size_t i = 0; i < t.length; ++i) {
            if (w->indent_pending) {
                if (w->output)
                    memset(w->output + w->length, ' ', w->newline_indent);
                w->length += w->newline_indent;
                if (w->newline_prefix) {
                    size_t prefix_length = strlen(w->newline_prefix);
                    if (w->output)
                        memcpy(w->output + w->length, w->newline_prefix, prefix_length);
                    w->length += prefix_length;
                }
                w->indent_pending = false;
            }
            if (w->output)
                w->output[w->length] = t.data[i];
            ++w->length;
            if (t.data[i] == '\n')
                w->indent_pending = true;
        }
        return;
    }
    if (w->output && t.length) {
        memcpy(w->output + w->length, t.data, t.length);
        if (w->flatten)
            for (size_t i = 0; i < t.length; ++i)
                if (w->output[w->length + i] == '\n')
                    w->output[w->length + i] = ' ';
    }
    w->length += t.length;
}
static void cui_text_view_copy_literal(cui_text_view_copy_writer *w, const char *t) {
    cui_text_view_copy_write(w, (cui_str){t, strlen(t)});
}
static void cui_text_view_copy_escape(cui_text_view_copy_writer *w, cui_str t, bool destination) {
    for (size_t i = 0; i < t.length; ++i) {
        char c = t.data[i];
        if (!destination && c == '\n' && !w->flatten) {
            cui_text_view_copy_literal(w, "  \n");
            continue;
        }
        if (c == '\\' ||
            (destination ? c == '(' || c == ')'
                         : c == '*' || c == '_' || c == '[' || c == ']' || c == '~' || c == '#' ||
                               c == '>' || c == '|' || c == '&' || c == '<' || c == '`'))
            cui_text_view_copy_literal(w, "\\");
        cui_text_view_copy_write(w, (cui_str){t.data + i, 1});
    }
}
static unsigned cui_text_view_copy_ticks(cui_str t) {
    unsigned longest = 0, run = 0;
    for (size_t i = 0; i < t.length; ++i) {
        run = t.data[i] == '`' ? run + 1 : 0;
        if (run > longest)
            longest = run;
    }
    return longest + 1;
}
static void cui_text_view_copy_repeat(cui_text_view_copy_writer *w, const char *t, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
        cui_text_view_copy_literal(w, t);
}
static void cui_text_view_copy_inline(const cui_text_view_state *s, cui_text_view_copy_writer *w,
                                      cui_text_view_range r, size_t block) {
    for (size_t i = 0; i < s->span_count; ++i) {
        const cui_text_view_span *span = &s->workspace.spans[i];
        if (span->block != block || span->text.end <= r.begin || span->text.begin >= r.end)
            continue;
        size_t a = span->text.begin > r.begin ? span->text.begin : r.begin;
        size_t b = span->text.end < r.end ? span->text.end : r.end;
        cui_str t = {s->workspace.rendered + a, b - a};
        if (span->flags & CUI_TEXT_VIEW_INLINE_IMAGE) {
            bool linked = span->link.end > span->link.begin;
            if (linked)
                cui_text_view_copy_literal(w, "[");
            cui_text_view_copy_write(w, (cui_str){s->workspace.source + span->source.begin,
                                                  span->source.end - span->source.begin});
            if (linked) {
                cui_text_view_copy_literal(w, "](");
                cui_text_view_copy_escape(w,
                                          (cui_str){s->workspace.source + span->link.begin,
                                                    span->link.end - span->link.begin},
                                          true);
                cui_text_view_copy_literal(w, ")");
            }
            continue;
        }
        bool link = span->link.end > span->link.begin;
        unsigned flags = span->flags, ticks = 0;
        if (link)
            cui_text_view_copy_literal(w, "[");
        if (flags & CUI_TEXT_VIEW_BOLD)
            cui_text_view_copy_literal(w, "**");
        if (flags & CUI_TEXT_VIEW_ITALIC)
            cui_text_view_copy_literal(w, "*");
        if (flags & CUI_TEXT_VIEW_STRIKE)
            cui_text_view_copy_literal(w, "~~");
        if (flags & CUI_TEXT_VIEW_CODE) {
            ticks = cui_text_view_copy_ticks(t);
            cui_text_view_copy_repeat(w, "`", ticks);
            if (t.length && (t.data[0] == '`' || t.data[t.length - 1] == '`'))
                cui_text_view_copy_literal(w, " ");
            cui_text_view_copy_write(w, t);
            if (t.length && (t.data[0] == '`' || t.data[t.length - 1] == '`'))
                cui_text_view_copy_literal(w, " ");
            cui_text_view_copy_repeat(w, "`", ticks);
        } else
            cui_text_view_copy_escape(w, t, false);
        if (flags & CUI_TEXT_VIEW_STRIKE)
            cui_text_view_copy_literal(w, "~~");
        if (flags & CUI_TEXT_VIEW_ITALIC)
            cui_text_view_copy_literal(w, "*");
        if (flags & CUI_TEXT_VIEW_BOLD)
            cui_text_view_copy_literal(w, "**");
        if (link) {
            cui_text_view_copy_literal(w, "](");
            cui_text_view_copy_escape(w,
                                      (cui_str){s->workspace.source + span->link.begin,
                                                span->link.end - span->link.begin},
                                      true);
            cui_text_view_copy_literal(w, ")");
        }
    }
}
static void cui_text_view_copy_table(const cui_text_view_state *s, cui_text_view_copy_writer *w,
                                     size_t start, size_t end, size_t a, size_t b) {
    unsigned row = s->workspace.blocks[start].row;
    unsigned columns = 0;
    cui_text_view_copy_literal(w, "| ");
    w->flatten = true;
    for (size_t i = start; i < end; ++i) {
        const cui_text_view_block *cell = &s->workspace.blocks[i];
        if (cell->row != row) {
            w->flatten = false;
            cui_text_view_copy_literal(w, " |\n");
            if (row == 0) {
                cui_text_view_copy_literal(w, "| ");
                for (unsigned col = 0; col < columns; ++col) {
                    if (col)
                        cui_text_view_copy_literal(w, " | ");
                    cui_text_view_alignment align = s->workspace.blocks[start + col].alignment;
                    cui_text_view_copy_literal(w, align == CUI_TEXT_VIEW_ALIGN_CENTER  ? ":-:"
                                                  : align == CUI_TEXT_VIEW_ALIGN_RIGHT ? "--:"
                                                                                       : ":--");
                }
                cui_text_view_copy_literal(w, " |\n");
            }
            row = cell->row;
            columns = 0;
            cui_text_view_copy_literal(w, "| ");
            w->flatten = true;
        }
        if (columns)
            cui_text_view_copy_literal(w, " | ");
        ++columns;
        cui_text_view_range selected = cui_text_view_block_selection(s, i, a, b);
        size_t begin = selected.begin, finish = selected.end;
        while (begin < finish && s->workspace.rendered[begin] == ' ' &&
               !(s->workspace.mapping[begin].flags & CUI_TEXT_VIEW_INLINE_IMAGE))
            ++begin;
        while (finish > begin && s->workspace.rendered[finish - 1] == ' ' &&
               !(s->workspace.mapping[finish - 1].flags & CUI_TEXT_VIEW_INLINE_IMAGE))
            --finish;
        if (begin < finish)
            cui_text_view_copy_inline(s, w, (cui_text_view_range){begin, finish}, i);
    }
    w->flatten = false;
    cui_text_view_copy_literal(w, " |");
    if (row == 0) {
        cui_text_view_copy_literal(w, "\n| ");
        for (unsigned col = 0; col < columns; ++col) {
            if (col)
                cui_text_view_copy_literal(w, " | ");
            cui_text_view_alignment align = s->workspace.blocks[start + col].alignment;
            cui_text_view_copy_literal(w, align == CUI_TEXT_VIEW_ALIGN_CENTER  ? ":-:"
                                          : align == CUI_TEXT_VIEW_ALIGN_RIGHT ? "--:"
                                                                               : ":--");
        }
        cui_text_view_copy_literal(w, " |");
    }
}
static void cui_text_view_copy_list_prefix(cui_text_view_copy_writer *w,
                                           const cui_text_view_block *block, bool content) {
    cui_text_view_copy_repeat(w, " ", block->list_source_indent);
    cui_text_view_copy_literal(w, block->list_ordered ? block->marker_text : "-");
    cui_text_view_copy_literal(w, " ");
    if (block->task) {
        cui_text_view_copy_literal(w, block->checked ? "[x]" : "[ ]");
        if (content)
            cui_text_view_copy_literal(w, " ");
    }
}
static cui_text_view_error cui_text_view_copy_source(const cui_text_view_state *s,
                                                     cui_text_view_copy_writer *w, size_t a,
                                                     size_t b) {
    bool prior = false, prior_list = false;
    bool listed[CUI_TEXT_VIEW_NODE_LIMIT] = {0};
    uint16_t order[CUI_TEXT_VIEW_NODE_LIMIT];
    for (size_t i = 0; i < s->block_count; ++i)
        order[i] = (uint16_t)i;
    /* Source serialization groups an item's paragraphs before its nested lists,
     * independently of the document's paint/selection order. */
    for (size_t i = 0; i < s->block_count; ++i) {
        const cui_text_view_block *block = &s->workspace.blocks[i];
        if (!block->list_continuation)
            continue;
        if (block->list_parent >= i)
            return CUI_TEXT_VIEW_INVALID;
        size_t parent = 0, position = 0;
        while (order[parent] != block->list_parent)
            ++parent;
        while (order[position] != i)
            ++position;
        size_t target = parent + 1;
        while (target < position && s->workspace.blocks[order[target]].list_continuation &&
               s->workspace.blocks[order[target]].list_parent == block->list_parent)
            ++target;
        if (target < position) {
            memmove(order + target + 1, order + target, (position - target) * sizeof *order);
            order[target] = (uint16_t)i;
        }
    }
    for (size_t position = 0; position < s->block_count; ++position) {
        size_t i = order[position];
        const cui_text_view_block *block = &s->workspace.blocks[i];
        if (block->text.end <= a || block->text.begin >= b)
            continue;
        cui_text_view_range selected = cui_text_view_block_selection(s, i, a, b);
        size_t begin = selected.begin, end = selected.end;
        if (begin >= end)
            continue;
        if (block->list_continuation && block->list_parent >= i)
            return CUI_TEXT_VIEW_INVALID;
        bool first_continuation = block->list_continuation && !listed[block->list_parent];
        if (block->list_item || block->list_continuation) {
            size_t parents[CUI_TEXT_VIEW_LIST_DEPTH_LIMIT], count = 0;
            for (size_t parent = first_continuation
                                     ? s->workspace.blocks[block->list_parent].list_parent
                                     : block->list_parent;
                 parent != SIZE_MAX; parent = s->workspace.blocks[parent].list_parent) {
                if (parent >= i || count == CUI_TEXT_VIEW_LIST_DEPTH_LIMIT)
                    return CUI_TEXT_VIEW_INVALID;
                parents[count++] = parent;
            }
            while (count) {
                size_t parent = parents[--count];
                if (listed[parent])
                    continue;
                if (prior)
                    cui_text_view_copy_literal(w, prior_list ? "\n" : "\n\n");
                cui_text_view_copy_list_prefix(w, &s->workspace.blocks[parent], false);
                listed[parent] = true;
                prior = prior_list = true;
            }
            listed[i] = true;
            if (block->list_continuation)
                listed[block->list_parent] = true;
        }
        if (prior)
            cui_text_view_copy_literal(
                w, prior_list && (block->list_item || block->list_continuation) ? "\n" : "\n\n");
        prior = true;
        prior_list = block->list_item || block->list_continuation;
        if (block->list_item)
            cui_text_view_copy_list_prefix(w, block, true);
        else if (block->list_continuation) {
            if (first_continuation)
                cui_text_view_copy_list_prefix(w, &s->workspace.blocks[block->list_parent], true);
            else
                cui_text_view_copy_repeat(w, " ", block->list_source_indent);
        }
        if (block->kind == CUI_TEXT_VIEW_TABLE_CELL) {
            size_t start = i, end = i + 1;
            while (start && s->workspace.blocks[start - 1].kind == CUI_TEXT_VIEW_TABLE_CELL &&
                   s->workspace.blocks[start - 1].table == block->table)
                --start;
            while (end < s->block_count &&
                   s->workspace.blocks[end].kind == CUI_TEXT_VIEW_TABLE_CELL &&
                   s->workspace.blocks[end].table == block->table)
                ++end;
            if (block->list_item)
                w->newline_indent =
                    block->list_source_indent +
                    (block->list_ordered ? (unsigned)strlen(block->marker_text) + 1 : 2);
            else if (block->list_continuation)
                w->newline_indent = block->list_source_indent;
            cui_text_view_copy_table(s, w, start, end, a, b);
            w->newline_indent = 0;
            w->indent_pending = false;
            position += end - i - 1;
            continue;
        }
        if (block->kind == CUI_TEXT_VIEW_HEADING) {
            cui_text_view_copy_repeat(w, "#", block->level);
            cui_text_view_copy_literal(w, " ");
        } else if (block->kind == CUI_TEXT_VIEW_QUOTE)
            cui_text_view_copy_literal(w, "> ");
        if (block->kind == CUI_TEXT_VIEW_CODE_BLOCK) {
            if (block->list_item)
                w->newline_indent =
                    block->list_source_indent +
                    (block->list_ordered ? (unsigned)strlen(block->marker_text) + 1 : 2);
            else if (block->list_continuation)
                w->newline_indent = block->list_source_indent;
            cui_str t = {s->workspace.rendered + begin, end - begin};
            unsigned ticks = cui_text_view_copy_ticks(t);
            if (ticks < 3)
                ticks = 3;
            cui_text_view_copy_repeat(w, "`", ticks);
            cui_text_view_copy_write(w, (cui_str){s->workspace.source + block->resource.begin,
                                                  block->resource.end - block->resource.begin});
            cui_text_view_copy_literal(w, "\n");
            cui_text_view_copy_write(w, t);
            if (!t.length || t.data[t.length - 1] != '\n')
                cui_text_view_copy_literal(w, "\n");
            cui_text_view_copy_repeat(w, "`", ticks);
            w->newline_indent = 0;
            w->indent_pending = false;
        } else if (block->kind == CUI_TEXT_VIEW_IMAGE) {
            cui_text_view_copy_literal(w, "![");
            cui_text_view_copy_escape(w, (cui_str){s->workspace.rendered + begin, end - begin},
                                      false);
            cui_text_view_copy_literal(w, "](");
            cui_text_view_copy_escape(w,
                                      (cui_str){s->workspace.source + block->resource.begin,
                                                block->resource.end - block->resource.begin},
                                      true);
            cui_text_view_copy_literal(w, ")");
        } else {
            if (block->list_item)
                w->newline_indent =
                    block->list_source_indent +
                    (block->list_ordered ? (unsigned)strlen(block->marker_text) + 1 : 2);
            else if (block->list_continuation)
                w->newline_indent = block->list_source_indent;
            if (block->kind == CUI_TEXT_VIEW_QUOTE)
                w->newline_prefix = "> ";
            cui_text_view_copy_inline(s, w, (cui_text_view_range){begin, end}, i);
            w->newline_prefix = NULL;
            w->newline_indent = 0;
            w->indent_pending = false;
        }
    }
    return CUI_TEXT_VIEW_OK;
}
static void cui_text_view_copy_plain(const cui_text_view_state *s, cui_text_view_copy_writer *w,
                                     size_t a, size_t b) {
    size_t start = a;
    for (size_t i = a; i < b; ++i) {
        if (!(s->workspace.mapping[i].flags & CUI_TEXT_VIEW_INLINE_IMAGE))
            continue;
        cui_text_view_copy_write(w, (cui_str){s->workspace.rendered + start, i - start});
        start = i + 1;
    }
    cui_text_view_copy_write(w, (cui_str){s->workspace.rendered + start, b - start});
}
cui_text_view_error cui_text_view_copy_selection(const cui_text_view_state *s,
                                                 cui_text_view_copy_format f, char *output,
                                                 size_t capacity, size_t *length) {
    if (!s || f < CUI_TEXT_VIEW_COPY_PLAIN || f > CUI_TEXT_VIEW_COPY_SOURCE ||
        (!output && capacity))
        return CUI_TEXT_VIEW_INVALID;
    size_t a = s->anchor < s->cursor ? s->anchor : s->cursor,
           b = s->anchor > s->cursor ? s->anchor : s->cursor;
    cui_str direct = {0};
    bool plain = f == CUI_TEXT_VIEW_COPY_PLAIN || s->format != CUI_TEXT_VIEW_MARKDOWN;
    if (!plain)
        cui_text_view_source_selection(s, &a, &b);
    if (!plain && !s->selection_projected && a == 0 && b == s->text_length && a != b)
        direct = cui_text_view_source(s);
    cui_text_view_copy_writer w = {0};
    cui_text_view_error error = CUI_TEXT_VIEW_OK;
    if (plain)
        cui_text_view_copy_plain(s, &w, a, b);
    else if (direct.data)
        cui_text_view_copy_write(&w, direct);
    else
        error = cui_text_view_copy_source(s, &w, a, b);
    if (error != CUI_TEXT_VIEW_OK)
        return error;
    if (length)
        *length = w.length;
    if (capacity <= w.length)
        return CUI_TEXT_VIEW_CAPACITY;
    w = (cui_text_view_copy_writer){.output = output};
    if (plain)
        cui_text_view_copy_plain(s, &w, a, b);
    else if (direct.data)
        cui_text_view_copy_write(&w, direct);
    else
        (void)cui_text_view_copy_source(s, &w, a, b);
    output[w.length] = 0;
    return CUI_TEXT_VIEW_OK;
}

static void cui_text_view_copy_plain_document(const cui_text_view_state *s,
                                              cui_text_view_copy_writer *w) {
    size_t a = s->anchor < s->cursor ? s->anchor : s->cursor,
           b = s->anchor > s->cursor ? s->anchor : s->cursor;
    if (s->selection_override) {
        cui_text_view_copy_plain(s, w, a, b);
        return;
    }
    for (size_t i = 0; i < s->block_count; ++i) {
        const cui_text_view_block *block = &s->workspace.blocks[i];
        cui_text_view_range r = cui_text_view_block_selection(s, i, a, b);
        if (block->kind == CUI_TEXT_VIEW_TABLE_CELL) {
            size_t end = i + 1;
            bool selected = r.begin < r.end;
            while (end < s->block_count &&
                   s->workspace.blocks[end].kind == CUI_TEXT_VIEW_TABLE_CELL &&
                   s->workspace.blocks[end].table == block->table) {
                cui_text_view_range cell = cui_text_view_block_selection(s, end, a, b);
                selected |= cell.begin < cell.end;
                ++end;
            }
            if (selected) {
                unsigned row = block->row;
                for (size_t j = i; j < end; ++j) {
                    const cui_text_view_block *cell = &s->workspace.blocks[j];
                    if (j != i)
                        cui_text_view_copy_literal(w, cell->row == row ? " " : "\n");
                    row = cell->row;
                    r = cui_text_view_block_selection(s, j, a, b);
                    if (r.begin < r.end)
                        cui_text_view_copy_plain(s, w, r.begin, r.end);
                }
                cui_text_view_copy_literal(w, "\n\n");
            }
            i = end - 1;
            continue;
        }
        if (r.begin >= r.end)
            continue;
        size_t before = w->length;
        cui_text_view_copy_plain(s, w, r.begin, r.end);
        if (w->length != before) {
            cui_text_view_copy_literal(w, "\n");
            if (block->kind == CUI_TEXT_VIEW_QUOTE &&
                (i + 1 == s->block_count || s->workspace.blocks[i + 1].kind != CUI_TEXT_VIEW_QUOTE))
                cui_text_view_copy_literal(w, "\n");
        }
    }
}
cui_text_view_error cui_text_view_copy_document_selection(const cui_text_view_state *s,
                                                          cui_text_view_copy_format format,
                                                          char *out, size_t capacity,
                                                          size_t *length) {
    if (!s || format < CUI_TEXT_VIEW_COPY_PLAIN || format > CUI_TEXT_VIEW_COPY_SOURCE ||
        (!out && capacity))
        return CUI_TEXT_VIEW_INVALID;
    if (format == CUI_TEXT_VIEW_COPY_SOURCE && s->format == CUI_TEXT_VIEW_MARKDOWN)
        return cui_text_view_copy_selection(s, format, out, capacity, length);
    cui_text_view_copy_writer w = {0};
    cui_text_view_copy_plain_document(s, &w);
    if (length)
        *length = w.length;
    if (capacity <= w.length)
        return CUI_TEXT_VIEW_CAPACITY;
    w = (cui_text_view_copy_writer){.output = out};
    cui_text_view_copy_plain_document(s, &w);
    out[w.length] = 0;
    return CUI_TEXT_VIEW_OK;
}
bool cui_text_view_selection_init(cui_text_view_selection *m,
                                  cui_text_view_selection_member *members, size_t capacity) {
    if (!m || !members || !capacity || capacity > CUI_TEXT_VIEW_NODE_LIMIT)
        return false;
    *m = (cui_text_view_selection){.members = members, .capacity = capacity};
    return true;
}
void cui_text_view_selection_clear(cui_text_view_selection *m) {
    if (!m)
        return;
    for (size_t i = 0; i < m->count; ++i) {
        cui_text_view_state *s = m->members[i].state;
        (void)cui_text_view_select(s, s->cursor, s->cursor);
        s->selecting = s->dragged = false;
    }
    m->active = m->dragging = m->dragged = false;
    m->owner = m->scope = 0;
}
bool cui_text_view_selection_register(cui_text_view_selection *m,
                                      cui_text_view_selection_member member) {
    if (!m || !m->members || !member.state || !member.id || !member.scope ||
        member.table_mode < CUI_TEXT_VIEW_TABLE_WRAP ||
        member.table_mode > CUI_TEXT_VIEW_TABLE_NOWRAP)
        return false;
    for (size_t i = 0; i < m->count; ++i) {
        if (m->members[i].id == member.id) {
            if (m->members[i].state != member.state)
                return false;
            bool reset = !member.enabled || m->members[i].scope != member.scope ||
                         m->members[i].table_mode != member.table_mode;
            if (reset) {
                (void)cui_text_view_select(member.state, member.state->cursor,
                                           member.state->cursor);
                if (m->owner == member.id)
                    cui_text_view_selection_clear(m);
            }
            m->members[i] = member;
            return true;
        }
        if (m->members[i].state == member.state)
            return false;
    }
    if (m->count == m->capacity)
        return false;
    m->members[m->count++] = member;
    return true;
}
bool cui_text_view_selection_remove(cui_text_view_selection *m, cui_id id) {
    if (!m)
        return false;
    for (size_t i = 0; i < m->count; ++i)
        if (m->members[i].id == id) {
            if (m->owner == id)
                cui_text_view_selection_clear(m);
            cui_text_view_state *s = m->members[i].state;
            (void)cui_text_view_select(s, s->cursor, s->cursor);
            s->selecting = s->dragged = false;
            memmove(m->members + i, m->members + i + 1, (m->count - i - 1) * sizeof *m->members);
            --m->count;
            return true;
        }
    return false;
}
static bool cui_text_view_selection_band(cui_rect r, float ax, float ay, float bx, float by) {
    float top = fminf(ay, by), bottom = fmaxf(ay, by), x = r.x + r.w / 2;
    if (r.y + r.h <= top || r.y > bottom)
        return false;
    bool a = ay >= r.y && ay < r.y + r.h, b = by >= r.y && by < r.y + r.h;
    if (a && b)
        return x >= fminf(ax, bx) && x <= fmaxf(ax, bx);
    float tx = ay < by ? ax : bx, ty = fminf(ay, by), lx = ay < by ? bx : ax, ly = fmaxf(ay, by);
    if (ty >= r.y && ty < r.y + r.h)
        return x >= tx;
    if (ly >= r.y && ly < r.y + r.h)
        return x <= lx;
    return true;
}
bool cui_text_view_selection_project(cui_text_view_selection *m, cui_id scope, float ax, float ay,
                                     float bx, float by) {
    if (!m || !isfinite(ax) || !isfinite(ay) || !isfinite(bx) || !isfinite(by))
        return false;
    bool changed = false;
    m->scope = scope;
    m->anchor_x = ax;
    m->anchor_y = ay;
    m->cursor_x = bx;
    m->cursor_y = by;
    m->active = true;
    bool forward = ay < by || (ay == by && ax <= bx);
    for (size_t i = 0; i < m->count; ++i) {
        cui_text_view_selection_member *p = &m->members[i];
        cui_text_view_state *s = p->state;
        if (p->scope != scope)
            continue;
        size_t first = SIZE_MAX, last = 0;
        for (size_t j = 0; j < s->block_count; ++j)
            s->workspace.blocks[j].projected_next = (cui_text_view_range){0};
        if (p->enabled && p->scope == scope)
            for (size_t j = 0; j < s->glyph_count; ++j) {
                cui_text_view_glyph *g = &s->workspace.glyphs[j];
                if (g->flags & CUI_TEXT_VIEW_INLINE_IMAGE)
                    continue;
                cui_rect r = g->bounds;
                r.x += s->viewport.x;
                r.y += s->viewport.y - s->scroll.y;
                if (p->table_mode != CUI_TEXT_VIEW_TABLE_WRAP &&
                    s->workspace.blocks[g->block].kind == CUI_TEXT_VIEW_TABLE_CELL)
                    r.x -= s->workspace.blocks[g->block].table_scroll.x;
                if (r.y + r.h <= s->viewport.y || r.y >= s->viewport.y + s->viewport.h ||
                    r.x + r.w <= s->viewport.x || r.x >= s->viewport.x + s->viewport.w)
                    continue;
                if (cui_text_view_selection_band(r, ax, ay, bx, by)) {
                    if (first == SIZE_MAX)
                        first = g->text.begin;
                    last = g->text.end;
                    cui_text_view_range *range = &s->workspace.blocks[g->block].projected_next;
                    if (range->begin == range->end)
                        range->begin = g->text.begin;
                    range->end = g->text.end;
                }
            }
        size_t a = first == SIZE_MAX ? s->cursor : forward ? first : last;
        size_t b = first == SIZE_MAX ? s->cursor : forward ? last : first;
        changed |= s->anchor != a || s->cursor != b || !s->selection_projected;
        s->anchor = a;
        s->cursor = b;
        for (size_t j = 0; j < s->block_count; ++j) {
            cui_text_view_block *block = &s->workspace.blocks[j];
            changed |= block->projected_selection.begin != block->projected_next.begin ||
                       block->projected_selection.end != block->projected_next.end;
            block->projected_selection = block->projected_next;
        }
        s->selection_projected = true;
    }
    return changed;
}
static bool cui_text_view_selection_has_copy(const cui_text_view_state *s,
                                             cui_text_view_copy_format format) {
    size_t a = s->anchor < s->cursor ? s->anchor : s->cursor,
           b = s->anchor > s->cursor ? s->anchor : s->cursor;
    for (size_t block = 0; block < s->block_count; ++block) {
        cui_text_view_range r = cui_text_view_block_selection(s, block, a, b);
        for (size_t i = r.begin; i < r.end; ++i) {
            if (s->workspace.mapping[i].flags & CUI_TEXT_VIEW_INLINE_IMAGE) {
                if (format == CUI_TEXT_VIEW_COPY_SOURCE && s->format == CUI_TEXT_VIEW_MARKDOWN)
                    return true;
            } else if (!strchr(" \t\n\r\f\v", s->workspace.rendered[i]))
                return true;
        }
    }
    return false;
}
cui_text_view_error cui_text_view_selection_copy(const cui_text_view_selection *m,
                                                 cui_text_view_copy_format format, char *out,
                                                 size_t capacity, size_t *length) {
    if (!m || format < CUI_TEXT_VIEW_COPY_PLAIN || format > CUI_TEXT_VIEW_COPY_SOURCE ||
        (!out && capacity))
        return CUI_TEXT_VIEW_INVALID;
    size_t total = 0, selected = 0;
    for (size_t i = 0; i < m->count; ++i) {
        const cui_text_view_selection_member *p = &m->members[i];
        if (!p->enabled || p->scope != m->scope ||
            !cui_text_view_selection_has_copy(p->state, format))
            continue;
        size_t n = 0;
        cui_text_view_error error =
            cui_text_view_copy_document_selection(p->state, format, NULL, 0, &n);
        if (error != CUI_TEXT_VIEW_CAPACITY)
            return error;
        if (n > SIZE_MAX - total - (selected != 0))
            return CUI_TEXT_VIEW_CAPACITY;
        total += n + (selected++ != 0);
    }
    if (length)
        *length = total;
    if (capacity <= total)
        return CUI_TEXT_VIEW_CAPACITY;
    size_t written = 0, previous = SIZE_MAX;
    for (size_t ordinal = 0; ordinal < selected; ++ordinal) {
        size_t next = SIZE_MAX;
        for (size_t i = 0; i < m->count; ++i) {
            const cui_text_view_selection_member *p = &m->members[i];
            if (!p->enabled || p->scope != m->scope ||
                !cui_text_view_selection_has_copy(p->state, format))
                continue;
            if (previous != SIZE_MAX &&
                (p->document_order < m->members[previous].document_order ||
                 (p->document_order == m->members[previous].document_order && i <= previous)))
                continue;
            if (next == SIZE_MAX || p->document_order < m->members[next].document_order)
                next = i;
        }
        if (ordinal)
            out[written++] = '\n';
        size_t n;
        (void)cui_text_view_copy_document_selection(m->members[next].state, format, out + written,
                                                    capacity - written, &n);
        written += n;
        previous = next;
    }
    out[written] = 0;
    return CUI_TEXT_VIEW_OK;
}
