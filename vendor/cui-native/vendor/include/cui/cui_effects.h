/* SPDX-License-Identifier: MIT */
#ifndef CUI_EFFECTS_H
#define CUI_EFFECTS_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_rect bounds;        /* Logical placement, including pixel-grid covering. */
    cui_rect covered_layer; /* Physical, integer covered UNBLURRED source bounds. */
    unsigned width, height;
    size_t bytes;
} cui_drop_shadow_raster;
/* Uniform physical transform: device = logical*scale + origin. Query validates
 * all input before publishing. Physical covering, after materializing logical bounds, determines
 * empty paint. Scale [1/65536,64]; positive physical sigma below 4*sqrt(FLT_MIN) is rejected
 * atomically (source f32 division would underflow). Max 16384 per side, 16M pixels. Raster storage
 * is caller owned. */
bool cui_effect_drop_shadow_query(cui_drop_shadow_paint, float scale, float origin_x,
                                  float origin_y, cui_drop_shadow_raster *out);
/* Caller-owned writable straight-alpha RGBA8 storage; size includes padding. */
typedef struct {
    unsigned char *pixels;
    unsigned width, height;
    size_t stride, size;
} cui_effect_buffer;
bool cui_effect_drop_shadow_raster(cui_drop_shadow_paint, float scale, float origin_x,
                                   float origin_y, cui_effect_buffer output);
/* Same checked transform/finite domain and caller-owned buffer contract as DROP.
 * Empty physical hole/element yields an empty raster. Consumers cull against
 * covered_hole but paint/crop covered_element; pure calls have no ancestor mask.
 * Invalid query/sample/raster calls preserve output. */
typedef struct {
    cui_rect bounds;                        /* Logical placement of covered-element raster. */
    cui_rect covered_hole, covered_element; /* Physical integer geometry. */
    unsigned width, height;
    size_t bytes;
} cui_inset_shadow_raster;
bool cui_effect_inset_shadow_query(cui_inset_shadow_paint, float scale, float origin_x,
                                   float origin_y, cui_inset_shadow_raster *out);
bool cui_effect_inset_shadow_sample(cui_inset_shadow_paint, float scale, float origin_x,
                                    float origin_y, float x, float y, float *out);
bool cui_effect_inset_shadow_raster(cui_inset_shadow_paint, float scale, float origin_x,
                                    float origin_y, cui_effect_buffer output);
/* Checked point sample in physical pixels, source f32 four-midpoint kernel. */
bool cui_effect_drop_shadow_sample(cui_drop_shadow_paint, float scale, float origin_x,
                                   float origin_y, float x, float y, float *out);
typedef struct {
    float position; /* Strictly increasing, in [0,1]. */
    cui_color color;
} cui_gradient_stop;
/* Angle in degrees: 0 runs left to right, 90 top to bottom. Endpoints span
 * the projected pixel-center bounds. Single-pixel images sample position 0.
 * Interpolates premultiplied sRGB channels, then stores straight alpha.
 * Stops may omit 0/1: outer colors extend to the endpoints. */
bool cui_effect_linear_gradient(cui_effect_buffer output, const cui_gradient_stop *stops,
                                size_t count, float angle_degrees);
/* Preserves straight RGB; scales alpha. Source/destination may be identical,
 * otherwise their memory ranges must not overlap. Finite opacity in [0,1]. */
bool cui_effect_opacity(cui_bitmap source, cui_effect_buffer output, float opacity);
/* Separable box blur, radius in pixels, replicated edge samples. Processes
 * premultiplied RGBA internally to prevent transparent color halos. Scratch
 * requires width*height*4 floats and must overlap neither image. Source and
 * destination may be identical. No allocation. Invalid calls leave output
 * untouched. Radius 0 copies the source. */
size_t cui_effect_blur_scratch_floats(unsigned width, unsigned height);
bool cui_effect_blur(cui_bitmap source, cui_effect_buffer output, unsigned radius, float *scratch,
                     size_t scratch_floats);
#ifdef __cplusplus
}
#endif
#endif
