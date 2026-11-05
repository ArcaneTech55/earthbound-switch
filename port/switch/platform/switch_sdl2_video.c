#include "platform/platform.h"
#include "../widescreen/layout.h"
#include "../widescreen/scaling.h"
#include "../widescreen/scene_art.h"
#include <stdio.h>
#include <SDL.h>
#include <string.h>
#include "game/game_state.h"

extern bool eb_switch_menu_open;
extern int eb_switch_menu_selected;
extern int eb_switch_menu_character;
extern int eb_switch_menu_item;
extern int eb_switch_menu_scale;
extern bool eb_switch_menu_instant_text;
extern bool eb_switch_menu_auto_heal;

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
/* Tiny embedded 5x7 uppercase font: avoids SDL_ttf and external assets.
 * Each glyph is five columns, bit 0 at the top. */
static const char *menu_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:+-/<>. ";
static const unsigned char menu_glyphs[][5] = {
 {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},
 {0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},
 {0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},
 {0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},
 {0x41,0x41,0x7f,0x41,0x41},{0x20,0x40,0x41,0x3f,0x01},
 {0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
 {0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},
 {0x3e,0x41,0x41,0x41,0x3e},{0x7f,0x09,0x09,0x09,0x06},
 {0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},
 {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},
 {0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},
 {0x7f,0x20,0x18,0x20,0x7f},{0x63,0x14,0x08,0x14,0x63},
 {0x03,0x04,0x78,0x04,0x03},{0x61,0x51,0x49,0x45,0x43},
 {0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},
 {0x62,0x51,0x49,0x49,0x46},{0x22,0x41,0x49,0x49,0x36},
 {0x18,0x14,0x12,0x7f,0x10},{0x2f,0x49,0x49,0x49,0x31},
 {0x3e,0x49,0x49,0x49,0x32},{0x01,0x71,0x09,0x05,0x03},
 {0x36,0x49,0x49,0x49,0x36},{0x26,0x49,0x49,0x49,0x3e},
 {0x00,0x36,0x36,0x00,0x00},{0x08,0x08,0x2a,0x1c,0x08},
 {0x08,0x14,0x22,0x41,0x00},{0x00,0x00,0x7f,0x00,0x00},
 {0x00,0x41,0x22,0x14,0x08},{0x08,0x08,0x08,0x08,0x08},
 {0x00,0x60,0x60,0x00,0x00},{0x00,0x00,0x00,0x00,0x00}
};
static void menu_text(int x,int y,const char *text,int scale) {
    for (; *text; ++text, x += 6 * scale) {
        char c = *text;
        if (c >= 'a' && c <= 'z') c -= 32;
        const char *pos = strchr(menu_alphabet, c);
        if (!pos) continue;
        const unsigned char *glyph = menu_glyphs[pos-menu_alphabet];
        for (int col=0;col<5;col++)
            for (int row=0;row<7;row++)
                if (glyph[col] & (1u<<row)) {
                    SDL_Rect dot={x+col*scale,y+row*scale,scale,scale};
                    SDL_RenderFillRect(renderer,&dot);
                }
    }
}
static void menu_overlay(int width,int height) {
    if (!eb_switch_menu_open) return;
    const int scale=2;
    const int x=width/2-260,y=height/2-285;
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,8,9,29,242);
    SDL_Rect bg={x-20,y-20,520,570};
    SDL_RenderFillRect(renderer,&bg);
    SDL_SetRenderDrawColor(renderer,255,214,97,255);
    SDL_RenderDrawRect(renderer,&bg);
    menu_text(x,y,"EARTHBOUND MOD MENU",scale);
    SDL_SetRenderDrawColor(renderer,224,233,255,255);
    menu_text(x,y+25,"ZL+PLUS TO CLOSE",scale);
    static const char *names[]={
        "CHARACTER","SCALING","INSTANT TEXT","AUTO HEAL",
        "HEAL PARTY","ADD 1000 CASH","ADD 1000 BANK",
        "MAX HP +/-10","MAX PP +/-10","LEVEL +/-1",
        "ITEM ID","GIVE ITEM","SPEED +/-1","GUTS +/-1","CLOSE"
    };
    char line[60];
    for(int i=0;i<15;i++) {
        int yy=y+61+i*29;
        if (i==eb_switch_menu_selected) {
            SDL_SetRenderDrawColor(renderer,65,75,125,255);
            SDL_Rect selected={x-7,yy-4,474,26};
            SDL_RenderFillRect(renderer,&selected);
        }
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        snprintf(line,sizeof(line),"%s",names[i]);
        if (i==0) snprintf(line,sizeof(line),"CHARACTER %d",eb_switch_menu_character+1);
        if (i==1) snprintf(line,sizeof(line),"SCALING %d",eb_switch_menu_scale);
        if (i==2) snprintf(line,sizeof(line),"INSTANT TEXT %s",eb_switch_menu_instant_text?"ON":"OFF");
        if (i==3) snprintf(line,sizeof(line),"AUTO HEAL %s",eb_switch_menu_auto_heal?"ON":"OFF");
        if (i==7) snprintf(line,sizeof(line),"MAX HP %u",party_characters[eb_switch_menu_character].max_hp);
        if (i==8) snprintf(line,sizeof(line),"MAX PP %u",party_characters[eb_switch_menu_character].max_pp);
        if (i==9) snprintf(line,sizeof(line),"LEVEL %u",party_characters[eb_switch_menu_character].level);
        if (i==10) snprintf(line,sizeof(line),"ITEM ID %d",eb_switch_menu_item);
        if (i==12) snprintf(line,sizeof(line),"SPEED %u",party_characters[eb_switch_menu_character].base_speed);
        if (i==13) snprintf(line,sizeof(line),"GUTS %u",party_characters[eb_switch_menu_character].base_guts);
        menu_text(x,yy,line,scale);
    }
    SDL_SetRenderDrawColor(renderer,255,214,97,255);
    menu_text(x,y+505,"DPAD MOVE   A SELECT   B BACK",scale);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
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
    menu_overlay(width,height);
    SDL_RenderPresent(renderer);
}
void platform_video_set_vsync(bool enabled) {
    /* SDL2 renderer vsync is selected at creation; preserve API contract. */
    use_vsync=enabled;
    (void)use_vsync;
}
