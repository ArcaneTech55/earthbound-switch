/* Companion Switch: original console-style overlay.
 * Visual reference: quiet white focus bar, blurred gameplay backdrop,
 * slim rule, translucent slot cards. Not an official NSO interface.
 * All cards are genuine controls, not pretend save/rewind slots.
 */
#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "game/settings.h"
extern bool eb_switch_menu_open;
extern int eb_switch_menu_selected;
extern int eb_switch_menu_scale;
extern bool eb_switch_menu_fast_forward;
extern int companion_hub_slot;
extern int companion_rewind_available(void);
extern bool companion_suspend_slot_exists(int);
extern bool companion_suspend_preview(int,uint16_t*);
extern char companion_hub_status[48];
static SDL_Texture *preview_textures[4];
static bool preview_checked[4];
static uint16_t preview_pixels[160*90];
void companion_hub_invalidate_previews(void) {
    for(int i=0;i<4;i++) {
        if(preview_textures[i])SDL_DestroyTexture(preview_textures[i]);
        preview_textures[i]=NULL;preview_checked[i]=false;
    }
}
static SDL_Texture *load_preview(SDL_Renderer *r,int slot){
    if(slot<0||slot>=4)return NULL;
    if(preview_checked[slot])return preview_textures[slot];
    preview_checked[slot]=true;
    if(!companion_suspend_preview(slot,preview_pixels))return NULL;
    SDL_Texture *t=SDL_CreateTexture(r,SDL_PIXELFORMAT_BGR565,
                                   SDL_TEXTUREACCESS_STATIC,160,90);
    if(!t)return NULL;
    if(SDL_UpdateTexture(t,NULL,preview_pixels,160*(int)sizeof(uint16_t))!=0){
        SDL_DestroyTexture(t);return NULL;
    }
    SDL_SetTextureScaleMode(t,SDL_ScaleModeLinear);
    preview_textures[slot]=t;
    return t;
}

typedef struct { char c; unsigned char row[7]; } Glyph;
static const Glyph glyphs[]={
{ '0', {14,19,21,21,25,17,14} },
{ '1', {4,12,4,4,4,4,14} },
{ '2', {14,17,1,2,4,8,31} },
{ '3', {30,1,1,14,1,1,30} },
{ 'A', {14,17,17,31,17,17,17} },
{ 'B', {30,17,17,30,17,17,30} },
{ 'C', {15,16,16,16,16,16,15} },
{ 'D', {30,17,17,17,17,17,30} },
{ 'E', {31,16,16,30,16,16,31} },
{ 'F', {31,16,16,30,16,16,16} },
{ 'G', {15,16,16,23,17,17,15} },
{ 'H', {17,17,17,31,17,17,17} },
{ 'I', {31,4,4,4,4,4,31} },
{ 'J', {7,2,2,2,18,18,12} },
{ 'K', {17,18,20,24,20,18,17} },
{ 'L', {16,16,16,16,16,16,31} },
{ 'M', {17,27,21,21,17,17,17} },
{ 'N', {17,25,21,19,17,17,17} },
{ 'O', {14,17,17,17,17,17,14} },
{ 'P', {30,17,17,30,16,16,16} },
{ 'Q', {14,17,17,17,21,18,13} },
{ 'R', {30,17,17,30,20,18,17} },
{ 'S', {15,16,16,14,1,1,30} },
{ 'T', {31,4,4,4,4,4,4} },
{ 'U', {17,17,17,17,17,17,14} },
{ 'V', {17,17,17,17,17,10,4} },
{ 'W', {17,17,17,21,21,21,10} },
{ 'X', {17,17,10,4,10,17,17} },
{ 'Y', {17,17,10,4,4,4,4} },
{ 'Z', {31,1,2,4,8,16,31} },
{ ':', {0,4,4,0,4,4,0} },
{ '/', {1,1,2,4,8,16,16} },
{ '+', {0,4,4,31,4,4,0} },
{ '-', {0,0,0,31,0,0,0} },
{ '.', {0,0,0,0,0,12,12} },
{ ' ', {0,0,0,0,0,0,0} }
};

