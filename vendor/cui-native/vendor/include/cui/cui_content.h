/* SPDX-License-Identifier: MIT */
#ifndef CUI_CONTENT_H
#define CUI_CONTENT_H
#include "cui_base.h"
#include "cui_icon.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_ALERT_INFO,
    CUI_ALERT_SUCCESS,
    CUI_ALERT_WARNING,
    CUI_ALERT_ERROR,
    CUI_ALERT_DEFAULT
} cui_alert_kind;
typedef void (*cui_content_slot_fn)(cui_context *, const cui_theme *, cui_color foreground,
                                    void *user);
typedef struct {
    cui_alert_kind kind;
    cui_str title, message, icon;
    float width, scale;
    bool banner;
    /* Optional controlled visibility; false omits layout and semantics. */
    bool *visible;
    bool dismissible;
    /* Named SVG; empty selects the kind's default. Legacy icon text overrides it. */
    cui_str icon_name;
    cui_content_slot_fn title_content, content, action;
    void *user;
    cui_str accessible_name; /* Required for custom content without message text. */
} cui_alert_options;
bool cui_alert(cui_context *ui, cui_id id, const cui_theme *theme, cui_alert_options options);
/* Writes first/last word initials (UTF-8 scalars), or "?". Requires 9 bytes. */
size_t cui_avatar_initials(cui_str name, char output[9]);
/* Requires a unique nonzero ID; zero reports CUI_ERROR_INVALID_ACTION. */
void cui_avatar(cui_context *ui, cui_id id, const cui_theme *theme, cui_str name, float size);
/* Caller-owned circular cover cache, no allocation. Storage must be disjoint
 * from source pixels and stay live through rendering. diameter:1..512; capacity
 * >=diameter*diameter*4. Revision/pointer/geometry changes rebuild the cache. */
typedef struct {
    unsigned char *storage;
    size_t capacity;
    unsigned diameter;
    cui_bitmap source, bitmap;
    bool ready;
} cui_avatar_image;
bool cui_avatar_image_init(cui_avatar_image *, unsigned char *storage, size_t capacity,
                           unsigned diameter);
/* Failure leaves the cache unchanged. Use the return value for this source,
 * not ready: a retained successful cache does not make a failed source valid. */
bool cui_avatar_image_prepare(cui_avatar_image *, cui_bitmap source);
typedef struct {
    float diameter, font_size;
    cui_text_style text_style;
} cui_avatar_metrics;
/* Explicit variants only. rem: finite (0,512]; false leaves output unchanged.
 * XS/SM/BASE=MD/LG preserve frozen 16/24/48/80px metrics at rem=16.
 * XL..9XL extend the shared scale. A shared scale cap bounds diameter to 512
 * while preserving the variant font-size/diameter ratio. */
bool cui_avatar_measure(cui_design_size, float rem, cui_avatar_metrics *);
typedef struct {
    cui_color background, foreground, border;
} cui_avatar_palette;
/* Deterministic CUI UTF-8 initials hash selects one of 12 OkLCH hues. The
 * assignment differs from GPUI's private hash. custom_hue accepts 0..11.
 * Invalid override/output leaves output unchanged. dark uses theme.dark. */
bool cui_avatar_identity(cui_str name, bool dark, bool custom_hue, unsigned hue,
                         cui_avatar_palette *);
typedef struct {
    float size, ring_width;
    cui_color ring;
    bool custom_ring, interactive, disabled;
    cui_bitmap image;
    cui_avatar_image *image_state;
    /* Fallback colors; flags permit explicit transparent/black colors. */
    bool custom_background, custom_foreground;
    cui_color background, foreground;
    /* INHERIT preserves size and legacy .38 text ratio. Explicit variants win
     * over size; rem_size zero uses 16 logical px. No font is loaded implicitly. */
    cui_design_size design_size;
    float rem_size;
    bool identity_colors, custom_identity_hue;
    unsigned identity_hue; /* 0..11; invalid selection falls back to legacy colors. */
    /* Empty-name fallback only, opt-in; photo and named initials take precedence.
     * Borrowed typed state wins name, then default user icon. State and all mask
     * versions must survive rendering; collect/destroy only after draw completion.
     * Named masks use the bounded context icon cache and expire next begin_frame.
     * Icons are decorative within the Avatar's existing semantic record. */
    bool anonymous_icon;
    cui_str placeholder_name;
    cui_icon_state *placeholder;
} cui_avatar_options;
/* Invalid/missing image/cache falls back to initials (or opt-in anonymous icon).
 * Returns activation
 * only when interactive; image name remains the supplied accessible name. */
