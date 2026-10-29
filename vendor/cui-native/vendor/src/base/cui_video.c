/* SPDX-License-Identifier: MIT */
#include "cui/cui_video.h"
#include <float.h>
#include <math.h>
static bool valid(const cui_video *s) {
    return s && isfinite(s->position) && s->position >= 0 && isfinite(s->duration) &&
           s->duration >= 0 &&
           (s->duration_known ? s->position <= s->duration : s->duration == 0) &&
           isfinite(s->rate) && s->rate > 0 && s->generation && !(s->playing && s->ended);
}
bool cui_video_init(cui_video *s, cui_video_config c) {
    if (!s || !isfinite(c.duration) || c.duration < 0 || (!c.duration_known && c.duration != 0))
        return false;
    *s = (cui_video){.duration = c.duration,
                     .duration_known = c.duration_known,
                     .seekable = c.seekable,
                     .loop = c.loop,
                     .rate = 1,
                     .generation = 1};
    return true;
}
bool cui_video_set_playing(cui_video *s, bool playing) {
    if (!valid(s))
        return false;
    cui_video next = *s;
    if (playing && next.ended) {
        if (!next.seekable || next.generation == UINT64_MAX)
            return false;
        next.position = 0;
        ++next.generation;
        next.ended = false;
    }
    next.playing = playing;
    if (playing && next.duration_known && next.duration == 0) {
        next.playing = false;
        next.ended = true;
    }
    *s = next;
    return true;
}
bool cui_video_set_rate(cui_video *s, double rate) {
    if (!valid(s) || !isfinite(rate) || rate <= 0)
        return false;
    s->rate = rate;
    return true;
}
bool cui_video_set_loop(cui_video *s, bool loop) {
    if (!valid(s))
        return false;
    s->loop = loop;
    return true;
}
bool cui_video_seek(cui_video *s, double position) {
    if (!valid(s) || !s->seekable || !isfinite(position) || position < 0 ||
        (s->duration_known && position > s->duration) || s->generation == UINT64_MAX)
        return false;
    s->position = position;
    s->ended = s->duration_known && position == s->duration;
    if (s->ended)
        s->playing = false;
    ++s->generation;
    return true;
}
bool cui_video_advance(cui_video *s, double elapsed) {
    if (!valid(s) || !isfinite(elapsed) || elapsed < 0)
        return false;
    if (!s->playing || elapsed == 0)
        return true;
    if (elapsed > DBL_MAX / s->rate)
        return false;
    double delta = elapsed * s->rate;
    cui_video next = *s;
    if (s->duration_known && delta >= s->duration - s->position) {
        if (s->loop && s->seekable && s->duration > 0) {
            if (s->generation == UINT64_MAX)
                return false;
            /* Reduce before adding to avoid overflow for huge elapsed times. */
            double reduced = fmod(delta, s->duration);
            double remaining = s->duration - s->position;
            next.position = reduced >= remaining ? reduced - remaining : s->position + reduced;
            ++next.generation;
        } else {
            next.position = s->duration;
            next.playing = false;
            next.ended = true;
        }
    } else {
        if (delta > DBL_MAX - s->position)
            return false;
        next.position += delta;
    }
    *s = next;
    return true;
}
bool cui_video_finish(cui_video *s) {
    if (!valid(s))
        return false;
    if (s->duration_known)
        s->position = s->duration;
    s->playing = false;
    s->ended = true;
    return true;
}
bool cui_video_surface(cui_context *ui, cui_id id, const cui_video *s, const cui_video_frame *frame,
                       cui_video_surface_options o) {
    cui_image_layout layout;
    if (!ui || !id || !valid(s) || !frame || frame->generation != s->generation ||
        !isfinite(frame->timestamp) || frame->timestamp < 0 ||
        (s->duration_known && frame->timestamp > s->duration) || !cui_bitmap_valid(frame->bitmap) ||
        !isfinite(o.opacity) || o.opacity < 0 || o.opacity > 1 ||
        (o.description.length && !o.description.data) ||
        !cui_image_layout_resolve(o.geometry, frame->bitmap.width, frame->bitmap.height, &layout))
        return false;
    if (o.description.length)
        cui_interact(ui, id, CUI_ROLE_LABEL, o.description, false, false, 0);
    cui_begin_box(ui, id,
                  (cui_box){.width = layout.width,
                            .height = layout.height,
                            .fixed_width = true,
                            .fixed_height = true,
                            .clip = true,
                            .row = true});
    cui_begin_box(ui, 0,
                  (cui_box){.width = layout.destination.x,
                            .fixed_width = true,
                            .height = layout.height,
                            .fixed_height = true});
    cui_end_box(ui);
    cui_begin_box(ui, 0,
                  (cui_box){.width = layout.destination.w,
                            .fixed_width = true,
                            .height = layout.height,
                            .fixed_height = true});
    cui_begin_box(ui, 0,
                  (cui_box){.height = layout.destination.y,
                            .fixed_height = true,
                            .width = layout.destination.w,
                            .fixed_width = true});
    cui_end_box(ui);
    bool painted = cui_paint_bitmap(
        ui, 0,
        (cui_bitmap_paint){.bitmap = frame->bitmap, .source = layout.source, .opacity = o.opacity},
        (cui_box){.width = layout.destination.w,
                  .height = layout.destination.h,
                  .fixed_width = true,
                  .fixed_height = true});
    cui_end_box(ui);
    cui_end_box(ui);
    return painted;
}
