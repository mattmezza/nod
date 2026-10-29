/* SPDX-License-Identifier: MIT */
#ifndef CUI_SDL3_MESH_PRIVATE_H
#define CUI_SDL3_MESH_PRIVATE_H
#include "../software/cui_mesh.h"
#include <SDL3/SDL.h>
#include <limits.h>
#include <stdlib.h>
/* Frame-local resources only. The 256-triangle budget accommodates ten Popover
 * arrows (15 fill + 60 outline vertices = 25 triangles each). It is not a Core
 * validity limit. SDL/driver-private staging is outside these owned budgets. */
#define CUI_SDL_MESH_TRIANGLES 256u
#define CUI_SDL_MESH_TEXELS (8u * 1024u * 1024u)
typedef struct {
    SDL_Texture *texture;
    SDL_FRect bounds;
    size_t command;
} cui_sdl_mesh_ticket;
typedef struct {
    cui_sdl_mesh_ticket tickets[CUI_SDL_MESH_TRIANGLES];
    size_t count, triangles, texels;
} cui_sdl_mesh_frame;
typedef struct {
    SDL_RendererLogicalPresentation mode;
    int logical_w, logical_h, width, height;
    float scale_x, scale_y;
    double sx, sy, ox, oy;
    SDL_Rect viewport, physical_viewport;
    bool viewport_set;
} cui_sdl_mesh_view;
static void cui_sdl_mesh_release(cui_sdl_mesh_frame *frame) {
    for (size_t i = 0; i < frame->count; i++)
        SDL_DestroyTexture(frame->tickets[i].texture);
    *frame = (cui_sdl_mesh_frame){0};
}
static bool cui_sdl_mesh_snapshot(SDL_Renderer *renderer, cui_sdl_mesh_view *view) {
    SDL_FRect presentation;
    SDL_Texture *target = SDL_GetRenderTarget(renderer);
    if (!SDL_GetRenderLogicalPresentation(renderer, &view->logical_w, &view->logical_h,
                                          &view->mode) ||
        !SDL_GetRenderLogicalPresentationRect(renderer, &presentation) ||
        !SDL_GetRenderScale(renderer, &view->scale_x, &view->scale_y) ||
        !SDL_GetRenderViewport(renderer, &view->viewport))
        return false;
    view->viewport_set = SDL_RenderViewportSet(renderer);
    if (target) {
        float w, h;
        if (!SDL_GetTextureSize(target, &w, &h) || w < 1 || h < 1 || w > 16384 || h > 16384)
            return false;
        view->width = (int)w;
        view->height = (int)h;
    } else if (!SDL_GetRenderOutputSize(renderer, &view->width, &view->height))
        return false;
    if (view->width < 1 || view->height < 1 || view->width > 16384 || view->height > 16384)
        return false;
    /* Match SDL's float current_scale and pixel viewport rounding, then retain
     * projected geometry/derivatives in double. Disabled presentation has no offset. */
    float sx = view->scale_x, sy = view->scale_y, ox = 0, oy = 0;
    int pixel_w = view->width, pixel_h = view->height;
    if (view->mode != SDL_LOGICAL_PRESENTATION_DISABLED) {
        if (view->logical_w <= 0 || view->logical_h <= 0 || !isfinite(presentation.w) ||
            !isfinite(presentation.h) || presentation.w < 0 || presentation.h < 0 ||
            (double)presentation.w > INT_MAX || (double)presentation.h > INT_MAX)
            return false;
        sx *= presentation.w / view->logical_w;
        sy *= presentation.h / view->logical_h;
        ox = presentation.x;
        oy = presentation.y;
        pixel_w = (int)presentation.w;
        pixel_h = (int)presentation.h;
    }
    if (!isfinite(sx) || !isfinite(sy) || sx <= 0 || sy <= 0 || sx > 16 || sy > 16 ||
        !isfinite(ox) || !isfinite(oy))
        return false;
    double x = floorf(view->viewport.x * sx + ox), y = floorf(view->viewport.y * sy + oy);
    double w = view->viewport_set ? ceilf(view->viewport.w * sx) : pixel_w;
    double h = view->viewport_set ? ceilf(view->viewport.h * sy) : pixel_h;
    if (x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX || w < 0 || w > INT_MAX || h < 0 ||
        h > INT_MAX || x + w > INT_MAX || y + h > INT_MAX)
        return false;
    view->sx = sx;
    view->sy = sy;
    view->ox = x;
    view->oy = y;
    view->physical_viewport = (SDL_Rect){(int)x, (int)y, (int)w, (int)h};
    return true;
}
static bool cui_sdl_mesh_clip(const cui_sdl_mesh_view *view, const SDL_Rect *logical,
                              SDL_Rect *out) {
    SDL_Rect target = {0, 0, view->width, view->height};
    if (!SDL_GetRectIntersection(&target, &view->physical_viewport, out)) {
        *out = (SDL_Rect){0};
        return true;
    }
    if (logical) {
        double x = view->ox + floorf(logical->x * (float)view->sx);
        double y = view->oy + floorf(logical->y * (float)view->sy);
        double w = ceilf(logical->w * (float)view->sx);
        double h = ceilf(logical->h * (float)view->sy);
        if (x < INT_MIN || x > INT_MAX || y < INT_MIN || y > INT_MAX || w < 0 || w > INT_MAX ||
            h < 0 || h > INT_MAX || x + w > INT_MAX || y + h > INT_MAX)
            return false;
        SDL_Rect clip = {(int)x, (int)y, (int)w, (int)h};
        if (!SDL_GetRectIntersection(out, &clip, out))
            *out = (SDL_Rect){0};
    }
    return true;
}
typedef struct {
    unsigned char *pixels;
    SDL_Rect bounds;
} cui_sdl_mesh_raster;
static void cui_sdl_mesh_pixel(void *userdata, int x, int y, cui_color color) {
    cui_sdl_mesh_raster *raster = userdata;
    unsigned char *p =
        raster->pixels + ((size_t)(y - raster->bounds.y) * (unsigned)raster->bounds.w +
                          (unsigned)(x - raster->bounds.x)) *
                             4;
    p[0] = color.r;
    p[1] = color.g;
    p[2] = color.b;
    p[3] = color.a;
}
static bool cui_sdl_mesh_prepare(SDL_Renderer *renderer, cui_sdl_mesh_frame *frame,
                                 const cui_sdl_mesh_view *view, const cui_mesh_paint *paint,
                                 size_t command, const SDL_Rect *logical_clip) {
    if (paint->vertex_count / 3 > CUI_SDL_MESH_TRIANGLES - frame->triangles ||
        !cui_mesh_valid(*paint))
        return false;
    frame->triangles += paint->vertex_count / 3;
    SDL_Rect clip;
    if (!cui_sdl_mesh_clip(view, logical_clip, &clip))
        return false;
    cui_color color = {cui_mesh_cpu_channel(paint->color.r, paint->color.a),
                       cui_mesh_cpu_channel(paint->color.g, paint->color.a),
                       cui_mesh_cpu_channel(paint->color.b, paint->color.a), 0};
    for (size_t i = 0; i < paint->vertex_count; i += 3) {
        cui_mesh_cpu_vertex v[3];
        for (size_t j = 0; j < 3; j++) {
            const cui_mesh_vertex *in = &paint->vertices[i + j];
            v[j] = (cui_mesh_cpu_vertex){
                (paint->origin_x + (double)paint->scale * in->x) * view->sx + view->ox,
                (paint->origin_y + (double)paint->scale * in->y) * view->sy + view->oy, in->s,
                in->t};
        }
        if (cui_mesh_cpu_edge(v[0], v[1], v[2].x, v[2].y) == 0)
            continue;
        double left = fmax(clip.x, fmin(v[0].x, fmin(v[1].x, v[2].x)));
        double top = fmax(clip.y, fmin(v[0].y, fmin(v[1].y, v[2].y)));
        double right = fmin((double)clip.x + clip.w, fmax(v[0].x, fmax(v[1].x, v[2].x)));
        double bottom = fmin((double)clip.y + clip.h, fmax(v[0].y, fmax(v[1].y, v[2].y)));
        if (left >= right || top >= bottom)
            continue;
        SDL_Rect bounds = {(int)floor(left), (int)floor(top), (int)(ceil(right) - floor(left)),
                           (int)(ceil(bottom) - floor(top))};
        size_t texels = (size_t)bounds.w * (unsigned)bounds.h;
        if (texels > CUI_SDL_MESH_TEXELS - frame->texels)
            return false;
        cui_sdl_mesh_raster raster = {calloc(texels, 4), bounds};
        if (!raster.pixels)
            return false;
        cui_mesh_cpu_triangle(
            v, color, paint->color.a, (unsigned)view->width, (unsigned)view->height,
            (cui_rect){(float)clip.x, (float)clip.y, (float)clip.w, (float)clip.h},
            cui_sdl_mesh_pixel, &raster);
        SDL_Surface *surface = SDL_CreateSurfaceFrom(bounds.w, bounds.h, SDL_PIXELFORMAT_RGBA32,
                                                     raster.pixels, bounds.w * 4);
        SDL_Texture *texture = surface ? SDL_CreateTextureFromSurface(renderer, surface) : NULL;
        SDL_DestroySurface(surface);
        free(raster.pixels);
        if (!texture)
            return false;
        if (!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) ||
            !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)) {
            SDL_DestroyTexture(texture);
            return false;
        }
        frame->tickets[frame->count++] = (cui_sdl_mesh_ticket){
            texture, {(float)bounds.x, (float)bounds.y, (float)bounds.w, (float)bounds.h}, command};
        frame->texels += texels;
    }
    return true;
}
static bool cui_sdl_mesh_draw(SDL_Renderer *renderer, const cui_sdl_mesh_frame *frame,
                              const cui_sdl_mesh_view *view, size_t command,
                              const SDL_Rect *logical_clip) {
    bool ok = SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED) &&
              SDL_SetRenderScale(renderer, 1, 1) && SDL_SetRenderViewport(renderer, NULL) &&
              SDL_SetRenderClipRect(renderer, NULL);
    for (size_t i = 0; ok && i < frame->count; i++)
        if (frame->tickets[i].command == command)
            ok = SDL_RenderTexture(renderer, frame->tickets[i].texture, NULL,
                                   &frame->tickets[i].bounds);
    /* Restore each state even if another setter or submission failed. */
    if (!SDL_SetRenderLogicalPresentation(renderer, view->logical_w, view->logical_h, view->mode))
        ok = false;
    if (!SDL_SetRenderScale(renderer, view->scale_x, view->scale_y))
        ok = false;
    if (!SDL_SetRenderViewport(renderer, view->viewport_set ? &view->viewport : NULL))
        ok = false;
    if (!SDL_SetRenderClipRect(renderer, logical_clip))
        ok = false;
    return ok;
}
#endif
