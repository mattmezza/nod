/* SPDX-License-Identifier: MIT */
#include "cui_color_entry_internal.h"
#include <string.h>

static int cui_color_entry_nibble(unsigned char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
cui_color_hex_status cui_color_entry_parse(cui_str text, cui_color *output) {
    if ((!text.data && text.length) || text.length > 9)
        return CUI_COLOR_HEX_INVALID;
    size_t start = text.length && text.data[0] == '#' ? 1 : 0;
    size_t n = text.length - start;
    if (n > 8)
        return CUI_COLOR_HEX_INVALID;
    for (size_t i = start; i < text.length; ++i)
        if (cui_color_entry_nibble((unsigned char)text.data[i]) < 0)
            return CUI_COLOR_HEX_INVALID;
    if (n != 3 && n != 4 && n != 6 && n != 8)
        return CUI_COLOR_HEX_INCOMPLETE;
    unsigned char channels[4] = {0, 0, 0, 255};
    size_t count = n == 3 || n == 6 ? 3 : 4;
    for (size_t i = 0; i < count; ++i) {
        int a = cui_color_entry_nibble((unsigned char)text.data[start + (n < 5 ? i : 2 * i)]);
        int b = n < 5 ? a : cui_color_entry_nibble((unsigned char)text.data[start + 2 * i + 1]);
        channels[i] = (unsigned char)(a * 16 + b);
    }
    if (output)
        *output = (cui_color){channels[0], channels[1], channels[2], channels[3]};
    return CUI_COLOR_HEX_COMPLETE;
}
bool cui_color_entry_private_admit(void *user, cui_str text) {
    (void)user;
    return text.length == 0 || (text.data && text.data[0] == '#' &&
                                cui_color_entry_parse(text, NULL) != CUI_COLOR_HEX_INVALID);
}
static void cui_color_entry_refresh(cui_color_entry_state *state) {
    cui_color color;
    state->status = cui_color_entry_parse(cui_edit_get_text(state->edit), &color);
    if (state->status == CUI_COLOR_HEX_COMPLETE)
        state->preview = (cui_color_entry_value){true, color};
}
bool cui_color_entry_init(cui_color_entry_state *state, cui_color_entry_value value) {
    if (!state || state->edit)
        return false;
    /* Allocation must bind the final address, not a copied local state. */
    char draft[10] = {0};
    if (value.present)
        (void)cui_color_format_hex(value.color, value.color.a != 255, draft, sizeof draft);
    char previous[10];
    memcpy(previous, state->draft, sizeof previous);
    memcpy(state->draft, draft, sizeof draft);
    cui_edit *edit = cui_edit_create((cui_edit_config){.buffer = state->draft,
                                                       .capacity = sizeof state->draft,
                                                       .history_bytes = 4096,
                                                       .max_history_steps = 32});
    if (!edit) {
        memcpy(state->draft, previous, sizeof previous);
        return false;
    }
    state->edit = edit;
    state->input = (cui_input_state){0};
    state->committed = state->preview = value.present ? value : (cui_color_entry_value){0};
    state->dirty = false;
    cui_color_entry_refresh(state);
    return true;
}
void cui_color_entry_destroy(cui_color_entry_state *state) {
    if (state) {
        cui_edit_destroy(state->edit);
        memset(state, 0, sizeof *state);
    }
}
cui_color_entry_outcome cui_color_entry_sync(cui_color_entry_state *state,
                                             cui_color_entry_value value, bool replace_draft) {
    if (!state || !state->edit)
        return CUI_COLOR_ENTRY_INVALID_ARGUMENT;
    if (state->dirty && !replace_draft)
        return CUI_COLOR_ENTRY_CONFLICT;
    char draft[10] = {0};
    if (value.present)
        (void)cui_color_format_hex(value.color, value.color.a != 255, draft, sizeof draft);
    char previous[10];
    memcpy(previous, state->draft, sizeof previous);
    memcpy(state->draft, draft, sizeof draft);
    cui_edit *replacement = cui_edit_create((cui_edit_config){.buffer = state->draft,
                                                              .capacity = sizeof state->draft,
                                                              .history_bytes = 4096,
                                                              .max_history_steps = 32});
    if (!replacement) {
        memcpy(state->draft, previous, sizeof previous);
        return CUI_COLOR_ENTRY_EDIT_ERROR;
    }
    cui_edit_destroy(state->edit);
    state->edit = replacement;
    state->input = (cui_input_state){0};
    state->committed = state->preview = value.present ? value : (cui_color_entry_value){0};
    state->dirty = false;
    cui_color_entry_refresh(state);
    return CUI_COLOR_ENTRY_OK;
}
cui_color_entry_outcome cui_color_entry_set_draft(cui_color_entry_state *state, cui_str draft) {
    if (!state || !state->edit || !cui_color_entry_private_admit(NULL, draft))
        return CUI_COLOR_ENTRY_INVALID_ARGUMENT;
    if (cui_edit_set_content(state->edit, (cui_edit_content){.text = draft}) != CUI_EDIT_OK)
        return CUI_COLOR_ENTRY_EDIT_ERROR;
    state->dirty = true;
    cui_color_entry_refresh(state);
    return CUI_COLOR_ENTRY_OK;
}
cui_color_entry_outcome cui_color_entry_commit(cui_color_entry_state *state) {
    if (!state || !state->edit)
        return CUI_COLOR_ENTRY_INVALID_ARGUMENT;
    if (state->status != CUI_COLOR_HEX_COMPLETE)
        return CUI_COLOR_ENTRY_INCOMPLETE;
    state->committed = state->preview;
    state->dirty = false;
    return CUI_COLOR_ENTRY_OK;
}
cui_color_entry_outcome cui_color_entry_restore(cui_color_entry_state *state) {
    if (!state || !state->edit)
        return CUI_COLOR_ENTRY_INVALID_ARGUMENT;
    return cui_color_entry_sync(state, state->committed, true);
}
void cui_color_entry_private_observe(void *user, const cui_component_text_event *event) {
    cui_color_entry_observation *observation = user;
    cui_color_entry_state *state = observation->state;
    if (event->event.kind == CUI_COMPONENT_EVENT_CHANGE) {
        state->dirty = true;
        cui_color_entry_refresh(state);
        observation->result->draft_changed = true;
    } else if (event->event.kind == CUI_COMPONENT_EVENT_SUBMIT) {
        if (cui_color_entry_commit(state) == CUI_COLOR_ENTRY_OK)
            ++observation->result->commit_count;
        else
            ++observation->result->rejected_commit_count;
    }
}
cui_color_entry_result cui_color_entry_private_begin(cui_context *ui, cui_id id,
                                                     cui_color_entry_state *state,
                                                     cui_color_entry_options options) {
    cui_color_entry_result result = {.input = {.error = CUI_EDIT_INVALID_ARGUMENT}};
    if (!ui || !id || !state || !state->edit)
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
void cui_color_entry_private_finish(cui_color_entry_state *state, cui_color_entry_result *result,
                                    cui_color_entry_handler handler) {
    result->status = state->status;
    result->committed = state->committed;
    result->preview = state->preview;
    if (result->commit_count && handler.on_commit)
        handler.on_commit(handler.user, result);
}
cui_color_entry_result cui_base_color_entry(cui_context *ui, cui_id id, cui_str name,
                                            cui_color_entry_state *state,
                                            cui_color_entry_options options,
                                            cui_color_entry_handler handler) {
    cui_color_entry_result result = cui_color_entry_private_begin(ui, id, state, options);
    if (result.input.error != CUI_EDIT_OK)
        return result;
    cui_color_entry_observation observation = {state, &result};
    cui_input_event_result input = cui_base_input_events(
        ui, id, name, state->edit, &state->input.behavior,
        (cui_base_input_options){.disabled = options.disabled,
                                 .readonly = options.readonly,
                                 .validate = cui_color_entry_private_admit},
        (cui_component_text_handler){.callback =
                                         result.submitted ? cui_color_entry_private_observe : NULL,
                                     .user = &observation});
    result.input = input.control;
    cui_color_entry_private_finish(state, &result, handler);
    return result;
}
