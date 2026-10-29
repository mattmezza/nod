/* SPDX-License-Identifier: MIT */
#ifndef CUI_DROPDOWN_BUTTON_STYLES_H
#define CUI_DROPDOWN_BUTTON_STYLES_H
#include "cui_button_styles.h"
#include "cui_choice.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_DROPDOWN_BUTTON_PAINT_LEGACY,
    CUI_DROPDOWN_BUTTON_PAINT_BUTTON
} cui_dropdown_button_paint_policy;
/* Independent opt-in; no legacy options/state ABI resize. NULL/all-zero takes
 * the exact legacy path. BUTTON uses source Component state layering with CUI's
 * solid theme palette, not source theme-token equality. Trigger callbacks are a
 * CUI extension. Container accepts existing masks EXCEPT FOREGROUND (no text
 * cascade); geometry/border widths/clip are inspection-only. Invalid policy or
 * container patch rejects before reduction/state changes. Invalid callback
 * output discards its whole patch without undoing accepted activations.
 * Descriptors copied at entry. Functions/user/current/info/out are synchronous
 * borrowed and pure: no CUI calls, input consumption, owner/options/styles
 * mutation or reentry. Only the output patch changes the submitted paint.
 * Instance runs once per visible half; its captured value replays after
 * selected/disabled. Hidden halves invoke no callback. Old body still runs once.
 * BUTTON suppresses pointer paint for selected/open/loading/disabled; trigger
 * open uses selected paint, action ghost gets only held group surface. Loading
 * fades after replay; focus uses focus-visible and theme gate. LEGACY preserves
 * old palette/state rules. No metadata/tab/toggle or binding facade is added. */
typedef struct cui_dropdown_button_styles {
    cui_dropdown_button_paint_policy paint_policy;
    cui_button_styles action, trigger;
    cui_button_paint_patch container;
    /* Both required, default false. Explicit custom variant intent and host
     * shadow-enable gate; cui_theme has no global shadow flag. BUTTON+CUSTOM
     * only: CUI_SHADOW_XS on each enabled half (including loading), using resolved
     * corners. Disabled suppresses shadow. No callback shadow mask. */
    bool custom_shadow, host_shadows_enabled;
} cui_dropdown_button_styles;
cui_dropdown_button_result
cui_dropdown_button_styled_ex(cui_context *, cui_id container, cui_id action_id, cui_id trigger_id,
                              const cui_theme *, cui_str action_name, cui_str menu_name,
                              const cui_menu_entry *, size_t count, cui_id *menu_action,
                              cui_menu_tree_state *, cui_dropdown_button_options,
                              const cui_dropdown_button_styles *);
cui_dropdown_event_result cui_dropdown_button_styled_events(
    cui_context *, cui_id container, cui_id action_id, cui_id trigger_id, const cui_theme *,
    cui_str action_name, cui_str menu_name, const cui_menu_entry *, size_t count,
    cui_id *menu_action, cui_menu_tree_state *, cui_dropdown_button_options,
    const cui_dropdown_button_styles *, cui_dropdown_event_handlers);
#ifdef __cplusplus
}
#endif
#endif
