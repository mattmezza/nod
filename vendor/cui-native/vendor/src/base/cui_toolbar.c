/* SPDX-License-Identifier: MIT */
#include "cui/cui_toolbar.h"
#include <math.h>
bool cui_toolbar_measure(cui_toolbar_size size, float custom, cui_toolbar_metrics *out) {
    if (!out || size < CUI_TOOLBAR_SMALL || size > CUI_TOOLBAR_CUSTOM || !isfinite(custom) ||
        custom < 0)
        return false;
    if (size == CUI_TOOLBAR_LARGE)
        size = CUI_TOOLBAR_MEDIUM;
    bool compact = size == CUI_TOOLBAR_SMALL || size == CUI_TOOLBAR_XSMALL;
    *out = (cui_toolbar_metrics){.size = size,
                                 .custom_size = custom,
                                 .height = size == CUI_TOOLBAR_XSMALL ? 28
                                           : compact                  ? 32
                                                                      : 48,
                                 .padding = compact ? 4 : 8,
                                 .gap = compact ? 4 : 8,
                                 .font_size = size == CUI_TOOLBAR_XSMALL ? 12 : 14,
                                 .control_height = size == CUI_TOOLBAR_XSMALL   ? 20
                                                   : size == CUI_TOOLBAR_MEDIUM ? 32
                                                                                : 24};
    return true;
}
static bool cui_toolbar_label_valid(cui_str label) {
    return label.length == 0 || label.data != NULL;
}
bool cui_base_toolbar_begin(cui_context *ui, cui_id id, cui_str label, cui_box box) {
    if (!ui || !id || !cui_toolbar_label_valid(label))
        return false;
    if (!cui_begin_focus_scope(ui, id, false))
        return false;
    cui_interact(ui, id, CUI_ROLE_TOOLBAR, label, false, false, 0);
    cui_set_tab_stop(ui, id, false);
    cui_set_semantic_details(ui, id,
                             (cui_semantic_details){.orientation = CUI_ORIENTATION_HORIZONTAL});
    cui_begin_box(ui, id, box);
    return true;
}
void cui_base_toolbar_end(cui_context *ui, cui_id id, bool disabled) {
    if (!ui)
        return;
    if (!disabled && cui_focus_within(ui, id)) {
        size_t count = 0;
        const cui_event *events = cui_frame_events(ui, &count);
        for (size_t i = 0; i < count; i++) {
            const cui_event *e = &events[i];
            if (e->type != CUI_EVENT_KEY_DOWN ||
                (e->key != CUI_KEY_LEFT && e->key != CUI_KEY_RIGHT) || cui_event_consumed(ui, i))
                continue;
            cui_focus_step_within(ui, id, e->key == CUI_KEY_RIGHT);
            cui_consume_event(ui, i);
        }
    }
    cui_end_box(ui);
    cui_end_focus_scope(ui);
}
bool cui_base_toolbar_group_begin(cui_context *ui, cui_id id, cui_str label, cui_box box) {
    if (!ui || !id || !cui_toolbar_label_valid(label))
        return false;
    cui_interact(ui, id, CUI_ROLE_GROUP, label, false, false, 0);
    cui_begin_box(ui, id, box);
    return true;
}
void cui_base_toolbar_group_end(cui_context *ui) {
    if (ui)
        cui_end_box(ui);
}
