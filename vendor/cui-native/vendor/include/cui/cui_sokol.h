/* SPDX-License-Identifier: MIT */
#ifndef CUI_SOKOL_H
#define CUI_SOKOL_H
#include "cui_renderer.h"
#include "cui_text.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct cui_sokol cui_sokol;
/* Borrowed GLCORE framebuffer: RGBA8, single sample, no depth attachment use.
 * width/height are physical pixels; draw-list dimensions are logical pixels. */
typedef struct {
    uint32_t framebuffer;
    unsigned width, height;
} cui_sokol_target;
typedef struct {
    cui_font *font;
    size_t max_quads;
    cui_sokol_target target;
    bool borrow_runtime;
} cui_sokol_config;
typedef struct {
    size_t entries, coverage_bytes, quads;
    uint64_t hits, misses;
} cui_sokol_stats;
/* A GL 3.3 core context must already be current on this thread. Neither its
 * window, context nor framebuffer is owned. By default this adapter initializes
 * Sokol and commits each frame. With borrow_runtime, host-owned Sokol GLCORE
 * must already be initialized; the host commits after render and outlives us.
 */
cui_sokol *cui_sokol_create(cui_sokol_config config);
void cui_sokol_destroy(cui_sokol *renderer);
bool cui_sokol_set_target(cui_sokol *renderer, cui_sokol_target target);
bool cui_sokol_render(cui_sokol *renderer, const cui_draw_list *list, cui_color clear);
cui_renderer cui_sokol_interface(cui_sokol *renderer);
cui_sokol_stats cui_sokol_get_stats(const cui_sokol *renderer);
const char *cui_sokol_error(const cui_sokol *renderer);
#ifdef __cplusplus
}
#endif
#endif
