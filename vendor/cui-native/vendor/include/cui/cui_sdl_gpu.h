/* SPDX-License-Identifier: MIT */
#ifndef CUI_SDL_GPU_H
#define CUI_SDL_GPU_H
#include "cui_renderer.h"
#include "cui_text.h"
typedef struct cui_sdl_gpu cui_sdl_gpu;
/* Borrow an SDL_GPUDevice (SPIR-V capable) and already claimed SDL_Window.
 * Device/window/font must outlive this adapter. No windowing or events owned. */
cui_sdl_gpu *cui_sdl_gpu_create(void *device, void *window, cui_font *font);
void cui_sdl_gpu_destroy(cui_sdl_gpu *renderer);
bool cui_sdl_gpu_render(cui_sdl_gpu *renderer, const cui_draw_list *list, cui_color clear);
bool cui_sdl_gpu_save_ppm(cui_sdl_gpu *renderer, const char *path);
const char *cui_sdl_gpu_driver(cui_sdl_gpu *renderer);
cui_renderer cui_sdl_gpu_interface(cui_sdl_gpu *renderer);
#endif
