NanoSVG parser and rasterizer pinned upstream commit
239e102ec2c691f2902e20ace2ed36ee4a35cfe6 (no dependency version change).
Source: https://github.com/memononen/nanosvg
Zlib license; original upstream nanosvg.h SHA256:
e34fd5d084be106cea972d19ce5d27fd96d17ba89f8d06bdceee058420c8b2b0.

CUI local patch, 2026-10-15, `svg-transform-validation-v1`:
- Malformed transform arities return identity before reading missing arguments.
  Matrix also initializes its result on rejection, preventing uninitialized or
  stale temporary matrices from reaching the transform-list composition.
- Transform numeric arguments overflowing to nonfinite float are rejected before
  matrix construction. Valid argument counts and finite transform order remain
  unchanged; this does not introduce a complete strict SVG grammar validator.
- Cast the ftell error sentinel explicitly to size_t, matching its existing
  converted representation and eliminating a strict signed-comparison warning
  without changing file-loading behavior.
Patched nanosvg.h SHA256:
99230b1f967ea52e1b002fea5c8882118501125cd6aa6e4c66b44eb97e2d6dc7.
Rasterizer and license remain byte-identical upstream; source/license hashes are
tracked by ../dependencies.json (integrator refreshes patched parser hash).
Validation and exact commands: ../../docs/SVG_TRANSFORM_VALIDATION.md.

The portable C icon adapter uses NanoSVG's supported SVG path/shape subset.
This is not a complete replacement for GPUI's SVG engine: filters, text and
advanced document features require a host renderer or subsequent implementation.
