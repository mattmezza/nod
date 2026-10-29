/* SPDX-License-Identifier: MIT */
#ifndef CUI_H
#define CUI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define CUI_VERSION "0.1.0-alpha.3"
typedef uint64_t cui_id;
typedef struct {
    const char *data;
    size_t length;
} cui_str;
/* String-literal initializer; expression form: (cui_str)CUI_STR("text"). */
#define CUI_STR(literal) {"" literal, sizeof(literal) - 1}
/* Full paragraph plus caller-selected line, in original UTF-8 bytes.
 * Hard paragraph separators are excluded; U+2028 retains paragraph context. */
typedef struct {
    cui_str paragraph;
    size_t byte_start, byte_end; /* Half-open; paragraph-relative. */
} cui_text_line_source;
typedef struct {
    float x, y, w, h;
} cui_rect;
typedef struct {
    uint8_t r, g, b, a;
} cui_color;
/* Alpha-premultiplied Oklab interpolation; amount is the second color weight.
 * Clamp to [0,1]; non-finite amount selects the first color. */
cui_color cui_color_mix_oklab(cui_color first, cui_color second, float amount);
typedef struct cui_context cui_context;
typedef enum {
    CUI_OK,
    CUI_ERROR_CAPACITY,
    CUI_ERROR_DUPLICATE_ID,
    CUI_ERROR_LIFECYCLE,
    CUI_ERROR_LAYOUT,
    CUI_ERROR_INVALID_ACTION
} cui_error;
/* Diagnostics are bounded value snapshots. Error state is sticky until clear_error.
 * Paths contain outermost open box IDs (including anonymous zero IDs). */
#define CUI_DIAGNOSTIC_PATH_MAX 16
#define CUI_DIAGNOSTIC_OPERATION_MAX 48
#define CUI_DIAGNOSTIC_MESSAGE_MAX 160
#define CUI_DIAGNOSTIC_SUBSYSTEM_MAX 48
typedef enum {
    CUI_DIAGNOSTIC_NONE = 0,
    CUI_DIAGNOSTIC_CAPACITY = 1,
    CUI_DIAGNOSTIC_DUPLICATE_ID = 2,
    CUI_DIAGNOSTIC_LIFECYCLE = 3,
    CUI_DIAGNOSTIC_LAYOUT = 4,
    CUI_DIAGNOSTIC_INVALID_ACTION = 5,
    CUI_DIAGNOSTIC_ZERO_ID = 6,
    CUI_DIAGNOSTIC_BOX_DEPTH = 7,
    CUI_DIAGNOSTIC_ELEMENTS = 8,
    CUI_DIAGNOSTIC_UNBALANCED_SCOPE = 9
} cui_diagnostic_detail;
typedef struct {
    cui_error error;
    cui_diagnostic_detail detail;
    char operation[CUI_DIAGNOSTIC_OPERATION_MAX];
    char message[CUI_DIAGNOSTIC_MESSAGE_MAX];
    char subsystem[CUI_DIAGNOSTIC_SUBSYSTEM_MAX];
    uint64_t frame_sequence;
    cui_id id;
    size_t used, requested, limit; /* Budget units follow subsystem; zero limit is unknown. */
    cui_id path[CUI_DIAGNOSTIC_PATH_MAX];
    size_t path_count;
    bool path_truncated;
} cui_diagnostic;
/* Callback receives a borrowed snapshot. first/latest_diagnostic, last_error,
 * frame_active and other pure getters are safe. Context mutations (including
 * destroy, begin/end_frame, resource_find and report) are rejected during it.
 * Do not reenter host callbacks or retain the borrowed pointer. */
typedef void (*cui_diagnostic_fn)(void *, const cui_diagnostic *);
void cui_set_diagnostic_callback(cui_context *, cui_diagnostic_fn, void *user);
bool cui_first_diagnostic(const cui_context *, cui_diagnostic *out);
bool cui_latest_diagnostic(const cui_context *, cui_diagnostic *out);
/* For application/component failures. Captures the current open box path. */
void cui_report_diagnostic(cui_context *, const cui_diagnostic *);
typedef enum { CUI_DEBUG_LAYOUT_TEXT, CUI_DEBUG_LAYOUT_JSON } cui_debug_layout_format;
/* Opt in before composition. Exports box nodes only, after end_frame and before
 * the next begin_frame. required includes NUL; insufficient buffers are untouched.
 * IDs are JSON strings to preserve all 64 bits. Requested dimensions retain
 * fixed/percent flags and maxima; nonfinite requests export as null. Bounds and
 * clips are resolved geometry. No allocation, file I/O or locale mutation. */
void cui_set_layout_debug(cui_context *, bool enabled);
bool cui_debug_layout_export(cui_context *, cui_debug_layout_format, char *buffer, size_t capacity,
                             size_t *required);
typedef enum {
    CUI_EVENT_POINTER_MOVE,
    CUI_EVENT_POINTER_DOWN,
    CUI_EVENT_POINTER_UP,
    CUI_EVENT_KEY_DOWN,
    CUI_EVENT_KEY_UP,
    CUI_EVENT_TEXT,
    CUI_EVENT_SCROLL,
    CUI_EVENT_CANCEL,
    CUI_EVENT_GESTURE, /* Opt-in viewport pan/pinch; scale is relative to previous event. */
    CUI_EVENT_COMPOSITION_START,
    CUI_EVENT_COMPOSITION_UPDATE,
    CUI_EVENT_COMPOSITION_COMMIT,
    CUI_EVENT_COMPOSITION_CANCEL,
    CUI_EVENT_ACCESSIBILITY_ACTION,
    /* Host-surface hover presence only; coordinates are ignored. ENTER never
     * invents a position. LEAVE preserves position, capture, focus and contacts. */
    CUI_EVENT_POINTER_ENTER,
    CUI_EVENT_POINTER_LEAVE
} cui_event_type;
/* Targeted semantic actions, independent of pointer or keyboard input. */
typedef enum { CUI_ACCESSIBILITY_FOCUS, CUI_ACCESSIBILITY_ACTIVATE } cui_accessibility_action;
typedef enum {
    CUI_KEY_NONE,
    CUI_KEY_TAB,
    CUI_KEY_ENTER,
    CUI_KEY_SPACE,
    CUI_KEY_ESCAPE,
    CUI_KEY_LEFT,
    CUI_KEY_RIGHT,
    CUI_KEY_UP,
    CUI_KEY_DOWN,
    CUI_KEY_HOME,
    CUI_KEY_END,
    CUI_KEY_BACKSPACE,
    CUI_KEY_DELETE,
    CUI_KEY_A,
    CUI_KEY_Z,
    CUI_KEY_Y,
    CUI_KEY_C,
    CUI_KEY_X,
    CUI_KEY_V,
    CUI_KEY_PAGEUP,
    CUI_KEY_PAGEDOWN,
    /* Appended to preserve the original key values used by host adapters. */
    CUI_KEY_B,
    CUI_KEY_D,
    CUI_KEY_E,
    CUI_KEY_F,
    CUI_KEY_G,
    CUI_KEY_H,
    CUI_KEY_I,
    CUI_KEY_J,
    CUI_KEY_K,
    CUI_KEY_L,
    CUI_KEY_M,
    CUI_KEY_N,
    CUI_KEY_O,
    CUI_KEY_P,
    CUI_KEY_Q,
    CUI_KEY_R,
    CUI_KEY_S,
    CUI_KEY_T,
    CUI_KEY_U,
    CUI_KEY_W,
    CUI_KEY_0,
    CUI_KEY_1,
    CUI_KEY_2,
    CUI_KEY_3,
    CUI_KEY_4,
    CUI_KEY_5,
    CUI_KEY_6,
    CUI_KEY_7,
    CUI_KEY_8,
    CUI_KEY_9,
    CUI_KEY_LEFT_BRACKET,
    CUI_KEY_RIGHT_BRACKET,
} cui_key;
/* ASCII shortcut identity, independent of text/IME input. Letters normalize to
 * uppercase; unsupported characters/keys map to NONE/zero. */
