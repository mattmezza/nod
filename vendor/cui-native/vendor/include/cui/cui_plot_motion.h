/* SPDX-License-Identifier: MIT */
#ifndef CUI_PLOT_MOTION_H
#define CUI_PLOT_MOTION_H
#include "cui.h"
#include "cui_plot.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_PLOT_INTERACTION_AUTO,
    CUI_PLOT_INTERACTION_CURRENT_FRAME,
    CUI_PLOT_INTERACTION_LEGACY
} cui_plot_interaction_mode;
typedef enum {
    CUI_PLOT_MOTION_LINEAR,
    CUI_PLOT_MOTION_BEZIER,
    CUI_PLOT_MOTION_QUARTIC_OUT
} cui_plot_motion_easing;
typedef struct {
    double duration, delay; /* finite seconds0..3600; delay may be negative */
    cui_plot_motion_easing easing;
    cui_theme_bezier bezier;
} cui_plot_motion_transition;
typedef struct {
    cui_plot_motion_transition enter, exit, appear;
    cui_plot_spring_policy pointer;
} cui_plot_motion_policy;
/* Styled source projection; no pointers/lifetime coupling to tokens. Invalid
 * input preserves output. Source ignores move/control springs and move easing. */
cui_plot_result cui_plot_motion_from_theme(const cui_theme_motion_tokens *,
                                           cui_plot_motion_policy *);
bool cui_plot_motion_valid(const cui_plot_motion_policy *);
/* Value sampling: progress0..1; invalid input preserves output. */
cui_plot_result cui_plot_motion_sample(const cui_plot_motion_transition *, double progress,
                                       double *);
/* Later stagger sampling uses an explicit value policy; safe for copied state. */
cui_plot_result cui_plot_motion_appear_stagger(const cui_plot_motion_transition *,
                                               const cui_plot_appear_value *, size_t index,
                                               size_t count, double spread, double *);
/* Synchronous adapter only: resulting user points to the supplied transition.
 * Never retain it or use it after that value's lifetime ends. Owner policies
 * themselves contain no callback/user pointers and are safely copied by value. */
cui_plot_transition_policy cui_plot_motion_transition_adapter(const cui_plot_motion_transition *);
typedef struct {
    cui_id owner;
    cui_plot_spring_clock clock;
} cui_plot_motion_pointer_state;
/* Marks callbacks use this with their own datum coordinate. It does not infer
 * datum coordinates or current-frame bounds. Zero-init caller state; owner change
 * resets. Core reduced motion wins; entering snaps. Invalid preserves state/out. */
cui_plot_result cui_plot_motion_follow(cui_context *, cui_id owner, cui_plot_motion_pointer_state *,
                                       cui_plot_spring_policy, double target, bool entering,
                                       cui_plot_spring_value *);
/* Pure clock seam for paint callbacks. Caller requests a frame when out says so. */
cui_plot_result cui_plot_motion_follow_at(cui_id, cui_plot_motion_pointer_state *,
                                          cui_plot_spring_policy, double target, double now,
                                          bool reduced_motion, bool entering,
                                          cui_plot_spring_value *);
#ifdef __cplusplus
}
#endif
#endif
