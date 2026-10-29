/* SPDX-License-Identifier: MIT */
#ifndef CUI_BRIDGE_H
#define CUI_BRIDGE_H
#include <stdint.h>
#if defined(_WIN32)
#if defined(CUI_BRIDGE_BUILD)
#define CUI_BRIDGE_API __declspec(dllexport)
#else
#define CUI_BRIDGE_API __declspec(dllimport)
#endif
#else
#define CUI_BRIDGE_API
#endif
#ifdef __cplusplus
extern "C" {
#endif

#define CUI_BRIDGE_ABI_VERSION 1u
#define CUI_BRIDGE_MAX_DEPTH 128u

typedef struct cui_bridge_app cui_bridge_app;
typedef struct {
    uint32_t slot, generation;
} cui_bridge_node;
typedef struct {
    float x, y, width, height;
} cui_bridge_rect;
typedef enum {
    CUI_BRIDGE_OK = 0,
    CUI_BRIDGE_EMPTY,
    CUI_BRIDGE_CLOSED,
    CUI_BRIDGE_INVALID_ARGUMENT,
    CUI_BRIDGE_STALE_HANDLE,
    CUI_BRIDGE_CAPACITY,
    CUI_BRIDGE_OUT_OF_MEMORY,
    CUI_BRIDGE_INVALID_UTF8,
    CUI_BRIDGE_INVALID_STATE,
    CUI_BRIDGE_BACKEND_UNAVAILABLE,
    CUI_BRIDGE_CORE_ERROR
} cui_bridge_status;
typedef enum { CUI_BRIDGE_BACKEND_NONE = 0, CUI_BRIDGE_BACKEND_SDL = 1 } cui_bridge_backend;
CUI_BRIDGE_API uint32_t cui_bridge_theme_preset_count(void);
CUI_BRIDGE_API cui_bridge_status cui_bridge_theme_preset_name(uint32_t index, char *out_utf8,
                                                              uint32_t capacity,
                                                              uint32_t *out_length);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_theme_preset(cui_bridge_app *, uint32_t index);
CUI_BRIDGE_API cui_bridge_status cui_bridge_get_theme_preset(const cui_bridge_app *,
                                                             uint32_t *out_index);
/* Clipboard calls must run on the SDL owner thread, outside a bridge step.
 * Headless apps report BACKEND_UNAVAILABLE. Reads copy before native storage expires. */
CUI_BRIDGE_API cui_bridge_status cui_bridge_clipboard_set(cui_bridge_app *, const char *text_utf8);
CUI_BRIDGE_API cui_bridge_status cui_bridge_clipboard_get(cui_bridge_app *, char *out_utf8,
                                                          uint32_t capacity, uint32_t *out_length);
typedef enum {
    CUI_BRIDGE_ROW = 1,
    CUI_BRIDGE_COLUMN,
    CUI_BRIDGE_LABEL,
    CUI_BRIDGE_BUTTON,
    CUI_BRIDGE_CHECKBOX,
    CUI_BRIDGE_RADIO,
    CUI_BRIDGE_SWITCH,
    CUI_BRIDGE_TOGGLE,
    CUI_BRIDGE_SLIDER,
    CUI_BRIDGE_PROGRESS,
    CUI_BRIDGE_BADGE,
    CUI_BRIDGE_INPUT,
    CUI_BRIDGE_TEXTAREA,
    CUI_BRIDGE_NUMBER_INPUT,
    CUI_BRIDGE_COLLAPSIBLE,
    CUI_BRIDGE_ALERT,
    CUI_BRIDGE_RADIO_GROUP,
    CUI_BRIDGE_TABS,
    CUI_BRIDGE_ACCORDION,
    CUI_BRIDGE_ITEM,
    CUI_BRIDGE_SELECT,
    CUI_BRIDGE_COMBOBOX,
    CUI_BRIDGE_DROPDOWN_BUTTON,
    CUI_BRIDGE_DIALOG,
    CUI_BRIDGE_ALERT_DIALOG,
    CUI_BRIDGE_SHEET,
    CUI_BRIDGE_POPOVER,
    CUI_BRIDGE_TOOLTIP,
    CUI_BRIDGE_HOVER_CARD,
    CUI_BRIDGE_AVATAR,
    CUI_BRIDGE_BUBBLE,
    CUI_BRIDGE_EMPTY_VIEW,
    CUI_BRIDGE_GROUP_BOX,
    CUI_BRIDGE_KBD,
    CUI_BRIDGE_SKELETON,
    CUI_BRIDGE_SPINNER,
    CUI_BRIDGE_TAG,
    CUI_BRIDGE_MARKER,
    CUI_BRIDGE_SHIMMER,
    CUI_BRIDGE_LIST,
    CUI_BRIDGE_MENU,
    CUI_BRIDGE_PAGINATION,
    CUI_BRIDGE_RATING,
    CUI_BRIDGE_STEPPER,
    CUI_BRIDGE_DESCRIPTION_LIST,
    CUI_BRIDGE_SCROLLABLE,
    CUI_BRIDGE_STATUS_BAR,
    CUI_BRIDGE_TOOLBAR,
    CUI_BRIDGE_TITLE_BAR,
    CUI_BRIDGE_ICON,
    CUI_BRIDGE_MESSAGE,
    CUI_BRIDGE_ATTACHMENT,
    CUI_BRIDGE_IMAGE,
    CUI_BRIDGE_TREE,
    CUI_BRIDGE_TIME_FIELD,
    CUI_BRIDGE_COLOR_PICKER,
    CUI_BRIDGE_CALENDAR,
    CUI_BRIDGE_DATE_PICKER,
    CUI_BRIDGE_OTP_INPUT,
    CUI_BRIDGE_FORM,
    CUI_BRIDGE_INPUT_GROUP,
    CUI_BRIDGE_NOTIFICATION,
    CUI_BRIDGE_FOCUS_TRAP,
    CUI_BRIDGE_MESSAGE_SCROLLER,
    CUI_BRIDGE_CAROUSEL,
    CUI_BRIDGE_RESIZABLE,
    CUI_BRIDGE_TEXT_VIEW,
    CUI_BRIDGE_EDITOR,
    CUI_BRIDGE_CHART,
    CUI_BRIDGE_TABLE,
    CUI_BRIDGE_DATA_TABLE,
    CUI_BRIDGE_PLOT,
    CUI_BRIDGE_COMMAND,
    CUI_BRIDGE_VIRTUAL_LIST,
    CUI_BRIDGE_SIDEBAR,
    CUI_BRIDGE_ROOT_VIEW,
    CUI_BRIDGE_SPEECH,
    CUI_BRIDGE_QUESTIONNAIRE,
    CUI_BRIDGE_DOCK,
    CUI_BRIDGE_SETTINGS
} cui_bridge_kind;
CUI_BRIDGE_API cui_bridge_status cui_bridge_settings_set_query(cui_bridge_app *,
                                                               cui_bridge_node settings,
                                                               const char *query_utf8);
CUI_BRIDGE_API cui_bridge_status cui_bridge_settings_get_page(cui_bridge_app *,
                                                              cui_bridge_node settings,
                                                              cui_bridge_node *out_page);
CUI_BRIDGE_API cui_bridge_status cui_bridge_settings_set_page(cui_bridge_app *,
                                                              cui_bridge_node settings,
                                                              cui_bridge_node page);
CUI_BRIDGE_API cui_bridge_status cui_bridge_settings_reset_page(cui_bridge_app *,
                                                                cui_bridge_node settings,
                                                                cui_bridge_node page);
CUI_BRIDGE_API cui_bridge_status cui_bridge_settings_field_bounds(cui_bridge_app *,
                                                                  cui_bridge_node settings,
                                                                  cui_bridge_node field,
                                                                  cui_bridge_rect *out_bounds);
CUI_BRIDGE_API cui_bridge_status cui_bridge_dock_activate(cui_bridge_app *, cui_bridge_node dock,
                                                          cui_bridge_node panel);
CUI_BRIDGE_API cui_bridge_status cui_bridge_dock_move_split(
    cui_bridge_app *, cui_bridge_node dock, cui_bridge_node panel, cui_bridge_node reference,
    uint32_t axis, double ratio, uint32_t before); /* axis 0 horizontal, 1 vertical */
CUI_BRIDGE_API cui_bridge_status cui_bridge_dock_get_active(cui_bridge_app *, cui_bridge_node dock,
                                                            cui_bridge_node *out_panel);
CUI_BRIDGE_API cui_bridge_status cui_bridge_dock_panel_bounds(cui_bridge_app *,
                                                              cui_bridge_node dock,
                                                              cui_bridge_node panel,
                                                              cui_bridge_rect *out_bounds);
CUI_BRIDGE_API cui_bridge_status cui_bridge_dock_serialize(cui_bridge_app *, cui_bridge_node dock,
                                                           char *destination, uint32_t capacity,
                                                           uint32_t *required_capacity);
CUI_BRIDGE_API cui_bridge_status cui_bridge_questionnaire_set_question(cui_bridge_app *,
                                                                       cui_bridge_node question,
                                                                       uint32_t required,
                                                                       uint32_t multiple);
CUI_BRIDGE_API cui_bridge_status cui_bridge_questionnaire_get_state(cui_bridge_app *,
                                                                    cui_bridge_node questionnaire,
                                                                    cui_bridge_node *current,
                                                                    uint32_t *complete);
CUI_BRIDGE_API cui_bridge_status cui_bridge_questionnaire_choice_selected(cui_bridge_app *,
                                                                          cui_bridge_node choice,
                                                                          uint32_t *selected);
CUI_BRIDGE_API cui_bridge_status cui_bridge_questionnaire_next(cui_bridge_app *,
                                                               cui_bridge_node questionnaire);
CUI_BRIDGE_API cui_bridge_status cui_bridge_questionnaire_previous(cui_bridge_app *,
                                                                   cui_bridge_node questionnaire);
CUI_BRIDGE_API cui_bridge_status cui_bridge_questionnaire_submit(cui_bridge_app *,
                                                                 cui_bridge_node questionnaire);
CUI_BRIDGE_API cui_bridge_status cui_bridge_speech_start(cui_bridge_app *, cui_bridge_node);
CUI_BRIDGE_API cui_bridge_status cui_bridge_speech_report(cui_bridge_app *, cui_bridge_node,
                                                          const char *text_utf8,
                                                          uint32_t final_phrase);
CUI_BRIDGE_API cui_bridge_status cui_bridge_speech_finish(cui_bridge_app *, cui_bridge_node);
CUI_BRIDGE_API cui_bridge_status cui_bridge_speech_transcript(cui_bridge_app *, cui_bridge_node,
                                                              char *out_utf8, uint32_t capacity,
                                                              uint32_t *out_length);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_root_padding(cui_bridge_app *,
                                                             cui_bridge_node root_view,
                                                             float padding);
CUI_BRIDGE_API cui_bridge_status cui_bridge_command_item_bounds(cui_bridge_app *,
                                                                cui_bridge_node command,
                                                                cui_bridge_node item,
                                                                cui_bridge_rect *out_bounds);
CUI_BRIDGE_API cui_bridge_status cui_bridge_virtual_list_item_bounds(cui_bridge_app *,
                                                                     cui_bridge_node list,
                                                                     cui_bridge_node item,
                                                                     cui_bridge_rect *out_bounds);
typedef struct {
    double x, y;
} cui_bridge_plot_point;
CUI_BRIDGE_API cui_bridge_status cui_bridge_plot_set_points(cui_bridge_app *, cui_bridge_node,
                                                            const cui_bridge_plot_point *points,
                                                            uint32_t count);
CUI_BRIDGE_API cui_bridge_status cui_bridge_plot_get_point(cui_bridge_app *, cui_bridge_node,
                                                           uint32_t index,
                                                           cui_bridge_plot_point *out_point);
typedef struct {
    const char *label_utf8;
    double value;
} cui_bridge_chart_datum;
CUI_BRIDGE_API cui_bridge_status cui_bridge_chart_set_data(cui_bridge_app *, cui_bridge_node,
                                                           const cui_bridge_chart_datum *,
                                                           uint32_t count);
CUI_BRIDGE_API cui_bridge_status cui_bridge_chart_set_kind(cui_bridge_app *, cui_bridge_node,
                                                           uint32_t kind);
CUI_BRIDGE_API cui_bridge_status cui_bridge_chart_get_datum(cui_bridge_app *, cui_bridge_node,
                                                            uint32_t index, char *label,
                                                            uint32_t capacity,
                                                            uint32_t *required_capacity,
                                                            double *value);
CUI_BRIDGE_API cui_bridge_status cui_bridge_data_table_set(cui_bridge_app *, cui_bridge_node,
                                                           const char *const *headers,
                                                           uint32_t column_count,
                                                           const char *const *cells,
                                                           uint32_t row_count);
CUI_BRIDGE_API cui_bridge_status cui_bridge_data_table_get_cell(cui_bridge_app *, cui_bridge_node,
                                                                uint32_t row, uint32_t column,
                                                                char *destination,
                                                                uint32_t capacity,
                                                                uint32_t *required_capacity);
CUI_BRIDGE_API cui_bridge_status cui_bridge_data_table_get_state(cui_bridge_app *, cui_bridge_node,
                                                                 uint32_t *row, uint32_t *column,
                                                                 uint32_t *sort_column,
                                                                 uint32_t *sort_direction);
CUI_BRIDGE_API cui_bridge_status cui_bridge_data_table_set_selection_mode(
    cui_bridge_app *, cui_bridge_node, uint32_t mode); /* 0 row, 1 column, 2 cell */
CUI_BRIDGE_API cui_bridge_status cui_bridge_data_table_cell_bounds(cui_bridge_app *,
                                                                   cui_bridge_node, uint32_t row,
                                                                   uint32_t column,
                                                                   cui_bridge_rect *out_bounds);
CUI_BRIDGE_API cui_bridge_status cui_bridge_data_table_header_bounds(cui_bridge_app *,
                                                                     cui_bridge_node,
                                                                     uint32_t column,
                                                                     cui_bridge_rect *out_bounds);
typedef struct {
    uint32_t hour, minute, second;
} cui_bridge_time;
typedef struct {
    int32_t year, month, day;
} cui_bridge_date;
typedef enum {
    CUI_BRIDGE_BUTTON_PRIMARY = 0,
    CUI_BRIDGE_BUTTON_SECONDARY,
    CUI_BRIDGE_BUTTON_OUTLINE,
    CUI_BRIDGE_BUTTON_GHOST
} cui_bridge_button_variant;
typedef enum {
    CUI_BRIDGE_SIZE_INHERIT = 0,
    CUI_BRIDGE_SIZE_XS,
    CUI_BRIDGE_SIZE_SM,
    CUI_BRIDGE_SIZE_BASE,
    CUI_BRIDGE_SIZE_MD,
    CUI_BRIDGE_SIZE_LG,
    CUI_BRIDGE_SIZE_XL,
    CUI_BRIDGE_SIZE_2XL,
    CUI_BRIDGE_SIZE_3XL,
    CUI_BRIDGE_SIZE_4XL,
    CUI_BRIDGE_SIZE_5XL,
    CUI_BRIDGE_SIZE_6XL,
    CUI_BRIDGE_SIZE_7XL,
    CUI_BRIDGE_SIZE_8XL,
    CUI_BRIDGE_SIZE_9XL
} cui_bridge_button_size;
/* Shared scale, currently supported by Button and Avatar only. Existing Button
 * size functions retain their Button-only contract and enum values. */
typedef cui_bridge_button_size cui_bridge_design_size;
typedef enum {
    CUI_BRIDGE_ROUND_THEME = 0,
    CUI_BRIDGE_ROUND_NONE,
    CUI_BRIDGE_ROUND_SMALL,
    CUI_BRIDGE_ROUND_LARGE,
    CUI_BRIDGE_ROUND_CUSTOM
} cui_bridge_button_rounding;
typedef enum {
    CUI_BRIDGE_ATTACHMENT_PENDING = 0,
    CUI_BRIDGE_ATTACHMENT_UPLOADING,
    CUI_BRIDGE_ATTACHMENT_PROCESSING,
    CUI_BRIDGE_ATTACHMENT_FAILED,
    CUI_BRIDGE_ATTACHMENT_COMPLETE
} cui_bridge_attachment_status;
typedef enum {
    CUI_BRIDGE_POINTER_MOVE = 0,
    CUI_BRIDGE_POINTER_DOWN,
    CUI_BRIDGE_POINTER_UP
} cui_bridge_pointer_phase;
typedef enum { CUI_BRIDGE_KEY_DOWN = 0, CUI_BRIDGE_KEY_UP = 1 } cui_bridge_key_phase;
typedef enum {
    CUI_BRIDGE_KEY_TAB = 1,
    CUI_BRIDGE_KEY_ENTER,
    CUI_BRIDGE_KEY_SPACE,
    CUI_BRIDGE_KEY_ESCAPE,
    CUI_BRIDGE_KEY_LEFT,
    CUI_BRIDGE_KEY_RIGHT,
    CUI_BRIDGE_KEY_UP_ARROW,
    CUI_BRIDGE_KEY_DOWN_ARROW
} cui_bridge_key;
enum {
    CUI_BRIDGE_MOD_SHIFT = 1u,
    CUI_BRIDGE_MOD_CTRL = 2u,
    CUI_BRIDGE_MOD_ALT = 4u,
    CUI_BRIDGE_MOD_META = 8u
};
typedef enum {
    CUI_BRIDGE_EVENT_CLICK = 1,
    CUI_BRIDGE_EVENT_CHANGE = 2,
    CUI_BRIDGE_EVENT_CLOSE = 3
} cui_bridge_event_type;
typedef enum {
    CUI_BRIDGE_SURFACE_ACCEPTED = 1,
    CUI_BRIDGE_SURFACE_CANCELLED = 2,
    CUI_BRIDGE_SURFACE_CLOSED = 3
} cui_bridge_surface_result;
typedef struct {
    cui_bridge_event_type type;
    cui_bridge_node node;
    uint64_t frame_sequence;
} cui_bridge_event;
typedef struct {
    cui_bridge_event_type type;
    cui_bridge_node node;
    uint64_t frame_sequence;
    uint32_t checked;
    double value;
    uint32_t text_length;     /* Bytes excluding NUL. */
    cui_bridge_node selected; /* Full selected Item snapshot; zero when inapplicable. */
} cui_bridge_event_ex;
/* Additive queued NOTIFICATION schema; existing ABI/version and event structs
 * are unchanged. Tags/flags are fixed-width fields, not ABI-sized enums. */
#define CUI_BRIDGE_NOTIFICATION_V1 1u
#define CUI_BRIDGE_NOTIFY_ACTIVATE 1u
#define CUI_BRIDGE_NOTIFY_CHANGE 2u
#define CUI_BRIDGE_NOTIFY_LIFECYCLE 3u
#define CUI_BRIDGE_PAYLOAD_NONE 0u
#define CUI_BRIDGE_PAYLOAD_BOOLEAN 1u
#define CUI_BRIDGE_PAYLOAD_SCALAR 2u    /* opt-in only; old V1 poll returns LEGACY */
#define CUI_BRIDGE_PAYLOAD_TEXT 3u      /* opt-in plain-text snapshot */
#define CUI_BRIDGE_PAYLOAD_SELECTION 4u /* opt-in selected handle */
#define CUI_BRIDGE_PAYLOAD_TABLE 5u
#define CUI_BRIDGE_PAYLOAD_SURFACE 6u
#define CUI_BRIDGE_PAYLOAD_LEGACY 7u
#define CUI_BRIDGE_NOTIFY_HAS_FRAME 1u
#define CUI_BRIDGE_TABLE_SELECTION 1u
#define CUI_BRIDGE_TABLE_SORT 2u
typedef struct {
    uint32_t reason, row, column, sort_column, sort_direction;
} cui_bridge_table_notification_v1;
typedef union {
    uint32_t boolean_value;   /* exactly0 or1 */
    double scalar_value;      /* opt-in scalar notification view */
    cui_bridge_node selected; /* opt-in full slot/generation snapshot */
    cui_bridge_table_notification_v1 table;
    uint32_t surface_result;
} cui_bridge_notification_value_v1;
typedef struct {
    uint32_t struct_size, schema_version;
    uint32_t kind, payload_kind, flags, node_kind;
    cui_bridge_node node;
    uint64_t frame_sequence;
    uint32_t text_length; /* excluding NUL; text copied separately */
    cui_bridge_notification_value_v1 payload;
    cui_bridge_event_ex legacy; /* complete old scalar/handle snapshot */
} cui_bridge_notification_v1;
CUI_BRIDGE_API uint32_t cui_bridge_notification_v1_size(void);
/* Bit0 advertises V1. Optional symbol lookup supports older libraries. */
CUI_BRIDGE_API uint32_t cui_bridge_notification_versions(void);
/* Drains the SAME queue as old polling, never broadcasts or dispatches callbacks.
 * Exact struct_size required. CAPACITY changes only required_capacity; a live
 * event stays queued, but preceding stale origins may already have been freed.
 * Other failures/EMPTY preserve all outputs. Success copies complete text plus
 * NUL, then consumes once. Payloads are notifications of existing behavior, never
 * requests. No source/index/evidence is invented; flags mark frame presence only.
 * Initial typed allowlist: CLICK, Checkbox/Switch/Toggle, DataTable, CLOSE.
 * Every other producer is LEGACY. No allocation, truncation or JSON parsing on
 * poll; caller provides readable/writable declared storage extents. */
CUI_BRIDGE_API cui_bridge_status cui_bridge_poll_notification_v1(cui_bridge_app *,
                                                                 uint32_t struct_size,
                                                                 cui_bridge_notification_v1 *out,
                                                                 char *text, uint32_t capacity,
                                                                 uint32_t *required_capacity);
/* Opt-in categories for the SAME queue and unchanged V1 layout. Old V1 polling
 * downgrades newly captured categories to complete LEGACY snapshots. */
#define CUI_BRIDGE_VALUE_SCALAR_V1 (1u << CUI_BRIDGE_PAYLOAD_SCALAR)
#define CUI_BRIDGE_VALUE_TEXT_V1 (1u << CUI_BRIDGE_PAYLOAD_TEXT)
#define CUI_BRIDGE_VALUE_SELECTION_V1 (1u << CUI_BRIDGE_PAYLOAD_SELECTION)
/* SCALAR: Slider/NumberInput/Rating/Pagination.
 * TEXT: plain Input/Textarea/Editor/InputGroup only, copied separately.
 * SELECTION: RadioGroup/Tabs/Select/List/Stepper only. */
CUI_BRIDGE_API uint32_t cui_bridge_notification_value_payloads_v1(void);
/* Unsupported mask bits/invalid size fail before queue cleanup or consumption.
 * Unrequested categories return LEGACY; all existing typed tags stay unchanged.
 * Capacity/lifetime/output rules match cui_bridge_poll_notification_v1. */
CUI_BRIDGE_API cui_bridge_status cui_bridge_poll_value_notification_v1(
    cui_bridge_app *, uint32_t struct_size, uint32_t enabled_payloads,
    cui_bridge_notification_v1 *out, char *text, uint32_t capacity, uint32_t *required_capacity);

typedef struct {
    uint32_t struct_size; /* Must equal cui_bridge_options_size(). */
    uint32_t abi_version;
    cui_bridge_backend backend;
    uint32_t max_nodes, max_events;
    float width, height;        /* Logical pixels; positive, finite, <= 32768. */
    const char *title_utf8;     /* Copied during create; optional for headless. */
    const char *font_path_utf8; /* Required for SDL; borrowed only during create. */
} cui_bridge_options;

/* All calls for an app must be confined to one thread. Serialize all calls
 * across all apps: the core currently uses a process-global Clay context.
 * Strings are UTF-8,
 * NUL-terminated, and limited to 4096 bytes excluding NUL. */
CUI_BRIDGE_API uint32_t cui_bridge_abi_version(void);
CUI_BRIDGE_API uint32_t cui_bridge_options_size(void);
CUI_BRIDGE_API const char *cui_bridge_status_message(cui_bridge_status status);
CUI_BRIDGE_API cui_bridge_app *cui_bridge_create(const cui_bridge_options *options,
                                                 cui_bridge_status *out_status);
CUI_BRIDGE_API void cui_bridge_destroy(cui_bridge_app *app);
/* Zero parent creates the single root. Keys are copied, unique among live nodes;
 * stale handles never alias a replacement node within an app. Handle provenance
 * is app-scoped: callers must use each handle only with its creating app.
 * Mutations are rejected during step. Trees are limited to 128 levels including
 * the root; deeper creation returns CAPACITY without changing the tree. */
CUI_BRIDGE_API cui_bridge_status cui_bridge_node_create(cui_bridge_app *app, cui_bridge_node parent,
                                                        cui_bridge_kind kind, const char *key_utf8,
                                                        cui_bridge_node *out_node);
CUI_BRIDGE_API cui_bridge_status cui_bridge_node_remove(cui_bridge_app *app, cui_bridge_node node);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_text(cui_bridge_app *app, cui_bridge_node node,
                                                     const char *text_utf8);
/* State setters apply to matching component kinds only. Boolean values must be 0 or 1.
 * Number ranges must be finite with minimum < maximum and positive step.
 * Setting a range clamps the current value into it atomically. */
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_checked(cui_bridge_app *, cui_bridge_node,
                                                        uint32_t checked);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_disabled(cui_bridge_app *, cui_bridge_node,
                                                         uint32_t disabled);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_value(cui_bridge_app *, cui_bridge_node,
                                                      double value);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_range(cui_bridge_app *, cui_bridge_node,
                                                      double minimum, double maximum, double step);
CUI_BRIDGE_API cui_bridge_status cui_bridge_get_checked(cui_bridge_app *, cui_bridge_node,
                                                        uint32_t *out_checked);
CUI_BRIDGE_API cui_bridge_status cui_bridge_get_value(cui_bridge_app *, cui_bridge_node,
                                                      double *out_value);
/* Copies editable value and NUL. On CAPACITY, required_capacity receives bytes
 * including NUL; destination and length are unchanged. */
CUI_BRIDGE_API cui_bridge_status cui_bridge_get_text(cui_bridge_app *, cui_bridge_node,
                                                     char *destination, uint32_t capacity,
                                                     uint32_t *required_capacity);
CUI_BRIDGE_API cui_bridge_status cui_bridge_queue_text(cui_bridge_app *, const char *text_utf8);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_detail_text(cui_bridge_app *, cui_bridge_node,
                                                            const char *text_utf8);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_open(cui_bridge_app *, cui_bridge_node,
                                                     uint32_t open);
CUI_BRIDGE_API cui_bridge_status cui_bridge_get_open(cui_bridge_app *, cui_bridge_node,
                                                     uint32_t *out_open);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_anchor(cui_bridge_app *, cui_bridge_node surface,
                                                       cui_bridge_node anchor);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_visual_size(cui_bridge_app *, cui_bridge_node,
                                                            float width, float height);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_total(cui_bridge_app *, cui_bridge_node,
                                                      uint32_t total);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_step_status(cui_bridge_app *, cui_bridge_node item,
                                                            uint32_t status);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_attachment_status(
    cui_bridge_app *, cui_bridge_node attachment, cui_bridge_attachment_status status);
/* Copies a tightly packed or padded RGBA8 image (at most 16 MiB). */
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_image_rgba(cui_bridge_app *, cui_bridge_node image,
                                                           const uint8_t *pixels, uint32_t width,
                                                           uint32_t height, uint32_t stride,
                                                           uint32_t byte_count);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_time(cui_bridge_app *, cui_bridge_node,
                                                     cui_bridge_time);
CUI_BRIDGE_API cui_bridge_status cui_bridge_get_time(cui_bridge_app *, cui_bridge_node,
                                                     cui_bridge_time *out_time);
/* Packed color is 0xRRGGBBAA. */
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_color_rgba(cui_bridge_app *, cui_bridge_node,
                                                           uint32_t rgba);
CUI_BRIDGE_API cui_bridge_status cui_bridge_get_color_rgba(cui_bridge_app *, cui_bridge_node,
                                                           uint32_t *out_rgba);
CUI_BRIDGE_API cui_bridge_status cui_bridge_color_part_bounds(cui_bridge_app *, cui_bridge_node,
                                                              uint32_t part,
                                                              cui_bridge_rect *out_bounds);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_date(cui_bridge_app *, cui_bridge_node,
                                                     cui_bridge_date);
CUI_BRIDGE_API cui_bridge_status cui_bridge_get_date(cui_bridge_app *, cui_bridge_node,
                                                     cui_bridge_date *out_date);
CUI_BRIDGE_API cui_bridge_status cui_bridge_calendar_day_bounds(cui_bridge_app *,
                                                                cui_bridge_node calendar,
                                                                cui_bridge_date day,
                                                                cui_bridge_rect *out_bounds);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_scroll_extent(cui_bridge_app *, cui_bridge_node,
                                                              float width, float height);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_selected(cui_bridge_app *, cui_bridge_node group,
                                                         cui_bridge_node item);
CUI_BRIDGE_API cui_bridge_status cui_bridge_get_selected(cui_bridge_app *, cui_bridge_node group,
                                                         cui_bridge_node *out_item);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_gap(cui_bridge_app *app, cui_bridge_node node,
                                                    float gap);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_button_variant(cui_bridge_app *app,
                                                               cui_bridge_node node,
                                                               cui_bridge_button_variant variant);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_button_size(cui_bridge_app *app,
                                                            cui_bridge_node node,
                                                            cui_bridge_button_size size);
/* INHERIT restores the component's legacy sizing; explicit variants win over
 * Avatar visual_size. Other component kinds return INVALID_STATE. */
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_design_size(cui_bridge_app *, cui_bridge_node,
                                                            cui_bridge_design_size);
/* Avatar only. Flags are 0/1; custom hue is 0..11. No other palette/ring API. */
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_avatar_identity(cui_bridge_app *, cui_bridge_node,
                                                                uint32_t enabled,
                                                                uint32_t custom_hue, uint32_t hue);
/* Avatar only. Copies valid UTF-8, at most 4096 bytes, transactionally. Empty
 * name selects the default user SVG. Used only for an empty accessible name;
 * named initials take precedence. No borrowed icon-state pointer is accepted. */
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_avatar_placeholder(cui_bridge_app *,
                                                                   cui_bridge_node,
                                                                   uint32_t enabled,
                                                                   const char *name_utf8);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_button_rounding(cui_bridge_app *app,
                                                                cui_bridge_node node,
                                                                cui_bridge_button_rounding rounding,
                                                                float custom_radius);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_button_disabled(cui_bridge_app *app,
                                                                cui_bridge_node node,
                                                                uint32_t disabled);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_button_hover_rgba(cui_bridge_app *app,
                                                                  cui_bridge_node node, uint8_t r,
                                                                  uint8_t g, uint8_t b, uint8_t a);
/* Host events are queued before step. SDL step also polls its own native queue.
 * Clicks are copied into a bounded queue; language callbacks run after step. */
CUI_BRIDGE_API cui_bridge_status cui_bridge_queue_pointer(cui_bridge_app *app,
                                                          cui_bridge_pointer_phase phase, float x,
                                                          float y);
CUI_BRIDGE_API cui_bridge_status cui_bridge_queue_key(cui_bridge_app *app,
                                                      cui_bridge_key_phase phase,
                                                      cui_bridge_key key, uint32_t modifiers,
                                                      uint32_t repeat);
CUI_BRIDGE_API cui_bridge_status cui_bridge_queue_scroll(cui_bridge_app *, float x, float y,
                                                         float dx, float dy);
/* SDL-only. Installs an owned, transactional fallback font chain for missing glyphs.
 * Count 0 clears the chain. Paths are copied by the native font loader. */
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_font_fallbacks(cui_bridge_app *,
                                                               const char *const *paths_utf8,
                                                               uint32_t count);
CUI_BRIDGE_API cui_bridge_status cui_bridge_resizable_handle_bounds(cui_bridge_app *,
                                                                    cui_bridge_node group,
                                                                    cui_bridge_node preceding_item,
                                                                    cui_bridge_rect *out_bounds);
CUI_BRIDGE_API cui_bridge_status cui_bridge_set_text_view_format(
    cui_bridge_app *, cui_bridge_node, uint32_t format); /* 0 Markdown, 1 HTML, 2 plain */
CUI_BRIDGE_API cui_bridge_status cui_bridge_text_view_select(cui_bridge_app *, cui_bridge_node,
                                                             uint32_t anchor, uint32_t cursor);
CUI_BRIDGE_API cui_bridge_status cui_bridge_text_view_copy_selection(cui_bridge_app *,
                                                                     cui_bridge_node,
                                                                     char *destination,
                                                                     uint32_t capacity,
                                                                     uint32_t *required_capacity);
CUI_BRIDGE_API cui_bridge_status cui_bridge_step(cui_bridge_app *app, float dt);
/* Main-thread native callback loop; SDL owns modal live-resize scheduling.
 * after_frame dispatches application events after each completed step, returning
 * OK to continue or another status to stop. Do not nest run/step from this callback
 * (INVALID_STATE). Destroy during a callback requests close and defers release until run exits; do
 * not reuse app afterwards. Foreign exceptions must be caught before returning through C. Window
 * close returns CLOSED. Native resources are released on return; app remains valid for destroy.
 * Portable builds return BACKEND_UNAVAILABLE. Existing step/manual loops remain supported, without
 * modal-resize iteration. */
typedef cui_bridge_status (*cui_bridge_run_callback)(void *user);
CUI_BRIDGE_API cui_bridge_status cui_bridge_run(cui_bridge_app *, cui_bridge_run_callback,
                                                void *user);

CUI_BRIDGE_API cui_bridge_status cui_bridge_poll_event(cui_bridge_app *app,
                                                       cui_bridge_event *out_event);
/* CHANGE snapshots retain their value from the triggering frame. Text is copied
 * to destination including NUL. On CAPACITY, required_capacity is set while the
 * event remains queued and other outputs are preserved. Clicks have empty text.
 * The legacy poll_event consumes only CLICK events and skips CHANGE events. */
CUI_BRIDGE_API cui_bridge_status cui_bridge_poll_event_ex(cui_bridge_app *,
                                                          cui_bridge_event_ex *out_event,
                                                          char *destination, uint32_t capacity,
                                                          uint32_t *required_capacity);
CUI_BRIDGE_API cui_bridge_status cui_bridge_node_bounds(cui_bridge_app *app, cui_bridge_node node,
                                                        cui_bridge_rect *out_bounds);
CUI_BRIDGE_API cui_bridge_status cui_bridge_dropdown_part_bounds(cui_bridge_app *, cui_bridge_node,
                                                                 uint32_t menu_part,
                                                                 cui_bridge_rect *out_bounds);
CUI_BRIDGE_API cui_bridge_status cui_bridge_numeric_part_bounds(cui_bridge_app *, cui_bridge_node,
                                                                uint32_t part,
                                                                cui_bridge_rect *out_bounds);
CUI_BRIDGE_API cui_bridge_status cui_bridge_request_close(cui_bridge_app *app);

#ifdef __cplusplus
}
#endif
#endif