cui_key cui_key_from_character(char character);
char cui_key_character(cui_key key);
enum { CUI_MOD_SHIFT = 1, CUI_MOD_CTRL = 2, CUI_MOD_ALT = 4, CUI_MOD_META = 8 };
typedef enum { CUI_POINTER_MOUSE, CUI_POINTER_TOUCH, CUI_POINTER_PEN } cui_pointer_type;
typedef enum { CUI_POINTER_PRIMARY, CUI_POINTER_SECONDARY, CUI_POINTER_MIDDLE } cui_pointer_button;
typedef enum {
    CUI_SCROLL_PHASE_NONE,
    CUI_SCROLL_PHASE_STARTED,
    CUI_SCROLL_PHASE_MOVED,
    CUI_SCROLL_PHASE_ENDED,
    CUI_SCROLL_PHASE_CANCELLED
} cui_scroll_phase;
typedef struct {
    cui_event_type type;
    float x, y, dx, dy;
    cui_key key;
    unsigned modifiers;
    bool repeat;
    cui_pointer_button pointer_button; /* Zero preserves primary-button initializers. */
    char text[64];
    /* Optional UTF-8 payload. Queue copies bytes; dispatched storage lives until
     * next begin_frame. Selection offsets are UTF-8 bytes in preedit. */
    const char *text_data;
    size_t text_length, composition_anchor, composition_cursor;
    cui_id composition_target; /* Optional originating focus; stale host updates are dropped. */
    /* Optional host gesture metadata. Zero preserves legacy wheel steps.
     * Precise deltas are logical pixels; zero-delta end/cancel events are valid. */
    bool scroll_precise;
    cui_scroll_phase scroll_phase;
    /* Host keyboard context: shortcut handlers must yield to composition and
     * preferred character input. Zero preserves ordinary shortcut delivery. */
    bool ime_composing, prefer_character_input;
    /* Host pointer click sequence count. Zero or one denotes an ordinary click. */
    unsigned click_count;
    cui_pointer_type pointer_type; /* Zero preserves mouse callers. */
    uint64_t pointer_id;           /* Contact identity, scoped to type/device. */
    uint64_t pointer_device;       /* Zero for hosts without a device namespace. */
    float gesture_scale;           /* Relative pinch multiplier; 1 for pan/end/cancel. */
    cui_id accessibility_target;
    cui_accessibility_action accessibility_action;
} cui_event;
/* Caller-owned gesture filter. Clock is monotonic seconds; no allocation.
 * Precise gestures lock their initial dominant axis, release on strong turns,
 * and reset after28ms silence or explicit end/cancel. Line wheels are unchanged.
 * Invalid input leaves both state and event unchanged. */
typedef struct {
    double last_time;
    bool has_last, locked, horizontal;
} cui_scroll_gesture;
bool cui_scroll_gesture_filter(cui_scroll_gesture *, cui_event *, double now);

