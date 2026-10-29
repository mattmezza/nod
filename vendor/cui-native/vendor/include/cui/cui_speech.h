/* SPDX-License-Identifier: MIT */
#ifndef CUI_SPEECH_H
#define CUI_SPEECH_H
#include "cui.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_SPEECH_IDLE,
    CUI_SPEECH_CONNECTING,
    CUI_SPEECH_RECORDING,
    CUI_SPEECH_STOPPING
} cui_speech_status;
typedef enum {
    CUI_SPEECH_OK,
    CUI_SPEECH_PERMISSION,
    CUI_SPEECH_NO_INPUT,
    CUI_SPEECH_UNSUPPORTED,
    CUI_SPEECH_INPUT_ERROR,
    CUI_SPEECH_RECOGNIZER_ERROR,
    CUI_SPEECH_CAPACITY,
    CUI_SPEECH_INVALID
} cui_speech_error;
typedef enum {
    CUI_SPEECH_STARTED,
    CUI_SPEECH_PARTIAL,
    CUI_SPEECH_FINAL,
    CUI_SPEECH_CANCELLED,
    CUI_SPEECH_ERROR
} cui_speech_event_type;
typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
} cui_speech_format;
typedef struct cui_speech_state cui_speech_state;
typedef struct {
    cui_speech_state *state;
    uint64_t generation;
} cui_speech_sink;
typedef struct {
    cui_speech_event_type type;
    cui_speech_error error;
    cui_str text;
} cui_speech_event;
typedef void (*cui_speech_event_fn)(void *, cui_speech_event);
/* Providers run on the owner thread. Reports copy their payload and are delivered
 * only by pump, including reports made synchronously from provider callbacks.
 * Provider tokens are borrowed handles; close releases each successful session.
 * A provider may report through its sink, but must not call lifecycle functions. */
typedef struct {
    void *user;
    cui_speech_format format; /* all zero selects 16000 Hz mono */
    bool (*available)(void *);
    cui_speech_error (*start)(void *, cui_speech_sink, void **session);
    void (*push_audio)(void *, void *session, const int16_t *, size_t);
    void (*finish)(void *, void *session);
    void (*close)(void *, void *session);
} cui_speech_recognizer;
typedef struct {
    void *user;
    cui_speech_error (*start)(void *, cui_speech_format, cui_speech_sink, void **capture);
    void (*close)(void *, void *capture);
} cui_speech_input;
typedef struct {
    unsigned kind;
    uint64_t generation;
    size_t offset, bytes;
    cui_speech_error error;
    double time;
} cui_speech_message;
typedef struct {
    char *transcript;
    size_t transcript_capacity;
    cui_speech_message *messages;
    size_t message_capacity;
    unsigned char *queue_bytes;
    size_t queue_capacity;
} cui_speech_workspace;
typedef struct {
    float levels[256], smoothed;
    size_t first, length, samples, window;
    double sum, anchor, pace;
    uint64_t recorded;
    bool anchored;
} cui_speech_meter;
struct cui_speech_state {
    cui_speech_workspace storage;
    cui_speech_recognizer recognizer;
    cui_speech_input input;
    cui_speech_event_fn event;
    void *event_user;
    cui_speech_status status;
    uint64_t generation;
    void *recognition, *capture;
    size_t committed, hypothesis, queued, queue_used;
    double stop_timeout, deadline;
    cui_speech_meter meter;
    bool initialized, accepting, provider_call, pumping, queue_overflow;
};
/* Zero-initialize. Workspace and providers outlive state; dispose before reuse.
 * Storage regions must not overlap. No allocation, platform types or threads. */
bool cui_speech_init(cui_speech_state *, cui_speech_workspace, cui_speech_recognizer,
                     cui_speech_input, cui_speech_event_fn, void *);
void cui_speech_dispose(cui_speech_state *);
bool cui_speech_has_recognizer(const cui_speech_state *);
bool cui_speech_available(const cui_speech_state *);
bool cui_speech_start(cui_speech_state *);
bool cui_speech_stop(cui_speech_state *, double now);
bool cui_speech_cancel(cui_speech_state *);
bool cui_speech_toggle(cui_speech_state *, double now);
bool cui_speech_set_stop_timeout(cui_speech_state *, double seconds);
/* Process copied reports in order and enforce the stop deadline. Finite seconds,
 * normally from a monotonic host clock. Nested pump calls are ignored. */
bool cui_speech_pump(cui_speech_state *, double now);
cui_str cui_speech_transcript(const cui_speech_state *);
bool cui_speech_report_ready(cui_speech_sink);
bool cui_speech_report_hypothesis(cui_speech_sink, cui_str);
bool cui_speech_report_phrase(cui_speech_sink, cui_str);
bool cui_speech_report_finish(cui_speech_sink);
bool cui_speech_report_error(cui_speech_sink, cui_speech_error);
bool cui_speech_report_audio(cui_speech_sink, const int16_t *, size_t, double arrival_time);
void cui_speech_meter_reset(cui_speech_meter *, cui_speech_format);
bool cui_speech_meter_push(cui_speech_meter *, const int16_t *, size_t, double now);
float cui_speech_meter_level(const cui_speech_meter *, size_t oldest_first);
float cui_speech_meter_lead(const cui_speech_meter *, double now);
typedef enum {
    CUI_SPEECH_MEDIUM,
    CUI_SPEECH_SMALL,
    CUI_SPEECH_LARGE,
    CUI_SPEECH_XSMALL
} cui_speech_size;
typedef struct {
    cui_speech_size size;
    bool disabled, show_when_unsupported;
    cui_str start_label, stop_label, unavailable_label; /* empty selects English defaults */
    cui_design_size design_size;                        /* INHERIT preserves legacy metrics. */
} cui_speech_button_options;
bool cui_speech_button(cui_context *, cui_id, const cui_theme *, cui_speech_state *,
                       cui_speech_button_options, double now);
typedef struct {
    cui_speech_size size;
    float width, height; /* zero selects 96 and the size-specific height */
    bool reduced_motion, custom_color;
    cui_color color;
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
} cui_speech_waveform_options;
/* Geometry in local coordinates; returns the required rectangle count. Invalid
 * geometry returns SIZE_MAX. Insufficient capacity leaves output untouched. */
size_t cui_speech_waveform_rects(const cui_speech_meter *, float width, float height, float lead,
                                 cui_rect *, size_t capacity);
bool cui_speech_waveform(cui_context *, cui_id, const cui_theme *, const cui_speech_state *,
                         cui_speech_waveform_options, double now);
#ifdef __cplusplus
}
#endif
#endif
