/* nod - suckless notification daemon
 * See LICENSE file for copyright and license details.
 *
 * D-Bus + Xlib host; cards are laid out and painted by CUI
 * (https://github.com/mattmezza/cui) through its SDL3 renderer, in a single
 * override-redirect window shaped to the cards.
 */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <fontconfig/fontconfig.h>

#include <SDL3/SDL.h>
#include <dbus/dbus.h>

#include "cui/cui_components.h"
#include "cui/cui_sdl3.h"

#include "config.h"

#define LEN(x)       (sizeof(x) / sizeof((x)[0]))
#define MAX_NOTIFS   32
#define MAX_ACTIONS  3
#define BUS_NAME     "org.freedesktop.Notifications"
#define OBJ_PATH     "/org/freedesktop/Notifications"
#define IFACE        "org.freedesktop.Notifications"

typedef struct {
	char *key, *label;
} Action;

typedef struct {
	uint32_t id;
	int      active;
	int      closing;         /* exit animation running; already signalled */
	int      fresh;           /* not laid out yet */
	int      dnd_notice;      /* local schedule notification */
	char    *app, *summary, *body;
	int      urgency;
	Action   actions[MAX_ACTIONS];
	int      nactions;
	int      timeout_ms;      /* 0 = persistent */
	struct timespec created;  /* ordering + "2m ago" */
	struct timespec deadline_base; /* timeout counts from here */
	float    h;               /* natural card height from the last layout */
	/* animated state (window coordinates, stack-relative y) */
	float    y, x, hcur, inset, op, cop, vis;
	float    swipe;           /* finger/touchpad offset while swiping */
	struct timespec swipe_t;  /* last touchpad swipe event */
	int      exit_dir;        /* -1 / +1 */
	cui_rect rect;            /* last painted card rectangle */
	cui_id   bid;             /* CUI id of the card body */
} Notif;

typedef struct {
	cui_color bg, fg, accent;
} UrgencyColors;

static const char *urgency_names[] = { "low", "normal", "critical" };

/* X */
static Display        *dpy;
static int             screen;
static Window          root;
static int             have_shape;
static int             argb;       /* 32-bit window under a compositor */
static char           *xrm_last;
static volatile sig_atomic_t reload;

/* theme */
static UrgencyColors   pal[3];
static cui_color       muted;
static char           *font_name, *mono_name;   /* effective fontconfig patterns */
static double          dpi = 96;                /* Xft.dpi, as Xft would use */

/* CUI */
static cui_font       *ui_font;
static float           font_px, mono_px;
static cui_sdl3       *host;
static cui_context    *ui;
static cui_theme       theme;
static SDL_Window     *sdl_win;
static Window          xwin;
static int             shown, dirty;
static int             win_x, win_y, win_w, win_h;
static float           pointer_x = -1, pointer_y = -1;
static int             animating, expanded;
static struct timespec last_frame, hover_t;
static Notif          *drag;                    /* card under a primary press */
static float           drag_x0;
static int             dragging;

static DBusConnection *dbus;
static int dnd_state, dnd_schedule_state, dnd_override = -1;
static uint32_t suppressed_id;
static Window tray_win, tray_owner;
static Atom tray_selection;
static Colormap tray_colormap;
static int tray_argb, tray_hovered;
static Window tray_tip;
static unsigned long tray_foreground = 0xffc4c4c4UL;
static struct timespec tray_hover_time;

static Notif           notifs[MAX_NOTIFS];
static uint32_t        next_id = 1;
static volatile sig_atomic_t running = 1;

static void dnd_set(int state);

/* ---------- utils ---------- */

static void
die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

static void *
ecalloc(size_t n, size_t s)
{
	void *p = calloc(n, s);
	if (!p) die("calloc: out of memory");
	return p;
}

static char *
estrdup(const char *s)
{
	char *r = strdup(s ? s : "");
	if (!r) die("strdup: out of memory");
	return r;
}

static long
ms_since(const struct timespec *t)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec - t->tv_sec) * 1000L + (now.tv_nsec - t->tv_nsec) / 1000000L;
}

static int
newer(const Notif *a, const Notif *b)
{
	return a->created.tv_sec > b->created.tv_sec ||
	       (a->created.tv_sec == b->created.tv_sec && a->created.tv_nsec > b->created.tv_nsec);
}

/* Strip HTML/pango markup. Decode basic entities. Return newly allocated. */
static char *
strip_markup(const char *in)
{
	if (!in) return estrdup("");
	size_t n = strlen(in);
	char *out = ecalloc(1, n + 1);
	size_t o = 0;
	for (size_t i = 0; i < n; ) {
		if (in[i] == '<') {
			while (i < n && in[i] != '>') i++;
			if (i < n) i++;
		} else if (in[i] == '&') {
			if (!strncmp(in + i, "&amp;", 5))       { out[o++] = '&';  i += 5; }
			else if (!strncmp(in + i, "&lt;", 4))   { out[o++] = '<';  i += 4; }
			else if (!strncmp(in + i, "&gt;", 4))   { out[o++] = '>';  i += 4; }
			else if (!strncmp(in + i, "&quot;", 6)) { out[o++] = '"';  i += 6; }
			else if (!strncmp(in + i, "&apos;", 6)) { out[o++] = '\''; i += 6; }
			else if (!strncmp(in + i, "&#", 2)) {
				size_t j = i + 2;
				while (j < n && in[j] != ';') j++;
				if (j < n) i = j + 1; else out[o++] = in[i++];
			} else {
				out[o++] = in[i++];
			}
		} else if (in[i] == '\t') {
			out[o++] = ' '; i++;
		} else if (in[i] == '\r') {
			i++;
		} else {
			out[o++] = in[i++];
		}
	}
	out[o] = 0;
	/* trim trailing newlines/spaces */
	while (o > 0 && (out[o - 1] == '\n' || out[o - 1] == ' ')) out[--o] = 0;
	return out;
}

/* ---------- monitor detection ---------- */

