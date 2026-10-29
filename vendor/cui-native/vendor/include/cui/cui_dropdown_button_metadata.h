/* SPDX-License-Identifier: MIT */
#ifndef CUI_DROPDOWN_BUTTON_METADATA_H
#define CUI_DROPDOWN_BUTTON_METADATA_H
#include "cui_choice.h"
#ifdef __cplusplus
extern "C" {
#endif
struct cui_dropdown_button_styles;
typedef struct {
    bool custom_name;
    cui_str name;
    bool custom_tab_index;
    int tab_index;
    bool skip_tab;
} cui_dropdown_button_part_metadata;
typedef struct {
    cui_dropdown_button_part_metadata action, trigger;
    bool toggle_semantics, toggled;
} cui_dropdown_button_metadata;
/* Additive submission, no legacy options/state resize. NULL/zero metadata and
 * styles independently preserve their legacy paths. Descriptors copy at entry;
 * custom name bytes are borrowed for submission and copied by Core for the
 * frame. custom_name allows explicit empty (including NULL+0). Visible custom
 * names must be valid UTF-8 without embedded NUL; invalid names reject before
 * state/input/handlers/paint. Hidden halves and unmasked fields are ignored.
 * Signed native-int ordering and skip_tab are independent. Dynamic routing uses
 * existing previous-frame eligibility/focus rules, never retroactive rerouting.
 * Action checked/value follows existing Button toggle_semantics convention,
 * independently of selected paint. It does not change the model. No optional
 * toggled-presence/pressed-button AT facade is provided: Core/adapters cannot
 * currently express that source contract. Trigger finishes unchecked (existing
 * open-menu callbacks still see its transient checked state); existing
 * busy/expanded/controls metadata survives. Metadata applies after each original
 * registration, before existing action/menu handlers; no end-frame deferral.
 * Include cui_dropdown_button_styles.h to construct the optional paint record.
 * No retained callbacks, new dispatcher, role override or binding facade. */
cui_dropdown_button_result cui_dropdown_button_with_metadata_ex(
    cui_context *, cui_id container, cui_id action_id, cui_id trigger_id, const cui_theme *,
    cui_str action_name, cui_str menu_name, const cui_menu_entry *, size_t count,
    cui_id *menu_action, cui_menu_tree_state *, cui_dropdown_button_options,
    const struct cui_dropdown_button_styles *, const cui_dropdown_button_metadata *);
cui_dropdown_event_result cui_dropdown_button_with_metadata_events(
    cui_context *, cui_id container, cui_id action_id, cui_id trigger_id, const cui_theme *,
    cui_str action_name, cui_str menu_name, const cui_menu_entry *, size_t count,
    cui_id *menu_action, cui_menu_tree_state *, cui_dropdown_button_options,
    const struct cui_dropdown_button_styles *, const cui_dropdown_button_metadata *,
    cui_dropdown_event_handlers);
#ifdef __cplusplus
}
#endif
#endif
