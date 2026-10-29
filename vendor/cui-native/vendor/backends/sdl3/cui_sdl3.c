/* SPDX-License-Identifier: MIT */
#include "cui/cui_sdl3.h"
#include "../../src/core/cui_text_paint_internal.h"
#include "../software/cui_bitmap.h"
#include "../software/cui_shadow.h"
#include "cui_sdl3_mesh.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define GLYPH_CACHE_SLOTS 256
#define GLYPH_CACHE_BYTES (4u * 1024u * 1024u)
typedef struct {
    SDL_Texture *texture;
    unsigned char *coverage;
    int w, h;
    uint64_t hash, used;
} glyph_entry;
#define BITMAP_SLOTS 64
#define BITMAP_BYTES (64u * 1024u * 1024u)
typedef struct {
    SDL_Texture *texture;
    cui_bitmap_key key;
    bool shadow;
    uint64_t used, frame;
} bitmap_entry;
struct cui_sdl3 {
    cui_shadow_cache shadows;
    bool shadow_frame;
    cui_platform_appearance appearance;
    SDL_Window *window;
    SDL_Renderer *renderer;
    cui_font *font;
    cui_color ink;
    cui_text_paint text_paint;
    float density_x, density_y, text_density;
    bool quit, glyph_ok, ime_composing, ime_commit_pending, input_suspended;
    cui_id ime_focus;
    cui_text_input_state input_state;
    bool input_active;
    glyph_entry glyphs[GLYPH_CACHE_SLOTS];
    bitmap_entry bitmaps[BITMAP_SLOTS];
    size_t bitmap_bytes, shadow_bytes;
    size_t glyph_bytes;
    uint64_t clock, hits, misses, frame, frame_ticks;
    bool frame_clock_started, event_pending, callback_mode;
    bool motion_pending;
    cui_event motion;
    SDL_Event pending_event;
    char *clipboard;
    SDL_Cursor *cursors[CUI_CURSOR_HELP + 1];
};
static cui_system_appearance system_appearance(void) {
    switch (SDL_GetSystemTheme()) {
    case SDL_SYSTEM_THEME_LIGHT:
        return CUI_SYSTEM_APPEARANCE_LIGHT;
    case SDL_SYSTEM_THEME_DARK:
        return CUI_SYSTEM_APPEARANCE_DARK;
    default:
        return CUI_SYSTEM_APPEARANCE_UNKNOWN;
    }
}
bool cui_sdl3_appearance(cui_sdl3 *s, cui_platform_appearance *out) {
    if (!s || !out)
        return false;
    *out = s->appearance;
    return true;
}
cui_sdl3 *cui_sdl3_create(const char *title, int w, int h, cui_font *font) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        return NULL;
    cui_sdl3 *s = calloc(1, sizeof(*s));
    if (!s) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return NULL;
    }
    s->font = font;
    s->window = SDL_CreateWindow(title, w, h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (s->window)
        s->renderer = SDL_CreateRenderer(s->window, NULL);
    if (!s->renderer) {
        cui_sdl3_destroy(s);
        return NULL;
    }
    SDL_SetRenderDrawBlendMode(s->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(s->renderer, 1);
    s->appearance = (cui_platform_appearance){system_appearance(), CUI_APPEARANCE_SYSTEM_THEME, 1};
    SDL_StopTextInput(s->window);
    return s;
}
void cui_sdl3_destroy(cui_sdl3 *s) {
    if (!s)
        return;
    cui_shadow_destroy(&s->shadows);
    for (size_t i = 0; i < GLYPH_CACHE_SLOTS; i++) {
        SDL_DestroyTexture(s->glyphs[i].texture);
        free(s->glyphs[i].coverage);
    }
    for (size_t i = 0; i < BITMAP_SLOTS; i++)
        SDL_DestroyTexture(s->bitmaps[i].texture);
    for (size_t i = 0; i <= CUI_CURSOR_HELP; i++)
        SDL_DestroyCursor(s->cursors[i]);
    SDL_free(s->clipboard);
    SDL_DestroyRenderer(s->renderer);
    SDL_DestroyWindow(s->window);
    free(s);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
static cui_key key(SDL_Keycode k) {
    switch (k) {
    case SDLK_TAB:
        return CUI_KEY_TAB;
    case SDLK_RETURN:
        return CUI_KEY_ENTER;
    case SDLK_SPACE:
        return CUI_KEY_SPACE;
    case SDLK_ESCAPE:
        return CUI_KEY_ESCAPE;
    case SDLK_LEFT:
        return CUI_KEY_LEFT;
    case SDLK_RIGHT:
        return CUI_KEY_RIGHT;
    case SDLK_UP:
        return CUI_KEY_UP;
    case SDLK_DOWN:
        return CUI_KEY_DOWN;
    case SDLK_HOME:
        return CUI_KEY_HOME;
    case SDLK_END:
        return CUI_KEY_END;
    case SDLK_BACKSPACE:
        return CUI_KEY_BACKSPACE;
    case SDLK_DELETE:
        return CUI_KEY_DELETE;
    case SDLK_A:
        return CUI_KEY_A;
    case SDLK_Z:
        return CUI_KEY_Z;
    case SDLK_Y:
        return CUI_KEY_Y;
    case SDLK_C:
        return CUI_KEY_C;
    case SDLK_X:
        return CUI_KEY_X;
    case SDLK_V:
        return CUI_KEY_V;
    case SDLK_PAGEUP:
        return CUI_KEY_PAGEUP;
    case SDLK_PAGEDOWN:
        return CUI_KEY_PAGEDOWN;
    default:
        return k <= 127 ? cui_key_from_character((char)k) : CUI_KEY_NONE;
    }
}
bool cui_sdl3_set_cursor(cui_sdl3 *s, cui_cursor cursor) {
    static const SDL_SystemCursor kinds[] = {
        SDL_SYSTEM_CURSOR_DEFAULT,     SDL_SYSTEM_CURSOR_POINTER,   SDL_SYSTEM_CURSOR_TEXT,
        SDL_SYSTEM_CURSOR_EW_RESIZE,   SDL_SYSTEM_CURSOR_NS_RESIZE, SDL_SYSTEM_CURSOR_MOVE,
        SDL_SYSTEM_CURSOR_NOT_ALLOWED, SDL_SYSTEM_CURSOR_CROSSHAIR, SDL_SYSTEM_CURSOR_MOVE,
        SDL_SYSTEM_CURSOR_MOVE,        SDL_SYSTEM_CURSOR_CROSSHAIR, SDL_SYSTEM_CURSOR_CROSSHAIR,
        SDL_SYSTEM_CURSOR_WAIT,        SDL_SYSTEM_CURSOR_DEFAULT};
    if (!s || cursor < CUI_CURSOR_DEFAULT || cursor > CUI_CURSOR_HELP)
        return false;
    if (!s->cursors[cursor])
        s->cursors[cursor] = SDL_CreateSystemCursor(kinds[cursor]);
    return s->cursors[cursor] && SDL_SetCursor(s->cursors[cursor]);
}
/* SDL normalizes preedit selection to Unicode scalar counts (Cocoa UTF32,
 * Wayland SDL_utf8strnlen); public CUI offsets are UTF-8 bytes. */
static size_t cui_sdl3_scalar_byte(const char *text, size_t scalar) {
    size_t byte = 0;
    if (!text)
        return 0;
    while (text[byte] && scalar) {
        byte++;
        while (text[byte] && ((unsigned char)text[byte] & 0xc0) == 0x80)
            byte++;
        scalar--;
    }
    return byte;
}
static void flush_motion(cui_sdl3 *s, cui_context *ui) {
    if (s->motion_pending) {
        (void)cui_queue_event(ui, s->motion);
        s->motion_pending = false;
    }
}
static bool poll_events(cui_sdl3 *s, cui_context *ui, const SDL_Event *provided, bool pump) {
    if (!s || !ui)
        return false;
    bool reset_input = false;
    cui_text_input_state input = {0};
    bool eligible = cui_get_text_input_config(ui, &input);
    if (s->input_active && (!eligible || input.owner != s->input_state.owner ||
                            input.generation != s->input_state.generation)) {
        SDL_StopTextInput(s->window);
        reset_input = true;
        s->input_active = false;
        if (s->ime_composing || s->ime_commit_pending)
            (void)cui_queue_event(ui, (cui_event){.type = CUI_EVENT_COMPOSITION_CANCEL,
                                                  .composition_target = s->ime_focus});
        s->ime_composing = s->ime_commit_pending = false;
        s->ime_focus = 0;
    }
    if (eligible && !s->input_active) {
        SDL_PropertiesID properties = SDL_CreateProperties();
        if (!properties)
            return false;
        SDL_TextInputType type = input.config.type == CUI_TEXT_INPUT_PASSWORD_HIDDEN
                                     ? SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN
                                 : input.config.type == CUI_TEXT_INPUT_PASSWORD_VISIBLE
                                     ? SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_VISIBLE
                                     : SDL_TEXTINPUT_TYPE_TEXT;
        bool secure = input.config.type != CUI_TEXT_INPUT_TEXT ||
                      input.config.purpose != CUI_TEXT_INPUT_PURPOSE_UNSPECIFIED;
        bool ok = SDL_SetNumberProperty(properties, SDL_PROP_TEXTINPUT_TYPE_NUMBER, type);
        ok = ok && SDL_SetBooleanProperty(properties, SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN,
                                          input.multiline);
        if (secure)
            ok = ok &&
                 SDL_SetBooleanProperty(properties, SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN, false);
        if (ok)
            ok = SDL_StartTextInputWithProperties(s->window, properties);
        SDL_DestroyProperties(properties);
        if (!ok)
            return false;
        s->input_active = true;
        s->input_state = input;
    }
    (void)cui_sdl3_set_cursor(s, cui_pointer_cursor(ui));
    cui_rect caret;
    if (cui_text_input_rect(ui, &caret)) {
        SDL_Rect area = {(int)caret.x, (int)caret.y, (int)ceilf(caret.w), (int)ceilf(caret.h)};
        (void)SDL_SetTextInputArea(s->window, &area, 0);
    }
    SDL_Event e;
    while (provided || (pump && (s->event_pending || SDL_PollEvent(&e)))) {
        if (provided) {
            e = *provided;
            provided = NULL;
        } else if (s->event_pending) {
            e = s->pending_event;
            s->event_pending = false;
        }
        SDL_Window *target = SDL_GetWindowFromEvent(&e);
        if (target && target != s->window)
            continue;
        if (e.type != SDL_EVENT_MOUSE_MOTION)
            flush_motion(s, ui);
        cui_event v = {0};
        bool send = true, leave_after = false;
        switch (e.type) {
        case SDL_EVENT_SYSTEM_THEME_CHANGED:
            s->appearance.appearance = system_appearance();
            if (s->appearance.revision != UINT64_MAX)
                s->appearance.revision++;
            cui_invalidate(ui);
            send = false;
            break;
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            s->quit = true;
            send = false;
            break;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED: {
            int width, height;
            SDL_GetWindowSize(s->window, &width, &height);
            v.type = e.type == SDL_EVENT_FINGER_DOWN     ? CUI_EVENT_POINTER_DOWN
                     : e.type == SDL_EVENT_FINGER_MOTION ? CUI_EVENT_POINTER_MOVE
                     : e.type == SDL_EVENT_FINGER_UP     ? CUI_EVENT_POINTER_UP
                                                         : CUI_EVENT_CANCEL;
            v.pointer_type = CUI_POINTER_TOUCH;
            v.pointer_id = (uint64_t)e.tfinger.fingerID;
            v.pointer_device = (uint64_t)e.tfinger.touchID;
            v.x = e.tfinger.x * width;
            v.y = e.tfinger.y * height;
            break;
        }
        case SDL_EVENT_WINDOW_MOUSE_ENTER:
            v.type = CUI_EVENT_POINTER_ENTER;
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            v.type = CUI_EVENT_POINTER_LEAVE;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (e.motion.which == SDL_TOUCH_MOUSEID) {
                send = false;
                break;
            }
            v.type = CUI_EVENT_POINTER_MOVE;
            v.x = e.motion.x;
            v.y = e.motion.y;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e.button.which == SDL_TOUCH_MOUSEID) {
                send = false;
                break;
            }
            if (e.button.button != SDL_BUTTON_LEFT && e.button.button != SDL_BUTTON_RIGHT &&
                e.button.button != SDL_BUTTON_MIDDLE) {
                send = false;
                break;
            }
            v.type = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? CUI_EVENT_POINTER_DOWN
                                                           : CUI_EVENT_POINTER_UP;
            v.pointer_button = e.button.button == SDL_BUTTON_RIGHT    ? CUI_POINTER_SECONDARY
                               : e.button.button == SDL_BUTTON_MIDDLE ? CUI_POINTER_MIDDLE
                                                                      : CUI_POINTER_PRIMARY;
            v.click_count = e.button.clicks;
            v.x = e.button.x;
            v.y = e.button.y;
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            v.type = CUI_EVENT_SCROLL;
            v.x = e.wheel.mouse_x;
            v.y = e.wheel.mouse_y;
            v.dx = e.wheel.x;
            v.dy = e.wheel.y;
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            v.type = e.type == SDL_EVENT_KEY_DOWN ? CUI_EVENT_KEY_DOWN : CUI_EVENT_KEY_UP;
            v.key = key(e.key.key);
            v.repeat = e.key.repeat;
            v.ime_composing = s->ime_composing;
            v.modifiers = ((e.key.mod & SDL_KMOD_SHIFT) ? CUI_MOD_SHIFT : 0) |
                          ((e.key.mod & SDL_KMOD_CTRL) ? CUI_MOD_CTRL : 0) |
                          ((e.key.mod & SDL_KMOD_ALT) ? CUI_MOD_ALT : 0) |
                          ((e.key.mod & SDL_KMOD_GUI) ? CUI_MOD_META : 0);
            break;
        case SDL_EVENT_TEXT_EDITING:
            if (s->input_suspended || reset_input) {
                send = false;
                break;
            }
            if (!s->ime_composing && !s->ime_commit_pending)
                s->ime_focus = cui_focused(ui);
            v.composition_target = s->ime_focus;
            v.type = e.edit.text && e.edit.text[0] ? CUI_EVENT_COMPOSITION_UPDATE
                                                   : CUI_EVENT_COMPOSITION_CANCEL;
            s->ime_commit_pending |= s->ime_composing;
            s->ime_composing = e.edit.text && e.edit.text[0];
            v.text_data = e.edit.text;
            v.text_length = e.edit.text ? SDL_strlen(e.edit.text) : 0;
            v.composition_anchor = e.edit.start >= 0
                                       ? cui_sdl3_scalar_byte(e.edit.text, (size_t)e.edit.start)
                                       : v.text_length;
            int64_t end = e.edit.start;
            if (e.edit.length != -1)
                end += e.edit.length;
            v.composition_cursor =
                e.edit.start >= 0 ? cui_sdl3_scalar_byte(e.edit.text, end > 0 ? (size_t)end : 0)
                                  : v.text_length;
            break;
        case SDL_EVENT_TEXT_INPUT:
            if (s->input_suspended || reset_input) {
                send = false;
                break;
            }
            if (s->ime_composing || s->ime_commit_pending)
                v.composition_target = s->ime_focus;
            v.type = s->ime_composing || s->ime_commit_pending ? CUI_EVENT_COMPOSITION_COMMIT
                                                               : CUI_EVENT_TEXT;
            s->ime_composing = s->ime_commit_pending = false;
            v.text_data = e.text.text;
            v.text_length = e.text.text ? SDL_strlen(e.text.text) : 0;
            if (v.text_length < sizeof(v.text) && v.text_length)
                memcpy(v.text, v.text_data, v.text_length);
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            s->input_suspended = false;
            send = false;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            s->input_suspended = true;
            s->ime_composing = s->ime_commit_pending = false;
            v.type = CUI_EVENT_CANCEL;
            leave_after = true;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_EXPOSED:
            cui_invalidate(ui);
            send = false;
            break;
        default:
            send = false;
            break;
        }
        if (send) {
            if (v.type == CUI_EVENT_POINTER_MOVE && v.pointer_type == CUI_POINTER_MOUSE) {
                s->motion = v;
                s->motion_pending = true;
            } else
                cui_queue_event(ui, v);
            if (v.pointer_type == CUI_POINTER_MOUSE &&
                (v.type == CUI_EVENT_POINTER_MOVE || v.type == CUI_EVENT_POINTER_DOWN ||
                 v.type == CUI_EVENT_POINTER_UP)) {
                int width = 0, height = 0;
                if (SDL_GetWindowSize(s->window, &width, &height) &&
                    (v.x < 0 || v.y < 0 || v.x >= width || v.y >= height))
                    leave_after = true;
            }
            if (leave_after) {
                flush_motion(s, ui);
                cui_queue_event(ui, (cui_event){.type = CUI_EVENT_POINTER_LEAVE});
            }
        }
    }
    if (pump)
        flush_motion(s, ui);
    return !s->quit;
}
bool cui_sdl3_poll(cui_sdl3 *s, cui_context *ui) {
    return poll_events(s, ui, NULL, s && !s->callback_mode);
}
void cui_sdl3_size(cui_sdl3 *s, float *w, float *h) {
    int x, y;
    SDL_GetWindowSize(s->window, &x, &y);
    *w = (float)x;
    *h = (float)y;
    SDL_SetRenderLogicalPresentation(s->renderer, x, y, SDL_LOGICAL_PRESENTATION_STRETCH);
}
bool cui_sdl3_accessibility_window(cui_sdl3 *s, cui_accessibility_window *out) {
    if (!s || !out)
        return false;
    int width = 0, height = 0, x = 0, y = 0, top = 0, left = 0, bottom = 0, right = 0;
    if (!SDL_GetWindowSize(s->window, &width, &height))
        return false;
    const char *driver = SDL_GetCurrentVideoDriver();
    bool screen = driver && strcmp(driver, "x11") == 0 && SDL_GetWindowPosition(s->window, &x, &y);
    if (screen)
        SDL_GetWindowBordersSize(s->window, &top, &left, &bottom, &right);
    const char *title = SDL_GetWindowTitle(s->window);
    cui_accessibility_window result = {
        .outer = {(float)x - left, (float)y - top, (float)width + left + right,
                  (float)height + top + bottom},
        .inner = {(float)x, (float)y, (float)width, (float)height},
        .title = {title, title ? strlen(title) : 0},
        .focused = (SDL_GetWindowFlags(s->window) & SDL_WINDOW_INPUT_FOCUS) != 0,
        .has_screen_position = screen};
    *out = result;
    return true;
}
/* Query every frame: moving a window between displays can change pixels without
 * changing its logical size. Include renderer scale for offscreen/headless users. */
static bool render_density(cui_sdl3 *s) {
    int w, h;
    SDL_RendererLogicalPresentation mode;
    SDL_FRect rect;
    float x, y;
    if (!SDL_GetRenderLogicalPresentation(s->renderer, &w, &h, &mode) ||
        !SDL_GetRenderLogicalPresentationRect(s->renderer, &rect) ||
        !SDL_GetRenderScale(s->renderer, &x, &y))
        return false;
    if (mode != SDL_LOGICAL_PRESENTATION_DISABLED) {
        if (w <= 0 || h <= 0)
            return false;
        x *= rect.w / w;
        y *= rect.h / h;
    }
    if (!isfinite(x) || !isfinite(y) || x <= 0 || y <= 0 || x > 16 || y > 16)
        return false;
    s->density_x = x;
    s->density_y = y;
    s->text_density = fmaxf(x, y);
    return true;
}
#define RECT_SPAN_BATCH 128
static bool fill_span(cui_sdl3 *s, const cui_draw_command *c, float left, float right, float y,
                      float height, SDL_FRect spans[RECT_SPAN_BATCH], int *count) {
    float x0 = ceilf((c->bounds.x + left) * s->density_x - .5f) / s->density_x;
    float x1 = ceilf((c->bounds.x + right) * s->density_x - .5f) / s->density_x;
    if (x1 <= x0)
        return true;
    spans[(*count)++] = (SDL_FRect){x0, c->bounds.y + y, x1 - x0, height};
    if (*count < RECT_SPAN_BATCH)
        return true;
    if (!SDL_RenderFillRects(s->renderer, spans, *count))
        return false;
    *count = 0;
    return true;
}
static bool rect(cui_sdl3 *s, const cui_draw_command *c) {
    SDL_SetRenderDrawColor(s->renderer, c->color.r, c->color.g, c->color.b, c->color.a);
    float radii[4], edges[4];
    cui_shape_values(c, radii, edges);
    if (c->type == CUI_DRAW_RECT && radii[0] == 0 && radii[1] == 0 && radii[2] == 0 &&
        radii[3] == 0) {
        SDL_FRect box = {c->bounds.x, c->bounds.y, c->bounds.w, c->bounds.h};
        return SDL_RenderFillRect(s->renderer, &box);
    }
    SDL_FRect spans[RECT_SPAN_BATCH];
    int count = 0;
    for (int row = 0; row < (int)ceilf(c->bounds.h * s->density_y); row++) {
        float y = row / s->density_y;
        float h = fminf(1 / s->density_y, c->bounds.h - y), left, right, il, ir;
        if (!cui_shape_span(c, radii, edges, false, y + h / 2, &left, &right))
            continue;
        if (c->type == CUI_DRAW_BORDER &&
            cui_shape_span(c, radii, edges, true, y + h / 2, &il, &ir)) {
            if (!fill_span(s, c, left, il, (float)y, h, spans, &count) ||
                !fill_span(s, c, ir, right, (float)y, h, spans, &count))
                return false;
        } else if (!fill_span(s, c, left, right, (float)y, h, spans, &count))
            return false;
    }
    return count == 0 || SDL_RenderFillRects(s->renderer, spans, count);
}
static void evict(cui_sdl3 *s, glyph_entry *e) {
    if (e->coverage)
        s->glyph_bytes -= (size_t)e->w * e->h;
    SDL_DestroyTexture(e->texture);
    free(e->coverage);
    memset(e, 0, sizeof(*e));
}
static glyph_entry *oldest(cui_sdl3 *s) {
    glyph_entry *best = &s->glyphs[0];
    for (size_t i = 0; i < GLYPH_CACHE_SLOTS; i++) {
        glyph_entry *e = &s->glyphs[i];
        if (!e->texture)
            return e;
        if (e->used < best->used)
            best = e;
    }
    return best;
}
static SDL_Texture *cached_glyph(cui_sdl3 *s, int w, int h, int pitch, const unsigned char *data) {
    if (w < 0 || h < 0 || (size_t)w > GLYPH_CACHE_BYTES / (size_t)h) {
        SDL_SetError("CUI glyph exceeds the bounded cache memory budget");
        return NULL;
    }
    size_t bytes = (size_t)w * h;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            hash ^= data[j * pitch + i];
            hash *= UINT64_C(1099511628211);
        }
    for (size_t i = 0; i < GLYPH_CACHE_SLOTS; i++) {
        glyph_entry *e = &s->glyphs[i];
        if (e->texture && e->w == w && e->h == h && e->hash == hash) {
            bool same = true;
            for (int j = 0; j < h && same; j++)
                same = memcmp(e->coverage + (size_t)j * w, data + j * pitch, (size_t)w) == 0;
            if (same) {
                e->used = ++s->clock;
                s->hits++;
                return e->texture;
            }
        }
    }
    s->misses++;
    while (s->glyph_bytes + bytes > GLYPH_CACHE_BYTES) {
        glyph_entry *e = oldest(s);
        if (!e->texture) { /* Find the oldest populated entry when empty slots
                              remain. */
            for (size_t i = 0; i < GLYPH_CACHE_SLOTS; i++)
                if (s->glyphs[i].texture && (!e->texture || s->glyphs[i].used < e->used))
                    e = &s->glyphs[i];
        }
        evict(s, e);
    }
    glyph_entry *e = oldest(s);
    if (e->texture)
        evict(s, e);
    SDL_Surface *surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!surface)
        return NULL;
    for (int j = 0; j < h; j++) {
        unsigned char *p = (unsigned char *)surface->pixels + j * surface->pitch;
        for (int i = 0; i < w; i++) {
            p[i * 4] = 255;
            p[i * 4 + 1] = 255;
            p[i * 4 + 2] = 255;
            p[i * 4 + 3] = data[j * pitch + i];
        }
    }
    SDL_Texture *t = SDL_CreateTextureFromSurface(s->renderer, surface);
    SDL_DestroySurface(surface);
    if (!t)
        return NULL;
    unsigned char *copy = malloc(bytes);
    if (!copy) {
        SDL_DestroyTexture(t);
        SDL_SetError("CUI glyph coverage allocation failed");
        return NULL;
    }
    for (int j = 0; j < h; j++)
        memcpy(copy + (size_t)j * w, data + j * pitch, (size_t)w);
    if (!SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND)) {
        SDL_DestroyTexture(t);
        free(copy);
        return NULL;
    }
    *e = (glyph_entry){t, copy, w, h, hash, ++s->clock};
    s->glyph_bytes += bytes;
    return t;
}
static void glyph(void *user, int x, int y, int w, int h, int pitch, const unsigned char *data) {
    cui_sdl3 *s = user;
    if (!w || !h || !data || !s->glyph_ok)
        return;
    if (s->text_paint.kind == CUI_TEXT_PAINT_LINEAR) {
        if (w < 0 || h < 0 || (size_t)w > GLYPH_CACHE_BYTES / 4 / (size_t)h) {
            s->glyph_ok = false;
            return;
        }
        SDL_Surface *surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        if (!surface) {
            s->glyph_ok = false;
            return;
        }
        for (int j = 0; j < h; ++j) {
            unsigned char *row = (unsigned char *)surface->pixels + j * surface->pitch;
            for (int i = 0; i < w; ++i) {
                cui_color color = cui_text_paint_sample(
                    s->text_paint, (x + i + .5) / s->text_density, (y + j + .5) / s->text_density);
                row[i * 4] = color.r;
                row[i * 4 + 1] = color.g;
                row[i * 4 + 2] = color.b;
                row[i * 4 + 3] = (uint8_t)((unsigned)color.a * data[j * pitch + i] / 255);
            }
        }
        SDL_Texture *texture = SDL_CreateTextureFromSurface(s->renderer, surface);
        SDL_DestroySurface(surface);
        SDL_FRect bounds = {x / s->text_density, y / s->text_density, w / s->text_density,
                            h / s->text_density};
        if (!texture || !SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) ||
            !SDL_RenderTexture(s->renderer, texture, NULL, &bounds))
            s->glyph_ok = false;
        SDL_DestroyTexture(texture);
        return;
    }
    SDL_Texture *t = cached_glyph(s, w, h, pitch, data);
    if (!t) {
        s->glyph_ok = false;
        return;
    }
    SDL_FRect r = {x / s->text_density, y / s->text_density, w / s->text_density,
                   h / s->text_density};
    if (!SDL_SetTextureColorMod(t, s->ink.r, s->ink.g, s->ink.b) ||
        !SDL_SetTextureAlphaMod(t, s->ink.a) || !SDL_RenderTexture(s->renderer, t, NULL, &r))
        s->glyph_ok = false;
}
static void color_glyph(void *user, int x, int y, int w, int h, int pitch,
                        const unsigned char *data, cui_color color) {
    cui_sdl3 *s = user;
    s->ink = color;
    glyph(user, x, y, w, h, pitch, data);
}
static bool text_draw(cui_sdl3 *s, const cui_draw_command *c) {
    s->text_paint = c->text_paint;
    if (c->text_color_count)
        return cui_font_draw_context_line_colored_scaled(
            s->font, c->text_source, c->font_size, c->text_style, c->bounds.x, c->bounds.y,
            c->color, c->text_colors, c->text_color_count, s->text_density, color_glyph, s);
    return cui_font_draw_context_line_scaled(s->font, c->text, c->text_source, c->font_size,
                                             c->text_style, c->bounds.x, c->bounds.y,
                                             s->text_density, glyph, s);
}

