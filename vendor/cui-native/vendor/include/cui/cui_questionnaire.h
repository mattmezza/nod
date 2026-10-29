/* SPDX-License-Identifier: MIT */
#ifndef CUI_QUESTIONNAIRE_H
#define CUI_QUESTIONNAIRE_H
#include "cui_edit.h"
#include "cui_platform.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct cui_questionnaire_state cui_questionnaire_state;
typedef struct cui_questionnaire_observer cui_questionnaire_observer;
typedef enum {
    CUI_QUESTIONNAIRE_OK,
    CUI_QUESTIONNAIRE_INVALID,
    CUI_QUESTIONNAIRE_CAPACITY,
    CUI_QUESTIONNAIRE_DUPLICATE_ITEM,
    CUI_QUESTIONNAIRE_DUPLICATE_CHOICE,
    CUI_QUESTIONNAIRE_MULTIPLE_DEFAULTS,
    CUI_QUESTIONNAIRE_UNKNOWN_ITEM,
    CUI_QUESTIONNAIRE_UNKNOWN_CHOICE,
    CUI_QUESTIONNAIRE_ANSWER_MISMATCH
} cui_questionnaire_result;
typedef enum {
    CUI_QUESTIONNAIRE_UNANSWERED,
    CUI_QUESTIONNAIRE_ANSWERED,
    CUI_QUESTIONNAIRE_SKIPPED
} cui_questionnaire_status;
typedef enum {
    CUI_QUESTIONNAIRE_VALID,
    CUI_QUESTIONNAIRE_REQUIRED,
    CUI_QUESTIONNAIRE_UNFILLED,
    CUI_QUESTIONNAIRE_MESSAGE
} cui_questionnaire_error_kind;
typedef struct {
    cui_questionnaire_error_kind kind;
    cui_str message;
} cui_questionnaire_error;
typedef enum {
    CUI_QUESTIONNAIRE_CURRENT_CHANGED,
    CUI_QUESTIONNAIRE_ANSWER_CHANGED,
    CUI_QUESTIONNAIRE_COMPLETED,
    CUI_QUESTIONNAIRE_SUBMIT
} cui_questionnaire_event_kind;
typedef struct {
    cui_questionnaire_event_kind kind;
    size_t item, previous;
} cui_questionnaire_event;
typedef void (*cui_questionnaire_event_fn)(const cui_questionnaire_state *, cui_questionnaire_event,
                                           void *);
/* Return borrowed error text (empty is valid). It must outlive the next validation.
 * Validator input is immutable; do not mutate this state from a validator. */
typedef cui_str (*cui_questionnaire_validate_fn)(const cui_questionnaire_state *, size_t item,
                                                 void *);
typedef struct {
    cui_str value, label, description;
    bool disabled, default_selected;
} cui_questionnaire_choice;
typedef struct {
    cui_str name, label, description;
    bool required, multiple, disabled;
    const cui_questionnaire_choice *choices;
    size_t choice_count;
    cui_edit *input;
    cui_str input_label;
    bool input_disabled;
    cui_questionnaire_validate_fn validate;
    void *validate_user;
} cui_questionnaire_item;
typedef struct {
    bool selected, initial_selected, disabled;
} cui_questionnaire_choice_runtime;
typedef struct {
    size_t choice_offset;
    bool disabled, skipped, validation_attempted;
    cui_questionnaire_error internal_error;
    cui_str external_error;
    bool has_external_error;
    char *answer, *initial_answer, *initial_input;
    size_t answer_length, initial_answer_length, initial_input_length;
} cui_questionnaire_item_runtime;
typedef struct {
    cui_questionnaire_item_runtime *items;
    size_t item_capacity;
    cui_questionnaire_choice_runtime *choices;
    size_t choice_capacity;
    char *text;
    size_t text_capacity, max_answer_bytes;
} cui_questionnaire_workspace;
struct cui_questionnaire_state {
    const cui_questionnaire_item *items;
    size_t count, current, initial_current;
    cui_questionnaire_workspace storage;
    cui_questionnaire_event_fn event;
    void *event_user;
    size_t pending_item;
    double pending_deadline;
    bool initialized, complete, pending_confirm;
    /* Optional caller-owned attachment. ABI extension: rebuild consumers with
     * matching headers/library. Init clears this; reset retains it. Configure
     * only via cui_questionnaire_observer APIs, not by writing/copying this field. */
    cui_questionnaire_observer *observer;
};
/* Definitions/editors and workspace outlive state. No allocation. Empty schemas
 * are valid. All storage regions must be disjoint. Index SIZE_MAX means none. */
