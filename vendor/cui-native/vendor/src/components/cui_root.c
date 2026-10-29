/* SPDX-License-Identifier: MIT */
#include "cui/cui_root.h"
typedef struct {
    cui_root *root;
    const cui_theme *theme;
    cui_rect viewport;
    cui_id id;
    double now_seconds;
} cui_root_facility_frame;
static void cui_root_facilities(cui_context *ui, void *user) {
    cui_root_facility_frame *frame = user;
    cui_root *root = frame->root;
    if (root->sheet.surface && root->sheet.surface->overlay.open) {
        cui_root_surface sheet = root->sheet;
        sheet.options.viewport = frame->viewport;
        sheet.options.layer = 1;
        if (cui_surface_begin(ui, frame->theme, sheet.surface, sheet.title, sheet.description,
                              sheet.options)) {
            sheet.content(ui, sheet.user);
            cui_surface_end(ui);
        }
    }
    size_t count = root->dialog_count;
    for (size_t i = 0; i < count; i++) {
        cui_root_surface dialog = root->dialogs[i];
        if (!dialog.surface || !dialog.surface->overlay.open)
            continue;
        dialog.options.viewport = frame->viewport;
        dialog.options.layer = (int16_t)(3 + i * 2);
        if (cui_surface_begin(ui, frame->theme, dialog.surface, dialog.title, dialog.description,
                              dialog.options)) {
            dialog.content(ui, dialog.user);
            cui_surface_end(ui);
        }
    }
    cui_rect notification_viewport = frame->viewport;
    if (root->sheet.surface && root->sheet.surface->overlay.open) {
        cui_surface_options options = root->sheet.options;
        options.viewport = frame->viewport;
        cui_rect sheet = cui_surface_current_geometry(root->sheet.surface, options);
        switch (options.edge) {
        case CUI_SHEET_LEFT:
            notification_viewport.x += sheet.w;
            notification_viewport.w -= sheet.w;
            break;
        case CUI_SHEET_RIGHT:
            notification_viewport.w -= sheet.w;
            break;
        case CUI_SHEET_TOP:
            notification_viewport.y += sheet.h;
            notification_viewport.h -= sheet.h;
            break;
        case CUI_SHEET_BOTTOM:
            notification_viewport.h -= sheet.h;
            break;
        }
    }
    if (root->notifications)
        (void)cui_notifications(ui, frame->id ^ UINT64_C(0x94fbd6a19873c20e), frame->theme,
                                root->notifications, notification_viewport, frame->now_seconds);
}
bool cui_root_render(cui_context *ui, cui_id id, cui_root *root, const cui_theme *theme,
                     cui_rect viewport, const cui_root_style_override *instance_override,
                     cui_root_content_fn content, void *content_user, double now_seconds) {
    if (!theme || !root || root->dialog_capacity > 8 || (root->dialog_count && !root->dialogs))
        return false;
    cui_root_facility_frame frame = {root, theme, viewport, id, now_seconds};
    cui_root_style defaults = {.background = theme->background, .foreground = theme->text};
    return cui_base_root_render(ui, id, root, viewport, defaults, instance_override, content,
                                content_user, cui_root_facilities, &frame);
}