typedef enum {
    CUI_ROLE_NONE,
    CUI_ROLE_BUTTON,
    CUI_ROLE_CHECKBOX,
    CUI_ROLE_SWITCH,
    CUI_ROLE_RADIO,
    CUI_ROLE_SLIDER,
    CUI_ROLE_TEXTBOX,
    CUI_ROLE_LABEL,
    CUI_ROLE_TAB,
    CUI_ROLE_GROUP,
    CUI_ROLE_TABLIST,
    CUI_ROLE_TABPANEL,
    CUI_ROLE_LIST,
    CUI_ROLE_LISTBOX,
    CUI_ROLE_OPTION,
    CUI_ROLE_MENU,
    CUI_ROLE_MENUITEM,
    CUI_ROLE_DIALOG,
    CUI_ROLE_ALERT,
    CUI_ROLE_STATUS,
    CUI_ROLE_IMAGE,
    CUI_ROLE_PROGRESSBAR,
    CUI_ROLE_TREE,
    CUI_ROLE_TREEITEM,
    CUI_ROLE_MENUITEMCHECKBOX,
    CUI_ROLE_MENUITEMRADIO,
    CUI_ROLE_TABLE,
    CUI_ROLE_ROW,
    CUI_ROLE_CELL,
    CUI_ROLE_COLUMNHEADER,
    CUI_ROLE_GRIDCELL,
    CUI_ROLE_GRID,
    CUI_ROLE_SCROLLBAR,
    CUI_ROLE_ROWHEADER,
    CUI_ROLE_ROWGROUP,
    CUI_ROLE_ALERTDIALOG,
    CUI_ROLE_LINK,
    CUI_ROLE_LISTITEM,
    CUI_ROLE_LOG,
    CUI_ROLE_TOOLBAR,
    CUI_ROLE_SEPARATOR,
    CUI_ROLE_REGION,
    CUI_ROLE_RADIOGROUP
} cui_role;
typedef enum {
    CUI_ORIENTATION_UNSPECIFIED,
    CUI_ORIENTATION_HORIZONTAL,
    CUI_ORIENTATION_VERTICAL
} cui_orientation;
typedef enum {
    CUI_CURSOR_DEFAULT,
    CUI_CURSOR_POINTER,
    CUI_CURSOR_TEXT,
    CUI_CURSOR_RESIZE_HORIZONTAL,
    CUI_CURSOR_RESIZE_VERTICAL,
    CUI_CURSOR_MOVE,
    CUI_CURSOR_NOT_ALLOWED,
    CUI_CURSOR_CROSSHAIR,
    CUI_CURSOR_GRAB,
    CUI_CURSOR_GRABBING,
    CUI_CURSOR_ZOOM_IN,
    CUI_CURSOR_ZOOM_OUT,
    CUI_CURSOR_WAIT,
    CUI_CURSOR_HELP
} cui_cursor;
typedef enum {
    CUI_TEXT_INPUT_TEXT = 0,
    CUI_TEXT_INPUT_PASSWORD_HIDDEN,
    CUI_TEXT_INPUT_PASSWORD_VISIBLE
} cui_text_input_type;
typedef enum {
    CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED = 0,
    CUI_TEXT_INPUT_PURPOSE_PASSWORD,
    CUI_TEXT_INPUT_PURPOSE_NEW_PASSWORD
} cui_text_input_purpose;
typedef struct {
    cui_text_input_type type;
    cui_text_input_purpose purpose;
} cui_text_input_config;
typedef struct {
    cui_id owner;
    cui_text_input_config config;
    uint64_t generation;
    bool explicit_config, multiline;
} cui_text_input_state;
typedef struct {
    bool expanded, selected, readonly, multiline, required, invalid, multiselectable, busy,
        scrollable;
    cui_orientation orientation;
    cui_id controls, labelled_by, parent;
    /* Passive label/addon pointer clicks focus this target before later queued keys. */
    cui_id focus_target;
    cui_str description;
    float minimum, maximum;
    unsigned level, pos_in_set, set_size;
    unsigned row_index, column_index, row_count, column_count;
    float scroll_x, scroll_y, scroll_max_x, scroll_max_y;
    /* Route only dominant-axis wheel motion for the declared orientation. */
    bool scroll_axis_lock;
    /* Horizontal masks retain horizontal-dominant wheel events at both edges,
     * including when content fits. Normal scroll owners leave this false. */
    bool scroll_trap_horizontal;
    bool scroll_gesture_lock; /* Opt-in per-owner precise gesture filtering. */
    /* Opt a normally passive semantic container into keyboard/pointer focus. */
    bool focusable;
    /* Primary pointer activation without opting into focus or keyboard activation. */
    bool pointer_activatable;
    bool handles_tab; /* Focused editable control consumes Tab and Shift-Tab itself. */
    cui_cursor cursor;
    bool no_focus; /* Suppress role-default focus; pointer activation remains opt-in. */
    bool gesture_pan, gesture_zoom; /* Explicit viewport opt-in; disabled by default. */
    bool touch_drag_priority;   /* Custom drag control retains pointer moves over ancestor pan. */
    bool touch_scroll_disabled; /* Scroll owners accept touch pan by default. */
    /* Preserve keyboard/programmatic focus while refusing pointer focus.
     * Set pointer_activatable as well to retain pointer activation/capture. */
    bool no_pointer_focus;
    /* Opt-in previous-frame hit barrier for pointer activation/focus only.
     * A topmost clipped/modal-eligible barrier denies activation beneath it,
     * including release, without cancelling existing capture or blocking scroll.
     * Hover, cursor and current-prepaint occlusion remain independent. */
    bool blocks_pointer_activation;
    bool secret, password;
    /* Internal frame registration; use set_text_input_config instead. */
    bool text_input_configured;
    cui_text_input_config text_input_config;
    /* Opt into unmodified Enter/Space release activation. A qualifying key down
     * latches the current focus generation; either activation-key up may fire.
     * Zero preserves the existing Enter-down and Space-up behavior. */
    bool release_key_activation;
} cui_semantic_details;
typedef struct {
    cui_id id;
    cui_role role;
    cui_rect bounds;
    cui_str name;
    bool disabled, checked, focused;
    float value;
    cui_semantic_details details;
    int tab_index; /* Legacy negative values exclude Tab; set_tab_index opts into signed order. */
    bool tab_ordered, tab_stop_disabled;
    /* Frame-owned lexicographic group prefix; applications use begin/end_tab_group. */
    int tab_path[32];
    size_t tab_path_count;
} cui_semantic;
typedef struct {
    bool hovered, pressed, focused, focus_visible, activated;
} cui_interaction;
typedef enum { CUI_ALIGN_START, CUI_ALIGN_CENTER, CUI_ALIGN_END } cui_align;
typedef struct {
    float top_left, top_right, bottom_left, bottom_right;
} cui_corner_radii;
/* Bounded drop shadow; blur sigma is standard deviation, in logical pixels. */
enum { CUI_DROP_SHADOW_MAX = 8 };
typedef struct {
    float offset_x, offset_y, sigma, spread;
    cui_color color;
} cui_drop_shadow;
typedef struct {
    cui_rect caster;
    cui_corner_radii radii; /* Clamped to original caster, never to spread bounds. */
    cui_drop_shadow layer;
} cui_drop_shadow_paint;
typedef enum { CUI_BOX_SHADOW_DROP = 0, CUI_BOX_SHADOW_INSET = 1 } cui_box_shadow_kind;
typedef struct {
    cui_drop_shadow layer;
    cui_box_shadow_kind kind;
} cui_box_shadow;
enum { CUI_BOX_SHADOW_MAX = CUI_DROP_SHADOW_MAX };
typedef cui_drop_shadow_paint cui_inset_shadow_paint;
/* Source named DROP lists; default/none means no layers, never an implicit shadow. */
typedef enum {
    CUI_SHADOW_NONE = 0,
    CUI_SHADOW_2XS,
    CUI_SHADOW_XS,
    CUI_SHADOW_SM,
    CUI_SHADOW_MD,
    CUI_SHADOW_LG,
    CUI_SHADOW_XL,
    CUI_SHADOW_2XL
} cui_shadow_preset;
enum { CUI_SHADOW_PRESET_MAX = 2 };
/* Checked source constructor: DROP, sigma/spread zero, explicit color.
 * Finite offsets must lie in [-4096,4096]; invalid input leaves output untouched. */
bool cui_box_shadow_init(float offset_x, float offset_y, cui_color, cui_box_shadow *out);
/* Copy ordered layers (black alpha13/26/64). Output count must not overlap the
 * written layer span. Invalid enum, capacity/overflow or pointers leave all outputs
 * untouched. NONE writes count0 only; its layer buffer may be NULL. */
bool cui_shadow_preset_layers(cui_shadow_preset, cui_drop_shadow *out, size_t capacity,
                              size_t *count);

