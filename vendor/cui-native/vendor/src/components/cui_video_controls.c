/* SPDX-License-Identifier: MIT */
#include "cui/cui_video_controls.h"
#include <math.h>
#include <stdio.h>
cui_id cui_video_part_id(cui_id id, cui_video_part part) {
    static const uint64_t salts[] = {UINT64_C(0x27cb57021ee10101), UINT64_C(0x27cb57021ee10102),
                                     UINT64_C(0x27cb57021ee10103), UINT64_C(0x27cb57021ee10104),
                                     UINT64_C(0x27cb57021ee10105), UINT64_C(0x27cb57021ee10106),
                                     UINT64_C(0x27cb57021ee10107)};
    if (!id || part < CUI_VIDEO_PART_PLAY || part > CUI_VIDEO_PART_SURFACE)
        return 0;
    cui_id child = id ^ salts[part];
    return child ? child : UINT64_MAX;
}
cui_video_controls_options cui_video_controls_defaults(void) {
    return (cui_video_controls_options){.visible = CUI_VIDEO_CONTROL_PLAY | CUI_VIDEO_CONTROL_SKIP |
                                                   CUI_VIDEO_CONTROL_TIMELINE,
                                        .width = 320,
                                        .skip_seconds = 10,
                                        .seek_step = 1,
                                        .requested_rate = 1,
                                        .play_icon = cui_string("play"),
                                        .pause_icon = cui_string("pause"),
                                        .back_icon = cui_string("skip-back"),
                                        .forward_icon = cui_string("skip-forward"),
                                        .fullscreen_icon = cui_string("maximize"),
                                        .leave_fullscreen_icon = cui_string("minimize"),
                                        .rate_icon = cui_string("gauge"),
                                        .play_label = cui_string("Play"),
                                        .pause_label = cui_string("Pause"),
                                        .back_label = cui_string("Skip backward"),
                                        .forward_label = cui_string("Skip forward"),
                                        .timeline_label = cui_string("Playback position"),
                                        .fullscreen_label = cui_string("Enter fullscreen"),
                                        .leave_fullscreen_label = cui_string("Leave fullscreen"),
                                        .rate_label = cui_string("Playback rate")};
}
cui_video_player_options cui_video_player_defaults(void) {
    cui_video_player_options o = {
        .surface = {.geometry = {.width = 320, .height = 180, .fit = CUI_IMAGE_CONTAIN},
                    .opacity = 1},
        .controls = true,
        .controls_options = cui_video_controls_defaults()};
    o.controls_options.compact = true;
    o.controls_options.visible |= CUI_VIDEO_CONTROL_FULLSCREEN;
    return o;
}
static void short_time(char *text, size_t size, double seconds) {
    double minutes = floor(seconds / 60);
    unsigned remainder = (unsigned)fmod(floor(seconds), 60);
    int n = snprintf(text, size, "%.0f:%02u", minutes, remainder);
    if (n < 0 || (size_t)n >= size)
        snprintf(text, size, "--:--");
}
static bool str_valid(cui_str s) {
    return !s.length || s.data;
}
static bool view_valid(cui_video_view v) {
    cui_video s = v.timeline;
    return !(v.capabilities & ~15u) && v.source_id && s.generation && isfinite(s.position) &&
           s.position >= 0 && isfinite(s.duration) && s.duration >= 0 && isfinite(s.rate) &&
           s.rate > 0 && (s.duration_known ? s.position <= s.duration : s.duration == 0) &&
           !(s.playing && s.ended) && v.status >= CUI_VIDEO_READY && v.status <= CUI_VIDEO_FAILED &&
           str_valid(v.status_description);
}
static bool options_valid(cui_video_controls_options o) {
    return !(o.visible & ~31u) && isfinite(o.width) && o.width >= 160 && o.width <= 65535 &&
           isfinite(o.skip_seconds) && o.skip_seconds > 0 && isfinite(o.seek_step) &&
           o.seek_step > 0 && isfinite(o.requested_rate) && o.requested_rate > 0 &&
           str_valid(o.play_icon) && str_valid(o.pause_icon) && str_valid(o.back_icon) &&
           str_valid(o.forward_icon) && str_valid(o.fullscreen_icon) &&
           str_valid(o.leave_fullscreen_icon) && str_valid(o.rate_icon) &&
           str_valid(o.play_label) && str_valid(o.pause_label) && str_valid(o.back_label) &&
           str_valid(o.forward_label) && str_valid(o.timeline_label) &&
           str_valid(o.fullscreen_label) && str_valid(o.leave_fullscreen_label) &&
           str_valid(o.rate_label);
}
typedef struct {
    cui_video_controls_state *state;
    cui_video_view view;
    cui_video_controls_options options;
    unsigned row_items, row_capacity;
} request_context;
static void emit(request_context *c, cui_video_request request) {
    if (!c->options.on_request || c->state->sequence == UINT64_MAX)
        return;
    request.source_id = c->view.source_id;
    request.generation = c->view.timeline.generation;
    request.sequence = ++c->state->sequence;
    c->options.on_request(c->options.user, &request);
}
static void seek_request(void *user, const cui_slider_request *r) {
    request_context *c = user;
    emit(c,
         (cui_video_request){.kind = CUI_VIDEO_SEEK, .phase = r->phase, .value.seconds = r->value});
}
typedef struct {
    request_context *context;
    cui_video_request request;
} button_request;
static void button_clicked(void *user, const cui_button_click_event *event) {
    (void)event;
    button_request *b = user;
    emit(b->context, b->request);
}
static void button(cui_context *ui, cui_id id, const cui_theme *theme, request_context *c,
                   cui_str label, cui_str icon, bool enabled, cui_video_request request) {
    if (c->row_items && c->row_items % c->row_capacity == 0) {
        cui_end_box(ui);
        cui_begin_box(ui, 0, (cui_box){.row = true, .gap = 4});
    }
    ++c->row_items;
    bool disabled = c->options.disabled || !c->options.on_request || !enabled ||
                    c->state->sequence == UINT64_MAX;
    request.phase = CUI_SLIDER_COMMIT;
    button_request b = {c, request};
    cui_button_ex(ui, id, theme, label,
                  (cui_button_options){.icon = icon,
                                       .icon_only = true,
                                       .disabled = disabled,
                                       .pixel_size = 44,
                                       .icon_size = c->options.compact ? 20 : 0,
                                       .variant = CUI_BUTTON_GHOST,
                                       .on_click_event = button_clicked,
                                       .action_user = &b});
}
static void timeline(cui_context *ui, cui_id id, const cui_theme *theme, request_context *c) {
    cui_video_view view = c->view;
    cui_video_controls_options o = c->options;
    cui_video_controls_state *state = c->state;
    bool ready = view.status == CUI_VIDEO_READY;
    if (o.visible & CUI_VIDEO_CONTROL_TIMELINE) {
        char time[96];
        int length = view.timeline.duration_known
                         ? snprintf(time, sizeof time, "%.3f / %.3f seconds",
                                    view.timeline.position, view.timeline.duration)
                         : snprintf(time, sizeof time, "%.3f seconds (duration unknown)",
                                    view.timeline.position);
        cui_str description =
            length > 0 && (size_t)length < sizeof time ? cui_string(time) : o.timeline_label;
        cui_id track = cui_video_part_id(id, CUI_VIDEO_PART_TIMELINE);
        cui_begin_box(ui, track,
                      (cui_box){.width = o.width,
                                .height = o.compact ? 44 : 32,
                                .fixed_width = true,
                                .fixed_height = true,
                                .background = o.compact ? (cui_color){0} : theme->surface,
                                .radius = theme->radius,
                                .border = theme->border,
                                .border_width = o.compact ? 0 : 1});
        bool seek = ready && (view.capabilities & CUI_VIDEO_CAP_SEEK) && view.timeline.seekable &&
                    view.timeline.duration_known && view.timeline.duration > 0;
        cui_base_slider_controlled(
            ui, track, o.timeline_label, view.timeline.position, &state->scrub,
            (cui_slider_options){.maximum = view.timeline.duration_known ? view.timeline.duration
                                                                         : view.timeline.position,
                                 .step = o.seek_step,
                                 .disabled = o.disabled || !seek,
                                 .readonly = !o.on_request || state->sequence == UINT64_MAX,
                                 .value_description = description,
                                 .on_request = o.on_request ? seek_request : NULL,
                                 .user = c});
        double shown = state->scrub.dragging ? state->scrub.preview : view.timeline.position;
        float width = view.timeline.duration > 0
                          ? (float)fmax(0, fmin(1, shown / view.timeline.duration)) * o.width
                          : 0;
        if (o.compact) {
            cui_begin_box(ui, 0,
                          (cui_box){.width = o.width,
                                    .height = 44,
                                    .fixed_width = true,
                                    .fixed_height = true,
                                    .align_y = CUI_ALIGN_CENTER});
            cui_begin_box(ui, 0,
                          (cui_box){.width = o.width,
                                    .height = 4,
                                    .fixed_width = true,
                                    .fixed_height = true,
                                    .background = theme->border,
                                    .radius = 2});
        }
        if (width > 0) {
            cui_begin_box(ui, 0,
                          (cui_box){.width = width,
                                    .height = o.compact ? 4 : 30,
                                    .fixed_width = true,
                                    .fixed_height = true,
                                    .background = theme->primary,
                                    .radius = o.compact ? 2 : theme->radius});
            cui_end_box(ui);
        }
        if (o.compact) {
            cui_end_box(ui);
            cui_end_box(ui);
        }
        cui_end_box(ui);
        if (!o.compact)
            cui_text(ui, description, theme->font_size, theme->text);
    } else
        state->scrub = (cui_slider_state){0};
}
bool cui_video_controls(cui_context *ui, cui_id id, const cui_theme *theme,
                        cui_video_controls_state *state, cui_video_view view,
                        cui_video_controls_options o) {
    if (!ui || !cui_frame_is_submitting(ui) || !id || !theme || !state || !view_valid(view) ||
        !options_valid(o))
        return false;
    request_context c = {.state = state,
                         .view = view,
                         .options = o,
                         .row_capacity = (unsigned)fmin(5, floor((o.width + 4) / 48))};
    if (state->source_id != view.source_id || state->generation != view.timeline.generation) {
        state->scrub = (cui_slider_state){0};
        state->source_id = view.source_id;
        state->generation = view.timeline.generation;
    }
    bool ready = view.status == CUI_VIDEO_READY;
    cui_begin_box(ui, id, (cui_box){.width = o.width, .fixed_width = true, .gap = 6});
    if (o.compact)
        timeline(ui, id, theme, &c);
    cui_begin_box(
        ui, 0,
        (cui_box){
            .row = true, .gap = 4, .width = o.compact ? o.width : 0, .fixed_width = o.compact});
    if (o.visible & CUI_VIDEO_CONTROL_PLAY)
        button(ui, cui_video_part_id(id, CUI_VIDEO_PART_PLAY), theme, &c,
               view.timeline.playing ? o.pause_label : o.play_label,
               view.timeline.playing ? o.pause_icon : o.play_icon,
               ready && (view.capabilities & CUI_VIDEO_CAP_PLAY),
               (cui_video_request){.kind = CUI_VIDEO_PLAYING,
                                   .value.enabled = !view.timeline.playing});
    if (o.visible & CUI_VIDEO_CONTROL_SKIP) {
        bool seek = ready && (view.capabilities & CUI_VIDEO_CAP_SEEK) && view.timeline.seekable;
        button(ui, cui_video_part_id(id, CUI_VIDEO_PART_BACK), theme, &c, o.back_label, o.back_icon,
               seek, (cui_video_request){.kind = CUI_VIDEO_SKIP, .value.seconds = -o.skip_seconds});
        button(ui, cui_video_part_id(id, CUI_VIDEO_PART_FORWARD), theme, &c, o.forward_label,
               o.forward_icon, seek,
               (cui_video_request){.kind = CUI_VIDEO_SKIP, .value.seconds = o.skip_seconds});
    }
    if (o.visible & CUI_VIDEO_CONTROL_FULLSCREEN)
        button(
            ui, cui_video_part_id(id, CUI_VIDEO_PART_FULLSCREEN), theme, &c,
            view.fullscreen ? o.leave_fullscreen_label : o.fullscreen_label,
            view.fullscreen ? o.leave_fullscreen_icon : o.fullscreen_icon,
            (view.capabilities & CUI_VIDEO_CAP_FULLSCREEN) != 0,
            (cui_video_request){.kind = CUI_VIDEO_FULLSCREEN, .value.enabled = !view.fullscreen});
    if (o.visible & CUI_VIDEO_CONTROL_RATE)
        button(ui, cui_video_part_id(id, CUI_VIDEO_PART_RATE), theme, &c, o.rate_label, o.rate_icon,
               ready && (view.capabilities & CUI_VIDEO_CAP_RATE),
               (cui_video_request){.kind = CUI_VIDEO_RATE, .value.rate = o.requested_rate});
    if (o.compact && (o.visible & CUI_VIDEO_CONTROL_TIMELINE)) {
        char position[32], duration[32], text[72];
        short_time(position, sizeof position,
                   state->scrub.dragging ? state->scrub.preview : view.timeline.position);
        short_time(duration, sizeof duration, view.timeline.duration);
        snprintf(text, sizeof text, "%s / %s", position,
                 view.timeline.duration_known ? duration : "--:--");
        float available = o.width - c.row_items * 48;
        float text_width = cui_measure_text(ui, cui_string(text), 12).width;
        if (c.row_items > c.row_capacity || available < text_width) {
            cui_end_box(ui);
            cui_begin_box(ui, 0, (cui_box){.width = o.width, .fixed_width = true});
            available = o.width;
        }
        cui_begin_box(ui, 0,
                      (cui_box){.width = available,
                                .fixed_width = true,
                                .height = 44,
                                .align_y = CUI_ALIGN_CENTER,
                                .clip = true});
        cui_text(ui, cui_string(text), 12, theme->text);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    if (!o.compact)
        timeline(ui, id, theme, &c);
    if (view.status_description.length) {
        cui_id status = id ^ UINT64_C(0x27cb57021ee10201);
        cui_interact(ui, status, CUI_ROLE_LABEL, view.status_description, false, false, 0);
        cui_set_semantic_details(ui, status,
                                 (cui_semantic_details){.busy = view.status == CUI_VIDEO_LOADING ||
                                                                view.status == CUI_VIDEO_BUFFERING,
                                                        .no_focus = true});
        cui_begin_box(ui, status, (cui_box){0});
        cui_text(ui, view.status_description, theme->font_size, theme->text);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    return true;
}
bool cui_video_player(cui_context *ui, cui_id id, const cui_theme *theme,
                      cui_video_controls_state *state, cui_video_view view,
                      const cui_video_player_frame *frame, cui_video_player_options o) {
    cui_image_layout layout;
    if (!ui || !cui_frame_is_submitting(ui) || !id || !theme || !state || !view_valid(view) ||
        (o.controls && !options_valid(o.controls_options)) || !isfinite(o.surface.opacity) ||
        o.surface.opacity < 0 || o.surface.opacity > 1 || !str_valid(o.surface.description) ||
        !cui_image_layout_resolve(o.surface.geometry, frame ? frame->frame.bitmap.width : 1,
                                  frame ? frame->frame.bitmap.height : 1, &layout) ||
        (frame &&
         (frame->source_id != view.source_id ||
          frame->frame.generation != view.timeline.generation ||
          !cui_bitmap_valid(frame->frame.bitmap) || !isfinite(frame->frame.timestamp) ||
          frame->frame.timestamp < 0 ||
          (view.timeline.duration_known && frame->frame.timestamp > view.timeline.duration))))
        return false;
    cui_begin_box(ui, id, (cui_box){.gap = 6});
    cui_id surface = cui_video_part_id(id, CUI_VIDEO_PART_SURFACE);
    bool ok = true;
    if (frame)
        ok = cui_video_surface(ui, surface, &view.timeline, &frame->frame, o.surface);
    else {
        cui_begin_box(ui, surface,
                      (cui_box){.width = layout.width,
                                .height = layout.height,
                                .fixed_width = true,
                                .fixed_height = true});
        cui_end_box(ui);
    }
    if (o.controls)
        ok = cui_video_controls(ui, id ^ UINT64_C(0x27cb57021ee10301), theme, state, view,
                                o.controls_options) &&
             ok;
    cui_end_box(ui);
    return ok;
}