static bool bitmap_draw(cui_sdl3 *s, const cui_draw_command *c, bool prepare_only) {
    if (c->bounds.w <= 0 || c->bounds.h <= 0)
        return true;
    cui_bitmap_key key;
    if (!cui_bitmap_key_make(c, &key))
        return false;
    /* Shadow preparation already supplies device-pixel rasters. Images need
     * device resolution too, including their rounded clipping mask. */
    if (!cui_shadow_type(c->type)) {
        key.width = (unsigned)ceilf(c->bounds.w * s->density_x);
        key.height = (unsigned)ceilf(c->bounds.h * s->density_y);
        float density = fminf(s->density_x, s->density_y);
        key.radii.top_left *= density;
        key.radii.top_right *= density;
        key.radii.bottom_left *= density;
        key.radii.bottom_right *= density;
    }
    if (!key.height || key.width > BITMAP_BYTES / 4 / key.height)
        return false;
    size_t bytes = (size_t)key.width * key.height * 4;
    bool shadow = cui_shadow_type(c->type);
    bitmap_entry *entry = NULL;
    for (size_t i = 0; i < BITMAP_SLOTS; i++)
        if (s->bitmaps[i].texture && cui_bitmap_key_equal(&key, &s->bitmaps[i].key)) {
            entry = &s->bitmaps[i];
            break;
        }
    if (!entry) {
        for (;;) {
            bitmap_entry *oldest = NULL, *empty = NULL;
            for (size_t i = 0; i < BITMAP_SLOTS; i++) {
                if (!s->bitmaps[i].texture)
                    empty = &s->bitmaps[i];
                else if ((!s->shadow_frame || s->bitmaps[i].frame != s->frame) &&
                         (!oldest || s->bitmaps[i].used < oldest->used))
                    oldest = &s->bitmaps[i];
            }
            if (empty && s->bitmap_bytes + bytes <= BITMAP_BYTES &&
                (!shadow || s->shadow_bytes + bytes <= CUI_SHADOW_CACHE_BYTES)) {
                entry = empty;
                break;
            }
            if (!oldest)
                return false;
            if (oldest->shadow)
                s->shadow_bytes -= (size_t)oldest->key.width * oldest->key.height * 4;
            s->bitmap_bytes -= (size_t)oldest->key.width * oldest->key.height * 4;
            SDL_DestroyTexture(oldest->texture);
            *oldest = (bitmap_entry){0};
        }
        unsigned char *rgba = cui_bitmap_raster(&key, BITMAP_BYTES);
        if (!rgba)
            return false;
        SDL_Surface *surface = SDL_CreateSurfaceFrom(
            (int)key.width, (int)key.height, SDL_PIXELFORMAT_RGBA32, rgba, (int)key.width * 4);
        SDL_Texture *texture = surface ? SDL_CreateTextureFromSurface(s->renderer, surface) : NULL;
        SDL_DestroySurface(surface);
        free(rgba);
        if (!texture)
            return false;
        *entry = (bitmap_entry){.texture = texture, .key = key, .shadow = shadow};
        s->bitmap_bytes += bytes;
        if (shadow)
            s->shadow_bytes += bytes;
    }
    entry->used = ++s->clock;
    entry->frame = s->frame;
    if (prepare_only)
        return true;
    SDL_FRect bounds = {c->bounds.x, c->bounds.y, c->bounds.w, c->bounds.h};
    if (!SDL_SetTextureBlendMode(entry->texture, SDL_BLENDMODE_BLEND) ||
        !SDL_SetTextureScaleMode(entry->texture, SDL_SCALEMODE_NEAREST) ||
        !SDL_SetTextureAlphaModFloat(entry->texture, c->bitmap.opacity))
        return false;
    if (!shadow)
        return SDL_RenderTexture(s->renderer, entry->texture, NULL, &bounds);
    SDL_RendererLogicalPresentation mode;
    int lw, lh;
    float sx, sy;
    SDL_FRect presentation;
    SDL_Rect viewport, clip;
    bool clipped = SDL_RenderClipEnabled(s->renderer);
    bool viewport_set = SDL_RenderViewportSet(s->renderer);
    if (!SDL_GetRenderLogicalPresentation(s->renderer, &lw, &lh, &mode) ||
        !SDL_GetRenderLogicalPresentationRect(s->renderer, &presentation) ||
        !SDL_GetRenderScale(s->renderer, &sx, &sy) ||
        !SDL_GetRenderViewport(s->renderer, &viewport) ||
        !SDL_GetRenderClipRect(s->renderer, &clip))
        return false;
    float px = mode == SDL_LOGICAL_PRESENTATION_DISABLED ? 1 : presentation.w / lw;
    float py = mode == SDL_LOGICAL_PRESENTATION_DISABLED ? 1 : presentation.h / lh;
    float ox = mode == SDL_LOGICAL_PRESENTATION_DISABLED ? 0 : presentation.x;
    float oy = mode == SDL_LOGICAL_PRESENTATION_DISABLED ? 0 : presentation.y;
    ox += viewport.x * px * sx;
    oy += viewport.y * py * sy;
    bounds = (SDL_FRect){roundf(c->bounds.x * px * sx + ox), roundf(c->bounds.y * py * sy + oy),
                         (float)entry->key.width, (float)entry->key.height};
    bool ok =
        SDL_SetRenderLogicalPresentation(s->renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED) &&
        SDL_SetRenderScale(s->renderer, 1, 1) && SDL_SetRenderViewport(s->renderer, NULL) &&
        SDL_SetRenderClipRect(s->renderer, NULL);
    if (ok)
        ok = SDL_RenderTexture(s->renderer, entry->texture, NULL, &bounds);
    /* Restore each property even when a previous operation failed. */
    if (!SDL_SetRenderLogicalPresentation(s->renderer, lw, lh, mode))
        ok = false;
    if (!SDL_SetRenderScale(s->renderer, sx, sy))
        ok = false;
    if (!SDL_SetRenderViewport(s->renderer, viewport_set ? &viewport : NULL))
        ok = false;
    if (!SDL_SetRenderClipRect(s->renderer, clipped ? &clip : NULL))
        ok = false;
    return ok;
}
static bool sdl_command_valid(const cui_draw_command *c) {
    return !(
        !isfinite(c->bounds.x) || !isfinite(c->bounds.y) || !isfinite(c->bounds.w) ||
        !isfinite(c->bounds.h) || !cui_shape_valid(c) || !isfinite(c->radius) ||
        !isfinite(c->stroke) || fabsf(c->bounds.x) > 1e6f || fabsf(c->bounds.y) > 1e6f ||
        (!cui_shadow_type(c->type) && (fabsf(c->bounds.w) > 16384 || fabsf(c->bounds.h) > 16384)));
}
/* Validate the complete mesh-bearing list before resource routines can convert
 * bounds to integer raster dimensions. No allocation, paint or input reduction. */
