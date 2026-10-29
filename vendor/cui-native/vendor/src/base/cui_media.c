/* SPDX-License-Identifier: MIT */
#include "cui/cui_media.h"
#include "cui_design_metrics.h"
#include <float.h>
#include <math.h>
#include <string.h>
bool cui_image_frame_valid(cui_image_frame f) {
    return f.pixels && f.width && f.height && f.stride / 4 >= f.width &&
           f.height <= SIZE_MAX / f.stride && isfinite(f.duration) && f.duration >= 0;
}
bool cui_image_layout_resolve(cui_image_geometry g, uint32_t iw, uint32_t ih,
                              cui_image_layout *out) {
    if (!out || !isfinite(g.width) || !isfinite(g.height) || !isfinite(g.aspect_ratio) ||
        g.width < 0 || g.height < 0 || g.aspect_ratio < 0 || g.fit < CUI_IMAGE_CONTAIN ||
        g.fit > CUI_IMAGE_NONE)
        return false;
    double ratio = g.aspect_ratio ? g.aspect_ratio : ih ? (double)iw / ih : 0;
    double w = g.width, h = g.height;
    if (!w && h && ratio)
        w = h * ratio;
    if (!h && w && ratio)
        h = w / ratio;
    if (!w && !h) {
        w = iw;
        h = ih;
    }
    if (!isfinite(w) || !isfinite(h) || w > FLT_MAX || h > FLT_MAX)
        return false;
    cui_image_layout r = {.width = (float)w, .height = (float)h};
    if (iw && ih && w && h) {
        double dw = w, dh = h, sw = iw, sh = ih;
        if (g.fit == CUI_IMAGE_COVER) {
            double scale = fmax(w / iw, h / ih);
            sw = w / scale;
            sh = h / scale;
        } else if (g.fit != CUI_IMAGE_FILL) {
            double scale = g.fit == CUI_IMAGE_NONE ? 1 : fmin(w / iw, h / ih);
            if (g.fit == CUI_IMAGE_SCALE_DOWN)
                scale = fmin(scale, 1);
            dw = iw * scale;
            dh = ih * scale;
            /* NONE may overflow: crop the centered natural image to its box. */
            sw = fmin(dw, w) / scale;
            sh = fmin(dh, h) / scale;
            dw = fmin(dw, w);
            dh = fmin(dh, h);
        }
        r.source = (cui_rect){(float)((iw - sw) / 2), (float)((ih - sh) / 2), (float)sw, (float)sh};
        r.destination =
            (cui_rect){(float)((w - dw) / 2), (float)((h - dh) / 2), (float)dw, (float)dh};
    }
    *out = r;
    return true;
}
void cui_image_state_reset(cui_image_state *s) {
    if (s)
        memset(s, 0, sizeof(*s));
}
cui_image_result cui_image_update(cui_image_state *s, cui_image_source source,
                                  cui_image_loader_fn loader, void *user, cui_image_clock clock) {
    cui_image_result r = {0};
    if (!s || !loader || !isfinite(clock.now_seconds) || clock.now_seconds < 0 ||
        source.kind < CUI_IMAGE_ASSET || source.kind > CUI_IMAGE_CUSTOM ||
        (source.name.length && !source.name.data) || (source.byte_count && !source.bytes))
        return r;
    bool reset = !s->initialized || s->identity != source.identity;
    if (reset) {
        cui_image_state_reset(s);
        s->identity = source.identity;
        s->started = s->last_time = clock.now_seconds;
        s->initialized = true;
        r.changed = true;
    }
    cui_image_status old = s->status;
    size_t previous = s->frame;
    if (s->status == CUI_IMAGE_PENDING || source.kind == CUI_IMAGE_CUSTOM) {
        cui_image_load load = loader(user, source);
        if (load.status < CUI_IMAGE_PENDING || load.status > CUI_IMAGE_FAILED)
            return r;
        if (load.status == CUI_IMAGE_READY) {
            if (!load.frames || !load.count || load.count > 4096)
                load.status = CUI_IMAGE_FAILED;
            else
                for (size_t i = 0; i < load.count; i++)
                    if (!cui_image_frame_valid(load.frames[i])) {
                        load.status = CUI_IMAGE_FAILED;
                        break;
                    }
        }
        if (load.status == CUI_IMAGE_READY) {
            double duration = 0;
            for (size_t i = 0; i < load.count; i++)
                duration += fmax(.01, load.frames[i].duration);
            if (!isfinite(duration))
                load.status = CUI_IMAGE_FAILED;
        }
        if (load.status == CUI_IMAGE_PENDING && s->status != CUI_IMAGE_PENDING)
            s->started = clock.now_seconds;
        bool fresh =
            s->status != CUI_IMAGE_READY || s->frames != load.frames || s->count != load.count;
        s->status = load.status;
        if (load.status == CUI_IMAGE_READY && fresh) {
            s->frames = load.frames;
            s->count = load.count;
            s->last_time = clock.now_seconds;
            s->elapsed = 0;
            s->frame = 0;
            r.changed = true;
        }
    }
    bool loading_was_visible =
        s->status == CUI_IMAGE_PENDING && clock.identified && s->last_time - s->started >= .2;
    double dt = fmax(0, clock.now_seconds - s->last_time);
    s->last_time = fmax(s->last_time, clock.now_seconds);
    if (!clock.identified)
        s->frame = 0;
    if (s->status == CUI_IMAGE_READY && s->count > 1 && clock.identified && clock.active &&
        !clock.reduced_motion) {
        double total = 0;
        for (size_t i = 0; i < s->count; i++)
            total += fmax(.01, s->frames[i].duration);
        if (isfinite(total)) {
            s->elapsed = fmod(s->elapsed, total);
            double delta = fmod(dt, total);
            s->elapsed =
                delta >= total - s->elapsed ? delta - (total - s->elapsed) : s->elapsed + delta;
            double at = s->elapsed;
            s->frame = 0;
            while (s->frame + 1 < s->count && at >= fmax(.01, s->frames[s->frame].duration)) {
                at -= fmax(.01, s->frames[s->frame].duration);
                s->frame++;
            }
            r.next_wake_seconds =
                fmin(DBL_MAX, clock.now_seconds + (fmax(.01, s->frames[s->frame].duration) - at));
        }
    }
    r.valid = true;
    r.status = s->status;
    r.changed |= old != s->status || previous != s->frame;
    r.loading_visible =
        s->status == CUI_IMAGE_PENDING && clock.identified && clock.now_seconds - s->started >= .2;
    r.changed |= loading_was_visible != r.loading_visible;
    if (s->status == CUI_IMAGE_PENDING && clock.identified && !r.loading_visible)
        r.next_wake_seconds = s->started + .2;
    if (s->status == CUI_IMAGE_READY)
        r.frame = &s->frames[s->frame];
    return r;
}
bool cui_attachment_in_progress(cui_attachment_status status) {
    return status == CUI_ATTACHMENT_UPLOADING || status == CUI_ATTACHMENT_PROCESSING;
}
bool cui_attachment_metrics_resolve(cui_media_size size, float font, float density,
                                    cui_attachment_metrics *out) {
    static const float table[4][10] = {
        {3.5f, 14.5f, 2.375f, 1.25f, .5f, .75f, .5f, .625f, .8125f, .75f},
        {2.5f, 11, 1.75f, .875f, .25f, .375f, .25f, .375f, .6875f, .625f},
        {3, 12.5f, 2, 1, .375f, .5f, .375f, .5f, .75f, .6875f},
        {4, 17, 2.75f, 1.5f, .625f, 1, .75f, .75f, .875f, .8125f}};
    if (!out || size < CUI_MEDIA_MEDIUM || size > CUI_MEDIA_CUSTOM || !isfinite(font) ||
        font <= 0 || !isfinite(density) || density <= 0)
        return false;
    float scale = size == CUI_MEDIA_CUSTOM ? density : font;
    const float *v = table[size == CUI_MEDIA_CUSTOM ? 0 : size];
    float values[10];
    for (size_t i = 0; i < 10; i++) {
        values[i] = v[i] * scale;
        if (!isfinite(values[i]))
            return false;
    }
    *out = (cui_attachment_metrics){values[0], values[1], values[2], values[3], values[4],
                                    values[5], values[6], values[7], values[8], values[9]};
    return true;
}
cui_attachment_model cui_attachment_model_default(void) {
    return (cui_attachment_model){.status = CUI_ATTACHMENT_COMPLETE,
                                  .size = CUI_MEDIA_MEDIUM,
                                  .root_font_size = 16,
                                  .custom_density = 16,
                                  .media_aspect = 1};
}
static bool cui_media_status_valid(cui_attachment_status status) {
    return status >= CUI_ATTACHMENT_PENDING && status <= CUI_ATTACHMENT_COMPLETE;
}
bool cui_base_attachment(cui_attachment_model m, cui_attachment_plan *out) {
    if (!out || !cui_media_status_valid(m.status) ||
        (m.title_status_override && !cui_media_status_valid(m.title_status)) ||
        (m.description_status_override && !cui_media_status_valid(m.description_status)) ||
        !isfinite(m.width) || m.width < 0 || !isfinite(m.media_aspect) || m.media_aspect <= 0 ||
        (m.progress_known && !isfinite(m.progress)))
        return false;
    cui_attachment_plan p = {0};
    if (!cui_attachment_metrics_resolve(m.size, m.root_font_size, m.custom_density, &p.metrics))
        return false;
    if (!cui_design_valid(m.design_size))
        return false;
    if (m.design_size) {
        cui_design_metrics d;
        float density = m.size == CUI_MEDIA_CUSTOM ? m.custom_density : m.root_font_size / 16;
        if (!cui_design_resolve(m.design_size, density, &d))
            return false;
        p.metrics = (cui_attachment_metrics){.height = d.control_height,
                                             .chip_width = 8 * d.font_size,
                                             .media = d.control_height,
                                             .glyph = d.indicator_size,
                                             .padding_start = d.padding_x,
                                             .padding_end = d.padding_x,
                                             .card_padding = d.padding_x,
                                             .gap = d.gap,
                                             .text = d.font_size,
                                             .description = d.font_size * .875f};
    }
    p.flush = m.vertical && !m.content;
    p.overhang = m.identified && m.remove ? 6 : 0;
    p.remove_visible = p.overhang && (m.touch || m.hovered);
    bool retry = m.identified && m.retry && m.status == CUI_ATTACHMENT_FAILED;
    p.retry_media = retry && m.media;
    p.retry_description = retry && m.content;
    p.progress = fminf(100, fmaxf(0, m.progress));
    p.progress_ring = m.media && m.progress_known && m.status == CUI_ATTACHMENT_UPLOADING;
    p.progress_bar = !m.vertical && m.progress_known && m.status == CUI_ATTACHMENT_UPLOADING;
    p.append_percentage = m.content && m.progress_known && m.status == CUI_ATTACHMENT_UPLOADING &&
                          !p.retry_description;
    p.spinner = m.media && cui_attachment_in_progress(m.status) && !p.progress_ring;
    p.rejection = m.media && m.status == CUI_ATTACHMENT_FAILED && !retry;
    p.scrim = !m.image                               ? 0
              : m.status == CUI_ATTACHMENT_FAILED    ? .55f
              : cui_attachment_in_progress(m.status) ? .35f
                                                     : 0;
    p.title_loading =
        cui_attachment_in_progress(m.title_status_override ? m.title_status : m.status);
    p.description_failed =
        (m.description_status_override ? m.description_status : m.status) == CUI_ATTACHMENT_FAILED;
    p.dashed = m.status == CUI_ATTACHMENT_PENDING;
    if (!m.vertical) {
        p.height = p.metrics.height;
        if (m.design_size && m.content)
            p.height = fmaxf(p.height, 1.25f * (p.metrics.text + p.metrics.description) + 2);
        p.width = m.content ? p.metrics.chip_width
                            : (m.media ? p.metrics.media + p.metrics.padding_start
                                       : p.metrics.padding_end) +
                                  p.metrics.padding_end;
        p.media_width = p.media_height = m.media ? p.metrics.media : 0;
    } else {
        p.width =
            p.flush ? p.metrics.height : 7.5f * (m.design_size ? p.metrics.text : m.root_font_size);
        if (m.width)
            p.width = m.width;
        p.media_width = m.media ? p.width - (p.flush ? 2 : 2 * p.metrics.card_padding) : 0;
        p.media_height = p.media_width / m.media_aspect;
        p.height = p.flush ? p.metrics.height : p.media_height + 2 * p.metrics.card_padding;
    }
    if (m.width)
        p.width = m.width;
    if (!isfinite(p.width) || !isfinite(p.height) || !isfinite(p.media_height) || p.media_width < 0)
        return false;
    *out = p;
    return true;
}

cui_image_options cui_image_options_default(void) {
    return (cui_image_options){.geometry = {.fit = CUI_IMAGE_CONTAIN},
                               .opacity = 1,
                               .clock = {.identified = true, .active = true}};
}
cui_attachment_options cui_attachment_options_default(void) {
    return (cui_attachment_options){.model = cui_attachment_model_default(),
                                    .remove_label = {"Remove attachment", 17},
                                    .retry_label = {"Retry", 5},
                                    .active = true};
}
cui_attachment_group_options cui_attachment_group_options_default(void) {
    return (cui_attachment_group_options){.width = 480, .gap = 12, .wheel_step = 48};
}
