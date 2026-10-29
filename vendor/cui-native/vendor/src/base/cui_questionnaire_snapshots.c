/* SPDX-License-Identifier: MIT */
#include "cui/cui_questionnaire_snapshots.h"
#include "cui_questionnaire_snapshot_math.h"
#include <string.h>
static cui_questionnaire_snapshot_result
cui_questionnaire_snapshot_measure(const cui_questionnaire_state *s, bool single, size_t item,
                                   cui_questionnaire_snapshot_requirements *required) {
    if (!required)
        return CUI_QUESTIONNAIRE_SNAPSHOT_INVALID;
    *required = (cui_questionnaire_snapshot_requirements){0};
    if (!s || !s->initialized)
        return CUI_QUESTIONNAIRE_SNAPSHOT_INVALID;
    if (single && item >= s->count)
        return CUI_QUESTIONNAIRE_SNAPSHOT_UNKNOWN_ITEM;
    cui_questionnaire_snapshot_requirements n = {0};
    size_t begin = single ? item : 0, end = single ? item + 1 : s->count;
    for (size_t i = begin; i < end; ++i) {
        if (!single && s->storage.items[i].disabled)
            continue;
        if (!cui_questionnaire_snapshot_add(&n.items, 1) ||
            !cui_questionnaire_snapshot_add(&n.text_bytes, s->items[i].name.length) ||
            !cui_questionnaire_snapshot_add(&n.text_bytes, 1))
            return CUI_QUESTIONNAIRE_SNAPSHOT_OVERFLOW;
        for (size_t c = 0; c < s->items[i].choice_count; ++c) {
            if (!cui_questionnaire_choice_selected(s, i, c))
                continue;
            if (!cui_questionnaire_snapshot_add(&n.choices, 1) ||
                !cui_questionnaire_snapshot_add(&n.text_bytes,
                                                s->items[i].choices[c].value.length) ||
                !cui_questionnaire_snapshot_add(&n.text_bytes, 1))
                return CUI_QUESTIONNAIRE_SNAPSHOT_OVERFLOW;
        }
        cui_str freeform = cui_questionnaire_freeform(s, i);
        if (freeform.length && (!cui_questionnaire_snapshot_add(&n.text_bytes, freeform.length) ||
                                !cui_questionnaire_snapshot_add(&n.text_bytes, 1)))
            return CUI_QUESTIONNAIRE_SNAPSHOT_OVERFLOW;
    }
    if (!cui_questionnaire_snapshot_extent(n.items, sizeof(cui_questionnaire_item_snapshot)) ||
        !cui_questionnaire_snapshot_extent(n.choices, sizeof(cui_str)))
        return CUI_QUESTIONNAIRE_SNAPSHOT_OVERFLOW;
    *required = n;
    return CUI_QUESTIONNAIRE_SNAPSHOT_OK;
}
cui_questionnaire_snapshot_result
cui_questionnaire_answer_measure(const cui_questionnaire_state *s, size_t item,
                                 cui_questionnaire_snapshot_requirements *required) {
    return cui_questionnaire_snapshot_measure(s, true, item, required);
}
cui_questionnaire_snapshot_result
cui_questionnaire_submission_measure(const cui_questionnaire_state *s,
                                     cui_questionnaire_snapshot_requirements *required) {
    return cui_questionnaire_snapshot_measure(s, false, 0, required);
}
static cui_str cui_questionnaire_snapshot_string(cui_str source, char *text, size_t *offset) {
    char *start = text + *offset;
    if (source.length)
        memcpy(start, source.data, source.length);
    start[source.length] = 0;
    *offset += source.length + 1; /* Fully checked by measure before any writes. */
    return (cui_str){start, source.length};
}
static cui_questionnaire_snapshot_result
cui_questionnaire_snapshot_copy(const cui_questionnaire_state *s, bool single, size_t item,
                                cui_questionnaire_snapshot_workspace w,
                                cui_questionnaire_snapshot_requirements *required) {
    cui_questionnaire_snapshot_result result =
        cui_questionnaire_snapshot_measure(s, single, item, required);
    if (result != CUI_QUESTIONNAIRE_SNAPSHOT_OK)
        return result;
    if ((w.item_capacity && !w.items) || (w.choice_capacity && !w.choices) ||
        (w.text_capacity && !w.text)) {
        *required = (cui_questionnaire_snapshot_requirements){0};
        return CUI_QUESTIONNAIRE_SNAPSHOT_INVALID;
    }
    if (w.item_capacity < required->items || w.choice_capacity < required->choices ||
        w.text_capacity < required->text_bytes)
        return CUI_QUESTIONNAIRE_SNAPSHOT_CAPACITY;
    size_t record = 0, choice = 0, text = 0;
    size_t begin = single ? item : 0, end = single ? item + 1 : s->count;
    for (size_t i = begin; i < end; ++i) {
        if (!single && s->storage.items[i].disabled)
            continue;
        cui_questionnaire_item_snapshot entry = {.item = i,
                                                 .status = cui_questionnaire_item_status(s, i),
                                                 .disabled = s->storage.items[i].disabled};
        entry.name = cui_questionnaire_snapshot_string(s->items[i].name, w.text, &text);
        size_t first = choice;
        for (size_t c = 0; c < s->items[i].choice_count; ++c) {
            if (cui_questionnaire_choice_selected(s, i, c))
                w.choices[choice++] =
                    cui_questionnaire_snapshot_string(s->items[i].choices[c].value, w.text, &text);
        }
        entry.answer.choice_count = choice - first;
        entry.answer.choices = choice != first ? w.choices + first : NULL;
        cui_str freeform = cui_questionnaire_freeform(s, i);
        if (freeform.length) {
            entry.answer.has_freeform = true;
            entry.answer.freeform = cui_questionnaire_snapshot_string(freeform, w.text, &text);
        }
        w.items[record++] = entry;
    }
    return CUI_QUESTIONNAIRE_SNAPSHOT_OK;
}
cui_questionnaire_snapshot_result cui_questionnaire_answer_copy(
    const cui_questionnaire_state *s, size_t item, cui_questionnaire_snapshot_workspace w,
    cui_questionnaire_snapshot_requirements *required, cui_questionnaire_item_snapshot *out) {
    if (out)
        *out = (cui_questionnaire_item_snapshot){0};
    if (!out) {
        if (required)
            *required = (cui_questionnaire_snapshot_requirements){0};
        return CUI_QUESTIONNAIRE_SNAPSHOT_INVALID;
    }
    cui_questionnaire_snapshot_result result =
        cui_questionnaire_snapshot_copy(s, true, item, w, required);
    if (result == CUI_QUESTIONNAIRE_SNAPSHOT_OK)
        *out = w.items[0];
    return result;
}
cui_questionnaire_snapshot_result cui_questionnaire_submission_copy(
    const cui_questionnaire_state *s, cui_questionnaire_snapshot_workspace w,
    cui_questionnaire_snapshot_requirements *required, cui_questionnaire_submission_snapshot *out) {
    if (out)
        *out = (cui_questionnaire_submission_snapshot){0};
    if (!out) {
        if (required)
            *required = (cui_questionnaire_snapshot_requirements){0};
        return CUI_QUESTIONNAIRE_SNAPSHOT_INVALID;
    }
    cui_questionnaire_snapshot_result result =
        cui_questionnaire_snapshot_copy(s, false, 0, w, required);
    if (result == CUI_QUESTIONNAIRE_SNAPSHOT_OK)
        *out = (cui_questionnaire_submission_snapshot){required->items ? w.items : NULL,
                                                       required->items};
    return result;
}
cui_questionnaire_snapshot_result
cui_questionnaire_query_copy(const cui_questionnaire_state *s,
                             cui_questionnaire_query_snapshot *out) {
    if (!out)
        return CUI_QUESTIONNAIRE_SNAPSHOT_INVALID;
    *out = (cui_questionnaire_query_snapshot){.current_item = SIZE_MAX, .position = SIZE_MAX};
    if (!s || !s->initialized)
        return CUI_QUESTIONNAIRE_SNAPSHOT_INVALID;
    out->complete = s->complete;
    out->total = cui_questionnaire_total(s);
    if (s->current >= s->count || s->storage.items[s->current].disabled)
        return CUI_QUESTIONNAIRE_SNAPSHOT_OK;
    out->current_item = s->current;
    out->position = cui_questionnaire_position(s);
    out->current = out->position + 1;
    out->previous_visible = out->position > 0;
    out->next_visible = out->current < out->total;
    out->skip_visible = !s->items[s->current].required;
    out->submit_visible = out->current == out->total;
    out->confirmable = cui_questionnaire_item_status(s, s->current) != CUI_QUESTIONNAIRE_UNANSWERED;
    return CUI_QUESTIONNAIRE_SNAPSHOT_OK;
}