static void
get_focused_monitor(int *mx, int *my, int *mw, int *mh)
{
	int px = 0, py = 0, have_point = 0;

	Atom net_active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop = NULL;

	if (XGetWindowProperty(dpy, root, net_active, 0, 1, False, XA_WINDOW,
	        &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success
	    && prop && nitems > 0) {
		Window aw = *(Window *)prop;
		if (aw) {
			XWindowAttributes wa;
			Window child;
			int rx, ry;
			if (XGetWindowAttributes(dpy, aw, &wa)
			    && XTranslateCoordinates(dpy, aw, root, 0, 0, &rx, &ry, &child)) {
				px = rx + wa.width / 2;
				py = ry + wa.height / 2;
				have_point = 1;
			}
		}
	}
	if (prop) XFree(prop);

	if (!have_point) {
		Window rret, cret;
		int rx, ry, wx, wy;
		unsigned int mask;
		if (XQueryPointer(dpy, root, &rret, &cret, &rx, &ry, &wx, &wy, &mask)) {
			px = rx; py = ry; have_point = 1;
		}
	}

	int nmon = 0;
	XRRMonitorInfo *mons = XRRGetMonitors(dpy, root, True, &nmon);
	if (mons && nmon > 0) {
		int chosen = -1;
		for (int i = 0; i < nmon; i++) {
			if (have_point &&
			    px >= mons[i].x && px < mons[i].x + mons[i].width &&
			    py >= mons[i].y && py < mons[i].y + mons[i].height) {
				chosen = i;
				break;
			}
		}
		if (chosen < 0)
			for (int i = 0; i < nmon; i++) if (mons[i].primary) { chosen = i; break; }
		if (chosen < 0) chosen = 0;
		*mx = mons[chosen].x;
		*my = mons[chosen].y;
		*mw = mons[chosen].width;
		*mh = mons[chosen].height;
		XRRFreeMonitors(mons);
	} else {
		*mx = 0; *my = 0;
		*mw = DisplayWidth(dpy, screen);
		*mh = DisplayHeight(dpy, screen);
	}
}

/* ---------- theme: X resources ---------- */

static int
x_error_handler(Display *d, XErrorEvent *e)
{
	(void)d; (void)e;
	return 0;
}

static char *
get_resource_manager(void)
{
	Atom atom = XInternAtom(dpy, "RESOURCE_MANAGER", False);
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop = NULL;
	char *ret = NULL;

	if (XGetWindowProperty(dpy, root, atom, 0, 256 * 1024, False, XA_STRING,
	        &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success
	    && prop && actual_type == XA_STRING && actual_format == 8 && nitems > 0) {
		ret = ecalloc(1, nitems + 1);
		memcpy(ret, prop, nitems);
	}
	if (prop) XFree(prop);
	return ret;
}

/* First resource found among the NULL-terminated names ("nod.x.y"). The class
 * is derived by capitalising each component, so *.foreground, *foreground,
 * Nod.Foreground etc. all match the way xrdb users expect. */
static const char *
xres(XrmDatabase db, ...)
{
	va_list ap;
	const char *name, *found = NULL;
	va_start(ap, db);
	while (!found && (name = va_arg(ap, const char *))) {
		char class[128];
		snprintf(class, sizeof(class), "%s", name);
		for (char *p = class; *p; p++)
			if (p == class || p[-1] == '.')
				*p = (char)(*p >= 'a' && *p <= 'z' ? *p - 32 : *p);
		XrmValue v;
		char *type;
		if (db && XrmGetResource(db, name, class, &type, &v) && v.addr && *v.addr)
			found = v.addr;
	}
	va_end(ap);
	return found;
}

static cui_color
parse_color(const char *spec, const char *fallback)
{
	XColor c;
	Colormap cmap = DefaultColormap(dpy, screen);
	if ((!spec || !XParseColor(dpy, cmap, spec, &c)) && !XParseColor(dpy, cmap, fallback, &c))
		die("cannot parse color: %s", fallback);
	return (cui_color){ c.red >> 8, c.green >> 8, c.blue >> 8, 255 };
}

static cui_color
mix(cui_color a, cui_color b, float t)
{
	return (cui_color){
		(uint8_t)(a.r + (b.r - a.r) * t), (uint8_t)(a.g + (b.g - a.g) * t),
		(uint8_t)(a.b + (b.b - a.b) * t), 255 };
}

static cui_color
alpha(cui_color c, uint8_t a)
{
	c.a = a;
	return c;
}

static float
luminance(cui_color c)
{
	float v[3] = { c.r / 255.f, c.g / 255.f, c.b / 255.f };
	for (int i = 0; i < 3; i++)
		v[i] = v[i] <= .03928f ? v[i] / 12.92f : powf((v[i] + .055f) / 1.055f, 2.4f);
	return .2126f * v[0] + .7152f * v[1] + .0722f * v[2];
}

static float
contrast(cui_color a, cui_color b)
{
	float la = luminance(a), lb = luminance(b);
	return la > lb ? (la + .05f) / (lb + .05f) : (lb + .05f) / (la + .05f);
}

/* Wallpaper palettes give no contrast guarantees: pull c towards fg until it
 * is readable on bg (WCAG ratio). */
static cui_color
readable(cui_color c, cui_color bg, cui_color fg, float min)
{
	for (float t = 0; t <= 1; t += .05f) {
		cui_color m = mix(c, fg, t);
		if (contrast(m, bg) >= min) return m;
	}
	return fg;
}

static void
theme_load(void)
{
	static const char *accent_fallback[] = { "nod.color8", "nod.color4", "nod.color1" };
	char *rm = use_xresources ? get_resource_manager() : NULL;
	XrmDatabase db = rm ? XrmGetStringDatabase(rm) : NULL;
	free(rm);

	for (int i = 0; i < 3; i++) {
		char bg[64], fg[64], ac[64], bd[64];
		snprintf(bg, sizeof(bg), "nod.background.%s", urgency_names[i]);
		snprintf(fg, sizeof(fg), "nod.foreground.%s", urgency_names[i]);
		snprintf(ac, sizeof(ac), "nod.accent.%s", urgency_names[i]);
		snprintf(bd, sizeof(bd), "nod.border.%s", urgency_names[i]);
		pal[i].bg     = parse_color(xres(db, bg, "nod.background", NULL), bg_colors[i]);
		pal[i].fg     = parse_color(xres(db, fg, "nod.foreground", NULL), fg_colors[i]);
		pal[i].accent = parse_color(xres(db, ac, bd, accent_fallback[i], NULL), accent_colors[i]);
	}
	muted = parse_color(xres(db, "nod.muted", "nod.color8", NULL), muted_color);

	/* critical cards get a tinted surface so they stand out beyond the border */
	pal[2].bg = mix(pal[2].bg, pal[2].accent, critical_tint);
	for (int i = 0; i < 3; i++) {
		cui_color ink = luminance(pal[i].bg) > .18f ? (cui_color){ 0, 0, 0, 255 }
		                                            : (cui_color){ 255, 255, 255, 255 };
		pal[i].fg     = readable(pal[i].fg, pal[i].bg, ink, min_contrast);
		pal[i].accent = readable(pal[i].accent, pal[i].bg, pal[i].fg, min_contrast);
	}
	muted = readable(muted, pal[1].bg, pal[1].fg, min_contrast);

	const char *d = xres(db, "Xft.dpi", NULL);
	dpi = d && atof(d) > 0 ? atof(d) : 96;

	const char *f;
	free(font_name);
	free(mono_name);
	font_name = estrdup((f = xres(db, "nod.font", NULL)) ? f : font);
	mono_name = estrdup((f = xres(db, "nod.monoFont", NULL)) ? f : mono_font);

	if (db) XrmDestroyDatabase(db);

	/* Only button focus/selection tokens are taken from the CUI theme. */
	theme = cui_theme_dark();
	theme.background = pal[1].bg;
	theme.text       = pal[1].fg;
	theme.muted      = muted;
	theme.focus      = pal[1].accent;
	theme.border     = mix(pal[1].bg, pal[1].fg, .15f);
}

/* ---------- fonts (fontconfig name -> file) ---------- */

static char *
font_file(const char *name, int bold, float *px)
{
	FcPattern *pat = FcNameParse((const FcChar8 *)name);
	if (!pat) return NULL;
	if (bold) {
		FcPatternDel(pat, FC_WEIGHT);
		FcPatternDel(pat, FC_STYLE);
		FcPatternAddInteger(pat, FC_WEIGHT, FC_WEIGHT_BOLD);
	}
	FcPatternDel(pat, FC_DPI);
	FcPatternAddDouble(pat, FC_DPI, dpi);
	FcConfigSubstitute(NULL, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);
	FcResult res;
	FcPattern *m = FcFontMatch(NULL, pat, &res);
	char *ret = NULL;
	FcChar8 *file;
	double size;
	if (m && FcPatternGetString(m, FC_FILE, 0, &file) == FcResultMatch)
		ret = estrdup((const char *)file);
	if (px && FcPatternGetDouble(pat, FC_PIXEL_SIZE, 0, &size) == FcResultMatch)
		*px = (float)size;
	if (m) FcPatternDestroy(m);
	FcPatternDestroy(pat);
	return ret;
}

static cui_font *
fonts_open(void)
{
	char *reg = font_file(font_name, 0, &font_px);
	char *bold = font_file(font_name, 1, NULL);
	char *mono = font_file(mono_name, 0, &mono_px);
	char *monob = font_file(mono_name, 1, NULL);
	cui_font *f = reg ? cui_font_open(reg) : NULL;
	if (!f) die("cannot load font: %s", font_name);
	/* missing variants silently fall back to the regular face */
	if (bold)  cui_font_register_style(f, (cui_text_style){ .weight = CUI_TEXT_WEIGHT_BOLD }, bold);
	if (mono)  cui_font_register_style(f, (cui_text_style){ .monospace = true }, mono);
	if (monob) cui_font_register_style(f, (cui_text_style){ .weight = CUI_TEXT_WEIGHT_BOLD,
	                                                        .monospace = true }, monob);
	free(reg); free(bold); free(mono); free(monob);
	return f;
}

/* ---------- CUI host window ---------- */

static void
host_destroy(void)
{
	cui_destroy(ui);
	cui_sdl3_destroy(host);
	cui_font_close(ui_font);
	ui = NULL; host = NULL; ui_font = NULL; sdl_win = NULL; xwin = 0;
	shown = 0;
}

static void
host_create(void)
{
	SDL_SetHint(SDL_HINT_APP_ID, "nod");
	/* lets compositors match us (picom: wintypes { notification = ... }) */
	SDL_SetHint(SDL_HINT_X11_WINDOW_TYPE, "_NET_WM_WINDOW_TYPE_NOTIFICATION");
	SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

	/* With a compositor, ask SDL for a 32-bit visual: real transparency and
	 * antialiased corners instead of a 1-bit shape mask. */
	char cm[32], vid[32];
	XVisualInfo vi;
	snprintf(cm, sizeof(cm), "_NET_WM_CM_S%d", screen);
	if (XGetSelectionOwner(dpy, XInternAtom(dpy, cm, False))
	    && XMatchVisualInfo(dpy, screen, 32, TrueColor, &vi)) {
		snprintf(vid, sizeof(vid), "0x%lx", vi.visualid);
		SDL_SetHint(SDL_HINT_VIDEO_X11_VISUALID, vid);
	} else {
		SDL_ResetHint(SDL_HINT_VIDEO_X11_VISUALID);
	}

	ui_font = fonts_open();
	host = cui_sdl3_create("nod", width, 100, ui_font);
	if (!host) die("cannot create SDL3 window: %s", cui_sdl3_error());

	/* cui_sdl3 owns the only SDL window; turn it into an unmanaged popup. */
	int n = 0;
	SDL_Window **ws = SDL_GetWindows(&n);
	if (!ws || n < 1) die("cannot find SDL window");
	sdl_win = ws[n - 1];
	SDL_free(ws);
	SDL_HideWindow(sdl_win);
	SDL_SetWindowBordered(sdl_win, false);
	SDL_SetWindowResizable(sdl_win, false);
	SDL_SetWindowFocusable(sdl_win, false);
	SDL_SyncWindow(sdl_win);
	xwin = (Window)SDL_GetNumberProperty(SDL_GetWindowProperties(sdl_win),
	                                     SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
	if (!xwin) die("nod needs SDL's X11 video driver");
	XWindowAttributes wa;
	argb = XGetWindowAttributes(dpy, xwin, &wa) && wa.depth == 32;
	if (getenv("NOD_DEBUG")) fprintf(stderr, "nod: window depth %d, renderer %s\n", wa.depth, cui_sdl3_driver(host));
	XSetWindowAttributes swa = { .override_redirect = True };
	XChangeWindowAttributes(dpy, xwin, CWOverrideRedirect, &swa);
	XSync(dpy, False);

	ui = cui_create((cui_config){
		.width = width, .height = 100,
		.measure = cui_font_measure,
		.measure_styled = cui_font_measure_styled,
		.text_geometry = cui_font_text_geometry,
		.text_geometry_line = cui_font_text_geometry_line,
		.font_metrics = cui_font_get_metrics,
		.measure_user = ui_font });
	if (!ui) die("cannot create CUI context");
	win_w = win_h = 0;
	dirty = 1;
}

static void
theme_reload(void)
{
	char *of = estrdup(font_name), *om = estrdup(mono_name);
	double odpi = dpi;
	theme_load();
	if (strcmp(of, font_name) || strcmp(om, mono_name) || odpi != dpi) {
		/* glyph caches are bound to the font: rebuild the host */
		host_destroy();
		host_create();
	}
	free(of); free(om);
	dirty = 1;
}

static void
check_xrm_change(void)
{
	if (!use_xresources || xresources_poll_ms <= 0)
		return;
	char *cur = get_resource_manager();
	if ((cur == NULL) != (xrm_last == NULL) || (cur && strcmp(cur, xrm_last))) {
		free(xrm_last);
		xrm_last = cur;
		theme_reload();
	} else {
		free(cur);
	}
}

static void
on_sigusr1(int sig) { (void)sig; reload = 1; }

/* Restrict the window to the cards: input only with a compositor (cards are
 * drawn with real alpha), input and visuals without one. */
static void
set_shape(int w, int h)
{
	static uint64_t last;
	uint64_t sig = (1469598103934665603ull ^ (uint64_t)xwin) * 1099511628211ull ^ (uint64_t)w << 32 ^ (uint64_t)h;
	for (int i = 0; i < MAX_NOTIFS; i++) {
		const Notif *n = &notifs[i];
		if (!n->active || n->op < .05f) continue;
		float v[4] = { n->rect.x, n->rect.y, n->rect.w, n->rect.h };
		for (int k = 0; k < 4; k++)
			sig = (sig ^ (uint64_t)(int64_t)lroundf(v[k])) * 1099511628211ull;
	}
	if (!have_shape || sig == last) return;
	last = sig;
	Pixmap mask = XCreatePixmap(dpy, xwin, w, h, 1);
	GC gc = XCreateGC(dpy, mask, 0, NULL);
	XSetForeground(dpy, gc, 0);
	XFillRectangle(dpy, mask, gc, 0, 0, w, h);
	XSetForeground(dpy, gc, 1);
	for (int i = 0; i < MAX_NOTIFS; i++) {
		Notif *n = &notifs[i];
		if (!n->active || n->op < .05f) continue;
		int x = (int)lroundf(n->rect.x), y = (int)lroundf(n->rect.y);
		int cw = (int)lroundf(n->rect.w), ch = (int)lroundf(n->rect.h);
		int r = border_radius, d;
		if (r > cw / 2) r = cw / 2;
		if (r > ch / 2) r = ch / 2;
		d = 2 * r;
		XFillRectangle(dpy, mask, gc, x + r, y, cw - d, ch);
		XFillRectangle(dpy, mask, gc, x, y + r, cw, ch - d);
		if (r > 0) {
			XFillArc(dpy, mask, gc, x, y, d, d, 0, 360 * 64);
			XFillArc(dpy, mask, gc, x + cw - d, y, d, d, 0, 360 * 64);
			XFillArc(dpy, mask, gc, x, y + ch - d, d, d, 0, 360 * 64);
			XFillArc(dpy, mask, gc, x + cw - d, y + ch - d, d, d, 0, 360 * 64);
		}
	}
	XShapeCombineMask(dpy, xwin, ShapeInput, 0, 0, mask, ShapeSet);
	if (!argb)
		XShapeCombineMask(dpy, xwin, ShapeBounding, 0, 0, mask, ShapeSet);
	XFreeGC(dpy, gc);
	XFreePixmap(dpy, mask);
}

/* ---------- notification table ---------- */

static int
find_free_slot(void)
{
	for (int i = 0; i < MAX_NOTIFS; i++)
		if (!notifs[i].active) return i;
	return -1;
}

static int
find_by_id(uint32_t id)
{
	for (int i = 0; i < MAX_NOTIFS; i++)
		if (notifs[i].active && !notifs[i].closing && notifs[i].id == id) return i;
	return -1;
}

static int
count_active(void)
{
	int c = 0;
	for (int i = 0; i < MAX_NOTIFS; i++) if (notifs[i].active && !notifs[i].closing) c++;
	return c;
}

static int
find_oldest(void)
{
	int oldest = -1;
	for (int i = 0; i < MAX_NOTIFS; i++)
		if (notifs[i].active && !notifs[i].closing
		    && (oldest < 0 || newer(&notifs[oldest], &notifs[i])))
			oldest = i;
	return oldest;
}

static void
free_notif_content(Notif *n)
{
	free(n->app);     n->app = NULL;
	free(n->summary); n->summary = NULL;
	free(n->body);    n->body = NULL;
	for (int i = 0; i < n->nactions; i++) {
		free(n->actions[i].key);
		free(n->actions[i].label);
	}
	n->nactions = 0;
}

/* ---------- dbus signal emit ---------- */

static void
emit_closed(uint32_t id, uint32_t reason)
{
	if (!dbus) return;
	DBusMessage *m = dbus_message_new_signal(OBJ_PATH, IFACE, "NotificationClosed");
	if (!m) return;
	dbus_message_append_args(m,
	    DBUS_TYPE_UINT32, &id,
	    DBUS_TYPE_UINT32, &reason,
	    DBUS_TYPE_INVALID);
	dbus_connection_send(dbus, m, NULL);
	dbus_message_unref(m);
}

static void
emit_action(uint32_t id, const char *key)
{
	if (!dbus) return;
	DBusMessage *m = dbus_message_new_signal(OBJ_PATH, IFACE, "ActionInvoked");
	if (!m) return;
	dbus_message_append_args(m,
	    DBUS_TYPE_UINT32, &id,
	    DBUS_TYPE_STRING, &key,
	    DBUS_TYPE_INVALID);
	dbus_connection_send(dbus, m, NULL);
	dbus_message_unref(m);
}

static int
exit_side(void)
{
	return (position == TOP_LEFT || position == BOTTOM_LEFT) ? -1 : 1;
}

/* Signal the close now; the card animates out and is freed by update(). */
static void
dismiss(int i, uint32_t reason)
{
	Notif *n = &notifs[i];
	if (!n->active || n->closing) return;
	n->closing = 1;
	if (!n->exit_dir)
		n->exit_dir = exit_side();
	emit_closed(n->id, reason);
	dirty = 1;
}

static void
release(int i)
{
	free_notif_content(&notifs[i]);
	notifs[i].active = notifs[i].closing = 0;
	if (drag == &notifs[i]) drag = NULL;
}

/* ---------- card composition ---------- */

static void
rel_time(const Notif *n, char *buf, size_t sz)
{
	long s = ms_since(&n->created) / 1000;
	if (s < 10)         snprintf(buf, sz, "just now");
	else if (s < 60)    snprintf(buf, sz, "%lds ago", s);
	else if (s < 3600)  snprintf(buf, sz, "%ldm ago", s / 60);
	else                snprintf(buf, sz, "%ldh ago", s / 3600);
}

static void
hline(cui_context *c, cui_color col)
{
	cui_begin_box(c, 0, (cui_box){ .width = -1, .height = 1, .background = col });
	cui_end_box(c);
}

static void
spacer(cui_context *c)
{
	cui_begin_box(c, 0, (cui_box){ .width = -1 });
	cui_end_box(c);
}

static bool
card_button(const char *key, const char *label, const char *icon, cui_color fg,
            cui_color hover_bg)
{
	cui_color none = { 0 };
	cui_button_options o = {
		.variant = CUI_BUTTON_CUSTOM,
		.size = CUI_CONTROL_SMALL,
		.icon = icon ? cui_string(icon) : (cui_str){ 0 },
		.font_size = mono_px,
		.custom_colors = true,
		.colors = { .normal = none, .hover = hover_bg, .active = alpha(hover_bg, 90),
		            .foreground = fg, .border = none },
		.hide_focus_ring = true,
		.skip_tab = true,
		.custom_cursor = true,
		.cursor = CUI_CURSOR_POINTER,
	};
	return cui_button_ex(ui, cui_id_from(ui, key), &theme, cui_string(label), o);
}

static void
card(Notif *n, int idx)
{
	UrgencyColors *p = &pal[n->urgency];
	cui_color divider = mix(p->bg, p->fg, .12f);
	cui_text_style mono = { .monospace = true };
	cui_text_style monob = { .monospace = true, .weight = CUI_TEXT_WEIGHT_BOLD };
	char key[32], when[32];

	snprintf(key, sizeof(key), "n%u", n->id);
	cui_push_id(ui, key);
	/* outer: animated frame (position, height, inset, opacity), clips content */
	cui_begin_absolute_box(ui, cui_id_from(ui, "card"), (cui_box){
		.radius = border_radius, .clip = true,
		.background = p->bg, .border = p->accent, .border_width = show_border ? border_width : 0,
		.opacity = n->op, .custom_opacity = true }, n->rect);
	/* body: always laid out at full width so its natural height is stable */
	cui_id bid = cui_id_from(ui, "body");
	cui_begin_box(ui, bid, (cui_box){ .width = width, .padding = padding, .gap = 10,
	                                  .opacity = n->cop, .custom_opacity = true });

	/* header: ● app • urgency ............ 2m ago */
	if (show_app || show_urgency || show_time) {
		cui_begin_box(ui, 0, (cui_box){ .width = -1, .row = true, .gap = 6,
		                                .align_y = CUI_ALIGN_CENTER });
		cui_begin_box(ui, 0, (cui_box){ .width = 8, .height = 8, .radius = 4,
		                                .background = p->accent });
		cui_end_box(ui);
		if (show_app)
			cui_text_ex(ui, cui_string(n->app[0] ? n->app : "notification"), mono_px,
			            p->accent, 0, monob);
		if (show_app && show_urgency)
			cui_text_ex(ui, cui_string("\xe2\x80\xa2"), mono_px, muted, 0, mono);
		if (show_urgency)
			cui_text_ex(ui, cui_string(urgency_names[n->urgency]),
			            mono_px, n->urgency == 2 ? p->accent : muted, 0, mono);
		spacer(ui);
		if (show_time) {
			rel_time(n, when, sizeof(when));
			cui_text_ex(ui, cui_string(when), mono_px, muted, 0, mono);
		}
		cui_end_box(ui);
		if (divider_top)
			hline(ui, divider);
	}

	/* content */
	float inner = width - 2 * padding; /* fixed width so text wraps */
	cui_begin_box(ui, 0, (cui_box){ .width = inner, .gap = 4 });
	if (n->summary[0])
		cui_text_paragraph_colored(ui, cui_string(n->summary), roundf(font_px * 1.08f), p->fg, 0,
		                           (cui_text_style){ .weight = CUI_TEXT_WEIGHT_BOLD }, true, NULL, 0);
	if (n->body[0]) {
		float fs = roundf(font_px * .92f), lh = roundf(fs * 1.45f);
		cui_begin_box(ui, 0, (cui_box){ .width = inner, .clip = true,
		                                .max_height = max_lines > 0 ? lh * max_lines : 0 });
		cui_text_ex(ui, cui_string(n->body), fs, mix(p->fg, p->bg, .15f), lh,
		            (cui_text_style){ 0 });
		cui_end_box(ui);
	}
	cui_end_box(ui);

	/* footer: [▶ action] [action] ............ [✕ dismiss] */
	if (((action_buttons || n->dnd_notice) && n->nactions) || dismiss_button) {
		if (divider_bottom)
			hline(ui, divider);
		cui_begin_box(ui, 0, (cui_box){ .width = -1, .row = true, .gap = 4,
		                                .align_y = CUI_ALIGN_CENTER });
		for (int i = 0; (action_buttons || n->dnd_notice) && i < n->nactions; i++) {
			char akey[16];
			snprintf(akey, sizeof(akey), "a%d", i);
			if (card_button(akey, n->actions[i].label, i == 0 ? "play" : NULL,
			                p->accent, alpha(p->accent, 40))) {
				if (n->dnd_notice) dnd_set(!strcmp(n->actions[i].key, "on"));
			else emit_action(n->id, n->actions[i].key);
				dismiss(idx, 2);
			}
		}
		spacer(ui);
		if (dismiss_button && card_button("dismiss", "Dismiss", "close", muted, alpha(p->fg, 25)))
			dismiss(idx, 2);
		cui_end_box(ui);
	}

	cui_end_box(ui); /* body */
	cui_end_box(ui); /* card */
	cui_pop_id(ui);
	n->bid = bid; /* natural height is read from it after end_frame */
}

/* Active notifications, newest first: index 0 is the front of the stack. */
static int
sorted(int *idx)
{
	int n = 0;
	for (int i = 0; i < MAX_NOTIFS; i++)
		if (notifs[i].active) idx[n++] = i;
	for (int i = 0; i < n; i++)
		for (int j = i + 1; j < n; j++)
			if (newer(&notifs[idx[j]], &notifs[idx[i]])) {
				int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
			}
	return n;
}

static int
inside(const Notif *n, float x, float y)
{
	return n->op > .05f && x >= n->rect.x && x < n->rect.x + n->rect.w &&
	       y >= n->rect.y && y < n->rect.y + n->rect.h;
}

/* Topmost card under (x, y): the front of the stack is painted last. */
static Notif *
hit(const int *idx, int n, float x, float y)
{
	for (int k = 0; k < n; k++)
		if (!notifs[idx[k]].closing && inside(&notifs[idx[k]], x, y))
			return &notifs[idx[k]];
	return NULL;
}

/* Cards only move towards the screen edge they leave through. */
static float
toward_exit(float dx)
{
	return dx * exit_side() > 0 ? dx : 0;
}

static Notif *click;

/* Left click on a card without action buttons: invoke its default action. */
static void
card_clicked(Notif *c)
{
	if (!c->active || c->closing) return; /* the Dismiss button got there first */
	const char *key = NULL;
	for (int i = 0; i < c->nactions && !key; i++)
		if (!strcmp(c->actions[i].key, "default")) key = c->actions[i].key;
	if (!key && c->nactions) key = c->actions[0].key;
	if (key && c->dnd_notice) dnd_set(!strcmp(key, "on"));
	else if (key) emit_action(c->id, key);
	dismiss((int)(c - notifs), 2);
}

/* Pointer input that is not a button press: hover, right-click, swipes. */
static void
track_pointer(const int *idx, int n)
{
	size_t nev = 0;
	const cui_event *ev = cui_frame_events(ui, &nev);
	for (size_t e = 0; e < nev; e++) {
		const cui_event *v = &ev[e];
		Notif *c;
		switch (v->type) {
		case CUI_EVENT_POINTER_LEAVE:
			pointer_x = pointer_y = -1;
			break;
		case CUI_EVENT_POINTER_MOVE:
			pointer_x = v->x; pointer_y = v->y;
			if (drag) {
				drag->swipe = toward_exit(v->x - drag_x0);
				if (fabsf(drag->swipe) > 6) dragging = 1;
			}
			break;
		case CUI_EVENT_POINTER_DOWN:
			pointer_x = v->x; pointer_y = v->y;
			if (!(c = hit(idx, n, v->x, v->y))) break;
			if (v->pointer_button == CUI_POINTER_SECONDARY) {
				dismiss((int)(c - notifs), 2);
			} else if (v->pointer_button == CUI_POINTER_PRIMARY) {
				drag = c; drag_x0 = v->x; dragging = 0;
			}
			break;
		case CUI_EVENT_POINTER_UP:
			if (!drag) break;
			if (dragging) {
				cui_cancel_event_activation(ui, e); /* a swipe is not a click */
				if (fabsf(drag->swipe) > width * swipe_threshold) {
					dismiss((int)(drag - notifs), 2);
				}
			} else if (!action_buttons) {
				click = drag; /* acted on after the frame, unless a button took it */
			}
			drag->swipe = 0;
			drag = NULL; dragging = 0;
			break;
		case CUI_EVENT_SCROLL: /* touchpad two-finger swipe */
			if (fabsf(v->dx) <= fabsf(v->dy) || !(c = hit(idx, n, v->x, v->y))) break;
			c->swipe = toward_exit(c->swipe - v->dx * (v->scroll_precise ? 1 : 40));
			clock_gettime(CLOCK_MONOTONIC, &c->swipe_t);
			if (fabsf(c->swipe) > width * swipe_threshold) {
				dismiss((int)(c - notifs), 2);
			}
			break;
		default:
			break;
		}
	}
}

/* Frame-rate independent ease towards target; returns 1 while still moving. */
static int
approach(float *v, float target, float dt, float eps)
{
	*v += (target - *v) * (1 - expf(-dt * anim_speed));
	if (fabsf(target - *v) < eps) { *v = target; return 0; }
	return 1;
}

/* Compute every card's target (expanded list or collapsed deck), advance the
 * animation by dt and lay out the rectangles. Returns the window height. */
static float
update(const int *idx, int n, float dt)
{
	float off = 0, total = 0, front_h = 0;
	int depth = 0, top = position == TOP_LEFT || position == TOP_RIGHT;
	int enter_dir = exit_side();

	for (int k = 0; k < n; k++) {
		Notif *c = &notifs[idx[k]];
		total += c->h + stack_gap;
		if (!c->closing && !front_h) front_h = c->h;
	}

	for (int k = 0; k < n; k++) {
		Notif *c = &notifs[idx[k]];
		int d = depth < max_peek ? depth : max_peek;
		float ty, th = c->h, tin = 0, tcop = 1, top_ = 1, tx = 0;
		float tvis = c->closing ? 0 : 1;

		if (expanded) {
			ty = off;
			off += (c->h + stack_gap) * c->vis;
		} else {
			ty = d * peek;
			tin = d * peek;
			if (depth > 0) {
				th = front_h;     /* behind cards show only their bottom edge */
				tcop = 0;
				top_ = depth <= max_peek ? 1 - .12f * depth : 0;
			}
		}
		if (!c->closing) depth++;

		int swiping = c == drag || ms_since(&c->swipe_t) < 200;
		if (!swiping) c->swipe = 0;
		if (c->closing) { tx = c->exit_dir * (float)width; top_ = 0; }
		else            tx = c->swipe;

		if (c->fresh && c->h > 0) {  /* first layout: start just off the edge */
			c->fresh = 0;
			c->y = ty; c->hcur = th; c->inset = tin; c->cop = tcop;
			c->x = enter_dir * width * .35f; c->op = 0; c->vis = 0;
		}
		if (swiping && !c->closing) c->x = tx; /* follow the finger exactly */
		if (!c->fresh) {
			animating |= approach(&c->y, ty, dt, .3f);
			animating |= approach(&c->x, tx, dt, .3f);
			animating |= approach(&c->hcur, th, dt, .3f);
			animating |= approach(&c->inset, tin, dt, .3f);
			animating |= approach(&c->op, top_, dt, .01f);
			animating |= approach(&c->cop, tcop, dt, .01f);
			animating |= approach(&c->vis, tvis, dt, .01f);
		}
	}

	float H = ceilf(total);
	for (int k = 0; k < n; k++) {
		Notif *c = &notifs[idx[k]];
		float ry = top ? c->y : H - c->y - c->hcur;
		float in = roundf(c->inset);
		c->rect = (cui_rect){ roundf(in + c->x), roundf(ry), width - 2 * in,
		                      c->fresh ? 0 : roundf(c->hcur) };
	}
	return H > 1 ? H : 1;
}

static void
hide(void)
{
	if (shown) SDL_HideWindow(sdl_win);
	shown = 0;
	pointer_x = pointer_y = -1;
	drag = NULL;
}

static void
render(void)
{
	int idx[MAX_NOTIFS];
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	float dt = (now.tv_sec - last_frame.tv_sec) + (now.tv_nsec - last_frame.tv_nsec) / 1e9f;
	if (dt > .05f || dt < 0) dt = .016f;
	last_frame = now;

	/* finished exit animations free their slot */
	for (int i = 0; i < MAX_NOTIFS; i++)
		if (notifs[i].active && notifs[i].closing && notifs[i].op < .02f && notifs[i].vis < .02f)
			release(i);

	int n = sorted(idx);
	if (!n) { hide(); animating = 0; return; }

	/* hover expands the deck; it folds back shortly after the pointer leaves */
	int live = count_active(), hovered = hit(idx, n, pointer_x, pointer_y) != NULL;
	if (hovered || drag) {
		hover_t = now;
		for (int k = 0; k < n; k++) /* reading: hold every timeout */
			notifs[idx[k]].deadline_base = now;
	}
	expanded = !collapse || live <= 1 || hovered || drag || ms_since(&hover_t) < 400;

	/* keep frames coming while a fold-back is pending */
	animating = expanded && !hovered && !drag && ms_since(&hover_t) < 400;
	cui_id rid = cui_id_from(ui, "root");
	cui_draw_list list = { 0 };
	float H = 1;
	/* new or re-wrapped cards need their height before they can be placed:
	 * lay out again (without advancing time) until heights are stable */
	for (int pass = 0; pass < 3; pass++) {
		H = update(idx, n, pass ? 0 : dt);
		if (!cui_begin_frame(ui, width, H, pass ? 0 : dt)) return;
		if (!pass)
			track_pointer(idx, n);
		cui_begin_box(ui, rid, (cui_box){ .width = width, .height = H,
		                                  .fixed_width = true, .fixed_height = true });
		for (int k = n - 1; k >= 0; k--) /* back to front */
			card(&notifs[idx[k]], idx[k]);
		cui_end_box(ui);
		list = cui_end_frame(ui);
		if (click) { card_clicked(click); click = NULL; }
		if (cui_last_error(ui) != CUI_OK) {
			fprintf(stderr, "nod: CUI layout error %d\n", (int)cui_last_error(ui));
			cui_clear_error(ui);
		}
		int changed = 0;
		for (int k = 0; k < n; k++) {
			Notif *c = &notifs[idx[k]];
			float h = cui_bounds(ui, c->bid).h;
			if (h > 0 && fabsf(h - c->h) > .5f) { c->h = h; changed = 1; }
		}
		if (!changed) break;
	}
	/* size and place the window, then paint */
	static int mx, my, mw, mh;
	static struct timespec mon_t;
	if (!shown || ms_since(&mon_t) > 1000) { /* X round trips: not every frame */
		get_focused_monitor(&mx, &my, &mw, &mh);
		mon_t = now;
	}
	int w = width, h = (int)H;
	int top = position == TOP_LEFT || position == TOP_RIGHT;
	int x = (position == TOP_LEFT || position == BOTTOM_LEFT) ? mx + gap_x : mx + mw - gap_x - w;
	int y = top ? my + gap_y : my + mh - gap_y - h;
	if (x != win_x || y != win_y || w != win_w || h != win_h || !shown) {
		/* one atomic request for the server (no half-moved frame on screen),
		 * then the same geometry through SDL so its cached size, used by the
		 * renderer's logical presentation, matches before we paint */
		XMoveResizeWindow(dpy, xwin, x, y, w, h);
		XFlush(dpy);
		SDL_SetWindowSize(sdl_win, w, h);
		SDL_SetWindowPosition(sdl_win, x, y);
		win_x = x; win_y = y; win_w = w; win_h = h;
		if (!shown) {
			SDL_ShowWindow(sdl_win);
			XRaiseWindow(dpy, xwin);
			XFlush(dpy);
			shown = 1;
		}
		SDL_SyncWindow(sdl_win);
		for (int tries = 0; tries < 20; tries++) {
			int cw = 0, ch = 0;
			SDL_PumpEvents();
			if (SDL_GetWindowSize(sdl_win, &cw, &ch) && cw == w && ch == h) break;
			SDL_Delay(2);
		}
	}
	set_shape(w, h);
	cui_sdl3_render(host, &list, (cui_color){ 0, 0, 0, 0 });
	cui_sdl3_set_cursor(host, dragging ? CUI_CURSOR_GRABBING : cui_pointer_cursor(ui));
	/* input of this frame changed the hover state: the next frame reacts */
	if ((hit(idx, n, pointer_x, pointer_y) != NULL) != hovered || drag)
		dirty = 1;
}

/* Schedule intervals belong to their starting day. */
static int
scheduled_dnd(void)
{
 time_t now = time(NULL);
 struct tm local;
 if (!localtime_r(&now, &local)) return 0;
 int minute = local.tm_hour * 60 + local.tm_min;
 for (int offset = 0; offset < 2; offset++) {
  int day = (local.tm_wday + 7 - offset) % 7;
  const int *v = dnd_schedule[day];
  if (v[0] < 0 || v[0] > 23 || v[1] < 0 || v[1] > 59 ||
      v[2] < 0 || v[2] > 23 || v[3] < 0 || v[3] > 59) continue;
  int start = v[0] * 60 + v[1], end = v[2] * 60 + v[3];
  if (start == end) continue;
  if (!offset && minute >= start && (end < start || minute < end)) return 1;
  if (offset && end < start && minute < end) return 1;
 }
 return 0;
}

static void
tray_draw(void)
{
 if (!tray_win) return;
 XWindowAttributes a;
 if (!XGetWindowAttributes(dpy, tray_win, &a)) return;
 GC gc = XCreateGC(dpy, tray_win, 0, NULL);
 if (tray_argb) {
  XSetForeground(dpy, gc, 0);
  XFillRectangle(dpy, tray_win, gc, 0, 0, a.width, a.height);
 } else {
  XSetWindowBackgroundPixmap(dpy, tray_win, ParentRelative);
  XClearWindow(dpy, tray_win);
 }
 XSetForeground(dpy, gc, tray_foreground);
 int size = (a.width < a.height ? a.width : a.height) - 6;
 if (size > 14) size = 14;
 if (size > 2) {
  XFillArc(dpy, tray_win, gc, (a.width-size)/2, (a.height-size)/2, size, size, 0, 360*64);
  if (dnd_state) {
   XSetForeground(dpy, gc, 0);
   XSetLineAttributes(dpy, gc, 2, LineSolid, CapRound, JoinRound);
   XDrawLine(dpy, tray_win, gc, (a.width-size)/2+2, (a.height+size)/2-2, (a.width+size)/2-2, (a.height-size)/2+2);
  }
 }
 XFreeGC(dpy, gc);
 XFlush(dpy);
}

static void
tray_tip_hide(void)
{
 if (tray_tip) { XDestroyWindow(dpy, tray_tip); tray_tip = 0; }
}

static void
tray_tip_show(void)
{
 if (!tray_win || tray_tip) return;
 const char *label = dnd_state ? "Do not disturb: on" : "Do not disturb: off";
 XFontStruct *font = XLoadQueryFont(dpy, "fixed");
 int width = (font ? XTextWidth(font, label, (int)strlen(label)) : (int)strlen(label) * 6) + 16;
 int height = font ? font->ascent + font->descent + 10 : 24;
 Window child; int x, y;
 XWindowAttributes a;
 if (!XGetWindowAttributes(dpy, tray_win, &a) ||
     !XTranslateCoordinates(dpy, tray_win, root, 0, 0, &x, &y, &child)) {
  if (font) XFreeFont(dpy, font);
  return;
 }
 y = y + a.height + height <= DisplayHeight(dpy, screen) ? y + a.height : y - height;
 if (x + width > DisplayWidth(dpy, screen)) x = DisplayWidth(dpy, screen) - width;
 if (x < 0) x = 0;
 if (y < 0) y = 0;
 XSetWindowAttributes attrs = { .override_redirect = True, .background_pixel = BlackPixel(dpy, screen) };
 tray_tip = XCreateWindow(dpy, root, x, y, width, height, 1, CopyFromParent,
                          InputOutput, CopyFromParent, CWOverrideRedirect | CWBackPixel, &attrs);
 XMapRaised(dpy, tray_tip);
 GC gc = XCreateGC(dpy, tray_tip, 0, NULL);
 XSetForeground(dpy, gc, WhitePixel(dpy, screen));
 if (font) XSetFont(dpy, gc, font->fid);
 XDrawString(dpy, tray_tip, gc, 8, font ? 5 + font->ascent : 16, label, (int)strlen(label));
 XFreeGC(dpy, gc);
 if (font) XFreeFont(dpy, font);
 XFlush(dpy);
}

/* Schedule notices are local so they remain visible while DND is active. */
static void
dnd_schedule_notice(int state)
{
 for (int i = 0; i < MAX_NOTIFS; i++)
  if (notifs[i].active && !notifs[i].closing && notifs[i].dnd_notice) dismiss(i, 3);
 while (count_active() >= max_visible) {
  int oldest = find_oldest();
  if (oldest < 0) break;
  dismiss(oldest, 2);
 }
 int slot = find_free_slot();
 if (slot < 0) {
  for (int i = 0; i < MAX_NOTIFS; i++)
   if (notifs[i].closing) { release(i); slot = i; break; }
 }
 if (slot < 0) return;
 Notif *n = &notifs[slot];
 memset(n, 0, sizeof(*n));
 n->id = next_id++;
 if (!next_id) next_id = 1;
 n->active = n->fresh = n->dnd_notice = 1;
 n->app = estrdup("nod");
 n->summary = estrdup(state ? "Do not disturb is on" : "Do not disturb is off");
 n->body = estrdup("Changed by your schedule.");
 n->urgency = 1;
 n->actions[0].key = estrdup(state ? "off" : "on");
 n->actions[0].label = estrdup(state ? "Turn off" : "Turn on");
 n->nactions = 1;
 n->timeout_ms = timeouts[1] * 1000;
 clock_gettime(CLOCK_MONOTONIC, &n->created);
 n->deadline_base = n->created;
 dirty = 1;
}

static void
dnd_update(void)
{
 int scheduled = scheduled_dnd();
 int transition = scheduled != dnd_schedule_state;
 if (transition) { dnd_schedule_state = scheduled; dnd_override = -1; }
 int state = dnd_override < 0 ? scheduled : dnd_override;
 if (state != dnd_state) {
  dnd_state = state;
  tray_draw(); tray_tip_hide();
  if (transition) dnd_schedule_notice(state);
 }
}

static void
dnd_set(int state)
{
 dnd_update();
 dnd_override = !!state;
 dnd_update();
}

static void
dnd_toggle(void)
{
 dnd_update();
 dnd_override = !dnd_state;
 dnd_update();
}

/* XEmbed system tray, supported by traditional X11 panels. */
static void
tray_update(void)
{
 Window owner = XGetSelectionOwner(dpy, tray_selection);
 if (owner != tray_owner) {
  tray_owner = owner;
  tray_tip_hide(); tray_hovered = 0;
  if (tray_win) { XDestroyWindow(dpy, tray_win); tray_win = 0; }
  if (tray_colormap) { XFreeColormap(dpy, tray_colormap); tray_colormap = 0; }
  if (owner) {
   /* Follow the visual requested by the tray manager, including ARGB trays. */
   Atom visual_atom = XInternAtom(dpy, "_NET_SYSTEM_TRAY_VISUAL", False), actual;
   int format; unsigned long count, remaining; unsigned char *property = NULL;
   XVisualInfo *vi = NULL; int nvi = 0;
   if (XGetWindowProperty(dpy, owner, visual_atom, 0, 1, False, XA_VISUALID,
       &actual, &format, &count, &remaining, &property) == Success &&
       property && actual == XA_VISUALID && format == 32 && count == 1) {
    XVisualInfo query = { .visualid = *(unsigned long *)property, .screen = screen };
    vi = XGetVisualInfo(dpy, VisualIDMask | VisualScreenMask, &query, &nvi);
   }
   if (property) XFree(property);
   XRenderPictFormat *pf = vi ? XRenderFindVisualFormat(dpy, vi->visual) : NULL;
   tray_argb = vi && vi->depth == 32 && pf && pf->type == PictTypeDirect && pf->direct.alphaMask;
   if (tray_argb) {
    tray_colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    XSetWindowAttributes attr = { .colormap = tray_colormap, .background_pixel = 0, .border_pixel = 0 };
    tray_win = XCreateWindow(dpy, root, 0, 0, 24, 24, 0, vi->depth, InputOutput,
                             vi->visual, CWColormap | CWBackPixel | CWBorderPixel, &attr);
   } else {
    tray_win = XCreateSimpleWindow(dpy, root, 0, 0, 24, 24, 0, 0, 0);
   }
   if (vi) XFree(vi);
   /* XEmbed tray foreground is three 16-bit CARDINAL color components. */
   Atom colors = XInternAtom(dpy, "_NET_SYSTEM_TRAY_COLORS", False);
   property = NULL;
   unsigned long red = 0xc4, green = 0xc4, blue = 0xc4;
   if (XGetWindowProperty(dpy, owner, colors, 0, 12, False, XA_CARDINAL,
       &actual, &format, &count, &remaining, &property) == Success && property &&
       actual == XA_CARDINAL && format == 32 && count >= 3) {
    unsigned long *rgb = (unsigned long *)property;
    red = (rgb[0] & 0xffff) >> 8; green = (rgb[1] & 0xffff) >> 8; blue = (rgb[2] & 0xffff) >> 8;
   }
   if (property) XFree(property);
   if (tray_argb) tray_foreground = 0xff000000UL | (red << 16) | (green << 8) | blue;
   else {
    XColor color = { .red = red * 257, .green = green * 257, .blue = blue * 257, .flags = DoRed | DoGreen | DoBlue };
    tray_foreground = XAllocColor(dpy, DefaultColormap(dpy, screen), &color) ? color.pixel : WhitePixel(dpy, screen);
   }
   XSelectInput(dpy, tray_win, ExposureMask | ButtonPressMask | StructureNotifyMask | EnterWindowMask | LeaveWindowMask);
   Atom info = XInternAtom(dpy, "_XEMBED_INFO", False);
   unsigned long data[2] = { 0, 1 };
   XChangeProperty(dpy, tray_win, info, info, 32, PropModeReplace, (unsigned char *)data, 2);
   XStoreName(dpy, tray_win, "nod: click to toggle do not disturb");
   XEvent e = {0}; e.xclient.type = ClientMessage; e.xclient.window = owner;
   e.xclient.message_type = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
   e.xclient.format = 32; e.xclient.data.l[0] = CurrentTime;
   e.xclient.data.l[1] = 0; e.xclient.data.l[2] = tray_win;
   XSendEvent(dpy, owner, False, NoEventMask, &e); XFlush(dpy);
  }
 }
 while (XPending(dpy)) {
  XEvent e; XNextEvent(dpy, &e);
  if (e.xany.window != tray_win) continue;
  if (e.type == Expose || e.type == ConfigureNotify) tray_draw();
  else if (e.type == EnterNotify) { tray_hovered = 1; clock_gettime(CLOCK_MONOTONIC, &tray_hover_time); }
  else if (e.type == LeaveNotify) { tray_hovered = 0; tray_tip_hide(); }
  else if (e.type == ButtonPress && e.xbutton.button == Button1) dnd_toggle();
  else if (e.type == DestroyNotify) { tray_win = 0; tray_owner = 0; tray_hovered = 0; tray_tip_hide(); }
 }
 if (tray_hovered && !tray_tip && ms_since(&tray_hover_time) >= 600) tray_tip_show();
}

/* ---------- Notify handler ---------- */

static uint32_t
handle_notify(DBusMessage *msg)
{
	DBusMessageIter args;
	if (!dbus_message_iter_init(msg, &args)) return 0;

	const char *app_name = "", *app_icon = "", *summary = "", *body = "";
	const char *acts[2 * MAX_ACTIONS];
	int nacts = 0;
	uint32_t replaces_id = 0;
	int32_t  expire_timeout = -1;
	int      urgency = 1;

	if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) return 0;
	dbus_message_iter_get_basic(&args, &app_name);
	dbus_message_iter_next(&args);

	if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_UINT32) return 0;
	dbus_message_iter_get_basic(&args, &replaces_id);
	dbus_message_iter_next(&args);

	if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) return 0;
	dbus_message_iter_get_basic(&args, &app_icon);
	dbus_message_iter_next(&args);

	if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) return 0;
	dbus_message_iter_get_basic(&args, &summary);
	dbus_message_iter_next(&args);

	if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) return 0;
	dbus_message_iter_get_basic(&args, &body);
	dbus_message_iter_next(&args);

	/* actions: as — alternating key, label */
	if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_ARRAY) {
		DBusMessageIter ai;
		dbus_message_iter_recurse(&args, &ai);
		while (dbus_message_iter_get_arg_type(&ai) == DBUS_TYPE_STRING
		       && nacts < 2 * MAX_ACTIONS) {
			dbus_message_iter_get_basic(&ai, &acts[nacts++]);
			dbus_message_iter_next(&ai);
		}
		nacts &= ~1;
	}
	dbus_message_iter_next(&args);

	/* hints: a{sv} */
	if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_ARRAY) {
		DBusMessageIter hi;
		dbus_message_iter_recurse(&args, &hi);
		while (dbus_message_iter_get_arg_type(&hi) == DBUS_TYPE_DICT_ENTRY) {
			DBusMessageIter de;
			dbus_message_iter_recurse(&hi, &de);
			const char *key = NULL;
			if (dbus_message_iter_get_arg_type(&de) == DBUS_TYPE_STRING)
				dbus_message_iter_get_basic(&de, &key);
			dbus_message_iter_next(&de);
			if (key && !strcmp(key, "urgency")
			    && dbus_message_iter_get_arg_type(&de) == DBUS_TYPE_VARIANT) {
				DBusMessageIter var;
				dbus_message_iter_recurse(&de, &var);
				if (dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_BYTE) {
					uint8_t u = 0;
					dbus_message_iter_get_basic(&var, &u);
					if (u <= 2) urgency = u;
				}
			}
			dbus_message_iter_next(&hi);
		}
	}
	dbus_message_iter_next(&args);

	if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INT32)
		dbus_message_iter_get_basic(&args, &expire_timeout);

	(void)app_icon;

	dnd_update();
	int muted = dnd_state;
	for (size_t i = 0; i < LEN(dnd_muted_apps); i++)
		if (dnd_muted_apps[i] && !strcasecmp(app_name, dnd_muted_apps[i])) muted = 1;
	if (muted) {
		int old = replaces_id ? find_by_id(replaces_id) : -1;
		uint32_t id = old >= 0 ? replaces_id : next_id++;
		if (!next_id) next_id = 1;
		if (old >= 0) dismiss(old, 3);
		else suppressed_id = id;
		return id;
	}

	/* locate / allocate slot */
	int slot = -1;
	int is_replace = 0;
	if (replaces_id != 0) {
		slot = find_by_id(replaces_id);
		if (slot >= 0) is_replace = 1;
	}
	if (slot < 0) {
		while (count_active() >= max_visible) {
			int o = find_oldest();
			if (o < 0) break;
			dismiss(o, 2);
		}
		if ((slot = find_free_slot()) < 0) { /* only exiting cards left: cut one short */
			for (int i = 0; i < MAX_NOTIFS && slot < 0; i++)
				if (notifs[i].closing) { release(i); slot = i; }
			if (slot < 0) return 0;
		}
	}

	Notif *n = &notifs[slot];
	uint32_t id;
	if (is_replace) {
		id = n->id;
		free_notif_content(n);
	} else {
		id = next_id++;
		if (next_id == 0) next_id = 1;
		memset(n, 0, sizeof(*n));
		n->id = id;
		n->fresh = 1;
		clock_gettime(CLOCK_MONOTONIC, &n->created);
	}

	n->app     = strip_markup(app_name);
	n->summary = strip_markup(summary);
	n->body    = strip_markup(body);
	n->urgency = urgency;
	for (int i = 0; i < nacts; i += 2) {
		Action *a = &n->actions[n->nactions++];
		a->key   = estrdup(acts[i]);
		/* "default" with an empty label still deserves a visible button */
		a->label = estrdup(acts[i + 1][0] ? acts[i + 1] : "Open");
	}

	n->timeout_ms = expire_timeout > 0 ? expire_timeout : timeouts[urgency] * 1000;
	clock_gettime(CLOCK_MONOTONIC, &n->deadline_base);
	n->active = 1;
	dirty = 1;
	return id;
}