static bool sdl_mesh_list_valid(const cui_draw_list *list, bool has_font) {
    if (list->count > 131072)
        return false;
    size_t depth = 0, triangles = 0;
    for (size_t i = 0; i < list->count; i++) {
        const cui_draw_command *c = &list->commands[i];
        if (!sdl_command_valid(c))
            return false;
        switch (c->type) {
        case CUI_DRAW_MESH:
            if (c->mesh.vertex_count / 3 > CUI_SDL_MESH_TRIANGLES - triangles ||
                !cui_mesh_valid(c->mesh))
                return false;
            triangles += c->mesh.vertex_count / 3;
            break;
        case CUI_DRAW_CLIP_PUSH:
            if (depth == 64 || c->bounds.w < 0 || c->bounds.h < 0)
                return false;
            depth++;
            break;
        case CUI_DRAW_CLIP_POP:
            if (!depth)
                return false;
            depth--;
            break;
        case CUI_DRAW_TEXT:
            if (!has_font)
                return false;
            break;
        case CUI_DRAW_INSET_SHADOW:
        case CUI_DRAW_DROP_SHADOW:
        case CUI_DRAW_IMAGE:
        case CUI_DRAW_RECT:
        case CUI_DRAW_BORDER:
            break;
        default:
            return false;
        }
    }
    return depth == 0;
}
bool cui_sdl3_render_capture(cui_sdl3 *s, const cui_draw_list *list, cui_color clear,
                             const char *path) {
    if (!s || !list || (!list->commands && list->count) || !isfinite(list->width) ||
        !isfinite(list->height) || list->width < 0 || list->height < 0 || list->width > 16384 ||
        list->height > 16384)
        return false;
    if (!cui_text_tracking_supported(list, cui_font_text_capabilities() != 0) ||
        !cui_text_paints_supported(list, true))
        return false;
    if (!render_density(s))
        return false;
    bool meshes = false;
    for (size_t i = 0; i < list->count; i++)
        meshes |= list->commands[i].type == CUI_DRAW_MESH;
    if (meshes && !sdl_mesh_list_valid(list, s->font != NULL))
        return false;
    cui_sdl_mesh_frame mesh_frame = {0};
    cui_sdl_mesh_view mesh_view = {0};
    s->frame++;
    bool shadows = cui_shadow_present(list);
    s->shadow_frame = shadows || meshes;
    cui_draw_list prepared;
    if (shadows) {
        SDL_RendererLogicalPresentation mode;
        int lw, lh;
        float sx, sy;
        SDL_FRect presentation;
        SDL_Rect viewport;
        if (!SDL_GetRenderLogicalPresentation(s->renderer, &lw, &lh, &mode) ||
            !SDL_GetRenderLogicalPresentationRect(s->renderer, &presentation) ||
            !SDL_GetRenderScale(s->renderer, &sx, &sy) ||
            !SDL_GetRenderViewport(s->renderer, &viewport))
            goto failure;
        float px = 1, py = 1, ox = 0, oy = 0;
        if (mode != SDL_LOGICAL_PRESENTATION_DISABLED) {
            if (lw <= 0 || lh <= 0)
                goto failure;
            px = presentation.w / lw;
            py = presentation.h / lh;
            ox = presentation.x;
            oy = presentation.y;
        }
        ox += viewport.x * px * sx;
        oy += viewport.y * py * sy;
        if (!cui_shadow_prepare(&s->shadows, list, px * sx, py * sy, ox, oy, &prepared))
            goto failure;
        list = &prepared;
    }
    if (shadows || meshes) {
        /* Resource creation is complete before clear; no texture draw here. */
        for (size_t i = 0; i < list->count; i++)
            if ((cui_shadow_type(list->commands[i].type) ||
                 list->commands[i].type == CUI_DRAW_IMAGE) &&
                !bitmap_draw(s, &list->commands[i], true))
                goto failure;
    }
    if (meshes) {
        if (list->count > 131072 || !cui_sdl_mesh_snapshot(s->renderer, &mesh_view))
            goto failure;
        SDL_Rect stack[64], clip = {0, 0, (int)ceilf(list->width), (int)ceilf(list->height)};
        size_t depth = 0;
        for (size_t i = 0; i < list->count; i++) {
            const cui_draw_command *c = &list->commands[i];
            if (!sdl_command_valid(c))
                goto failure;
            switch (c->type) {
            case CUI_DRAW_MESH:
                if (!cui_sdl_mesh_prepare(s->renderer, &mesh_frame, &mesh_view, &c->mesh, i,
                                          depth ? &clip : NULL))
                    goto failure;
                break;
            case CUI_DRAW_CLIP_PUSH: {
                if (depth == 64 || c->bounds.w < 0 || c->bounds.h < 0)
                    goto failure;
                stack[depth++] = clip;
                SDL_Rect next = {(int)floorf(c->bounds.x), (int)floorf(c->bounds.y),
                                 (int)ceilf(c->bounds.w), (int)ceilf(c->bounds.h)};
                if (!SDL_GetRectIntersection(&clip, &next, &clip))
                    clip = (SDL_Rect){0};
                break;
            }
            case CUI_DRAW_CLIP_POP:
                if (!depth)
                    goto failure;
                clip = stack[--depth];
                break;
            case CUI_DRAW_TEXT:
                if (!s->font)
                    goto failure;
                break;
            case CUI_DRAW_INSET_SHADOW:
            case CUI_DRAW_DROP_SHADOW:
            case CUI_DRAW_IMAGE:
            case CUI_DRAW_RECT:
            case CUI_DRAW_BORDER:
                break;
            default:
                goto failure;
            }
        }
        if (depth)
            goto failure;
    }
    if (!SDL_SetRenderClipRect(s->renderer, NULL) ||
        !SDL_SetRenderDrawColor(s->renderer, clear.r, clear.g, clear.b, clear.a))
        goto failure;
    if (!SDL_RenderClear(s->renderer))
        goto failure;
    SDL_Rect stack[64], clip = {0, 0, (int)ceilf(list->width), (int)ceilf(list->height)};
    size_t depth = 0;
    for (size_t i = 0; i < list->count; i++) {
        const cui_draw_command *c = &list->commands[i];
        if (!sdl_command_valid(c))
            goto failure;
        switch (c->type) {
        case CUI_DRAW_MESH:
            if (!cui_sdl_mesh_draw(s->renderer, &mesh_frame, &mesh_view, i, depth ? &clip : NULL))
                goto failure;
            break;
        case CUI_DRAW_INSET_SHADOW:
        case CUI_DRAW_DROP_SHADOW: {
            SDL_Rect saved;
            bool enabled = SDL_RenderClipEnabled(s->renderer);
            if (!SDL_GetRenderClipRect(s->renderer, &saved) ||
                !SDL_SetRenderClipRect(s->renderer, NULL))
                goto failure;
            bool ok = bitmap_draw(s, c, false);
            if (!SDL_SetRenderClipRect(s->renderer, enabled ? &saved : NULL))
                ok = false;
            if (!ok)
                goto failure;
            break;
        }
        case CUI_DRAW_IMAGE:
            if (!bitmap_draw(s, c, false))
                goto failure;
            break;
        case CUI_DRAW_RECT:
        case CUI_DRAW_BORDER:
            if (!rect(s, c))
                goto failure;
            break;
        case CUI_DRAW_TEXT:
            s->ink = c->color;
            s->glyph_ok = true;
            if (!s->font || !text_draw(s, c) || !s->glyph_ok)
                goto failure;
            break;
        case CUI_DRAW_CLIP_PUSH: {
            if (depth == 64)
                goto failure;
            stack[depth++] = clip;
            SDL_Rect next = {(int)floorf(c->bounds.x), (int)floorf(c->bounds.y),
                             (int)ceilf(c->bounds.w), (int)ceilf(c->bounds.h)};
            if (!SDL_GetRectIntersection(&clip, &next, &clip))
                clip = (SDL_Rect){0, 0, 0, 0};
            if (!SDL_SetRenderClipRect(s->renderer, &clip))
                goto failure;
            break;
        }
        case CUI_DRAW_CLIP_POP:
            if (!depth)
                goto failure;
            clip = stack[--depth];
            if (!SDL_SetRenderClipRect(s->renderer, depth ? &clip : NULL))
                goto failure;
            break;
        default:
            goto failure;
        }
    }
    if (depth)
        goto failure;
    if (path && !cui_sdl3_save_bmp(s, path))
        goto failure;
    bool ok = SDL_RenderPresent(s->renderer);
    cui_sdl_mesh_release(&mesh_frame);
    return ok;
failure:
    cui_sdl_mesh_release(&mesh_frame);
    return false;
}
bool cui_sdl3_render(cui_sdl3 *s, const cui_draw_list *list, cui_color clear) {
    return cui_sdl3_render_capture(s, list, clear, NULL);
}
bool cui_sdl3_save_bmp(cui_sdl3 *s, const char *path) {
    SDL_Surface *surface = SDL_RenderReadPixels(s->renderer, NULL);
    if (!surface)
        return false;
    bool ok = SDL_SaveBMP(surface, path);
    SDL_DestroySurface(surface);
    return ok;
}
const char *cui_sdl3_driver(cui_sdl3 *s) {
    return s ? SDL_GetRendererName(s->renderer) : "unavailable";
}
const char *cui_sdl3_video_driver(void) {
    return SDL_GetCurrentVideoDriver();
}
const char *cui_sdl3_error(void) {
    return SDL_GetError();
}
cui_sdl3_cache_stats cui_sdl3_glyph_cache_stats(cui_sdl3 *s) {
    cui_sdl3_cache_stats stats = {0};
    if (!s)
        return stats;
    stats.coverage_bytes = s->glyph_bytes;
    stats.hits = s->hits;
    stats.misses = s->misses;
    for (size_t i = 0; i < GLYPH_CACHE_SLOTS; i++)
        stats.entries += s->glyphs[i].texture != NULL;
    return stats;
}
static bool clipboard_set(void *user, cui_str text) {
    if (!user || (!text.data && text.length) || text.length > 16u * 1024u * 1024u ||
        (text.length && memchr(text.data, 0, text.length)))
        return false;
    char *copy = malloc(text.length + 1);
    if (!copy)
        return false;
    if (text.length)
        memcpy(copy, text.data, text.length);
    copy[text.length] = 0;
    bool ok = SDL_SetClipboardText(copy);
    free(copy);
    return ok;
}
static cui_str clipboard_get(void *user) {
    cui_sdl3 *s = user;
    if (!s)
        return (cui_str){NULL, 0};
    SDL_free(s->clipboard);
    s->clipboard = SDL_GetClipboardText();
    return (cui_str){s->clipboard, s->clipboard ? strlen(s->clipboard) : 0};
}
cui_platform cui_sdl3_clipboard_interface(cui_sdl3 *s) {
    return (cui_platform){.user = s,
                          .capabilities = s ? CUI_PLATFORM_CLIPBOARD : 0,
                          .clipboard_set = clipboard_set,
                          .clipboard_get = clipboard_get};
}
void cui_sdl3_wait(unsigned ms) {
    SDL_Delay(ms);
}
static bool render(void *s, const cui_draw_list *l, cui_color c) {
    return cui_sdl3_render(s, l, c);
}
static void destroy(void *s) {
    cui_sdl3_destroy(s);
}
cui_renderer cui_sdl3_interface(cui_sdl3 *s) {
    const char *name = cui_sdl3_driver(s);
    return (cui_renderer){s, name,
                          (s && s->font ? CUI_RENDER_TEXT | CUI_RENDER_TEXT_LINEAR_GRADIENT : 0) |
                              CUI_RENDER_ROUNDED | CUI_RENDER_CLIP | CUI_RENDER_IMAGE |
                              CUI_RENDER_MESH | CUI_RENDER_DROP_SHADOW | CUI_RENDER_INSET_SHADOW |
                              (s && strcmp(name, "software") ? CUI_RENDER_ACCELERATED : 0),
                          render, destroy};
}