/* Invalid input leaves output untouched. Logical support is metadata; checked physical covering
 * determines empty paint. */
bool cui_drop_shadow_bounds(cui_drop_shadow_paint, cui_rect *out);
typedef struct {
    float left, right, top, bottom;
} cui_border_edges;
typedef struct {
    float width, height, padding, gap, radius;
    bool row, clip;
    cui_color background, border;
    float border_width;
    cui_align align_x, align_y; /* Physical horizontal/vertical axes in rows and columns. */
    float offset_x, offset_y;   /* Child translation in clipped boxes; negative scrolls forward. */
    cui_corner_radii corner_radii;
    cui_border_edges border_edges;
    /* Flags preserve zero corners/edges. Radii are clamped to half the shorter
     * side by adapters; border widths use the same integer layout units as
     * border_width. Negative/nonfinite box values normalize to zero. */
    bool custom_corners, custom_border_edges; /* Replace uniform radius/width, including zero. */
    /* Explicit zero dimensions; otherwise zero retains the FIT convention. */
    bool fixed_width, fixed_height;
    /* Measure/layout descendants, including portals, without paint, input or
     * semantics. Application callbacks still run and must avoid side effects. */
    bool layout_only;
    /* Preserve painting/layout but suppress descendant semantics and input. */
    bool inert;
    /* Positive finite caps apply to FIT/GROW/fixed; zero means unbounded. */
    float max_width, max_height;
    /* Inherited per-draw opacity; custom_opacity distinguishes zero from default1.
     * Finite values clamp to0..1; nonfinite values normalize to0. */
    float opacity;
    bool custom_opacity;
    /* Interpret the corresponding dimension as a fraction of the parent inner extent in0..1.
     * Fraction flags take precedence over fixed flags and max caps. */
    bool width_percent, height_percent;
    /* Replace uniform padding with physical edges, including explicit zero. */
    cui_border_edges padding_edges;
    bool custom_padding;
} cui_box;
/* Borrowed straight-alpha RGBA8 pixels, valid through renderer completion.
 * Increment revision whenever pixels change; stride includes row padding. */
typedef struct {
    const unsigned char *pixels;
    unsigned width, height;
    size_t stride;
    uint64_t revision;
} cui_bitmap;
typedef struct {
    cui_bitmap bitmap;
    cui_rect source;
    float opacity;
    bool grayscale;
    cui_color tint;
    bool custom_tint;
} cui_bitmap_paint;
bool cui_bitmap_valid(cui_bitmap);
/* A zero source rectangle selects the whole bitmap. Opacity is explicitly 0..1.
 * FIT dimensions default to source pixel dimensions. */
bool cui_paint_bitmap(cui_context *, cui_id, cui_bitmap_paint, cui_box);
/* Expanded, non-indexed triangles. XY is local logical geometry; ST contains
 * analytic quadratic coverage coordinates and is never scaled/translated. */
typedef struct {
    float x, y, s, t;
} cui_mesh_vertex;
/* Linear RGB premultiplied by alpha, before coverage. RGB remains unclamped
 * (for out-of-gamut/HDR color); alpha is finite in0..1. Adapters convert to
 * their target color space before blending. */
typedef struct {
    float r, g, b, a;
} cui_linear_color;
typedef struct {
    const cui_mesh_vertex *vertices;
    size_t vertex_count;
    float origin_x, origin_y, scale;
    cui_linear_color color;
} cui_mesh_paint;
/* Positive multiple-of-three count; finite vertices/origin/color; scale>0;
 * finite transformed bounds. Degenerate triangles are valid empty geometry. */
bool cui_mesh_valid(cui_mesh_paint);
/* Borrowed vertices remain valid through renderer completion. Box placement
 * adds to origin; only XY is transformed. FIT defaults to transformed AABB
 * spans, without normalizing local coordinates; origin=-min normalizes them.
 * Ancestor clips apply, including geometry extending outside the layout box. */
bool cui_paint_mesh(cui_context *, cui_id, cui_mesh_paint, cui_box);
typedef enum {
    CUI_DRAW_RECT,
    CUI_DRAW_BORDER,
    CUI_DRAW_TEXT,
    CUI_DRAW_CLIP_PUSH,
    CUI_DRAW_CLIP_POP,
    CUI_DRAW_IMAGE,
    CUI_DRAW_MESH,
    CUI_DRAW_DROP_SHADOW,
    CUI_DRAW_INSET_SHADOW
} cui_draw_type;
typedef enum {
    CUI_TEXT_WEIGHT_NORMAL,
    CUI_TEXT_WEIGHT_MEDIUM,
    CUI_TEXT_WEIGHT_SEMIBOLD,
    CUI_TEXT_WEIGHT_BOLD
} cui_text_weight;
typedef struct {
    cui_text_weight weight;
    bool italic, monospace;
    uint32_t family; /* Application-defined font-family ID; zero selects default. */
    /* Logical pixels between intact visual shaping clusters, never after a line.
     * Requires CUI_TEXT_CAP_LETTER_SPACING; zero preserves natural shaping.
     * Finite signed values; negative gaps must not reverse cluster progression. */
    float letter_spacing;
} cui_text_style;
enum { CUI_TEXT_COLOR_SPAN_MAX = 4096 };
typedef struct {
    size_t byte_start, byte_end; /* Paragraph-relative; sorted, nonoverlapping. */
    cui_color color;             /* A shaped cluster takes the color at its first byte. */
} cui_text_color_span;
/* Spatial text paint. Linear endpoints are normalized to the selected anchor
 * on submission, resolved to global logical coordinates in draw commands.
 * Stops interpolate premultiplied RGBA in encoded sRGB; endpoint colors extend.
 * Duplicate positions form hard edges (later stop wins at the boundary).
 * Core copies 2..8 sorted stops; draw-list storage lasts until next begin_frame. */