bool cui_avatar_ex(cui_context *, cui_id, const cui_theme *, cui_str name, cui_avatar_options);
typedef struct {
    uint64_t key; /* Stable unique application key; zero uses the index. */
    cui_str name;
    cui_bitmap image;
    cui_avatar_image *image_state;
} cui_avatar_item;
typedef struct {
    float size, overlap, width, ring_width;
    size_t max_visible; /* Zero defaults to three photos, plus an overflow badge. */
    bool interactive, disabled;
    cui_str name;
} cui_avatar_stack_options;
/* <=64 items. width>0 reduces visible photos to fit; overlap zero uses size/4.
 * Later photos paint/hit above earlier ones. Returns activated index or SIZE_MAX.
 * The overflow badge reports the remaining count and is passive. */
size_t cui_avatar_stack(cui_context *, cui_id, const cui_theme *, const cui_avatar_item *,
                        size_t count, cui_avatar_stack_options);
typedef enum {
    CUI_BUBBLE_FILLED,
    CUI_BUBBLE_SECONDARY,
    CUI_BUBBLE_MUTED,
    CUI_BUBBLE_TINTED,
    CUI_BUBBLE_OUTLINE,
    CUI_BUBBLE_GHOST,
    CUI_BUBBLE_DESTRUCTIVE,
    CUI_BUBBLE_INFO,
    CUI_BUBBLE_SUCCESS,
    CUI_BUBBLE_WARNING,
    CUI_BUBBLE_ERROR
} cui_bubble_variant;
/* Composable surface. Alignment and wrapping belong to the parent layout. */
void cui_begin_bubble(cui_context *ui, cui_id id, const cui_theme *theme,
                      cui_bubble_variant variant, float width);
void cui_end_bubble(cui_context *ui);
/* Slot foreground is semantic; custom children choose their own paint explicitly. */
cui_color cui_bubble_foreground(const cui_theme *, cui_bubble_variant);
typedef enum { CUI_BUBBLE_INCOMING, CUI_BUBBLE_OUTGOING } cui_bubble_alignment;
typedef enum { CUI_BUBBLE_REACTIONS_BOTTOM, CUI_BUBBLE_REACTIONS_TOP } cui_bubble_reaction_side;
typedef enum {
    CUI_BUBBLE_ANNOUNCE_NONE,
    CUI_BUBBLE_ANNOUNCE_POLITE,
    CUI_BUBBLE_ANNOUNCE_ASSERTIVE
} cui_bubble_announcement;
/* Optional caller-owned measurement cache; zero initialize, never share between IDs. */
typedef struct {
    cui_rect surface, reactions, row;
    bool measured;
    uint64_t revision;
    float width, font_size, padding;
    unsigned configuration;
} cui_bubble_state;
typedef struct {
    cui_bubble_variant variant;
    cui_bubble_alignment alignment;
    cui_bubble_reaction_side reaction_side;
    cui_bubble_alignment reaction_alignment;
    cui_bubble_announcement announcement;
    float width; /* Row width; surface is capped at 80%, ghost at 100%. */
    cui_str message, accessible_name;
    cui_content_slot_fn content, reactions;
    void *user;
    cui_bubble_state *state;   /* NULL retains normal-flow reactions. */
    uint64_t content_revision; /* Increment when custom slot geometry changes. */
} cui_bubble_options;
/* Synchronous borrowed slots. Reaction controls retain their base button semantics. */
void cui_bubble(cui_context *, cui_id, const cui_theme *, cui_bubble_options);
/* Source-composed Empty. Borrowed pointers and callbacks live through this call.
 * Obtain defaults, then change desired fields for independent style refinements.
 * Custom children own their IDs and ordinary native control behavior. */
