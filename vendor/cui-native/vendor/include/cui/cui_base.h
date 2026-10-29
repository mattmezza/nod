/* SPDX-License-Identifier: MIT */
#ifndef CUI_BASE_H
#define CUI_BASE_H
#include "cui.h"
#include "cui_edit.h"
#include "cui_platform.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Submit inside a box with the same id. Base controls draw nothing. Values are
 * caller-owned and changes are applied synchronously; return true on change. */
cui_interaction cui_base_button(cui_context *, cui_id, cui_str, bool disabled);
bool cui_base_checkbox(cui_context *, cui_id, cui_str, bool *value, bool disabled);
bool cui_base_switch(cui_context *, cui_id, cui_str, bool *value, bool disabled);
bool cui_base_toggle(cui_context *, cui_id, cui_str, bool *value, bool disabled);
bool cui_base_radio(cui_context *, cui_id, cui_str, int *value, int option, bool disabled);
typedef enum { CUI_CHECK_UNCHECKED, CUI_CHECK_CHECKED, CUI_CHECK_MIXED } cui_check_state;
bool cui_base_check(cui_context *, cui_id, cui_str, cui_check_state *, bool disabled);
typedef void (*cui_check_change_fn)(void *user, cui_check_state requested,
                                    const cui_activation_snapshot *event);
typedef struct {
    bool disabled, required, invalid, skip_tab, no_pointer_focus, custom_role, controlled;
    int tab_index;
    cui_role role;
    cui_check_change_fn on_change;
    void *change_user;
} cui_check_options;
/* Extended Base: pointer focuses by default. Callback emits requests without
 * writing *value; owner may accept synchronously or reject. controlled=true
 * without a callback is read-only. Event is copied and
 * borrowed for the call only. Without callback retains synchronous mutation.
 * Return true for accepted activation, including a rejected controlled request. */
bool cui_base_check_ex(cui_context *, cui_id, cui_str, cui_check_state *, cui_check_options);
typedef void (*cui_check_box_style_fn)(void *user, cui_box *);
typedef struct {
    cui_check_box_style_fn instance, checked, mixed, disabled;
    void *user;
} cui_check_box_styles;
typedef void (*cui_check_content_fn)(cui_context *, void *);
/* Application-owned Base state refinement, applied over the supplied box in
 * instance -> checked/mixed -> disabled order. No allocation or theme lookup. */
bool cui_check_box_style(cui_check_state, bool disabled, cui_check_box_styles, cui_box *);
/* Unstyled indicator part. Draws the projected box and arbitrary child content;
 * owns no interaction/semantics. Root behavior remains cui_base_check_ex. */
bool cui_base_check_indicator(cui_context *, cui_id, cui_check_state, bool disabled, cui_box,
                              cui_check_box_styles, cui_check_content_fn, void *);

bool cui_base_slider(cui_context *, cui_id, cui_str, float *value, float minimum, float maximum,
                     float step, bool disabled);
/* Controlled double-precision slider. Submit inside a box with the same ID.
 * Emits requests only; value is never mutated. State is presentation/capture only.
 * begin/update/commit is one pointer gesture; cancel restores its starting value.
 * Keyboard changes emit begin/update/commit per key, with no hidden acceleration.
 * Missing callback is read-only. Disabled/read-only/source reset cancels a drag.
 * Semantic numeric values are normalized0..1 (Core's numeric ABI is float); name
 * and value_description carry application time units for accessibility. */
typedef enum {
    CUI_SLIDER_BEGIN,
    CUI_SLIDER_UPDATE,
    CUI_SLIDER_COMMIT,
    CUI_SLIDER_CANCEL
} cui_slider_phase;
typedef struct {
    cui_slider_phase phase;
    double value;
    cui_event event; /* Copied, borrowed only for the synchronous callback. */
} cui_slider_request;
typedef void (*cui_slider_request_fn)(void *, const cui_slider_request *);
typedef struct {
    cui_id id;
    double preview, start;
    bool dragging;
} cui_slider_state;
typedef struct {
    double minimum, maximum, step;
    bool disabled, readonly;
    cui_str value_description;
    cui_slider_request_fn on_request;
    void *user;
} cui_slider_options;
/* false: invalid arguments, unchanged state and no submission. true: submitted.
 * Callbacks may commit/reject externally; each call uses one copied snapshot.
 * Callback must not destroy context/state during submission. */
