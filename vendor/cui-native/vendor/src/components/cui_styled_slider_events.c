/* SPDX-License-Identifier: MIT */
#include "cui/cui_styled_slider_events.h"
#include <math.h>
static bool cui_slider_dimension(float value) {
    return isfinite(value) && value > 0 && value <= 4096;
}
cui_component_slider_result cui_slider_events(cui_context *ui, cui_id id, const cui_theme *theme,
                                              cui_str name, double value, cui_slider_state *state,
                                              cui_slider_options options,
                                              cui_slider_style_options style,
                                              cui_component_slider_handler handler) {
    cui_component_slider_result invalid = {0};
    if (!ui || !cui_frame_is_submitting(ui) || !id || !theme || !state ||
        (name.length && !name.data) ||
        (options.value_description.length && !options.value_description.data) ||
        options.on_request || options.user || !isfinite(value) || !isfinite(options.minimum) ||
        !isfinite(options.maximum) || options.maximum < options.minimum ||
        !isfinite(options.maximum - options.minimum) || value < options.minimum ||
        value > options.maximum || !isfinite(options.step) || options.step < 0 ||
        (state->id && state->id != id) ||
        (state->dragging && (!isfinite(state->preview) || !isfinite(state->start))) ||
        !isfinite(theme->radius) || theme->radius < 0 || theme->radius > 4096)
        return invalid;
    float width = style.width ? style.width : 240;
    float track_height = style.track_height ? style.track_height : 8;
    float hit_height = style.hit_height ? style.hit_height : 24;
    float font = style.font_size ? style.font_size : theme->font_size;
    if (!cui_slider_dimension(width) || !cui_slider_dimension(track_height) ||
        !cui_slider_dimension(hit_height) || hit_height < track_height ||
        !cui_slider_dimension(font))
        return invalid;
    cui_color fill = style.custom_colors ? style.fill_color : theme->primary;
    cui_color track = style.custom_colors ? style.track_color : theme->primary;
    if (!style.custom_colors)
        track.a = (uint8_t)lroundf(track.a * .2f);
    if (options.disabled) {
        fill = theme->muted;
        track = theme->muted;
        track.a = (uint8_t)lroundf(track.a * .2f);
    }
    cui_color focus = {0};
    if (cui_focused(ui) == id && !options.disabled) {
        focus = theme->focus;
        focus.a = (uint8_t)lroundf(focus.a * .15f);
    }
    cui_begin_box(ui, id ^ UINT64_C(0x6a5039f01d82e7b4),
                  (cui_box){.width = width, .fixed_width = true});
    if (!style.hide_label)
        cui_text(ui, name, font, options.disabled ? theme->muted : theme->text);
    cui_begin_box(ui, id,
                  (cui_box){.width = width,
                            .height = hit_height,
                            .fixed_width = true,
                            .fixed_height = true,
                            .background = focus,
                            .radius = fminf(theme->radius, hit_height / 2)});
    cui_component_slider_result result =
        cui_base_slider_events(ui, id, name, value, state, options, handler);
    /* All reducer admission checks precede boxes. The reducer is invoked once. */
    double paint = state->dragging ? state->preview : value;
    paint = fmax(options.minimum, fmin(options.maximum, paint));
    double span = options.maximum - options.minimum;
    double ratio = span > 0 ? (paint - options.minimum) / span : 0;
    ratio = fmax(0, fmin(1, ratio));
    float fill_width = width * (float)ratio;
    float radius = fminf(theme->radius, track_height / 2);
    float inset = (hit_height - track_height) / 2;
    if (inset > 0) {
        cui_begin_box(ui, 0, (cui_box){.height = inset, .fixed_height = true});
        cui_end_box(ui);
    }
    cui_begin_box(ui, id ^ UINT64_C(0x7b1a0ed95e63cf28),
                  (cui_box){.width = width,
                            .height = track_height,
                            .fixed_width = true,
                            .fixed_height = true,
                            .background = track,
                            .radius = radius,
                            .clip = true});
    if (fill_width > 0) {
        cui_begin_box(ui, 0,
                      (cui_box){.width = fill_width,
                                .height = track_height,
                                .fixed_width = true,
                                .fixed_height = true,
                                .background = fill,
                                .radius = radius});
        cui_end_box(ui);
    }
    cui_end_box(ui);
    cui_end_box(ui);
    cui_end_box(ui);
    return result;
}