typedef enum {
    CUI_EMPTY_ROOT,
    CUI_EMPTY_HEADER,
    CUI_EMPTY_MEDIA,
    CUI_EMPTY_TITLE,
    CUI_EMPTY_DESCRIPTION,
    CUI_EMPTY_CONTENT
} cui_empty_part;
typedef enum { CUI_EMPTY_MEDIA_DEFAULT, CUI_EMPTY_MEDIA_ICON } cui_empty_media_variant;
typedef struct {
    cui_box box;
    float font_size, line_height;
    cui_color foreground;
    cui_text_style text;
} cui_empty_style;
typedef struct {
    const cui_empty_style *style;
    cui_str text;
    cui_content_slot_fn children;
    void *user;
} cui_empty_slot;
typedef struct {
    const cui_empty_style *style;
    const cui_empty_slot *media, *title, *description;
    cui_empty_media_variant media_variant;
} cui_empty_header;
typedef struct {
    const cui_empty_style *style;
    const cui_empty_header *header;
    const cui_empty_slot *content;
    cui_content_slot_fn children;
    void *user;
    cui_str accessible_name;
    float rem; /* zero selects 16 logical pixels; independent of theme font size. */
} cui_empty_options;
cui_empty_style cui_empty_default_style(const cui_theme *, cui_empty_part, cui_empty_media_variant,
                                        float rem);
/* Rejects malformed styles/spans before any layout, callback or semantic mutation.
 * Slots are rendered header(media/title/description), content, appended children. */
bool cui_empty_ex(cui_context *, cui_id, const cui_theme *, cui_empty_options);
void cui_empty(cui_context *ui, cui_id id, const cui_theme *theme, cui_str title,
               cui_str description, float width);
void cui_begin_group_box(cui_context *ui, cui_id id, const cui_theme *theme, cui_str title,
                         float width);
void cui_end_group_box(cui_context *ui);
/* Platform convention selected explicitly; no platform types in public API. */
size_t cui_kbd_format(unsigned modifiers, cui_str key, bool mac_symbols, char *output,
                      size_t capacity);
void cui_kbd(cui_context *ui, cui_id id, const cui_theme *theme, cui_str chord);
/* Seconds are application-owned; invalid/negative values resolve to zero. */
float cui_loading_phase(double seconds, float period);
void cui_spinner(cui_context *ui, cui_id id, const cui_theme *theme, float size, float phase,
                 bool reduced_motion);
void cui_skeleton(cui_context *ui, cui_id id, const cui_theme *theme, float width, float height,
                  float radius, float phase, bool secondary, bool reduced_motion);
/* Opt-in, caller-clocked loading motion. State is caller-owned, contains no
 * allocation, and must not be shared by independent animations. Zero duration
 * selects the source period (.8 seconds Spinner, 2 seconds Skeleton). */
typedef struct {
    bool ready;
    double started, last_time;
} cui_loading_motion;
typedef float (*cui_loading_ease_fn)(float phase, void *user);
typedef struct {
    float phase, opacity; /* Spinner custom phase is finite turns, potentially outside [0,1]. */
    bool active;
    double next_wake_seconds; /* Absolute caller-clock deadline; zero when static. */
} cui_loading_motion_result;
/* Legacy bounded pure sampler; period in (0,60], now in [0,1e12]. Skeleton
 * uses a triangular bounce. Easing must be finite [0,1]; reduced motion skips it.
 * cui_spinner_ex separately accepts finite turns and evaluates reduced ease(0). */
bool cui_loading_motion_sample(cui_loading_motion *, double now_seconds, float period,
                               bool reduced_motion, bool skeleton, cui_loading_ease_fn,
                               void *ease_user, cui_loading_motion_result *);
/* Callback paints one icon; radians are center-relative, modulo-one-turn rotation. No callback
 * userdata is retained. A failing callback must leave its own paint unchanged. */
typedef bool (*cui_spinner_paint_fn)(cui_context *, cui_id, const cui_theme *, float size,
                                     cui_color, float radians, void *user);
