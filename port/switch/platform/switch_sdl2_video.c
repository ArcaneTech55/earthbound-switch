#include "platform/platform.h"
#include "../widescreen/layout.h"
#include <SDL.h>
#include <string.h>

/* Base-port video path: original geometry, never stretch to 16:9.
 * This is deliberately separate from the eventual scene-level 16:9 renderer. */
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *frame_texture;
static pixel_t frame[EB_VIEWPORT_WIDTH*EB_VIEWPORT_HEIGHT];
static bool use_vsync=true;

bool platform_video_init(void) {
    window=SDL_CreateWindow("EarthBound Switch",SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,1280,720,SDL_WINDOW_SHOWN);
    if(!window) return false;
    renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED |
                                        SDL_RENDERER_PRESENTVSYNC);
    if(!renderer) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!renderer)return false;
    frame_texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_BGR565,
        SDL_TEXTUREACCESS_STREAMING,EB_VIEWPORT_WIDTH,EB_VIEWPORT_HEIGHT);
    return frame_texture!=NULL;
}
void platform_video_shutdown(void) {
    if(frame_texture){SDL_DestroyTexture(frame_texture);frame_texture=NULL;}
    if(renderer){SDL_DestroyRenderer(renderer);renderer=NULL;}
    if(window){SDL_DestroyWindow(window);window=NULL;}
}
void platform_video_begin_frame(void) { }
void platform_video_send_scanline(int y,const pixel_t *pixels) {
    if(y>=0 && y<EB_VIEWPORT_HEIGHT && pixels)
        memcpy(frame+(size_t)y*EB_VIEWPORT_WIDTH,pixels,
               (size_t)EB_VIEWPORT_WIDTH*sizeof(pixel_t));
}
pixel_t *platform_video_get_framebuffer(void) { return frame; }
void platform_video_end_frame(void) {
    if(!renderer || !frame_texture)return;
    if(SDL_UpdateTexture(frame_texture,NULL,frame,
        EB_VIEWPORT_WIDTH*(int)sizeof(pixel_t))!=0)return;
    int width=1280,height=720;
    SDL_GetRendererOutputSize(renderer,&width,&height);
    /* No stretched widescreen: original viewport is aspect-fitted until the
     * world renderer itself can draw additional scenery at 16:9. */
    EbWsRect fit=eb_ws_fit((EbWsSize){EB_VIEWPORT_WIDTH,EB_VIEWPORT_HEIGHT},
                          (EbWsSize){width,height});
    SDL_Rect dst={fit.x,fit.y,fit.width,fit.height};
    SDL_SetRenderDrawColor(renderer,0,0,0,255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer,frame_texture,NULL,&dst);
    SDL_RenderPresent(renderer);
}
void platform_video_set_vsync(bool enabled) {
    /* SDL2 renderer vsync is selected at creation; preserve API contract. */
    use_vsync=enabled;
    (void)use_vsync;
}
