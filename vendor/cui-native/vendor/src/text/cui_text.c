/* SPDX-License-Identifier: MIT */
#include "cui/cui_text.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <float.h>
#include <hb-ft.h>
#include <hb.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Per-face storage makes font identity implicit. Raster flags and render mode
 * are fixed below; only glyph and the exact 26.6 character size can vary.
 * At most 128 entries retain 16 KiB each (2 MiB plus fixed metadata per face).
 * Oversized bitmaps use the FreeType slot directly instead of growing storage. */
#define GLYPH_CACHE_SLOTS 128u
#define GLYPH_CACHE_BITMAP_MAX 16384u
typedef struct {
    FT_UInt glyph;
    FT_F26Dot6 size;
    FT_GlyphSlotRec slot;
    unsigned char *pixels;
    bool valid;
} glyph_cache_entry;
typedef struct cui_font_family {
    uint32_t id;
    struct cui_font *variants[4][2][2];
    bool owns_variant[4][2][2];
    struct cui_font_family *next;
} cui_font_family;
struct cui_font {
    glyph_cache_entry *glyph_cache;
    FT_Library library;
    FT_Face face;
    hb_font_t *shaper;
    cui_font_family default_family;
    cui_font_family *families;
    size_t references;
    struct cui_font **fallbacks;
    size_t fallback_count;
    bool owns_fallbacks;
};
static FT_GlyphSlot raster_glyph(cui_font *font, FT_UInt glyph, float size) {
    FT_F26Dot6 raster_size = (FT_F26Dot6)lroundf(size * 64);
    size_t index = ((size_t)glyph * 33u + (size_t)raster_size) % GLYPH_CACHE_SLOTS;
    glyph_cache_entry *entry = font->glyph_cache ? &font->glyph_cache[index] : NULL;
    if (entry && entry->valid && entry->glyph == glyph && entry->size == raster_size)
        return &entry->slot;
    if (FT_Load_Glyph(font->face, glyph, FT_LOAD_DEFAULT) ||
        FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL))
        return NULL;
    FT_GlyphSlot slot = font->face->glyph;
    /* FreeType's negative-pitch layout is left untouched on the uncached path. */
    if (slot->bitmap.pitch < 0 ||
        (slot->bitmap.rows &&
         (size_t)slot->bitmap.pitch > GLYPH_CACHE_BITMAP_MAX / slot->bitmap.rows))
        return slot;
    size_t bytes = (size_t)slot->bitmap.pitch * slot->bitmap.rows;
    unsigned char *pixels = bytes ? malloc(bytes) : NULL;
    if (bytes && !pixels)
        return slot;
    if (!font->glyph_cache)
        font->glyph_cache = calloc(GLYPH_CACHE_SLOTS, sizeof *font->glyph_cache);
    if (!font->glyph_cache) {
        free(pixels);
        return slot;
    }
    if (bytes)
        memcpy(pixels, slot->bitmap.buffer, bytes);
    entry = &font->glyph_cache[index];
    free(entry->pixels);
    entry->glyph = glyph;
    entry->size = raster_size;
    entry->slot = (FT_GlyphSlotRec){0};
    entry->slot.bitmap = slot->bitmap;
    entry->slot.bitmap_left = slot->bitmap_left;
    entry->slot.bitmap_top = slot->bitmap_top;
    entry->pixels = pixels;
    entry->slot.bitmap.buffer = pixels;
    entry->valid = true;
    return &entry->slot;
}
static bool valid_style(cui_text_style style) {
    return style.weight >= CUI_TEXT_WEIGHT_NORMAL && style.weight <= CUI_TEXT_WEIGHT_BOLD &&
           isfinite(style.letter_spacing);
}
static cui_font_family *family_find(cui_font *font, uint32_t id, bool create) {
    if (!id)
        return &font->default_family;
    for (cui_font_family *f = font->families; f; f = f->next)
        if (f->id == id)
            return f;
    if (!create)
        return NULL;
    cui_font_family *f = calloc(1, sizeof(*f));
    if (!f)
        return NULL;
    f->id = id;
    f->next = font->families;
    font->families = f;
    return f;
}
static cui_font *selected(cui_font *font, cui_text_style style) {
    if (!font || !valid_style(style))
        return NULL;
    cui_font_family *family = family_find(font, style.family, false);
    if (family) {
        cui_font *variant = family->variants[style.weight][style.italic][style.monospace];
        if (variant)
            return variant;
        if (style.family && family->variants[0][0][0])
            return family->variants[0][0][0];
    }
    cui_font *variant = font->default_family.variants[style.weight][style.italic][style.monospace];
    return variant ? variant : font;
}
cui_font *cui_font_open(const char *path) {
    cui_font *f = calloc(1, sizeof(*f));
    if (!f)
        return NULL;
    f->references = 1;
    if (!path || FT_Init_FreeType(&f->library) || FT_New_Face(f->library, path, 0, &f->face)) {
        cui_font_close(f);
        return NULL;
    }
    f->shaper = hb_ft_font_create_referenced(f->face);
    if (!f->shaper) {
        cui_font_close(f);
        return NULL;
    }
    return f;
}
cui_font *cui_font_open_memory(const void *data, size_t size) {
    if (!data || !size || size > LONG_MAX)
        return NULL;
    cui_font *f = calloc(1, sizeof(*f));
    if (!f)
        return NULL;
    f->references = 1;
    if (FT_Init_FreeType(&f->library) ||
        FT_New_Memory_Face(f->library, data, (FT_Long)size, 0, &f->face)) {
        cui_font_close(f);
        return NULL;
    }
    f->shaper = hb_ft_font_create_referenced(f->face);
    if (!f->shaper) {
        cui_font_close(f);
        return NULL;
    }
    return f;
}
void cui_font_close(cui_font *f) {
    if (!f || --f->references)
        return;
    if (f->owns_fallbacks)
        for (size_t i = 0; i < f->fallback_count; ++i)
            cui_font_close(f->fallbacks[i]);
    free(f->fallbacks);
    cui_font_family *family = &f->default_family;
    while (family) {
        for (size_t weight = 0; weight < 4; weight++)
            for (size_t italic = 0; italic < 2; italic++)
                for (size_t mono = 0; mono < 2; mono++)
                    if (family->owns_variant[weight][italic][mono])
                        cui_font_close(family->variants[weight][italic][mono]);
        cui_font_family *next = family == &f->default_family ? f->families : family->next;
        if (family != &f->default_family)
            free(family);
        family = next;
    }
    if (f->glyph_cache) {
        for (size_t i = 0; i < GLYPH_CACHE_SLOTS; ++i)
            free(f->glyph_cache[i].pixels);
        free(f->glyph_cache);
    }
    if (f->shaper)
        hb_font_destroy(f->shaper);
    if (f->face)
        FT_Done_Face(f->face);
    if (f->library)
        FT_Done_FreeType(f->library);
    free(f);
}
bool cui_font_set_style(cui_font *font, cui_text_style style, cui_font *variant) {
    if (!font || !valid_style(style) ||
        (!style.family && style.weight == CUI_TEXT_WEIGHT_NORMAL && !style.italic &&
         !style.monospace) ||
        font == variant)
        return false;
    cui_font_family *family = family_find(font, style.family, variant != NULL);
    if (!family) {
        return variant == NULL;
    }
    cui_font **slot = &family->variants[style.weight][style.italic][style.monospace];
    bool *owned = &family->owns_variant[style.weight][style.italic][style.monospace];
    if (*slot == variant)
        return true;
    if (*owned)
        cui_font_close(*slot);
    *slot = variant;
    *owned = false;
    return true;
}
bool cui_font_register_style(cui_font *font, cui_text_style style, const char *path) {
    if (!font || !valid_style(style) ||
        (!style.family && style.weight == CUI_TEXT_WEIGHT_NORMAL && !style.italic &&
         !style.monospace) ||
        !path)
        return false;
    cui_font *variant = cui_font_open(path);
    if (!variant)
        return false;
    cui_font_family *family = family_find(font, style.family, true);
    if (!family) {
        cui_font_close(variant);
        return false;
    }
    cui_font **slot = &family->variants[style.weight][style.italic][style.monospace];
    bool *owned = &family->owns_variant[style.weight][style.italic][style.monospace];
    if (*owned)
        cui_font_close(*slot);
    *slot = variant;
    *owned = true;
    return true;
}
static void replace_fallbacks(cui_font *font, cui_font **entries, size_t count, bool owned) {
    cui_font **old = font->fallbacks;
    size_t old_count = font->fallback_count;
    bool old_owned = font->owns_fallbacks;
    font->fallbacks = entries;
    font->fallback_count = count;
    font->owns_fallbacks = owned;
    if (old_owned)
        for (size_t i = 0; i < old_count; ++i)
            cui_font_close(old[i]);
    free(old);
}
bool cui_font_set_fallbacks(cui_font *font, cui_font *const *entries, size_t count) {
    if (!font || count > CUI_FONT_FALLBACK_MAX || (count && !entries))
        return false;
    for (size_t i = 0; i < count; ++i) {
        if (!entries[i] || entries[i] == font)
            return false;
        for (size_t j = 0; j < i; ++j)
            if (entries[j] == entries[i])
                return false;
        /* Owned entries cannot be reborrowed from this chain: replacement would
         * otherwise destroy the very handles being installed. */
        if (font->owns_fallbacks)
            for (size_t j = 0; j < font->fallback_count; ++j)
                if (entries[i] == font->fallbacks[j])
                    return false;
    }
    cui_font **copy = NULL;
    if (count) {
        copy = malloc(count * sizeof *copy);
        if (!copy)
            return false;
        for (size_t i = 0; i < count; ++i)
            copy[i] = entries[i];
    }
    replace_fallbacks(font, copy, count, false);
    return true;
}
bool cui_font_register_fallbacks(cui_font *font, const char *const *paths, size_t count) {
    if (!font || count > CUI_FONT_FALLBACK_MAX || (count && !paths))
        return false;
    cui_font **copy = count ? calloc(count, sizeof *copy) : NULL;
    if (count && !copy)
        return false;
    for (size_t i = 0; i < count; ++i)
        if (!paths[i] || !(copy[i] = cui_font_open(paths[i]))) {
            for (size_t j = 0; j < i; ++j)
                cui_font_close(copy[j]);
            free(copy);
            return false;
        }
    replace_fallbacks(font, copy, count, true);
    return true;
}
#ifdef CUI_TEXT_BIDI
typedef struct text_lines {
    cui_text_layout *layout;
    struct text_lines *next;
    float y;
} text_lines;
static void text_lines_destroy(text_lines *lines);
static bool text_lines_create(cui_font *font, cui_str text, float size, cui_text_style style,
                              text_lines **out, cui_size *bounds);
