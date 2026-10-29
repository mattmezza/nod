/* SPDX-License-Identifier: MIT */
#ifndef CUI_COMPONENTS_H
#define CUI_COMPONENTS_H
#include "cui_base.h"
#include "cui_boolean_events.h"
#include "cui_carousel.h"
#include "cui_chart.h"
#include "cui_choice.h"
#include "cui_chrome.h"
#include "cui_command.h"
#include "cui_content.h"
#include "cui_control_events.h"
#include "cui_description.h"
#include "cui_disclosure.h"
#include "cui_editor.h"
#include "cui_form.h"
#include "cui_icon.h"
#include "cui_label.h"
#include "cui_numeric.h"
#include "cui_otp.h"
#include "cui_picker.h"
#include "cui_plot_view.h"
#include "cui_questionnaire.h"
#include "cui_resizable.h"
#include "cui_root.h"
#include "cui_scroll.h"
#include "cui_speech.h"
#include "cui_status.h"
#include "cui_surface.h"
#include "cui_table.h"
#include "cui_text_input_events.h"
#include "cui_text_overflow.h"
#include "cui_text_view.h"
#include "cui_theme.h"
#include "cui_toolbar.h"
#include "cui_tree.h"
#include "cui_virtual_scroll.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_BUTTON_PRIMARY,
    CUI_BUTTON_SECONDARY,
    CUI_BUTTON_OUTLINE,
    CUI_BUTTON_GHOST,
    CUI_BUTTON_DANGER,
    CUI_BUTTON_LINK,
    CUI_BUTTON_WARNING,
    CUI_BUTTON_SUCCESS,
    CUI_BUTTON_INFO,
    CUI_BUTTON_DEFAULT,
    CUI_BUTTON_TEXT,
    CUI_BUTTON_CUSTOM
} cui_button_variant;
typedef enum {
    CUI_CONTROL_MEDIUM,
    CUI_CONTROL_SMALL,
    CUI_CONTROL_LARGE,
    CUI_CONTROL_XSMALL
} cui_control_size;
typedef void (*cui_button_content_fn)(cui_context *, void *user);
typedef enum {
    CUI_BUTTON_ROUNDED_THEME,
    CUI_BUTTON_ROUNDED_NONE,
    CUI_BUTTON_ROUNDED_SMALL,
    CUI_BUTTON_ROUNDED_LARGE,
    CUI_BUTTON_ROUNDED_CUSTOM
} cui_button_rounding;
typedef void (*cui_button_click_fn)(void *user);
typedef enum {
    CUI_BUTTON_CLICK_POINTER,
    CUI_BUTTON_CLICK_KEYBOARD,
    CUI_BUTTON_CLICK_ACCESSIBILITY
} cui_button_click_origin;
/* Pointer-free accepted-event snapshot; copyable beyond callback/frame lifetime.
 * Legacy pointer fields describe release; evidence preserves raw Core samples.
 * Evidence is frozen before legacy callbacks can consume accessibility input. */
typedef struct {
    cui_button_click_origin origin;
    cui_id target;
    uint64_t frame_sequence;
    size_t event_index;
    cui_event_type event_type;
    cui_key key;
    unsigned modifiers;
    bool repeat, has_position;
    float x, y;
    cui_pointer_type pointer_type;
    cui_pointer_button pointer_button;
    uint64_t pointer_id, pointer_device;
    unsigned click_count; /* Pointer zero normalizes to one; nonpointer is zero. */
    bool has_evidence;
    cui_activation_snapshot evidence; /* Fully copied; no host metadata is fabricated. */
} cui_button_click_event;
typedef void (*cui_button_click_event_fn)(void *user, const cui_button_click_event *event);
typedef struct cui_button_artwork cui_button_artwork;
typedef void (*cui_button_hover_fn)(void *user, bool hovered);
/* Optional bounded storage, one state per stable Button ID; retain through end_frame.
 * No allocation/cleanup. Hover callbacks and tooltip require this state. */
