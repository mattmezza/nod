/* SPDX-License-Identifier: MIT
 * Private cui_theme_private_bounded JSON reader; no borrowed tokens escape an import call. */
#ifndef CUI_THEME_JSON_PRIVATE_H
#define CUI_THEME_JSON_PRIVATE_H
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define CUI_THEME_PRIVATE_THEME_JSON_NODES 4096
#define CUI_THEME_PRIVATE_THEME_JSON_DEPTH 16
enum {
    CUI_THEME_PRIVATE_J_OBJECT,
    CUI_THEME_PRIVATE_J_ARRAY,
    CUI_THEME_PRIVATE_J_STRING,
    CUI_THEME_PRIVATE_J_NUMBER,
    CUI_THEME_PRIVATE_J_TRUE,
    CUI_THEME_PRIVATE_J_FALSE,
    CUI_THEME_PRIVATE_J_NULL
};
typedef struct {
    unsigned type;
    size_t start, end, next, count;
} cui_theme_private_theme_json_node;
typedef struct {
    cui_str source;
    cui_theme_private_theme_json_node nodes[CUI_THEME_PRIVATE_THEME_JSON_NODES];
    /* One shared validation workspace per import, never per recursive level.
     * A string is decoded/validated completely before parsing resumes. */
    char decoded[CUI_THEME_JSON_MAX_BYTES + 1];
    size_t count, cursor;
    cui_theme_registry_error error;
} cui_theme_private_theme_json;
static bool cui_theme_private_theme_utf8(const char *s, size_t n) {
    for (size_t i = 0; i < n;) {
        unsigned c = (unsigned char)s[i++], v = c, k = 0, min = 0;
        if (c < 128) {
            if (!c)
                return false;
            continue;
        }
        if (c >= 0xc2 && c <= 0xdf) {
            v = c & 31;
            k = 1;
            min = 128;
        } else if (c >= 0xe0 && c <= 0xef) {
            v = c & 15;
            k = 2;
            min = 2048;
        } else if (c >= 0xf0 && c <= 0xf4) {
            v = c & 7;
            k = 3;
            min = 65536;
        } else
            return false;
        if (k > n - i)
            return false;
        while (k--) {
            c = (unsigned char)s[i++];
            if ((c & 192) != 128)
                return false;
            v = (v << 6) | (c & 63);
        }
        if (v < min || v > 0x10ffff || (v >= 0xd800 && v <= 0xdfff))
            return false;
    }
    return true;
}
static int cui_theme_private_theme_hex(unsigned c) {
    if (c >= '0' && c <= '9')
        return (int)c - '0';
    if (c >= 'a' && c <= 'f')
        return (int)c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return (int)c - 'A' + 10;
    return -1;
}
static bool cui_theme_private_json_string(const cui_theme_private_theme_json *j, size_t node,
                                          char *out, size_t cap) {
    if (node >= j->count || j->nodes[node].type != CUI_THEME_PRIVATE_J_STRING || !out || !cap)
        return false;
    cui_theme_private_theme_json_node t = j->nodes[node];
    size_t used = 0;
    for (size_t i = t.start; i < t.end;) {
        unsigned c = (unsigned char)j->source.data[i++];
        if (c == '\\') {
            if (i >= t.end)
                return false;
            c = (unsigned char)j->source.data[i++];
            if (c == 'u') {
                unsigned v = 0;
                if (t.end - i < 4)
                    return false;
                for (unsigned z = 0; z < 4; z++) {
                    int h = cui_theme_private_theme_hex((unsigned char)j->source.data[i++]);
                    if (h < 0)
                        return false;
                    v = (v << 4) | (unsigned)h;
                }
                if (v >= 0xd800 && v <= 0xdbff) {
                    if (t.end - i < 6 || j->source.data[i++] != '\\' || j->source.data[i++] != 'u')
                        return false;
                    unsigned low = 0;
                    for (unsigned z = 0; z < 4; z++) {
                        int h = cui_theme_private_theme_hex((unsigned char)j->source.data[i++]);
                        if (h < 0)
                            return false;
                        low = (low << 4) | (unsigned)h;
                    }
                    if (low < 0xdc00 || low > 0xdfff)
                        return false;
                    v = 0x10000 + ((v - 0xd800) << 10) + (low - 0xdc00);
                } else if (v >= 0xdc00 && v <= 0xdfff)
                    return false;
                if (!v)
                    return false;
                unsigned char bytes[4];
                size_t n;
                if (v < 128) {
                    bytes[0] = (unsigned char)v;
                    n = 1;
                } else if (v < 2048) {
                    bytes[0] = (unsigned char)(0xc0 | (v >> 6));
                    bytes[1] = (unsigned char)(0x80 | (v & 63));
                    n = 2;
                } else if (v < 65536) {
                    bytes[0] = (unsigned char)(0xe0 | (v >> 12));
                    bytes[1] = (unsigned char)(0x80 | ((v >> 6) & 63));
                    bytes[2] = (unsigned char)(0x80 | (v & 63));
                    n = 3;
                } else {
                    bytes[0] = (unsigned char)(0xf0 | (v >> 18));
                    bytes[1] = (unsigned char)(0x80 | ((v >> 12) & 63));
                    bytes[2] = (unsigned char)(0x80 | ((v >> 6) & 63));
                    bytes[3] = (unsigned char)(0x80 | (v & 63));
                    n = 4;
                }
                if (n >= cap - used)
                    return false;
                memcpy(out + used, bytes, n);
                used += n;
                continue;
            }
            switch (c) {
            case '"':
            case '\\':
            case '/':
                break;
            case 'b':
                c = 8;
                break;
            case 'f':
                c = 12;
                break;
            case 'n':
                c = 10;
                break;
            case 'r':
                c = 13;
                break;
            case 't':
                c = 9;
                break;
            default:
                return false;
            }
        }
        if (used + 1 >= cap || !c)
            return false;
        out[used++] = (char)c;
    }
    out[used] = 0;
    return cui_theme_private_theme_utf8(out, used);
}
static void cui_theme_private_json_space(cui_theme_private_theme_json *j) {
    while (j->cursor < j->source.length && strchr(" \t\n\r", j->source.data[j->cursor]))
        j->cursor++;
}
static bool cui_theme_private_json_value(cui_theme_private_theme_json *j, unsigned depth) {
    cui_theme_private_json_space(j);
    if (depth > CUI_THEME_PRIVATE_THEME_JSON_DEPTH ||
        j->count == CUI_THEME_PRIVATE_THEME_JSON_NODES) {
        j->error = CUI_THEME_REGISTRY_CAPACITY_ERROR;
        return false;
    }
    if (j->cursor >= j->source.length)
        return false;
    size_t index = j->count++, start = j->cursor;
    unsigned c = (unsigned char)j->source.data[j->cursor++];
    cui_theme_private_theme_json_node *n = &j->nodes[index];
    *n = (cui_theme_private_theme_json_node){.start = start};
    if (c == '{' || c == '[') {
        n->type = c == '{' ? CUI_THEME_PRIVATE_J_OBJECT : CUI_THEME_PRIVATE_J_ARRAY;
        cui_theme_private_json_space(j);
        char end = c == '{' ? '}' : ']';
        if (j->cursor < j->source.length && j->source.data[j->cursor] == end)
            j->cursor++;
        else
            for (;;) {
                if (c == '{' && (j->cursor >= j->source.length || j->source.data[j->cursor] != '"'))
                    return false;
                size_t key = j->count;
                if (!cui_theme_private_json_value(j, depth + 1))
                    return false;
                n->count++;
                if (c == '{') {
                    char current[256];
                    if (!cui_theme_private_json_string(j, key, current, sizeof current)) {
                        j->error = CUI_THEME_REGISTRY_CAPACITY_ERROR;
                        return false;
                    }
                    for (size_t previous = index + 1; previous < key;) {
                        char old[256];
                        if (!cui_theme_private_json_string(j, previous, old, sizeof old))
                            return false;
                        if (!strcmp(old, current))
                            return false;
                        previous = j->nodes[j->nodes[previous].next].next;
                    }
                    cui_theme_private_json_space(j);
                    if (j->cursor >= j->source.length || j->source.data[j->cursor++] != ':')
                        return false;
                    if (!cui_theme_private_json_value(j, depth + 1))
                        return false;
                    n->count++;
                }
                cui_theme_private_json_space(j);
                if (j->cursor >= j->source.length)
                    return false;
                char separator = j->source.data[j->cursor++];
                if (separator == end)
                    break;
                if (separator != ',')
                    return false;
                cui_theme_private_json_space(j);
            }
    } else if (c == '"') {
        n->type = CUI_THEME_PRIVATE_J_STRING;
        n->start = j->cursor;
        bool closed = false;
        while (j->cursor < j->source.length) {
            c = (unsigned char)j->source.data[j->cursor++];
            if (c == '"') {
                n->end = j->cursor - 1;
                closed = true;
                break;
            }
            if (c < 32)
                return false;
            if (c == '\\') {
                if (j->cursor >= j->source.length)
                    return false;
                j->cursor++;
            }
        }
        if (!closed)
            return false;
        /* Decode arbitrary source strings once for strict Unicode/escape validity.
         * Input is cui_theme_private_bounded; the temporary never becomes retained registry data.
         */
        if (!cui_theme_private_json_string(j, index, j->decoded, sizeof j->decoded))
            return false;
    } else if (c == 't' || c == 'f' || c == 'n') {
        const char *word = c == 't' ? "true" : c == 'f' ? "false" : "null";
        size_t length = strlen(word);
        if (length > j->source.length - start || memcmp(j->source.data + start, word, length))
            return false;
        j->cursor = start + length;
        n->type = c == 't'   ? CUI_THEME_PRIVATE_J_TRUE
                  : c == 'f' ? CUI_THEME_PRIVATE_J_FALSE
                             : CUI_THEME_PRIVATE_J_NULL;
    } else {
        n->type = CUI_THEME_PRIVATE_J_NUMBER;
        j->cursor = start;
        if (j->source.data[j->cursor] == '-')
            j->cursor++;
        if (j->cursor >= j->source.length)
            return false;
        if (j->source.data[j->cursor] == '0')
            j->cursor++;
        else {
            if (j->source.data[j->cursor] < '1' || j->source.data[j->cursor] > '9')
                return false;
            while (j->cursor < j->source.length &&
                   isdigit((unsigned char)j->source.data[j->cursor]))
                j->cursor++;
        }
        if (j->cursor < j->source.length && j->source.data[j->cursor] == '.') {
            j->cursor++;
            size_t digits = j->cursor;
            while (j->cursor < j->source.length &&
                   isdigit((unsigned char)j->source.data[j->cursor]))
                j->cursor++;
            if (j->cursor == digits)
                return false;
        }
        if (j->cursor < j->source.length &&
            (j->source.data[j->cursor] == 'e' || j->source.data[j->cursor] == 'E')) {
            j->cursor++;
            if (j->cursor < j->source.length &&
                (j->source.data[j->cursor] == '+' || j->source.data[j->cursor] == '-'))
                j->cursor++;
            size_t digits = j->cursor;
            while (j->cursor < j->source.length &&
                   isdigit((unsigned char)j->source.data[j->cursor]))
                j->cursor++;
            if (j->cursor == digits)
                return false;
        }
    }
    if (n->type != CUI_THEME_PRIVATE_J_STRING)
        n->end = j->cursor;
    n->next = j->count;
    return true;
}
static bool cui_theme_private_json_parse(cui_theme_private_theme_json *j, cui_str source) {
    memset(j, 0, sizeof *j);
    j->source = source;
    j->error = CUI_THEME_REGISTRY_INVALID;
    if (!source.data || !source.length || source.length > CUI_THEME_JSON_MAX_BYTES) {
        j->error = source.length > CUI_THEME_JSON_MAX_BYTES ? CUI_THEME_REGISTRY_CAPACITY_ERROR
                                                            : CUI_THEME_REGISTRY_INVALID;
        return false;
    }
    if (!cui_theme_private_json_value(j, 0))
        return false;
    cui_theme_private_json_space(j);
    return j->cursor == source.length;
}
/* Locale-independent decimal conversion; JSON uses '.', regardless of host locale. */
static bool cui_theme_private_theme_decimal(const char *text, size_t length, double *out) {
    if (!length || length >= 64)
        return false;
    size_t i = 0;
    bool negative = false;
    if (text[i] == '-' || text[i] == '+') {
        negative = text[i] == '-';
        if (++i == length)
            return false;
    }
    double value = 0;
    size_t digits = 0;
    while (i < length && text[i] >= '0' && text[i] <= '9') {
        value = value * 10 + (text[i++] - '0');
        digits++;
    }
    if (!digits)
        return false;
    if (i < length && text[i] == '.') {
        i++;
        double factor = .1;
        digits = 0;
        while (i < length && text[i] >= '0' && text[i] <= '9') {
            value += (text[i++] - '0') * factor;
            factor *= .1;
            digits++;
        }
        if (!digits)
            return false;
    }
    int exponent = 0;
    bool down = false;
    if (i < length && (text[i] == 'e' || text[i] == 'E')) {
        i++;
        if (i < length && (text[i] == '-' || text[i] == '+'))
            down = text[i++] == '-';
        digits = 0;
        while (i < length && text[i] >= '0' && text[i] <= '9') {
            if (exponent > 10000)
                return false;
            exponent = exponent * 10 + (text[i++] - '0');
            digits++;
        }
        if (!digits)
            return false;
    }
    if (i != length)
        return false;
    if (exponent)
        value *= pow(10, down ? -exponent : exponent);
    if (negative)
        value = -value;
    if (!isfinite(value))
        return false;
    *out = value;
    return true;
}
static bool cui_theme_private_json_number(const cui_theme_private_theme_json *j, size_t node,
                                          float *out) {
    if (node >= j->count || j->nodes[node].type != CUI_THEME_PRIVATE_J_NUMBER)
        return false;
    cui_theme_private_theme_json_node n = j->nodes[node];
    double value;
    if (!cui_theme_private_theme_decimal(j->source.data + n.start, n.end - n.start, &value) ||
        fabs(value) > 1e6)
        return false;
    *out = (float)value;
    return true;
}
static bool cui_theme_private_json_bool(const cui_theme_private_theme_json *j, size_t node,
                                        bool *out) {
    if (node >= j->count || (j->nodes[node].type != CUI_THEME_PRIVATE_J_TRUE &&
                             j->nodes[node].type != CUI_THEME_PRIVATE_J_FALSE))
        return false;
    *out = j->nodes[node].type == CUI_THEME_PRIVATE_J_TRUE;
    return true;
}
static size_t cui_theme_private_json_member(const cui_theme_private_theme_json *j, size_t object,
                                            const char *key) {
    if (object >= j->count || j->nodes[object].type != CUI_THEME_PRIVATE_J_OBJECT)
        return SIZE_MAX;
    for (size_t i = object + 1; i < j->nodes[object].next;) {
        char name[256];
        if (!cui_theme_private_json_string(j, i, name, sizeof name))
            return SIZE_MAX;
        size_t value = j->nodes[i].next;
        if (!strcmp(name, key))
            return value;
        i = j->nodes[value].next;
    }
    return SIZE_MAX;
}
#endif