static const SDL_Color white={250,250,250,255};
static const SDL_Color muted={215,216,218,255};
static const SDL_Color faded={180,184,189,255};
static SDL_Texture *backdrop;
static SDL_Renderer *backdrop_renderer;
#define BLUR_W 160
#define BLUR_H 90
static uint16_t blurred[BLUR_W * BLUR_H];

static void rect(SDL_Renderer*r,int x,int y,int w,int h,SDL_Color c) {
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    SDL_Rect a={x,y,w,h};
    SDL_RenderFillRect(r,&a);
}
static void outline(SDL_Renderer*r,int x,int y,int w,int h,SDL_Color c) {
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    SDL_Rect a={x,y,w,h};
    SDL_RenderDrawRect(r,&a);
}
static const unsigned char *get_glyph(char c) {
    if(c>='a'&&c<='z')c=(char)(c-'a'+'A');
    for(unsigned i=0;i<sizeof(glyphs)/sizeof(glyphs[0]);++i)
        if(glyphs[i].c==c)return glyphs[i].row;
    return glyphs[sizeof(glyphs)/sizeof(glyphs[0])-1].row;
}
static void text(SDL_Renderer*r,int x,int y,int size,const char*s,SDL_Color color) {
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
    for(;*s;s++,x+=size*6){
        const unsigned char *g=get_glyph(*s);
        for(int row=0;row<7;row++)for(int col=0;col<5;col++)
            if(g[row] & (1u<<(4-col))) {
                SDL_Rect p={x+col*size,y+row*size,size,size};
                SDL_RenderFillRect(r,&p);
            }
    }
}
/* Actual blurred game image, generated from the PPU framebuffer. A low-res
 * multi-tap box filter avoids depending on Switch SDL render targets and
 * doesn't allocate a full-size intermediate surface every frame. */