typedef struct {
    cui_id id;
    bool initialized, hovered;
    cui_hover_state tooltip;
    char label[256]; /* Bounded grapheme-safe visible ellipsis; full name stays semantic. */
    double next_wake_seconds; /* Absolute Core frame time, zero when idle. */
} cui_button_state;
typedef struct {
    cui_button_variant variant;
    cui_control_size size;
    bool disabled, loading, selected;
    float width; /* zero fits contents; negative grows within parent */
    cui_str leading, trailing;
    cui_str icon;   /* Catalog name/path; theme.icon_loader may replace its SVG. */
    bool icon_only; /* Keep name for accessibility; render a square icon button. */
    /* Arbitrary children instead of the label; invoked once during submission.
     * Fixed/grow buttons lay out children within the current inner width, after
     * padding and sibling slots. Zero-width buttons retain intrinsic sizing. */
    cui_button_content_fn content;
    void *content_user;
    bool outline;                /* Orthogonal colored outline; selected is presentation only. */
    cui_design_size design_size; /* INHERIT keeps the frozen .size metrics. */
    bool compact;
    float height, font_size, icon_size; /* Positive finite overrides; zero derives metrics. */
    cui_button_rounding rounding;
    float radius; /* ROUND_CUSTOM: nonnegative finite logical pixels. */
    bool custom_colors;
    cui_theme_button_colors colors; /* Resolved normal/hover/active + outlined palette. */
    cui_str trailing_icon, loading_icon;
    bool dropdown_caret;
    cui_button_content_fn leading_content, trailing_content, children;
    void *leading_user, *trailing_user, *children_user;
    float label_width; /* Positive label maximum; constrained labels ellipsize automatically. */
    cui_button_state *state;
    cui_button_click_fn on_click; /* Called once on activation; bool result still reports it. */
    cui_button_hover_fn on_hover; /* Interactive hover edges, requires state. */
    void *action_user;
    cui_str accessibility_label, tooltip;
    cui_hover_options tooltip_options; /* viewport required; trigger/dt/disabled supplied. */
    bool skip_tab, hide_focus_ring, prevent_pointer_focus;
    /* Explicit signed source ordering; zero defaults retain legacy ordering.
     * skip_tab controls eligibility independently, including index -1. */
    bool custom_tab_index;
    int tab_index;
    bool toggle_semantics,
        toggled; /* Explicit checked/value metadata; selected remains paint-only. */
    bool custom_cursor;
    cui_cursor cursor; /* Applied with Core cursor-only setter; zero defaults unchanged. */
    bool custom_corners, custom_border_edges;
    cui_corner_radii corner_radii;
    cui_border_edges border_edges; /* Finite nonnegative logical widths. */
    bool allow_content_overflow;   /* Explicit legacy overflow/wrapping opt-out; default clips. */
    float pixel_size; /* 0: legacy/design sizing; finite (0,4096]: numeric source sizing. */
    bool open; /* Active presentation only; independent of selected/checked/menu lifecycle. */
    cui_button_click_event_fn on_click_event; /* After legacy on_click, per accepted event. */
    const cui_button_artwork *leading_artwork, *loading_artwork; /* Optional typed slots. */
} cui_button_options;
/* Paint an independently registered button without reducing events or submitting semantics. */
void cui_button_paint(cui_context *, cui_id, const cui_theme *, cui_str name, cui_button_options);
bool cui_button_ex(cui_context *, cui_id, const cui_theme *, cui_str name, cui_button_options);
/* Additive per-event facade: legacy click -> old typed click -> new handler.
 * Original action_user (including hover) is independent of handler.user.
 * Counts accepted activations even with no new handler. Existing resource
 * lifetimes and no-destruction-during-dispatch preconditions remain in force. */
cui_component_action_result cui_button_events(cui_context *, cui_id, const cui_theme *, cui_str,
                                              cui_button_options, cui_component_activation_handler);
bool cui_button(cui_context *, cui_id, const cui_theme *, cui_str, bool disabled);
#include "cui_button_group.h"
typedef struct {
    cui_id id;
    cui_str label;
    cui_button_options options;
} cui_button_group_styled_item;
typedef struct {
    cui_button_group_options behavior;
    bool override_size, override_variant;
    cui_control_size size;
    cui_design_size design_size;
    cui_button_variant variant;
    bool compact, outline;
    cui_box box; /* Orientation and zero segmented gap supplied by the group. */
} cui_button_group_style_options;
/* Bounded copied descriptor snapshot; borrowed Button states/content remain under
 * existing Button lifetimes. Zero initialize; do not reuse during callbacks. */
typedef struct {
    cui_button_group_workspace behavior;
    cui_button_group_styled_item items[CUI_BUTTON_GROUP_MAX_ITEMS];
    cui_button_state passive_states[CUI_BUTTON_GROUP_MAX_ITEMS];
    bool composing;
} cui_button_group_style_workspace;
bool cui_button_group_ex(cui_context *, cui_id, const cui_theme *,
                         const cui_button_group_styled_item *, bool *selected, size_t count,
                         cui_button_group_style_workspace *, cui_button_group_style_options,
                         cui_button_group_result *result);