static cui_window_info cui_sdl3_window_query(void *user) {
    cui_sdl3 *s = user;
    if (!s || !s->window)
        return (cui_window_info){0};
    SDL_WindowFlags flags = SDL_GetWindowFlags(s->window);
    cui_window_info info = {.decoration = (flags & SDL_WINDOW_BORDERLESS)
                                              ? CUI_WINDOW_CLIENT_DECORATED
                                              : CUI_WINDOW_SERVER_DECORATED,
                            .actions = 1u << CUI_WINDOW_CLOSE,
                            .maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0,
                            .fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0};
#ifdef SDL_PLATFORM_MACOS
    info.decoration = CUI_WINDOW_NATIVE_TRAFFIC_LIGHTS;
#endif
    const char *driver = SDL_GetCurrentVideoDriver();
    if (driver && strcmp(driver, "dummy") && strcmp(driver, "offscreen")) {
        info.actions |= 1u << CUI_WINDOW_MINIMIZE;
        if (flags & SDL_WINDOW_RESIZABLE) {
            info.actions |= (1u << CUI_WINDOW_MAXIMIZE) | (1u << CUI_WINDOW_RESTORE);
#ifndef SDL_PLATFORM_MACOS
            info.actions |= 1u << CUI_WINDOW_TITLEBAR_DOUBLE_CLICK;
#endif
        }
    }
    return info;
}
static bool cui_sdl3_window_perform(void *user, cui_window_action action, float x, float y) {
    (void)x;
    (void)y;
    cui_sdl3 *s = user;
    if (!s || action < CUI_WINDOW_MOVE || action > CUI_WINDOW_TITLEBAR_DOUBLE_CLICK)
        return false;
    cui_window_info info = cui_sdl3_window_query(user);
    if (!(info.actions & (1u << action)))
        return false;
    switch (action) {
    case CUI_WINDOW_MINIMIZE:
        return SDL_MinimizeWindow(s->window);
    case CUI_WINDOW_MAXIMIZE:
        return SDL_MaximizeWindow(s->window);
    case CUI_WINDOW_RESTORE:
        return SDL_RestoreWindow(s->window);
    case CUI_WINDOW_TITLEBAR_DOUBLE_CLICK:
        return info.maximized ? SDL_RestoreWindow(s->window) : SDL_MaximizeWindow(s->window);
    case CUI_WINDOW_CLOSE:
        s->quit = true;
        return true;
    default:
        return false;
    }
}
cui_window_host cui_sdl3_window_interface(cui_sdl3 *s) {
    return (cui_window_host){s, cui_sdl3_window_query, cui_sdl3_window_perform};
}
bool cui_sdl3_set_decorated(cui_sdl3 *s, bool decorated) {
    const char *driver = SDL_GetCurrentVideoDriver();
    if (!s || !s->window || !driver || !strcmp(driver, "dummy") || !strcmp(driver, "offscreen"))
        return false;
    return SDL_SetWindowBordered(s->window, decorated);
}

