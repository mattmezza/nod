# nod

Suckless-style notification daemon for X11. Implements the
[Desktop Notifications Specification 1.2][spec] over D-Bus.

Think "herbe with native D-Bus support" — no wrapper scripts, works out
of the box with `notify-send` and any app that emits standard
notifications.

[spec]: https://specifications.freedesktop.org/notification-spec/notification-spec-latest.html

[Website and interactive demo](https://matteo.merola.co/nod/) ·
[Documentation](https://matteo.merola.co/nod/docs.html) ·
[Releases](https://github.com/mattmezza/nod/releases)

## Features

- Single `nod.c`, single `config.h`, no runtime config
- Native D-Bus — no `herbe` wrapper, no FIFOs
- Cards drawn with [CUI](https://github.com/mattmezza/cui) (downloaded at build time), GPU
  accelerated through SDL3's renderer
- Header (app, urgency, relative time), summary, body, action buttons — each part toggleable
- Folding stack that unfolds on hover, animated entrance/exit, swipe to dismiss
- Multi-monitor aware (follows focused window via XRandR)
- Stacked notifications, configurable corner, rounded corners (X Shape)
- Theming from X resources, falling back to `*.background`, `*.foreground`
  and `*.colorN` — wallpaper palettes (pywal, wallust, …) just work
- Runtime theme reload (polling or `SIGUSR1`)
- Scheduled do not disturb, app muting, X11 tray toggle and `nod-dnd` CLI

## Requirements

- libX11, libXrandr, libXrender, libXext, fontconfig
- SDL3 >= 3.2, FreeType, HarfBuzz (for CUI)
- libdbus-1
- a running X session

On Arch: `pacman -S libx11 libxrandr libxrender libxext fontconfig sdl3 freetype2 harfbuzz dbus`

## Build

```sh
make
sudo make install
```

The first `make` downloads and verifies the pinned CUI source archive, generates
its native bundle, and compiles it into `build/libcui.a`. This bootstrap needs
`curl`, `sha256sum`, `tar`, and Python 3. Later builds use the local copy and
only rebuild changed sources. Edit `config.h` (copied from `config.def.h`) and
run `make` to reconfigure.

## CUI dependency

`vendor/cui-native/` is generated locally and ignored by Git. `cui.lock.mk`
pins the upstream commit and archive SHA-256. No globally installed CUI library
is needed. Run `make fetch-cui` to download it ahead of time; subsequent builds
work offline. `make distclean` keeps the downloaded dependency.

To update CUI, change both pins in `cui.lock.mk`, then run `make distclean`
and `make`. The downloader verifies the archive before running upstream's
Python bundle generator. It retains the source bundle's license and dependency
notices, removing only the unused starter app and fonts (nod uses fontconfig).

Release source archives include the pinned native bundle, so building a release
tarball needs neither network access nor Python.

## Release

Set `VERSION` in `config.mk`, commit all changes, then run:

```sh
make release name=v0.1.0
```

The target requires authenticated GitHub CLI (`gh`) and push access to `origin`.
It checks the version and clean checkout, builds a fresh source extraction,
checks its version, runs the isolated Xvfb/D-Bus smoke test, and creates
`nod-0.1.0.tar.gz` with a SHA-256 sidecar.
It then creates an annotated tag, pushes the commit and tag, and publishes a
GitHub release containing both files. Release validation also needs `xvfb-run`,
`xauth`, `dbus-run-session`, `gdbus`, `dbus-send`, and `xrdb`. Your local
`config.h` is preserved. System libraries remain prerequisites;
the release is a source distribution, not a standalone binary.

## Theming via X resources

With `use_xresources = 1` (default) nod reads, per urgency
(`low`, `normal`, `critical`):

| what | resource | falls back to |
| --- | --- | --- |
| card background | `nod.background.<u>` | `nod.background`, `*.background` |
| text | `nod.foreground.<u>` | `nod.foreground`, `*.foreground` |
| border, dot, buttons | `nod.accent.<u>` | `nod.border.<u>`, `*.color8` / `*.color4` / `*.color1` |
| secondary text | `nod.muted` | `*.color8` |
| fonts | `nod.font`, `nod.monoFont` | `config.h` |

Font sizes in points are resolved at `Xft.dpi` (96 when unset), like Xft.
Accent, text and muted colours are nudged towards the foreground until they
reach `min_contrast` against the card, so arbitrary palettes stay readable;
critical cards are tinted with their accent (`critical_tint`).

With a compositor (picom, …) nod uses a 32-bit window: real transparency and
antialiased corners. Without one it falls back to a Shape mask. Exclude
nod from compositor blur/rounding, e.g. for picom:

```
blur-background-exclude = [ "class_g = 'nod'" ];
rounded-corners-exclude = [ "class_g = 'nod'" ];
```

So a wallpaper-generated palette works with no nod entries at all. Override
only what you want:

```
nod.font:            Noto Sans:size=11
nod.monoFont:        JetBrains Mono:size=10
nod.accent.critical: #cc241d
```

nod polls the resource database every `xresources_poll_ms` (default 2000)
and repaints when it changes; `xresources_poll_ms = 0` disables polling,
then use `killall -USR1 nod`.

## Run

```sh
# Start from ~/.xinitrc or equivalent, before your WM:
nod &
exec dwm
```

Only one notification daemon can own the name
`org.freedesktop.Notifications` at a time — make sure `dunst`, `mako`,
or any DE daemon is not running.

## nod-send

A `notify-send`-compatible CLI wrapper shipped alongside nod. Adds
support for the `-A/--action` flag (upstream `notify-send` already has
this, but `libnotify`-less systems may not). Installed to
`$(PREFIX)/bin/nod-send`. Depends on `gdbus` (from `glib2`).

```sh
# basic — identical to notify-send
nod-send "Hello" "This is a test"
nod-send -u critical -t 3000 "Critical" "Server down"

# with a default action — blocks until dismissed, prints "default" if clicked
key=$(nod-send -A "Open" "New mail" "Click to open inbox")
[ "$key" = "default" ] && xdg-open https://mail.example.com

# named actions
nod-send -A open="Open" -A snooze="Snooze" "Reminder" "Meeting in 5min"
```

The wait is bounded: `expire-time + 3s` slack for finite timeouts,
unbounded for persistent (`-t 0`).

## Test

```sh
notify-send "Hello" "This is a test"
notify-send -u low "Low" "Low urgency"
notify-send -u critical "Critical" "Critical urgency"
notify-send -t 3000 "Quick" "3 second timeout"

# Replace an existing notification
notify-send -r 1 "Updated" "Replaced notification"

# Notification with a default action
gdbus call --session \
    --dest org.freedesktop.Notifications \
    --object-path /org/freedesktop/Notifications \
    --method org.freedesktop.Notifications.Notify \
    "test" 0 "" "Click me" "Has default action" \
    '["default", "Open"]' '{}' -1
```

There is also an isolated Xephyr test for X resources reload:

```sh
./tests/test_xresources.sh
```

## Interaction

- **Stack** — several notifications fold into a deck; hover to unfold it
- **Action buttons** — one per action (max 3); emits `ActionInvoked`, then dismisses.
  With `action_buttons = 0` a left click on the card invokes the default action
- **Dismiss** button, **right-click**, or **swipe** (drag or two-finger
  touchpad swipe towards the screen edge) — dismiss only
- **Hover** — holds every timeout while the pointer is over the stack
- **Timeout** — auto-dismiss

Card parts (app name, urgency, time, dividers, buttons, border), stacking and
motion are toggled in `config.h`.

## What nod does NOT do

Icons. Markup. Sound. History. Runtime config. CLI flags beyond `-v`.
Focus stealing. Wayland.

If you want those, use `dunst`.

## Screenshot

![unfolded stack](screenshot.png)

Folded (default when more than one notification is shown):

![folded stack](screenshot-stack.png)

## License

MIT — see `LICENSE`.

## Website

The static marketing and documentation site is in `web/`. Build it with
Node.js 24+ and npm:

```sh
make site
python3 -m http.server 4173 -d web/dist
```

Tailwind CSS is compiled locally; the published site has no runtime CSS CDN
dependency. `.github/workflows/pages.yml` builds and deploys `web/dist` to
GitHub Pages on changes to the site on `main`. Pull requests build without
deploying. See `web/README.md` for editing and previewing the site.

### Do not disturb

Click nod's circle icon in an XEmbed system tray to toggle do not disturb.
A slash means DND is on. The icon appears when an X11 system tray is available.
From a terminal or script, run `nod-dnd` (toggle), `nod-dnd on`,
`nod-dnd off`, or `nod-dnd status`; each prints `on` or `off`. The command
requires `dbus-send` and a running nod daemon.

Edit `dnd_schedule` in `config.h` (or `config.def.h` before generating
`config.h`) and rebuild. Rows run Sunday through Saturday and contain
`{ start_hour, start_minute, end_hour, end_minute }` in local time. A start
hour of -1 disables the row; an end before the start crosses midnight. Equal
times disable the row. Manual changes last until the scheduled state next
changes or nod restarts. When the schedule changes the DND state, nod shows a
notification with a button to reverse it and updates the tray icon. For example, `{ 22, 0, 7, 30 }` on Monday mutes
from Monday 22:00 until Tuesday 07:30.

Add exact notification app names to `dnd_muted_apps` to mute them at all
times, independently of DND. Matching ignores case. DND and app muting discard
new notifications, including critical ones; already displayed cards remain.
