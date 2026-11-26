#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "settings.h"

struct settings cfg;

static char defaults[] = {
#include "build/default-config.h"
};

enum option_type { INTEGER, BOOLEAN, DECIMAL, STRING, COLOR, POSITION, INTERVAL, MUTED_APP };

static const struct option {
	const char *section, *key;
	enum option_type type;
	void *target;
	double min, max;
} options[] = {
#define OPTION(section, key, type, field) { #section, #key, type, &cfg.field, 0, 0 }
#define RANGE(section, field, type, min, max) { #section, #field, type, &cfg.field, min, max }
#define PALETTE(section, index) \
	OPTION(section, background, COLOR, bg_colors[index]), \
	OPTION(section, foreground, COLOR, fg_colors[index]), \
	OPTION(section, accent, COLOR, accent_colors[index]), \
	{ #section, "timeout", INTEGER, &cfg.timeouts[index], 0, INT_MAX / 1000 }
#define DAY(day, index) OPTION(dnd, day, INTERVAL, dnd_schedule[index][0])
	RANGE(layout, width, INTEGER, 32, 8192),
	RANGE(layout, max_lines, INTEGER, 0, 128),
	RANGE(layout, padding, INTEGER, 0, 1024),
	RANGE(layout, border_width, INTEGER, 0, 256),
	RANGE(layout, border_radius, INTEGER, 0, 1024),
	RANGE(layout, gap_x, INTEGER, 0, SHRT_MAX),
	RANGE(layout, gap_y, INTEGER, 0, SHRT_MAX),
	RANGE(layout, stack_gap, INTEGER, 0, 1024),
	OPTION(layout, position, POSITION, position),
	RANGE(layout, max_visible, INTEGER, 1, MAX_NOTIFS),
	OPTION(card, show_app, BOOLEAN, show_app),
	OPTION(card, show_urgency, BOOLEAN, show_urgency),
	OPTION(card, show_time, BOOLEAN, show_time),
	OPTION(card, divider_top, BOOLEAN, divider_top),
	OPTION(card, divider_bottom, BOOLEAN, divider_bottom),
	OPTION(card, action_buttons, BOOLEAN, action_buttons),
	OPTION(card, dismiss_button, BOOLEAN, dismiss_button),
	OPTION(card, show_border, BOOLEAN, show_border),
	OPTION(stack, collapse, BOOLEAN, collapse),
	RANGE(stack, peek, INTEGER, 0, 1024),
	RANGE(stack, max_peek, INTEGER, 0, MAX_NOTIFS),
	RANGE(motion, anim_speed, DECIMAL, 0.01, 1000),
	RANGE(motion, swipe_threshold, DECIMAL, 0, 1),
	OPTION(theme, font, STRING, font),
	OPTION(theme, mono_font, STRING, mono_font),
	OPTION(theme, muted_color, COLOR, muted_color),
	RANGE(theme, min_contrast, DECIMAL, 1, 21),
	RANGE(theme, critical_tint, DECIMAL, 0, 1),
	OPTION(theme, use_xresources, BOOLEAN, use_xresources),
	RANGE(theme, xresources_poll_ms, INTEGER, 0, INT_MAX),
	PALETTE(low, 0),
	PALETTE(normal, 1),
	PALETTE(critical, 2),
	DAY(sunday, 0), DAY(monday, 1), DAY(tuesday, 2), DAY(wednesday, 3),
	DAY(thursday, 4), DAY(friday, 5), DAY(saturday, 6),
	{ "dnd", "muted_app", MUTED_APP, NULL, 0, 0 },
#undef DAY
#undef PALETTE
#undef RANGE
#undef OPTION
};

static void
fatal(const char *message)
{
	fprintf(stderr, "nod: settings: %s\n", message);
	exit(EXIT_FAILURE);
}

static char *
copy_string(const char *value)
{
	char *copy = strdup(value);
	if (!copy) fatal("out of memory");
	return copy;
}

static char *
trim(char *s)
{
	while (isspace((unsigned char)*s)) s++;
	char *end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
	return s;
}

static int
parse_interval(const char *value, int *target)
{
	if (!strcasecmp(value, "off")) {
		target[0] = -1;
		return 1;
	}
	int h1, m1, h2, m2, consumed;
	if (strlen(value) != 11 || strspn(value, "0123456789:-") != 11 ||
	    sscanf(value, "%2d:%2d-%2d:%2d%n", &h1, &m1, &h2, &m2, &consumed) != 4 ||
	    consumed != 11 || h1 < 0 || h1 > 23 || h2 < 0 || h2 > 23 ||
	    m1 < 0 || m1 > 59 || m2 < 0 || m2 > 59)
		return 0;
	target[0] = h1;
	target[1] = m1;
	target[2] = h2;
	target[3] = m2;
	return 1;
}

static int
apply(const struct option *option, const char *value, Display *display, Colormap colormap)
{
	char *end;
	void *target = option->target;
	switch (option->type) {
	case INTEGER: {
		errno = 0;
		long number = strtol(value, &end, 10);
		if (end == value || *end || errno || number < option->min || number > option->max)
			return 0;
		*(int *)target = (int)number;
		return 1;
	}
	case BOOLEAN:
		if (!strcasecmp(value, "true") || !strcmp(value, "1"))
			*(int *)target = 1;
		else if (!strcasecmp(value, "false") || !strcmp(value, "0"))
			*(int *)target = 0;
		else return 0;
		return 1;
	case DECIMAL: {
		errno = 0;
		float number = strtof(value, &end);
		if (end == value || *end || errno || !isfinite(number) ||
		    number < (float)option->min || number > (float)option->max)
			return 0;
		*(float *)target = number;
		return 1;
	}
	case COLOR: {
		XColor color;
		if (!XParseColor(display, colormap, value, &color)) return 0;
		break;
	}
	case STRING:
		if (!*value) return 0;
		break;
	case POSITION: {
		static const char *const corners[] = {
			[TOP_LEFT] = "top-left", [TOP_RIGHT] = "top-right",
			[BOTTOM_LEFT] = "bottom-left", [BOTTOM_RIGHT] = "bottom-right"
		};
		for (size_t i = 0; i < sizeof corners / sizeof *corners; i++) {
			if (!strcasecmp(value, corners[i])) {
				*(int *)target = (int)i;
				return 1;
			}
		}
		return 0;
	}
	case INTERVAL:
		return parse_interval(value, target);
	case MUTED_APP:
		if (*value) {
			char **list = realloc(cfg.muted_apps, (cfg.muted_count + 1) * sizeof *list);
			if (!list) fatal("out of memory");
			cfg.muted_apps = list;
			cfg.muted_apps[cfg.muted_count++] = copy_string(value);
		}
		return 1;
	}
	char **text = target;
	free(*text);
	*text = copy_string(value);
	return 1;
}

static const struct option *
find_option(const char *section, const char *key)
{
	for (size_t i = 0; i < sizeof options / sizeof *options; i++)
		if (!strcmp(section, options[i].section) && !strcmp(key, options[i].key))
			return &options[i];
	return NULL;
}

static int
read_ini(FILE *file, const char *path, Display *display, Colormap colormap)
{
	char *line = NULL, *section = copy_string("");
	size_t capacity = 0, number = 0;
	int ok = 1;
	while (getline(&line, &capacity, file) >= 0) {
		number++;
		char *s = trim(line);
		if (!*s || *s == ';' || *s == '#') continue;
		if (*s == '[') {
			char *end = strchr(s + 1, ']');
			if (end && !end[1] && end > s + 1) {
				*end = '\0';
				free(section);
				section = copy_string(trim(s + 1));
				continue;
			}
			fprintf(stderr, "nod: %s:%zu: invalid section\n", path, number);
			*section = '\0';
		} else {
			char *eq = strchr(s, '=');
			if (eq) {
				*eq = '\0';
				const char *key = trim(s);
				const struct option *option = find_option(section, key);
				if (option && apply(option, trim(eq + 1), display, colormap)) continue;
				fprintf(stderr, "nod: %s:%zu: %s [%s] %s\n", path, number,
				        option ? "invalid value for" : "unknown setting", section, key);
			} else {
				fprintf(stderr, "nod: %s:%zu: expected key = value\n", path, number);
			}
		}
		ok = 0;
	}
	if (!feof(file)) {
		fprintf(stderr, "nod: cannot read %s: %s\n", path, strerror(errno));
		ok = 0;
	}
	free(section);
	free(line);
	return ok;
}

void
settings_load(Display *display, Colormap colormap)
{
	FILE *file = fmemopen(defaults, sizeof defaults, "r");
	if (!file) fatal(strerror(errno));
	int ok = read_ini(file, "built-in config.ini", display, colormap);
	fclose(file);
	if (!ok) fatal("invalid built-in config.ini");

	const char *base = getenv("XDG_CONFIG_HOME");
	const char *suffix = "/nod/config.ini";
	if (!base || *base != '/') {
		base = getenv("HOME");
		suffix = "/.config/nod/config.ini";
	}
	if (base && *base == '/') {
		size_t size = strlen(base) + strlen(suffix) + 1;
		char *path = malloc(size);
		if (!path) fatal("out of memory");
		snprintf(path, size, "%s%s", base, suffix);
		file = fopen(path, "r");
		if (file) {
			read_ini(file, path, display, colormap);
			fclose(file);
		} else if (errno != ENOENT) {
			fprintf(stderr, "nod: cannot open %s: %s\n", path, strerror(errno));
		}
		free(path);
	}

	if (cfg.padding * 2 >= cfg.width) cfg.padding = (cfg.width - 1) / 2;
	if (cfg.max_peek > 0 && cfg.peek * cfg.max_peek * 2 >= cfg.width)
		cfg.peek = (cfg.width - 1) / (2 * cfg.max_peek);
}

void
settings_free(void)
{
	for (size_t i = 0; i < sizeof options / sizeof *options; i++)
		if (options[i].type == STRING || options[i].type == COLOR)
			free(*(char **)options[i].target);
	for (size_t i = 0; i < cfg.muted_count; i++) free(cfg.muted_apps[i]);
	free(cfg.muted_apps);
}
