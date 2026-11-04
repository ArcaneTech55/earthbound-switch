/* Original Companion Hub overlay. SDL-only, no Nintendo artwork/fonts. */
#include <SDL.h>
#include <stdbool.h>
#include <string.h>
#include "game/settings.h"

extern bool eb_switch_menu_open;
extern int eb_switch_menu_selected;
extern int eb_switch_menu_scale;
extern bool eb_switch_menu_fast_forward;

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
static SDL_Color fg={239,244,247,255}, dim={162,179,190,255}, accent={98,224,205,255};
static void fill(SDL_Renderer *r,int x,int y,int w,int h,SDL_Color c){
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    SDL_Rect q={x,y,w,h};SDL_RenderFillRect(r,&q);
}
static void stroke(SDL_Renderer *r,int x,int y,int w,int h,SDL_Color c){
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    SDL_Rect q={x,y,w,h};SDL_RenderDrawRect(r,&q);
}
static const unsigned char *glyph(char c){
    if(c>='a'&&c<='z')c=(char)(c-'a'+'A');
    for(unsigned i=0;i<sizeof(glyphs)/sizeof(glyphs[0]);i++)
        if(glyphs[i].c==c)return glyphs[i].row;
    return glyphs[sizeof(glyphs)/sizeof(glyphs[0])-1].row;
}
static void label(SDL_Renderer*r,int x,int y,int scale,const char*s,SDL_Color color){
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
    for(;*s;s++,x+=6*scale){
        const unsigned char *g=glyph(*s);
        for(int row=0;row<7;row++)for(int col=0;col<5;col++)
            if(g[row]&(1u<<(4-col))){
                SDL_Rect pixel={x+col*scale,y+row*scale,scale,scale};
                SDL_RenderFillRect(r,&pixel);
            }
    }
}
static void detail(SDL_Renderer*r,int x,int y,const char*name,const char*value){
    label(r,x,y,2,name,dim);
    label(r,x,y+29,2,value,fg);
}
void companion_hub_draw(SDL_Renderer*r,int width,int height){
    if(!eb_switch_menu_open || !r || width<480 || height<300)return;
    SDL_BlendMode old;SDL_GetRenderDrawBlendMode(r,&old);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    fill(r,0,0,width,height,(SDL_Color){5,12,22,205});
    /* Coordinates in a virtual 960x540 layout, centered and scaled. */
    float scale=(float)width/960.0f;
    float sy=(float)height/540.0f;
    if(sy<scale)scale=sy;
    int tw=(int)(960*scale),th=(int)(540*scale);
    int ox=(width-tw)/2,oy=(height-th)/2;
    SDL_Rect viewport={ox,oy,tw,th};
    SDL_RenderSetViewport(r,&viewport);
    SDL_RenderSetScale(r,scale,scale);
    fill(r,32,32,896,476,(SDL_Color){18,29,42,248});
    fill(r,32,32,896,5,accent);
    label(r,65,62,4,"COMPANION HUB",fg);
    label(r,66,104,2,"EARTHBOUND / SWITCH",dim);
    fill(r,64,140,832,1,(SDL_Color){70,91,102,255});
    const char*names[6]={"RESUME GAME","SMOOTH PIXELS","SCANLINES","SCALE MODE","FAST FORWARD","CLOSE HUB"};
    int ys[6]={172,220,268,316,364,412};
    for(int i=0;i<6;i++){
        if(eb_switch_menu_selected==i){
            fill(r,61,ys[i]-10,401,41,(SDL_Color){61,85,104,255});
            fill(r,61,ys[i]-10,4,41,accent);
        }
        label(r,81,ys[i],2,names[i],i==eb_switch_menu_selected?fg:dim);
    }
    fill(r,492,161,1,296,(SDL_Color){70,91,102,255});
    label(r,525,171,3,"DISPLAY",accent);
    detail(r,528,222,"PIXEL FILTER",engine_fx_antialiasing?"SOFT":"SHARP");
    detail(r,528,294,"SCANLINES",engine_fx_scanlines?"ON":"OFF");
    detail(r,528,366,"LAYOUT",eb_switch_menu_scale==1?"INTEGER":eb_switch_menu_scale==2?"LINEAR":"ASPECT");
    detail(r,528,438,"SPEED",eb_switch_menu_fast_forward?"FAST":"NORMAL");
    fill(r,64,470,832,1,(SDL_Color){70,91,102,255});
    label(r,70,485,2,"A SELECT   B BACK   ZL + PLUS MENU",dim);
    SDL_RenderSetScale(r,1.0f,1.0f);
    SDL_RenderSetViewport(r,NULL);
    SDL_SetRenderDrawBlendMode(r,old);
}