/* Paint-only antialiased check glyph for composed selection indicators. */
void cui_checkmark(cui_context *, float size, cui_color);
/* Caller-owned, one stable state per Checkbox ID; no allocation or cleanup.
 * Zero initialization snaps to the initial logical value. Retain through end_frame. */
typedef struct {
    cui_id id;
    cui_disclosure_motion checked, mixed;
    cui_hover_state tooltip;
    double next_wake_seconds; /* Absolute Core frame time; zero when quiescent. */
} cui_checkbox_state;
typedef struct {
    cui_box root, indicator;
    cui_color foreground, label;
    float label_font_size, label_line_height;
    cui_text_style label_style;
} cui_checkbox_paint;
/* Refine initialized defaults. Order: instance, checked/mixed, then disabled.
 * Application callbacks own hover/active/focus refinements via existing queries. */
typedef void (*cui_checkbox_style_fn)(cui_context *, void *, cui_check_state, bool disabled,
                                      cui_checkbox_paint *);
typedef struct {
    cui_checkbox_style_fn instance, checked, mixed, disabled;
    void *user;
} cui_checkbox_styles;
typedef struct {
    cui_check_state value;
    float width, height, checked_progress, mixed_progress;
    cui_color foreground;
    bool disabled;
} cui_checkbox_indicator_info;
typedef void (*cui_checkbox_indicator_fn)(cui_context *, void *, cui_checkbox_indicator_info);
typedef struct {
    cui_control_size size;
    bool disabled, required, invalid, skip_tab, hide_focus_ring;
    cui_str accessibility_label;
    cui_button_content_fn content;
    void *content_user;
    cui_design_size design_size; /* INHERIT keeps the frozen .size metrics. */
    /* Checkbox-only extensions; cui_radio_options aliases this struct but ignores these fields. */
    cui_checkbox_state *state; /* Optional spring mark/tooltip storage; NULL keeps legacy paint. */
    float width; /* Checkbox: positive bounds wrapped labels/custom content; zero fits. */
    cui_str tooltip;
    cui_hover_options tooltip_options; /* viewport required; trigger/dt/disabled are supplied. */
    bool reduced_motion;
    /* Checkbox-only source seams. Radio alias ignores every extension below. */
    cui_check_change_fn on_change;
    void *change_user;
    int tab_index;
    bool custom_role;
    cui_role role;
    cui_checkbox_styles styles;
    cui_checkbox_indicator_fn indicator;
    void *indicator_user;
    bool source_behavior; /* Styled no-pointer-focus; use cui_checkbox_source. */
} cui_checkbox_options;
bool cui_checkbox_ex(cui_context *, cui_id, const cui_theme *, cui_str, cui_check_state *,
                     cui_checkbox_options);
bool cui_checkbox(cui_context *, cui_id, const cui_theme *, cui_str, bool *, bool disabled);
/* Frozen-source controlled facade: no callback leaves the value unchanged.
 * Pointer activation preserves focus; keyboard focus and
 * tab ordering remain available. Legacy checkbox/ex retain pointer-focus policy. */
bool cui_checkbox_source(cui_context *, cui_id, const cui_theme *, cui_str, cui_check_state *,
                         cui_checkbox_options);

/* Controlled immutable entry snapshot, source no-pointer-focus policy.
 * options.on_change/change_user must be NULL. Owner assignments are observed
 * next submission; this call keeps its snapshot for requests/paint/semantics.
 * Same callback lifetime/teardown preconditions as cui_base_check_events. */
cui_component_action_result cui_checkbox_events(cui_context *, cui_id, const cui_theme *, cui_str,
                                                cui_check_state, cui_checkbox_options,
                                                cui_component_check_handler);

typedef void (*cui_boolean_change_fn)(void *user, bool requested);
typedef struct {
    double position, velocity, target, updated_at, next_wake_seconds;
    bool initialized;
    cui_hover_state tooltip;
} cui_switch_state;
typedef struct {
    cui_control_size size;
    bool disabled, skip_tab, hide_focus_ring, reduced_motion, label_left, custom_color;
    float width;
    cui_color color;
    cui_str accessibility_label, tooltip;
    cui_button_content_fn content;
    void *content_user;
    cui_boolean_change_fn on_change;
    void *change_user;
    double now_seconds;
    cui_hover_options tooltip_options;
    cui_design_size design_size; /* INHERIT keeps the frozen .size metrics. */
} cui_switch_options;
cui_switch_options cui_switch_options_default(void);
cui_id cui_switch_track_id(cui_id);
cui_id cui_switch_thumb_id(cui_id);
/* Callback is a controlled request; without it, activation writes *value.
 * State is optional for static painting, retained for interruptible spring motion. */