#endif
#ifndef CUI_TEXT_BIDI
static hb_buffer_t *shape(cui_font *f, cui_str s, float size) {
    if (!f || !isfinite(size) || size <= 0 || size > 4096 || s.length > INT_MAX ||
        (!s.data && s.length))
        return NULL;
    if (FT_Set_Char_Size(f->face, 0, (FT_F26Dot6)lroundf(size * 64), 72, 72))
        return NULL;
    hb_ft_font_changed(f->shaper);
    hb_buffer_t *b = hb_buffer_create();
    hb_buffer_add_utf8(b, s.data, (int)s.length, 0, (int)s.length);
    hb_buffer_guess_segment_properties(b);
    hb_shape(f->shaper, b, NULL, 0);
    return b;
}
#endif
bool cui_font_get_metrics(void *font, float size, cui_text_style style, cui_font_metrics *out) {
    cui_font *f = selected(font, style);
    if (!f || !out || !isfinite(size) || size <= 0 || size > 4096 ||
        FT_Set_Char_Size(f->face, 0, (FT_F26Dot6)lroundf(size * 64), 72, 72))
        return false;
    cui_font_metrics result = {(float)f->face->size->metrics.ascender / 64,
                               -(float)f->face->size->metrics.descender / 64,
                               (float)f->face->size->metrics.height / 64};
    if (result.ascent < 0 || result.descent < 0 || result.line_height <= 0 ||
        result.ascent + result.descent <= 0)
        return false;
    *out = result;
    return true;
}
uint32_t cui_font_text_capabilities(void) {
#ifdef CUI_TEXT_BIDI
    return CUI_TEXT_CAP_LETTER_SPACING;
#else
    return 0;
#endif
}
cui_size cui_font_measure_styled(void *font, cui_str s, float size, cui_text_style style) {
    cui_size result = {0, 0};
    if (!valid_style(style) || (style.letter_spacing != 0 && !cui_font_text_capabilities()))
        return result;
#ifdef CUI_TEXT_BIDI
    text_lines *lines = NULL;
    if (!text_lines_create(font, s, size, style, &lines, &result))
        return (cui_size){0, 0};
    text_lines_destroy(lines);
    return result;
#else
    cui_font *f = selected(font, style);
    hb_buffer_t *b = shape(f, s, size);
    if (!b)
        return result;
    unsigned n;
    hb_glyph_position_t *p = hb_buffer_get_glyph_positions(b, &n);
    for (unsigned i = 0; i < n; i++)
        result.width += (float)p[i].x_advance / 64;
    result.width = fabsf(result.width);
    result.height = (float)f->face->size->metrics.height / 64;
    hb_buffer_destroy(b);
    return result;
#endif
}
cui_size cui_font_measure(void *font, cui_str s, float size) {
    return cui_font_measure_styled(font, s, size, (cui_text_style){0});
}
static bool font_draw_styled_scaled(cui_font *font, cui_str s, float size, cui_text_style style,
                                    float x, float y, float scale, cui_glyph_fn emit, void *user) {
    if (!valid_style(style) || (style.letter_spacing != 0 && !cui_font_text_capabilities()) ||
        !isfinite(x) || !isfinite(y) || fabsf(x) > 1e6f || fabsf(y) > 1e6f || !isfinite(scale) ||
        scale <= 0 || scale > 16)
        return false;
#ifdef CUI_TEXT_BIDI
    text_lines *lines = NULL;
    cui_size bounds;
    if (!emit || !text_lines_create(font, s, size, style, &lines, &bounds))
        return false;
    bool result = true;
    for (text_lines *line = lines; line; line = line->next)
        if (!cui_text_layout_draw_scaled(line->layout, x, y + line->y, scale, emit, user))
            result = false;
    text_lines_destroy(lines);
    return result;
#else
    cui_font *f = selected(font, style);
    hb_buffer_t *b = shape(f, s, size);
    if (!b || !emit) {
        if (b)
            hb_buffer_destroy(b);
        return false;
    }
    unsigned n;
    hb_glyph_position_t *p = hb_buffer_get_glyph_positions(b, &n);
    hb_glyph_info_t *g = hb_buffer_get_glyph_infos(b, NULL);
    float baseline = y + (float)f->face->size->metrics.ascender / 64;
    if (FT_Set_Char_Size(f->face, 0, (FT_F26Dot6)lroundf(size * scale * 64), 72, 72)) {
        hb_buffer_destroy(b);
        return false;
    }
    bool ok = true;
    for (unsigned i = 0; i < n; i++) {
        if (fabsf(x) > 1e7f || fabsf(baseline) > 1e7f) {
            ok = false;
            break;
        }
        FT_GlyphSlot slot = raster_glyph(f, g[i].codepoint, size * scale);
        if (!slot) {
            ok = false;
            continue;
        }
        emit(user, (int)lroundf((x + (float)p[i].x_offset / 64) * scale) + slot->bitmap_left,
             (int)lroundf((baseline - (float)p[i].y_offset / 64) * scale) - slot->bitmap_top,
             (int)slot->bitmap.width, (int)slot->bitmap.rows, slot->bitmap.pitch,
             slot->bitmap.buffer);
        x += (float)p[i].x_advance / 64;
        baseline -= (float)p[i].y_advance / 64;
    }
    hb_buffer_destroy(b);
    return ok;
#endif
}
bool cui_font_draw_styled(cui_font *font, cui_str text, float size, cui_text_style style, float x,
                          float y, cui_glyph_fn emit, void *user) {
    return font_draw_styled_scaled(font, text, size, style, x, y, 1, emit, user);
}
bool cui_font_draw(cui_font *f, cui_str s, float size, float x, float y, cui_glyph_fn emit,
                   void *user) {
    return cui_font_draw_styled(f, s, size, (cui_text_style){0}, x, y, emit, user);
}