/* Optional source size identity. NULL sizing retains legacy options.size pixels.
 * Named sizes resolve .75/.875/1/1.5 rem; rem_size=0 selects 16 logical pixels.
 * CUSTOM uses pixels, independent of rem_size and theme font size. */
typedef enum {
    CUI_SPINNER_CUSTOM,
    CUI_SPINNER_XSMALL,
    CUI_SPINNER_SMALL,
    CUI_SPINNER_MEDIUM,
    CUI_SPINNER_LARGE
} cui_spinner_size;
typedef struct {
    cui_spinner_size size;
    float pixels, rem_size;
} cui_spinner_sizing;
typedef struct {
    float size, duration;
    double now_seconds;
    bool reduced_motion, custom_color;
    cui_color color;
    cui_loading_ease_fn ease; /* Finite turns; also evaluated at raw zero when reduced. */
    void *ease_user;
    cui_spinner_paint_fn paint;
    void *paint_user;
    const cui_spinner_sizing *sizing; /* Borrowed for submission; overrides size. */
} cui_spinner_options;
typedef struct {
    float width, height, radius, duration;
    double now_seconds;
    bool secondary, reduced_motion;
    cui_loading_ease_fn ease;
    void *ease_user;
} cui_skeleton_options;
/* Source Loader, 16 logical pixels, theme foreground, .8-second quadratic loop.
 * Without sizing, size remains an explicit pixel size (legacy default 16).
 * Parent boxes provide layout/padding/opacity/clip; see docs/SPINNER.md. */
cui_spinner_options cui_spinner_options_default(void);
cui_skeleton_options cui_skeleton_options_default(void);
/* Paint-only, source-compatible semantics: neither adds a focus stop or busy
 * status. On success caller schedules result.next_wake_seconds; these APIs do
 * not invalidate perpetually. Legacy phase APIs above retain their behavior. */
bool cui_spinner_ex(cui_context *, cui_id, const cui_theme *, cui_loading_motion *,
                    cui_spinner_options, cui_loading_motion_result *);
bool cui_skeleton_ex(cui_context *, cui_id, const cui_theme *, cui_loading_motion *,
                     cui_skeleton_options, cui_loading_motion_result *);
void cui_divider(cui_context *ui, cui_id id, const cui_theme *theme, float length, bool vertical);
/* Shared delayed lifecycle for hover cards/tooltips, independent of styling. */
typedef struct {
    bool open, pending, target, suppressed;
    float elapsed;
} cui_hover_state;
bool cui_hover_update(cui_hover_state *state, bool trigger_hovered, bool content_hovered,
                      bool focused, bool escape, bool disabled, float dt, float open_delay,
                      float close_delay);
typedef struct {
    cui_id trigger;
    cui_rect viewport;
    cui_popup_side side;
    float width, height, gap, dt, open_delay, close_delay;
    bool disabled;
    /* HoverCard only: height<=0 fits children; positive max_height bounds the
     * intrinsic panel. Increment revision when arbitrary child content changes
     * without a width change so collision placement receives a measured redraw. */
    float max_height;
    uint64_t content_revision;
} cui_hover_options;
/* Trigger must already have been submitted. State is persistent/application-owned.
 * Explicit dimensions clip content, flip at viewport edges, and bound placement. */
bool cui_tooltip(cui_context *ui, cui_id id, const cui_theme *theme, cui_hover_state *state,
                 cui_str text, cui_hover_options options);
/* Pair end only when begin returns true; accepts arbitrary children without a
 * modal focus trap. Hover/focus inside the open card extends its lifetime.
 * Default height fits children and is bounded by the viewport. Collision
 * placement settles after the retained measurement redraw; explicit positive
 * height remains fixed. Context owns measurement storage, not the caller. */
bool cui_begin_hover_card(cui_context *ui, cui_id id, const cui_theme *theme,
                          cui_hover_state *state, cui_str name, cui_hover_options options);
void cui_end_hover_card(cui_context *ui);
#ifdef __cplusplus
}
#endif
#endif
