/* SPDX-License-Identifier: MIT */
#include "cui/cui_stepper_navigation.h"
/* Private paint seam shared with legacy styled Stepper. */
void cui_numeric_stepper_paint(cui_context *, cui_id, const cui_theme *, const cui_step *, size_t,
                               size_t, bool, bool);
cui_stepper_navigation_result cui_stepper_navigation(cui_context *ui, cui_id id,
                                                     const cui_theme *theme, const cui_step *steps,
                                                     size_t count, size_t selected, bool vertical,
                                                     bool disabled,
                                                     cui_stepper_navigation_handler handler) {
    if (!theme)
        return (cui_stepper_navigation_result){0};
    cui_stepper_navigation_result result =
        cui_base_stepper_navigation(ui, id, steps, count, selected, disabled, handler);
    if (result.valid)
        cui_numeric_stepper_paint(ui, id, theme, steps, count, selected, vertical, disabled);
    return result;
}