float cui_sdl3_frame_delta(cui_sdl3 *s) {
    if (!s)
        return 0;
    Uint64 now = SDL_GetTicksNS();
    float dt = s->frame_clock_started ? (float)((double)(now - s->frame_ticks) / 1e9) : 0;
    s->frame_ticks = now;
    s->frame_clock_started = true;
    return dt;
}
bool cui_sdl3_wait_events(cui_sdl3 *s, int timeout_ms) {
    if (!s)
        return false;
    /* Retain the wakeup event for the ordinary input/IME dispatcher. Passing
     * NULL to SDL's wait can leave a poll-cycle sentinel before later events. */
    if (!s->event_pending)
        s->event_pending = timeout_ms < 0 ? SDL_WaitEvent(&s->pending_event)
                                          : SDL_WaitEventTimeout(&s->pending_event, timeout_ms);
    s->frame_ticks = SDL_GetTicksNS();
    return s->event_pending;
}

/* SDL owns modal-loop iteration; application drawing never runs in our watch. */
typedef struct {
    cui_sdl3_app app;
    cui_sdl3 *host;
    cui_context *ui;
    bool iterating, idle;
} callback_app;
static callback_app *starting_app;
static SDL_AppResult SDLCALL app_init(void **state, int argc, char **argv) {
    callback_app *app = starting_app;
    *state = app;
    if (!app->app.init(app->app.user, argc, argv, &app->host, &app->ui) || !app->host || !app->ui)
        return SDL_APP_FAILURE;
    app->host->callback_mode = true;
    return SDL_APP_CONTINUE;
}
static SDL_AppResult SDLCALL app_event(void *state, SDL_Event *event) {
    callback_app *app = state;
    /* SDL lifecycle events may originate on worker threads. Leave lifecycle
     * termination to SDL itself; never mutate Core or paint on that thread. */
    if (!SDL_IsMainThread())
        return SDL_APP_CONTINUE;
    return poll_events(app->host, app->ui, event, false) ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}
