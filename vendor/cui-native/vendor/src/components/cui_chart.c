/* SPDX-License-Identifier: MIT */
#include "cui/cui_chart.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
uint64_t cui_chart_frame_key(const cui_chart_options *, const cui_chart_state *);
static double chart_appear_duration(const cui_chart_options *o) {
    return o->appear_duration > 0 ? o->appear_duration
           : o->custom_motion     ? o->motion.appear.duration
                                  : 1;
}
static bool chart_motion_valid(const cui_chart_options *o) {
    return !o->custom_motion || cui_plot_motion_valid(&o->motion);
}
static bool chart_motion_transition_equal(cui_plot_motion_transition a,
                                          cui_plot_motion_transition b) {
    return a.duration == b.duration && a.delay == b.delay && a.easing == b.easing &&
           a.bezier.x1 == b.bezier.x1 && a.bezier.y1 == b.bezier.y1 && a.bezier.x2 == b.bezier.x2 &&
           a.bezier.y2 == b.bezier.y2;
}
static bool chart_motion_equal(cui_plot_motion_policy a, cui_plot_motion_policy b) {
    return chart_motion_transition_equal(a.enter, b.enter) &&
           chart_motion_transition_equal(a.exit, b.exit) &&
           chart_motion_transition_equal(a.appear, b.appear) &&
           a.pointer.response == b.pointer.response && a.pointer.damping == b.pointer.damping &&
           a.pointer.epsilon == b.pointer.epsilon && a.pointer.travel == b.pointer.travel;
}
static void chart_motion_reset(cui_chart_state *s) {
    s->motion_tooltip = (cui_plot_transition_clock){0};
    memset(s->motion_line, 0, sizeof s->motion_line);
    s->motion_mark = (cui_plot_spring_clock){0};
    memset(s->motion_pie, 0, sizeof s->motion_pie);
}
static cui_str cui_chart_number(char *buffer, size_t capacity, double value,
                                cui_chart_format_fn formatter, void *user) {
    size_t length;
    if (formatter)
        length = formatter(value, buffer, capacity, user);
    else {
        length = 0;
        for (int precision = 0; precision <= DBL_DECIMAL_DIG; precision++) {
            int written = snprintf(buffer, capacity, "%.*f", precision, value);
            if (written <= 0 || (size_t)written >= capacity)
                break;
            char *end = NULL;
            if (strtod(buffer, &end) == value && end && !*end) {
                length = (size_t)written;
                break;
            }
        }
        if (!length) {
            int written = snprintf(buffer, capacity, "%.*g", DBL_DECIMAL_DIG, value);
            length = written > 0 ? (size_t)written : 0;
        }
    }
    if (length >= capacity)
        length = capacity ? capacity - 1 : 0;
    return (cui_str){buffer, length};
}
static cui_str cui_chart_tick_number(char *buffer, size_t capacity, double value,
                                     cui_chart_format_fn formatter, void *user) {
    if (formatter)
        return cui_chart_number(buffer, capacity, value, formatter, user);
    int written =
        snprintf(buffer, capacity, fabs(value - round(value)) < .001 ? "%.0f" : "%.1f", value);
    size_t length = written > 0 ? (size_t)written : 0;
    if (length >= capacity)
        length = capacity ? capacity - 1 : 0;
    return (cui_str){buffer, length};
}
static void cui_chart_hover_spring(float *position, float *velocity, float target, float dt,
                                   float response) {
    float omega = 6.28318530718f / response;
    float displacement = *position - target;
    float movement = *velocity + omega * displacement;
    float decay = expf(-omega * dt);
    *position = target + (displacement + movement * dt) * decay;
    *velocity = (*velocity - omega * movement * dt) * decay;
    if (fabsf(*position - target) < .1f && fabsf(*velocity) < .1f) {
        *position = target;
        *velocity = 0;
    }
}
static float cui_chart_hover_ease(float progress, bool entering) {
    progress = fmaxf(0, fminf(1, progress));
    if (progress == 0 || progress == 1)
        return progress;
    float x1 = entering ? .16f : .4f, y1 = entering ? 1.f : 0.f;
    float x2 = entering ? .3f : 1.f, y2 = 1.f;
    float lo = 0, hi = 1;
    for (unsigned i = 0; i < 20; i++) {
        float t = (lo + hi) / 2, inv = 1 - t;
        float x = 3 * inv * inv * t * x1 + 3 * inv * t * t * x2 + t * t * t;
        if (x < progress)
            lo = t;
        else
            hi = t;
    }
    float t = (lo + hi) / 2, inv = 1 - t;
    return 3 * inv * inv * t * y1 + 3 * inv * t * t * y2 + t * t * t;
}
static void cui_chart_hover_dot(cui_context *ui, cui_id id, float x, float y, cui_color fill,
                                cui_color background, float progress) {
    float halo_size = 20 * progress;
    cui_color halo = fill;
    halo.a = (unsigned char)lroundf(halo.a * .2f * progress);
    if (halo_size > 0 && halo.a) {
        cui_begin_absolute_box(
            ui, id ^ UINT64_C(0x6b9a27a71d001011),
            (cui_box){.width = halo_size,
                      .height = halo_size,
                      .radius = halo_size / 2,
                      .fixed_width = true,
                      .fixed_height = true,
                      .background = halo},
            (cui_rect){x - halo_size / 2, y - halo_size / 2, halo_size, halo_size});
        cui_end_box(ui);
    }
    fill.a = (unsigned char)lroundf(fill.a * progress);
    background.a = (unsigned char)lroundf(background.a * progress);
    cui_begin_absolute_box(ui, id ^ UINT64_C(0x36b0f2a18e001011),
                           (cui_box){.width = 8,
                                     .height = 8,
                                     .radius = 4,
                                     .fixed_width = true,
                                     .fixed_height = true,
                                     .background = fill,
                                     .border = background,
                                     .border_width = 1},
                           (cui_rect){x - 4, y - 4, 8, 8});
    cui_end_box(ui);
}
static void cui_chart_horizontal_bar_extent(const cui_chart_options *o, float *left, float *right) {
    float category_gap = o->hide_x_axis                     ? 0
                         : o->custom_horizontal_bar_gutters ? o->bar_category_gutter
                                                            : 24;
    float value_end = o->custom_horizontal_bar_gutters ? o->bar_value_end_gutter : 12;
    *left = o->bar_alignment == CUI_CHART_BAR_LEFT ? category_gap : value_end;
    *right = o->width - (o->bar_alignment == CUI_CHART_BAR_RIGHT ? category_gap : value_end);
}
static bool cui_chart_sankey_label_lines(const cui_chart_options *o,
                                         const cui_chart_sankey_node *node, size_t index,
                                         cui_chart_sankey_label out[16], size_t *count,
                                         char value_buffer[48]) {
    size_t n = 0;
    if (o->sankey_labels)
        n = o->sankey_labels(&o->data[index], index, node->throughput, out, 16,
                             o->sankey_labels_user);
    else {
        float font = o->sankey_font_size > 0 ? o->sankey_font_size : 10;
        if (o->sankey_show_values) {
            cui_str value = cui_chart_number(value_buffer, 48, node->throughput,
                                             o->sankey_value_format, o->format_user);
            out[n++] = (cui_chart_sankey_label){.text = value, .font_size = font};
        }
        if (o->sankey_show_labels && o->data[index].label.length)
            out[n++] = (cui_chart_sankey_label){.text = o->data[index].label, .font_size = font};
    }
    if (n > 16)
        return false;
    for (size_t i = 0; i < n; i++)
        if ((out[i].text.length && !out[i].text.data) || !isfinite(out[i].font_size) ||
            out[i].font_size < 0)
            return false;
    *count = n;
    return true;
}
static bool chart_radar_title_valid(const cui_chart_options *o) {
    return o && o->radar_tooltip_title_mode >= CUI_CHART_RADAR_TOOLTIP_TITLE_LEGACY &&
           o->radar_tooltip_title_mode <= CUI_CHART_RADAR_TOOLTIP_TITLE_SOURCE_LABEL &&
           (o->radar_tooltip_title_mode == CUI_CHART_RADAR_TOOLTIP_TITLE_LEGACY ||
            o->kind == CUI_CHART_RADAR);
}
static cui_str chart_tooltip_fallback_title(const cui_chart_options *o,
                                            const cui_chart_datum *datum) {
    if (o->kind == CUI_CHART_PIE || (o->kind == CUI_CHART_SANKEY && !o->sankey_show_labels) ||
        (o->kind == CUI_CHART_RADAR &&
         o->radar_tooltip_title_mode == CUI_CHART_RADAR_TOOLTIP_TITLE_SOURCE_LABEL &&
         o->radar_label_measure && o->radar_label_paint))
        return (cui_str){0};
    return datum->label;
}
static float chart_point_gutter(const cui_chart_options *o) {
    size_t slots;
    float gutter;
    return cui_chart_point_axis_layout(o, &slots, &gutter) == CUI_PLOT_OK
               ? gutter
               : (o->hide_x_axis ? 0 : 20);
}
static size_t chart_point_slots(const cui_chart_options *o) {
    size_t slots;
    float gutter;
    return cui_chart_point_axis_layout(o, &slots, &gutter) == CUI_PLOT_OK
               ? slots
               : (o->point_count ? o->point_count : o->count);
}
float cui_chart_measure_value_axis_gutter(cui_context *ui, const cui_chart_options *o) {
    if (!ui || !o || !o->show_y_axis || o->y_axis_inside || !o->data || !o->count ||
        (o->kind == CUI_CHART_BAR &&
         (o->bar_alignment == CUI_CHART_BAR_LEFT || o->bar_alignment == CUI_CHART_BAR_RIGHT)) ||
        o->kind == CUI_CHART_PIE || o->kind == CUI_CHART_RADAR || o->kind == CUI_CHART_SANKEY ||
        o->kind == CUI_CHART_CANDLESTICK)
        return 0;
    double lo, hi;
    if (!cui_chart_value_domain(o, &lo, &hi))
        return 0;
    unsigned count = o->tick_count ? o->tick_count : 5;
    if (count < 2)
        count = 2;
    if (count > 1000)
        return 0;
    unsigned visible = count;
    float gutter = 32;
    bool point_chart = o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA;
    double top = point_chart ? (o->custom_y_padding ? o->y_padding_top : 10) : 0;
    double bottom = point_chart ? o->height - chart_point_gutter(o) -
                                      (o->custom_y_padding ? o->y_padding_bottom : 0)
                                : 0;
    cui_plot_scale scale = {CUI_PLOT_LINEAR, lo, hi, bottom, top, true};
    for (unsigned i = 0; i < visible; i++) {
        unsigned tick = i;
        double value = 0;
        cui_plot_scale tick_scale = {CUI_PLOT_LINEAR, lo, hi, 1, 0, true};
        if (cui_plot_inverse(&tick_scale, (double)tick / (count - 1), &value) != CUI_PLOT_OK)
            return 0;
        if (point_chart) {
            double pixel_y = (o->height - chart_point_gutter(o)) * tick / (count - 1);
            if (cui_plot_inverse(&scale, pixel_y, &value) != CUI_PLOT_OK)
                return 0;
        }
        char buffer[48];
        cui_str label =
            cui_chart_tick_number(buffer, sizeof buffer, value, o->y_tick_format, o->format_user);
        cui_size measured = cui_measure_text(ui, label, 10);
        if (isfinite(measured.width))
            gutter = fmaxf(gutter, measured.width + 4);
    }
    return gutter;
}
cui_plot_result cui_chart_measure_horizontal_bar_gutters(cui_context *ui,
                                                         const cui_chart_options *o,
                                                         float *category, float *value_end) {
    if (!ui || !o || !category || !value_end || o->kind != CUI_CHART_BAR ||
        (o->bar_alignment != CUI_CHART_BAR_LEFT && o->bar_alignment != CUI_CHART_BAR_RIGHT) ||
        !o->data || !o->count || o->count > 4096 || !o->width || !o->height)
        return CUI_PLOT_INVALID;
    float measured_category = 0, measured_end = o->show_bar_values ? 0 : 8;
    for (size_t i = 0; i < o->count; i++) {
        cui_str label = o->data[i].label;
        if ((label.length && !label.data) || !isfinite(o->data[i].value))
            return CUI_PLOT_INVALID;
        if (!o->hide_x_axis) {
            cui_size size = cui_measure_text(ui, label, 10);
            if (!isfinite(size.width) || size.width < 0)
                return CUI_PLOT_INVALID;
            measured_category = fmaxf(measured_category, size.width);
        }
        if (o->show_bar_values) {
            char buffer[48];
            cui_str value = cui_chart_number(buffer, sizeof buffer, o->data[i].value,
                                             o->bar_label_format, o->format_user);
            cui_size size = cui_measure_text(ui, value, 10);
            if (!isfinite(size.width) || size.width < 0)
                return CUI_PLOT_INVALID;
            measured_end = fmaxf(measured_end, size.width);
        }
    }
    if (!o->hide_x_axis)
        measured_category += 4;
    if (o->show_bar_values)
        measured_end += 4;
    if (!isfinite(measured_category) || !isfinite(measured_end) ||
        measured_category + measured_end >= o->width)
        return CUI_PLOT_EMPTY;
    *category = measured_category;
    *value_end = measured_end;
    return CUI_PLOT_OK;
}
cui_plot_result cui_chart_measure_sankey_margins(cui_context *ui, const cui_chart_options *o,
                                                 cui_chart_sankey_margins *out) {
    if (!ui || !o || !out || o->kind != CUI_CHART_SANKEY || !o->width || !o->height ||
        !isfinite(o->sankey_font_size) || o->sankey_font_size < 0 ||
        !isfinite(o->sankey_label_gap) || o->sankey_label_gap < 0 ||
        (!!o->sankey_label_measure != !!o->sankey_label_paint))
        return CUI_PLOT_INVALID;
    cui_chart_options plain = *o;
    plain.sankey_margins = (cui_chart_sankey_margins){0};
    cui_chart_sankey_node nodes[256];
    cui_plot_result status = cui_chart_sankey_layout(&plain, nodes, 256);
    if (status != CUI_PLOT_OK)
        return status;
    cui_chart_sankey_margins measured = {0};
    if (!o->sankey_show_labels && !o->sankey_show_values && !o->sankey_labels &&
        !o->sankey_label_measure) {
        *out = measured;
        return CUI_PLOT_OK;
    }
    size_t last_layer = 0;
    for (size_t i = 0; i < o->count; i++)
        if (nodes[i].layer > last_layer)
            last_layer = nodes[i].layer;
    float gap = o->sankey_label_gap > 0 ? o->sankey_label_gap : 6;
    bool has_content = false;
    for (size_t i = 0; i < o->count; i++) {
        float width = 0, block_height = 0;
        if (o->sankey_label_measure) {
            cui_size size = o->sankey_label_measure(ui, &o->data[i], i, nodes[i].throughput,
                                                    o->sankey_label_user);
            if (!isfinite(size.width) || !isfinite(size.height) || size.width < 0 ||
                size.height < 0 || size.width > 16384 || size.height > 16384)
                return CUI_PLOT_INVALID;
            width = size.width;
            block_height = size.height;
        } else {
            cui_chart_sankey_label lines[16] = {0};
            size_t line_count = 0;
            char value_buffer[48];
            if (!cui_chart_sankey_label_lines(o, &nodes[i], i, lines, &line_count, value_buffer))
                return CUI_PLOT_INVALID;
            for (size_t line = 0; line < line_count; line++) {
                float size = lines[line].font_size > 0 ? lines[line].font_size : 10;
                width = fmaxf(width, cui_measure_text(ui, lines[line].text, size).width);
                block_height += size + 2;
            }
        }
        if (width == 0 && block_height == 0)
            continue;
        has_content = true;
        if (!isfinite(width) || width < 0)
            return CUI_PLOT_INVALID;
        if (nodes[i].layer == 0)
            measured.left = fmaxf(measured.left, width + gap);
        else if (nodes[i].layer == last_layer)
            measured.right = fmaxf(measured.right, width + gap);
        else
            measured.top = fmaxf(measured.top, block_height + 2);
    }
    if (!has_content) {
        *out = (cui_chart_sankey_margins){0};
        return CUI_PLOT_OK;
    }
    measured.left = fminf(measured.left, o->width * .2f);
    measured.right = fminf(measured.right, o->width * .2f);
    measured.bottom = 2;
    float vertical = measured.top + measured.bottom;
    if (vertical > o->height * .6f) {
        float scale = o->height * .6f / vertical;
        measured.top *= scale;
        measured.bottom *= scale;
    }
    plain.sankey_margins = measured;
    if (cui_chart_sankey_layout(&plain, nodes, 256) != CUI_PLOT_OK)
        return CUI_PLOT_INVALID;
    *out = measured;
    return CUI_PLOT_OK;
}
static bool cui_chart_x_label_visible(const cui_chart_options *o, size_t index) {
    size_t slots = o->point_count > o->count ? o->point_count : o->count;
    if (!o->x_tick_count && !o->custom_x_tick_count) {
        size_t margin = o->x_tick_margin ? o->x_tick_margin : 1;
        return (index + 1) % margin == 0;
    }
    if (!o->x_tick_count)
        return false;
    if (o->x_tick_count >= slots)
        return true;
    if (o->x_tick_count == 1)
        return index == 0;
    for (unsigned tick = 0; tick < o->x_tick_count; tick++) {
        size_t selected = (size_t)lround((double)tick * (slots - 1) / (o->x_tick_count - 1));
        if (selected == index)
            return true;
    }
    return false;
}
cui_chart_options cui_chart_theme(cui_chart_options o, const cui_theme *theme) {
    if (!theme)
        return o;
    if (!o.background.a)
        o.background = theme->background;
    if (!o.foreground.a)
        o.foreground = theme->text;
    if (!o.grid.a)
        o.grid = theme->chart_grid;
    cui_color palette[5] = {theme->chart_1, theme->chart_2, theme->chart_3, theme->chart_4,
                            theme->chart_5};
    for (size_t i = 0; i < 8; i++)
        if (!o.series[i].a)
            o.series[i] = o.kind == CUI_CHART_RADAR || o.kind == CUI_CHART_SANKEY ? palette[i % 5]
                                                                                  : theme->chart_2;
    if (!o.bullish.a)
        o.bullish = theme->chart_bullish;
    if (!o.bearish.a)
        o.bearish = theme->chart_bearish;
    return o;
}
cui_plot_result cui_chart_render_animated(cui_context *ui, cui_chart_state *state,
                                          const cui_chart_options *options) {
    if (!ui || !state || !chart_radar_title_valid(options) ||
        options->layout_mode < CUI_CHART_LAYOUT_LEGACY ||
        options->layout_mode > CUI_CHART_LAYOUT_SOURCE_POINT ||
        (options->layout_mode == CUI_CHART_LAYOUT_SOURCE_POINT && options->kind != CUI_CHART_LINE &&
         options->kind != CUI_CHART_AREA) ||
        !chart_motion_valid(options) || !isfinite(options->appear_duration) ||
        options->appear_duration < 0 || !isfinite(options->hover_duration) ||
        options->hover_duration < 0)
        return CUI_PLOT_INVALID;
    cui_chart_options frame = *options;
    if (frame.kind == CUI_CHART_SANKEY && !frame.custom_sankey_margins &&
        frame.sankey_margins.left == 0 && frame.sankey_margins.right == 0 &&
        frame.sankey_margins.top == 0 && frame.sankey_margins.bottom == 0) {
        cui_plot_result measured =
            cui_chart_measure_sankey_margins(ui, &frame, &frame.sankey_margins);
        if (measured != CUI_PLOT_OK)
            return measured;
    }
    if (frame.show_y_axis && !frame.y_axis_inside && frame.value_axis_gutter == 0 &&
        (frame.kind == CUI_CHART_LINE || frame.kind == CUI_CHART_AREA ||
         (frame.kind == CUI_CHART_BAR && frame.bar_alignment != CUI_CHART_BAR_LEFT &&
          frame.bar_alignment != CUI_CHART_BAR_RIGHT))) {
        frame.value_axis_gutter = cui_chart_measure_value_axis_gutter(ui, &frame);
        if (frame.value_axis_gutter <= 0)
            return CUI_PLOT_INVALID;
    }
    if (frame.kind == CUI_CHART_BAR &&
        (frame.bar_alignment == CUI_CHART_BAR_LEFT || frame.bar_alignment == CUI_CHART_BAR_RIGHT) &&
        !frame.custom_horizontal_bar_gutters) {
        cui_plot_result measured = cui_chart_measure_horizontal_bar_gutters(
            ui, &frame, &frame.bar_category_gutter, &frame.bar_value_end_gutter);
        if (measured != CUI_PLOT_OK)
            return measured;
        frame.custom_horizontal_bar_gutters = true;
    }
    cui_chart_options request = frame;
    bool restart = !state->appear_started || state->active_appear_key != options->appear_key;
    double now = cui_frame_time(ui);
    double started = restart ? now : state->appear_started_at;
    double duration = chart_appear_duration(options);
    frame.reveal_enabled = true;
    frame.reduced_motion = options->reduced_motion || cui_reduced_motion(ui);
    bool appearance_active = false;
    if (options->disable_appear || frame.reduced_motion)
        frame.reveal_progress = 1;
    else {
        double delay = options->custom_motion ? options->motion.appear.delay : 0;
        double elapsed = fmax(0, now - started - delay);
        double t = duration == 0 ? 1 : fmax(0, fmin(1, elapsed / duration));
        /* Paint may round to1 before the linear hover gate completes. */
        appearance_active = t < 1;
        double sampled = 1 - pow(1 - t, 4);
        if (options->custom_motion &&
            cui_plot_motion_sample(&options->motion.appear, t, &sampled) != CUI_PLOT_OK)
            return CUI_PLOT_INVALID;
        frame.reveal_progress = (float)fmax(0, fmin(1, sampled));
    }
    cui_plot_result result = cui_chart_render(state, &frame);
    if (result != CUI_PLOT_OK)
        return result;
    state->appear_started = true;
    state->appear_started_at = started;
    state->active_appear_key = options->appear_key;
    state->animated_at_time = now;
    state->animated_at_valid = true;
    state->animated_custom_motion = options->custom_motion;
    if (options->custom_motion)
        state->animated_motion = options->motion;
    state->animated_request_key = cui_chart_frame_key(&request, state);
    if (appearance_active)
        cui_invalidate(ui);
    return CUI_PLOT_OK;
}
typedef struct {
    double now;
    float delta;
    bool reduced;
    cui_prepaint_writer *writer;
    cui_context *ui;
} chart_paint_clock;
static void chart_request(const chart_paint_clock *clock) {
    if (clock->writer)
        cui_prepaint_request_frame(clock->writer);
    else
        cui_invalidate(clock->ui);
}
static bool chart_pointer_at(const chart_paint_clock *clock, const cui_chart_options *o,
                             cui_plot_spring_clock *spring, float target, bool entering,
                             float *position, float *velocity) {
    cui_plot_spring_policy policy = o->motion.pointer;
    if (o->hover_duration > 0)
        policy.response = o->hover_duration;
    cui_plot_spring_value value;
    if (cui_plot_spring_step(spring, target, clock->now, policy,
                             o->reduced_motion || clock->reduced, entering, &value) != CUI_PLOT_OK)
        return false;
    *position = (float)value.value;
    *velocity = (float)value.velocity;
    if (value.request_frame)
        chart_request(clock);
    return true;
}
static bool chart_paint_update(const chart_paint_clock *clock, cui_chart_state *state,
                               const cui_chart_options *o, float w, float h,
                               bool rendered_for_appearance) {
    /* Source prepaint supplies no live datum until appearance is complete.
     * Keep the valid cursor separately so a stationary pointer can resume;
     * retained tooltip/emphasis presentation still follows its exit clock. */
    bool appearing = state->rendered_reveal_progress < 1;
    if (!o->reveal_enabled && state->appear_started && !o->disable_appear && !o->reduced_motion &&
        !clock->reduced) {
        double duration = chart_appear_duration(o);
        /* Gate by linear time: quartic float progress may round to1 early. */
        double delay = o->custom_motion ? o->motion.appear.delay : 0;
        appearing =
            duration > 0 && fmax(0, clock->now - state->appear_started_at - delay) < duration;
    }
    state->appear_hover_pending = appearing && !o->noninteractive && state->hovered < o->count;
    if (appearing) {
        state->hovered = SIZE_MAX;
        state->hovered_value = 0;
    }
    {
        bool active = !o->noninteractive && state->hovered < o->count;
        bool instant = o->reduced_motion || clock->reduced;
        if (active) {
            state->tooltip_visual_index = state->hovered;
            state->tooltip_visual_value = state->hovered_value;
        }
        float dt = clock->delta;
        if (!isfinite(dt) || dt < 0)
            dt = 0;
        if (dt == 0 && active && state->tooltip_phase == 0)
            dt = 1.f / 60;
        float duration = o->hover_duration > 0 ? o->hover_duration : .12f;
        if (!state->tooltip_started || state->tooltip_active != active) {
            state->tooltip_from = state->tooltip_focus;
            state->tooltip_phase = 0;
            state->tooltip_active = active;
            state->tooltip_started = true;
        }
        if (o->custom_motion) {
            cui_plot_motion_transition policy = active ? o->motion.enter : o->motion.exit;
            if (o->hover_duration > 0)
                policy.duration = o->hover_duration;
            cui_plot_motion_value value;
            if (cui_plot_transition_step(&state->motion_tooltip, active ? 1 : 0, clock->now,
                                         cui_plot_motion_transition_adapter(&policy), instant,
                                         &value) != CUI_PLOT_OK)
                return false;
            state->tooltip_focus = (float)fmax(0, fmin(1, value.value));
            state->tooltip_phase = value.request_frame ? 0 : 1;
            if (value.request_frame)
                chart_request(clock);
        } else if (instant) {
            state->tooltip_phase = 1;
            state->tooltip_focus = active ? 1 : 0;
        } else {
            state->tooltip_phase = fminf(1, state->tooltip_phase + dt / duration);
            float eased = cui_chart_hover_ease(state->tooltip_phase, active);
            state->tooltip_focus =
                state->tooltip_from + ((active ? 1.f : 0.f) - state->tooltip_from) * eased;
        }
        if (!active && state->tooltip_focus <= 0)
            state->tooltip_visual_index = SIZE_MAX;
        if (!instant && state->tooltip_phase < 1)
            chart_request(clock);
    }
    if (o->kind == CUI_CHART_BAR || o->kind == CUI_CHART_PIE || o->kind == CUI_CHART_SANKEY ||
        o->kind == CUI_CHART_CANDLESTICK) {
        bool active = !o->noninteractive && state->hovered < o->count;
        if (active)
            state->emphasis_visual_index = state->hovered;
        bool instant = o->reduced_motion || clock->reduced;
        float duration = o->hover_duration > 0 ? o->hover_duration : .12f;
        float dt = clock->delta;
        if (!isfinite(dt) || dt < 0)
            dt = 0;
        if (dt == 0 && active && !instant)
            dt = 1.f / 60;
        dt = fminf(dt, .25f);
        state->emphasis_focus = state->tooltip_focus;
        bool moving = state->emphasis_focus > 0 && state->emphasis_focus < 1;
        if (o->kind == CUI_CHART_BAR && active) {
            float target;
            if (cui_chart_bar_band_center(o, state->hovered, &target) == CUI_PLOT_OK) {
                if (o->custom_motion) {
                    if (!chart_pointer_at(clock, o, &state->motion_mark, target,
                                          !state->emphasis_started, &state->emphasis_center,
                                          &state->emphasis_velocity))
                        return false;
                    state->emphasis_started = true;
                } else if (!state->emphasis_started || instant) {
                    state->emphasis_center = target;
                    state->emphasis_velocity = 0;
                    state->emphasis_started = true;
                } else
                    cui_chart_hover_spring(&state->emphasis_center, &state->emphasis_velocity,
                                           target, dt, duration);
                moving |= fabsf(state->emphasis_center - target) > .1f ||
                          fabsf(state->emphasis_velocity) > .1f;
            }
        }
        if (o->kind == CUI_CHART_CANDLESTICK && active) {
            cui_chart_candle_geometry candle;
            if (cui_chart_candle_bounds(o, state->hovered, &candle) == CUI_PLOT_OK) {
                float target = (float)candle.wick.x_min;
                if (o->custom_motion) {
                    if (!chart_pointer_at(clock, o, &state->motion_mark, target,
                                          !state->emphasis_started, &state->emphasis_center,
                                          &state->emphasis_velocity))
                        return false;
                    state->emphasis_started = true;
                } else if (!state->emphasis_started || instant) {
                    state->emphasis_center = target;
                    state->emphasis_velocity = 0;
                    state->emphasis_started = true;
                } else
                    cui_chart_hover_spring(&state->emphasis_center, &state->emphasis_velocity,
                                           target, dt, duration);
                moving |= fabsf(state->emphasis_center - target) > .1f ||
                          fabsf(state->emphasis_velocity) > .1f;
            }
        }
        if (o->kind == CUI_CHART_PIE)
            for (size_t i = 0; i < o->count && i < 256; i++) {
                float target = active && state->hovered == i ? 1 : 0;
                if (o->custom_motion) {
                    if (!chart_pointer_at(clock, o, &state->motion_pie[i], target, false,
                                          &state->pie_lift[i], &state->pie_lift_velocity[i]))
                        return false;
                } else if (instant) {
                    state->pie_lift[i] = target;
                    state->pie_lift_velocity[i] = 0;
                } else
                    cui_chart_hover_spring(&state->pie_lift[i], &state->pie_lift_velocity[i],
                                           target, dt, duration);
                moving |= fabsf(state->pie_lift[i] - target) > .001f ||
                          fabsf(state->pie_lift_velocity[i]) > .001f;
            }
        if (!active && state->emphasis_focus == 0)
            state->emphasis_started = false;
        if (!active && state->emphasis_focus == 0)
            state->emphasis_visual_index = SIZE_MAX;
        if (!instant && moving)
            chart_request(clock);
        bool hover_raster =
            (o->kind == CUI_CHART_BAR || o->kind == CUI_CHART_PIE || o->kind == CUI_CHART_SANKEY) &&
            (active || moving || state->emphasis_focus > 0);
        if (!rendered_for_appearance || hover_raster) {
            cui_chart_options frame = *o;
            if (!frame.reveal_enabled && state->appear_started &&
                state->rendered_reveal_progress < 1) {
                frame.reveal_enabled = true;
                frame.reveal_progress = state->rendered_reveal_progress;
            }
            if (cui_chart_render(state, &frame) != CUI_PLOT_OK)
                return false;
        }
    }
    if (o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA) {
        bool active = !o->noninteractive && state->hovered < o->count;
        float target_x = 0, target_y[8] = {0}, pixel_x, pixel_y;
        if (active &&
            cui_chart_point_coordinates(o, state->hovered, 0, &pixel_x, &pixel_y) != CUI_PLOT_OK)
            active = false;
        if (active) {
            target_x = pixel_x * w / o->width;
            for (size_t series = 0; series <= o->extra_series_count; series++) {
                if (cui_chart_point_coordinates(o, state->hovered, series, &pixel_x, &pixel_y) ==
                    CUI_PLOT_OK)
                    target_y[series] = pixel_y * h / o->height;
            }
            if (!state->hover_visual_started) {
                state->hover_visual_started = true;
                state->hover_progress = 0;
                state->hover_x = target_x;
                state->hover_vx = 0;
                for (size_t series = 0; series <= o->extra_series_count; series++) {
                    state->hover_y[series] = target_y[series];
                    state->hover_vy[series] = 0;
                }
            }
            state->hover_visual_index = state->hovered;
        }
        if (state->hover_visual_started) {
            bool instant = o->reduced_motion || clock->reduced;
            float duration = o->hover_duration > 0 ? o->hover_duration : .12f;
            float dt = clock->delta;
            if (!isfinite(dt) || dt < 0)
                dt = 0;
            if (dt == 0 && active && state->hover_progress == 0)
                dt = 1.f / 60;
            dt = fminf(dt, .25f);
            state->hover_progress =
                o->custom_motion ? state->tooltip_focus
                : instant
                    ? (active ? 1 : 0)
                    : fmaxf(0, fminf(1, state->hover_progress + (active ? dt : -dt) / duration));
            if (active) {
                if (o->custom_motion) {
                    if (!chart_pointer_at(clock, o, &state->motion_line[0], target_x,
                                          !state->motion_line[0].initialized, &state->hover_x,
                                          &state->hover_vx))
                        return false;
                } else if (instant) {
                    state->hover_x = target_x;
                    state->hover_vx = 0;
                } else
                    cui_chart_hover_spring(&state->hover_x, &state->hover_vx, target_x, dt,
                                           duration);
                for (size_t series = 0; series <= o->extra_series_count; series++)
                    if (o->custom_motion) {
                        if (!chart_pointer_at(clock, o, &state->motion_line[series + 1],
                                              target_y[series],
                                              !state->motion_line[series + 1].initialized,
                                              &state->hover_y[series], &state->hover_vy[series]))
                            return false;
                    } else if (instant) {
                        state->hover_y[series] = target_y[series];
                        state->hover_vy[series] = 0;
                    } else
                        cui_chart_hover_spring(&state->hover_y[series], &state->hover_vy[series],
                                               target_y[series], dt, duration);
            }
            bool unsettled = state->hover_progress < 1;
            if (active) {
                unsettled |= fabsf(state->hover_x - target_x) > .1f || fabsf(state->hover_vx) > .1f;
                for (size_t series = 0; series <= o->extra_series_count; series++)
                    unsettled |= fabsf(state->hover_y[series] - target_y[series]) > .1f ||
                                 fabsf(state->hover_vy[series]) > .1f;
            }
            if (state->hover_progress > 0 && !instant && !o->custom_motion &&
                (clock->ui || unsettled))
                chart_request(clock);
            if (state->hover_progress == 0) {
                state->hover_visual_started = false;
                if (o->custom_motion)
                    memset(state->motion_line, 0, sizeof state->motion_line);
            }
        }
    }
    return true;
}
static cui_color chart_alpha(cui_color c, float opacity) {
    c.a = (unsigned char)lroundf(c.a * fmaxf(0, fminf(1, opacity)));
    return c;
}
static bool chart_emit(cui_prepaint_writer *writer, cui_draw_command command, bool deferred) {
    return cui_prepaint_emit(writer, &command, deferred);
}
static bool chart_tooltip_style_valid(const cui_chart_tooltip_style *s) {
    if (!s)
        return true;
    if (s->appearance < CUI_CHART_TOOLTIP_BOXED || s->appearance > CUI_CHART_TOOLTIP_BARE ||
        s->font_size <= 0 || s->line_height <= 0)
        return false;
    if (s->shadow_count > CUI_DROP_SHADOW_MAX)
        return false;
    for (size_t i = 0; i < s->shadow_count; i++) {
        cui_rect ignored;
        if (!cui_drop_shadow_bounds(
                (cui_drop_shadow_paint){.caster = {0, 0, 1, 1}, .layer = s->shadows[i]}, &ignored))
            return false;
    }
    const float dimensions[] = {s->font_size,   s->line_height, s->padding,         s->min_width,
                                s->cursor_gap,  s->row_gap,     s->label_value_gap, s->swatch_gap,
                                s->swatch_size, s->radius,      s->swatch_radius};
    for (size_t i = 0; i < sizeof dimensions / sizeof dimensions[0]; i++)
        if (!isfinite(dimensions[i]) || dimensions[i] < 0 || dimensions[i] > 16384)
            return false;
    return 2 * s->padding + s->line_height <= 16384 &&
           2 * s->padding + s->swatch_size + s->swatch_gap + s->label_value_gap <= 16384;
}
bool cui_chart_tooltip_style_project(const cui_theme *theme, float rem_size,
                                     float inherited_line_height, cui_chart_tooltip_style *out) {
    if (!theme || !out || !isfinite(rem_size) || rem_size <= 0 || rem_size > 16384)
        return false;
    cui_chart_tooltip_style next = {.font_size = .75f * rem_size,
                                    .line_height = inherited_line_height,
                                    .padding = .5f * rem_size,
                                    .min_width = 150,
                                    .cursor_gap = 8,
                                    .row_gap = .25f * rem_size,
                                    .label_value_gap = .75f * rem_size,
                                    .swatch_gap = .375f * rem_size,
                                    .swatch_size = .5f * rem_size,
                                    .radius = theme->radius,
                                    .swatch_radius = theme->radius / 2,
                                    .background = theme->popover,
                                    .foreground = theme->popover_foreground,
                                    .muted_foreground = theme->muted,
                                    .ring_color = theme->text};
    next.ring_color.a = (unsigned char)lroundf(255 * .1f);
    next.shadow_count = 3;
    if (!cui_theme_popover_shadows(theme, 1, next.shadows))
        return false;
    if (!chart_tooltip_style_valid(&next))
        return false;
    *out = next;
    return true;
}
struct cui_chart_tooltip_builder {
    bool failed, has_title;
    cui_str title;
    cui_chart_tooltip_row rows[32];
    size_t count, used;
    char bytes[4096];
};
typedef struct {
    float width, height, label_offset, row_height;
    float value_width[32], label_height[32], value_height[32], title_height;
    bool swatched;
} chart_tooltip_layout;
static bool chart_tooltip_string_valid(cui_str s) {
    if ((!s.data && s.length) || s.length > 1024)
        return false;
    for (size_t i = 0; i < s.length;) {
        unsigned char c = (unsigned char)s.data[i++];
        if (c < 0x80)
            continue;
        size_t tail = c >= 0xc2 && c <= 0xdf   ? 1
                      : c >= 0xe0 && c <= 0xef ? 2
                      : c >= 0xf0 && c <= 0xf4 ? 3
                                               : 0;
        if (!tail || tail > s.length - i)
            return false;
        unsigned char next = (unsigned char)s.data[i];
        if ((c == 0xe0 && next < 0xa0) || (c == 0xed && next > 0x9f) ||
            (c == 0xf0 && next < 0x90) || (c == 0xf4 && next > 0x8f))
            return false;
        while (tail--)
            if (((unsigned char)s.data[i++] & 0xc0) != 0x80)
                return false;
    }
    return true;
}
static cui_str chart_tooltip_copy(cui_chart_tooltip_builder *b, cui_str s) {
    cui_str copy = {b->bytes + b->used, s.length};
    if (s.length)
        memcpy(b->bytes + b->used, s.data, s.length);
    b->used += s.length;
    return copy;
}
bool cui_chart_tooltip_set_title(cui_chart_tooltip_builder *b, cui_str title) {
    if (!b)
        return false;
    if (b->failed || !chart_tooltip_string_valid(title) ||
        title.length > sizeof b->bytes - b->used) {
        b->failed = true;
        return false;
    }
    b->title = chart_tooltip_copy(b, title);
    b->has_title = true;
    return true;
}
bool cui_chart_tooltip_add_row(cui_chart_tooltip_builder *b, const cui_chart_tooltip_row *row) {
    if (!b)
        return false;
    if (b->failed || !row || b->count == 32 || !chart_tooltip_string_valid(row->label) ||
        !chart_tooltip_string_valid(row->value) ||
        row->label.length + row->value.length > sizeof b->bytes - b->used) {
        b->failed = true;
        return false;
    }
    cui_chart_tooltip_row copy = *row;
    copy.label = chart_tooltip_copy(b, row->label);
    copy.value = chart_tooltip_copy(b, row->value);
    b->rows[b->count++] = copy;
    return true;
}
static bool chart_tooltip_build(const cui_chart_state *state, const cui_chart_options *o,
                                cui_str name, cui_chart_tooltip_builder *builder) {
    size_t index = state->tooltip_visual_index;
    const cui_chart_datum *d = &o->data[index];
    cui_chart_tooltip_info info = {
        .kind = o->kind,
        .index = index,
        .datum = d,
        .value_count = o->kind == CUI_CHART_CANDLESTICK ? 4 : o->extra_series_count + 1};
    info.fallback_title = chart_tooltip_fallback_title(o, d);
    info.has_fallback_title = info.fallback_title.data != NULL;
    for (size_t i = 0; i < info.value_count; i++) {
        info.values[i] = o->kind == CUI_CHART_CANDLESTICK ? (i == 0   ? d->open
                                                             : i == 1 ? d->high
                                                             : i == 2 ? d->low
                                                                      : d->close)
                         : o->kind == CUI_CHART_SANKEY    ? state->tooltip_visual_value
                         : i                              ? o->extra_series[i - 1].values[index]
                                                          : d->value;
        if (!isfinite(info.values[i]))
            return false;
        if (o->kind == CUI_CHART_CANDLESTICK) {
            static const cui_str labels[] = {{"Open", 4}, {"High", 4}, {"Low", 3}, {"Close", 5}};
            info.series_labels[i] = labels[i];
            info.series_colors[i] = d->close > d->open ? o->bullish : o->bearish;
        } else if (o->kind == CUI_CHART_PIE || o->kind == CUI_CHART_SANKEY) {
            info.series_labels[i] = o->kind == CUI_CHART_PIE ? name : (cui_str){0};
            info.series_colors[i] = d->color.a ? d->color : o->series[index % 8];
        } else if (o->kind == CUI_CHART_BAR) {
            info.series_labels[i] = name;
            info.series_colors[i] = o->bar_gradient_start.a ? o->bar_gradient_start
                                    : d->color.a            ? d->color
                                                            : o->series[0];
        } else {
            info.series_labels[i] = i ? o->extra_series[i - 1].name : name;
            info.series_colors[i] =
                i && o->extra_series[i - 1].color.a ? o->extra_series[i - 1].color : o->series[i];
        }
        if (!info.series_colors[i].a)
            info.series_colors[i] = o->foreground;
    }
    return o->tooltip_build(&info, builder, o->tooltip_build_user) && !builder->failed;
}
static bool chart_tooltip_collect(const cui_chart_state *state, const cui_chart_options *o,
                                  cui_str name, cui_chart_tooltip_builder *builder) {
    if (o->tooltip_build)
        return chart_tooltip_build(state, o, name, builder);
    size_t tooltip_index = state->tooltip_visual_index;
    const cui_chart_datum *datum = &o->data[tooltip_index];
    cui_str title = o->tooltip_title ? o->tooltip_title(datum, tooltip_index, o->tooltip_user)
                                     : chart_tooltip_fallback_title(o, datum);
    if ((o->tooltip_title || title.data) && !cui_chart_tooltip_set_title(builder, title))
        return false;
    char row_text[8][52] = {{0}};
    size_t row_length[8] = {0}, row_count = 0;
    cui_color row_color[8] = {{0}}, row_swatch[8] = {{0}};
    cui_str row_label[8] = {{0}};
    size_t rows = o->kind == CUI_CHART_CANDLESTICK ? 4 : o->extra_series_count + 1;
    for (size_t series = 0; series < rows; series++) {
        double number = o->kind == CUI_CHART_SANKEY ? state->tooltip_visual_value : datum->value;
        if (o->kind != CUI_CHART_CANDLESTICK && series)
            number = o->extra_series[series - 1].values[tooltip_index];
        if (o->kind == CUI_CHART_CANDLESTICK)
            number = series == 0   ? datum->open
                     : series == 1 ? datum->high
                     : series == 2 ? datum->low
                                   : datum->close;
        char value[48];
        cui_str formatted;
        double pie_share = 0;
        if (o->kind == CUI_CHART_PIE &&
            (o->pie_tooltip_value || (!o->tooltip_value && !o->tooltip_format))) {
            long double total = 0;
            for (size_t part = 0; part < o->count; part++)
                total += fmax(0, o->data[part].value);
            pie_share = total > 0 ? (double)((long double)number / total * 100) : 0;
        }
        if (o->kind == CUI_CHART_PIE && o->pie_tooltip_value) {
            size_t written = o->pie_tooltip_value(datum, tooltip_index, number, pie_share, value,
                                                  sizeof value, o->tooltip_user);
            formatted = (cui_str){value, written < sizeof value ? written : sizeof value - 1};
        } else if (o->tooltip_value) {
            size_t written = o->tooltip_value(datum, tooltip_index, series, number, value,
                                              sizeof value, o->tooltip_user);
            formatted = (cui_str){value, written < sizeof value ? written : sizeof value - 1};
        } else if (o->kind == CUI_CHART_PIE && !o->tooltip_format) {
            char raw_value[32];
            cui_str raw = cui_chart_number(raw_value, sizeof raw_value, number, NULL, NULL);
            int written = snprintf(value, sizeof value, "%.*s (%.1f%%)", (int)raw.length, raw.data,
                                   pie_share);
            formatted = (cui_str){value, written <= 0                     ? 0
                                         : (size_t)written < sizeof value ? (size_t)written
                                                                          : sizeof value - 1};
        } else
            formatted = cui_chart_number(value, sizeof value, number,
                                         o->tooltip_format             ? o->tooltip_format
                                         : o->kind == CUI_CHART_SANKEY ? o->sankey_value_format
                                                                       : NULL,
                                         o->format_user);
        memcpy(row_text[row_count], formatted.data, formatted.length);
        row_length[row_count] = formatted.length;
        if (o->kind == CUI_CHART_CANDLESTICK) {
            static const cui_str candle_labels[] = {
                {"Open", 4}, {"High", 4}, {"Low", 3}, {"Close", 5}};
            row_label[row_count] = candle_labels[series];
            row_swatch[row_count] = datum->close > datum->open ? o->bullish : o->bearish;
        } else if (o->kind == CUI_CHART_PIE) {
            row_label[row_count] = name;
            row_swatch[row_count] = datum->color.a ? datum->color : o->series[tooltip_index % 8];
        } else if (o->kind == CUI_CHART_SANKEY) {
            row_swatch[row_count] = datum->color.a ? datum->color : o->series[tooltip_index % 8];
        } else if (o->kind == CUI_CHART_BAR) {
            row_label[row_count] = name;
            row_swatch[row_count] = o->bar_gradient_start.a ? o->bar_gradient_start
                                    : datum->color.a        ? datum->color
                                                            : o->series[0];
        } else {
            row_label[row_count] = series ? o->extra_series[series - 1].name : name;
            row_swatch[row_count] = series && o->extra_series[series - 1].color.a
                                        ? o->extra_series[series - 1].color
                                        : o->series[series % 8];
        }
        if (o->tooltip_label)
            row_label[row_count] = o->tooltip_label(datum, tooltip_index, series, o->tooltip_user);
        if (row_label[row_count].length && !row_label[row_count].data)
            row_label[row_count] = (cui_str){0};
        row_color[row_count] = o->tooltip_style ? o->tooltip_style->foreground : o->foreground;
        if (o->tooltip_value_color)
            row_color[row_count] =
                o->tooltip_value_color(datum, tooltip_index, series, number, o->tooltip_user);
        if (!row_swatch[row_count].a)
            row_swatch[row_count] = o->foreground;
        cui_chart_tooltip_row row = {.has_swatch = true,
                                     .swatch = row_swatch[row_count],
                                     .has_value_color = true,
                                     .value_color = row_color[row_count],
                                     .label = row_label[row_count],
                                     .value = {row_text[row_count], row_length[row_count]}};
        if (!cui_chart_tooltip_add_row(builder, &row))
            return false;
        row_count++;
    }
    return !builder->failed;
}
typedef cui_size (*chart_tooltip_measure_fn)(void *, cui_str, float, cui_text_style);
static cui_size chart_tooltip_measure_ui(void *user, cui_str text, float font,
                                         cui_text_style style) {
    return cui_measure_text_ex(user, text, font, style);
}
static cui_size chart_tooltip_measure_paint(void *user, cui_str text, float font,
                                            cui_text_style style) {
    return cui_prepaint_measure(user, text, font, style);
}
static bool chart_tooltip_measure(const cui_chart_tooltip_builder *b,
                                  chart_tooltip_measure_fn measure, void *user,
                                  chart_tooltip_layout *layout,
                                  const cui_chart_tooltip_style *style, float plot_width,
                                  float plot_height) {
    if (style) {
        bool bare = style->appearance == CUI_CHART_TOOLTIP_BARE;
        float pad = bare ? 0 : style->padding;
        *layout = (chart_tooltip_layout){.width = bare ? 0 : fmaxf(style->min_width, 2 * pad)};
        for (size_t i = 0; i < b->count; i++)
            layout->swatched |= b->rows[i].has_swatch;
        layout->label_offset =
            pad + (layout->swatched ? style->swatch_size + style->swatch_gap : 0);
        layout->row_height = fmaxf(style->line_height, layout->swatched ? style->swatch_size : 0);
        if (b->has_title) {
            cui_size title = measure(user, b->title, style->font_size,
                                     (cui_text_style){.weight = CUI_TEXT_WEIGHT_SEMIBOLD});
            if (!isfinite(title.width) || !isfinite(title.height) || title.width < 0 ||
                title.height < 0)
                return false;
            layout->title_height = title.height;
            layout->width = fmaxf(layout->width, title.width + 2 * pad);
        }
        for (size_t i = 0; i < b->count; i++) {
            cui_size label = measure(user, b->rows[i].label, style->font_size, (cui_text_style){0});
            cui_size value = measure(user, b->rows[i].value, style->font_size, (cui_text_style){0});
            if (!isfinite(label.width) || !isfinite(label.height) || label.width < 0 ||
                label.height < 0 || !isfinite(value.width) || !isfinite(value.height) ||
                value.width < 0 || value.height < 0)
                return false;
            layout->value_width[i] = value.width;
            layout->label_height[i] = label.height;
            layout->value_height[i] = value.height;
            layout->width = fmaxf(layout->width, layout->label_offset + label.width +
                                                     style->label_value_gap + value.width + pad);
        }
        size_t entries = b->count + (b->has_title ? 1 : 0);
        layout->height = 2 * pad + (b->has_title ? style->line_height : 0) +
                         (float)b->count * layout->row_height +
                         (entries ? (float)(entries - 1) * style->row_gap : 0);
        if (!isfinite(layout->width) || !isfinite(layout->height) || layout->width > 16384 ||
            layout->height > 16384 || (bare && layout->width > plot_width))
            return false;
        if (bare) {
            layout->width = plot_width;
            layout->height = plot_height;
        }
        return true;
    }
    *layout = (chart_tooltip_layout){.width = 150,
                                     .height = 16 + (b->has_title ? 18 : 0) + 16 * (float)b->count};
    for (size_t i = 0; i < b->count; i++)
        layout->swatched |= b->rows[i].has_swatch;
    layout->label_offset = layout->swatched ? 22 : 8;
    if (b->has_title) {
        cui_size size =
            measure(user, b->title, 11, (cui_text_style){.weight = CUI_TEXT_WEIGHT_SEMIBOLD});
        if (!isfinite(size.width) || !isfinite(size.height) || size.width < 0 || size.height < 0)
            return false;
        layout->width = fmaxf(layout->width, size.width + 16);
    }
    for (size_t i = 0; i < b->count; i++) {
        cui_size label = measure(user, b->rows[i].label, 11, (cui_text_style){0});
        cui_size value = measure(user, b->rows[i].value, 11, (cui_text_style){0});
        if (!isfinite(label.width) || !isfinite(label.height) || label.width < 0 ||
            label.height < 0 || !isfinite(value.width) || !isfinite(value.height) ||
            value.width < 0 || value.height < 0)
            return false;
        layout->value_width[i] = value.width;
        layout->width =
            fmaxf(layout->width, layout->label_offset + label.width + 12 + value.width + 8);
    }
    return layout->width <= 16384;
}
static cui_rect chart_tooltip_placement(cui_rect bounds, const cui_chart_state *state,
                                        const cui_chart_tooltip_style *style,
                                        const chart_tooltip_layout *layout) {
    if (style->appearance == CUI_CHART_TOOLTIP_BARE)
        return bounds;
    float cx = state->tooltip_cursor_valid ? state->tooltip_cursor_x : 0;
    float cy = state->tooltip_cursor_valid ? state->tooltip_cursor_y : 0;
    return (cui_rect){bounds.x + (cx < bounds.w / 2 ? cx + style->cursor_gap
                                                    : cx - style->cursor_gap - layout->width),
                      bounds.y + (cy < bounds.h / 2 ? cy + style->cursor_gap
                                                    : cy - style->cursor_gap - layout->height),
                      layout->width, layout->height};
}
/* Retain ring_color as the override for the canonical source ring. Custom
 * layers with other geometry use their own colors; count0 uses legacy ring. */