cui_questionnaire_result cui_questionnaire_init(cui_questionnaire_state *,
                                                const cui_questionnaire_item *, size_t,
                                                cui_questionnaire_workspace,
                                                cui_questionnaire_event_fn, void *);
size_t cui_questionnaire_find(const cui_questionnaire_state *, cui_str name);
size_t cui_questionnaire_total(const cui_questionnaire_state *);
size_t cui_questionnaire_position(
    const cui_questionnaire_state *); /* zero based among enabled, or SIZE_MAX */
bool cui_questionnaire_choice_selected(const cui_questionnaire_state *, size_t item, size_t choice);
/* One-based position among enabled choices. Disabled/unknown choices return
 * false and leave both required output pointers unchanged. */
bool cui_questionnaire_choice_position(const cui_questionnaire_state *, size_t item, size_t choice,
                                       size_t *position, size_t *total);
cui_str cui_questionnaire_freeform(const cui_questionnaire_state *, size_t item);
cui_questionnaire_status cui_questionnaire_item_status(const cui_questionnaire_state *,
                                                       size_t item);
cui_questionnaire_error cui_questionnaire_item_error(const cui_questionnaire_state *, size_t item);
cui_questionnaire_result cui_questionnaire_set_current(cui_questionnaire_state *, size_t item,
                                                       bool initial);
cui_questionnaire_result cui_questionnaire_set_answer(cui_questionnaire_state *, size_t item,
                                                      const size_t *choices, size_t count,
                                                      cui_str freeform);
cui_questionnaire_result cui_questionnaire_set_input(cui_questionnaire_state *, size_t item,
                                                     cui_str);
cui_questionnaire_result cui_questionnaire_sync_input(cui_questionnaire_state *, size_t item,
                                                      bool emit);
cui_questionnaire_result cui_questionnaire_disable_item(cui_questionnaire_state *, size_t item,
                                                        bool);
cui_questionnaire_result cui_questionnaire_disable_choice(cui_questionnaire_state *, size_t item,
                                                          size_t choice, bool);
cui_questionnaire_result cui_questionnaire_external_error(cui_questionnaire_state *, size_t item,
                                                          const cui_str *); /* NULL clears */
cui_questionnaire_result cui_questionnaire_activate(cui_questionnaire_state *, size_t item,
                                                    size_t choice);
cui_questionnaire_result cui_questionnaire_choose(cui_questionnaire_state *, size_t item,
                                                  size_t choice, double now, bool reduced_motion);
bool cui_questionnaire_tick(cui_questionnaire_state *,
                            double now); /* also services zero-delay deferred confirmation */
bool cui_questionnaire_previous(cui_questionnaire_state *);
bool cui_questionnaire_next(cui_questionnaire_state *);
bool cui_questionnaire_skip(cui_questionnaire_state *);
bool cui_questionnaire_submit(cui_questionnaire_state *);
bool cui_questionnaire_confirm(cui_questionnaire_state *);
bool cui_questionnaire_validate_item(cui_questionnaire_state *, size_t item);
bool cui_questionnaire_reset(cui_questionnaire_state *);
char cui_questionnaire_shortcut(const cui_questionnaire_state *, size_t item, size_t choice,
                                bool numbers);
bool cui_questionnaire_activate_shortcut(cui_questionnaire_state *, char key, bool numbers,
                                         double now, bool reduced_motion);
typedef enum {
    CUI_QUESTIONNAIRE_MEDIUM,
    CUI_QUESTIONNAIRE_SMALL,
    CUI_QUESTIONNAIRE_LARGE,
    CUI_QUESTIONNAIRE_XSMALL
} cui_questionnaire_size;
typedef struct {
    size_t item, choice;
    cui_questionnaire_size size;
    bool multiple, selected, disabled, invalid, focused;
    char shortcut;               /* Zero when no default shortcut is available. */
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
} cui_questionnaire_choice_view;
/* Render balanced child elements inside the slot. The snapshot is borrowed for
 * this call; do not mutate questionnaire state or end the enclosing slot.
 * The supplied ID is a stable namespace for the slot's custom content. */