bool cui_base_slider_controlled(cui_context *, cui_id, cui_str, double value, cui_slider_state *,
                                cui_slider_options);

typedef struct {
    float fraction, start, length;
    bool animating;
} cui_progress_geometry;
/* Pure geometry in normalized track units. Nonfinite fraction becomes zero;
 * reduced/invalid clocks produce a static centered loading segment. Source
 * loading uses the frozen one-second growing/shrinking quadratic cycle; false
 * preserves the legacy1.6s fixed-width CUI cycle. */
cui_progress_geometry cui_progress_measure(float fraction, bool indeterminate, bool source_motion,
                                           double now_seconds, bool reduced_motion);
/* Circle canvas geometry in logical pixels, clockwise from12 o'clock. */
typedef struct {
    float diameter, stroke_width, inner_radius, outer_radius, start_angle, end_angle;
} cui_progress_circle_geometry;
bool cui_progress_circle_measure(cui_progress_geometry, float diameter,
                                 cui_progress_circle_geometry *);
/* Allocation-free4x4 antialiased RGBA raster into caller storage. Diameter1..4096,
 * scale(0,4], at most16384px per side. Failure preserves pixels and output.
 * Pixels must remain alive and unmodified through renderer completion. */
bool cui_progress_circle_raster(cui_progress_circle_geometry, cui_color, float scale,
                                unsigned char *pixels, size_t capacity, cui_bitmap *);
/* Submit inside a box with this id; paints nothing. Controlled normalized
 * fraction, numeric0..1 semantics, indeterminate busy/unknown value and no focus. */
float cui_base_progress(cui_context *, cui_id, cui_str name, float fraction, bool indeterminate);
typedef struct {
    double start, last_time;
    float from, target, value, duration;
    bool ready, active;
} cui_progress_motion;
typedef struct {
    float value;
    bool active;
} cui_progress_motion_result;
/* Caller-owned deterministic transition, source move cubic-bezier(.2,0,0,1).
 * Initial submission is immediate; changes/reversals continue from the current
 * value. Duration0/reduced motion snaps. Invalid/nonfinite arguments preserve
 * state and result. A regressing clock is clamped to the last observed time. */
bool cui_progress_animate(cui_progress_motion *, float fraction, double now_seconds, float duration,
                          bool reduced_motion, cui_progress_motion_result *);
typedef struct {
    bool changed, submitted;
    cui_edit_result error;
    bool rejected; /* Acceptance rejection; distinct from errors/explicit invalid. */
} cui_input_result;
typedef struct {
    cui_context *ui;
    cui_rect viewport;
    float font_size, scroll_x;
} cui_input_hit_geometry;
/* Optional visual host geometry, otherwise measured scalar hit test. */
size_t cui_base_input_hit_test(void *geometry, cui_str, float x, float y);
/* Explicit measurement-only legacy path; never calls optional geometry factory. */
size_t cui_base_input_measured_hit_test(void *geometry, cui_str, float x, float y);
/* Borrowed immutable visual session plus existing viewport/scroll mapping. */
typedef struct {
    cui_input_hit_geometry measured;
    const cui_text_geometry *visual;
    cui_text_affinity *affinity; /* Optional output, preserved on failed hit. */
} cui_input_visual_hit_geometry;
size_t cui_base_input_visual_hit_test(void *geometry, cui_str, float x, float y);
typedef size_t (*cui_input_hit_test_fn)(void *user, cui_str text, float x, float y);
typedef struct {
    bool dragging;
    cui_edit_composition *composition;   /* Optional caller-owned session and view buffers. */
    bool composition_owned;              /* Internal default resource flag; initialize false. */
    uintptr_t bound_edit_identity;       /* Internal numeric identity; no retained model pointer. */
    bool focus_initialized, was_focused; /* Zero-init; focus/rebind splits typing history. */
    bool secure_initialized, was_masked;
    cui_text_input_purpose previous_content_type;
    bool pointer_word_active; /* Internal double-click drag range; zero-init. */
    size_t pointer_word_start, pointer_word_end;
    uint64_t pointer_word_revision; /* Numeric revision; no borrowed text pointer. */
} cui_input_behavior_state;
/* Lazy bounded default preedit (4095 bytes); model-owned rendered view grows. */
cui_edit_composition *cui_base_composition(cui_context *, cui_id, cui_edit_composition *,
                                           bool *owned);
