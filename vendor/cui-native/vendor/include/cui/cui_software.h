/* SPDX-License-Identifier: MIT */
#ifndef CUI_SOFTWARE_H
#define CUI_SOFTWARE_H
#include "cui_renderer.h"
#include "cui_text.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct cui_software cui_software;
cui_software *cui_software_create(unsigned width, unsigned height, cui_font *font);
void cui_software_destroy(cui_software *renderer);
bool cui_software_resize(cui_software *renderer, unsigned width, unsigned height);
bool cui_software_render(cui_software *renderer, const cui_draw_list *list, cui_color clear);
const unsigned char *cui_software_pixels(const cui_software *renderer, unsigned *width,
                                         unsigned *height);
bool cui_software_save_ppm(const cui_software *renderer, const char *path);
cui_renderer cui_software_interface(cui_software *renderer);
#ifdef __cplusplus
}
#endif
#endif
