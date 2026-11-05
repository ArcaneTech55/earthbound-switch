#include "platform/platform.h"
#include "../widescreen/layout.h"
#include "../widescreen/scaling.h"
#include "../widescreen/scene_art.h"
#include <stdio.h>
#include <SDL.h>
#include <string.h>

/* Native game PPU produces EB_VIEWPORT_WIDTH pixels per scanline.
 * Both framebuffer row stride and SDL texture upload pitch use that width.
 * Do not use SNES_WIDTH (256) here: it is a separate hardware constant. */
_Static_assert(sizeof(pixel_t) == 2, "SDL BGR565 requires 16-bit pixel_t");
_Static_assert(EB_VIEWPORT_HEIGHT == 224, "Switch PPU target is 224 rows");
_Static_assert(EB_VIEWPORT_WIDTH == 400 || EB_VIEWPORT_WIDTH == 256,
               "Switch viewport must be 400 or 256 pixels wide");
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *frame_texture;
static pixel_t frame[EB_VIEWPORT_WIDTH*EB_VIEWPORT_HEIGHT];
static bool use_vsync=true;
static EbScaleMode scale_mode=EB_SCALE_ASPECT_NEAREST;

/* Minimal embedded 5x7 font; no SDL_ttf dependency or external assets. */
extern bool eb_switch_menu_open;
extern int eb_switch_menu_selected;
extern int eb_switch_menu_scale;
extern bool eb_switch_menu_instant_text;
extern bool eb_switch_menu_auto_heal;
static const char *const trainer_items[] = {
    "SCALING", "INSTANT TEXT", "AUTO HEAL", "FULL HEAL",
    "ADD 1000 CASH", "BANK DEPOSIT 1000", "BANK WITHDRAW 1000",
    "RESTORE HP PP", "CLOSE"
};
static const unsigned char trainer_glyphs[][7] = {
    {14,17,17,31,17,17,17}, /* A */
    {30,17,17,30,17,17,30}, /* B */
    {14,17,16,16,16,17,14}, /* C */
    {30,17,17,17,17,17,30}, /* D */
    {31,16,16,30,16,16,31}, /* E */
    {31,16,16,30,16,16,16}, /* F */
    {14,17,16,23,17,17,15}, /* G */
    {17,17,17,31,17,17,17}, /* H */
    {14,4,4,4,4,4,14},       /* I */
    {7,2,2,2,18,18,12},      /* J */
    {17,18,20,24,20,18,17}, /* K */
    {16,16,16,16,16,16,31}, /* L */
    {17,27,21,21,17,17,17}, /* M */
    {17,25,21,19,17,17,17}, /* N */
    {14,17,17,17,17,17,14}, /* O */
    {30,17,17,30,16,16,16}, /* P */
    {14,17,17,17,21,18,13}, /* Q */
    {30,17,17,30,20,18,17}, /* R */
    {15,16,16,14,1,1,30},   /* S */
    {31,4,4,4,4,4,4},        /* T */
    {17,17,17,17,17,17,14}, /* U */
    {17,17,17,17,17,10,4},  /* V */
    {17,17,17,21,21,21,10}, /* W */
    {17,17,10,4,10,17,17},  /* X */
    {17,17,10,4,4,4,4},     /* Y */
    {31,1,2,4,8,16,31},    /* Z */
    {14,17,19,21,25,17,14}, /* 0 */
    {4,12,4,4,4,4,14},       /* 1 */
    {14,17,1,2,4,8,31},      /* 2 */
    {30,1,1,14,1,1,30},      /* 3 */
    {2,6,10,18,31,2,2},     /* 4 */
    {31,16,16,30,1,1,30},   /* 5 */
    {14,16,16,30,17,17,14}, /* 6 */
    {31,1,2,4,8,8,8},       /* 7 */
    {14,17,17,14,17,17,14}, /* 8 */
    {14,17,17,15,1,1,14},   /* 9 */
};
static void trainer_text(int x, int y, const char *text, int scale) {
    for (; *text; ++text, x += 6 * scale) {
        int g = (*text >= 'A' && *text <= 'Z') ? *text - 'A' :
                (*text >= '0' && *text <= '9') ? 26 + *text - '0' : -1;
        if (g < 0) continue;
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (trainer_glyphs[g][row] & (1 << (4-col))) {
                    SDL_Rect p = {x + col*scale, y + row*scale, scale, scale};
                    SDL_RenderFillRect(renderer, &p);
                }
    }
}
static void trainer_overlay(int width, int height) {
    if (!eb_switch_menu_open) return;
    int scale = width >= 1000 ? 3 : 2;
    int panel_w = 245*scale, panel_h = 137*scale;
    int left = (width-panel_w)/2, top = (height-panel_h)/2;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 12, 12, 30, 236);
    SDL_Rect panel = {left, top, panel_w, panel_h};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderDrawRect(renderer, &panel);
    trainer_text(left+8*scale, top+6*scale, "EARTHBOUND MOD MENU", scale);
    SDL_SetRenderDrawColor(renderer, 110, 205, 255, 255);
    trainer_text(left+8*scale, top+17*scale, "NATIVE SWITCH EDITION", scale);
    for (int i = 0; i < 9; ++i) {
        int y = top+(31+i*10)*scale;
        if (i == eb_switch_menu_selected) {
            SDL_SetRenderDrawColor(renderer, 40, 95, 160, 255);
            SDL_Rect highlight = {left+5*scale, y-1*scale, panel_w-10*scale, 9*scale};
            SDL_RenderFillRect(renderer, &highlight);
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        trainer_text(left+10*scale, y, trainer_items[i], scale);
        if (i == 0) {
            static const char *modes[] = {"NEAREST", "INTEGER", "LINEAR"};
            trainer_text(left+145*scale, y, modes[(eb_switch_menu_scale+3)%3], scale);
        } else if (i == 1 || i == 2) {
            trainer_text(left+145*scale, y,
                (i == 1 ? eb_switch_menu_instant_text : eb_switch_menu_auto_heal)
                ? "ON" : "OFF", scale);
        }
    }
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    trainer_text(left+8*scale, top+124*scale, "A SELECT  B BACK", scale);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}


