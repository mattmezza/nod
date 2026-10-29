/* SPDX-License-Identifier: MIT */
#ifndef CUI_EDITOR_EVENTS_H
#define CUI_EDITOR_EVENTS_H
#include "cui_editor.h"
#include "cui_multiline_events.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Aggregate committed CHANGE only, zero/one callback per outer submission.
 * Base notifies after completion/multicursor/textarea/search passes; Styled
 * notifies after the whole call, including painting and pointer completion popup.
 * No nested observer. Frame flag only when nonzero; UNKNOWN source/no index.
 * Multiline borrowed-text, primary-selection and lifetime preconditions apply. */
cui_multiline_event_result cui_base_editor_events(cui_context *, cui_id, cui_str name,
                                                  cui_editor_state *, cui_editor_options,
                                                  cui_textarea_metrics, cui_component_text_handler);
cui_multiline_event_result cui_editor_events(cui_context *, cui_id, const cui_theme *, cui_str name,
                                             cui_editor_state *, cui_editor_options,
                                             cui_component_text_handler);
#ifdef __cplusplus
}
#endif
#endif
