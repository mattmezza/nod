#ifndef CUI_RENDERER_H
#define CUI_RENDERER_H
#include "cui.h"
enum {
    CUI_RENDER_ACCELERATED = 1,
    CUI_RENDER_TEXT = 2,
    CUI_RENDER_ROUNDED = 4,
    CUI_RENDER_CLIP = 8,
    CUI_RENDER_IMAGE = 16,
    CUI_RENDER_MESH = 32,
    CUI_RENDER_DROP_SHADOW = 64,
    CUI_RENDER_INSET_SHADOW = 128,
    CUI_RENDER_TEXT_LINEAR_GRADIENT = 256
};
typedef struct {
    void *user;
    const char *name;
    unsigned capabilities;
    bool (*render)(void *user, const cui_draw_list *list, cui_color clear);
    void (*destroy)(void *user);
} cui_renderer;
#endif