bool cui_switch_ex(cui_context *, cui_id, const cui_theme *, cui_str label, bool *value,
                   cui_switch_state *, cui_switch_options);
bool cui_switch(cui_context *, cui_id, const cui_theme *, cui_str, bool *, bool disabled);
/* Immutable controlled snapshot. Legacy on_change/change_user must both be NULL;
 * conflicts fail before semantics/state/paint. Handler may accept externally;
 * requests, semantics and painting retain entry value for this call. Optional
 * state retains spring/tooltip presentation only. Same teardown preconditions
 * as cui_base_switch_events; no handler means read-only. */
cui_component_action_result cui_switch_events(cui_context *, cui_id, const cui_theme *, cui_str,
                                              bool, cui_switch_state *, cui_switch_options,
                                              cui_component_boolean_handler);
typedef enum { CUI_TOGGLE_GHOST, CUI_TOGGLE_OUTLINE } cui_toggle_variant;
typedef struct {
    cui_toggle_variant variant;
    cui_control_size size;
    bool disabled, skip_tab, focus_ring;
    float width;
    cui_str accessibility_label, tooltip;
    cui_button_content_fn content;
    void *content_user;
    cui_boolean_change_fn on_change;
    void *change_user;
    cui_corner_radii corner_radii;
    cui_border_edges border_edges;
    bool custom_corners, custom_border_edges;
    cui_hover_options tooltip_options;
    cui_design_size design_size; /* INHERIT keeps the frozen .size metrics. */
} cui_toggle_options;
cui_toggle_options cui_toggle_options_default(void);
/* Paint only; compose with independently registered headless toggle behavior.
 * Does not register semantics, mutate state, invoke on_change or open tooltips. */
void cui_toggle_paint(cui_context *, cui_id, const cui_theme *, cui_str, bool, cui_toggle_options);
bool cui_toggle_ex(cui_context *, cui_id, const cui_theme *, cui_str, bool *,
                   cui_hover_state *tooltip_state, cui_toggle_options);
/* Same immutable/request/conflict rules as cui_switch_events. Tooltip state is
 * optional presentation storage; passive ancestors do not mutate it. */
cui_component_action_result cui_toggle_events(cui_context *, cui_id, const cui_theme *, cui_str,
                                              bool, cui_hover_state *, cui_toggle_options,
                                              cui_component_boolean_handler);
typedef struct {
    cui_id id;
    cui_str label;
    cui_toggle_options options;
    cui_hover_state *tooltip_state;
} cui_toggle_item;
typedef void (*cui_toggle_group_change_fn)(void *, const bool *requested_values, size_t count);
typedef struct {
    cui_control_size size;
    cui_toggle_variant variant;
    bool disabled, segmented, custom_gap;
    float gap;
    cui_str accessibility_label;
    cui_toggle_group_change_fn on_change;
    void *change_user;
    /* Required with on_change; borrowed synchronous request scratch, separate from values. */
    bool *requested_values;
    size_t requested_capacity;
    cui_design_size design_size; /* INHERIT keeps the frozen .size metrics. */
} cui_toggle_group_style_options;
bool cui_toggle_group_ex(cui_context *, cui_id, const cui_theme *, const cui_toggle_item *,
                         bool *values, size_t count, cui_toggle_group_style_options);
bool cui_toggle(cui_context *, cui_id, const cui_theme *, cui_str, bool *, bool disabled);
typedef cui_checkbox_options cui_radio_options;
bool cui_radio_ex(cui_context *, cui_id, const cui_theme *, cui_str, int *, int option,
                  cui_radio_options);
bool cui_radio(cui_context *, cui_id, const cui_theme *, cui_str, int *, int option, bool disabled);
/* Double controlled Styled facade: include cui_styled_slider_events.h. */
bool cui_slider(cui_context *, cui_id, const cui_theme *, cui_str, float *, float minimum,
                float maximum, float step, bool disabled);
