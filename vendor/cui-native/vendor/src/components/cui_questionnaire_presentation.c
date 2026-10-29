/* SPDX-License-Identifier: MIT */
#include "cui/cui_questionnaire_presentation.h"
#include "cui_questionnaire_presentation_private.h"
#include <math.h>
static bool cui_questionnaire_presentation_valid(cui_questionnaire_text_options p) {
    cui_questionnaire_text_refinement r = p.text;
    return (unsigned)p.policy <= CUI_QUESTIONNAIRE_TEXT_TOKENS && !(p.fields & ~UINT32_C(1)) &&
           !(r.fields & ~UINT32_C(15)) &&
           (!(p.fields & CUI_QUESTIONNAIRE_TEXT_VISIBLE) || cui_edit_valid_utf8(p.visible)) &&
           (!(r.fields & CUI_QUESTIONNAIRE_TEXT_SIZE) ||
            (isfinite(r.font_size) && r.font_size > 0 && r.font_size <= 512)) &&
           (!(r.fields & CUI_QUESTIONNAIRE_TEXT_LINE_HEIGHT) ||
            (isfinite(r.line_height) && r.line_height >= 0 && r.line_height <= 4096)) &&
           (!(r.fields & CUI_QUESTIONNAIRE_TEXT_STYLE) ||
            ((unsigned)r.style.weight <= CUI_TEXT_WEIGHT_BOLD && isfinite(r.style.letter_spacing)));
}
/* role0 title,1 secondary,2 error. Explicit tokens do not change legacy paint. */
static cui_questionnaire_text_view
cui_questionnaire_presentation_resolve(const cui_theme *t, cui_questionnaire_options o,
                                       cui_questionnaire_text_metrics m, unsigned role,
                                       cui_str fallback, cui_questionnaire_text_options p) {
    cui_questionnaire_text_view v = {
        .text = (p.fields & CUI_QUESTIONNAIRE_TEXT_VISIBLE) ? p.visible : fallback,
        .font_size = role == 0     ? m.font + 2
                     : m.font > 14 ? m.font - 2
                                   : 12,
        .line_height = role == 0 ? m.line + 4 : m.line,
        .color = role == 0   ? t->text
                 : role == 2 ? t->danger
                             : t->muted,
        .style = {.weight = role == 0 ? CUI_TEXT_WEIGHT_MEDIUM : CUI_TEXT_WEIGHT_NORMAL}};
    if (role == 2) {
        v.font_size = o.design_size ? m.font : o.size == CUI_QUESTIONNAIRE_LARGE ? 16 : 12;
        v.line_height = 0;
    }
    if (p.policy == CUI_QUESTIONNAIRE_TEXT_TOKENS) {
        if (o.design_size) {
            v.font_size = role == 0 ? m.font * 1.125f : m.font * .875f;
            v.line_height = role == 0 ? m.line * 1.125f : m.line * .875f;
        } else {
            unsigned n = o.size == CUI_QUESTIONNAIRE_XSMALL  ? 0
                         : o.size == CUI_QUESTIONNAIRE_SMALL ? 1
                         : o.size == CUI_QUESTIONNAIRE_LARGE ? 3
                                                             : 2;
            const float title_font[] = {14, 16, 18, 20}, title_line[] = {20, 24, 28, 28};
            const float secondary_font[] = {12, 12, 14, 16}, secondary_line[] = {16, 16, 20, 24};
            v.font_size = role == 0 ? title_font[n] : secondary_font[n];
            v.line_height = role == 0 ? title_line[n] : secondary_line[n];
        }
    }
    cui_questionnaire_text_refinement r = p.text;
    if (r.fields & CUI_QUESTIONNAIRE_TEXT_SIZE)
        v.font_size = r.font_size;
    if (r.fields & CUI_QUESTIONNAIRE_TEXT_LINE_HEIGHT)
        v.line_height = r.line_height;
    if (r.fields & CUI_QUESTIONNAIRE_TEXT_COLOR)
        v.color = r.color;
    if (r.fields & CUI_QUESTIONNAIRE_TEXT_STYLE)
        v.style = r.style;
    return v;
}
static void cui_questionnaire_presentation_paint(cui_context *ui, cui_id id,
                                                 cui_questionnaire_text_view v,
                                                 cui_questionnaire_text_options p) {
    if (p.render_child) {
        cui_id child = id ^ UINT64_C(0x763edc4ca33e9a51);
        p.render_child(ui, child ? child : 1, &v, p.render_user);
    } else
        cui_text_ex(ui, v.text, v.font_size, v.color, v.line_height, v.style);
}
static bool cui_questionnaire_presentation_item_text(cui_context *ui, cui_id id, const cui_theme *t,
                                                     const cui_questionnaire_state *s, size_t item,
                                                     cui_questionnaire_options o,
                                                     cui_questionnaire_text_options p, bool title) {
    cui_questionnaire_text_metrics m;
    if (!ui || !id || !t || !s || !s->initialized || item >= s->count ||
        !cui_questionnaire_presentation_valid(p) || !cui_questionnaire_text_prepare(ui, s, &o, &m))
        return false;
    const cui_questionnaire_item *d = &s->items[item];
    cui_str text = title ? (d->label.length ? d->label : d->name) : d->description;
    bool close_gap = title && !d->description.length && p.policy == CUI_QUESTIONNAIRE_TEXT_TOKENS;
    if (!title && !text.length && !(p.fields & CUI_QUESTIONNAIRE_TEXT_VISIBLE) && !p.render_child)
        return true;
    cui_questionnaire_text_view v =
        cui_questionnaire_presentation_resolve(t, o, m, title ? 0 : 1, text, p);
    cui_begin_box(ui, id, (cui_box){.width = -1});
    cui_questionnaire_presentation_paint(ui, id, v, p);
    if (close_gap) {
        cui_begin_box(ui, 0, (cui_box){.height = m.gap});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return true;
}
bool cui_questionnaire_title_ex(cui_context *ui, cui_id id, const cui_theme *t,
                                const cui_questionnaire_state *s, size_t item,
                                cui_questionnaire_options o, cui_questionnaire_text_options p) {
    return cui_questionnaire_presentation_item_text(ui, id, t, s, item, o, p, true);
}
bool cui_questionnaire_description_ex(cui_context *ui, cui_id id, const cui_theme *t,
                                      const cui_questionnaire_state *s, size_t item,
                                      cui_questionnaire_options o,
                                      cui_questionnaire_text_options p) {
    return cui_questionnaire_presentation_item_text(ui, id, t, s, item, o, p, false);
}
bool cui_questionnaire_choice_description_ex(cui_context *ui, cui_id id, const cui_theme *t,
                                             cui_str text, cui_questionnaire_options o,
                                             cui_questionnaire_text_options p) {
    cui_questionnaire_text_metrics m;
    if (!ui || !id || !t || !cui_edit_valid_utf8(text) ||
        !cui_questionnaire_presentation_valid(p) ||
        !cui_questionnaire_text_prepare(ui, NULL, &o, &m))
        return false;
    cui_questionnaire_text_view v = cui_questionnaire_presentation_resolve(t, o, m, 1, text, p);
    cui_begin_box(ui, id, (cui_box){.width = -1});
    cui_questionnaire_presentation_paint(ui, id, v, p);
    cui_end_box(ui);
    return true;
}
bool cui_questionnaire_error_ex(cui_context *ui, cui_id id, const cui_theme *t,
                                const cui_questionnaire_state *s, size_t item,
                                cui_questionnaire_options o, cui_questionnaire_text_options p) {
    cui_questionnaire_text_metrics m;
    if (!ui || !id || !t || !s || !s->initialized || item >= s->count ||
        !cui_questionnaire_presentation_valid(p) ||
        !cui_questionnaire_text_prepare(ui, item == s->current ? s : NULL, &o, &m))
        return false;
    if (item != s->current)
        return true;
    cui_questionnaire_error e = cui_questionnaire_item_error(s, item);
    if (e.kind == CUI_QUESTIONNAIRE_VALID)
        return true;
    cui_str message =
        e.kind == CUI_QUESTIONNAIRE_MESSAGE
            ? e.message
            : cui_string(e.kind == CUI_QUESTIONNAIRE_REQUIRED ? "This question is required"
                                                              : "Answer or skip this question");
    cui_questionnaire_labels labels = o.labels ? *o.labels : (cui_questionnaire_labels){0};
    if (e.kind == CUI_QUESTIONNAIRE_REQUIRED && (labels.fields & CUI_QUESTIONNAIRE_LABEL_REQUIRED))
        message = labels.required;
    if (e.kind == CUI_QUESTIONNAIRE_UNFILLED && (labels.fields & CUI_QUESTIONNAIRE_LABEL_UNFILLED))
        message = labels.unfilled;
    cui_questionnaire_text_view v = cui_questionnaire_presentation_resolve(t, o, m, 2, message, p);
    cui_interact(ui, id, CUI_ROLE_ALERT, message, false, false, 0);
    cui_begin_box(ui, id, (cui_box){.width = -1});
    if (p.policy == CUI_QUESTIONNAIRE_TEXT_TOKENS) {
        cui_begin_box(ui, 0, (cui_box){.height = 8});
        cui_end_box(ui);
    }
    cui_questionnaire_presentation_paint(ui, id, v, p);
    cui_end_box(ui);
    return true;
}