static SDL_AppResult SDLCALL app_frame(void *state) {
    callback_app *app = state;
    if (app->iterating || !SDL_IsMainThread())
        return SDL_APP_CONTINUE;
    app->iterating = true;
    flush_motion(app->host, app->ui);
    if (app->idle)
        app->host->frame_ticks = SDL_GetTicksNS();
    int result = app->app.frame(app->app.user, app->host, app->ui);
    app->iterating = false;
    app->idle = !cui_needs_redraw(app->ui);
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, app->idle ? "waitevent" : "0");
    return result < 0 ? SDL_APP_FAILURE : result > 0 ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}
static void SDLCALL app_quit(void *state, SDL_AppResult result) {
    callback_app *app = state;
    app->app.quit(app->app.user, result == SDL_APP_SUCCESS);
}
int cui_sdl3_run(int argc, char **argv, cui_sdl3_app callbacks) {
    if (!SDL_IsMainThread() || starting_app || !callbacks.init || !callbacks.frame ||
        !callbacks.quit)
        return 1;
    callback_app app = {.app = callbacks};
    starting_app = &app;
    const char *rate = SDL_GetHint(SDL_HINT_MAIN_CALLBACK_RATE);
    char *saved_rate = rate ? SDL_strdup(rate) : NULL;
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "0");
    int result = SDL_EnterAppMainCallbacks(argc, argv, app_init, app_frame, app_event, app_quit);
    starting_app = NULL;
    if (saved_rate)
        SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, saved_rate);
    else
        SDL_ResetHint(SDL_HINT_MAIN_CALLBACK_RATE);
    SDL_free(saved_rate);
    return result;
}