/* Optional three disjoint caller-owned staging buffers; capacities include NUL.
 * Keep descriptor/buffers unchanged and alive through context resource release.
 * Every slot must fit the proposed scalar mask, otherwise edits fail atomically. */
typedef struct {
    char *buffers[3];
    size_t capacities[3];
} cui_input_mask_storage;
/* Optional mask-buffer allocator pair (zero = malloc/free). Returned storage
 * must be fresh/disjoint. User/callbacks live through retained resource release.
 * Geometry sessions/cache copies use their normal host/system ownership. */
typedef struct {
    void *user;
    void *(*allocate)(void *user, size_t bytes);
    void (*release)(void *user, void *storage);
} cui_input_mask_allocator;
typedef struct {
    cui_input_behavior_state behavior;
    float scroll_x;
} cui_input_state;
typedef enum {
    CUI_INPUT_MEDIUM,
    CUI_INPUT_SMALL,
    CUI_INPUT_LARGE,
    CUI_INPUT_XSMALL,
    CUI_INPUT_CUSTOM
} cui_input_size;
typedef struct {
    float height, font_size, line_height, padding_x, padding_y, gap;
} cui_input_metrics;
typedef struct {
    bool disabled, readonly, required, invalid;
    float width; /* Zero defaults to 280; negative grows. */
    cui_str placeholder;
    const cui_platform *clipboard;
    cui_input_size size;
    float custom_size, density; /* density 0 derives theme font_size/16. */
    float height;               /* Optional frame-height refinement; line box remains 20*density. */
    bool custom_height;  /* Preserve explicit zero height. Positive height always overrides. */
    bool custom_padding; /* Use padding_x/y in pixels, including explicit zero. */
    float padding_x, padding_y;
    bool no_appearance, no_border, hide_focus_ring;
    cui_align text_align;
    bool delegate_up_down;       /* Composite owns list navigation instead of caret movement. */
    cui_design_size design_size; /* INHERIT keeps the frozen .size metrics. */
    cui_edit_accept_fn validate;
    void *validate_user; /* See cui_edit_apply_checked: synchronous, nonretaining. */
    cui_edit_normalization normalization; /* Zero preserves legacy CR/LF rejection. */
    bool masked;
    cui_text_input_purpose content_type;
    cui_input_mask_storage *mask_storage;
    cui_input_mask_allocator mask_allocator;
    cui_edit_format format;
    cui_edit_workspace format_workspace; /* Caller-owned final formatted value + NUL. */
    cui_text_match matcher; /* Optional synchronous whole-value regex/provider policy. */
} cui_input_options;
bool cui_input_measure(cui_input_options, float root_font_size, cui_input_metrics *);
cui_id cui_input_viewport_id(cui_id);
/* Paint only: compose with a reducer without applying the event stream twice. */
void cui_input_paint(cui_context *, cui_id, const cui_theme *, cui_str, cui_edit *,
                     cui_input_state *, cui_input_options);
cui_input_result cui_input_ex(cui_context *, cui_id, const cui_theme *, cui_str name, cui_edit *,
                              cui_input_state *, cui_input_options);
/* CHANGE/SUBMIT observers follow Base Input timing and lifetime rules. Secure text
 * is redacted. Defer model/context destruction until frame/render consumption. */
cui_input_event_result cui_input_events(cui_context *, cui_id, const cui_theme *, cui_str name,
                                        cui_edit *, cui_input_state *, cui_input_options,
                                        cui_component_text_handler);
cui_input_result cui_input(cui_context *, cui_id, const cui_theme *, cui_str name, cui_edit *,
                           bool disabled);
void cui_label(cui_context *, const cui_theme *, cui_str);
void cui_badge(cui_context *, cui_id, const cui_theme *, cui_str);
typedef enum { CUI_BADGE_COUNT, CUI_BADGE_DOT, CUI_BADGE_ICON } cui_badge_variant;
typedef struct {
    cui_badge_variant variant;
    size_t count, maximum;
    cui_control_size size;
    cui_str icon, name;
    cui_color color;
    bool custom_color;
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
} cui_badge_options;
cui_badge_options cui_badge_defaults(void);
/* Compose arbitrary content with a zero-flow corner indicator. Count zero hides
 * only the indicator; count overflow reads maximum+. Dot/icon always show.
 * INHERIT uses source offsets and fixed10px text, dots6px, icon10/16/24px.
 * Explicit design_size scales indicator text, padding and artwork.
 * Content/name/icon bytes are borrowed for the call. Invalid options submit no
 * UI and invoke no callback. Legacy cui_badge remains a standalone text pill. */