typedef void (*cui_questionnaire_choice_render_fn)(cui_context *, cui_id,
                                                   const cui_questionnaire_choice_view *,
                                                   void *user);
/* Optional box-only refinements; not a general Styled or typography cascade.
 * Only masked cui_box members are read. Unknown bits and invalid active values
 * reject the entire options submission before routing, callbacks or interaction.
 * Dimensions accept -1 (grow), 0 (explicit fixed zero), or [0,4096]. Other
 * lengths are [0,4096], opacity [0,1], alignment START/CENTER/END. */
enum {
    CUI_QUESTIONNAIRE_SLOT_WIDTH = 1u << 0,
    CUI_QUESTIONNAIRE_SLOT_HEIGHT = 1u << 1,
    CUI_QUESTIONNAIRE_SLOT_PADDING = 1u << 2,
    CUI_QUESTIONNAIRE_SLOT_GAP = 1u << 3,
    CUI_QUESTIONNAIRE_SLOT_RADIUS = 1u << 4,
    CUI_QUESTIONNAIRE_SLOT_BACKGROUND = 1u << 5,
    CUI_QUESTIONNAIRE_SLOT_BORDER = 1u << 6, /* border and border_width */
    CUI_QUESTIONNAIRE_SLOT_ALIGN = 1u << 7,  /* align_x and align_y */
    CUI_QUESTIONNAIRE_SLOT_OPACITY = 1u << 8
};
typedef struct {
    uint32_t fields;
    cui_box box;
} cui_questionnaire_slot_style;
enum {
    CUI_QUESTIONNAIRE_LABEL_PREVIOUS = 1u << 0,
    CUI_QUESTIONNAIRE_LABEL_SKIP = 1u << 1,
    CUI_QUESTIONNAIRE_LABEL_NEXT = 1u << 2,
    CUI_QUESTIONNAIRE_LABEL_SUBMIT = 1u << 3,
    CUI_QUESTIONNAIRE_LABEL_REQUIRED = 1u << 4,
    CUI_QUESTIONNAIRE_LABEL_UNFILLED = 1u << 5
};
typedef struct {
    uint32_t fields;
    cui_str previous, skip, next, submit, required, unfilled;
} cui_questionnaire_labels;
typedef struct {
    cui_questionnaire_size size;
    float width;
    bool disabled, numeric_shortcuts;
    /* Choice-only overrides. NULL keeps the corresponding default content.
     * Card interaction, semantic label and inherited disabled opacity remain. */
    cui_questionnaire_choice_render_fn render_indicator, render_content, render_shortcut;
    void *render_user;
    /* Non-medium sizes override the root; use custom_size for explicit Medium. */
    bool custom_size;
    cui_design_size design_size; /* INHERIT preserves legacy metrics. */
    float density;               /* zero inherits root theme density, or1 for standalone parts. */
    /* Borrowed descriptors copied at entry. Indicator/shortcut refine only the
     * default element, so a custom renderer replaces their refinement. Content
     * refines the wrapper around default or custom content. Alignment offsets
     * remain based on default adornment metrics, independent of refined size.
     * Disabled ancestor opacity still multiplies slot opacity. */
    const cui_questionnaire_slot_style *indicator_style, *content_style, *shortcut_style;
    /* Masked UTF-8 labels, including explicit empty strings; unmasked labels
     * keep English defaults. Descriptors are copied, string bytes are borrowed
     * through completed-frame draw consumption. No root-to-part inheritance.
     * Nonempty action label argument wins; custom validation messages stay exact.
     * Unknown mask bits, invalid UTF-8 or NULL data with nonzero length reject
     * the entire options submission, even on parts not using these labels. */
    const cui_questionnaire_labels *labels;
} cui_questionnaire_options;
/* Composable containers. Item/choices begin returns false for inactive, unknown
 * or disabled items and opens nothing. Call part_end only after a true result.
 * Item is a programmatically focusable Group, excluded from Tab order; choices
 * uses Group for multiple answers and RadioGroup for single answers. Root begin
 * captures keyboard events using the preceding frame's submitted part bindings;
 * standalone parts may use the explicit keyboard helpers. Root numeric_shortcuts
 * also supplies the shortcut labels of its parts. */
