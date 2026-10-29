#ifndef CUI_PLATFORM_H
#define CUI_PLATFORM_H
#include "cui.h"
enum { CUI_PLATFORM_CLIPBOARD = 1, CUI_PLATFORM_IME = 2, CUI_PLATFORM_ACCESSIBILITY = 4 };
typedef struct {
    void *user;
    unsigned capabilities;
    bool (*poll)(void *user, cui_event *event);
    bool (*clipboard_set)(void *user, cui_str text);
    cui_str (*clipboard_get)(void *user);
    void (*text_input)(void *user, bool enabled, cui_rect bounds);
} cui_platform;
/* Host appearance observation, independent of optional Theme policies.
 * UNKNOWN is a valid result when the video driver cannot discover appearance. */
typedef enum {
    CUI_SYSTEM_APPEARANCE_UNKNOWN,
    CUI_SYSTEM_APPEARANCE_LIGHT,
    CUI_SYSTEM_APPEARANCE_DARK
} cui_system_appearance;
enum { CUI_APPEARANCE_SYSTEM_THEME = 1 };
typedef struct {
    cui_system_appearance appearance;
    unsigned capabilities;
    uint64_t revision;
} cui_platform_appearance;
/* Portable window services; adapters expose only actions they can honor. */
typedef enum {
    CUI_WINDOW_MOVE,
    CUI_WINDOW_MINIMIZE,
    CUI_WINDOW_MAXIMIZE,
    CUI_WINDOW_RESTORE,
    CUI_WINDOW_CLOSE,
    CUI_WINDOW_MENU,
    CUI_WINDOW_TITLEBAR_DOUBLE_CLICK
} cui_window_action;
typedef enum {
    CUI_WINDOW_BROWSER,
    CUI_WINDOW_SERVER_DECORATED,
    CUI_WINDOW_CLIENT_DECORATED,
    CUI_WINDOW_NATIVE_TRAFFIC_LIGHTS
} cui_window_decoration;
typedef struct {
    cui_window_decoration decoration;
    unsigned actions; /* Bit mask: 1u << cui_window_action. */
    bool maximized, fullscreen;
} cui_window_info;
typedef struct {
    void *user;
    cui_window_info (*query)(void *user);
    bool (*perform)(void *user, cui_window_action, float x, float y);
} cui_window_host;
#endif
