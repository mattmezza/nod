/* SPDX-License-Identifier: MIT */
#ifndef CUI_OVERLAY_H
#define CUI_OVERLAY_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Application-owned persistent overlay state. Bounds describe the preceding
 * frame's panel in logical pixels; scopes themselves do not paint or lay out. */
typedef struct {
    cui_id id, trigger;
    cui_rect bounds;
    bool open, modal, dismiss_on_escape, dismiss_on_outside, has_bounds;
} cui_overlay;
void cui_overlay_init(cui_overlay *, cui_id id, bool modal);
void cui_overlay_open(cui_context *, cui_overlay *, cui_id trigger);
void cui_overlay_close(cui_context *, cui_overlay *);
void cui_overlay_set_bounds(cui_overlay *, cui_rect);
/* Call after begin_frame, innermost overlay first, before submitting controls.
 * Consumed events are ignored. Returns true when an input event closes this overlay. */
bool cui_overlay_update(cui_context *, cui_overlay *);
/* Pair only when begin returns true. Layout/paint remain application-owned. */
bool cui_overlay_begin(cui_context *, const cui_overlay *);
void cui_overlay_end(cui_context *);
#ifdef __cplusplus
}
#endif
#endif