/* ---------- introspection + dispatch ---------- */

static const char *introspect_xml =
	"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
	" \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
	"<node>\n"
	"  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
	"    <method name=\"Introspect\">\n"
	"      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
	"    </method>\n"
	"  </interface>\n"
	"  <interface name=\"org.nod.Dnd\">\n"
	"    <method name=\"Toggle\"><arg direction=\"out\" type=\"b\"/></method>\n"
	"    <method name=\"Status\"><arg direction=\"out\" type=\"b\"/></method>\n"
	"    <method name=\"On\"><arg direction=\"out\" type=\"b\"/></method>\n"
	"    <method name=\"Off\"><arg direction=\"out\" type=\"b\"/></method>\n"
	"  </interface>\n"
	"  <interface name=\"org.freedesktop.Notifications\">\n"
	"    <method name=\"GetCapabilities\">\n"
	"      <arg direction=\"out\" name=\"caps\" type=\"as\"/>\n"
	"    </method>\n"
	"    <method name=\"Notify\">\n"
	"      <arg direction=\"in\"  name=\"app_name\"       type=\"s\"/>\n"
	"      <arg direction=\"in\"  name=\"replaces_id\"    type=\"u\"/>\n"
	"      <arg direction=\"in\"  name=\"app_icon\"       type=\"s\"/>\n"
	"      <arg direction=\"in\"  name=\"summary\"        type=\"s\"/>\n"
	"      <arg direction=\"in\"  name=\"body\"           type=\"s\"/>\n"
	"      <arg direction=\"in\"  name=\"actions\"        type=\"as\"/>\n"
	"      <arg direction=\"in\"  name=\"hints\"          type=\"a{sv}\"/>\n"
	"      <arg direction=\"in\"  name=\"expire_timeout\" type=\"i\"/>\n"
	"      <arg direction=\"out\" name=\"id\"             type=\"u\"/>\n"
	"    </method>\n"
	"    <method name=\"CloseNotification\">\n"
	"      <arg direction=\"in\" name=\"id\" type=\"u\"/>\n"
	"    </method>\n"
	"    <method name=\"GetServerInformation\">\n"
	"      <arg direction=\"out\" name=\"name\"         type=\"s\"/>\n"
	"      <arg direction=\"out\" name=\"vendor\"       type=\"s\"/>\n"
	"      <arg direction=\"out\" name=\"version\"      type=\"s\"/>\n"
	"      <arg direction=\"out\" name=\"spec_version\" type=\"s\"/>\n"
	"    </method>\n"
	"    <signal name=\"NotificationClosed\">\n"
	"      <arg name=\"id\"     type=\"u\"/>\n"
	"      <arg name=\"reason\" type=\"u\"/>\n"
	"    </signal>\n"
	"    <signal name=\"ActionInvoked\">\n"
	"      <arg name=\"id\"         type=\"u\"/>\n"
	"      <arg name=\"action_key\" type=\"s\"/>\n"
	"    </signal>\n"
	"  </interface>\n"
	"</node>\n";

