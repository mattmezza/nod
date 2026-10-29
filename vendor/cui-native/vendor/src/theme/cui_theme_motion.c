/* SPDX-License-Identifier: MIT */
#include "cui/cui_theme.h"
#include <math.h>
static bool cui_theme_motion_bezier_valid(cui_theme_bezier b) {
    return isfinite(b.x1) && isfinite(b.y1) && isfinite(b.x2) && isfinite(b.y2) && b.x1 >= 0 &&
           b.x1 <= 1 && b.x2 >= 0 && b.x2 <= 1 && fabsf(b.y1) <= 1000 && fabsf(b.y2) <= 1000;
}
static bool cui_theme_motion_spring_valid(cui_spring_settings s) {
    return isfinite(s.response) && s.response >= 0 && s.response <= 60 && isfinite(s.damping) &&
           s.damping >= 0 && s.damping <= 1000 && isfinite(s.epsilon) && s.epsilon > 0 &&
           s.epsilon <= 1000;
}
cui_theme_motion_tokens cui_theme_motion_default(void) {
    return (cui_theme_motion_tokens){.duration_fast = .12f,
                                     .duration_normal = .18f,
                                     .duration_slow = .28f,
                                     .easing_enter = {.16f, 1, .3f, 1},
                                     .easing_exit = {.4f, 0, 1, 1},
                                     .easing_move = {.2f, 0, 0, 1},
                                     .spring_control = {.18f, 1, .001f, true},
                                     .spring_move = {.28f, .85f, .1f, true},
                                     .distance_short = .25f,
                                     .distance_medium = .5f};
}
bool cui_theme_motion_valid(const cui_theme_motion_tokens *m) {
    if (!m)
        return false;
    const float d[] = {m->duration_instant, m->duration_fast, m->duration_normal, m->duration_slow};
    for (size_t i = 0; i < 4; i++)
        if (!isfinite(d[i]) || d[i] < 0 || d[i] > 60)
            return false;
    return cui_theme_motion_bezier_valid(m->easing_enter) &&
           cui_theme_motion_bezier_valid(m->easing_exit) &&
           cui_theme_motion_bezier_valid(m->easing_move) &&
           cui_theme_motion_spring_valid(m->spring_control) &&
           cui_theme_motion_spring_valid(m->spring_move) && isfinite(m->distance_short) &&
           m->distance_short >= 0 && m->distance_short <= 4096 && isfinite(m->distance_medium) &&
           m->distance_medium >= 0 && m->distance_medium <= 4096;
}
bool cui_theme_motion_project(const cui_theme_motion_tokens *m, float rem, bool reduced,
                              cui_theme_motion_tokens *out) {
    if (!out || !cui_theme_motion_valid(m) || !isfinite(rem) || rem <= 0 || rem > 4096 ||
        m->distance_short * rem > 1e6f || m->distance_medium * rem > 1e6f)
        return false;
    cui_theme_motion_tokens n = *m;
    n.distance_short *= rem;
    n.distance_medium *= rem;
    if (reduced) {
        n.duration_instant = n.duration_fast = n.duration_normal = n.duration_slow = 0;
        n.distance_short = n.distance_medium = 0;
        n.spring_control.response = n.spring_move.response = 0;
        n.spring_control.travel = n.spring_move.travel = false;
    }
    *out = n;
    return true;
}
bool cui_theme_bezier_sample(cui_theme_bezier b, float p, float *out) {
    if (!out || !cui_theme_motion_bezier_valid(b) || !isfinite(p) || p < 0 || p > 1)
        return false;
    if (p == 0 || p == 1) {
        *out = p;
        return true;
    }
    double lo = 0, hi = 1, t = .5;
    for (unsigned i = 0; i < 40; i++) {
        t = (lo + hi) * .5;
        double q = 1 - t, x = 3 * q * q * t * b.x1 + 3 * q * t * t * b.x2 + t * t * t;
        if (x < p)
            lo = t;
        else
            hi = t;
    }
    double q = 1 - t;
    *out = (float)(3 * q * q * t * b.y1 + 3 * q * t * t * b.y2 + t * t * t);
    return true;
}
