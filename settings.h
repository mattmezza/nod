#ifndef NOD_SETTINGS_H
#define NOD_SETTINGS_H

#include <stddef.h>
#include <X11/Xlib.h>

enum { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };
enum { MAX_NOTIFS = 32 };

struct settings {
	int width, max_lines, padding, border_width, border_radius;
	int gap_x, gap_y, stack_gap, position, max_visible;
	int show_app, show_urgency, show_time, divider_top, divider_bottom;
	int action_buttons, dismiss_button, show_border;
	int collapse, peek, max_peek;
	float anim_speed, swipe_threshold;
	char *font, *mono_font, *muted_color;
	float min_contrast, critical_tint;
	int use_xresources, xresources_poll_ms;
	char *bg_colors[3], *fg_colors[3], *accent_colors[3];
	int timeouts[3];
	int dnd_schedule[7][4];
	char **muted_apps;
	size_t muted_count;
};

extern struct settings cfg;

void settings_load(Display *display, Colormap colormap);
void settings_free(void);

#endif