/* Called by the existing Switch input menu; keep presentation state local
 * to the video backend instead of leaving an unresolved external symbol. */
void eb_switch_video_set_scale(int mode) {
    switch (mode) {
    case 1: scale_mode = EB_SCALE_INTEGER; break;
    case 2: scale_mode = EB_SCALE_ASPECT_LINEAR; break;
    default: scale_mode = EB_SCALE_ASPECT_NEAREST; break;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
                scale_mode == EB_SCALE_ASPECT_LINEAR ? "linear" : "nearest");
}

static void read_presentation_settings(void) {
    FILE *f=fopen("/switch/earthbound-switch/scaling.cfg","r");
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
        memcpy(frame + (size_t)y * (size_t)EB_VIEWPORT_WIDTH, pixels,
               (size_t)EB_VIEWPORT_WIDTH * sizeof(pixel_t));
}
pixel_t *platform_video_get_framebuffer(void) { return frame; }
void platform_video_end_frame(void) {
    if(!renderer || !frame_texture)return;
    eb_switch_scene_art_frame(frame,EB_VIEWPORT_WIDTH,EB_VIEWPORT_HEIGHT);
    /* SDL_UpdateTexture pitch is in BYTES, not pixels: 400 * 2 = 800. */
    const int stride_bytes = EB_VIEWPORT_WIDTH * (int)sizeof(pixel_t);
    if(SDL_UpdateTexture(frame_texture,NULL,frame,stride_bytes)!=0)return;
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
    trainer_overlay(width,height);
    SDL_RenderPresent(renderer);
}
void platform_video_set_vsync(bool enabled) {
    /* SDL2 renderer vsync is selected at creation; preserve API contract. */
    use_vsync=enabled;
    (void)use_vsync;
}
