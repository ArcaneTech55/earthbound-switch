#include "platform/platform.h"
#include "../widescreen/layout.h"
#include "../widescreen/scaling.h"
#include "../widescreen/scene_art.h"
#include <stdio.h>
#include <SDL.h>
#include <string.h>

/* Presents the native game renderer viewport without changing scene geometry.
 * The optional 400x225 viewport draws additional world content where supported.
 * Remaining scene-specific artwork still needs proper widescreen authoring. */
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *frame_texture;
static pixel_t frame[EB_VIEWPORT_WIDTH*EB_VIEWPORT_HEIGHT];
static bool use_vsync=true;
static EbScaleMode scale_mode=EB_SCALE_ASPECT_NEAREST;

static void read_presentation_settings(void) {
    FILE *f=fopen("sdmc:/switch/earthbound-switch/scaling.cfg","r");
    if(!f)return;
    char mode[32]={0};
    if(fgets(mode,sizeof(mode),f)) {
        mode[strcspn(mode,"\r\n")]=0;
        scale_mode=eb_scale_mode_parse(mode);
    }
    fclose(f);
}

bool platform_video_init(void) {
    read_presentation_settings();
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
                scale_mode==EB_SCALE_ASPECT_LINEAR?"linear":"nearest");
    SDL_DisplayMode display={0};
    int output_width=1280, output_height=720;
    /* The Switch SDL video backend may advertise a higher resolution when
     * docked. The game scene remains 400x225 independent of display size. */
    if(SDL_GetCurrentDisplayMode(0,&display)==0 &&
       display.w>=640 && display.h>=360 &&
       display.w<=3840 && display.h<=2160) {
        output_width=display.w;
        output_height=display.h;
    }
    window=SDL_CreateWindow("EarthBound Switch",SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,output_width,output_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
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
    eb_switch_scene_art_frame(frame,EB_VIEWPORT_WIDTH,EB_VIEWPORT_HEIGHT);
    if(SDL_UpdateTexture(frame_texture,NULL,frame,
        EB_VIEWPORT_WIDTH*(int)sizeof(pixel_t))!=0)return;
    int width=1280,height=720;
    if(SDL_GetRendererOutputSize(renderer,&width,&height)!=0) {
        width=1280; height=720;
    }
    /* No stretched widescreen: original viewport is aspect-fitted until the
     * world renderer itself can draw additional scenery at 16:9. */
    EbWsRect fit=eb_scale_destination((EbWsSize){EB_VIEWPORT_WIDTH,EB_VIEWPORT_HEIGHT},
                                          (EbWsSize){width,height},scale_mode);
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
