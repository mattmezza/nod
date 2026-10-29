/* SPDX-License-Identifier: MIT */
#include "cui_color_hsla_entry_internal.h"
#include <string.h>
bool cui_color_hsla_entry_private_admit(void *user, cui_str text) {
    (void)user;
    return !text.length || (text.data && text.data[0] == '#' &&
                            cui_color_hsla_parse_hex(text, NULL) != CUI_COLOR_HSLA_HEX_INVALID);
}
static void cui_color_hsla_entry_refresh(cui_color_hsla_entry_state *s) {
    cui_color_hsla value;
    s->status = cui_color_hsla_parse_hex(cui_edit_get_text(s->edit), &value);
    if (s->status == CUI_COLOR_HSLA_HEX_COMPLETE)
        s->preview = (cui_color_hsla_value){true, value};
}
/* No callback/allocation can observe a partially published model. On failure
 * restore the borrowed bytes before returning to the existing editor owner. */
static cui_color_hsla_entry_outcome cui_color_hsla_entry_replace(cui_color_hsla_entry_state *s,
                                                                 cui_color_hsla_value value) {
    if (value.present && !cui_color_hsla_valid(value.color))
        return CUI_COLOR_HSLA_ENTRY_INVALID_ARGUMENT;
    if (!value.present)
        value = (cui_color_hsla_value){0};
    char candidate[10] = {0}, previous[10];
    if (value.present &&
        cui_color_hsla_format_hex(value.color, candidate, sizeof candidate).status !=
            CUI_COLOR_HSLA_OK)
        return CUI_COLOR_HSLA_ENTRY_INVALID_ARGUMENT;
    memcpy(previous, s->draft, sizeof previous);
    memcpy(s->draft, candidate, sizeof candidate);
    cui_edit *edit = cui_edit_create((cui_edit_config){.buffer = s->draft,
                                                       .capacity = sizeof s->draft,
                                                       .history_bytes = 4096,
                                                       .max_history_steps = 32});
    if (!edit) {
        memcpy(s->draft, previous, sizeof previous);
        return CUI_COLOR_HSLA_ENTRY_ALLOCATION_FAILED;
    }
    cui_edit_destroy(s->edit);
    s->edit = edit;
    s->input = (cui_input_state){0};
    s->preview = value;
    s->status = value.present ? CUI_COLOR_HSLA_HEX_COMPLETE : CUI_COLOR_HSLA_HEX_INCOMPLETE;
    s->dirty = false;
    return CUI_COLOR_HSLA_ENTRY_OK;
}
cui_color_hsla_entry_outcome cui_color_hsla_entry_init(cui_color_hsla_entry_state *s,
                                                       cui_color_hsla_value value) {
    if (!s || s->edit)
        return CUI_COLOR_HSLA_ENTRY_INVALID_ARGUMENT;
    cui_color_hsla_entry_outcome result = cui_color_hsla_entry_replace(s, value);
    if (result == CUI_COLOR_HSLA_ENTRY_OK)
        s->committed = s->preview;
    return result;
}
void cui_color_hsla_entry_destroy(cui_color_hsla_entry_state *s) {
    if (s) {
        cui_edit_destroy(s->edit);
        memset(s, 0, sizeof *s);
    }
}
cui_color_hsla_entry_outcome cui_color_hsla_entry_sync(cui_color_hsla_entry_state *s,
                                                       cui_color_hsla_value value,
                                                       bool replace_draft) {
    if (!s || !s->edit || (value.present && !cui_color_hsla_valid(value.color)))
        return CUI_COLOR_HSLA_ENTRY_INVALID_ARGUMENT;
    if (s->dirty && !replace_draft)
        return CUI_COLOR_HSLA_ENTRY_CONFLICT;
    cui_color_hsla_entry_outcome result = cui_color_hsla_entry_replace(s, value);
    if (result == CUI_COLOR_HSLA_ENTRY_OK)
        s->committed = s->preview;
    return result;
}
cui_color_hsla_entry_outcome cui_color_hsla_entry_preview(cui_color_hsla_entry_state *s,
                                                          cui_color_hsla value,
                                                          bool replace_draft) {
    if (!s || !s->edit || !cui_color_hsla_valid(value))
        return CUI_COLOR_HSLA_ENTRY_INVALID_ARGUMENT;
    if (s->dirty && !replace_draft)
        return CUI_COLOR_HSLA_ENTRY_CONFLICT;
    return cui_color_hsla_entry_replace(s, (cui_color_hsla_value){true, value});
}
cui_color_hsla_entry_outcome cui_color_hsla_entry_restore(cui_color_hsla_entry_state *s,
                                                          bool replace_draft) {
    if (!s || !s->edit)
        return CUI_COLOR_HSLA_ENTRY_INVALID_ARGUMENT;
    if (s->dirty && !replace_draft)
        return CUI_COLOR_HSLA_ENTRY_CONFLICT;
    return cui_color_hsla_entry_replace(s, s->committed);
}
cui_color_hsla_entry_outcome cui_color_hsla_entry_set_draft(cui_color_hsla_entry_state *s,
                                                            cui_str text) {
    if (!s || !s->edit || !cui_color_hsla_entry_private_admit(NULL, text))
        return CUI_COLOR_HSLA_ENTRY_INVALID_ARGUMENT;
    if (cui_edit_set_content(s->edit, (cui_edit_content){.text = text}) != CUI_EDIT_OK)
        return CUI_COLOR_HSLA_ENTRY_EDIT_ERROR;
    s->dirty = true;
    cui_color_hsla_entry_refresh(s);
    return CUI_COLOR_HSLA_ENTRY_OK;
}
static bool cui_color_hsla_entry_kind_valid(cui_color_hsla_commit_kind kind) {
    return kind == CUI_COLOR_HSLA_COMMIT_SELECT || kind == CUI_COLOR_HSLA_COMMIT_UPDATE;
}
cui_color_hsla_commit_result cui_color_hsla_entry_commit_hex(cui_color_hsla_entry_state *s,
                                                             cui_color_hsla_commit_kind kind) {
    cui_color_hsla_commit_result result = {.outcome = CUI_COLOR_HSLA_ENTRY_INVALID_ARGUMENT};
    if (!s || !s->edit)
        return result;
    result.previous = result.value = s->committed;
    if (!cui_color_hsla_entry_kind_valid(kind))
        return result;
    cui_color_hsla color;
    cui_color_hsla_hex_status status = cui_color_hsla_parse_hex(cui_edit_get_text(s->edit), &color);
    if (status != CUI_COLOR_HSLA_HEX_COMPLETE) {
        result.outcome = status == CUI_COLOR_HSLA_HEX_INCOMPLETE
                             ? CUI_COLOR_HSLA_ENTRY_INCOMPLETE
                             : CUI_COLOR_HSLA_ENTRY_INVALID_ARGUMENT;
        return result;
    }
    s->committed = s->preview = (cui_color_hsla_value){true, color};
    s->dirty = false;
    s->status = status;
    result.outcome = CUI_COLOR_HSLA_ENTRY_OK;
    result.committed = true;
    result.close_requested = kind == CUI_COLOR_HSLA_COMMIT_SELECT;
    result.value = s->committed;
    return result;
}
cui_color_hsla_commit_result cui_color_hsla_entry_commit_value(cui_color_hsla_entry_state *s,
                                                               cui_color_hsla color,
                                                               cui_color_hsla_commit_kind kind,
                                                               bool replace_draft) {
    cui_color_hsla_commit_result result = {.outcome = CUI_COLOR_HSLA_ENTRY_INVALID_ARGUMENT};
    if (!s || !s->edit)
        return result;
    result.previous = result.value = s->committed;
    if (!cui_color_hsla_entry_kind_valid(kind) || !cui_color_hsla_valid(color))
        return result;
    result.outcome =
        cui_color_hsla_entry_sync(s, (cui_color_hsla_value){true, color}, replace_draft);
    if (result.outcome == CUI_COLOR_HSLA_ENTRY_OK) {
        result.committed = true;
        result.close_requested = kind == CUI_COLOR_HSLA_COMMIT_SELECT;
        result.value = s->committed;
    }
    return result;
}
void cui_color_hsla_entry_private_observe(void *user, const cui_component_text_event *event) {
    cui_color_hsla_entry_observation *observation = user;
    cui_color_hsla_entry_state *state = observation->state;
    if (event->event.kind == CUI_COMPONENT_EVENT_CHANGE) {
        state->dirty = true;
        cui_color_hsla_entry_refresh(state);
        observation->result->draft_changed = true;
    } else if (event->event.kind == CUI_COMPONENT_EVENT_SUBMIT) {
        if (cui_color_hsla_entry_commit_hex(state, CUI_COLOR_HSLA_COMMIT_SELECT).committed) {
            ++observation->result->commit_count;
            ++observation->result->close_request_count;
        } else
            ++observation->result->rejected_commit_count;
    }
}
cui_color_hsla_entry_result
cui_color_hsla_entry_private_begin(cui_context *ui, cui_id id, cui_color_hsla_entry_state *state,
                                   cui_color_hsla_entry_options options) {
    cui_color_hsla_entry_result result = {.input = {.error = CUI_EDIT_INVALID_ARGUMENT}};
    if (!ui || !id || !state || !state->edit || !cui_frame_is_submitting(ui))
        return result;
    result.previous = state->committed;
    result.submitted = !options.disabled && !options.readonly && cui_frame_sequence(ui) != 0;
    result.event = (cui_component_event_metadata){.kind = CUI_COMPONENT_EVENT_SUBMIT,
                                                  .target = id,
                                                  .source = CUI_COMPONENT_EVENT_SOURCE_UNKNOWN};
    if (result.submitted) {
        result.event.flags = CUI_COMPONENT_EVENT_HAS_FRAME;
        result.event.frame_sequence = cui_frame_sequence(ui);
    }
    result.input.error = CUI_EDIT_OK;
    return result;
}
void cui_color_hsla_entry_private_finish(cui_color_hsla_entry_state *state,
                                         cui_color_hsla_entry_result *result,
                                         cui_color_hsla_entry_handler handler) {
    result->status = state->status;
    result->committed = state->committed;
    result->preview = state->preview;
    if (result->input.error == CUI_EDIT_INVALID_ARGUMENT) {
        result->submitted = false;
        result->event = (cui_component_event_metadata){0};
    }
    if (result->commit_count && handler.on_commit)
        handler.on_commit(handler.user, result);
}
cui_color_hsla_entry_result cui_base_color_hsla_entry(cui_context *ui, cui_id id, cui_str name,
                                                      cui_color_hsla_entry_state *state,
                                                      cui_color_hsla_entry_options options,
                                                      cui_color_hsla_entry_handler handler) {
    if (!name.data && name.length)
        return (cui_color_hsla_entry_result){.input = {.error = CUI_EDIT_INVALID_ARGUMENT}};
    cui_color_hsla_entry_result result = cui_color_hsla_entry_private_begin(ui, id, state, options);
    if (result.input.error != CUI_EDIT_OK)
        return result;
    cui_color_hsla_entry_observation observation = {state, &result};
    cui_input_event_result input = cui_base_input_events(
        ui, id, name, state->edit, &state->input.behavior,
        (cui_base_input_options){.disabled = options.disabled,
                                 .readonly = options.readonly,
                                 .validate = cui_color_hsla_entry_private_admit},
        (cui_component_text_handler){
            .callback = result.submitted ? cui_color_hsla_entry_private_observe : NULL,
            .user = &observation});
    result.input = input.control;
    cui_color_hsla_entry_private_finish(state, &result, handler);
    return result;
}