static cui_drop_shadow chart_tooltip_shadow(const cui_chart_tooltip_style *style, size_t index) {
    cui_drop_shadow layer = style->shadow_count
                                ? style->shadows[index]
                                : (cui_drop_shadow){.spread = 1, .color = style->ring_color};
    if (layer.sigma == 0 && layer.spread == 1 && layer.offset_x == 0 && layer.offset_y == 0)
        layer.color = style->ring_color;
    return layer;
}
static bool chart_tooltip_emit_styled(const cui_prepaint_info *info, cui_prepaint_writer *writer,
                                      const cui_chart_state *state, const cui_chart_options *o,
                                      const cui_chart_tooltip_builder *b,
                                      const chart_tooltip_layout *layout) {
    const cui_chart_tooltip_style *style = o->tooltip_style;
    bool bare = style->appearance == CUI_CHART_TOOLTIP_BARE;
    float pad = bare ? 0 : style->padding;
    cui_rect rect = chart_tooltip_placement(info->bounds, state, style, layout);
    float alpha = state->tooltip_focus;
    if (!bare) {
        size_t count = style->shadow_count ? style->shadow_count : 1;
        for (size_t i = 0; i < count; i++) {
            cui_drop_shadow layer = chart_tooltip_shadow(style, i);
            layer.color = chart_alpha(layer.color, alpha);
            cui_drop_shadow_paint shadow = {
                .caster = rect,
                .radii = {style->radius, style->radius, style->radius, style->radius},
                .layer = layer};
            if (!cui_prepaint_drop_shadow(writer, &shadow, true))
                return false;
        }
    }
    if (!bare && !chart_emit(writer,
                             (cui_draw_command){.type = CUI_DRAW_RECT,
                                                .bounds = rect,
                                                .radius = style->radius,
                                                .color = chart_alpha(style->background, alpha)},
                             true))
        return false;
    float row_y = rect.y + pad;
    if (b->has_title) {
        if (!chart_emit(writer,
                        (cui_draw_command){
                            .type = CUI_DRAW_TEXT,
                            .bounds = {rect.x + pad,
                                       row_y + (style->line_height - layout->title_height) / 2,
                                       rect.w - 2 * pad, style->line_height},
                            .font_size = style->font_size,
                            .text_style = {.weight = CUI_TEXT_WEIGHT_SEMIBOLD},
                            .text = b->title,
                            .color = chart_alpha(style->foreground, alpha)},
                        true))
            return false;
        row_y += style->line_height + style->row_gap;
    }
    for (size_t i = 0; i < b->count; i++) {
        const cui_chart_tooltip_row *row = &b->rows[i];
        float text_y = row_y + (layout->row_height - style->line_height) / 2;
        if (row->has_swatch &&
            !chart_emit(
                writer,
                (cui_draw_command){.type = CUI_DRAW_RECT,
                                   .bounds = {rect.x + pad,
                                              row_y + (layout->row_height - style->swatch_size) / 2,
                                              style->swatch_size, style->swatch_size},
                                   .radius = style->swatch_radius,
                                   .color = chart_alpha(row->swatch, alpha)},
                true))
            return false;
        if (!chart_emit(writer,
                        (cui_draw_command){
                            .type = CUI_DRAW_TEXT,
                            .bounds = {rect.x + layout->label_offset,
                                       text_y + (style->line_height - layout->label_height[i]) / 2,
                                       rect.w - layout->label_offset - pad, style->line_height},
                            .font_size = style->font_size,
                            .text = row->label,
                            .color = chart_alpha(style->muted_foreground, alpha)},
                        true) ||
            !chart_emit(
                writer,
                (cui_draw_command){
                    .type = CUI_DRAW_TEXT,
                    .bounds = {rect.x + rect.w - pad - layout->value_width[i],
                               text_y + (style->line_height - layout->value_height[i]) / 2,
                               layout->value_width[i], style->line_height},
                    .font_size = style->font_size,
                    .text = row->value,
                    .color = chart_alpha(
                        row->has_value_color ? row->value_color : style->foreground, alpha)},
                true))
            return false;
        row_y += layout->row_height + style->row_gap;
    }
    return true;
}
static void chart_tooltip_text_box(cui_context *ui, cui_id id, cui_rect rect, cui_str text,
                                   cui_color color, const cui_chart_tooltip_style *style,
                                   bool title) {
    cui_begin_absolute_box(
        ui, id,
        (cui_box){.width = rect.w, .height = rect.h, .fixed_width = true, .fixed_height = true},
        rect);
    cui_text_ex(
        ui, text, style->font_size, color, style->line_height,
        (cui_text_style){.weight = title ? CUI_TEXT_WEIGHT_SEMIBOLD : CUI_TEXT_WEIGHT_NORMAL});
    cui_end_box(ui);
}
static void chart_tooltip_compose_styled(cui_context *ui, cui_id id, const cui_chart_state *state,
                                         const cui_chart_options *o, float w, float h,
                                         const cui_chart_tooltip_builder *b,
                                         const chart_tooltip_layout *layout) {
    const cui_chart_tooltip_style *style = o->tooltip_style;
    bool bare = style->appearance == CUI_CHART_TOOLTIP_BARE;
    float pad = bare ? 0 : style->padding;
    cui_rect rect = chart_tooltip_placement((cui_rect){state->bounds.x, state->bounds.y, w, h},
                                            state, style, layout);
    cui_begin_portal(ui, id ^ UINT64_C(0xd8a736411cb50000),
                     (cui_box){.width = rect.w,
                               .height = rect.h,
                               .fixed_width = true,
                               .fixed_height = true,
                               .radius = bare ? 0 : style->radius,
                               .background = bare ? (cui_color){0} : style->background,
                               .custom_opacity = true,
                               .opacity = state->tooltip_focus},
                     rect, 120);
    if (!bare) {
        cui_drop_shadow layers[CUI_DROP_SHADOW_MAX];
        size_t count = style->shadow_count ? style->shadow_count : 1;
        for (size_t i = 0; i < count; i++)
            layers[i] = chart_tooltip_shadow(style, i);
        if (!cui_set_drop_shadows(
                ui, layers, count,
                (cui_corner_radii){style->radius, style->radius, style->radius, style->radius})) {
            cui_end_box(ui);
            return;
        }
    }
    if (o->tooltip_content) {
        cui_begin_absolute_box(ui, id ^ UINT64_C(0x6c712ab004e9b669),
                               (cui_box){.width = rect.w - 2 * pad,
                                         .height = rect.h - 2 * pad,
                                         .fixed_width = true,
                                         .fixed_height = true},
                               (cui_rect){pad, pad, rect.w - 2 * pad, rect.h - 2 * pad});
        o->tooltip_content(ui, id, state->tooltip_visual_index,
                           &o->data[state->tooltip_visual_index], o->tooltip_user);
        cui_end_box(ui);
        cui_end_box(ui);
        return;
    }
    float row_y = pad;
    if (b->has_title) {
        chart_tooltip_text_box(ui, id ^ UINT64_C(0x31ded050691f0000),
                               (cui_rect){pad, row_y, rect.w - 2 * pad, style->line_height},
                               b->title, style->foreground, style, true);
        row_y += style->line_height + style->row_gap;
    }
    for (size_t i = 0; i < b->count; i++) {
        const cui_chart_tooltip_row *row = &b->rows[i];
        float text_y = row_y + (layout->row_height - style->line_height) / 2;
        if (row->has_swatch) {
            cui_begin_absolute_box(ui, id ^ (UINT64_C(0x44a55e948c7f0000) + i),
                                   (cui_box){.width = style->swatch_size,
                                             .height = style->swatch_size,
                                             .fixed_width = true,
                                             .fixed_height = true,
                                             .radius = style->swatch_radius,
                                             .background = row->swatch},
                                   (cui_rect){pad,
                                              row_y + (layout->row_height - style->swatch_size) / 2,
                                              style->swatch_size, style->swatch_size});
            cui_end_box(ui);
        }
        chart_tooltip_text_box(ui, id ^ (UINT64_C(0xb48d186a501e6b77) + i),
                               (cui_rect){layout->label_offset, text_y,
                                          rect.w - layout->label_offset - pad, style->line_height},
                               row->label, style->muted_foreground, style, false);
        chart_tooltip_text_box(
            ui, id ^ (UINT64_C(0x23d40cb791a60000) + i),
            (cui_rect){rect.w - pad - layout->value_width[i], text_y, layout->value_width[i],
                       style->line_height},
            row->value, row->has_value_color ? row->value_color : style->foreground, style, false);
        row_y += layout->row_height + style->row_gap;
    }
    cui_end_box(ui);
}
static bool chart_tooltip_emit(const cui_prepaint_info *info, cui_prepaint_writer *writer,
                               const cui_chart_state *state, const cui_chart_options *o,
                               const cui_chart_tooltip_builder *b,
                               const chart_tooltip_layout *layout) {
    if (o->tooltip_style)
        return chart_tooltip_emit_styled(info, writer, state, o, b, layout);
    float cx = state->tooltip_cursor_valid ? state->tooltip_cursor_x : 0;
    float cy = state->tooltip_cursor_valid ? state->tooltip_cursor_y : 0;
    float x = info->bounds.x + (cx < info->bounds.w / 2 ? cx + 8 : cx - 8 - layout->width);
    float y = info->bounds.y + (cy < info->bounds.h / 2 ? cy + 8 : cy - 8 - layout->height);
    float alpha = state->tooltip_focus;
    if (!chart_emit(writer,
                    (cui_draw_command){.type = CUI_DRAW_RECT,
                                       .bounds = {x, y, layout->width, layout->height},
                                       .radius = 4,
                                       .color = chart_alpha(o->background, alpha)},
                    true) ||
        !chart_emit(writer,
                    (cui_draw_command){.type = CUI_DRAW_BORDER,
                                       .bounds = {x, y, layout->width, layout->height},
                                       .radius = 4,
                                       .stroke = 1,
                                       .color = chart_alpha(o->grid, alpha)},
                    true))
        return false;
    float row_y = y + 8;
    if (b->has_title) {
        if (!chart_emit(writer,
                        (cui_draw_command){.type = CUI_DRAW_TEXT,
                                           .bounds = {x + 8, row_y, layout->width - 16, 14},
                                           .font_size = 11,
                                           .text_style = {.weight = CUI_TEXT_WEIGHT_SEMIBOLD},
                                           .text = b->title,
                                           .color = chart_alpha(o->foreground, alpha)},
                        true))
            return false;
        row_y += 18;
    }
    for (size_t i = 0; i < b->count; i++) {
        const cui_chart_tooltip_row *row = &b->rows[i];
        if (row->has_swatch &&
            !chart_emit(writer,
                        (cui_draw_command){.type = CUI_DRAW_RECT,
                                           .bounds = {x + 8, row_y + 3, 8, 8},
                                           .radius = 4,
                                           .color = chart_alpha(row->swatch, alpha)},
                        true))
            return false;
        if (!chart_emit(writer,
                        (cui_draw_command){.type = CUI_DRAW_TEXT,
                                           .bounds = {x + layout->label_offset, row_y,
                                                      layout->width - layout->label_offset - 8, 16},
                                           .font_size = 11,
                                           .text = row->label,
                                           .color = chart_alpha(o->foreground, alpha * .7f)},
                        true) ||
            !chart_emit(writer,
                        (cui_draw_command){
                            .type = CUI_DRAW_TEXT,
                            .bounds = {x + layout->width - 8 - layout->value_width[i], row_y,
                                       layout->value_width[i], 16},
                            .font_size = 11,
                            .text = row->value,
                            .color = chart_alpha(
                                row->has_value_color ? row->value_color : o->foreground, alpha)},
                        true))
            return false;
        row_y += 16;
    }
    return true;
}
static void chart_tooltip_compose(cui_context *ui, cui_id id, const cui_chart_state *state,
                                  const cui_chart_options *o, float w, float h,
                                  const cui_chart_tooltip_builder *b,
                                  const chart_tooltip_layout *layout) {
    if (o->tooltip_style) {
        chart_tooltip_compose_styled(ui, id, state, o, w, h, b, layout);
        return;
    }
    float cx = state->tooltip_cursor_valid ? state->tooltip_cursor_x : 0;
    float cy = state->tooltip_cursor_valid ? state->tooltip_cursor_y : 0;
    float x = cx < w / 2 ? cx + 8 : cx - 8 - layout->width;
    float y = cy < h / 2 ? cy + 8 : cy - 8 - layout->height;
    cui_begin_portal(ui, id ^ UINT64_C(0xd8a736411cb50000),
                     (cui_box){.width = layout->width,
                               .height = layout->height,
                               .fixed_width = true,
                               .fixed_height = true,
                               .radius = 4,
                               .background = o->background,
                               .border = o->grid,
                               .border_width = 1,
                               .custom_opacity = true,
                               .opacity = state->tooltip_focus},
                     (cui_rect){x, y, layout->width, layout->height}, 120);
    float row_y = 8;
    if (b->has_title) {
        cui_begin_absolute_box(ui, id ^ UINT64_C(0x31ded050691f0000),
                               (cui_box){.width = layout->width - 16,
                                         .height = 14,
                                         .fixed_width = true,
                                         .fixed_height = true},
                               (cui_rect){8, row_y, layout->width - 16, 14});
        cui_text_ex(ui, b->title, 11, o->foreground, 0,
                    (cui_text_style){.weight = CUI_TEXT_WEIGHT_SEMIBOLD});
        cui_end_box(ui);
        row_y += 18;
    }
    for (size_t i = 0; i < b->count; i++) {
        const cui_chart_tooltip_row *row = &b->rows[i];
        if (row->has_swatch) {
            cui_begin_absolute_box(ui, id ^ (UINT64_C(0x44a55e948c7f0000) + i),
                                   (cui_box){.width = 8,
                                             .height = 8,
                                             .fixed_width = true,
                                             .fixed_height = true,
                                             .radius = 4,
                                             .background = row->swatch},
                                   (cui_rect){8, row_y + 3, 8, 8});
            cui_end_box(ui);
        }
        cui_begin_absolute_box(
            ui, id ^ (UINT64_C(0xb48d186a501e6b77) + i),
            (cui_box){.width = layout->width - layout->label_offset - 8,
                      .height = 16,
                      .fixed_width = true,
                      .fixed_height = true},
            (cui_rect){layout->label_offset, row_y, layout->width - layout->label_offset - 8, 16});
        cui_color muted = o->foreground;
        muted.a = (unsigned char)lroundf(muted.a * .7f);
        cui_text(ui, row->label, 11, muted);
        cui_end_box(ui);
        cui_begin_absolute_box(ui, id ^ (UINT64_C(0x23d40cb791a60000) + i),
                               (cui_box){.width = layout->value_width[i],
                                         .height = 16,
                                         .fixed_width = true,
                                         .fixed_height = true},
                               (cui_rect){layout->width - 8 - layout->value_width[i], row_y,
                                          layout->value_width[i], 16});
        cui_text(ui, row->value, 11, row->has_value_color ? row->value_color : o->foreground);
        cui_end_box(ui);
        row_y += 16;
    }
    cui_end_box(ui);
}
static bool chart_paint_tooltip(const cui_prepaint_info *info, cui_prepaint_writer *writer,
                                const cui_chart_state *state, const cui_chart_options *o,
                                cui_str name) {
    if (o->noninteractive || state->tooltip_visual_index >= o->count || state->tooltip_focus <= 0 ||
        !info->modal_allowed || info->inert)
        return true;
    if (o->tooltip_build || o->tooltip_style) {
        cui_chart_tooltip_builder builder = {0};
        chart_tooltip_layout layout;
        return chart_tooltip_collect(state, o, name, &builder) &&
               chart_tooltip_measure(&builder, chart_tooltip_measure_paint, writer, &layout,
                                     o->tooltip_style, info->bounds.w, info->bounds.h) &&
               chart_tooltip_emit(info, writer, state, o, &builder, &layout);
    }
    float w = info->bounds.w, h = info->bounds.h;
    size_t tooltip_index = state->tooltip_visual_index;
    const cui_chart_datum *datum = &o->data[tooltip_index];
    cui_str title = {0};
    char row_text[8][52] = {{0}};
    size_t row_length[8] = {0}, row_count = 0;
    cui_color row_color[8] = {{0}};
    cui_str row_label[8] = {{0}};
    cui_color row_swatch[8] = {{0}};
    float tooltip_width = 150;
    if (!o->tooltip_content) {
        title = o->tooltip_title ? o->tooltip_title(datum, tooltip_index, o->tooltip_user)
                                 : chart_tooltip_fallback_title(o, datum);
        if (title.data && title.length)
            tooltip_width =
                fmaxf(tooltip_width,
                      cui_prepaint_measure(writer, title, 11,
                                           (cui_text_style){.weight = CUI_TEXT_WEIGHT_SEMIBOLD})
                              .width +
                          16);
        size_t rows = o->kind == CUI_CHART_CANDLESTICK ? 4 : o->extra_series_count + 1;
        for (size_t series = 0; series < rows; series++) {
            double number =
                o->kind == CUI_CHART_SANKEY ? state->tooltip_visual_value : datum->value;
            if (o->kind != CUI_CHART_CANDLESTICK && series)
                number = o->extra_series[series - 1].values[tooltip_index];
            if (o->kind == CUI_CHART_CANDLESTICK)
                number = series == 0   ? datum->open
                         : series == 1 ? datum->high
                         : series == 2 ? datum->low
                                       : datum->close;
            char value[48];
            cui_str formatted;
            double pie_share = 0;
            if (o->kind == CUI_CHART_PIE &&
                (o->pie_tooltip_value || (!o->tooltip_value && !o->tooltip_format))) {
                long double total = 0;
                for (size_t part = 0; part < o->count; part++)
                    total += fmax(0, o->data[part].value);
                pie_share = total > 0 ? (double)((long double)number / total * 100) : 0;
            }
            if (o->kind == CUI_CHART_PIE && o->pie_tooltip_value) {
                size_t written = o->pie_tooltip_value(datum, tooltip_index, number, pie_share,
                                                      value, sizeof value, o->tooltip_user);
                formatted = (cui_str){value, written < sizeof value ? written : sizeof value - 1};
            } else if (o->tooltip_value) {
                size_t written = o->tooltip_value(datum, tooltip_index, series, number, value,
                                                  sizeof value, o->tooltip_user);
                formatted = (cui_str){value, written < sizeof value ? written : sizeof value - 1};
            } else if (o->kind == CUI_CHART_PIE && !o->tooltip_format) {
                char raw_value[32];
                cui_str raw = cui_chart_number(raw_value, sizeof raw_value, number, NULL, NULL);
                int written = snprintf(value, sizeof value, "%.*s (%.1f%%)", (int)raw.length,
                                       raw.data, pie_share);
                formatted = (cui_str){value, written <= 0                     ? 0
                                             : (size_t)written < sizeof value ? (size_t)written
                                                                              : sizeof value - 1};
            } else
                formatted = cui_chart_number(value, sizeof value, number,
                                             o->tooltip_format             ? o->tooltip_format
                                             : o->kind == CUI_CHART_SANKEY ? o->sankey_value_format
                                                                           : NULL,
                                             o->format_user);
            memcpy(row_text[row_count], formatted.data, formatted.length);
            row_length[row_count] = formatted.length;
            if (o->kind == CUI_CHART_CANDLESTICK) {
                static const cui_str candle_labels[] = {
                    {"Open", 4}, {"High", 4}, {"Low", 3}, {"Close", 5}};
                row_label[row_count] = candle_labels[series];
                row_swatch[row_count] = datum->close > datum->open ? o->bullish : o->bearish;
            } else if (o->kind == CUI_CHART_PIE) {
                row_label[row_count] = name;
                row_swatch[row_count] =
                    datum->color.a ? datum->color : o->series[tooltip_index % 8];
            } else if (o->kind == CUI_CHART_SANKEY) {
                row_swatch[row_count] =
                    datum->color.a ? datum->color : o->series[tooltip_index % 8];
            } else if (o->kind == CUI_CHART_BAR) {
                row_label[row_count] = name;
                row_swatch[row_count] = o->bar_gradient_start.a ? o->bar_gradient_start
                                        : datum->color.a        ? datum->color
                                                                : o->series[0];
            } else {
                row_label[row_count] = series ? o->extra_series[series - 1].name : name;
                row_swatch[row_count] = series && o->extra_series[series - 1].color.a
                                            ? o->extra_series[series - 1].color
                                            : o->series[series % 8];
            }
            if (o->tooltip_label)
                row_label[row_count] =
                    o->tooltip_label(datum, tooltip_index, series, o->tooltip_user);
            if (row_label[row_count].length && !row_label[row_count].data)
                row_label[row_count] = (cui_str){0};
            row_color[row_count] = o->foreground;
            if (o->tooltip_value_color)
                row_color[row_count] =
                    o->tooltip_value_color(datum, tooltip_index, series, number, o->tooltip_user);
            if (!row_swatch[row_count].a)
                row_swatch[row_count] = o->foreground;
            float label_width =
                cui_prepaint_measure(writer, row_label[row_count], 11, (cui_text_style){0}).width;
            float value_width =
                cui_prepaint_measure(writer, (cui_str){row_text[row_count], row_length[row_count]},
                                     11, (cui_text_style){0})
                    .width;
            tooltip_width = fmaxf(tooltip_width, 16 + 8 + 6 + label_width + 12 + value_width);
            row_count++;
        }
    }
    float tooltip_height = o->tooltip_content
                               ? 64
                               : 16 + (title.length && title.data ? 18 : 0) + 16 * (float)row_count;
    float cx = state->tooltip_cursor_valid ? state->tooltip_cursor_x : 0;
    float cy = state->tooltip_cursor_valid ? state->tooltip_cursor_y : 0;
    float x = info->bounds.x + (cx < w / 2 ? cx + 8 : cx - 8 - tooltip_width);
    float y = info->bounds.y + (cy < h / 2 ? cy + 8 : cy - 8 - tooltip_height);
    float alpha = state->tooltip_focus;
    if (!chart_emit(writer,
                    (cui_draw_command){.type = CUI_DRAW_RECT,
                                       .bounds = {x, y, tooltip_width, tooltip_height},
                                       .radius = 4,
                                       .color = chart_alpha(o->background, alpha)},
                    true) ||
        !chart_emit(writer,
                    (cui_draw_command){.type = CUI_DRAW_BORDER,
                                       .bounds = {x, y, tooltip_width, tooltip_height},
                                       .radius = 4,
                                       .stroke = 1,
                                       .color = chart_alpha(o->grid, alpha)},
                    true))
        return false;
    float row_y = y + 8;
    if (title.length && title.data) {
        if (!chart_emit(writer,
                        (cui_draw_command){.type = CUI_DRAW_TEXT,
                                           .bounds = {x + 8, row_y, tooltip_width - 16, 14},
                                           .font_size = 11,
                                           .text_style = {.weight = CUI_TEXT_WEIGHT_SEMIBOLD},
                                           .text = title,
                                           .color = chart_alpha(o->foreground, alpha)},
                        true))
            return false;
        row_y += 18;
    }
    for (size_t i = 0; i < row_count; i++) {
        cui_str text = {row_text[i], row_length[i]};
        float vw = cui_prepaint_measure(writer, text, 11, (cui_text_style){0}).width;
        if (!chart_emit(writer,
                        (cui_draw_command){.type = CUI_DRAW_RECT,
                                           .bounds = {x + 8, row_y + 3, 8, 8},
                                           .radius = 4,
                                           .color = chart_alpha(row_swatch[i], alpha)},
                        true) ||
            !chart_emit(writer,
                        (cui_draw_command){.type = CUI_DRAW_TEXT,
                                           .bounds = {x + 22, row_y, tooltip_width - 30, 16},
                                           .font_size = 11,
                                           .text = row_label[i],
                                           .color = chart_alpha(o->foreground, alpha * .7f)},
                        true) ||
            !chart_emit(writer,
                        (cui_draw_command){.type = CUI_DRAW_TEXT,
                                           .bounds = {x + tooltip_width - 8 - vw, row_y, vw, 16},
                                           .font_size = 11,
                                           .text = text,
                                           .color = chart_alpha(row_color[i], alpha)},
                        true))
            return false;
        row_y += 16;
    }
    return true;
}
static bool chart_paint_dot(cui_prepaint_writer *writer, float x, float y, cui_color dot,
                            cui_color background, float progress) {
    float halo = 20 * progress;
    return chart_emit(writer,
                      (cui_draw_command){.type = CUI_DRAW_RECT,
                                         .bounds = {x - halo / 2, y - halo / 2, halo, halo},
                                         .radius = halo / 2,
                                         .color = chart_alpha(dot, .2f * progress)},
                      false) &&
           chart_emit(writer,
                      (cui_draw_command){.type = CUI_DRAW_RECT,
                                         .bounds = {x - 4, y - 4, 8, 8},
                                         .radius = 4,
                                         .color = chart_alpha(dot, progress)},
                      false) &&
           chart_emit(writer,
                      (cui_draw_command){.type = CUI_DRAW_BORDER,
                                         .bounds = {x - 4, y - 4, 8, 8},
                                         .radius = 4,
                                         .stroke = 1,
                                         .color = chart_alpha(background, progress)},
                      false);
}
typedef struct {
    cui_chart_options options;
    cui_chart_tooltip_style tooltip_style;
    bool has_tooltip_style;
    cui_chart_state *state;
    cui_str name;
    bool reduced;
} chart_prepaint_payload;
static bool chart_prepaint(const cui_prepaint_info *info, const void *data,
                           cui_prepaint_writer *writer) {
    const chart_prepaint_payload *p = data;
    cui_chart_options o = p->options;
    o.tooltip_style = p->has_tooltip_style ? &p->tooltip_style : NULL;
    cui_chart_state next = *p->state;
    if (next.motion_active != o.custom_motion ||
        (o.custom_motion && (next.motion_data != o.data || next.motion_count != o.count ||
                             next.motion_kind != o.kind))) {
        chart_motion_reset(&next);
        next.motion_active = o.custom_motion;
        next.motion_data = o.data;
        next.motion_count = o.count;
        next.motion_kind = o.kind;
    }
    chart_paint_clock clock = {info->frame_time, info->frame_delta, p->reduced, writer, NULL};
    next.bounds = info->bounds;
    o.logical_width = info->bounds.w;
    o.logical_height = info->bounds.h;
    if (!o.reveal_enabled) {
        bool restart = !next.appear_started || next.active_appear_key != o.appear_key;
        double started = restart ? info->frame_time : next.appear_started_at;
        double delay = o.custom_motion ? o.motion.appear.delay : 0;
        double duration = chart_appear_duration(&o);
        double t =
            duration == 0 ? 1 : fmax(0, fmin(1, (info->frame_time - started - delay) / duration));
        if (o.disable_appear || o.reduced_motion || p->reduced)
            t = 1;
        double sampled = 1 - pow(1 - t, 4);
        if (o.custom_motion && cui_plot_motion_sample(&o.motion.appear, t, &sampled) != CUI_PLOT_OK)
            return false;
        cui_chart_options frame = o;
        frame.reveal_enabled = true;
        frame.reveal_progress = (float)fmax(0, fmin(1, sampled));
        if (cui_chart_render(&next, &frame) != CUI_PLOT_OK)
            return false;
        next.appear_started = true;
        next.appear_started_at = started;
        next.active_appear_key = o.appear_key;
        if (t < 1)
            chart_request(&clock);
    }
    (void)cui_chart_update_hover(
        &next, &o,
        info->pointer_valid && info->pointer_visible ? info->pointer_x - info->bounds.x : NAN,
        info->pointer_valid && info->pointer_visible ? info->pointer_y - info->bounds.y : NAN);
    if (!chart_paint_update(&clock, &next, &o, info->bounds.w, info->bounds.h, true))
        return false;
    cui_chart_options frame = o;
    if (!frame.reveal_enabled) {
        frame.reveal_enabled = true;
        frame.reveal_progress = next.rendered_reveal_progress;
    }
    if (cui_chart_render(&next, &frame) != CUI_PLOT_OK)
        return false;
    if (!chart_emit(writer,
                    (cui_draw_command){.type = CUI_DRAW_IMAGE,
                                       .bounds = info->bounds,
                                       .bitmap = {.bitmap = {next.pixels, o.width, o.height,
                                                             (size_t)o.width * 4, next.revision},
                                                  .opacity = 1}},
                    false))
        return false;
    if ((o.kind == CUI_CHART_LINE || o.kind == CUI_CHART_AREA) && next.hover_visual_started &&
        next.tooltip_focus > 0) {
        float bottom = info->bounds.h - chart_point_gutter(&o) * info->bounds.h / o.height;
        cui_color color =
            chart_alpha(o.grid.a ? o.grid : (cui_color){120, 120, 120, 150}, next.tooltip_focus);
        for (float y = 0; y < bottom; y += 6)
            if (!chart_emit(
                    writer,
                    (cui_draw_command){.type = CUI_DRAW_RECT,
                                       .bounds = {info->bounds.x + next.hover_x, info->bounds.y + y,
                                                  1, fminf(4, bottom - y)},
                                       .color = color},
                    false))
                return false;
        for (size_t i = 0; i <= o.extra_series_count; i++) {
            cui_color dot = i ? o.extra_series[i - 1].color : o.series[0];
            if (!dot.a)
                dot = o.series[i].a ? o.series[i] : o.foreground;
            float x = info->bounds.x + next.hover_x, y = info->bounds.y + next.hover_y[i];
            if (!chart_paint_dot(writer, x, y, dot, o.background, next.tooltip_focus))
                return false;
        }
    }
    if ((o.kind == CUI_CHART_BAR || o.kind == CUI_CHART_CANDLESTICK) && next.emphasis_focus > 0 &&
        next.emphasis_visual_index < o.count) {
        float w = info->bounds.w, h = info->bounds.h;
        cui_rect band = {0};
        if (o.kind == CUI_CHART_BAR) {
            cui_plot_rect bar;
            if (cui_chart_bar_bounds(&o, next.emphasis_visual_index, &bar) == CUI_PLOT_OK) {
                bool horizontal =
                    o.bar_alignment == CUI_CHART_BAR_LEFT || o.bar_alignment == CUI_CHART_BAR_RIGHT;
                float thickness = (float)(horizontal ? (bar.y_max - bar.y_min) * h / o.height
                                                     : (bar.x_max - bar.x_min) * w / o.width);
                float left = 0, right = 0;
                if (horizontal)
                    cui_chart_horizontal_bar_extent(&o, &left, &right);
                band = (cui_rect){
                    horizontal ? left * w / o.width
                               : next.emphasis_center * w / o.width - thickness / 2,
                    horizontal ? next.emphasis_center * h / o.height - thickness / 2
                    : o.bar_alignment == CUI_CHART_BAR_TOP && !o.hide_x_axis ? 20 * h / o.height
                                                                             : 0,
                    horizontal ? (right - left) * w / o.width : thickness,
                    horizontal ? thickness : h - (o.hide_x_axis ? 0 : 20 * h / o.height)};
            }
        } else {
            cui_chart_candle_geometry candle;
            if (cui_chart_candle_bounds(&o, next.emphasis_visual_index, &candle) == CUI_PLOT_OK) {
                float ratio = o.candle_body_ratio > 0 ? o.candle_body_ratio : .8f;
                float thickness =
                    (float)((candle.body.x_max - candle.body.x_min) / ratio * w / o.width);
                band = (cui_rect){next.emphasis_center * w / o.width - thickness / 2, 0, thickness,
                                  h - (o.hide_x_axis ? 0 : 20 * h / o.height)};
            }
        }
        band.x += info->bounds.x;
        band.y += info->bounds.y;
        if (band.w > 0 && band.h > 0 &&
            !chart_emit(writer,
                        (cui_draw_command){.type = CUI_DRAW_RECT,
                                           .bounds = band,
                                           .color = chart_alpha(o.foreground.a
                                                                    ? o.foreground
                                                                    : (cui_color){32, 32, 32, 255},
                                                                .08f * next.emphasis_focus)},
                        false))
            return false;
    }
    if (o.kind == CUI_CHART_RADAR && next.tooltip_visual_index < o.count && next.tooltip_focus > 0)
        for (size_t i = 0; i <= o.extra_series_count; i++) {
            float x, y;
            if (cui_chart_radar_vertex(&o, next.tooltip_visual_index, i, &x, &y) != CUI_PLOT_OK)
                continue;
            x = info->bounds.x + x * info->bounds.w / o.width;
            y = info->bounds.y + y * info->bounds.h / o.height;
            cui_color dot = i ? o.extra_series[i - 1].color : o.series[0];
            if (!dot.a)
                dot = o.series[i].a ? o.series[i] : o.foreground;
            if (!chart_paint_dot(writer, x, y, dot, o.background, next.tooltip_focus))
                return false;
        }
    if (!chart_paint_tooltip(info, writer, &next, &o, p->name))
        return false;
    *p->state = next;
    return true;
}
bool cui_chart(cui_context *ui, cui_id id, cui_chart_state *state, const cui_chart_options *o,
               cui_str name) {
    cui_chart_options resolved;
    cui_chart_tooltip_style resolved_style;
    if (o && o->tooltip_build && o->interaction_mode == CUI_PLOT_INTERACTION_AUTO) {
        resolved = *o;
        resolved.interaction_mode = CUI_PLOT_INTERACTION_CURRENT_FRAME;
        o = &resolved;
    }
    if (!ui || !id || !state || !chart_radar_title_valid(o) ||
        !chart_tooltip_style_valid(o ? o->tooltip_style : NULL) ||
        (o->tooltip_build && (o->tooltip_content || o->tooltip_measure)) ||
        o->layout_mode < CUI_CHART_LAYOUT_LEGACY ||
        o->layout_mode > CUI_CHART_LAYOUT_SOURCE_POINT ||
        (o->layout_mode == CUI_CHART_LAYOUT_SOURCE_POINT && o->kind != CUI_CHART_LINE &&
         o->kind != CUI_CHART_AREA) ||
        o->interaction_mode < CUI_PLOT_INTERACTION_AUTO ||
        o->interaction_mode > CUI_PLOT_INTERACTION_LEGACY ||
        (o->interaction_mode == CUI_PLOT_INTERACTION_CURRENT_FRAME &&
         (o->tooltip_content || o->tooltip_measure || o->logical_width > 16384 ||
          o->logical_height > 16384 || name.length > 4096)) ||
        !chart_motion_valid(o) || !isfinite(o->appear_duration) || o->appear_duration < 0 ||
        !isfinite(o->hover_duration) || o->hover_duration < 0 || !name.data || !name.length ||
        !state->pixels || !o->data || !o->count || o->count > 4096 || o->extra_series_count > 7 ||
        (o->extra_series_count && !o->extra_series) || !isfinite(o->logical_width) ||
        (!!o->sankey_label_measure != !!o->sankey_label_paint) || !isfinite(o->logical_height) ||
        o->width > 4096 || o->height > 4096 || !o->width || !o->height ||
        o->point_count > 1000000 || state->byte_capacity < (size_t)o->width * o->height * 4)
        return false;
    if (o->tooltip_style) {
        resolved_style = *o->tooltip_style;
        resolved = *o;
        resolved.tooltip_style = &resolved_style;
        o = &resolved;
    }
    cui_chart_state *published_state = state;
    cui_chart_state next_state;
    bool staged = (o->tooltip_build || o->tooltip_style) &&
                  o->interaction_mode != CUI_PLOT_INTERACTION_CURRENT_FRAME;
    if (staged) {
        next_state = *state;
        state = &next_state;
    }
    cui_chart_tooltip_builder staged_builder = {0};
    chart_tooltip_layout staged_layout;
    bool typed_active = false;
    if (o->interaction_mode != CUI_PLOT_INTERACTION_CURRENT_FRAME &&
        (state->motion_active != o->custom_motion ||
         (o->custom_motion && (state->motion_data != o->data || state->motion_count != o->count ||
                               state->motion_kind != o->kind)))) {
        chart_motion_reset(state);
        state->motion_active = o->custom_motion;
        state->motion_data = o->data;
        state->motion_count = o->count;
        state->motion_kind = o->kind;
    }
    cui_chart_options measured_options = *o;
    if (o->kind == CUI_CHART_SANKEY && !o->custom_sankey_margins && o->sankey_margins.left == 0 &&
        o->sankey_margins.right == 0 && o->sankey_margins.top == 0 &&
        o->sankey_margins.bottom == 0) {
        if (cui_chart_measure_sankey_margins(ui, o, &measured_options.sankey_margins) !=
            CUI_PLOT_OK)
            return false;
        o = &measured_options;
    }
    bool measure_outside_axis =
        o->show_y_axis && !o->y_axis_inside && o->value_axis_gutter == 0 &&
        (o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA ||
         (o->kind == CUI_CHART_BAR && o->bar_alignment != CUI_CHART_BAR_LEFT &&
          o->bar_alignment != CUI_CHART_BAR_RIGHT));
    if (measure_outside_axis) {
        measured_options.value_axis_gutter =
            cui_chart_measure_value_axis_gutter(ui, &measured_options);
        if (measured_options.value_axis_gutter <= 0)
            return false;
        o = &measured_options;
    }
    if (o->kind == CUI_CHART_BAR &&
        (o->bar_alignment == CUI_CHART_BAR_LEFT || o->bar_alignment == CUI_CHART_BAR_RIGHT) &&
        !o->custom_horizontal_bar_gutters) {
        if (cui_chart_measure_horizontal_bar_gutters(ui, o, &measured_options.bar_category_gutter,
                                                     &measured_options.bar_value_end_gutter) !=
            CUI_PLOT_OK)
            return false;
        measured_options.custom_horizontal_bar_gutters = true;
        o = &measured_options;
    }
    bool current = o->interaction_mode == CUI_PLOT_INTERACTION_CURRENT_FRAME;
    chart_prepaint_payload payload = {
        .options = *o, .state = state, .name = name, .reduced = cui_reduced_motion(ui)};
    payload.has_tooltip_style = o->tooltip_style != NULL;
    if (o->tooltip_style)
        payload.tooltip_style = *o->tooltip_style;
    /* Core copies payload bytes; callback rebinds to its copied value. */
    payload.options.tooltip_style = NULL;
    cui_chart_state layout_state;
    cui_chart_options layout_options;
    if (current) {
        /* Reuse Base's complete atomic validation before any UI is submitted.
         * Prepared pixels remain borrowed until final current-frame paint. */
        cui_chart_state checked = *state;
        if (cui_chart_render(&checked, o) != CUI_PLOT_OK)
            return false;
        char *saved_name =
            cui_resource_get(ui, id, UINT64_C(0x79ed53a49b715104), name.length, NULL, NULL);
        if (!saved_name)
            return false;
        memcpy(saved_name, name.data, name.length);
        payload.name = (cui_str){saved_name, name.length};
    }
    if (current) {
        layout_state = *state;
        layout_options = *o;
        layout_options.noninteractive = true;
        layout_options.disable_appear = true;
        layout_options.reveal_enabled = true;
        layout_options.reveal_progress = 1;
        layout_state.hovered = layout_state.tooltip_visual_index = SIZE_MAX;
        layout_state.tooltip_focus = layout_state.emphasis_focus = layout_state.hover_progress = 0;
        state = &layout_state;
        o = &layout_options;
    }
    bool animation_current =
        state->animated_at_valid && state->animated_at_time == cui_frame_time(ui) &&
        state->animated_custom_motion == o->custom_motion &&
        (!o->custom_motion || chart_motion_equal(state->animated_motion, o->motion)) &&
        state->active_appear_key == o->appear_key && state->animated_request_key != 0 &&
        state->animated_request_key == cui_chart_frame_key(o, state) &&
        (!(o->disable_appear || o->reduced_motion || cui_reduced_motion(ui)) ||
         state->rendered_reveal_progress == 1);
    bool rendered_for_appearance = animation_current && !o->reveal_enabled;
    if (!o->reveal_enabled && (state->appear_started || state->revision == 0) &&
        !animation_current) {
        if (cui_chart_render_animated(ui, state, o) != CUI_PLOT_OK)
            return false;
        rendered_for_appearance = true;
    }
    float w = o->logical_width > 0 ? o->logical_width : (float)o->width;
    float h = o->logical_height > 0 ? o->logical_height : (float)o->height;
    if (!staged) {
        cui_interact(ui, id, CUI_ROLE_IMAGE, name, false, false, 0);
        cui_set_semantic_details(
            ui, id,
            (cui_semantic_details){.pointer_activatable = !(current ? payload.options.noninteractive
                                                                    : o->noninteractive),
                                   .no_focus = true});
    }
    if (o->noninteractive) {
        state->appear_hover_pending = false;
        (void)cui_chart_update_hover(state, o, NAN, NAN);
    } else {
        if ((state->hovered < o->count || state->appear_hover_pending) &&
            state->tooltip_cursor_valid)
            (void)cui_chart_update_hover(state, o, state->tooltip_cursor_x,
                                         state->tooltip_cursor_y);
        size_t count = 0;
        const cui_event *events = cui_frame_events(ui, &count);
        for (size_t i = 0; i < count; i++) {
            if (events[i].type == CUI_EVENT_POINTER_MOVE) {
                if (cui_event_target(ui, i) != id && !cui_hovered(ui, id)) {
                    (void)cui_chart_update_hover(state, o, NAN, NAN);
                    continue;
                }
                (void)cui_chart_update_hover(state, o, events[i].x - state->bounds.x,
                                             events[i].y - state->bounds.y);
            }
        }
    }
    chart_paint_clock clock = {cui_frame_time(ui), cui_frame_delta(ui), cui_reduced_motion(ui),
                               NULL, ui};
    if (!current && !chart_paint_update(&clock, state, o, w, h, rendered_for_appearance))
        return false;
    if (staged) {
        typed_active = !o->noninteractive && state->tooltip_visual_index < o->count &&
                       state->tooltip_focus > 0;
        if (typed_active && !o->tooltip_content &&
            (!chart_tooltip_collect(state, o, name, &staged_builder) ||
             !chart_tooltip_measure(&staged_builder, chart_tooltip_measure_ui, ui, &staged_layout,
                                    o->tooltip_style, w, h)))
            return false;
        if (typed_active && o->tooltip_content && o->tooltip_style) {
            const cui_chart_tooltip_style *style = o->tooltip_style;
            bool bare = style->appearance == CUI_CHART_TOOLTIP_BARE;
            cui_size size = {style->min_width, 48};
            if (o->tooltip_measure)
                size = o->tooltip_measure(ui, state->tooltip_visual_index,
                                          &o->data[state->tooltip_visual_index], o->tooltip_user);
            if (!isfinite(size.width) || !isfinite(size.height) || size.width < 0 ||
                size.height < 0 || size.width + 2 * style->padding > 16384 ||
                size.height + 2 * style->padding > 16384)
                return false;
            staged_layout.width =
                bare ? w : fmaxf(style->min_width, size.width + 2 * style->padding);
            staged_layout.height = bare ? h : size.height + 2 * style->padding;
        }
        if (cui_last_error(ui) != CUI_OK)
            return false;
        cui_interact(ui, id, CUI_ROLE_IMAGE, name, false, false, 0);
        cui_set_semantic_details(
            ui, id,
            (cui_semantic_details){.pointer_activatable = !(current ? payload.options.noninteractive
                                                                    : o->noninteractive),
                                   .no_focus = true});
    }
    cui_begin_box(
        ui, id,
        (cui_box){
            .width = w, .height = h, .fixed_width = true, .fixed_height = true, .clip = true});
    if (!current)
        (void)cui_track_bounds(ui, id, staged ? &published_state->bounds : &state->bounds);
    cui_bitmap_paint paint = {
        .bitmap = {state->pixels, o->width, o->height, (size_t)o->width * 4, state->revision},
        .opacity = 1};
    bool ok;
    if (current) {
        size_t budget = 80 + (size_t)ceilf(h / 6) + (payload.options.tooltip_build ? 96 : 0) +
                        (payload.has_tooltip_style ? CUI_DROP_SHADOW_MAX : 0);
        ok = cui_set_prepaint(
            ui, (cui_prepaint_options){.callback = chart_prepaint,
                                       .payload = &payload,
                                       .payload_bytes = sizeof payload,
                                       .command_budget = budget,
                                       .hover_hitbox = !payload.options.noninteractive});
    } else {
        ok = cui_paint_bitmap(
            ui, id ^ UINT64_C(0x381c6b293497e8a1), paint,
            (cui_box){.width = w, .height = h, .fixed_width = true, .fixed_height = true});
    }
    if (o->kind == CUI_CHART_BAR && !o->noninteractive && state->emphasis_focus > 0 &&
        state->emphasis_visual_index < o->count) {
        cui_plot_rect bar;
        if (cui_chart_bar_bounds(o, state->emphasis_visual_index, &bar) == CUI_PLOT_OK) {
            bool horizontal =
                o->bar_alignment == CUI_CHART_BAR_LEFT || o->bar_alignment == CUI_CHART_BAR_RIGHT;
            float thickness = horizontal ? (bar.y_max - bar.y_min) * h / o->height
                                         : (bar.x_max - bar.x_min) * w / o->width;
            float horizontal_left = 0, horizontal_right = 0;
            if (horizontal)
                cui_chart_horizontal_bar_extent(o, &horizontal_left, &horizontal_right);
            float band_x = horizontal ? horizontal_left * w / o->width
                                      : state->emphasis_center * w / o->width - thickness / 2;
            float band_y = horizontal ? state->emphasis_center * h / o->height - thickness / 2
                           : o->bar_alignment == CUI_CHART_BAR_TOP && !o->hide_x_axis
                               ? 20 * h / o->height
                               : 0;
            float band_w =
                horizontal ? (horizontal_right - horizontal_left) * w / o->width : thickness;
            float band_h = horizontal ? thickness : h - (o->hide_x_axis ? 0 : 20 * h / o->height);
            cui_color band = o->foreground.a ? o->foreground : (cui_color){32, 32, 32, 255};
            band.a = (unsigned char)lroundf(band.a * .08f * state->emphasis_focus);
            if (band.a && band_w > 0 && band_h > 0) {
                cui_begin_absolute_box(ui, id ^ UINT64_C(0x9c97c1765a400001),
                                       (cui_box){.width = band_w,
                                                 .height = band_h,
                                                 .fixed_width = true,
                                                 .fixed_height = true,
                                                 .background = band},
                                       (cui_rect){band_x, band_y, band_w, band_h});
                cui_end_box(ui);
            }
        }
    }
    if (o->kind == CUI_CHART_CANDLESTICK && !o->noninteractive && state->emphasis_focus > 0 &&
        state->emphasis_visual_index < o->count) {
        cui_chart_candle_geometry candle;
        if (cui_chart_candle_bounds(o, state->emphasis_visual_index, &candle) == CUI_PLOT_OK) {
            float ratio = o->candle_body_ratio > 0 ? o->candle_body_ratio : .8f;
            float thickness =
                (float)((candle.body.x_max - candle.body.x_min) / ratio * w / o->width);
            float x = state->emphasis_center * w / o->width - thickness / 2;
            float height = h - (o->hide_x_axis ? 0 : 20 * h / o->height);
            cui_color band = o->foreground.a ? o->foreground : (cui_color){32, 32, 32, 255};
            band.a = (unsigned char)lroundf(band.a * .08f * state->emphasis_focus);
            if (band.a && thickness > 0 && height > 0) {
                cui_begin_absolute_box(ui, id ^ UINT64_C(0x0d239bcd14a60001),
                                       (cui_box){.width = thickness,
                                                 .height = height,
                                                 .fixed_width = true,
                                                 .fixed_height = true,
                                                 .background = band},
                                       (cui_rect){x, 0, thickness, height});
                cui_end_box(ui);
            }
        }
    }
    if (o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA) {
        if (state->hover_visual_started && state->hover_progress > 0) {
            float opacity = state->tooltip_focus;
            float x = state->hover_x;
            float top = 0;
            float bottom = h - chart_point_gutter(o) * h / o->height;
            cui_color crosshair = o->grid.a ? o->grid : (cui_color){120, 120, 120, 150};
            crosshair.a = (unsigned char)lroundf(crosshair.a * opacity);
            unsigned char *guide_pixels = state->hover_guide_pixels;
            if (bottom > 0 && bottom <= 4096 &&
                (!guide_pixels || state->hover_guide_capacity < (size_t)ceilf(bottom) * 4))
                guide_pixels =
                    cui_resource_get(ui, id, UINT64_C(0xc168179b47d043a2), 4096 * 4, NULL, NULL);
            if (bottom > 0 && bottom <= 16384 && guide_pixels &&
                (guide_pixels != state->hover_guide_pixels ||
                 state->hover_guide_capacity >= (size_t)ceilf(bottom) * 4)) {
                unsigned rows = (unsigned)ceilf(bottom);
                for (unsigned row = 0; row < rows; row++) {
                    unsigned char *pixel = guide_pixels + (size_t)row * 4;
                    pixel[0] = crosshair.r;
                    pixel[1] = crosshair.g;
                    pixel[2] = crosshair.b;
                    pixel[3] = row % 6 < 4 ? crosshair.a : 0;
                }
                state->hover_guide_revision++;
                if (!state->hover_guide_revision)
                    state->hover_guide_revision = 1;
                cui_begin_absolute_box(
                    ui, id ^ UINT64_C(0x9f39c738cd235001),
                    (cui_box){
                        .width = 1, .height = bottom, .fixed_width = true, .fixed_height = true},
                    (cui_rect){x, top, 1, bottom});
                ok &= cui_paint_bitmap(
                    ui, id ^ UINT64_C(0x9f39c738cd235002),
                    (cui_bitmap_paint){
                        .bitmap = {guide_pixels, 1, rows, 4, state->hover_guide_revision},
                        .opacity = 1},
                    (cui_box){
                        .width = 1, .height = bottom, .fixed_width = true, .fixed_height = true});
                cui_end_box(ui);
            } else {
                cui_begin_absolute_box(ui, id ^ UINT64_C(0x9f39c738cd235001),
                                       (cui_box){.width = 1,
                                                 .height = bottom - top,
                                                 .fixed_width = true,
                                                 .fixed_height = true,
                                                 .background = crosshair},
                                       (cui_rect){x, top, 1, bottom - top});
                cui_end_box(ui);
            }
            for (size_t series = 0; series <= o->extra_series_count; series++) {
                float dot_x = state->hover_x;
                float dot_y = state->hover_y[series];
                cui_color dot = series ? o->extra_series[series - 1].color : o->series[0];
                if (!dot.a)
                    dot = o->series[series].a ? o->series[series] : o->foreground;
                cui_chart_hover_dot(ui, id ^ (UINT64_C(0x8e4ad6f381012000) + series), dot_x, dot_y,
                                    dot, o->background, opacity);
            }
        }
    }
    if (o->kind == CUI_CHART_RADAR && !o->noninteractive &&
        state->tooltip_visual_index < o->count && state->tooltip_focus > 0)
        for (size_t series = 0; series <= o->extra_series_count; series++) {
            float pixel_x, pixel_y;
            if (cui_chart_radar_vertex(o, state->tooltip_visual_index, series, &pixel_x,
                                       &pixel_y) != CUI_PLOT_OK)
                continue;
            float dot_x = pixel_x * w / o->width;
            float dot_y = pixel_y * h / o->height;
            cui_color dot = series ? o->extra_series[series - 1].color : o->series[0];
            if (!dot.a)
                dot = o->series[series].a ? o->series[series] : o->foreground;
            cui_chart_hover_dot(ui, id ^ (UINT64_C(0x46a5c19a4f310000) + series), dot_x, dot_y, dot,
                                o->background, state->tooltip_focus);
        }
    if (!o->hide_axes && !o->hide_x_axis && o->kind != CUI_CHART_PIE &&
        o->kind != CUI_CHART_RADAR && o->kind != CUI_CHART_SANKEY && w >= 60 && h >= 40) {
        size_t slots = (o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA)
                           ? chart_point_slots(o)
                           : (o->point_count ? o->point_count : o->count);
        for (size_t i = 0; i < o->count; i++) {
            if (!cui_chart_x_label_visible(o, i))
                continue;
            cui_str label = o->data[i].label;
            if (!label.length || !label.data)
                continue;
            bool point_chart = o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA;
            float left = point_chart ? (o->show_y_axis && !o->y_axis_inside
                                            ? (o->value_axis_gutter > 0 ? o->value_axis_gutter : 32)
                                            : 0)
                                     : (o->show_y_axis && !o->y_axis_inside
                                            ? (o->value_axis_gutter > 0 ? o->value_axis_gutter : 50)
                                            : 24);
            float right = point_chart ? o->width : o->width - 12;
            float x = slots > 1
                          ? (left + (right - left) * (float)i / (float)(slots - 1)) * w / o->width
                          : w / 2;
            float y = point_chart && o->layout_mode == CUI_CHART_LAYOUT_SOURCE_POINT
                          ? (o->height - chart_point_gutter(o) + 6) * h / o->height
                          : h - 14;
            cui_size measured = cui_measure_text(ui, label, 10);
            float box_width = fmaxf(1, measured.width);
            float box_height = fmaxf(12, measured.height);
            float box_x = x - box_width / 2;
            if (point_chart && slots > 1) {
                if (i == 0)
                    box_x = x;
                else if (i == slots - 1)
                    box_x = x - box_width;
            }
            if (o->kind == CUI_CHART_CANDLESTICK) {
                cui_chart_candle_geometry candle;
                if (cui_chart_candle_bounds(o, i, &candle) == CUI_PLOT_OK) {
                    x = (float)(candle.wick.x_min * w / o->width);
                    box_x = x - box_width / 2;
                }
            }
            if (o->kind == CUI_CHART_BAR) {
                float band_center;
                if (cui_chart_bar_band_center(o, i, &band_center) == CUI_PLOT_OK) {
                    double lo, hi;
                    if (!cui_chart_value_domain(o, &lo, &hi))
                        continue;
                    double zero_fraction;
                    cui_plot_scale zero_scale = {CUI_PLOT_LINEAR, lo, hi, 0, 1, true};
                    if (cui_plot_map(&zero_scale, 0, &zero_fraction) != CUI_PLOT_OK)
                        continue;
                    float fraction = (float)zero_fraction;
                    if (o->bar_alignment == CUI_CHART_BAR_LEFT ||
                        o->bar_alignment == CUI_CHART_BAR_RIGHT) {
                        float bar_left, bar_right;
                        cui_chart_horizontal_bar_extent(o, &bar_left, &bar_right);
                        float baseline = (o->bar_alignment == CUI_CHART_BAR_LEFT
                                              ? bar_left + (bar_right - bar_left) * fraction
                                              : bar_right - (bar_right - bar_left) * fraction) *
                                         w / o->width;
                        box_x = o->bar_alignment == CUI_CHART_BAR_LEFT ? baseline - 2 - box_width
                                                                       : baseline + 2;
                        y = band_center * h / o->height - box_height / 2;
                    } else {
                        x = band_center * w / o->width;
                        box_x = x - box_width / 2;
                        float top = (o->bar_alignment == CUI_CHART_BAR_TOP
                                         ? (o->hide_x_axis ? 0 : 20)
                                         : (o->show_bar_values ? 12 : 10)) *
                                    h / o->height;
                        float bottom = (o->bar_alignment == CUI_CHART_BAR_TOP
                                            ? o->height - (o->show_bar_values ? 12 : 10)
                                            : o->height - (o->hide_x_axis ? 0 : 20)) *
                                       h / o->height;
                        float baseline = o->bar_alignment == CUI_CHART_BAR_BOTTOM
                                             ? bottom - (bottom - top) * fraction
                                             : top + (bottom - top) * fraction;
                        bool below =
                            (o->data[i].value < 0) == (o->bar_alignment == CUI_CHART_BAR_TOP);
                        y = baseline + (below ? 2 : -12);
                    }
                }
            }
            cui_begin_absolute_box(ui, id ^ (UINT64_C(0x5db92a387c4e1100) + i),
                                   (cui_box){.width = box_width,
                                             .height = box_height,
                                             .fixed_width = true,
                                             .fixed_height = true},
                                   (cui_rect){box_x, y, box_width, box_height});
            cui_text(ui, label, 10, o->foreground);
            cui_end_box(ui);
        }
    }
    if (!o->hide_axes && o->show_y_axis && o->kind != CUI_CHART_PIE && o->kind != CUI_CHART_RADAR &&
        o->kind != CUI_CHART_SANKEY && w >= 60 && h >= 40 && o->width >= 60 && o->height >= 40) {
        double lo, hi;
        if (cui_chart_value_domain(o, &lo, &hi)) {
            unsigned count = o->tick_count ? o->tick_count : 5;
            if (count < 2)
                count = 2;
            unsigned visible = count;
            bool horizontal_bar =
                o->kind == CUI_CHART_BAR &&
                (o->bar_alignment == CUI_CHART_BAR_LEFT || o->bar_alignment == CUI_CHART_BAR_RIGHT);
            bool point_chart = o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA;
            double top = point_chart ? (o->custom_y_padding ? o->y_padding_top : 10) : 12;
            double bottom = point_chart ? o->height - chart_point_gutter(o) -
                                              (o->custom_y_padding ? o->y_padding_bottom : 0)
                                        : (o->height > 32 ? o->height - 20 : 12);
            if (o->kind == CUI_CHART_BAR) {
                bool top_aligned = o->bar_alignment == CUI_CHART_BAR_TOP;
                top = horizontal_bar ? 0
                      : top_aligned  ? (o->hide_x_axis ? 0 : 20)
                                     : (o->show_bar_values ? 12 : 10);
                bottom = horizontal_bar ? o->height - (o->show_y_axis && !o->y_axis_inside ? 32 : 0)
                         : top_aligned  ? o->height - (o->show_bar_values ? 12 : 10)
                                        : o->height - (o->hide_x_axis ? 0 : 20);
            }
            bool top_bar = o->kind == CUI_CHART_BAR && o->bar_alignment == CUI_CHART_BAR_TOP;
            cui_plot_scale scale = {CUI_PLOT_LINEAR,        lo,  hi, top_bar ? top : bottom,
                                    top_bar ? bottom : top, true};
            for (unsigned i = 0; i < visible; i++) {
                char text[48];
                unsigned tick = i;
                double pixel_y =
                    point_chart ? (o->height - chart_point_gutter(o)) * (double)tick / (count - 1)
                                : top + (bottom - top) * (double)tick / (count - 1);
                double value = lo;
                if (horizontal_bar) {
                    cui_plot_scale tick_scale = {CUI_PLOT_LINEAR, lo, hi, 0, 1, true};
                    if (cui_plot_inverse(&tick_scale, (double)tick / (count - 1), &value) !=
                        CUI_PLOT_OK)
                        continue;
                } else
                    (void)cui_plot_inverse(&scale, pixel_y, &value);
                cui_str label = cui_chart_tick_number(text, sizeof text, value, o->y_tick_format,
                                                      o->format_user);
                float y = (float)(pixel_y * h / o->height);
                float gutter = o->value_axis_gutter > 0 ? o->value_axis_gutter : 32;
                cui_size measured = cui_measure_text(ui, label, 10);
                float label_width = fmaxf(1, measured.width);
                float x = o->y_axis_inside ? 2 : gutter * w / o->width - 4 - label_width;
                float label_y = o->y_axis_inside ? (y < 12 ? y + 2 : y - 12) : y - 5;
                if (point_chart && !o->y_axis_inside)
                    label_y =
                        fmaxf(0, fminf(label_y,
                                       (o->height - chart_point_gutter(o)) * h / o->height - 10));
                if (horizontal_bar) {
                    float bar_left, bar_right;
                    cui_chart_horizontal_bar_extent(o, &bar_left, &bar_right);
                    float left = bar_left * w / o->width;
                    float right = bar_right * w / o->width;
                    float fraction = (float)tick / (count - 1);
                    x = (o->bar_alignment == CUI_CHART_BAR_LEFT
                             ? left + (right - left) * fraction
                             : right - (right - left) * fraction) -
                        label_width / 2;
                    label_y = o->y_axis_inside ? h - 12 : h - 26;
                }
                cui_begin_absolute_box(ui, id ^ (UINT64_C(0x9120416c66313b00) + i),
                                       (cui_box){.width = label_width,
                                                 .height = 12,
                                                 .fixed_width = true,
                                                 .fixed_height = true},
                                       (cui_rect){x, label_y, label_width, 12});
                cui_text(ui, label, 10, o->foreground);
                cui_end_box(ui);
            }
        }
    }
    if (o->kind == CUI_CHART_RADAR && w >= 50 && h >= 50) {
        double cx = w / 2.0, cy = h / 2.0;
        double radius =
            (o->radar_outer_radius > 0 ? o->radar_outer_radius : o->height * .4) * h / o->height;
        for (size_t i = 0; i < o->count; i++) {
            cui_str label = o->data[i].label;
            bool custom = o->radar_label_measure && o->radar_label_paint;
            if (!custom && (!label.length || !label.data))
                continue;
            double angle = -3.14159265358979323846 / 2 + 2 * 3.14159265358979323846 * i / o->count;
            float gap = o->radar_label_gap > 0 ? o->radar_label_gap : 10;
            float x = (float)(cx + (radius + gap) * cos(angle));
            float y = (float)(cy + (radius + gap) * sin(angle));
            cui_size measured =
                custom ? o->radar_label_measure(ui, &o->data[i], i, o->radar_label_user)
                       : cui_measure_text(ui, label, 10);
            if (!isfinite(measured.width) || !isfinite(measured.height) || measured.width <= 0 ||
                measured.height <= 0)
                continue;
            float label_width = fmaxf(1, fminf(w, measured.width));
            float label_height = custom ? fminf(h, measured.height) : fmaxf(12, measured.height);
            float left = fmaxf(0, fminf(w - label_width, x - label_width / 2));
            float top = fmaxf(0, fminf(h - label_height, y - label_height / 2));
            cui_begin_absolute_box(ui, id ^ (UINT64_C(0x25a41f7c9d310000) + i),
                                   (cui_box){.width = label_width,
                                             .height = label_height,
                                             .fixed_width = true,
                                             .fixed_height = true},
                                   (cui_rect){left, top, label_width, label_height});
            if (custom)
                o->radar_label_paint(ui, id ^ (UINT64_C(0x35a41f7c9d310000) + i), &o->data[i], i,
                                     o->radar_label_user);
            else
                cui_text(ui, label, 10,
                         o->radar_label_color.a ? o->radar_label_color : o->foreground);
            cui_end_box(ui);
        }
    }
    if (o->kind == CUI_CHART_SANKEY && (o->sankey_show_labels || o->sankey_show_values ||
                                        o->sankey_labels || o->sankey_label_measure)) {
        cui_chart_sankey_node nodes[256];
        if (cui_chart_sankey_layout(o, nodes, 256) == CUI_PLOT_OK)
            for (size_t i = 0; i < o->count; i++) {
                cui_chart_sankey_label lines[16] = {0};
                size_t line_count = 0;
                char value_buffer[48];
                bool custom = o->sankey_label_measure != NULL;
                if (!custom && (!cui_chart_sankey_label_lines(o, &nodes[i], i, lines, &line_count,
                                                              value_buffer) ||
                                !line_count))
                    continue;
                float gap = o->sankey_label_gap > 0 ? o->sankey_label_gap : 6;
                bool first = nodes[i].layer == 0;
                bool last = true;
                for (size_t j = 0; j < o->count; j++)
                    if (nodes[j].layer > nodes[i].layer)
                        last = false;
                float block_height = 0, text_width = 0;
                if (custom) {
                    cui_size size = o->sankey_label_measure(ui, &o->data[i], i, nodes[i].throughput,
                                                            o->sankey_label_user);
                    if (!isfinite(size.width) || !isfinite(size.height) || size.width < 0 ||
                        size.height < 0 || size.width > 16384 || size.height > 16384)
                        continue;
                    text_width = size.width;
                    block_height = size.height;
                } else {
                    for (size_t line = 0; line < line_count; line++) {
                        float font = lines[line].font_size > 0 ? lines[line].font_size : 10;
                        block_height += font + 2;
                        text_width =
                            fmaxf(text_width, cui_measure_text(ui, lines[line].text, font).width);
                    }
                }
                if (text_width <= 0 || block_height <= 0)
                    continue;
                text_width = fmaxf(1, fminf(w * .2f, text_width));
                float center_y = (nodes[i].y0 + nodes[i].y1) * h / (2 * o->height);
                float y = first || last
                              ? fmaxf(0, fminf(h - block_height, center_y - block_height / 2))
                              : nodes[i].y0 * h / o->height - block_height - 2;
                float node_left = nodes[i].x0 * w / o->width;
                float node_right = nodes[i].x1 * w / o->width;
                float x = first  ? fmaxf(0, node_left - gap - text_width)
                          : last ? fminf(w - text_width, node_right + gap)
                                 : fmaxf(0, fminf(w - text_width,
                                                  (node_left + node_right - text_width) / 2));
                if (custom) {
                    cui_id label_id = id ^ (UINT64_C(0xdcb59171b0060000) + i);
                    cui_begin_absolute_box(ui, label_id,
                                           (cui_box){.width = text_width,
                                                     .height = block_height,
                                                     .fixed_width = true,
                                                     .fixed_height = true},
                                           (cui_rect){x, y, text_width, block_height});
                    o->sankey_label_paint(ui, label_id, &o->data[i], i, nodes[i].throughput,
                                          o->sankey_label_user);
                    cui_end_box(ui);
                    continue;
                }
                for (size_t line = 0; line < line_count; line++) {
                    float font = lines[line].font_size > 0 ? lines[line].font_size : 10;
                    float line_height = font + 2;
                    cui_begin_absolute_box(ui, id ^ (UINT64_C(0xe5374a8b09ef0000) + i * 16 + line),
                                           (cui_box){.width = text_width,
                                                     .height = line_height,
                                                     .fixed_width = true,
                                                     .fixed_height = true},
                                           (cui_rect){x, y, text_width, line_height});
                    cui_text(ui, lines[line].text, font,
                             lines[line].color.a ? lines[line].color : o->foreground);
                    cui_end_box(ui);
                    y += line_height;
                }
            }
    }
    if (o->kind == CUI_CHART_BAR && o->show_bar_values && o->count <= 128) {
        for (size_t i = 0; i < o->count; i++) {
            cui_plot_rect bar;
            if (cui_chart_bar_bounds(o, i, &bar) != CUI_PLOT_OK)
                continue;
            char value[48];
            cui_str label = cui_chart_number(value, sizeof value, o->data[i].value,
                                             o->bar_label_format, o->format_user);
            if (!label.length)
                continue;
            cui_size measured = cui_measure_text(ui, label, 10);
            float label_width = fmaxf(1, measured.width);
            float label_height = fmaxf(12, measured.height);
            float x, y;
            bool negative = o->data[i].value < 0;
            if (o->bar_alignment == CUI_CHART_BAR_LEFT || o->bar_alignment == CUI_CHART_BAR_RIGHT) {
                bool right = (o->bar_alignment == CUI_CHART_BAR_LEFT) != negative;
                x = (float)((right ? bar.x_max + 2 : bar.x_min - 2) * w / o->width);
                if (!right)
                    x -= label_width;
                y = (float)((bar.y_min + bar.y_max) / 2 * h / o->height) - label_height / 2;
            } else {
                bool above = (o->bar_alignment == CUI_CHART_BAR_BOTTOM) != negative;
                x = (float)((bar.x_min + bar.x_max) / 2 * w / o->width) - label_width / 2;
                y = (float)((above ? bar.y_min : bar.y_max + 2) * h / o->height);
                if (above)
                    y -= label_height;
            }
            cui_begin_absolute_box(ui, id ^ (UINT64_C(0x81ce703914b50000) + i),
                                   (cui_box){.width = label_width,
                                             .height = label_height,
                                             .fixed_width = true,
                                             .fixed_height = true},
                                   (cui_rect){x, y, label_width, label_height});
            cui_color color = o->bar_label_color_fn
                                  ? o->bar_label_color_fn(&o->data[i], i, o->bar_label_color_user)
                                  : (o->bar_label_color.a ? o->bar_label_color : o->foreground);
            cui_text(ui, label, 10, color);
            cui_end_box(ui);
        }
    }
    if (o->kind == CUI_CHART_PIE && o->pie_show_labels && o->count <= 256) {
        cui_chart_pie_label labels[256];
        if (cui_chart_pie_labels(o, labels, 256) == CUI_PLOT_OK) {
            float opacity = fmaxf(0, fminf(1, (state->rendered_reveal_progress - .7f) / .3f));
            cui_color color = o->pie_label_color.a ? o->pie_label_color
                              : o->foreground.a    ? o->foreground
                                                   : (cui_color){32, 32, 32, 255};
            color.a = (unsigned char)lroundf(color.a * opacity);
            for (size_t i = 0; i < o->count; i++) {
                if (!labels[i].visible || !color.a)
                    continue;
                cui_size measured = cui_measure_text(ui, labels[i].text, 10);
                float label_width = fmaxf(1, fminf(w, measured.width));
                float label_height = fmaxf(12, measured.height);
                float anchor_x = labels[i].anchor_x * w / o->width;
                float anchor_y = labels[i].anchor_y * h / o->height;
                float label_x = labels[i].right ? anchor_x + 4 : anchor_x - 4 - label_width;
                float label_y = anchor_y - label_height / 2;
                label_x = fmaxf(0, fminf(w - label_width, label_x));
                label_y = fmaxf(0, fminf(h - label_height, label_y));
                cui_begin_absolute_box(ui, id ^ (UINT64_C(0xa6a9f030b2543000) + i),
                                       (cui_box){.width = label_width,
                                                 .height = label_height,
                                                 .fixed_width = true,
                                                 .fixed_height = true},
                                       (cui_rect){label_x, label_y, label_width, label_height});
                cui_text(ui, labels[i].text, 10, color);
                cui_end_box(ui);
            }
        }
    }
    if (!o->hide_legend && (o->kind == CUI_CHART_LINE || o->kind == CUI_CHART_AREA) && w >= 100 &&
        h >= 40)
        for (size_t i = 0; i < o->extra_series_count; i++) {
            cui_str label = o->extra_series[i].name;
            if (!label.length || !label.data)
                continue;
            cui_color color =
                o->extra_series[i].color.a ? o->extra_series[i].color : o->series[(i + 1) % 8];
            cui_begin_absolute_box(
                ui, id ^ (UINT64_C(0xe24a05cd17230000) + i),
                (cui_box){.width = 72, .height = 12, .fixed_width = true, .fixed_height = true},
                (cui_rect){w - 74, 2 + 12 * (float)i, 72, 12});
            cui_text(ui, label, 10, color);
            cui_end_box(ui);
        }
    if (staged && typed_active) {
        chart_tooltip_compose(ui, id, state, o, w, h, &staged_builder, &staged_layout);
    } else if (!staged && !o->noninteractive && state->tooltip_visual_index < o->count &&
               state->tooltip_focus > 0) {
        size_t tooltip_index = state->tooltip_visual_index;
        const cui_chart_datum *datum = &o->data[tooltip_index];
        cui_str title = {0};
        char row_text[8][52] = {{0}};
        size_t row_length[8] = {0}, row_count = 0;
        cui_color row_color[8] = {{0}};
        cui_str row_label[8] = {{0}};
        cui_color row_swatch[8] = {{0}};
        float tooltip_width = 150;
        if (!o->tooltip_content) {
            title = o->tooltip_title ? o->tooltip_title(datum, tooltip_index, o->tooltip_user)
                                     : chart_tooltip_fallback_title(o, datum);
            if (title.data && title.length)
                tooltip_width =
                    fmaxf(tooltip_width,
                          cui_measure_text_ex(ui, title, 11,
                                              (cui_text_style){.weight = CUI_TEXT_WEIGHT_SEMIBOLD})
                                  .width +
                              16);
            size_t rows = o->kind == CUI_CHART_CANDLESTICK ? 4 : o->extra_series_count + 1;
            for (size_t series = 0; series < rows; series++) {
                double number =
                    o->kind == CUI_CHART_SANKEY ? state->tooltip_visual_value : datum->value;
                if (o->kind != CUI_CHART_CANDLESTICK && series)
                    number = o->extra_series[series - 1].values[tooltip_index];
                if (o->kind == CUI_CHART_CANDLESTICK)
                    number = series == 0   ? datum->open
                             : series == 1 ? datum->high
                             : series == 2 ? datum->low
                                           : datum->close;
                char value[48];
                cui_str formatted;
                double pie_share = 0;
                if (o->kind == CUI_CHART_PIE &&
                    (o->pie_tooltip_value || (!o->tooltip_value && !o->tooltip_format))) {
                    long double total = 0;
                    for (size_t part = 0; part < o->count; part++)
                        total += fmax(0, o->data[part].value);
                    pie_share = total > 0 ? (double)((long double)number / total * 100) : 0;
                }
                if (o->kind == CUI_CHART_PIE && o->pie_tooltip_value) {
                    size_t written = o->pie_tooltip_value(datum, tooltip_index, number, pie_share,
                                                          value, sizeof value, o->tooltip_user);
                    formatted =
                        (cui_str){value, written < sizeof value ? written : sizeof value - 1};
                } else if (o->tooltip_value) {
                    size_t written = o->tooltip_value(datum, tooltip_index, series, number, value,
                                                      sizeof value, o->tooltip_user);
                    formatted =
                        (cui_str){value, written < sizeof value ? written : sizeof value - 1};
                } else if (o->kind == CUI_CHART_PIE && !o->tooltip_format) {
                    char raw_value[32];
                    cui_str raw = cui_chart_number(raw_value, sizeof raw_value, number, NULL, NULL);
                    int written = snprintf(value, sizeof value, "%.*s (%.1f%%)", (int)raw.length,
                                           raw.data, pie_share);
                    formatted =
                        (cui_str){value, written <= 0                     ? 0
                                         : (size_t)written < sizeof value ? (size_t)written
                                                                          : sizeof value - 1};
                } else
                    formatted =
                        cui_chart_number(value, sizeof value, number,
                                         o->tooltip_format             ? o->tooltip_format
                                         : o->kind == CUI_CHART_SANKEY ? o->sankey_value_format
                                                                       : NULL,
                                         o->format_user);
                memcpy(row_text[row_count], formatted.data, formatted.length);
                row_length[row_count] = formatted.length;
                if (o->kind == CUI_CHART_CANDLESTICK) {
                    static const cui_str candle_labels[] = {
                        {"Open", 4}, {"High", 4}, {"Low", 3}, {"Close", 5}};
                    row_label[row_count] = candle_labels[series];
                    row_swatch[row_count] = datum->close > datum->open ? o->bullish : o->bearish;
                } else if (o->kind == CUI_CHART_PIE) {
                    row_label[row_count] = name;
                    row_swatch[row_count] =
                        datum->color.a ? datum->color : o->series[tooltip_index % 8];
                } else if (o->kind == CUI_CHART_SANKEY) {
                    row_swatch[row_count] =
                        datum->color.a ? datum->color : o->series[tooltip_index % 8];
                } else if (o->kind == CUI_CHART_BAR) {
                    row_label[row_count] = name;
                    row_swatch[row_count] = o->bar_gradient_start.a ? o->bar_gradient_start
                                            : datum->color.a        ? datum->color
                                                                    : o->series[0];
                } else {
                    row_label[row_count] = series ? o->extra_series[series - 1].name : name;
                    row_swatch[row_count] = series && o->extra_series[series - 1].color.a
                                                ? o->extra_series[series - 1].color
                                                : o->series[series % 8];
                }
                if (o->tooltip_label)
                    row_label[row_count] =
                        o->tooltip_label(datum, tooltip_index, series, o->tooltip_user);
                if (row_label[row_count].length && !row_label[row_count].data)
                    row_label[row_count] = (cui_str){0};
                row_color[row_count] = o->foreground;
                if (o->tooltip_value_color)
                    row_color[row_count] = o->tooltip_value_color(datum, tooltip_index, series,
                                                                  number, o->tooltip_user);
                if (!row_swatch[row_count].a)
                    row_swatch[row_count] = o->foreground;
                float label_width = cui_measure_text(ui, row_label[row_count], 11).width;
                float value_width =
                    cui_measure_text(ui, (cui_str){row_text[row_count], row_length[row_count]}, 11)
                        .width;
                tooltip_width = fmaxf(tooltip_width, 16 + 8 + 6 + label_width + 12 + value_width);
                row_count++;
            }
        }
        float tooltip_height =
            o->tooltip_content ? 64
                               : 16 + (title.length && title.data ? 18 : 0) + 16 * (float)row_count;
        if (o->tooltip_content && o->tooltip_measure) {
            cui_size content = o->tooltip_measure(ui, tooltip_index, datum, o->tooltip_user);
            if (isfinite(content.width) && isfinite(content.height) && content.width >= 0 &&
                content.height >= 0 && content.width <= 16368 && content.height <= 16368) {
                tooltip_width = fmaxf(150, content.width + 16);
                tooltip_height = content.height + 16;
            }
        }
        float cursor_x = state->tooltip_cursor_valid ? state->tooltip_cursor_x : 0;
        float cursor_y = state->tooltip_cursor_valid ? state->tooltip_cursor_y : 0;
        float tooltip_x = cursor_x < w / 2 ? cursor_x + 8 : cursor_x - 8 - tooltip_width;
        float tooltip_y = cursor_y < h / 2 ? cursor_y + 8 : cursor_y - 8 - tooltip_height;
        cui_begin_portal(ui, id ^ UINT64_C(0xd8a736411cb50000),
                         (cui_box){.width = tooltip_width,
                                   .height = tooltip_height,
                                   .fixed_width = true,
                                   .fixed_height = true,
                                   .radius = 4,
                                   .background = o->background,
                                   .border = o->grid,
                                   .border_width = 1,
                                   .opacity = state->tooltip_focus,
                                   .custom_opacity = true},
                         (cui_rect){state->bounds.x + tooltip_x, state->bounds.y + tooltip_y,
                                    tooltip_width, tooltip_height},
                         120);
        if (o->tooltip_content) {
            cui_begin_absolute_box(ui, id ^ UINT64_C(0x6c712ab004e9b669),
                                   (cui_box){.width = tooltip_width - 16,
                                             .height = tooltip_height - 16,
                                             .fixed_width = true,
                                             .fixed_height = true},
                                   (cui_rect){8, 8, tooltip_width - 16, tooltip_height - 16});
            o->tooltip_content(ui, id, tooltip_index, datum, o->tooltip_user);
            cui_end_box(ui);
        } else {
            float row_y = 8;
            if (title.length && title.data) {
                cui_begin_absolute_box(ui, id ^ UINT64_C(0x6c712ab004e9b668),
                                       (cui_box){.width = tooltip_width - 16,
                                                 .height = 14,
                                                 .fixed_width = true,
                                                 .fixed_height = true},
                                       (cui_rect){8, row_y, tooltip_width - 16, 14});
                cui_text_ex(ui, title, 11, o->foreground, 0,
                            (cui_text_style){.weight = CUI_TEXT_WEIGHT_SEMIBOLD});
                cui_end_box(ui);
                row_y += 18;
            }
            for (size_t series = 0; series < row_count; series++) {
                cui_begin_absolute_box(ui, id ^ (UINT64_C(0x44a55e948c7f0000) + series),
                                       (cui_box){.width = 8,
                                                 .height = 8,
                                                 .fixed_width = true,
                                                 .fixed_height = true,
                                                 .radius = 4,
                                                 .background = row_swatch[series]},
                                       (cui_rect){8, row_y + 3, 8, 8});
                cui_end_box(ui);
                cui_begin_absolute_box(ui, id ^ (UINT64_C(0xb48d186a501e6b77) + series),
                                       (cui_box){.width = tooltip_width - 24,
                                                 .height = 16,
                                                 .fixed_width = true,
                                                 .fixed_height = true},
                                       (cui_rect){22, row_y, tooltip_width - 30, 16});
                cui_color muted = o->foreground;
                muted.a = (unsigned char)(muted.a * .7f);
                cui_text(ui, row_label[series], 11, muted);
                cui_end_box(ui);
                float value_width =
                    cui_measure_text(ui, (cui_str){row_text[series], row_length[series]}, 11).width;
                cui_begin_absolute_box(
                    ui, id ^ (UINT64_C(0x23d40cb791a60000) + series),
                    (cui_box){.width = value_width,
                              .height = 16,
                              .fixed_width = true,
                              .fixed_height = true},
                    (cui_rect){tooltip_width - 8 - value_width, row_y, value_width, 16});
                cui_text(ui, (cui_str){row_text[series], row_length[series]}, 11,
                         row_color[series]);
                cui_end_box(ui);
                row_y += 16;
            }
        }
        cui_end_box(ui);
    }
    cui_end_box(ui);
    if (staged) {
        if (!ok || cui_last_error(ui) != CUI_OK)
            return false;
        *published_state = *state;
    }
    return ok;
}
