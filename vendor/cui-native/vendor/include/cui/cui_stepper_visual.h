/* SPDX-License-Identifier: MIT */
#ifndef CUI_STEPPER_VISUAL_H
#define CUI_STEPPER_VISUAL_H
#include "cui_stepper_navigation.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_STEPPER_DENSITY_MD,
    CUI_STEPPER_DENSITY_XS,
    CUI_STEPPER_DENSITY_SM,
    CUI_STEPPER_DENSITY_LG
} cui_stepper_density;
typedef struct {
    bool present;
    cui_bitmap bitmap;
} cui_stepper_icon;
typedef struct {
    bool vertical, disabled;
    cui_stepper_density density;
    cui_design_size design_size; /* INHERIT preserves source density metrics. */
    const cui_stepper_icon *icons;
    size_t icon_count;
} cui_stepper_visual_options;
/* Opt-in non-centered indicator/connector composition. Legacy paint is unchanged.
 * Existing navigation REQUEST ownership/timing rules apply; no reduction twice.
 * Preflight validates every present straight-alpha RGBA8 bitmap with
 * cui_bitmap_valid, even in XS (which hides icon/ordinal). Icons are absent with
 * zero count, or one descriptor per step. No callbacks/semantics on invalid input.
 * Descriptor arrays/steps/theme are borrowed through return, never retained;
 * bitmap PIXEL BYTES remain valid through renderer completion. Revision must
 * change when pixels change. Callbacks must not mutate borrowed configuration.
 * Icon RGB is tinted by foreground (use white RGB for monochrome masks); alpha
 * is preserved. Missing icons show a one-based ordinal, except XS.
 * Passed connectors/indicators derive from index<selected, not caller status.
 * Shared XS..9XL design sizing is optional; density selects source proportions.
 * No loading state, centered text, arbitrary children/style or responsive flex
 * growth API is provided. No new semantic/focus targets for decoration. */
cui_stepper_navigation_result cui_stepper_visual_navigation(cui_context *, cui_id,
                                                            const cui_theme *, const cui_step *,
                                                            size_t count, size_t selected,
                                                            cui_stepper_visual_options,
                                                            cui_stepper_navigation_handler);
#ifdef __cplusplus
}
#endif
#endif