/* Runs synchronously after the checked engine request has returned. Publish a
 * prepared stage only when applied; callback side effects are outside rollback. */
typedef void (*cui_input_presentation_complete_fn)(void *user, bool prepared, bool applied);
typedef struct {
    bool disabled, readonly, required, invalid;
    const cui_platform *clipboard;  /* Optional synchronous host clipboard interface. */
    cui_input_hit_test_fn hit_test; /* UTF-8 byte offset in current text. */
    void *hit_test_user;
    bool delegate_up_down;  /* Composite popup owns vertical navigation. */
    bool delegate_home_end; /* Composite popup owns these keys; editor leaves them untouched. */
    cui_edit_accept_fn validate;          /* Optional source single-line whole-value predicate. */
    void *validate_user;                  /* Borrowed synchronously; no retained candidate text. */
    cui_edit_normalization normalization; /* Zero preserves legacy CR/LF rejection. */
    bool masked;
    cui_text_input_purpose content_type;
    cui_edit_presentation presentation;
    cui_input_presentation_complete_fn presentation_complete;
    void *presentation_user;
    cui_edit_format format;
    cui_edit_workspace format_workspace; /* Caller-owned final formatted value + NUL. */
    cui_text_match matcher; /* Optional synchronous whole-value regex/provider policy. */
} cui_base_input_options;
/* Reduce one frame event in queue order after registering the textbox with
 * cui_interact. Does not submit/paint semantics or replay other events. The
 * caller aggregates errors and publishes final semantic state. Invalid event
 * indices/NULL arguments return INVALID_ARGUMENT without mutating state. Hit
 * geometry and clipboard user data are borrowed synchronously for this call. */
cui_input_result cui_base_input_reduce_event(cui_context *, cui_id, cui_edit *,
                                             cui_input_behavior_state *, cui_base_input_options,
                                             size_t event_index);
cui_input_result cui_base_input_ex(cui_context *, cui_id, cui_str, cui_edit *,
                                   cui_input_behavior_state *, cui_base_input_options);
/* Text is owned by the editor; changed concerns text, not cursor movement. */
cui_input_result cui_base_input(cui_context *, cui_id, cui_str, cui_edit *, bool disabled);
/* Standalone allocation-free layout models. */
typedef enum { CUI_POPUP_BELOW, CUI_POPUP_ABOVE, CUI_POPUP_RIGHT, CUI_POPUP_LEFT } cui_popup_side;
typedef struct {
    cui_rect bounds;
    cui_popup_side side;
    bool flipped;
} cui_popup_result;
cui_popup_result cui_popup_place(cui_rect anchor, cui_size popup, cui_rect viewport,
                                 cui_popup_side preferred, float gap);
typedef struct {
    size_t begin, end;
    float before, after, total, scroll;
} cui_virtual_range;
/* heights must be finite and nonnegative; invalid entries are treated as zero. */
cui_virtual_range cui_virtualize(const float *heights, size_t count, float scroll, float viewport,
                                 size_t overscan);
typedef struct {
    int year, month, day;
} cui_date;
bool cui_date_valid(cui_date);
int cui_date_days_in_month(int year, int month);
int cui_date_weekday(cui_date);                     /* Sunday = 0, invalid = -1 */
cui_date cui_date_add_months(cui_date, int months); /* Clamp day; invalid -> zero */
cui_date cui_date_add_days(cui_date, int days);     /* years limited to 1..9999 */
#ifdef __cplusplus
}
#endif
#endif
