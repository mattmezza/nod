/* SPDX-License-Identifier: MIT */
#include "cui/cui_speech.h"
#include "../base/cui_design_metrics.h"
#include "cui/cui_components.h"
#include <math.h>
static bool cui_speech_capturing(const cui_speech_state *s) {
    return s->status == CUI_SPEECH_CONNECTING || s->status == CUI_SPEECH_RECORDING;
}
bool cui_speech_button(cui_context *ui, cui_id id, const cui_theme *theme, cui_speech_state *s,
                       cui_speech_button_options o, double now) {
    if (!ui || !id || !theme || !cui_design_valid(o.design_size) || !s || !s->initialized ||
        !isfinite(now) || o.size < CUI_SPEECH_MEDIUM || o.size > CUI_SPEECH_XSMALL ||
        (o.start_label.length && !o.start_label.data) ||
        (o.stop_label.length && !o.stop_label.data) ||
        (o.unavailable_label.length && !o.unavailable_label.data))
        return false;
    if (!cui_speech_has_recognizer(s) && !o.show_when_unsupported)
        return false;
    bool available = s->status != CUI_SPEECH_IDLE || cui_speech_available(s);
    bool capturing = cui_speech_capturing(s);
    cui_str label = !available ? o.unavailable_label : capturing ? o.stop_label : o.start_label;
    if (!label.length)
        label = cui_string(!available  ? "Speech unavailable"
                           : capturing ? "Stop dictation"
                                       : "Start dictation");
    cui_control_size size = CUI_CONTROL_MEDIUM;
    if (o.size == CUI_SPEECH_SMALL)
        size = CUI_CONTROL_SMALL;
    else if (o.size == CUI_SPEECH_LARGE)
        size = CUI_CONTROL_LARGE;
    else if (o.size == CUI_SPEECH_XSMALL)
        size = CUI_CONTROL_XSMALL;
    bool pressed =
        cui_button_ex(ui, id, theme, label,
                      (cui_button_options){.variant = CUI_BUTTON_GHOST,
                                           .size = size,
                                           .disabled = o.disabled || !available,
                                           .loading = s->status == CUI_SPEECH_STOPPING,
                                           .selected = capturing,
                                           .icon = cui_string(capturing ? "square" : "mic"),
                                           .icon_only = true,
                                           .design_size = o.design_size});
    if (pressed) {
        (void)cui_speech_toggle(s, now);
        cui_invalidate(ui);
    }
    return pressed;
}
bool cui_speech_waveform(cui_context *ui, cui_id id, const cui_theme *theme,
                         const cui_speech_state *s, cui_speech_waveform_options o, double now) {
    if (!ui || !id || !theme || !cui_design_valid(o.design_size) || !s || !s->initialized ||
        !isfinite(now) || !isfinite(o.width) || !isfinite(o.height) || o.width < 0 ||
        o.height < 0 || o.size < CUI_SPEECH_MEDIUM || o.size > CUI_SPEECH_XSMALL)
        return false;
    static const float heights[] = {20, 16, 24, 12};
    float width = o.width ? o.width : 96, height = o.height ? o.height : heights[o.size];
    if (o.design_size) {
        cui_design_metrics d;
        if (!cui_design_resolve(o.design_size, theme->font_size / 16, &d))
            return false;
        if (!o.width)
            width = 6 * d.font_size;
        if (!o.height)
            height = d.control_height;
    }
    bool capturing = cui_speech_capturing(s),
         animate = capturing && !o.reduced_motion && !cui_reduced_motion(ui);
    float lead = animate ? cui_speech_meter_lead(&s->meter, now) : 0;
    cui_rect rects[256];
    size_t count = cui_speech_waveform_rects(&s->meter, width, height, lead, rects, 256);
    if (count == SIZE_MAX)
        return false;
    cui_color color = o.custom_color ? o.color : capturing ? theme->primary : theme->muted;
    cui_begin_box(ui, id, (cui_box){.width = width, .height = height, .clip = true});
    for (size_t i = 0; i < count; i++) {
        cui_rect r = rects[i];
        cui_begin_absolute_box(ui, 0,
                               (cui_box){.background = color, .radius = fminf(r.w, r.h) * .5f}, r);
        cui_end_box(ui);
    }
    cui_end_box(ui);
    if (animate)
        cui_invalidate(ui);
    return true;
}