static DBusHandlerResult
dbus_msg_handler(DBusConnection *c, DBusMessage *msg, void *ud)
{
	(void)ud;
	const char *iface  = dbus_message_get_interface(msg);
	const char *member = dbus_message_get_member(msg);
	DBusMessage *reply = NULL;

	if (!iface || !member)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!strcmp(iface, "org.freedesktop.DBus.Introspectable")
	    && !strcmp(member, "Introspect")) {
		reply = dbus_message_new_method_return(msg);
		if (reply) dbus_message_append_args(reply,
		    DBUS_TYPE_STRING, &introspect_xml, DBUS_TYPE_INVALID);
	} else if (!strcmp(iface, "org.nod.Dnd")) {
		if (!strcmp(member, "Toggle")) dnd_toggle();
		else if (!strcmp(member, "Status")) dnd_update();
		else if (!strcmp(member, "On") || !strcmp(member, "Off")) {
			dnd_update(); dnd_override = !strcmp(member, "On"); dnd_update();
		} else return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		reply = dbus_message_new_method_return(msg);
		dbus_bool_t state = dnd_state;
		if (reply) dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &state, DBUS_TYPE_INVALID);
	} else if (!strcmp(iface, IFACE)) {
		if (!strcmp(member, "GetCapabilities")) {
			reply = dbus_message_new_method_return(msg);
			if (reply) {
				DBusMessageIter it, sub;
				dbus_message_iter_init_append(reply, &it);
				dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY, "s", &sub);
				const char *c1 = "body", *c2 = "actions";
				dbus_message_iter_append_basic(&sub, DBUS_TYPE_STRING, &c1);
				dbus_message_iter_append_basic(&sub, DBUS_TYPE_STRING, &c2);
				dbus_message_iter_close_container(&it, &sub);
			}
		} else if (!strcmp(member, "GetServerInformation")) {
			reply = dbus_message_new_method_return(msg);
			if (reply) {
				const char *name = "nod", *vendor = "nod";
				const char *ver  = VERSION, *spec = "1.2";
				dbus_message_append_args(reply,
				    DBUS_TYPE_STRING, &name,
				    DBUS_TYPE_STRING, &vendor,
				    DBUS_TYPE_STRING, &ver,
				    DBUS_TYPE_STRING, &spec,
				    DBUS_TYPE_INVALID);
			}
		} else if (!strcmp(member, "Notify")) {
			uint32_t id = handle_notify(msg);
			reply = dbus_message_new_method_return(msg);
			if (reply) dbus_message_append_args(reply,
			    DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);
		} else if (!strcmp(member, "CloseNotification")) {
			uint32_t id = 0;
			dbus_message_get_args(msg, NULL,
			    DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);
			int i = find_by_id(id);
			if (i >= 0) dismiss(i, 3);
			reply = dbus_message_new_method_return(msg);
		} else {
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
	} else {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (reply) {
		dbus_connection_send(c, reply, NULL);
		dbus_message_unref(reply);
	}
	if (suppressed_id) { emit_closed(suppressed_id, 3); suppressed_id = 0; }
	return DBUS_HANDLER_RESULT_HANDLED;
}