bool cui_questionnaire_item_begin(cui_context *, cui_id, const cui_questionnaire_state *,
                                  size_t item, cui_questionnaire_options);
bool cui_questionnaire_choices_begin(cui_context *, cui_id, const cui_questionnaire_state *,
                                     size_t item, cui_questionnaire_options);
bool cui_questionnaire_actions_begin(cui_context *, cui_id, const cui_questionnaire_state *,
                                     cui_questionnaire_options);
void cui_questionnaire_part_end(cui_context *);
typedef struct {
    bool dragging;
    float scroll_x;
    cui_edit_composition *composition;
    bool composition_owned;
} cui_questionnaire_input_state;
/* Keep one input state per item. Text editing updates the retained answer and
 * emits AnswerChanged; a capacity error leaves the previous semantic answer. */
cui_questionnaire_result cui_questionnaire_input_control(cui_context *, cui_id, const cui_theme *,
                                                         cui_questionnaire_state *, size_t item,
                                                         cui_questionnaire_input_state *,
                                                         const cui_platform *clipboard,
                                                         cui_questionnaire_options);
typedef enum {
    CUI_QUESTIONNAIRE_PREVIOUS,
    CUI_QUESTIONNAIRE_SKIP,
    CUI_QUESTIONNAIRE_NEXT,
    CUI_QUESTIONNAIRE_FINISH
} cui_questionnaire_action;
void cui_questionnaire_heading(cui_context *, cui_id, const cui_theme *,
                               const cui_questionnaire_state *, size_t item,
                               cui_questionnaire_options);
void cui_questionnaire_error_control(cui_context *, cui_id, const cui_theme *,
                                     const cui_questionnaire_state *, size_t item,
                                     cui_questionnaire_options);
bool cui_questionnaire_action_control(cui_context *, cui_id, const cui_theme *,
                                      cui_questionnaire_state *, cui_questionnaire_action,
                                      cui_str label, cui_questionnaire_options);
/* Route focused radio Left/Right before rendering the choices. IDs follow schema
 * order, including disabled choices. Consumed/repeated/modified events are ignored. */
bool cui_questionnaire_radio_keys(cui_context *, cui_questionnaire_state *,
                                  const cui_id *choice_ids, size_t count);
/* Up/Down traverses enabled answers in schema order, including the optional
 * input. Nonempty input drafts keep cursor movement. Call before rendering. */
bool cui_questionnaire_answer_keys(cui_context *, cui_id scope, cui_questionnaire_state *,
                                   const cui_id *choice_ids, size_t count, cui_id input_id);
/* Bare A-Z or 1-9 activates the enabled-position choice and focuses it.
 * Input focus, modifiers, repeats, composition and consumed events are ignored.
 * Call inside the root before rendering input/choices; IDs follow schema order. */
bool cui_questionnaire_shortcut_keys(cui_context *, cui_id scope, cui_questionnaire_state *,
                                     const cui_id *choice_ids, size_t count, cui_id input_id,
                                     bool numbers);
/* Enter confirms a focused filled answer. A single host-chosen Ctrl or Meta
 * modifier confirms from anywhere in the scope. Invoke before input/choices. */
bool cui_questionnaire_confirm_keys(cui_context *, cui_id scope, cui_questionnaire_state *,
                                    const cui_id *choice_ids, size_t count, cui_id input_id,
                                    unsigned submit_modifier);
/* Composable parts: begin/end enclose caller-selected children. Choice parts
 * render only for the current item. Call tick from the host frame clock. */
bool cui_questionnaire_begin(cui_context *, cui_id, const cui_theme *, cui_questionnaire_state *,
                             cui_questionnaire_options);
void cui_questionnaire_end(cui_context *);
void cui_questionnaire_progress(cui_context *, cui_id, const cui_theme *,
                                const cui_questionnaire_state *, cui_questionnaire_options);
bool cui_questionnaire_choice_control(cui_context *, cui_id, const cui_theme *,
                                      cui_questionnaire_state *, size_t item, size_t choice,
                                      cui_questionnaire_options);
#ifdef __cplusplus
}
#endif
#endif
