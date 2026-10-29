/* SPDX-License-Identifier: MIT */
#ifndef CUI_QUESTIONNAIRE_KEYBOARD_H
#define CUI_QUESTIONNAIRE_KEYBOARD_H
#include "cui_questionnaire.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Additive policies: no changes to cui_questionnaire_options or retained model.
 * Zero/NULL preserves legacy behavior. Unknown enum values normalize independently
 * to LEGACY; they do not reject the other valid fields. */
typedef enum {
    CUI_QUESTIONNAIRE_SHORTCUT_LEGACY,
    CUI_QUESTIONNAIRE_SHORTCUT_OFF,
    CUI_QUESTIONNAIRE_SHORTCUT_LETTERS,
    CUI_QUESTIONNAIRE_SHORTCUT_NUMBERS
} cui_questionnaire_shortcut_mode;
typedef enum {
    CUI_QUESTIONNAIRE_ENTRY_LEGACY,
    CUI_QUESTIONNAIRE_ENTRY_ITEM_FOCUS_ONLY
} cui_questionnaire_entry_policy;
typedef enum {
    CUI_QUESTIONNAIRE_SUBMIT_LEGACY,
    CUI_QUESTIONNAIRE_SUBMIT_UNMODIFIED_ONLY,
    CUI_QUESTIONNAIRE_SUBMIT_CONTROL,
    CUI_QUESTIONNAIRE_SUBMIT_META
} cui_questionnaire_submit_policy;
typedef struct {
    cui_questionnaire_shortcut_mode shortcuts;
    cui_questionnaire_entry_policy answer_entry;
    cui_questionnaire_submit_policy submit;
} cui_questionnaire_keyboard_options;
/* Root copies the record at entry; no pointer is retained. LEGACY shortcuts use
 * options.numeric_shortcuts; LEGACY entry restores a filled answer from any
 * non-answer focus. ITEM_FOCUS_ONLY restores only from the submitted current-item
 * container; other focus starts at the first/last enabled answer. Neither changes
 * established answer-to-answer navigation or nonempty editor cursor ownership.
 * LEGACY submit accepts exactly Control OR Meta; CONTROL/META accept exactly the
 * named existing CUI_MOD_* bit (no extra modifiers). UNMODIFIED_ONLY disables
 * modified confirmation. Bare Enter still confirms a focused filled answer.
 * Host chooses CONTROL/META; this API performs no platform detection.
 * Prior-frame bindings and ordered event consumption remain the legacy contract.
 * No new capture/bubble, nested-root isolation, event observer or snapshot API.
 * Tick, callbacks, pending confirmation, disabled handling and begin/end balance
 * retain legacy behavior. A successful begin must use cui_questionnaire_end. */
bool cui_questionnaire_begin_with_keyboard(cui_context *, cui_id, const cui_theme *,
                                           cui_questionnaire_state *, cui_questionnaire_options,
                                           const cui_questionnaire_keyboard_options *);
/* Ordinary choice_control inherits the matching active root's shortcut mode.
 * This entry point permits an explicit standalone shortcut mode. NULL uses active
 * matching root mode or legacy numeric_shortcuts when standalone. A non-NULL
 * record overrides that inheritance; LEGACY resolves options.numeric_shortcuts.
 * Only shortcuts is used here. OFF removes the badge, supplies shortcut=0 to
 * custom slot renderers, and never invokes render_shortcut. Indicator/content and
 * pointer/key/AT choice activation remain available. Use a consistent root/part
 * policy: a per-choice override does not alter root-wide shortcut activation. */
bool cui_questionnaire_choice_control_with_keyboard(cui_context *, cui_id, const cui_theme *,
                                                    cui_questionnaire_state *, size_t item,
                                                    size_t choice, cui_questionnaire_options,
                                                    const cui_questionnaire_keyboard_options *);
/* Standalone helpers have no implicit root-policy inheritance. NULL/zero keeps
 * each corresponding legacy helper contract. item_id identifies the current item
 * container for ITEM_FOCUS_ONLY; zero means no item-focus restoration. Choice IDs
 * remain schema ordered. Policies are copied at entry, not retained. Only the
 * relevant field is read: answer_entry, shortcuts, or submit respectively.
 * shortcuts LEGACY preserves the supplied bool numbers argument. OFF returns false without
 * activation/consumption. confirm LEGACY uses the explicit legacy_submit_modifier exactly as the
 * existing confirm_keys helper does (unlike the automatic root's dual-modifier legacy). Existing
 * helpers remain unchanged. These functions do not paint or dispatch. */
bool cui_questionnaire_answer_keys_with_keyboard(cui_context *, cui_id scope,
                                                 cui_questionnaire_state *,
                                                 const cui_id *choice_ids, size_t count,
                                                 cui_id input_id, cui_id item_id,
                                                 const cui_questionnaire_keyboard_options *);
bool cui_questionnaire_shortcut_keys_with_keyboard(cui_context *, cui_id scope,
                                                   cui_questionnaire_state *,
                                                   const cui_id *choice_ids, size_t count,
                                                   cui_id input_id, bool numbers,
                                                   const cui_questionnaire_keyboard_options *);
bool cui_questionnaire_confirm_keys_with_keyboard(cui_context *, cui_id scope,
                                                  cui_questionnaire_state *,
                                                  const cui_id *choice_ids, size_t count,
                                                  cui_id input_id, unsigned legacy_submit_modifier,
                                                  const cui_questionnaire_keyboard_options *);
#ifdef __cplusplus
}
#endif
#endif
