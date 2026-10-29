/* SPDX-License-Identifier: MIT */
#include "../base/cui_color_entry_internal.h"
#include <math.h>
cui_color_entry_result cui_color_entry(cui_context *ui, cui_id id, const cui_theme *theme,
                                       cui_str name, cui_color_entry_state *state,
                                       cui_color_entry_options options,
                                       cui_color_entry_handler handler) {
    if (!theme || !isfinite(options.width))
        return (cui_color_entry_result){.input = {.error = CUI_EDIT_INVALID_ARGUMENT}};
    cui_color_entry_result result = cui_color_entry_private_begin(ui, id, state, options);
    if (result.input.error == CUI_EDIT_INVALID_ARGUMENT)
        return result;
    cui_color_entry_observation observation = {state, &result};
    cui_input_event_result input =
        cui_input_events(ui, id, theme, name, state->edit, &state->input,
                         (cui_input_options){.disabled = options.disabled,
                                             .readonly = options.readonly,
                                             .width = options.width,
                                             .placeholder = cui_string("#RRGGBB"),
                                             .validate = cui_color_entry_private_admit},
                         (cui_component_text_handler){
                             .callback = result.submitted ? cui_color_entry_private_observe : NULL,
                             .user = &observation});
    result.input = input.control;
    cui_color_entry_private_finish(state, &result, handler);
    return result;
}
