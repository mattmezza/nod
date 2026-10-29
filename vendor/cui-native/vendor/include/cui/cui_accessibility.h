/* SPDX-License-Identifier: MIT */
#ifndef CUI_ACCESSIBILITY_H
#define CUI_ACCESSIBILITY_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Optional native backend. All create/update/drain/destroy calls run on the UI
 * thread. request/token/status may run on other threads. Never race destroy
 * with public calls. The wake callback runs on a foreign thread, and its user
 * must remain alive until destroy returns; destroy disables it synchronously.
 * wake must not reenter the backend or wait for the UI thread. */
typedef struct cui_accessibility cui_accessibility;
typedef enum {
    CUI_A11Y_OK,
    CUI_A11Y_CAPACITY,
    CUI_A11Y_INVALID,
    CUI_A11Y_ALLOCATION,
    CUI_A11Y_CLOSED
} cui_accessibility_status;
typedef struct {
    size_t max_nodes, string_bytes, max_actions; /* zero selects 2048/65536/128 */
    void (*wake)(void *user);                    /* optional; must not call SDL/Core */
    void *user;
} cui_accessibility_config;
typedef struct {
    cui_rect outer, inner;             /* screen coordinates, same units as CUI bounds */
    cui_str title;                     /* copied by update */
    bool focused, has_screen_position; /* Wayland cannot supply global origin */
} cui_accessibility_window;
typedef struct {
    size_t pending;
    bool native_active; /* Native activation observed; false when no a11y bus/client. */
    uint64_t accepted, rejected, overflow, snapshots;
    cui_accessibility_status status;
} cui_accessibility_stats;
cui_accessibility *cui_accessibility_create(cui_accessibility_config);
/* Only after cui_end_frame. Copies all frame-borrowed strings; errors preserve
 * the last snapshot and token generation. No text/value editing actions. */
bool cui_accessibility_update(cui_accessibility *, cui_context *, cui_accessibility_window);
/* Main thread, before begin_frame; returns number queued into Core. Core event
 * backpressure retains remaining requests for a later drain. */
size_t cui_accessibility_drain(cui_accessibility *, cui_context *);
/* Tokens are backend generations, not CUI IDs. Removed/reappeared or changed
 * role targets get a new token. Caller must change CUI ID for same-frame logical
 * replacement with identical role; Core has no stronger identity information. */
uint64_t cui_accessibility_token(cui_accessibility *, cui_id);
bool cui_accessibility_request(cui_accessibility *, uint64_t, cui_accessibility_action);
cui_accessibility_stats cui_accessibility_get_stats(cui_accessibility *);
/* Disables callbacks/wake, drops native adapter, releases public handle now.
 * Private callback storage is reclaimed by its final native handler owner. */
void cui_accessibility_destroy(cui_accessibility *);
#ifdef __cplusplus
}
#endif
#endif
