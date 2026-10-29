/* SPDX-License-Identifier: MIT */
#ifndef CUI_SDL3_H
#define CUI_SDL3_H
#include "cui_accessibility.h"
#include "cui_platform.h"
#include "cui_renderer.h"
#include "cui_text.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct cui_sdl3 cui_sdl3;
typedef struct {
    size_t entries, coverage_bytes;
    uint64_t hits, misses;
} cui_sdl3_cache_stats;
/* Optional SDL3 adapter; all operations must run on the main thread. */
cui_sdl3 *cui_sdl3_create(const char *title, int width, int height, cui_font *font);
void cui_sdl3_destroy(cui_sdl3 *backend);
/* Main-thread copied system-theme snapshot. Initial query revision is 1; poll
 * observes SDL notifications and invalidates Core. Revision saturates at UINT64_MAX.
 * SYSTEM_THEME means query/notification API support, not a known OS answer.
 * No reduced-motion/high-contrast detection. Null arguments leave output intact. */
bool cui_sdl3_appearance(cui_sdl3 *backend, cui_platform_appearance *out);
bool cui_sdl3_poll(cui_sdl3 *backend, cui_context *ui);
/* Callback-main host for live native resize. Call once from main; init creates
 * backend/Core and returns them through out parameters. frame composes/presents
 * one frame (0 continue, positive success, negative failure). quit releases app
 * resources, including backend/Core. All three callbacks run on the main thread.
 * Do not pump/wait for events or nest run inside a callback. SDL owns scheduling.
 * Native modal resize can call frame while the OS event pump is suspended. */
typedef struct {
    void *user;
    bool (*init)(void *, int, char **, cui_sdl3 **, cui_context **);
    int (*frame)(void *, cui_sdl3 *, cui_context *);
    void (*quit)(void *, bool success);
} cui_sdl3_app;
int cui_sdl3_run(int argc, char **argv, cui_sdl3_app app);

/* Apply after end_frame for immediate feedback. poll also applies the previous
 * frame hint. Returns false if the host driver does not support system cursors. */
bool cui_sdl3_set_cursor(cui_sdl3 *backend, cui_cursor cursor);
void cui_sdl3_size(cui_sdl3 *backend, float *width, float *height);
/* Optional native-a11y metadata only; no Rust/backend linkage required.
 * Main thread. title is borrowed until host title change/destruction; update
 * copies it. Only X11 supplies screen position; Wayland reports false. */
bool cui_sdl3_accessibility_window(cui_sdl3 *, cui_accessibility_window *out);
bool cui_sdl3_render(cui_sdl3 *backend, const cui_draw_list *list, cui_color clear);
/* Capture before presenting, when the framebuffer contents are defined. */
bool cui_sdl3_render_capture(cui_sdl3 *backend, const cui_draw_list *list, cui_color clear,
                             const char *path);
bool cui_sdl3_save_bmp(cui_sdl3 *backend, const char *path);
const char *cui_sdl3_driver(cui_sdl3 *backend);
const char *cui_sdl3_video_driver(void);
const char *cui_sdl3_error(void);
cui_sdl3_cache_stats cui_sdl3_glyph_cache_stats(cui_sdl3 *backend);
/* Clipboard-only services. get() storage lasts until next get() or destruction.
 * Native input dispatch remains cui_sdl3_poll(); no IME composition capability.
 */
cui_platform cui_sdl3_clipboard_interface(cui_sdl3 *backend);
/* Window actions are deferred by the OS; close requests stop the next poll.
 * Native move/system-menu dispatch is not available through this interface. */
cui_window_host cui_sdl3_window_interface(cui_sdl3 *backend);
bool cui_sdl3_set_decorated(cui_sdl3 *backend, bool decorated);
/* Seconds since the preceding call for this backend, including presentation.
 * Idle event waits reset this clock so newly triggered animations do not advance
 * by the preceding idle period. First call returns zero. Call once immediately
 * before begin_frame. */
float cui_sdl3_frame_delta(cui_sdl3 *backend);
/* Wait for input, retaining the wakeup event for poll afterwards. timeout_ms
 * is a caller deadline: zero polls, negative waits indefinitely. Returns true
 * when an event is available. Only wait when Core has no redraw demand; external
 * model/timer changes must invalidate Core. This does not synchronize IME state:
 * poll once after rendering before waiting so the latest caret/focus is applied. */
bool cui_sdl3_wait_events(cui_sdl3 *backend, int timeout_ms);
/* Unconditional delay; prefer wait_events for idle UI and presentation sync for
 * active frames. */
void cui_sdl3_wait(unsigned milliseconds);
cui_renderer cui_sdl3_interface(cui_sdl3 *backend);
#ifdef __cplusplus
}
#endif
#endif