bool cui_badge_ex(cui_context *, cui_id, const cui_theme *, cui_badge_options,
                  cui_button_content_fn content, void *user);
typedef struct {
    bool indeterminate, reduced_motion;
    double now_seconds; /* Monotonic host clock; source cycle1s, legacy cycle1.6s. */
    float height;       /* Zero selects legacy4/6/8/10px, or design control_height/4. */
    cui_str name;       /* Zero selects "Progress". */
    cui_control_size size;
    cui_color color;
    bool custom_color, source_motion;
    cui_corner_radii corners;
    bool custom_corners;
    cui_progress_motion *motion; /* Optional caller-owned determinate transition. */
    float duration; /* Zero selects source180ms; explicit instant via reduced_motion. */
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
} cui_progress_options;
cui_progress_options cui_progress_defaults(void);
/* Returns true while indeterminate motion needs another frame. The application
 * owns the fraction and clock; reduced motion shows a static centered segment. */
bool cui_progress_ex(cui_context *, cui_id, const cui_theme *, float fraction, float width,
                     cui_progress_options);
typedef struct {
    unsigned char *pixels;
    size_t capacity;
    uint64_t revision;
} cui_progress_circle_state;
typedef struct {
    cui_progress_options progress;
    float diameter, raster_scale; /* Zero uses legacy8/12/16/20px or design control_height. */
} cui_progress_circle_options;
cui_progress_circle_options cui_progress_circle_defaults(void);
/* Button artwork descriptors are copied during composition. Referenced states,
 * pixel storage and result outputs remain caller-owned; keep render resources
 * alive and unchanged until rendering completes. loading_artwork accepts ICON
 * only. Explicit SPINNER/PROGRESS_CIRCLE leading artwork survives loading.
 * leading_content takes priority. Button metrics override artwork dimensions.
 * Group borrows immutable descriptors until group_ex returns; standalone Button
 * copies their values before click callbacks. Result objects must not alias resources.
 * Layout-only composition writes neither resources nor results. */
typedef enum {
    CUI_BUTTON_ARTWORK_ICON,
    CUI_BUTTON_ARTWORK_SPINNER,
    CUI_BUTTON_ARTWORK_PROGRESS_CIRCLE
} cui_button_artwork_kind;
typedef enum {
    CUI_BUTTON_ARTWORK_OK,
    CUI_BUTTON_ARTWORK_ICON_FAILURE,
    CUI_BUTTON_ARTWORK_PROGRESS_FAILURE
} cui_button_artwork_status;
typedef struct {
    cui_button_artwork_status status;
    cui_icon_error icon_error;
} cui_button_artwork_result;
struct cui_button_artwork {
    cui_button_artwork_kind kind;
    cui_icon_state *icon; /* Required for ICON; optional custom Spinner icon. */
    cui_icon_options icon_options;
    cui_progress_circle_state *progress; /* Required for PROGRESS_CIRCLE. */
    cui_progress_circle_options progress_options;
    float fraction;
    cui_button_artwork_result *result; /* Optional; local paint failures only. */
};

/* Caller owns pixel storage; no resource allocation. Returns animation demand.
 * Optional child callback composes centered content. Invalid arguments submit
 * no UI, do not invoke children and preserve motion/buffer/state. */
bool cui_progress_circle(cui_context *, cui_id, const cui_theme *, float fraction,
                         cui_progress_circle_state *, cui_progress_circle_options,
                         cui_button_content_fn, void *user);
void cui_progress(cui_context *, cui_id, const cui_theme *, float fraction, float width);
void cui_separator(cui_context *, cui_id, const cui_theme *, float width);
typedef struct {
    float length; /* Zero defaults to160; explicit positive pixels, at most4096. */
    bool vertical, dashed, has_label, custom_color;
    cui_str label;
    cui_color color;
} cui_separator_options;
cui_separator_options cui_separator_defaults(void);
/* One-pixel line; dashed uses4px on/2px off. Label, including an explicitly
 * empty label, overlays the line with theme background and8px/4px padding.
 * Text is borrowed through renderer completion. Invalid options submit nothing.
 * Length is explicit; caller resolves parent fill extent. No keyboard behavior. */
bool cui_separator_ex(cui_context *, cui_id, const cui_theme *, cui_separator_options);

#ifdef __cplusplus
}
#endif
#include "cui_radio_group.h"
#include "cui_settings.h"
#endif