typedef enum { CUI_TEXT_PAINT_SOLID, CUI_TEXT_PAINT_LINEAR } cui_text_paint_kind;
typedef enum { CUI_TEXT_ANCHOR_BLOCK, CUI_TEXT_ANCHOR_LINE } cui_text_paint_anchor;
typedef struct {
    float position;
    cui_color color;
} cui_text_gradient_stop;
typedef struct {
    cui_text_paint_kind kind;
    cui_color color; /* Solid only. */
    float x0, y0, x1, y1;
    cui_text_paint_anchor anchor;
    const cui_text_gradient_stop *stops;
    size_t stop_count;
} cui_text_paint;
typedef struct {
    cui_draw_type type;
    cui_rect bounds;
    cui_color color;
    float radius, stroke, font_size;
    cui_text_style text_style;
    cui_str text;
    cui_corner_radii corner_radii;
    cui_border_edges border_edges;
    bool custom_corners, custom_border_edges;
    cui_bitmap_paint bitmap;
    cui_mesh_paint mesh;
    cui_text_line_source text_source;       /* Frame-owned; zero = legacy text. */
    const cui_text_color_span *text_colors; /* Frame-owned. */
    size_t text_color_count;
    cui_text_paint text_paint;    /* Linear text only; solid uses command.color. */
    cui_drop_shadow_paint shadow; /* Value-owned; color includes ancestor opacity. */
} cui_draw_command;
typedef struct {
    const cui_draw_command *commands;
    size_t count;
    float width, height;
} cui_draw_list;
typedef struct {
    float width, height;
} cui_size;
typedef cui_size (*cui_measure_fn)(void *user, cui_str text, float font_size);
typedef cui_size (*cui_measure_styled_fn)(void *user, cui_str text, float font_size,
                                          cui_text_style style);
/* Distances from the baseline in logical pixels. Descent is nonnegative;
 * line_height is the renderer's line spacing, independent of ink bounds. */
typedef struct {
    float ascent, descent, line_height;
} cui_font_metrics;
typedef bool (*cui_font_metrics_fn)(void *user, float font_size, cui_text_style style,
                                    cui_font_metrics *out);
typedef enum { CUI_TEXT_UPSTREAM, CUI_TEXT_DOWNSTREAM } cui_text_affinity;
typedef struct {
    size_t byte_offset, run_index;
    cui_text_affinity affinity;
    float x, y, height;
} cui_text_caret;
typedef struct {
    cui_rect bounds;
    size_t byte_start, byte_end, run_index;
} cui_text_selection_rect;
typedef enum {
    CUI_TEXT_LAYOUT_OK,
    CUI_TEXT_LAYOUT_INVALID,
    CUI_TEXT_LAYOUT_MEMORY,
    CUI_TEXT_LAYOUT_UNAVAILABLE,
    CUI_TEXT_LAYOUT_MISSING_GLYPH,
    CUI_TEXT_LAYOUT_CAPACITY
} cui_text_layout_status;
/* Owned, immutable single-line geometry. Factory consumes borrowed text only
 * synchronously; session must retain everything its queries need. Queries use
 * original UTF-8 offsets. Invalid/capacity failures preserve buffers/counts.
 * At most two caret affinities per boundary. No interior-ligature/GDEF promise. */
typedef struct cui_text_geometry_ops {
    cui_text_layout_status (*carets)(const void *, size_t, cui_text_caret *, size_t, size_t *);
    cui_text_layout_status (*hit_test)(const void *, float, float, cui_text_affinity,
                                       cui_text_caret *);
    cui_text_layout_status (*selection)(const void *, size_t, size_t, cui_text_selection_rect *,
                                        size_t, size_t *);
    void (*release)(void *);
} cui_text_geometry_ops;
typedef struct {
    void *session;
    const cui_text_geometry_ops *ops; /* Must remain valid until release. */
    float width, height;
    size_t byte_length;
} cui_text_geometry;
typedef cui_text_layout_status (*cui_text_geometry_fn)(void *, cui_str, float, cui_text_style,
                                                       cui_text_geometry *);
typedef cui_text_layout_status (*cui_text_geometry_line_fn)(void *, cui_text_line_source, float,
                                                            cui_text_style, cui_text_geometry *);
enum { CUI_TEXT_CAP_LETTER_SPACING = 1u << 0 };
typedef struct {
    float width, height;
    size_t max_elements, max_commands, frame_bytes;
    cui_measure_fn measure;
    cui_measure_styled_fn measure_styled; /* Preferred for regular and styled text. */
    void *measure_user;
    /* Context resources: zero selects 128 slots and 1 MiB of payload storage.
     * Nested allocations owned by payloads require their own explicit limits. */
    size_t max_resources, resource_bytes;
    size_t event_text_bytes;          /* Aggregate queued payload bound; zero = 65536. */
    cui_font_metrics_fn font_metrics; /* Optional; shares measure_user. */
    cui_text_geometry_fn
        text_geometry; /* Optional single-line visual factory; shares measure_user. */
    cui_text_geometry_line_fn text_geometry_line; /* Optional contextual line factory. */
    /* Provider promises consistent measurement, contextual geometry and drawing.
     * Zero keeps existing providers compatible; unsupported tracking fails. */
    uint32_t text_capabilities;
} cui_config;
uint32_t cui_text_capabilities(const cui_context *);
cui_str cui_string(const char *s);
cui_context *cui_create(cui_config config);
void cui_destroy(cui_context *ui);
bool cui_queue_event(cui_context *ui, cui_event event);
/* Completed-frame eligibility only; false while a frame is open. Focus includes
 * programmatic-only tab stops. Activate never changes focus or pointer capture.
 * Queued actions share the bounded input queue and are validated again on dispatch.
 * Missing/disabled/modal-blocked/unsupported targets report INVALID_ACTION;
 * lifecycle/capacity failure leaves the queue and interaction state unchanged. */
bool cui_accessibility_action_supported(const cui_context *, cui_id, cui_accessibility_action);
bool cui_queue_accessibility_action(cui_context *, cui_id, cui_accessibility_action);

bool cui_begin_frame(cui_context *ui, float width, float height, float dt);
bool cui_frame_active(const cui_context *);
cui_draw_list cui_end_frame(cui_context *ui);
cui_id cui_id_from(cui_context *ui, const char *key);
bool cui_push_id(cui_context *ui, const char *key);
void cui_pop_id(cui_context *ui);
/* Paint one rectangle below children of the current open box, clipped to it.
 * Coordinates are relative to the box's current-frame origin; last call wins. */
bool cui_set_underlay(cui_context *, cui_rect, cui_color, cui_corner_radii);
/* Paint an outside border band without affecting layout or filling its interior.
 * Uses current-frame box bounds and ancestor clipping. Zero width clears it. */
bool cui_set_outline(cui_context *, float width, cui_color, cui_corner_radii);
/* Copies <=8 layers immediately; count 0 clears. Paints before background. */
/* Replaces the copied complete mixed list, at most eight total layers. */
bool cui_set_box_shadows(cui_context *, const cui_box_shadow *, size_t count, cui_corner_radii);
bool cui_set_drop_shadows(cui_context *, const cui_drop_shadow *, size_t count, cui_corner_radii);
/* Replaces the whole mixed list. NONE clears DROP and INSET; no buffer is retained. */
bool cui_set_shadow_preset(cui_context *, cui_shadow_preset, cui_corner_radii);

