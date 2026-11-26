# Changelog

## v0.3.0

- Configure notifications with `$XDG_CONFIG_HOME/nod/config.ini` or
  `~/.config/nod/config.ini`. The included example documents every default.
- Install or upgrade the static Linux x86_64 and ARM64 release packages
  with the repository installer.

## v0.2.0

- Keep the latest 50 notifications in memory, including messages received
  during Do Not Disturb. Exclude muted apps and transient notifications.
- Expose recent notifications, dismissal, and Do Not Disturb updates to
  `mwm-tray notifications` over the session bus.
- Restore the native tray icon when the mwm applet exits.
- Support `nod-send --transient` to send a notification outside history.

## v0.1.0 — 2026-10-31

First release of nod, a suckless-style X11 notification daemon with native
Desktop Notifications 1.2 D-Bus support.

- GPU-rendered CUI cards with app name, urgency, relative time, summary, body,
  and up to three action buttons.
- Animated folding stacks, hover to expand and hold timeouts, and mouse or
  touchpad swipes to dismiss.
- Focused-monitor placement, configurable screen corner, rounded corners,
  and transparency with a compositor.
- X resources colors and fonts, wallpaper palette fallbacks, contrast
  adjustment, and live theme reload by polling or `SIGUSR1`.
- Do not disturb through an XEmbed tray icon, `nod-dnd`, or a weekly schedule;
  case-insensitive app muting.
- `nod-send` for notification IDs, replacement, urgency, timeouts, actions,
  and waiting for dismissal.
- Compile-time configuration in `config.h`, MIT license, and manual pages
  for all three commands.
- Pinned, checksum-verified CUI download on the first build. Release source
  archives include CUI for building without fetching it again.
- A matching documentation and demo website built with compiled Tailwind CSS
  and deployed through GitHub Pages.

nod requires X11. It does not provide icons, markup, sound, history, or
Wayland support.
