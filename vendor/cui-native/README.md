# CUI native source bundle

From the extracted bundle directory:

```sh
make
./build/native_starter
```

The consumer build needs a C11 compiler (`cc`), GNU make, `ar`, and `pkg-config`
with SDL3 >= 3.2, FreeType, and HarfBuzz development packages. It needs no Python,
CMake, Rust, network access, or installed CUI library. Python and CMake belong to
the repository's producer and validation workflow.

Check system prerequisites separately from bundle completeness:

```sh
pkg-config --atleast-version=3.2 sdl3
pkg-config --modversion sdl3 freetype2 harfbuzz
```

SDL2 cannot replace SDL3. Install development packages through your system's
package manager or explicitly build upstream releases before running `make`.
The build performs no downloads. These are dynamically linked system libraries;
shipping this source bundle is separate from deploying a binary and its libraries.
Running the app requires an X11 display session. Automated Linux validation needs
Xvfb (`xvfb-run`) and `xauth`, in addition to the build prerequisites.

Each CUI source compiles separately into `build/vendor/`, then into static CUI
archives. Editing `app.c` rebuilds `build/app.o` and relinks the executable;
ordinary app edits leave CUI objects alone. `.d` files track header dependencies.
Use `make CC=clang` to select another compiler in a fresh extraction. If changing
compiler or flags after building, use a fresh extraction or remove your build
directory explicitly. `sources.mk` is generated from the repository's authoritative
source metadata. No implementation macros are required in consumer code.

The starter loads bundled Inter 4.1 Regular and Bold before creating its layout.
Unmodified official static OTF files and their SIL Open Font License are in
`assets/fonts/Inter-Regular.otf`, `assets/fonts/Inter-Bold.otf`, and
`assets/fonts/LICENSE.txt`. Keep the license when redistributing the fonts.
Defaults resolve relative to the executable (`build/../assets/fonts`), so launching
from another working directory works. Keep that directory relationship when moving
the app. Override each face explicitly if needed:

```sh
./build/native_starter --font /path/regular.otf --bold-font /path/bold.otf
./build/native_starter --frames 3 --capture frame.bmp
```

Only regular (400) and bold (700) are bundled; CUI does not synthesize other
weights or italics. Register additional matching faces before using other styles;
an unavailable style falls back to regular. Inter's script coverage is finite:
this pair supplies neither
universal Unicode coverage nor automatic font fallback. The archive retains
font provenance and SHA-256 pins in `assets/fonts/manifest.json`, upstream
dependency records in `upstream-dependencies.json`, and checksums for every payload
file in `bundle-manifest.json` (excluding that checksum manifest itself).

`--frames` accepts positive decimal integers through `UINT_MAX`; only successful
renders count. A bounded run fails if the window closes early, layout/rendering
fails, a font is missing, or capture fails. `--capture` requires `--frames` and
captures the final frame before presentation. Capture paths are relative to the
working directory. Without `--frames`, close the window to exit. The button toggles
a greeting and demonstrates ordinary pointer/keyboard component interaction.

Linux X11 is the supported validated native target. Other platforms and display
servers are unverified. Native automated tests must use the repository's invisible
Xvfb wrapper (`python scripts/test_local.py --build-dir build`), never the desktop
display. Browser tests run headless; virtual-display results do not establish GPU
hardware coverage.

The default is the existing SDL3 `SDL_Renderer` adapter. SDL chooses an available
driver; distribution tests force its software driver under Xvfb. This does not
require Sokol, Vulkan, or CUI's separate SDL_GPU adapter.

The dependency map is:

| Layer | Dependencies and purpose |
| --- | --- |
| Portable CUI | Clay layout, NanoSVG SVG rasterization, generated icon/theme tables, generated Unicode properties |
| SDL3 adapter | SDL3 host window, input, clipboard, cursors, and renderer |
| Native text adapter | FreeType font loading/rasterization and HarfBuzz shaping |
| Bidirectional text | Vendored SheenBidi, compiled as C with the bundle |
| Starter assets | Inter 4.1 static Regular/Bold OTF files, SIL Open Font License |

Sokol, SDL_GPU, the Rust regex provider, and native accessibility integrations
are outside the default build. Public headers may declare optional interfaces;
their presence does not mean those optional implementations are linked. All 78
component manifest entries are retained in `component-manifest.json`, including
their incomplete behavior and validation status.

Application layout and component calls use public CUI APIs. The starter uses
SDL's executable-path helper only to locate its bundled assets. Public CUI
interfaces contain no SDL or Clay types. This bundle demonstrates a minimal
native integration; it does not claim complete platform or component parity.

## Two application files

`main.c` owns fonts, the SDL3 host, event polling, frame boundaries, rendering and
cleanup. `app.c` contains `app_frame` and only uses portable public CUI APIs.
The host owns the `clicked` value across frames and passes its address into the
application submission function. Edit the labels, root box and button in `app.c`
then run `make` again. The button toggles the greeting; keyboard Tab and Enter
exercise the same component interaction. No implementation macro is needed.

To rename the executable or add ordinary C source files:

```sh
make APP_SOURCES="main.c app.c" TARGET=build/my_app
./build/my_app
```

`APP_SOURCES` is a space-separated list of relative `.c` paths (subdirectories
are supported; filenames with spaces are unsupported). Include the host and all
application translation units, but leave vendor sources in generated
`sources.mk`. `TARGET` names the linked executable. Keep it directly in `build/`
to preserve the default executable-relative font paths; elsewhere, pass explicit
`--font` and `--bold-font` paths. A new source set or target links from the
selected objects; unchanged vendor objects are reused. Changing compiler or
flags requires a fresh build directory. External dependency requirements remain
unchanged.

## Mail layout acceptance fixture

```sh
make APP_SOURCES="main.c mail.c" TARGET=build/mail_demo
./build/mail_demo
```

`mail.c` exercises three independently scrollable panes, repeated scoped avatars
and tags, long clipped subjects, fixed trailing star actions, message selection
and a Compose/Discard preview path. The 720×420 host is a desktop fixture;
narrower windows clip panes rather than provide responsive navigation. Compose
has no editor, storage or delivery. State is process-local. This fixture does not
claim a complete mail client or complete component parity.

For geometry inspection use `--debug-layout text` or `--debug-layout json`.
The first completed frame is exported once to stdout; JSON can be redirected to
an application-owned file. Capture remains independent. Core/component reported
failures go to stderr with subsystem, operation, bounded message, ID, frame and
budget counters; font/SDL initialization and renderer failures retain their own
host messages. The exporter covers box nodes rather than individual text nodes.