typedef struct {
    cui_rect bounds;
    cui_align horizontal, vertical;
    /* Growing dimensions add bounds.w/h to the current box size, clamped at zero.
     * Alignment is resolved after sizing; x/y then offset that aligned origin. */
    bool grow_width, grow_height;
} cui_underlay_layout;
bool cui_set_underlay_ex(cui_context *, cui_underlay_layout, cui_color, cui_corner_radii);
/* Borrowed bitmap below children and above the box background, with the same
 * first-frame grow/alignment and lifetime rules as solid underlays. */
bool cui_set_bitmap_underlay(cui_context *, cui_underlay_layout, cui_bitmap_paint,
                             cui_corner_radii);
/* Current-frame paint after the single layout pass. Register on an open box
 * with a nonzero ID. No layout/input/focus mutation is permitted in callbacks. */
typedef struct cui_prepaint_writer cui_prepaint_writer;
bool cui_prepaint_inset_shadow(cui_prepaint_writer *, const cui_inset_shadow_paint *,
                               bool deferred);
bool cui_prepaint_drop_shadow(cui_prepaint_writer *, const cui_drop_shadow_paint *, bool deferred);
typedef struct {
    cui_id owner;
    cui_rect bounds, clip, visible_bounds;
    float pointer_x, pointer_y;
    bool pointer_valid, pointer_inside, pointer_visible, modal_allowed, inert;
    cui_id pointer_hit, capture;
    float opacity;
    double frame_time;
    float frame_delta;
    bool pointer_present; /* Host presence; pointer_valid additionally requires known position. */
} cui_prepaint_info;
typedef bool (*cui_prepaint_fn)(const cui_prepaint_info *, const void *, cui_prepaint_writer *);
typedef struct {
    cui_prepaint_fn callback;
    const void *payload;
    size_t payload_bytes;   /* Copied/aligned frame storage; contained pointers remain borrowed. */
    size_t command_budget;  /* Positive; includes two clip commands per nonempty stream. */
    bool hover_hitbox;      /* Passive point-occlusion participation, never activation/focus. */
    int16_t deferred_layer; /* Zero inherits owner; other layers must not precede owner. */
} cui_prepaint_options;
bool cui_set_prepaint(cui_context *, cui_prepaint_options);
/* Atomically copy payload followed by tail into one aligned frame allocation.
 * Callback tail starts at copied + original payload_bytes. Both spans may be
 * empty; nonempty spans require valid pointers. No contained pointer is copied
 * recursively. Overflow/capacity failure publishes no registration or budget. */
bool cui_set_prepaint_parts(cui_context *, cui_prepaint_options, const void *tail,
                            size_t tail_bytes);
/* Explicit passive barrier, including inert painted overlays; false clears it.
 * Layout-only nodes never participate. Does not alter input dispatch. */
bool cui_set_hover_hitbox(cui_context *, bool enabled);
/* Global-coordinate paint. Local stream clips to visible_bounds; deferred stream
 * clips to viewport, uses the requested layer and is denied outside modal scope.
 * No caller clip commands. TEXT data is copied; bitmap/mesh storage is borrowed
 * through renderer completion. A false callback/invalid emit rolls back its output;
 * capacity failure empties the final draw list. Writer is callback-local only. */
bool cui_prepaint_emit(cui_prepaint_writer *, const cui_draw_command *, bool deferred);
cui_size cui_prepaint_measure(cui_prepaint_writer *, cui_str, float, cui_text_style);
void cui_prepaint_request_frame(cui_prepaint_writer *);

void cui_begin_box(cui_context *ui, cui_id id, cui_box box);
/* Zero-flow child at parent-relative bounds, inheriting clipping and layer.
 * A width/height of -1 fills the corresponding parent extent; zero is fixed.
 * Must be nested in a box. End with cui_end_box, including invalid bounds. */
void cui_begin_absolute_box(cui_context *ui, cui_id id, cui_box box, cui_rect bounds);
/* Floating viewport coordinates, independent of ancestor clipping/layout.
 * layer must be positive. Close with cui_end_box. Portal dimensions come from
 * bounds (zero dimensions fit contents); box controls styling and child layout. */
void cui_begin_portal(cui_context *ui, cui_id id, cui_box box, cui_rect bounds, int16_t layer);
void cui_end_box(cui_context *ui);
void cui_text(cui_context *ui, cui_str text, float font_size, cui_color color);
/* Explicit logical line height; zero selects the measured natural line height. */
void cui_text_with_line_height(cui_context *, cui_str, float font_size, cui_color,
                               float line_height);
/* Validation performs no allocation. Ranges must end at UTF-8 scalar boundaries.
 * Contextual submission copies the full paragraph into bounded frame storage.
 * False leaves submission/storage unchanged; text/layout errors are observable. */
bool cui_text_line_source_valid(cui_text_line_source source);
bool cui_text_context_line(cui_context *, cui_text_line_source, float, cui_color, float,
                           cui_text_style);
bool cui_text_context_line_colored(cui_context *, cui_text_line_source, float, cui_color, float,
                                   cui_text_style, const cui_text_color_span *, size_t);
/* Complete strict UTF-8 text with hard paragraphs, U+2028 visual breaks, and
 * empty lines. Color spans use original text bytes and may cross separators.
 * wrap=true uses current parent width and the pinned GPUI candidate/fallback
 * policy, with Unicode17 grapheme-safe boundaries and preserved whitespace.
 * Contextual geometry supplies shaped advances/clusters when available; without
 * it the host measures individual graphemes. false preserves unwrapped lines.
 * Invalid input and preflight capacity failures publish no layout. Final layout
 * errors remain observable through the normal Core error/draw-list contract. */
bool cui_text_paragraph_colored(cui_context *, cui_str, float font_size, cui_color,
                                float line_height, cui_text_style, bool wrap,
                                const cui_text_color_span *, size_t);
typedef enum { CUI_TEXT_ALIGN_LEFT, CUI_TEXT_ALIGN_CENTER, CUI_TEXT_ALIGN_RIGHT } cui_text_align;
/* Opt-in physical alignment of each visual row within current available parent
 * width, including nowrap rows. Empty rows retain line height. Oversized rows
 * may receive negative offsets; alignment preserves original-byte source/spans. */
bool cui_text_paragraph_colored_aligned(cui_context *, cui_str, float font_size, cui_color,
                                        float line_height, cui_text_style, bool wrap,
                                        const cui_text_color_span *, size_t, cui_text_align);