struct cui_text_layout {
    cui_text_layout_view view;
    cui_text_glyph *glyphs;
    cui_text_run *runs;
    cui_font **faces;
    cui_text_cluster *clusters;
    float size;
};
void cui_text_layout_destroy(cui_text_layout *layout) {
    if (!layout)
        return;
    free(layout->glyphs);
    free(layout->runs);
    free(layout->clusters);
    for (size_t i = 0; i < layout->view.face_count; ++i)
        cui_font_close(layout->faces[i]);
    free(layout->faces);
    free(layout);
}
bool cui_text_layout_get(const cui_text_layout *layout, cui_text_layout_view *out) {
    if (!layout || !out)
        return false;
    *out = layout->view;
    return true;
}

#ifdef CUI_TEXT_BIDI
#include <string.h>
typedef struct {
    size_t start, end;
    hb_script_t script;
    uint8_t level;
    hb_buffer_t *buffer;
    size_t face_index;
} layout_segment;
static int cluster_order(const void *a, const void *b) {
    const cui_text_cluster *x = a, *y = b;
    if (x->left != y->left)
        return x->left < y->left ? -1 : 1;
    if (x->right != y->right)
        return x->right < y->right ? -1 : 1;
    if (x->run_index != y->run_index)
        return x->run_index < y->run_index ? -1 : 1;
    if (x->byte_start != y->byte_start)
        return x->byte_start < y->byte_start ? -1 : 1;
    return 0;
}
static bool weak_script(hb_script_t script) {
    return script == HB_SCRIPT_COMMON || script == HB_SCRIPT_INHERITED ||
           script == HB_SCRIPT_UNKNOWN;
}

