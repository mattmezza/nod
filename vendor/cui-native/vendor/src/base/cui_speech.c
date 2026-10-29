/* SPDX-License-Identifier: MIT */
#include "cui/cui_speech.h"
#include <math.h>
#include <string.h>
enum {
    CUI_SPEECH_MSG_READY,
    CUI_SPEECH_MSG_HYPOTHESIS,
    CUI_SPEECH_MSG_PHRASE,
    CUI_SPEECH_MSG_FINISH,
    CUI_SPEECH_MSG_ERROR,
    CUI_SPEECH_MSG_AUDIO
};
static bool cui_speech_utf8(cui_str text) {
    if (text.length && !text.data)
        return false;
    for (size_t i = 0; i < text.length;) {
        unsigned c = (unsigned char)text.data[i++], n, value, minimum;
        if (c < 128)
            continue;
        if (c >= 0xc2 && c <= 0xdf) {
            n = 1;
            value = c & 31;
            minimum = 128;
        } else if (c >= 0xe0 && c <= 0xef) {
            n = 2;
            value = c & 15;
            minimum = 2048;
        } else if (c >= 0xf0 && c <= 0xf4) {
            n = 3;
            value = c & 7;
            minimum = 65536;
        } else
            return false;
        if (n > text.length - i)
            return false;
        while (n--) {
            unsigned next = (unsigned char)text.data[i++];
            if ((next & 0xc0) != 0x80)
                return false;
            value = (value << 6) | (next & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            return false;
    }
    return true;
}
static bool cui_speech_format_valid(cui_speech_format f) {
    return f.sample_rate && f.sample_rate <= 768000 && f.channels && f.channels <= 256;
}
void cui_speech_meter_reset(cui_speech_meter *m, cui_speech_format f) {
    if (!m)
        return;
    if (!cui_speech_format_valid(f))
        f = (cui_speech_format){16000, 1};
    *m = (cui_speech_meter){.window = (size_t)((uint64_t)f.sample_rate * f.channels * 80 / 1000),
                            .pace = .08};
    if (!m->window)
        m->window = 1;
}
bool cui_speech_meter_push(cui_speech_meter *m, const int16_t *samples, size_t count, double now) {
    if (!m || !m->window || (count && !samples) || !isfinite(now))
        return false;
    bool changed = false;
    for (size_t i = 0; i < count; i++) {
        double sample = samples[i] / 32767.0;
        m->sum += sample * sample;
        if (++m->samples != m->window)
            continue;
        double rms = sqrt(m->sum / m->samples);
        float raw = rms > 0 ? (float)fmin(1, fmax(0, (20 * log10(rms) + 50) / 50)) : 0;
        if (raw < .06f)
            raw = 0;
        m->smoothed += (raw - m->smoothed) * (raw > m->smoothed ? .85f : .3f);
        if (m->length == 256)
            m->first = (m->first + 1) % 256;
        else
            m->length++;
        m->levels[(m->first + m->length - 1) % 256] = m->smoothed;
        m->sum = 0;
        m->samples = 0;
        m->recorded++;
        if (!m->anchored) {
            m->anchor = now - .08;
            m->anchored = true;
        } else {
            double elapsed = now - m->anchor;
            double error = elapsed - m->recorded * m->pace;
            double pace = fmin(.16, fmax(.04, m->pace + error * .01));
            double anchor = now - elapsed / m->pace * pace + error * .1;
            if (isfinite(anchor)) {
                m->anchor = anchor;
                m->pace = pace;
            }
        }
        changed = true;
    }
    return changed;
}
float cui_speech_meter_level(const cui_speech_meter *m, size_t index) {
    return m && index < m->length ? m->levels[(m->first + index) % 256] : 0;
}
float cui_speech_meter_lead(const cui_speech_meter *m, double now) {
    if (!m || !m->anchored || !isfinite(now) || m->pace <= 0)
        return 0;
    return (float)fmin(3, fmax(0, m->recorded - (now - m->anchor) / m->pace + 1.75));
}
static void cui_speech_emit(cui_speech_state *s, cui_speech_event_type type,
                            cui_speech_error error) {
    if (s->event)
        s->event(s->event_user, (cui_speech_event){type, error, cui_speech_transcript(s)});
}
cui_str cui_speech_transcript(const cui_speech_state *s) {
    return s && s->initialized ? (cui_str){s->storage.transcript, s->committed + s->hypothesis}
                               : (cui_str){0};
}
bool cui_speech_init(cui_speech_state *s, cui_speech_workspace w, cui_speech_recognizer r,
                     cui_speech_input input, cui_speech_event_fn event, void *user) {
    if (!s || s->initialized || !w.transcript || !w.transcript_capacity || !w.messages ||
        !w.message_capacity || !w.queue_bytes || !w.queue_capacity)
        return false;
    if (!r.format.sample_rate && !r.format.channels)
        r.format = (cui_speech_format){16000, 1};
    if (!cui_speech_format_valid(r.format) ||
        (r.start && (!r.close || !r.finish || !r.push_audio)) || (input.start && !input.close))
        return false;
    uint64_t previous_generation = s->generation;
    *s = (cui_speech_state){.storage = w,
                            .generation = previous_generation,
                            .recognizer = r,
                            .input = input,
                            .event = event,
                            .event_user = user,
                            .stop_timeout = 3,
                            .initialized = true};
    w.transcript[0] = 0;
    cui_speech_meter_reset(&s->meter, r.format);
    return true;
}
bool cui_speech_has_recognizer(const cui_speech_state *s) {
    return s && s->initialized && s->recognizer.start && s->input.start;
}
bool cui_speech_available(const cui_speech_state *s) {
    return cui_speech_has_recognizer(s) &&
           (!s->recognizer.available || s->recognizer.available(s->recognizer.user));
}
static void cui_speech_close(cui_speech_state *s) {
    void *capture = s->capture, *recognition = s->recognition;
    s->capture = NULL;
    s->recognition = NULL;
    s->accepting = false;
    s->status = CUI_SPEECH_IDLE;
    s->provider_call = true;
    if (capture)
        s->input.close(s->input.user, capture);
    if (recognition)
        s->recognizer.close(s->recognizer.user, recognition);
    s->provider_call = false;
    cui_speech_meter_reset(&s->meter, s->recognizer.format);
}
static void cui_speech_end(cui_speech_state *s, cui_speech_event_type type,
                           cui_speech_error error) {
    cui_speech_close(s);
    cui_speech_emit(s, type, error);
}
void cui_speech_dispose(cui_speech_state *s) {
    if (!s || !s->initialized || s->provider_call || s->pumping)
        return;
    cui_speech_close(s);
    uint64_t generation = s->generation;
    *s = (cui_speech_state){.generation = generation};
}
bool cui_speech_start(cui_speech_state *s) {
    if (!s || !s->initialized || s->provider_call || s->status != CUI_SPEECH_IDLE)
        return false;
    if (!cui_speech_has_recognizer(s)) {
        cui_speech_emit(s, CUI_SPEECH_ERROR, CUI_SPEECH_UNSUPPORTED);
        return false;
    }
    if (s->generation == UINT64_MAX) {
        cui_speech_emit(s, CUI_SPEECH_ERROR, CUI_SPEECH_CAPACITY);
        return false;
    }
    s->queued = s->queue_used = 0;
    s->queue_overflow = false;
    s->generation++;
    s->accepting = true;
    cui_speech_sink sink = {s, s->generation};
    s->provider_call = true;
    cui_speech_error error = s->recognizer.start(s->recognizer.user, sink, &s->recognition);
    if (error == CUI_SPEECH_OK && !s->recognition)
        error = CUI_SPEECH_RECOGNIZER_ERROR;
    if (error == CUI_SPEECH_OK) {
        error = s->input.start(s->input.user, s->recognizer.format, sink, &s->capture);
        if (error == CUI_SPEECH_OK && !s->capture)
            error = CUI_SPEECH_INPUT_ERROR;
    }
    s->provider_call = false;
    if (error != CUI_SPEECH_OK) {
        cui_speech_close(s);
        s->queued = s->queue_used = 0;
        cui_speech_emit(s, CUI_SPEECH_ERROR, error);
        return false;
    }
    s->committed = s->hypothesis = 0;
    s->storage.transcript[0] = 0;
    cui_speech_meter_reset(&s->meter, s->recognizer.format);
    s->status = CUI_SPEECH_CONNECTING;
    cui_speech_emit(s, CUI_SPEECH_STARTED, CUI_SPEECH_OK);
    return true;
}
bool cui_speech_set_stop_timeout(cui_speech_state *s, double seconds) {
    if (!s || !s->initialized || s->provider_call || !isfinite(seconds) || seconds < 0)
        return false;
    s->stop_timeout = seconds;
    return true;
}
bool cui_speech_stop(cui_speech_state *s, double now) {
    if (!s || !s->initialized || s->provider_call || !isfinite(now) ||
        !isfinite(now + s->stop_timeout) ||
        (s->status != CUI_SPEECH_CONNECTING && s->status != CUI_SPEECH_RECORDING))
        return false;
    s->status = CUI_SPEECH_STOPPING;
    s->deadline = now + s->stop_timeout;
    s->provider_call = true;
    void *capture = s->capture;
    s->capture = NULL;
    if (capture)
        s->input.close(s->input.user, capture);
    s->recognizer.finish(s->recognizer.user, s->recognition);
    s->provider_call = false;
    return true;
}
bool cui_speech_cancel(cui_speech_state *s) {
    if (!s || !s->initialized || s->provider_call || s->status == CUI_SPEECH_IDLE)
        return false;
    s->committed = s->hypothesis = 0;
    s->storage.transcript[0] = 0;
    cui_speech_end(s, CUI_SPEECH_CANCELLED, CUI_SPEECH_OK);
    return true;
}
bool cui_speech_toggle(cui_speech_state *s, double now) {
    return s && s->status != CUI_SPEECH_IDLE ? cui_speech_stop(s, now) : cui_speech_start(s);
}
static bool cui_speech_enqueue(cui_speech_sink sink, unsigned kind, const void *data, size_t bytes,
                               cui_speech_error error, double time) {
    cui_speech_state *s = sink.state;
    if (!s || !s->initialized || !s->accepting || sink.generation != s->generation ||
        (bytes && !data))
        return false;
    if (s->queued == s->storage.message_capacity ||
        bytes > s->storage.queue_capacity - s->queue_used) {
        s->queue_overflow = true;
        return false;
    }
    if (bytes)
        memmove(s->storage.queue_bytes + s->queue_used, data, bytes);
    s->storage.messages[s->queued++] =
        (cui_speech_message){kind, sink.generation, s->queue_used, bytes, error, time};
    s->queue_used += bytes;
    return true;
}
bool cui_speech_report_ready(cui_speech_sink sink) {
    return cui_speech_enqueue(sink, CUI_SPEECH_MSG_READY, NULL, 0, CUI_SPEECH_OK, 0);
}
bool cui_speech_report_finish(cui_speech_sink sink) {
    return cui_speech_enqueue(sink, CUI_SPEECH_MSG_FINISH, NULL, 0, CUI_SPEECH_OK, 0);
}
bool cui_speech_report_error(cui_speech_sink sink, cui_speech_error error) {
    return error > CUI_SPEECH_OK && error <= CUI_SPEECH_INVALID &&
           cui_speech_enqueue(sink, CUI_SPEECH_MSG_ERROR, NULL, 0, error, 0);
}
bool cui_speech_report_hypothesis(cui_speech_sink sink, cui_str text) {
    return cui_speech_utf8(text) && cui_speech_enqueue(sink, CUI_SPEECH_MSG_HYPOTHESIS, text.data,
                                                       text.length, CUI_SPEECH_OK, 0);
}
bool cui_speech_report_phrase(cui_speech_sink sink, cui_str text) {
    return cui_speech_utf8(text) && cui_speech_enqueue(sink, CUI_SPEECH_MSG_PHRASE, text.data,
                                                       text.length, CUI_SPEECH_OK, 0);
}
bool cui_speech_report_audio(cui_speech_sink sink, const int16_t *samples, size_t count,
                             double now) {
    return isfinite(now) && count <= SIZE_MAX / sizeof *samples &&
           cui_speech_enqueue(sink, CUI_SPEECH_MSG_AUDIO, samples, count * sizeof *samples,
                              CUI_SPEECH_OK, now);
}
bool cui_speech_pump(cui_speech_state *s, double now) {
    if (!s || !s->initialized || s->provider_call || s->pumping || !isfinite(now))
        return false;
    s->pumping = true;
    bool changed = false;
    /* Bound one pump to its initial queue length: providers can enqueue again. */
    size_t remaining = s->queued;
    while (remaining-- && s->queued) {
        uint64_t processing_generation = s->generation;
        cui_speech_message m = s->storage.messages[0];
        const unsigned char *data = s->storage.queue_bytes + m.offset;
        if (s->accepting && m.generation == s->generation) {
            changed = true;
            switch (m.kind) {
            case CUI_SPEECH_MSG_READY:
                if (s->status == CUI_SPEECH_CONNECTING)
                    s->status = CUI_SPEECH_RECORDING;
                break;
            case CUI_SPEECH_MSG_HYPOTHESIS:
            case CUI_SPEECH_MSG_PHRASE:
                if (m.bytes >= s->storage.transcript_capacity - s->committed) {
                    cui_speech_end(s, CUI_SPEECH_ERROR, CUI_SPEECH_CAPACITY);
                    break;
                }
                memmove(s->storage.transcript + s->committed, data, m.bytes);
                if (m.kind == CUI_SPEECH_MSG_PHRASE) {
                    s->committed += m.bytes;
                    s->hypothesis = 0;
                } else
                    s->hypothesis = m.bytes;
                s->storage.transcript[s->committed + s->hypothesis] = 0;
                cui_speech_emit(s, CUI_SPEECH_PARTIAL, CUI_SPEECH_OK);
                break;
            case CUI_SPEECH_MSG_FINISH:
                cui_speech_end(s, CUI_SPEECH_FINAL, CUI_SPEECH_OK);
                break;
            case CUI_SPEECH_MSG_ERROR:
                cui_speech_end(s, CUI_SPEECH_ERROR, m.error);
                break;
            case CUI_SPEECH_MSG_AUDIO:
                if (s->status == CUI_SPEECH_CONNECTING || s->status == CUI_SPEECH_RECORDING) {
                    size_t offset = 0;
                    while (offset < m.bytes) {
                        int16_t samples[256];
                        size_t bytes = m.bytes - offset;
                        if (bytes > sizeof samples)
                            bytes = sizeof samples;
                        memcpy(samples, data + offset, bytes);
                        s->provider_call = true;
                        s->recognizer.push_audio(s->recognizer.user, s->recognition, samples,
                                                 bytes / sizeof *samples);
                        s->provider_call = false;
                        (void)cui_speech_meter_push(&s->meter, samples, bytes / sizeof *samples,
                                                    m.time);
                        offset += bytes;
                    }
                }
                break;
            }
        }
        /* A completion subscriber may start a new generation and replace the queue. */
        if (s->generation != processing_generation)
            break;
        /* Lifecycle callbacks may cancel, but cannot dispose during pump. */
        size_t consumed = m.bytes;
        memmove(s->storage.queue_bytes, s->storage.queue_bytes + consumed,
                s->queue_used - consumed);
        s->queue_used -= consumed;
        for (size_t i = 1; i < s->queued; i++) {
            s->storage.messages[i - 1] = s->storage.messages[i];
            s->storage.messages[i - 1].offset -= consumed;
        }
        s->queued--;
    }
    if (s->queue_overflow && s->status != CUI_SPEECH_IDLE) {
        s->queue_overflow = false;
        cui_speech_end(s, CUI_SPEECH_ERROR, CUI_SPEECH_CAPACITY);
        changed = true;
    }
    if (s->status == CUI_SPEECH_STOPPING && now >= s->deadline) {
        cui_speech_end(s, CUI_SPEECH_FINAL, CUI_SPEECH_OK);
        changed = true;
    }
    s->pumping = false;
    return changed;
}

size_t cui_speech_waveform_rects(const cui_speech_meter *meter, float width, float height,
                                 float lead, cui_rect *output, size_t capacity) {
    if (!meter || meter->length > 256 || !isfinite(width) || !isfinite(height) || !isfinite(lead) ||
        width <= 0 || height <= 0 || lead < 0 || lead > 3 || (capacity && !output))
        return SIZE_MAX;
    cui_rect rects[256];
    size_t count = 0;
    float bar = fminf(4, fmaxf(2, roundf(height * .125f)));
    float pitch = bar + roundf(bar * 2.5f);
    for (size_t i = 0; i < meter->length; i++) {
        float right = width - pitch * ((float)i - lead), left = right - bar;
        if (left < -.5f || left >= width)
            continue;
        float level = cui_speech_meter_level(meter, meter->length - i - 1);
        float h = fmaxf(bar, height * fminf(1, fmaxf(0, level)));
        rects[count++] = (cui_rect){left, (height - h) * .5f, bar, h};
    }
    if (count <= capacity && count)
        memcpy(output, rects, count * sizeof *output);
    return count;
}