/* Full paragraph paint; no color-span overrides. Block anchor is the complete
 * resolved paragraph container, including hard/empty rows; line uses each visual
 * row's advance box. Paint changes preserve shaping/caret/selection geometry.
 * Linear endpoints must be finite in 0..1 and distinct. Invalid input publishes
 * no layout. Prepaint text-gradient emission is currently rejected explicitly. */
bool cui_text_paragraph_painted(cui_context *, cui_str, float font_size, float line_height,
                                cui_text_style, bool wrap, cui_text_align, cui_text_paint);
bool cui_has_text_geometry_line(const cui_context *);
cui_text_layout_status cui_get_text_geometry_line(cui_context *, cui_text_line_source, float,
                                                  cui_text_style, cui_text_geometry *);
void cui_text_ex(cui_context *, cui_str, float font_size, cui_color, float line_height,
                 cui_text_style);
/* Unwrapped text for caller-laid-out lines. Overflows narrow boxes; use a
 * clipping ancestor when needed. Same style and line-height rules as text_ex. */
void cui_text_line(cui_context *, cui_str, float font_size, cui_color, float line_height,
                   cui_text_style);
/* Uses the same configured font measurement as layout; no frame allocation. */
/* Returns false without a provider or for invalid arguments/results. Failure
 * preserves out; no approximate baseline is fabricated from text height. */
/* Factory failure preserves out. Successful sessions require exactly one release.
 * No callback returns UNAVAILABLE. Helper rejects nonfinite/malformed metadata. */
cui_text_layout_status cui_get_text_geometry(cui_context *, cui_str, float, cui_text_style,
                                             cui_text_geometry *out);
void cui_release_text_geometry(cui_text_geometry *);
bool cui_has_text_geometry(const cui_context *);
bool cui_measure_font_metrics(cui_context *, float font_size, cui_text_style,
                              cui_font_metrics *out);
cui_size cui_measure_text(cui_context *, cui_str text, float font_size);
cui_size cui_measure_text_ex(cui_context *, cui_str text, float font_size, cui_text_style);
/* Call after changing a host font registry or styled measurement policy. */
void cui_text_metrics_changed(cui_context *);
/* Context-local font measurement generation, initially1; null returns0.
 * Changes only on accepted text_metrics_changed calls outside a frame.
 * Cache by equality, not ordering; wrap skips0. */
uint64_t cui_text_metrics_revision(const cui_context *);
cui_interaction cui_interact(cui_context *ui, cui_id id, cui_role role, cui_str name, bool disabled,
                             bool checked, float value);
/* Update the controlled value of a semantic already submitted this frame. */
void cui_update_semantic(cui_context *ui, cui_id id, bool checked, float value);
/* Refine a submitted current-frame role without replacing its other metadata. */
bool cui_set_semantic_role(cui_context *, cui_id, cui_role);
void cui_set_semantic_details(cui_context *ui, cui_id id, cui_semantic_details details);
void cui_set_tab_stop(cui_context *ui, cui_id id, bool enabled);
/* Signed source ordering, independent of Tab eligibility. Last setter wins. */
void cui_set_tab_index(cui_context *ui, cui_id id, int index);
/* Up to 32 nested groups. Failure leaves the group stack unchanged. */
bool cui_begin_tab_group(cui_context *ui, int index);
void cui_end_tab_group(cui_context *ui);
/* Target resolved in input order against previous frame regions. */
cui_id cui_event_target(cui_context *ui, size_t index);
/* Defer an unconsumed scroll event currently routed to from_id to an eligible
 * layout ancestor. Preserves deltas and phase, clipping, modal/overlay and axis
 * constraints. Call before the ancestor handles input. False leaves it intact. */
bool cui_defer_scroll_event(cui_context *, size_t index, cui_id from_id);
/* Validated activation generated by this event, before widgets consume it.
 * Zero for cancelled pointer/Space capture, repeats, and non-activation events. */
cui_id cui_event_activation(cui_context *ui, size_t index);
/* Accepted by a successfully submitted current semantic, after widget reduction.
 * Zero before registration, after current accessibility role/scope rejection,
 * outside ordinary submission, or for cancelled/inert/unsubmitted targets.
 * Does not consume input. Raw cui_event_activation keeps its existing contract. */
cui_id cui_event_accepted_activation(const cui_context *, size_t index);
/* Nonzero generation of this successful begin_frame, including dt=0. Zero
 * outside ordinary submission. Wrap skips zero; reset caller workspaces on
 * context recreation and do not retain them across a full uint64_t cycle. */
uint64_t cui_frame_sequence(const cui_context *);
/* Fully copied accepted activation evidence. No host-derived first-focus or
 * long-press values are fabricated. Pointer samples preserve raw click count.
 * Keyboard bounds are the previous rendered clipped routing rectangle. */
typedef struct {
    float x, y;
    unsigned modifiers, click_count;
    cui_pointer_type type;
    cui_pointer_button button;
    uint64_t contact, device;
} cui_pointer_activation_sample;
typedef struct {
    cui_id target;
    uint64_t frame_sequence;
    size_t event_index;
    cui_event_type event_type;
    cui_key key;
    unsigned modifiers;
    bool has_press, has_release, has_key_bounds;
    cui_pointer_activation_sample press, release;
    cui_rect key_bounds;
} cui_activation_snapshot;
/* Only accepted ordinary submission activations; same rejection guards as
 * cui_event_accepted_activation. False leaves out untouched. Successful copied
 * values remain usable after later frames/context destruction. Frame/index
 * identity is local to this context, not global across recreated contexts. */
bool cui_event_activation_snapshot(const cui_context *, size_t event_index,
                                   cui_activation_snapshot *out);
/* Ordinary composition phase, including inert/layout-only ancestry. False
 * outside a frame, during host measurement, layout resolution or prepaint. */
bool cui_frame_is_submitting(const cui_context *);
/* Ordinary composition with painting enabled. Inert ancestry still paints;
 * layout-only ancestry, host measurement, prepaint and outside-frame calls
 * return false. Use to avoid scheduling animation for unpainted content. */
bool cui_frame_paint_enabled(const cui_context *);
/* Cooperative bubbling: leaf handlers consume events before ancestor handlers.
 * Event targets remain available for diagnostics; callers check consumption.
 * Consuming an accessibility action also cancels its pending activation. */
bool cui_event_consumed(cui_context *, size_t index);
bool cui_consume_event(cui_context *, size_t index);
/* Capture-phase cancellation: suppress only this event's pending activation.
 * Call before submitting its target widget. Event target and pointer release
 * remain intact; false means no pending activation (including already used). */