static void update_background(SDL_Renderer*r,const uint16_t *frame,int w,int h) {
    if(!frame||w<1||h<1)return;
    if(backdrop_renderer!=r) {
        if(backdrop)SDL_DestroyTexture(backdrop);
        backdrop=NULL;
        backdrop_renderer=r;
    }
    if(!backdrop){
        backdrop=SDL_CreateTexture(r,SDL_PIXELFORMAT_BGR565,
                                  SDL_TEXTUREACCESS_STREAMING,BLUR_W,BLUR_H);
        if(!backdrop)return;
        SDL_SetTextureScaleMode(backdrop,SDL_ScaleModeLinear);
    }
    for(int y=0;y<BLUR_H;y++){
        int cy=(y*h+h/2)/BLUR_H;
        for(int x=0;x<BLUR_W;x++){
            int cx=(x*w+w/2)/BLUR_W;
            unsigned rr=0,gg=0,bb=0;
            for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++){
                int sy=cy+dy*3, sx=cx+dx*3;
                if(sy<0)sy=0;else if(sy>=h)sy=h-1;
                if(sx<0)sx=0;else if(sx>=w)sx=w-1;
                uint16_t v=frame[(size_t)sy*w+sx];
                bb+=(v>>11)&31u;
                gg+=(v>>5)&63u;
                rr+=v&31u;
            }
            blurred[y*BLUR_W+x]=(uint16_t)(((bb/25)<<11)|((gg/25)<<5)|(rr/25));
        }
    }
    SDL_UpdateTexture(backdrop,NULL,blurred,BLUR_W*(int)sizeof(uint16_t));
}
void companion_hub_draw(SDL_Renderer *r,int width,int height,
                        const uint16_t *frame,int frame_width,int frame_height){
    if(!eb_switch_menu_open||!r||width<480||height<300)return;
    SDL_BlendMode old_blend;SDL_GetRenderDrawBlendMode(r,&old_blend);
    SDL_Rect old_view;SDL_RenderGetViewport(r,&old_view);
    float old_x,old_y;SDL_RenderGetScale(r,&old_x,&old_y);

    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    update_background(r,frame,frame_width,frame_height);
    if(backdrop) {
        SDL_Rect full={0,0,width,height};
        SDL_RenderCopy(r,backdrop,NULL,&full);
    }
    rect(r,0,0,width,height,(SDL_Color){0,0,0,125});

    /* Reference screenshot composition: left list, central divider,
     * four fine-bordered cards, and quiet control hints along the foot. */
    float scale=(float)width/960.0f;
    if((float)height/540.0f<scale)scale=(float)height/540.0f;
    int vw=(int)(960*scale),vh=(int)(540*scale);
    SDL_Rect view={(width-vw)/2,(height-vh)/2,vw,vh};
    SDL_RenderSetViewport(r,&view);
    SDL_RenderSetScale(r,scale,scale);

    /* Minimal profile marker without fake Nintendo user account artwork. */
    rect(r,18,24,26,26,(SDL_Color){227,229,231,255});
    text(r,25,30,2,"P", (SDL_Color){62,66,73,255});
    text(r,52,25,2,"P1",white);
    text(r,52,43,1,"EARTHBOUND COMPANION",muted);

    rect(r,471,36,1,445,(SDL_Color){217,217,217,165});
    const char *options[8]={
        "CONTINUE",
        "CREATE POINT",
        "LOAD POINT",
        "REWIND 3 SEC",
        "SMOOTH PIXELS",
        "SCANLINES",
        "SCALE MODE",
        "CLOSE MENU"
    };
    for(int i=0;i<8;i++) {
        int y=150+i*39;
        if(eb_switch_menu_selected==i) {
            rect(r,114,y-9,357,34,(SDL_Color){249,249,249,255});
            text(r,152,y,2,options[i],(SDL_Color){69,72,76,255});
        } else {
            text(r,152,y,2,options[i],white);
        }
    }
    /* Right-side cards are real suspend thumbnails. The selected slot is
     * highlighted; empty slots have a calm blank placeholder. */
    for(int i=0;i<4;i++) {
        int y=71+i*101;
        bool selected=companion_hub_slot==i;
        outline(r,518,y,320,81,
                selected?(SDL_Color){255,255,255,255}:(SDL_Color){225,225,225,160});
        rect(r,520,y+2,101,77,(SDL_Color){20,24,30,180});
        SDL_Texture *preview=load_preview(r,i);
        if(preview) {
            SDL_Rect area={522,y+12,97,55};
            SDL_RenderCopy(r,preview,NULL,&area);
        } else {
            text(r,528,y+31,2,"EMPTY",faded);
        }
        text(r,640,y+14,2,i==0?"POINT 1":i==1?"POINT 2":i==2?"POINT 3":"POINT 4",white);
        text(r,640,y+44,2,companion_suspend_slot_exists(i)?"SAVED":"NO SNAPSHOT",muted);
        if(selected)rect(r,831,y,7,81,(SDL_Color){255,255,255,255});
    }
    text(r,535,459,2,"LEFT / RIGHT CHOOSE POINT",muted);
    if(companion_hub_status[0])
        text(r,535,480,2,companion_hub_status,white);
    else if(companion_rewind_available()>0)
        text(r,535,480,2,"REWIND READY",muted);
    /* Rounded controller symbols aren't part of the engine's tiny bitmap
     * font; neutral square keycaps give a similar understated console vibe. */
    rect(r,632,498,23,23,(SDL_Color){251,251,251,240});
    text(r,637,503,2,"B",(SDL_Color){33,35,39,255});
    text(r,662,503,2,"BACK",white);
    rect(r,763,498,23,23,(SDL_Color){251,251,251,240});
    text(r,768,503,2,"A",(SDL_Color){33,35,39,255});
    text(r,793,503,2,"OK",white);

    SDL_RenderSetScale(r,old_x,old_y);
    SDL_RenderSetViewport(r,&old_view);
    SDL_SetRenderDrawBlendMode(r,old_blend);
}
