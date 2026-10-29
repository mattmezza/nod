/* SPDX-License-Identifier: MIT */
#include "cui/cui_plot_motion.h"
#include <math.h>
static bool cui_plot_motion_transition_valid(const cui_plot_motion_transition *p) {
    if (!p || !isfinite(p->duration) || p->duration < 0 || p->duration > 3600 ||
        !isfinite(p->delay) || fabs(p->delay) > 3600 || p->easing < CUI_PLOT_MOTION_LINEAR ||
        p->easing > CUI_PLOT_MOTION_QUARTIC_OUT)
        return false;
    float sample;
    return p->easing != CUI_PLOT_MOTION_BEZIER || cui_theme_bezier_sample(p->bezier, .5f, &sample);
}
bool cui_plot_motion_valid(const cui_plot_motion_policy *p) {
    return p && cui_plot_motion_transition_valid(&p->enter) &&
           cui_plot_motion_transition_valid(&p->exit) &&
           cui_plot_motion_transition_valid(&p->appear) && isfinite(p->pointer.response) &&
           p->pointer.response >= 0 && p->pointer.response <= 3600 &&
           isfinite(p->pointer.damping) && p->pointer.damping >= 0 && p->pointer.damping <= 100 &&
           isfinite(p->pointer.epsilon) && p->pointer.epsilon > 0 && p->pointer.epsilon <= 65536;
}
cui_plot_result cui_plot_motion_from_theme(const cui_theme_motion_tokens *tokens,
                                           cui_plot_motion_policy *out) {
    if (!out || !cui_theme_motion_valid(tokens))
        return CUI_PLOT_INVALID;
    cui_plot_motion_policy p = {.enter = {.duration = tokens->duration_fast,
                                          .easing = CUI_PLOT_MOTION_BEZIER,
                                          .bezier = tokens->easing_enter},
                                .exit = {.duration = tokens->duration_fast,
                                         .easing = CUI_PLOT_MOTION_BEZIER,
                                         .bezier = tokens->easing_exit},
                                .appear = {.duration = 1, .easing = CUI_PLOT_MOTION_QUARTIC_OUT},
                                .pointer = cui_plot_spring_defaults(tokens->duration_fast)};
    p.pointer.epsilon = .1;
    if (!cui_plot_motion_valid(&p))
        return CUI_PLOT_INVALID;
    *out = p;
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_motion_sample(const cui_plot_motion_transition *p, double t, double *out) {
    if (!out || !cui_plot_motion_transition_valid(p) || !isfinite(t) || t < 0 || t > 1)
        return CUI_PLOT_INVALID;
    double value = t;
    if (p->easing == CUI_PLOT_MOTION_QUARTIC_OUT)
        value = 1 - pow(1 - t, 4);
    if (p->easing == CUI_PLOT_MOTION_BEZIER) {
        float sampled;
        if (!cui_theme_bezier_sample(p->bezier, (float)t, &sampled))
            return CUI_PLOT_INVALID;
        value = sampled;
    }
    *out = value;
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_motion_appear_stagger(const cui_plot_motion_transition *policy,
                                               const cui_plot_appear_value *appear, size_t index,
                                               size_t count, double spread, double *out) {
    if (!out || !appear || !cui_plot_motion_transition_valid(policy))
        return CUI_PLOT_INVALID;
    double linear;
    cui_plot_result result = cui_plot_appear_stagger(appear->time, index, count, spread, &linear);
    if (result != CUI_PLOT_OK)
        return result;
    return cui_plot_motion_sample(policy, linear, out);
}
static double cui_plot_motion_sample_adapter(double t, void *user) {
    double result = NAN;
    (void)cui_plot_motion_sample(user, t, &result);
    return result;
}
cui_plot_transition_policy cui_plot_motion_transition_adapter(const cui_plot_motion_transition *p) {
    return p ? (cui_plot_transition_policy){p->duration, p->delay, cui_plot_motion_sample_adapter,
                                            (void *)p}
             : (cui_plot_transition_policy){0};
}
cui_plot_result cui_plot_motion_follow_at(cui_id owner, cui_plot_motion_pointer_state *state,
                                          cui_plot_spring_policy policy, double target, double now,
                                          bool reduced_motion, bool entering,
                                          cui_plot_spring_value *out) {
    cui_plot_motion_policy checked = {.pointer = policy};
    if (!owner || !state || !out || !cui_plot_motion_valid(&checked))
        return CUI_PLOT_INVALID;
    cui_plot_motion_pointer_state next = *state;
    if (next.owner != owner)
        next = (cui_plot_motion_pointer_state){.owner = owner};
    cui_plot_spring_value value;
    cui_plot_result result =
        cui_plot_spring_step(&next.clock, target, now, policy, reduced_motion, entering, &value);
    if (result != CUI_PLOT_OK)
        return result;
    *state = next;
    *out = value;
    return CUI_PLOT_OK;
}
cui_plot_result cui_plot_motion_follow(cui_context *ui, cui_id owner,
                                       cui_plot_motion_pointer_state *state,
                                       cui_plot_spring_policy policy, double target, bool entering,
                                       cui_plot_spring_value *out) {
    if (!ui)
        return CUI_PLOT_INVALID;
    cui_plot_result result = cui_plot_motion_follow_at(
        owner, state, policy, target, cui_frame_time(ui), cui_reduced_motion(ui), entering, out);
    if (result == CUI_PLOT_OK && out->request_frame)
        cui_invalidate(ui);
    return result;
}
