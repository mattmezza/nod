/* nod - default configuration. copy to config.h and edit. */

/* Position values */
enum { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };

/* Fonts: fontconfig patterns. The size of `font` sets the base text size (points at
 * Xft.dpi, 96 if unset; use pixelsize= for pixels);
 * `mono_font` is used for the header and timestamps. */
static const char *font         = "sans-serif:size=11";
static const char *mono_font    = "monospace:size=10";

/* Geometry (pixels) */
static const int   width        = 500;  /* card width */
static const int   max_lines    = 5;    /* body is clipped after this many lines */
static const int   padding      = 14;
static const int   border_width = 1;
static const int   border_radius = 4;   /* 0 = square */
static const int   gap_x        = 13;   /* from screen edge */
static const int   gap_y        = 38;   /* from screen edge */
static const int   stack_gap    = 10;   /* between stacked cards */

/* Position: TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT */
static const int   position     = TOP_RIGHT;

/* Maximum visible notifications at once */
static const int   max_visible  = 5;

/* Card parts */
static const int   show_app       = 1;  /* header: app name */
static const int   show_urgency   = 1;  /* header: low / normal / critical */
static const int   show_time      = 1;  /* header: "2m ago" */
static const int   divider_top    = 0;  /* line under the header */
static const int   divider_bottom = 0;  /* line above the buttons */
static const int   action_buttons = 1;  /* 0: left click invokes the default action */
static const int   dismiss_button = 1;  /* 0: right click / swipe to dismiss */
static const int   show_border    = 1;

/* Stack: collapse into a deck that unfolds on hover (0 = always a list) */
static const int   collapse     = 1;
static const int   peek         = 8;    /* px each card behind peeks out (and is inset) */
static const int   max_peek     = 3;    /* cards visible behind the front one */

/* Motion: higher is snappier. Swipe a card this fraction of its width
 * (mouse drag or two-finger touchpad swipe) to dismiss it. */
static const float anim_speed      = 14;
static const float swipe_threshold = 0.3;

/* Colors per urgency: low, normal, critical.
 * Any of these is overridden by X resources when use_xresources = 1. */
static const char *bg_colors[]     = { "#0d1117", "#0d1117", "#0d1117" };
static const char *fg_colors[]     = { "#c9d1d9", "#e6edf3", "#e6edf3" };
static const char *accent_colors[] = { "#6e7681", "#79c0ff", "#ffa657" };
static const char *muted_color     = "#8b949e";

/* Palette safety net: accents and secondary text are nudged towards the
 * foreground until they reach this WCAG contrast ratio against the card. */
static const float min_contrast    = 4.5;
/* How much of the accent colour tints critical cards (0 = none) */
static const float critical_tint   = 0.14;

/* Read colors and fonts from X resources. Lookup order per urgency u:
 *
 *   nod.background.u  -> nod.background  (matches *.background / *background)
 *   nod.foreground.u  -> nod.foreground  (matches *.foreground)
 *   nod.accent.u      -> nod.color8 / nod.color4 / nod.color1 (low/normal/critical)
 *   nod.muted         -> nod.color8
 *   nod.font, nod.monoFont
 *
 * So a plain pywal/wallust style Xresources (*.background, *.color0..15)
 * themes nod without any nod-specific entries. nod.border.u is accepted as
 * an alias of nod.accent.u. */
static const int use_xresources = 1;

/* Poll interval (ms) for detecting X resource changes at runtime.
 * 0 disables polling; send SIGUSR1 to force a reload. */
static const int xresources_poll_ms = 2000;

/* Timeouts in seconds per urgency: low, normal, critical.
 * 0 = do not auto-dismiss. The timer restarts while the pointer is over a card. */
static const int timeouts[] = { 5, 10, 0 };

/* DND schedule in local time, Sunday through Saturday. Each row is
 * {start hour, start minute, end hour, end minute}; -1 disables a day.
 * Overnight intervals end on the following day. Equal times are disabled.
 * Example: {19, 0, 6, 0} enables DND from 7pm until 6am the next day. */
static const int dnd_schedule[7][4] = {
    {-1, 0, 0, 0}, /* Sunday */
    {-1, 0, 0, 0}, /* Monday */
    {-1, 0, 0, 0}, /* Tuesday */
    {-1, 0, 0, 0}, /* Wednesday */
    {-1, 0, 0, 0}, /* Thursday */
    {-1, 0, 0, 0}, /* Friday */
    {-1, 0, 0, 0}, /* Saturday */
};
/* Exact, case-insensitive notification app names muted at all times. */
static const char *dnd_muted_apps[] = { NULL /* e.g. "Example App" */ };
