/* SPDX-License-Identifier: MIT */
#ifndef CUI_DISCLOSURE_H
#define CUI_DISCLOSURE_H
#include "cui.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Arrays and controlled expanded state belong to the caller. IDs must be unique,
 * nonzero and stable across frames, including IDs submitted by panel callbacks. */
typedef struct {
    cui_id id;
    cui_str label;
    bool disabled;
} cui_disclosure_item;
/* Custom title/icon body; trigger semantics and activation remain item-owned. */
typedef void (*cui_disclosure_header_fn)(cui_context *, const cui_disclosure_item *, size_t index,
                                         bool expanded, void *user);
/* Caller-owned, stable per-panel motion/measurement storage. Zero initialize. */
typedef struct {
    cui_id id;
    cui_rect measured;
    float progress, velocity;
    bool initialized;
} cui_disclosure_motion;
/* Reversible reveal; initial state and reduced motion snap.
 * Legacy step uses critical damping; response is natural-period seconds
 * (zero defaults to .2). Styled components use theme.spring_control by default. */
void cui_disclosure_motion_step(cui_disclosure_motion *, bool open, float dt, float response,
                                bool reduced_motion);
/* Explicit theme spring policy; response zero or travel false snaps. */
void cui_disclosure_motion_step_ex(cui_disclosure_motion *, bool open, float dt,
                                   cui_spring_settings, bool reduced_motion);
typedef struct {
    bool multiple, collapsible, disabled, bordered;
    cui_disclosure_header_fn render_header;
    void *header_user;
    const cui_box *trigger_boxes, *panel_boxes; /* optional per-item full box styles */
    cui_disclosure_motion *motion; /* optional per-item array, stable through end_frame */
    size_t motion_count;
    float dt, response;
    bool reduced_motion;
    size_t style_count; /* available entries in either nonnull style array */
} cui_accordion_options;
typedef struct {
    bool disabled, borderless;
    cui_disclosure_header_fn render_header;
    void *header_user;
    const cui_box *trigger_box, *panel_box; /* optional full box styles */
    cui_disclosure_motion *motion;          /* optional stable through end_frame */
    float dt, response;
    bool reduced_motion;
} cui_collapsible_options;
typedef void (*cui_disclosure_panel_fn)(cui_context *, size_t index, void *user);
/* Applied transition observers: see cui_disclosure_events.h. */
/* Base submits semantic triggers without layout or painting. */
bool cui_base_collapsible(cui_context *, cui_id, cui_str, bool *open, bool disabled);
bool cui_base_accordion(cui_context *, const cui_disclosure_item *, size_t count, bool *open,
                        cui_accordion_options);
/* Without motion, styled components render panels only while expanded. Size and
 * colors come from the supplied theme; callbacks can compose arbitrary CUI. */
bool cui_collapsible_ex(cui_context *, cui_id, const cui_theme *, cui_str, bool *open,
                        cui_collapsible_options, cui_disclosure_panel_fn, void *);
bool cui_collapsible(cui_context *, cui_id, const cui_theme *, cui_str, bool *open, bool disabled,
                     cui_disclosure_panel_fn, void *);
bool cui_accordion(cui_context *, cui_id container, const cui_theme *, const cui_disclosure_item *,
                   size_t count, bool *open, cui_accordion_options, cui_disclosure_panel_fn,
                   void *);
#ifdef __cplusplus
}
#endif
#endif
