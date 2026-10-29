/* SPDX-License-Identifier: MIT */
#ifndef CUI_VIDEO_H
#define CUI_VIDEO_H
#include "cui_media.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Decoder-neutral silent timeline. All times are seconds; elapsed time is supplied
 * by the host. This model never decodes, queues frames, or owns resources. */
typedef struct {
    double position, duration, rate;
    uint64_t generation;
    bool duration_known, seekable, playing, ended, loop;
} cui_video;
typedef struct {
    double duration;
    bool duration_known, seekable, loop;
} cui_video_config;
/* Known duration may be zero. Unknown duration requires duration=0. */
bool cui_video_init(cui_video *, cui_video_config);
bool cui_video_set_playing(cui_video *, bool);
bool cui_video_set_rate(cui_video *, double rate);
bool cui_video_set_loop(cui_video *, bool);
/* Seek requires seekable=true; known duration bounds the target. A successful
 * seek increments generation even if position is unchanged. All failures leave
 * the model unchanged. UINT64_MAX generation rejects seek/restart/wrap. */
bool cui_video_seek(cui_video *, double position);
bool cui_video_advance(cui_video *, double elapsed_seconds);
/* Host end-of-stream acknowledgement. Unknown duration remains unknown. */
bool cui_video_finish(cui_video *);
typedef struct {
    cui_bitmap bitmap;
    uint64_t generation;
    double timestamp;
} cui_video_frame;
typedef struct {
    cui_image_geometry geometry;
    float opacity;
    cui_str description;
} cui_video_surface_options;
/* Submit one already-decoded straight-alpha RGBA8 frame, borrowing its pixels.
 * The host schedules frames and retains pixels unchanged through ALL renderer
 * consumption, not merely end_frame. Seek/restart/loop invalidate old generations.
 * Timestamp is validated but does not schedule or drop frames automatically.
 * Same pixels changed in place require bitmap.revision increment. */
bool cui_video_surface(cui_context *, cui_id, const cui_video *, const cui_video_frame *,
                       cui_video_surface_options);
#ifdef __cplusplus
}
#endif
#endif
