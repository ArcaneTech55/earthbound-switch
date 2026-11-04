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
extern bool eb_switch_menu_open;
extern int eb_switch_menu_selected;
extern bool eb_switch_menu_instant_text;
extern bool eb_switch_menu_auto_heal;
extern int eb_switch_menu_scale;
static bool use_vsync=true;
static EbScaleMode scale_mode=EB_SCALE_ASPECT_NEAREST;

static void read_presentation_settings(void) {
    FILE *f=fopen("/switch/earthbound-switch/scaling.cfg","r");
    if(!f)return;
    char mode[32]={0};
    if(fgets(mode,sizeof(mode),f)) {
        mode[strcspn(mode,"\r\n")]=0;
        scale_mode=eb_scale_mode_parse(mode);
        eb_switch_menu_scale = scale_mode == EB_SCALE_INTEGER ? 1 :
                               scale_mode == EB_SCALE_ASPECT_LINEAR ? 2 : 0;
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
void eb_switch_video_set_scale(int mode) {
    if(mode<0 || mode>2) return;
    scale_mode = mode == 1 ? EB_SCALE_INTEGER :
                 mode == 2 ? EB_SCALE_ASPECT_LINEAR : EB_SCALE_ASPECT_NEAREST;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
                mode == 2 ? "linear" : "nearest");
    if(frame_texture) SDL_SetTextureScaleMode(frame_texture,
                mode == 2 ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
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

/* Switch-local, font-independent overlay. Five-by-seven pixel glyphs are drawn
 * through SDL; no OS font, extra portlib or asset dependency is required. */
static const char *glyph(char c) {
    static const struct { char c; const char *p; } font[] = {
        {'A',"01110100011000111111100011000110001"},
        {'B',"11110100011000111110100011000111110"},
        {'C',"01111100001000010000100001000001111"},
        {'D',"11110100011000110001100011000111110"},
        {'E',"11111100001000011110100001000011111"},
        {'F',"11111100001000011110100001000010000"},
        {'G',"01111100001000010111100011000101111"},
        {'H',"10001100011000111111100011000110001"},
        {'I',"11111001000010000100001000010011111"},
        {'J',"00111000100001000010100101001001100"},
        {'K',"10001100101010011000101001001010001"},
        {'L',"10000100001000010000100001000011111"},
        {'M',"10001110111010110101100011000110001"},
        {'N',"10001110011010110011100011000110001"},
        {'O',"01110100011000110001100011000101110"},
        {'P',"11110100011000111110100001000010000"},
        {'Q',"01110100011000110001101011001001101"},
        {'R',"11110100011000111110101001001010001"},
        {'S',"01111100001000001110000010000111110"},
        {'T',"11111001000010000100001000010000100"},
        {'U',"10001100011000110001100011000101110"},
        {'V',"10001100011000110001100010101000100"},
        {'W',"10001100011000110101101011101110001"},
        {'X',"10001100010101000100010101000110001"},
        {'Y',"10001100010101000100001000010000100"},
        {'Z',"11111000010001000100010001000011111"},
        {'0',"01110100011001110101110011000101110"},
        {'1',"00100011000010000100001000010001110"},
        {'2',"01110100010000100010001000100011111"},
        {'3',"11110000010000101110000010000111110"},
        {'4',"00010001100101010010111110001000010"},
        {'5',"11111100001000011110000010000111110"},
        {'6',"01110100001000011110100011000101110"},
        {'7',"11111000010001000100010000100001000"},
        {'8',"01110100011000101110100011000101110"},
        {'9',"01110100011000101111000010000101110"},
        {' ',"00000000000000000000000000000000000"},
        {'-',"00000000000000011111000000000000000"},
        {':',"00000001000010000000001000010000000"},
        {'/',"00001000010001000100010001000000000"},
        {'+', "00000001000010011111001000010000000"},
        {'
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
    if(eb_switch_menu_open) draw_menu(width,height);
    SDL_RenderPresent(renderer);
}
void platform_video_set_vsync(bool enabled) {
    /* SDL2 renderer vsync is selected at creation; preserve API contract. */
    use_vsync=enabled;
    (void)use_vsync;
}
,"00100011111010001110001011111000100"},
        {'.',"00000000000000000000000000110001100"},
        {'>',"10000010000010000010001001000000000"},
    };
    for(unsigned i=0; i<sizeof(font)/sizeof(font[0]); ++i)
        if(font[i].c==c) return font[i].p;
    return font[sizeof(font)/sizeof(font[0])-1].p;
}
static void draw_text(int x,int y,const char *text,int px,SDL_Color c) {
    SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,255);
    while(*text) {
        const char *bitmap=glyph(*text++);
        for(int row=0;row<7;++row)
            for(int col=0;col<5;++col)
                if(bitmap[row*5+col]=='1') {
                    SDL_Rect rect={x+col*px,y+row*px,px,px};
                    SDL_RenderFillRect(renderer,&rect);
                }
        x+=6*px;
    }
}
static void fill_rect(int x,int y,int w,int h, SDL_Color c) {
    SDL_Rect r={x,y,w,h};
    SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);
    SDL_RenderFillRect(renderer,&r);
}
static void draw_menu(int w,int h) {
    /* Scale UI relative to actual output size; logical gameplay size unchanged. */
    SDL_RenderSetScale(renderer,(float)w/1280.0f,(float)h/720.0f);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    fill_rect(0,0,1280,720,(SDL_Color){7,13,24,202});
    fill_rect(218,69,844,585,(SDL_Color){17,25,37,255});
    fill_rect(218,69,844,7,(SDL_Color){130,204,189,255});
    draw_text(272,115,"EARTHBOUND",4,(SDL_Color){245,249,247,255});
    draw_text(276,157,"SETTINGS + CHEATS",2,(SDL_Color){135,158,178,255});
    fill_rect(265,206,747,1,(SDL_Color){62,76,88,255});
    const char *names[]={"DISPLAY SCALING","QUICK TEXT","AUTO HEAL","HEAL PARTY NOW","ADD $1000","RETURN TO GAME"};
    const char *scale[]={"NEAREST","INTEGER","LINEAR"};
    for(int i=0;i<6;++i) {
        int y=225+i*58;
        if(i==eb_switch_menu_selected) {
            fill_rect(260,y,758,49,(SDL_Color){50,92,107,255});
            fill_rect(260,y,5,49,(SDL_Color){136,226,198,255});
            draw_text(280,y+16,">",2,(SDL_Color){174,239,207,255});
        }
        draw_text(311,y+15,names[i],2,(SDL_Color){244,246,245,255});
        if(i==0) draw_text(820,y+15,scale[eb_switch_menu_scale%3],2,
                              (SDL_Color){173,225,206,255});
        if(i==1 || i==2)
            draw_text(891,y+15,((i==1?eb_switch_menu_instant_text:eb_switch_menu_auto_heal)?"ON":"OFF"),2,
                      (SDL_Color){173,225,206,255});
    }
    fill_rect(265,590,747,1,(SDL_Color){62,76,88,255});
    draw_text(281,607,"DPAD  SELECT   A  CHANGE   B  CLOSE",2,(SDL_Color){170,188,201,255});
    SDL_RenderSetScale(renderer,1.0f,1.0f);
}

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
    SDL_RenderPresent(renderer);
}
void platform_video_set_vsync(bool enabled) {
    /* SDL2 renderer vsync is selected at creation; preserve API contract. */
    use_vsync=enabled;
    (void)use_vsync;
}