bool cui_cancel_event_activation(cui_context *, size_t index);
/* Whether the event target belonged to this named focus scope (or a nested
 * scope) in the previous frame. The scope must be active in this frame.
 * Portals retain their logical scope. This query does not consume input. */
bool cui_event_within_scope(cui_context *, size_t index, cui_id scope);
/* Original topmost eligible pointer/scroll hit, before capture or wheel routing.
 * Uses clipped previous-frame regions, modal and overlay ordering. The named
 * logical scope must be active now. Non-pointer events return false. */
bool cui_event_hit_within_scope(cui_context *, size_t index, cui_id scope);
const cui_semantic *cui_semantics(cui_context *ui, size_t *count);
/* After end_frame: explicit semantic parent, otherwise the nearest semantic
 * layout ancestor. Zero for roots, unknown IDs, or while a frame is open. */
cui_id cui_semantic_parent(cui_context *ui, cui_id id);
cui_rect cui_bounds(cui_context *ui, cui_id id);
/* Update a current-frame clipped box before end_frame computes layout. Useful
 * for scroll events deferred by children after their parent's begin call. */
bool cui_set_box_scroll_offset(cui_context *, cui_id, float x, float y);
/* Register one current-layout bounds result per ID for this frame. The retained
 * pointer must remain valid through cui_end_frame, which writes the computed
 * rectangle (zero if absent) and invalidates only when its fields change.
 * Repeating the same ID/pointer is idempotent; conflicting IDs/pointers fail
 * with DUPLICATE_ID. Registrations are bounded by max_elements. Null arguments
 * or zero IDs return false. Call only between begin_frame and end_frame. */
bool cui_track_bounds(cui_context *ui, cui_id id, cui_rect *retained);
/* Previous-frame clipped region, excluding higher interactive layers/modal scopes. */
bool cui_hovered(cui_context *ui, cui_id id);
/* Cursor of the captured or topmost hovered enabled semantic from the completed
 * frame. Hosts apply this after end_frame; clipping and modal scopes apply. */
cui_cursor cui_pointer_cursor(cui_context *ui);
/* Apply after building a component; preserves its other semantic details. */
bool cui_set_cursor(cui_context *, cui_id, cui_cursor);
bool cui_set_gestures(cui_context *, cui_id, bool pan, bool zoom);
bool cui_set_touch_drag_priority(cui_context *, cui_id, bool enabled);
/* Non-mutating: activation remains available to its component/event consumer. */
cui_interaction cui_interaction_state(const cui_context *, cui_id);
/* Most recent processed pointer Move/Down/Up coordinates, initially0,0.
 * Queued events update them at begin_frame; idle frames retain them. This is
 * position only, not hover/occlusion/capture. Outputs must be distinct. Null or
 * nonfinite coordinates return false and leave both outputs unchanged. */
bool cui_pointer_position(const cui_context *, float *x, float *y);

/* Current primary-pointer or Space capture; submit behavior before querying. */
bool cui_pressed(cui_context *ui, cui_id id);
cui_id cui_focused(cui_context *ui);
void cui_focus(cui_context *ui, cui_id id);
/* Is a coordinate-based opener permitted by current/previous modal scopes?
 * owned_scope allows reanchoring the opener's own popup. Call before opening it. */
bool cui_pointer_scope_allowed(cui_context *, cui_id owned_scope);
bool cui_begin_focus_scope(cui_context *ui, cui_id id, bool modal);
void cui_end_focus_scope(cui_context *ui);
/* Withdraw current scope descendants before painting a closing popup. Paired
 * begin/end remain required. Restores a previous-frame enabled trigger, if any. */
bool cui_dismiss_focus_scope(cui_context *ui, cui_id scope, cui_id trigger);
bool cui_focus_restore(cui_context *ui, cui_id trigger);
/* Scope ancestry, using current submissions or the last completed frame. */
bool cui_focus_within(cui_context *ui, cui_id scope);
/* After submitting descendants, move through current enabled Tab stops in this
 * scope, wrapping at either end. Requires focus already inside the scope.
 * Returns true when an eligible stop exists, even if it is the current one. */
bool cui_focus_step_within(cui_context *, cui_id scope, bool forward);
/* Hover follows focus-scope ownership, including explicitly scoped portals. */
bool cui_hover_within(cui_context *ui, cui_id scope);
const cui_event *cui_frame_events(cui_context *ui, size_t *count);
cui_error cui_last_error(cui_context *ui);
void cui_clear_error(cui_context *ui);
bool cui_needs_redraw(cui_context *ui);
void cui_invalidate(cui_context *ui);
/* Stable zeroed context storage keyed by owner and namespace. Use only in-frame.
 * Finish rendering a draw list before the next begin_frame: that call collects
 * resources used last frame and destroys those omitted for a complete frame.
 * Callbacks must not reenter this context. Existing keys require identical size
 * and callbacks. Capacity/allocation failure reports CUI_ERROR_CAPACITY. */
typedef void (*cui_resource_fn)(void *payload);
void *cui_resource_get(cui_context *, cui_id owner, cui_id namespace_id, size_t bytes,
                       cui_resource_fn collect, cui_resource_fn destroy);
/* In-frame lookup only: no allocation and no error for missing/invalid input.
 * Existing storage is marked used this frame. Context owns it; the pointer has
 * the same renderer-completion/next-begin lifetime as resource_get. */
void *cui_resource_find(cui_context *, cui_id owner, cui_id namespace_id);
float cui_frame_delta(cui_context *);
double cui_frame_time(cui_context *);
/* Frame-only registration against a submitted textbox ID. Getter after end_frame
 * resolves a live focused editable textbox (ordinary multiline is supported);
 * explicit secure configuration is single-line only. Absent/readonly/disabled or
 * changed focus returns false without output writes. Zero config is ordinary.
 * Generation changes with owner/config/removal; saturation fails closed until
 * context recreation. No strings/model/platform pointers are retained. */
bool cui_set_text_input_config(cui_context *, cui_id owner, cui_text_input_config);
bool cui_get_text_input_config(const cui_context *, cui_text_input_state *);
/* Host candidate-window rectangle, logical pixels, last painted focused caret. */
void cui_set_text_input_rect(cui_context *, cui_id, cui_rect);
/* Resolve a painted caret child's final layout bounds for the IME line box.
 * Call during the frame; query text_input_rect after end_frame. */
bool cui_set_text_input_geometry_rect(cui_context *, cui_id owner, cui_id caret, float line_height);
bool cui_text_input_rect(const cui_context *, cui_rect *);
bool cui_reduced_motion(cui_context *);
void cui_set_reduced_motion(cui_context *, bool);
#ifdef __cplusplus
}
#endif
#endif