cui_text_layout_status cui_font_layout_line(cui_font *font, cui_str text, float size,
                                            cui_text_style style, cui_bidi_direction direction,
                                            size_t start, size_t length, const char *language,
                                            cui_text_layout **out) {
    cui_font *face = selected(font, style);
    cui_bidi_paragraph *paragraph = NULL;
    cui_bidi_result analysis;
    cui_text_layout *layout = NULL;
    uint32_t *scalars = NULL, *mirrors = NULL;
    size_t *bytes = NULL, *mapping = NULL;
    uint8_t *levels = NULL;
    hb_script_t *scripts = NULL;
    cui_bidi_run *bidi_runs = NULL;
    layout_segment *segments = NULL;
    size_t segment_count = 0, glyph_count = 0;
    bool uses_fallback = false;
    cui_text_layout_status status = CUI_TEXT_LAYOUT_MEMORY;
    if (!out || !face || !isfinite(size) || size <= 0 || size > 4096 || text.length > INT_MAX ||
        (text.length && !text.data) || (unsigned)direction > CUI_BIDI_RTL)
        return CUI_TEXT_LAYOUT_INVALID;
    /* Every scalar/subrun is bounded by UTF-8 length; checked typed allocations. */
    size_t capacity = text.length ? text.length : 1;
    if (capacity > SIZE_MAX / sizeof(layout_segment) || capacity == SIZE_MAX)
        return CUI_TEXT_LAYOUT_INVALID;
    scalars = malloc(capacity * sizeof *scalars);
    if (!scalars)
        goto done;
    cui_bidi_status bidi =
        cui_bidi_paragraph_create(text.data, text.length, direction, scalars, capacity, &paragraph);
    if (bidi != CUI_BIDI_OK) {
        status = bidi == CUI_BIDI_MEMORY ? CUI_TEXT_LAYOUT_MEMORY : CUI_TEXT_LAYOUT_INVALID;
        goto done;
    }
    levels = malloc(capacity * sizeof *levels);
    bytes = malloc((capacity + 1) * sizeof *bytes);
    mapping = malloc(capacity * sizeof *mapping);
    mirrors = malloc(capacity * sizeof *mirrors);
    scripts = malloc(capacity * sizeof *scripts);
    bidi_runs = malloc(capacity * sizeof *bidi_runs);
    segments = calloc(capacity, sizeof *segments);
    if (!levels || !bytes || !mapping || !mirrors || !scripts || !bidi_runs || !segments)
        goto done;
    if (cui_bidi_paragraph_levels(paragraph, levels, capacity, &analysis) != CUI_BIDI_OK)
        goto done;
    if (start > analysis.scalar_count) {
        status = CUI_TEXT_LAYOUT_INVALID;
        goto done;
    }
    if (length == SIZE_MAX)
        length = analysis.scalar_count - start;
    if (length > analysis.scalar_count - start) {
        status = CUI_TEXT_LAYOUT_INVALID;
        goto done;
    }
    size_t scalar = 0;
    for (size_t b = 0; b < text.length; ++b)
        if (((unsigned char)text.data[b] & 0xc0) != 0x80)
            bytes[scalar++] = b;
    bytes[scalar] = text.length;
    layout = calloc(1, sizeof *layout);
    if (!layout)
        goto done;
    layout->size = size;
    layout->faces = calloc(font->fallback_count + 1, sizeof *layout->faces);
    if (!layout->faces)
        goto done;
    layout->faces[0] = face;
    ++face->references;
    layout->view.face_count = 1;
    for (size_t i = 0; i < font->fallback_count; ++i) {
        cui_font *fallback = selected(font->fallbacks[i], style);
        bool duplicate = false;
        for (size_t j = 0; j < layout->view.face_count; ++j)
            if (layout->faces[j] == fallback)
                duplicate = true;
        if (!duplicate) {
            layout->faces[layout->view.face_count++] = fallback;
            ++fallback->references;
        }
    }
    layout->view.faces = layout->faces;
    if (FT_Set_Char_Size(face->face, 0, (FT_F26Dot6)lroundf(size * 64), 72, 72)) {
        status = CUI_TEXT_LAYOUT_INVALID;
        goto done;
    }
    hb_ft_font_changed(face->shaper);
    layout->view.height = (float)face->face->size->metrics.height / 64;
    layout->view.ascent = (float)face->face->size->metrics.ascender / 64;
    layout->view.paragraph_level = analysis.paragraph_level;
    layout->view.byte_start = bytes[start];
    layout->view.byte_end = bytes[start + length];
    float descent = -(float)face->face->size->metrics.descender / 64;
    if (!length) {
        *out = layout;
        layout = NULL;
        status = CUI_TEXT_LAYOUT_OK;
        goto done;
    }
    bidi = cui_bidi_paragraph_line(paragraph, start, length, levels, mapping, mirrors, bidi_runs,
                                   capacity, &analysis);
    if (bidi != CUI_BIDI_OK) {
        status = bidi == CUI_BIDI_MEMORY ? CUI_TEXT_LAYOUT_MEMORY : CUI_TEXT_LAYOUT_INVALID;
        goto done;
    }
    for (size_t r = 0; r < analysis.run_count; ++r) {
        size_t first = bidi_runs[r].logical_start, end = first + bidi_runs[r].length;
        hb_script_t previous = HB_SCRIPT_COMMON;
        for (size_t i = first; i < end; ++i) {
            scripts[i] = hb_unicode_script(hb_unicode_funcs_get_default(), scalars[i]);
            if (!weak_script(scripts[i]))
                previous = scripts[i];
            else if (!weak_script(previous))
                scripts[i] = previous;
        }
        previous = HB_SCRIPT_COMMON;
        for (size_t i = end; i > first;) {
            --i;
            if (!weak_script(scripts[i]))
                previous = scripts[i];
            else if (!weak_script(previous))
                scripts[i] = previous;
        }
        size_t before = segment_count;
        for (size_t i = first; i < end;) {
            size_t limit = i + 1;
            while (limit < end && scripts[limit] == scripts[i])
                ++limit;
            segments[segment_count++] =
                (layout_segment){i, limit, scripts[i], bidi_runs[r].level, NULL, 0};
            i = limit;
        }
        /* Directional runs arrive in visual order. RTL script subruns also need
         * visual order; HarfBuzz alone orders glyph clusters inside each subrun. */
        if (bidi_runs[r].level & 1)
            for (size_t a = before, b = segment_count - 1; a < b; ++a, --b) {
                layout_segment temp = segments[a];
                segments[a] = segments[b];
                segments[b] = temp;
            }
    }
    for (size_t i = 0; i < segment_count; ++i) {
        layout_segment *seg = &segments[i];
        bool covered = false;
        /* Actual shaping is the coverage test. Nominal cmap checks alone can
         * reject valid composed marks/variation sequences, or accept a face
         * whose shaping substitutions still produce missing glyphs. */
        for (size_t f = 0; f < layout->view.face_count; ++f) {
            cui_font *candidate = layout->faces[f];
            if (FT_Set_Char_Size(candidate->face, 0, (FT_F26Dot6)lroundf(size * 64), 72, 72)) {
                status = CUI_TEXT_LAYOUT_INVALID;
                goto done;
            }
            hb_ft_font_changed(candidate->shaper);
            hb_buffer_t *attempt = hb_buffer_create();
            seg->buffer = attempt;
            hb_buffer_set_direction(attempt,
                                    (seg->level & 1) ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
            hb_buffer_set_script(attempt, seg->script);
            hb_buffer_set_language(attempt,
                                   hb_language_from_string(language ? language : "und", -1));
            hb_buffer_set_cluster_level(attempt, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
            hb_buffer_flags_t flags = HB_BUFFER_FLAG_REMOVE_DEFAULT_IGNORABLES;
            if (seg->start == start)
                flags |= HB_BUFFER_FLAG_BOT;
            if (seg->end == start + length)
                flags |= HB_BUFFER_FLAG_EOT;
            hb_buffer_set_flags(attempt, flags);
            hb_buffer_add_utf8(attempt, text.data + bytes[start],
                               (int)(bytes[start + length] - bytes[start]),
                               (unsigned)(bytes[seg->start] - bytes[start]),
                               (int)(bytes[seg->end] - bytes[seg->start]));
            hb_shape(candidate->shaper, attempt, NULL, 0);
            if (!hb_buffer_allocation_successful(attempt))
                goto done;
            unsigned count;
            hb_glyph_info_t *shaped = hb_buffer_get_glyph_infos(attempt, &count);
            bool complete = true;
            for (unsigned j = 0; j < count; ++j)
                if (!shaped[j].codepoint) {
                    complete = false;
                    break;
                }
            if (complete) {
                seg->face_index = f;
                covered = true;
                break;
            }
            hb_buffer_destroy(attempt);
            seg->buffer = NULL;
        }
        if (!covered) {
            status = CUI_TEXT_LAYOUT_MISSING_GLYPH;
            goto done;
        }
        cui_font *run_face = layout->faces[seg->face_index];
        if (seg->face_index)
            uses_fallback = true;
        float run_ascent = (float)run_face->face->size->metrics.ascender / 64;
        float run_descent = -(float)run_face->face->size->metrics.descender / 64;
        float run_height = (float)run_face->face->size->metrics.height / 64;
        if (run_ascent > layout->view.ascent)
            layout->view.ascent = run_ascent;
        if (run_descent > descent)
            descent = run_descent;
        if (run_height > layout->view.height)
            layout->view.height = run_height;
        hb_buffer_t *buffer = seg->buffer;
        size_t added = hb_buffer_get_length(buffer);
        if (added > SIZE_MAX - glyph_count)
            goto done;
        glyph_count += added;
    }
    if (glyph_count > SIZE_MAX / sizeof *layout->glyphs)
        goto done;
    layout->glyphs = calloc(glyph_count ? glyph_count : 1, sizeof *layout->glyphs);
    layout->runs = calloc(segment_count, sizeof *layout->runs);
    if (!layout->glyphs || !layout->runs)
        goto done;
    if (segment_count > SIZE_MAX - glyph_count ||
        glyph_count + segment_count > SIZE_MAX / sizeof *layout->clusters)
        goto done;
    layout->clusters = calloc(glyph_count + segment_count, sizeof *layout->clusters);
    if (!layout->clusters)
        goto done;
    size_t glyph = 0;
    float pen = 0;
    for (size_t i = 0; i < segment_count; ++i) {
        layout_segment *seg = &segments[i];
        unsigned count;
        hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(seg->buffer, &count);
        hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(seg->buffer, NULL);
        float advance = 0;
        for (unsigned j = 0; j < count; ++j)
            advance += (float)positions[j].x_advance / 64;
        float x = pen + (advance < 0 ? -advance : 0), y = 0;
        layout->runs[i] = (cui_text_run){bytes[seg->start],
                                         bytes[seg->end] - bytes[seg->start],
                                         glyph,
                                         count,
                                         (uint32_t)hb_script_to_iso15924_tag(seg->script),
                                         seg->level,
                                         seg->face_index};
        if (!count)
            layout->clusters[layout->view.cluster_count++] =
                (cui_text_cluster){bytes[seg->start], bytes[seg->end], i, glyph, 0, pen, pen};
        unsigned group_start = 0;
        float group_pen = x;
        for (unsigned j = 0; j < count; ++j) {
            layout->glyphs[glyph++] = (cui_text_glyph){infos[j].codepoint,
                                                       infos[j].cluster + bytes[start],
                                                       x + (float)positions[j].x_offset / 64,
                                                       y - (float)positions[j].y_offset / 64,
                                                       (float)positions[j].x_advance / 64,
                                                       -(float)positions[j].y_advance / 64};
            x += (float)positions[j].x_advance / 64;
            y -= (float)positions[j].y_advance / 64;
            if (j + 1 == count || infos[j + 1].cluster != infos[j].cluster) {
                bool rtl = (seg->level & 1) != 0;
                size_t begin = infos[group_start].cluster + bytes[start];
                size_t end =
                    rtl ? (group_start ? infos[group_start - 1].cluster + bytes[start]
                                       : bytes[seg->end])
                        : (j + 1 < count ? infos[j + 1].cluster + bytes[start] : bytes[seg->end]);
                if ((!rtl && !group_start) || (rtl && j + 1 == count))
                    begin = bytes[seg->start];
                if (begin >= end || begin < layout->view.byte_start ||
                    end > layout->view.byte_end || !isfinite(group_pen) || !isfinite(x)) {
                    status = CUI_TEXT_LAYOUT_INVALID;
                    goto done;
                }
                layout->clusters[layout->view.cluster_count++] =
                    (cui_text_cluster){begin,
                                       end,
                                       i,
                                       glyph - (j - group_start + 1),
                                       j - group_start + 1,
                                       fminf(group_pen, x),
                                       fmaxf(group_pen, x)};
                group_start = j + 1;
                group_pen = x;
            }
        }
        pen += fabsf(advance);
    }
    if (uses_fallback && layout->view.ascent + descent > layout->view.height)
        layout->view.height = layout->view.ascent + descent;
    layout->view.glyphs = layout->glyphs;
    layout->view.runs = layout->runs;
    layout->view.glyph_count = glyph_count;
    layout->view.run_count = segment_count;
    layout->view.width = pen;
    qsort(layout->clusters, layout->view.cluster_count, sizeof *layout->clusters, cluster_order);
    /* Shape first, then move intact visual clusters. Required/optional
     * ligatures and marks retain their original glyphs and relative offsets.
     * Zero-advance clusters do not earn gaps, nor does the last eligible one. */
    if (style.letter_spacing != 0) {
        size_t remaining = 0;
        for (size_t i = 0; i < layout->view.cluster_count; ++i)
            remaining += layout->clusters[i].right > layout->clusters[i].left;
        double shift = 0;
        for (size_t i = 0; i < layout->view.cluster_count; ++i) {
            cui_text_cluster *c = &layout->clusters[i];
            bool eligible = c->right > c->left;
            float gap = eligible && --remaining ? style.letter_spacing : 0;
            if ((double)c->right - c->left + gap < 0 || !isfinite((double)c->right + shift + gap)) {
                status = CUI_TEXT_LAYOUT_INVALID;
                goto done;
            }
            for (size_t j = 0; j < c->glyph_count; ++j)
                layout->glyphs[c->glyph_start + j].x += (float)shift;
            c->left += (float)shift;
            c->right += (float)(shift + gap);
            shift += gap;
        }
        double width = (double)pen + shift;
        if (!isfinite(width) || width < 0 || width > FLT_MAX) {
            status = CUI_TEXT_LAYOUT_INVALID;
            goto done;
        }
        layout->view.width = (float)width;
    }
    layout->view.clusters = layout->clusters;
    *out = layout;
    layout = NULL;
    status = CUI_TEXT_LAYOUT_OK;
done:
    for (size_t i = 0; i < segment_count; ++i)
        if (segments[i].buffer)
            hb_buffer_destroy(segments[i].buffer);
    cui_text_layout_destroy(layout);
    cui_bidi_paragraph_destroy(paragraph);
    free(scalars);
    free(mirrors);
    free(bytes);
    free(mapping);
    free(levels);
    free(scripts);
    free(bidi_runs);
    free(segments);
    return status;
}
#else
cui_text_layout_status cui_font_layout_line(cui_font *font, cui_str paragraph, float size,
                                            cui_text_style style, cui_bidi_direction direction,
                                            size_t start, size_t length, const char *language,
                                            cui_text_layout **out) {
    (void)font;
    (void)paragraph;
    (void)size;
    (void)style;
    (void)direction;
    (void)start;
    (void)length;
    (void)language;
    (void)out;
    return CUI_TEXT_LAYOUT_UNAVAILABLE;
}
#endif

static bool text_layout_draw(cui_text_layout *layout, float x, float y, cui_glyph_fn emit,
                             cui_color_glyph_fn colored, cui_color color,
                             const cui_text_color_span *spans, size_t count, float scale,
                             void *user) {
    if (!isfinite(scale) || scale <= 0 || scale > 16)
        return false;
    if (!layout || (!emit && !colored) || !isfinite(x) || !isfinite(y) || fabsf(x) > 1e6f ||
        fabsf(y) > 1e6f)
        return false;
    bool ok = true;
    size_t run = 0;
    cui_font *face = NULL;
    for (size_t i = 0; i < layout->view.glyph_count; ++i) {
        while (run + 1 < layout->view.run_count &&
               i >= layout->runs[run].glyph_start + layout->runs[run].glyph_count)
            ++run;
        cui_font *selected_face = layout->faces[layout->runs[run].face_index];
        if (selected_face != face) {
            face = selected_face;
            if (FT_Set_Char_Size(face->face, 0, (FT_F26Dot6)lroundf(layout->size * scale * 64), 72,
                                 72))
                return false;
        }
        const cui_text_glyph *g = &layout->glyphs[i];
        float px = x + g->x, py = y + layout->view.ascent + g->y;
        if (!isfinite(px) || !isfinite(py) || fabsf(px) > 1e7f || fabsf(py) > 1e7f)
            return false;
        FT_GlyphSlot slot = raster_glyph(face, g->glyph_id, layout->size * scale);
        if (!slot) {
            ok = false;
            continue;
        }
        if (colored) {
            cui_color ink = color;
            for (size_t span = 0; span < count; span++)
                if (g->cluster_byte >= spans[span].byte_start &&
                    g->cluster_byte < spans[span].byte_end) {
                    ink = spans[span].color;
                    break;
                }
            colored(user, (int)lroundf(px * scale) + slot->bitmap_left,
                    (int)lroundf(py * scale) - slot->bitmap_top, (int)slot->bitmap.width,
                    (int)slot->bitmap.rows, slot->bitmap.pitch, slot->bitmap.buffer, ink);
        } else
            emit(user, (int)lroundf(px * scale) + slot->bitmap_left,
                 (int)lroundf(py * scale) - slot->bitmap_top, (int)slot->bitmap.width,
                 (int)slot->bitmap.rows, slot->bitmap.pitch, slot->bitmap.buffer);
    }
    return ok;
}

bool cui_text_layout_draw(cui_text_layout *layout, float x, float y, cui_glyph_fn emit,
                          void *user) {
    if (!layout || !emit || !isfinite(x) || !isfinite(y))
        return false;
    return text_layout_draw(layout, x, y, emit, NULL, (cui_color){0}, NULL, 0, 1, user);
}
bool cui_text_layout_draw_scaled(cui_text_layout *layout, float x, float y, float scale,
                                 cui_glyph_fn emit, void *user) {
    return text_layout_draw(layout, x, y, emit, NULL, (cui_color){0}, NULL, 0, scale, user);
}

#ifdef CUI_TEXT_BIDI
/* Strict scanning is necessary before recognizing separators: malformed bytes
 * must not become boundaries that hide invalid text from paragraph validation. */
static bool text_next(cui_str text, size_t *index, uint32_t *cp) {
    size_t i = *index, more = 0;
    uint32_t value = (unsigned char)text.data[i++], minimum = 0;
    if (value < 0x80) {
    } else if (value >= 0xc2 && value <= 0xdf) {
        value &= 31;
        more = 1;
        minimum = 0x80;
    } else if (value >= 0xe0 && value <= 0xef) {
        value &= 15;
        more = 2;
        minimum = 0x800;
    } else if (value >= 0xf0 && value <= 0xf4) {
        value &= 7;
        more = 3;
        minimum = 0x10000;
    } else
        return false;
    if (more > text.length - i)
        return false;
    while (more--) {
        uint32_t b = (unsigned char)text.data[i++];
        if ((b & 0xc0) != 0x80)
            return false;
        value = (value << 6) | (b & 63);
    }
    if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
        return false;
    *index = i;
    *cp = value;
    return true;
}
static bool paragraph_separator(uint32_t cp) {
    return cp == 0xa || cp == 0xd || cp == 0x1c || cp == 0x1d || cp == 0x1e || cp == 0x85 ||
           cp == 0x2029;
}
static void text_lines_destroy(text_lines *lines) {
    while (lines) {
        text_lines *next = lines->next;
        cui_text_layout_destroy(lines->layout);
        free(lines);
        lines = next;
    }
}
static bool text_line_append(cui_font *font, cui_str paragraph, float size, cui_text_style style,
                             size_t start, size_t length, text_lines ***tail, cui_size *bounds) {
    text_lines *node = calloc(1, sizeof *node);
    cui_text_layout_view view;
    if (!node)
        return false;
    if (cui_font_layout_line(font, paragraph, size, style, CUI_BIDI_AUTO_LTR, start, length, NULL,
                             &node->layout) != CUI_TEXT_LAYOUT_OK) {
        free(node);
        return false;
    }
    if (!cui_text_layout_get(node->layout, &view)) {
        cui_text_layout_destroy(node->layout);
        free(node);
        return false;
    }
    node->y = bounds->height;
    bounds->height += view.height;
    if (view.width > bounds->width)
        bounds->width = view.width;
    **tail = node;
    *tail = &node->next;
    return true;
}
static bool text_paragraph_append(cui_font *font, cui_str paragraph, float size,
                                  cui_text_style style, text_lines ***tail, cui_size *bounds) {
    size_t index = 0, scalar = 0, start = 0;
    while (index < paragraph.length) {
        uint32_t cp;
        if (!text_next(paragraph, &index, &cp))
            return false;
        if (cp == 0x2028) {
            if (!text_line_append(font, paragraph, size, style, start, scalar - start, tail,
                                  bounds))
                return false;
            start = scalar + 1;
        }
        ++scalar;
    }
    return text_line_append(font, paragraph, size, style, start, scalar - start, tail, bounds);
}
static bool text_lines_create(cui_font *font, cui_str text, float size, cui_text_style style,
                              text_lines **out, cui_size *bounds) {
    text_lines *head = NULL, **tail = &head;
    cui_size measure = {0, 0};
    size_t index = 0, start = 0;
    if (text.length > INT_MAX || (text.length && !text.data))
        return false;
    while (index < text.length) {
        size_t before = index;
        uint32_t cp;
        if (!text_next(text, &index, &cp))
            goto failed;
        if (paragraph_separator(cp)) {
            cui_str paragraph = {text.data + start, before - start};
            if (!text_paragraph_append(font, paragraph, size, style, &tail, &measure))
                goto failed;
            if (cp == 0xd && index < text.length && text.data[index] == '\n')
                ++index;
            start = index;
        }
    }
    {
        cui_str paragraph = {text.data ? text.data + start : NULL, text.length - start};
        if (!text_paragraph_append(font, paragraph, size, style, &tail, &measure))
            goto failed;
    }
    *out = head;
    *bounds = measure;
    return true;
failed:
    text_lines_destroy(head);
    return false;
}
#endif

#ifdef CUI_TEXT_BIDI
static bool cluster_boundary(const cui_text_layout *layout, size_t byte) {
    if (byte < layout->view.byte_start || byte > layout->view.byte_end)
        return false;
    if (!layout->view.cluster_count)
        return byte == layout->view.byte_start;
    for (size_t i = 0; i < layout->view.cluster_count; ++i)
        if (layout->clusters[i].byte_start == byte || layout->clusters[i].byte_end == byte)
            return true;
    return false;
}
static cui_text_caret cluster_caret(const cui_text_layout *layout, size_t index,
                                    cui_text_affinity affinity) {
    const cui_text_cluster *cluster = &layout->clusters[index];
    bool rtl = (layout->runs[cluster->run_index].level & 1) != 0;
    size_t byte = affinity == CUI_TEXT_DOWNSTREAM ? cluster->byte_start : cluster->byte_end;
    float x = (rtl == (affinity == CUI_TEXT_DOWNSTREAM)) ? cluster->right : cluster->left;
    return (cui_text_caret){byte, cluster->run_index, affinity, x, 0, layout->view.height};
}
cui_text_layout_status cui_text_layout_carets(const cui_text_layout *layout, size_t byte,
                                              cui_text_caret *out, size_t capacity, size_t *count) {
    size_t needed = 0;
    if (!layout || !count || (!out && capacity) || !cluster_boundary(layout, byte))
        return CUI_TEXT_LAYOUT_INVALID;
    if (!layout->view.cluster_count)
        needed = 2;
    else
        for (size_t i = 0; i < layout->view.cluster_count; ++i) {
            needed += layout->clusters[i].byte_start == byte;
            needed += layout->clusters[i].byte_end == byte;
        }
    if (!out && !capacity) {
        *count = needed;
        return CUI_TEXT_LAYOUT_OK;
    }
    if (capacity < needed)
        return CUI_TEXT_LAYOUT_CAPACITY;
    size_t k = 0;
    if (!layout->view.cluster_count) {
        out[k++] = (cui_text_caret){byte, SIZE_MAX, CUI_TEXT_UPSTREAM, 0, 0, layout->view.height};
        out[k++] = (cui_text_caret){byte, SIZE_MAX, CUI_TEXT_DOWNSTREAM, 0, 0, layout->view.height};
    } else
        for (size_t i = 0; i < layout->view.cluster_count; ++i) {
            if (layout->clusters[i].byte_start == byte)
                out[k++] = cluster_caret(layout, i, CUI_TEXT_DOWNSTREAM);
            if (layout->clusters[i].byte_end == byte)
                out[k++] = cluster_caret(layout, i, CUI_TEXT_UPSTREAM);
        }
    /* Same original boundary has at most two logical neighbors, except that
     * empty lines explicitly represent both affinities. Stable bounded sort. */
    for (size_t i = 1; i < k; ++i) {
        cui_text_caret value = out[i];
        size_t j = i;
        while (j && (out[j - 1].x > value.x ||
                     (out[j - 1].x == value.x && out[j - 1].affinity > value.affinity))) {
            out[j] = out[j - 1];
            --j;
        }
        out[j] = value;
    }
    *count = k;
    return CUI_TEXT_LAYOUT_OK;
}
cui_text_layout_status cui_text_layout_hit_test(const cui_text_layout *layout, float x, float y,
                                                cui_text_affinity preferred, cui_text_caret *out) {
    if (!layout || !out || !isfinite(x) || !isfinite(y) ||
        (unsigned)preferred > CUI_TEXT_DOWNSTREAM)
        return CUI_TEXT_LAYOUT_INVALID;
    cui_text_caret best = {layout->view.byte_start, SIZE_MAX, preferred, 0, 0, layout->view.height};
    double distance = HUGE_VAL;
    bool found = false;
    for (size_t i = 0; i < layout->view.cluster_count; ++i)
        for (unsigned a = 0; a < 2; ++a) {
            cui_text_caret candidate = cluster_caret(layout, i, (cui_text_affinity)a);
            double delta = fabs((double)x - candidate.x);
            if (!found || delta < distance ||
                (delta == distance && candidate.affinity == preferred &&
                 best.affinity != preferred)) {
                best = candidate;
                distance = delta;
                found = true;
            }
        }
    *out = best;
    return CUI_TEXT_LAYOUT_OK;
}
cui_text_layout_status cui_text_layout_selection(const cui_text_layout *layout, size_t start,
                                                 size_t end, cui_text_selection_rect *out,
                                                 size_t capacity, size_t *count) {
    if (!layout || !count || (!out && capacity))
        return CUI_TEXT_LAYOUT_INVALID;
    if (start > end) {
        size_t temporary = start;
        start = end;
        end = temporary;
    }
    if (!cluster_boundary(layout, start) || !cluster_boundary(layout, end))
        return CUI_TEXT_LAYOUT_INVALID;
    size_t needed = 0;
    if (start != end)
        for (size_t i = 0; i < layout->view.cluster_count; ++i)
            if (layout->clusters[i].byte_start >= start && layout->clusters[i].byte_end <= end)
                ++needed;
    if (!out && !capacity) {
        *count = needed;
        return CUI_TEXT_LAYOUT_OK;
    }
    if (capacity < needed)
        return CUI_TEXT_LAYOUT_CAPACITY;
    size_t k = 0;
    if (start != end)
        for (size_t i = 0; i < layout->view.cluster_count; ++i) {
            const cui_text_cluster *cluster = &layout->clusters[i];
            if (cluster->byte_start >= start && cluster->byte_end <= end)
                out[k++] = (cui_text_selection_rect){
                    {cluster->left, 0, cluster->right - cluster->left, layout->view.height},
                    cluster->byte_start,
                    cluster->byte_end,
                    cluster->run_index};
        }
    *count = k;
    return CUI_TEXT_LAYOUT_OK;
}
#else
cui_text_layout_status cui_text_layout_carets(const cui_text_layout *layout, size_t byte,
                                              cui_text_caret *out, size_t capacity, size_t *count) {
    (void)layout;
    (void)byte;
    (void)out;
    (void)capacity;
    (void)count;
    return CUI_TEXT_LAYOUT_UNAVAILABLE;
}
cui_text_layout_status cui_text_layout_hit_test(const cui_text_layout *layout, float x, float y,
                                                cui_text_affinity preferred, cui_text_caret *out) {
    (void)layout;
    (void)x;
    (void)y;
    (void)preferred;
    (void)out;
    return CUI_TEXT_LAYOUT_UNAVAILABLE;
}
cui_text_layout_status cui_text_layout_selection(const cui_text_layout *layout, size_t start,
                                                 size_t end, cui_text_selection_rect *out,
                                                 size_t capacity, size_t *count) {
    (void)layout;
    (void)start;
    (void)end;
    (void)out;
    (void)capacity;
    (void)count;
    return CUI_TEXT_LAYOUT_UNAVAILABLE;
}
#endif

static cui_text_layout_status text_geometry_carets(const void *session, size_t byte,
                                                   cui_text_caret *out, size_t capacity,
                                                   size_t *count) {
    return cui_text_layout_carets(session, byte, out, capacity, count);
}
static cui_text_layout_status text_geometry_hit(const void *session, float x, float y,
                                                cui_text_affinity affinity, cui_text_caret *out) {
    return cui_text_layout_hit_test(session, x, y, affinity, out);
}
static cui_text_layout_status text_geometry_selection(const void *session, size_t start, size_t end,
                                                      cui_text_selection_rect *out, size_t capacity,
                                                      size_t *count) {
    return cui_text_layout_selection(session, start, end, out, capacity, count);
}
static void text_geometry_release(void *session) {
    cui_text_layout_destroy(session);
}
cui_text_layout_status cui_font_text_geometry(void *font, cui_str text, float size,
                                              cui_text_style style, cui_text_geometry *out) {
    static const cui_text_geometry_ops ops = {text_geometry_carets, text_geometry_hit,
                                              text_geometry_selection, text_geometry_release};
    if (!out)
        return CUI_TEXT_LAYOUT_INVALID;
#ifdef CUI_TEXT_BIDI
    if (!text.data && text.length)
        return CUI_TEXT_LAYOUT_INVALID;
    size_t offset = 0;
    while (offset < text.length) {
        uint32_t cp;
        if (!text_next(text, &offset, &cp) || paragraph_separator(cp) || cp == 0x2028)
            return CUI_TEXT_LAYOUT_INVALID;
    }
#endif
    cui_text_layout *layout = NULL;
    cui_text_layout_view view;
    cui_text_layout_status status = cui_font_layout_line(font, text, size, style, CUI_BIDI_AUTO_LTR,
                                                         0, SIZE_MAX, NULL, &layout);
    if (status != CUI_TEXT_LAYOUT_OK)
        return status;
    if (!cui_text_layout_get(layout, &view)) {
        cui_text_layout_destroy(layout);
        return CUI_TEXT_LAYOUT_INVALID;
    }
    *out = (cui_text_geometry){layout, &ops, view.width, view.height, text.length};
    return CUI_TEXT_LAYOUT_OK;
}

cui_text_layout_status cui_font_text_geometry_line(void *font, cui_text_line_source source,
                                                   float size, cui_text_style style,
                                                   cui_text_geometry *out) {
    static const cui_text_geometry_ops ops = {text_geometry_carets, text_geometry_hit,
                                              text_geometry_selection, text_geometry_release};
    if (!out || !cui_text_line_source_valid(source))
        return CUI_TEXT_LAYOUT_INVALID;
#ifdef CUI_TEXT_BIDI
    size_t offset = 0, scalar = 0, start = 0, end = 0;
    while (offset < source.paragraph.length) {
        if (offset == source.byte_start)
            start = scalar;
        if (offset == source.byte_end)
            end = scalar;
        uint32_t cp;
        size_t scalar_byte = offset;
        if (!text_next(source.paragraph, &offset, &cp) || paragraph_separator(cp) ||
            (cp == 0x2028 && scalar_byte >= source.byte_start && scalar_byte < source.byte_end))
            return CUI_TEXT_LAYOUT_INVALID;
        scalar++;
    }
    if (source.byte_start == offset)
        start = scalar;
    if (source.byte_end == offset)
        end = scalar;
    cui_text_layout *layout = NULL;
    cui_text_layout_status status = cui_font_layout_line(
        font, source.paragraph, size, style, CUI_BIDI_AUTO_LTR, start, end - start, NULL, &layout);
    if (status != CUI_TEXT_LAYOUT_OK)
        return status;
    cui_text_layout_view view;
    if (!cui_text_layout_get(layout, &view)) {
        cui_text_layout_destroy(layout);
        return CUI_TEXT_LAYOUT_INVALID;
    }
    *out = (cui_text_geometry){layout, &ops, view.width, view.height, source.paragraph.length};
    return CUI_TEXT_LAYOUT_OK;
#else
    (void)font;
    (void)size;
    (void)style;
    (void)ops;
    return CUI_TEXT_LAYOUT_UNAVAILABLE;
#endif
}
bool cui_font_draw_context_line_scaled(cui_font *font, cui_str text, cui_text_line_source source,
                                       float size, cui_text_style style, float x, float y,
                                       float scale, cui_glyph_fn emit, void *user) {
    if (!isfinite(scale) || scale <= 0 || scale > 16)
        return false;
    if (!source.paragraph.data) {
        if (source.paragraph.length || source.byte_start || source.byte_end)
            return false;
        return font_draw_styled_scaled(font, text, size, style, x, y, scale, emit, user);
    }
#ifdef CUI_TEXT_BIDI
    if (!emit || !cui_text_line_source_valid(source))
        return false;
    size_t scan = source.byte_start, start = scan;
    float top = y;
    while (scan < source.byte_end) {
        size_t before = scan;
        uint32_t cp;
        if (!text_next(source.paragraph, &scan, &cp))
            return false;
        if (cp != 0x2028)
            continue;
        cui_text_geometry geometry = {0};
        cui_text_line_source line = {source.paragraph, start, before};
        if (cui_font_text_geometry_line(font, line, size, style, &geometry) != CUI_TEXT_LAYOUT_OK)
            return false;
        bool result = cui_text_layout_draw_scaled(geometry.session, x, top, scale, emit, user);
        top += geometry.height;
        cui_release_text_geometry(&geometry);
        if (!result)
            return false;
        start = scan;
    }
    cui_text_geometry geometry = {0};
    source.byte_start = start;
    if (cui_font_text_geometry_line(font, source, size, style, &geometry) != CUI_TEXT_LAYOUT_OK)
        return false;
    bool result = cui_text_layout_draw_scaled(geometry.session, x, top, scale, emit, user);
    cui_release_text_geometry(&geometry);
    return result;
#else
    return font_draw_styled_scaled(font, text, size, style, x, y, scale, emit, user);
#endif
}

bool cui_font_draw_context_line(cui_font *font, cui_str text, cui_text_line_source source,
                                float size, cui_text_style style, float x, float y,
                                cui_glyph_fn emit, void *user) {
    return cui_font_draw_context_line_scaled(font, text, source, size, style, x, y, 1, emit, user);
}

bool cui_font_draw_context_line_colored_scaled(cui_font *font, cui_text_line_source source,
                                               float size, cui_text_style style, float x, float y,
                                               cui_color color, const cui_text_color_span *spans,
                                               size_t count, float scale, cui_color_glyph_fn emit,
                                               void *user) {
    if (!emit || (!spans && count) || count > CUI_TEXT_COLOR_SPAN_MAX || !isfinite(x) ||
        !isfinite(y))
        return false;
    size_t previous = source.byte_start;
    for (size_t i = 0; i < count; i++) {
        if (spans[i].byte_start < previous || spans[i].byte_end > source.byte_end ||
            !cui_text_line_source_valid(
                (cui_text_line_source){source.paragraph, spans[i].byte_start, spans[i].byte_end}))
            return false;
        previous = spans[i].byte_end;
    }
    cui_text_geometry geometry = {0};
    if (cui_font_text_geometry_line(font, source, size, style, &geometry) != CUI_TEXT_LAYOUT_OK)
        return false;
    bool result =
        text_layout_draw(geometry.session, x, y, NULL, emit, color, spans, count, scale, user);
    cui_release_text_geometry(&geometry);
    return result;
}

bool cui_font_draw_context_line_colored(cui_font *font, cui_text_line_source source, float size,
                                        cui_text_style style, float x, float y, cui_color color,
                                        const cui_text_color_span *spans, size_t count,
                                        cui_color_glyph_fn emit, void *user) {
    return cui_font_draw_context_line_colored_scaled(font, source, size, style, x, y, color, spans,
                                                     count, 1, emit, user);
}