static void
dbus_init(void)
{
	DBusError err;
	dbus_error_init(&err);
	dbus = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) die("dbus: %s", err.message);
	if (!dbus) die("cannot connect to session bus");

	int r = dbus_bus_request_name(dbus, BUS_NAME,
	    DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
	if (dbus_error_is_set(&err)) die("dbus request_name: %s", err.message);
	if (r != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
		die("another notification daemon owns %s", BUS_NAME);

	static const DBusObjectPathVTable vt = {
		.message_function = dbus_msg_handler,
	};
	if (!dbus_connection_register_object_path(dbus, OBJ_PATH, &vt, NULL))
		die("dbus register object path failed");
}

/* ---------- main loop ---------- */

static void
on_signal(int sig) { (void)sig; running = 0; }

/* ms until n times out; -1 if persistent */
static long
ms_remaining(Notif *n)
{
	if (n->timeout_ms <= 0) return -1;
	long rem = n->timeout_ms - ms_since(&n->deadline_base);
	return rem < 0 ? 0 : rem;
}

int
main(int argc, char *argv[])
{
	if (argc > 1) {
		if (!strcmp(argv[1], "-v")) {
			printf("nod %s\n", VERSION);
			return 0;
		}
		fprintf(stderr, "usage: nod [-v]\n");
		return 1;
	}

	struct sigaction sa = { 0 };
	sa.sa_handler = on_signal;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT,  &sa, NULL);
	signal(SIGPIPE, SIG_IGN);
	sa.sa_handler = on_sigusr1;
	sigaction(SIGUSR1, &sa, NULL);

	/* SDL must render through X11: nod relies on override-redirect + Shape */
	SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");

	if (!(dpy = XOpenDisplay(NULL))) die("cannot open display");
	XSetErrorHandler(x_error_handler);
	XrmInitialize();
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	int sev, serr;
	have_shape = XShapeQueryExtension(dpy, &sev, &serr);
	if (!FcInit()) die("cannot initialise fontconfig");

	xrm_last = use_xresources ? get_resource_manager() : NULL;
	theme_load();
	host_create();
	dbus_init();
	char tray_name[64];
	snprintf(tray_name, sizeof(tray_name), "_NET_SYSTEM_TRAY_S%d", screen);
	tray_selection = XInternAtom(dpy, tray_name, False);
	dnd_schedule_state = scheduled_dnd(); dnd_state = dnd_schedule_state;
	tray_update();

	int dfd = -1;
	if (!dbus_connection_get_unix_fd(dbus, &dfd))
		die("cannot get dbus fd");

	struct timespec last_paint = { 0 }, last_xrm;
	clock_gettime(CLOCK_MONOTONIC, &last_xrm);

	while (running) {
		dnd_update();
		tray_update();
		dbus_connection_read_write(dbus, 0);
		while (dbus_connection_get_dispatch_status(dbus) == DBUS_DISPATCH_DATA_REMAINS)
			dbus_connection_dispatch(dbus);
		dbus_connection_flush(dbus);

		if (reload) {
			reload = 0;
			theme_reload();
		}
		if (xresources_poll_ms > 0 && ms_since(&last_xrm) >= xresources_poll_ms) {
			clock_gettime(CLOCK_MONOTONIC, &last_xrm);
			check_xrm_change();
		}

		long next_ms = -1;
		for (int i = 0; i < MAX_NOTIFS; i++) {
			if (!notifs[i].active || notifs[i].closing) continue;
			long r = ms_remaining(&notifs[i]);
			if (r == 0) dismiss(i, 1);
			else if (r > 0 && (next_ms < 0 || r < next_ms)) next_ms = r;
		}

		/* SDL delivers pointer input only while cards are shown */
		if (shown) {
			static float lx, ly;
			static SDL_Window *lf;
			float mx, my;
			cui_sdl3_poll(host, ui);
			SDL_MouseButtonFlags b = SDL_GetMouseState(&mx, &my);
			SDL_Window *f = SDL_GetMouseFocus();
			/* pointer motion drives hover/swipe even when CUI has nothing to redraw */
			if (f != lf || (f && (mx != lx || my != ly)) || b) dirty = 1;
			lx = mx; ly = my; lf = f;
		}
		/* repaint on change, on CUI request (hover, press) and once a second
		 * so "2m ago" stays current */
		if (dirty || animating ||
		    (shown && (cui_needs_redraw(ui) || ms_since(&last_paint) >= 1000))) {
			dirty = 0;
			render();
			clock_gettime(CLOCK_MONOTONIC, &last_paint);
		}

		int timeout = next_ms > INT_MAX ? INT_MAX : (int)next_ms;
		if (shown && (timeout < 0 || timeout > 33)) timeout = 33;
		if (animating) { /* ~60 fps; vsync in present keeps frames on the beat */
			long left = 16 - ms_since(&last_frame);
			timeout = left > 0 ? (int)left : 0;
		}
		if (xresources_poll_ms > 0 && (timeout < 0 || timeout > xresources_poll_ms))
			timeout = xresources_poll_ms;

		if (tray_hovered && !tray_tip && (timeout < 0 || timeout > 100)) timeout = 100;
		if (timeout < 0 || timeout > 1000) timeout = 1000;
		struct pollfd pfd[2] = { { .fd = dfd, .events = POLLIN }, { .fd = ConnectionNumber(dpy), .events = POLLIN } };
		if (poll(pfd, 2, dirty ? 0 : timeout) < 0 && errno != EINTR)
			break;
	}

	for (int i = 0; i < MAX_NOTIFS; i++)
		if (notifs[i].active) dismiss(i, 3);

	if (dbus) {
		dbus_connection_flush(dbus);
		dbus_bus_release_name(dbus, BUS_NAME, NULL);
		dbus_connection_unref(dbus);
	}
	host_destroy();
	SDL_Quit();
	free(xrm_last);
	free(font_name);
	free(mono_name);
	tray_tip_hide();
	if (tray_win) XDestroyWindow(dpy, tray_win);
	if (tray_colormap) XFreeColormap(dpy, tray_colormap);
	XCloseDisplay(dpy);
	return 0;
}
