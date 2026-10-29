/* SPDX-License-Identifier: MIT */
/* Logical comparisons avoid struct padding and unused shadow-array storage. */
static bool cui_theme_private_color_equal(cui_color a, cui_color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
static bool cui_theme_private_spring_equal(cui_spring_settings a, cui_spring_settings b) {
    return a.response == b.response && a.damping == b.damping && a.epsilon == b.epsilon &&
           a.travel == b.travel;
}
static bool cui_theme_private_scrollbar_equal(cui_scrollbar_style a, cui_scrollbar_style b) {
    return a.visibility == b.visibility && a.no_track == b.no_track &&
           a.custom_thumb_color == b.custom_thumb_color &&
           a.custom_track_color == b.custom_track_color && a.no_fade == b.no_fade &&
           a.reduced_motion == b.reduced_motion && a.thickness == b.thickness &&
           a.min_thumb == b.min_thumb && a.thumb_radius == b.thumb_radius &&
           a.track_radius == b.track_radius && a.idle_seconds == b.idle_seconds &&
           a.fade_seconds == b.fade_seconds &&
           cui_theme_private_color_equal(a.thumb_color, b.thumb_color) &&
           cui_theme_private_color_equal(a.track_color, b.track_color);
}
static bool cui_theme_private_theme_equal(const cui_theme *a, const cui_theme *b) {
    return cui_theme_private_color_equal(a->background, b->background) &&
           cui_theme_private_color_equal(a->surface, b->surface) &&
           cui_theme_private_color_equal(a->text, b->text) &&
           cui_theme_private_color_equal(a->muted, b->muted) &&
           cui_theme_private_color_equal(a->primary, b->primary) &&
           cui_theme_private_color_equal(a->on_primary, b->on_primary) &&
           cui_theme_private_color_equal(a->border, b->border) &&
           cui_theme_private_color_equal(a->danger, b->danger) &&
           cui_theme_private_color_equal(a->focus, b->focus) && a->font_size == b->font_size &&
           a->padding == b->padding && a->gap == b->gap && a->radius == b->radius &&
           a->control_height == b->control_height &&
           cui_theme_private_color_equal(a->warning, b->warning) &&
           cui_theme_private_color_equal(a->on_warning, b->on_warning) &&
           cui_theme_private_color_equal(a->success, b->success) &&
           cui_theme_private_color_equal(a->on_success, b->on_success) &&
           cui_theme_private_color_equal(a->info, b->info) &&
           cui_theme_private_color_equal(a->on_info, b->on_info) &&
           cui_theme_private_color_equal(a->on_danger, b->on_danger) &&
           cui_theme_private_color_equal(a->buttons[0].normal, b->buttons[0].normal) &&
           cui_theme_private_color_equal(a->buttons[0].hover, b->buttons[0].hover) &&
           cui_theme_private_color_equal(a->buttons[0].active, b->buttons[0].active) &&
           cui_theme_private_color_equal(a->buttons[0].foreground, b->buttons[0].foreground) &&
           cui_theme_private_color_equal(a->buttons[0].outline_normal,
                                         b->buttons[0].outline_normal) &&
           cui_theme_private_color_equal(a->buttons[0].outline_hover,
                                         b->buttons[0].outline_hover) &&
           cui_theme_private_color_equal(a->buttons[0].outline_active,
                                         b->buttons[0].outline_active) &&
           cui_theme_private_color_equal(a->buttons[0].border, b->buttons[0].border) &&
           cui_theme_private_color_equal(a->buttons[0].outline_border,
                                         b->buttons[0].outline_border) &&
           cui_theme_private_color_equal(a->buttons[0].outline_foreground,
                                         b->buttons[0].outline_foreground) &&
           cui_theme_private_color_equal(a->buttons[1].normal, b->buttons[1].normal) &&
           cui_theme_private_color_equal(a->buttons[1].hover, b->buttons[1].hover) &&
           cui_theme_private_color_equal(a->buttons[1].active, b->buttons[1].active) &&
           cui_theme_private_color_equal(a->buttons[1].foreground, b->buttons[1].foreground) &&
           cui_theme_private_color_equal(a->buttons[1].outline_normal,
                                         b->buttons[1].outline_normal) &&
           cui_theme_private_color_equal(a->buttons[1].outline_hover,
                                         b->buttons[1].outline_hover) &&
           cui_theme_private_color_equal(a->buttons[1].outline_active,
                                         b->buttons[1].outline_active) &&
           cui_theme_private_color_equal(a->buttons[1].border, b->buttons[1].border) &&
           cui_theme_private_color_equal(a->buttons[1].outline_border,
                                         b->buttons[1].outline_border) &&
           cui_theme_private_color_equal(a->buttons[1].outline_foreground,
                                         b->buttons[1].outline_foreground) &&
           cui_theme_private_color_equal(a->buttons[2].normal, b->buttons[2].normal) &&
           cui_theme_private_color_equal(a->buttons[2].hover, b->buttons[2].hover) &&
           cui_theme_private_color_equal(a->buttons[2].active, b->buttons[2].active) &&
           cui_theme_private_color_equal(a->buttons[2].foreground, b->buttons[2].foreground) &&
           cui_theme_private_color_equal(a->buttons[2].outline_normal,
                                         b->buttons[2].outline_normal) &&
           cui_theme_private_color_equal(a->buttons[2].outline_hover,
                                         b->buttons[2].outline_hover) &&
           cui_theme_private_color_equal(a->buttons[2].outline_active,
                                         b->buttons[2].outline_active) &&
           cui_theme_private_color_equal(a->buttons[2].border, b->buttons[2].border) &&
           cui_theme_private_color_equal(a->buttons[2].outline_border,
                                         b->buttons[2].outline_border) &&
           cui_theme_private_color_equal(a->buttons[2].outline_foreground,
                                         b->buttons[2].outline_foreground) &&
           cui_theme_private_color_equal(a->buttons[3].normal, b->buttons[3].normal) &&
           cui_theme_private_color_equal(a->buttons[3].hover, b->buttons[3].hover) &&
           cui_theme_private_color_equal(a->buttons[3].active, b->buttons[3].active) &&
           cui_theme_private_color_equal(a->buttons[3].foreground, b->buttons[3].foreground) &&
           cui_theme_private_color_equal(a->buttons[3].outline_normal,
                                         b->buttons[3].outline_normal) &&
           cui_theme_private_color_equal(a->buttons[3].outline_hover,
                                         b->buttons[3].outline_hover) &&
           cui_theme_private_color_equal(a->buttons[3].outline_active,
                                         b->buttons[3].outline_active) &&
           cui_theme_private_color_equal(a->buttons[3].border, b->buttons[3].border) &&
           cui_theme_private_color_equal(a->buttons[3].outline_border,
                                         b->buttons[3].outline_border) &&
           cui_theme_private_color_equal(a->buttons[3].outline_foreground,
                                         b->buttons[3].outline_foreground) &&
           cui_theme_private_color_equal(a->buttons[4].normal, b->buttons[4].normal) &&
           cui_theme_private_color_equal(a->buttons[4].hover, b->buttons[4].hover) &&
           cui_theme_private_color_equal(a->buttons[4].active, b->buttons[4].active) &&
           cui_theme_private_color_equal(a->buttons[4].foreground, b->buttons[4].foreground) &&
           cui_theme_private_color_equal(a->buttons[4].outline_normal,
                                         b->buttons[4].outline_normal) &&
           cui_theme_private_color_equal(a->buttons[4].outline_hover,
                                         b->buttons[4].outline_hover) &&
           cui_theme_private_color_equal(a->buttons[4].outline_active,
                                         b->buttons[4].outline_active) &&
           cui_theme_private_color_equal(a->buttons[4].border, b->buttons[4].border) &&
           cui_theme_private_color_equal(a->buttons[4].outline_border,
                                         b->buttons[4].outline_border) &&
           cui_theme_private_color_equal(a->buttons[4].outline_foreground,
                                         b->buttons[4].outline_foreground) &&
           cui_theme_private_color_equal(a->buttons[5].normal, b->buttons[5].normal) &&
           cui_theme_private_color_equal(a->buttons[5].hover, b->buttons[5].hover) &&
           cui_theme_private_color_equal(a->buttons[5].active, b->buttons[5].active) &&
           cui_theme_private_color_equal(a->buttons[5].foreground, b->buttons[5].foreground) &&
           cui_theme_private_color_equal(a->buttons[5].outline_normal,
                                         b->buttons[5].outline_normal) &&
           cui_theme_private_color_equal(a->buttons[5].outline_hover,
                                         b->buttons[5].outline_hover) &&
           cui_theme_private_color_equal(a->buttons[5].outline_active,
                                         b->buttons[5].outline_active) &&
           cui_theme_private_color_equal(a->buttons[5].border, b->buttons[5].border) &&
           cui_theme_private_color_equal(a->buttons[5].outline_border,
                                         b->buttons[5].outline_border) &&
           cui_theme_private_color_equal(a->buttons[5].outline_foreground,
                                         b->buttons[5].outline_foreground) &&
           cui_theme_private_color_equal(a->buttons[6].normal, b->buttons[6].normal) &&
           cui_theme_private_color_equal(a->buttons[6].hover, b->buttons[6].hover) &&
           cui_theme_private_color_equal(a->buttons[6].active, b->buttons[6].active) &&
           cui_theme_private_color_equal(a->buttons[6].foreground, b->buttons[6].foreground) &&
           cui_theme_private_color_equal(a->buttons[6].outline_normal,
                                         b->buttons[6].outline_normal) &&
           cui_theme_private_color_equal(a->buttons[6].outline_hover,
                                         b->buttons[6].outline_hover) &&
           cui_theme_private_color_equal(a->buttons[6].outline_active,
                                         b->buttons[6].outline_active) &&
           cui_theme_private_color_equal(a->buttons[6].border, b->buttons[6].border) &&
           cui_theme_private_color_equal(a->buttons[6].outline_border,
                                         b->buttons[6].outline_border) &&
           cui_theme_private_color_equal(a->buttons[6].outline_foreground,
                                         b->buttons[6].outline_foreground) &&
           cui_theme_private_color_equal(a->link, b->link) &&
           cui_theme_private_color_equal(a->link_hover, b->link_hover) &&
           cui_theme_private_color_equal(a->link_active, b->link_active) &&
           cui_theme_private_color_equal(a->input, b->input) &&
           cui_theme_private_color_equal(a->input_background, b->input_background) &&
           cui_theme_private_color_equal(a->secondary_foreground, b->secondary_foreground) &&
           cui_theme_private_color_equal(a->accent, b->accent) &&
           cui_theme_private_color_equal(a->accent_foreground, b->accent_foreground) &&
           cui_theme_private_spring_equal(a->spring_control, b->spring_control) &&
           cui_theme_private_color_equal(a->popover, b->popover) &&
           cui_theme_private_color_equal(a->secondary, b->secondary) &&
           a->radius_lg == b->radius_lg &&
           cui_theme_private_color_equal(a->secondary_hover, b->secondary_hover) &&
           cui_theme_private_color_equal(a->tab_foreground, b->tab_foreground) &&
           cui_theme_private_color_equal(a->tab_active_foreground, b->tab_active_foreground) &&
           cui_theme_private_color_equal(a->tab_active, b->tab_active) &&
           cui_theme_private_color_equal(a->tab_bar, b->tab_bar) &&
           cui_theme_private_color_equal(a->tab_bar_segmented, b->tab_bar_segmented) &&
           cui_theme_private_spring_equal(a->spring_move, b->spring_move) &&
           cui_theme_private_color_equal(a->switch_background, b->switch_background) &&
           cui_theme_private_color_equal(a->switch_thumb, b->switch_thumb) &&
           cui_theme_private_color_equal(a->popover_foreground, b->popover_foreground) &&
           cui_theme_private_color_equal(a->list_hover, b->list_hover) &&
           cui_theme_private_color_equal(a->list_active, b->list_active) &&
           cui_theme_private_color_equal(a->selection, b->selection) &&
           a->list_active_highlight == b->list_active_highlight &&
           cui_theme_private_color_equal(a->description_list_label, b->description_list_label) &&
           cui_theme_private_color_equal(a->description_list_label_foreground,
                                         b->description_list_label_foreground) &&
           cui_theme_private_color_equal(a->status_bar, b->status_bar) &&
           cui_theme_private_color_equal(a->status_bar_border, b->status_bar_border) &&
           cui_theme_private_color_equal(a->title_bar, b->title_bar) &&
           cui_theme_private_color_equal(a->title_bar_border, b->title_bar_border) &&
           cui_theme_private_color_equal(a->sidebar, b->sidebar) &&
           cui_theme_private_color_equal(a->sidebar_foreground, b->sidebar_foreground) &&
           cui_theme_private_color_equal(a->sidebar_border, b->sidebar_border) &&
           cui_theme_private_color_equal(a->sidebar_accent, b->sidebar_accent) &&
           cui_theme_private_color_equal(a->sidebar_accent_foreground,
                                         b->sidebar_accent_foreground) &&
           a->icon_loader == b->icon_loader && a->icon_loader_user == b->icon_loader_user &&
           a->icon_revision == b->icon_revision &&
           cui_theme_private_color_equal(a->caret, b->caret) &&
           cui_theme_private_color_equal(a->group_box, b->group_box) &&
           cui_theme_private_color_equal(a->group_box_foreground, b->group_box_foreground) &&
           cui_theme_private_color_equal(a->muted_background, b->muted_background) &&
           a->hide_focus_ring == b->hide_focus_ring && a->dark == b->dark &&
           cui_theme_private_color_equal(a->chart_1, b->chart_1) &&
           cui_theme_private_color_equal(a->chart_2, b->chart_2) &&
           cui_theme_private_color_equal(a->chart_3, b->chart_3) &&
           cui_theme_private_color_equal(a->chart_4, b->chart_4) &&
           cui_theme_private_color_equal(a->chart_5, b->chart_5) &&
           cui_theme_private_color_equal(a->chart_bullish, b->chart_bullish) &&
           cui_theme_private_color_equal(a->chart_bearish, b->chart_bearish) &&
           cui_theme_private_color_equal(a->chart_grid, b->chart_grid) &&
           cui_theme_private_scrollbar_equal(a->scrollbar, b->scrollbar);
}
static bool cui_theme_private_shadows_equal(const cui_theme_shadows *a,
                                            const cui_theme_shadows *b) {
    if (a->count != b->count)
        return false;
    for (size_t i = 0; i < a->count; i++) {
        const cui_theme_shadow *x = &a->values[i], *y = &b->values[i];
        if (!cui_theme_private_color_equal(x->color, y->color) || x->offset_x != y->offset_x ||
            x->offset_y != y->offset_y || x->blur_radius != y->blur_radius ||
            x->spread_radius != y->spread_radius || x->inset != y->inset)
            return false;
    }
    return true;
}
static bool cui_theme_private_text_equal(cui_theme_text_token a, cui_theme_text_token b) {
    return a.size == b.size && a.line_height == b.line_height && a.weight == b.weight;
}
static bool cui_theme_private_typography_equal(const cui_theme_semantic_typography *a,
                                               const cui_theme_semantic_typography *b) {
    return !strcmp(a->sans, b->sans) && !strcmp(a->mono, b->mono) &&
           cui_theme_private_text_equal(a->xs, b->xs) &&
           cui_theme_private_text_equal(a->sm, b->sm) &&
           cui_theme_private_text_equal(a->md, b->md) &&
           cui_theme_private_text_equal(a->lg, b->lg) &&
           cui_theme_private_text_equal(a->xl, b->xl) &&
           cui_theme_private_text_equal(a->mono_md, b->mono_md);
}
static bool cui_theme_private_semantic_equal(const cui_theme_semantic_tokens *a,
                                             const cui_theme_semantic_tokens *b) {
    return cui_theme_private_color_equal(a->colors.background, b->colors.background) &&
           cui_theme_private_color_equal(a->colors.foreground, b->colors.foreground) &&
           cui_theme_private_color_equal(a->colors.surface, b->colors.surface) &&
           cui_theme_private_color_equal(a->colors.surface_foreground,
                                         b->colors.surface_foreground) &&
           cui_theme_private_color_equal(a->colors.primary, b->colors.primary) &&
           cui_theme_private_color_equal(a->colors.primary_foreground,
                                         b->colors.primary_foreground) &&
           cui_theme_private_color_equal(a->colors.secondary, b->colors.secondary) &&
           cui_theme_private_color_equal(a->colors.secondary_foreground,
                                         b->colors.secondary_foreground) &&
           cui_theme_private_color_equal(a->colors.muted, b->colors.muted) &&
           cui_theme_private_color_equal(a->colors.muted_foreground, b->colors.muted_foreground) &&
           cui_theme_private_color_equal(a->colors.accent, b->colors.accent) &&
           cui_theme_private_color_equal(a->colors.accent_foreground,
                                         b->colors.accent_foreground) &&
           cui_theme_private_color_equal(a->colors.destructive, b->colors.destructive) &&
           cui_theme_private_color_equal(a->colors.destructive_foreground,
                                         b->colors.destructive_foreground) &&
           cui_theme_private_color_equal(a->colors.border, b->colors.border) &&
           cui_theme_private_color_equal(a->colors.input, b->colors.input) &&
           cui_theme_private_color_equal(a->colors.ring, b->colors.ring) &&
           cui_theme_private_color_equal(a->colors.selection, b->colors.selection) &&
           a->radius.none == b->radius.none && a->radius.sm == b->radius.sm &&
           a->radius.md == b->radius.md && a->radius.lg == b->radius.lg &&
           a->radius.xl == b->radius.xl && a->radius.full == b->radius.full &&
           a->spacing.xxs == b->spacing.xxs && a->spacing.xs == b->spacing.xs &&
           a->spacing.sm == b->spacing.sm && a->spacing.md == b->spacing.md &&
           a->spacing.lg == b->spacing.lg && a->spacing.xl == b->spacing.xl &&
           a->spacing.xxl == b->spacing.xxl &&
           cui_theme_private_typography_equal(&a->typography, &b->typography) &&
           cui_theme_private_shadows_equal(&a->shadow.sm, &b->shadow.sm) &&
           cui_theme_private_shadows_equal(&a->shadow.md, &b->shadow.md) &&
           cui_theme_private_shadows_equal(&a->shadow.lg, &b->shadow.lg);
}
static bool cui_theme_private_bezier_equal(cui_theme_bezier a, cui_theme_bezier b) {
    return a.x1 == b.x1 && a.y1 == b.y1 && a.x2 == b.x2 && a.y2 == b.y2;
}
static bool cui_theme_private_motion_equal(const cui_theme_motion_tokens *a,
                                           const cui_theme_motion_tokens *b) {
    return a->duration_instant == b->duration_instant && a->duration_fast == b->duration_fast &&
           a->duration_normal == b->duration_normal && a->duration_slow == b->duration_slow &&
           cui_theme_private_bezier_equal(a->easing_enter, b->easing_enter) &&
           cui_theme_private_bezier_equal(a->easing_exit, b->easing_exit) &&
           cui_theme_private_bezier_equal(a->easing_move, b->easing_move) &&
           cui_theme_private_spring_equal(a->spring_control, b->spring_control) &&
           cui_theme_private_spring_equal(a->spring_move, b->spring_move) &&
           a->distance_short == b->distance_short && a->distance_medium == b->distance_medium;
}
