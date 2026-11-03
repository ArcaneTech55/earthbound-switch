#include "scene_art.h"
#include <stdint.h>

/* Original procedural 16:9 art for the new horizontal pixels. These are new
 * scenery/effect pixels, not mirrored or scaled copies of the 256px original.
 * The pixel-art overlay leaves the original 256px picture untouched.
 * Scene-by-scene artwork in the original SNES band remains to be reauthored. */
static EbSwitchScene active_scene;
static uint32_t animation_frame;

void eb_switch_scene_set(EbSwitchScene scene) {
    if (active_scene != scene) animation_frame=0;
    active_scene=scene;
}
EbSwitchScene eb_switch_scene_get(void) { return active_scene; }

static uint32_t noise2(int x,int y,uint32_t salt) {
    uint32_t h=(uint32_t)x*0x9e3779b9u ^ (uint32_t)y*0x85ebca6bu ^ salt;
    h^=h>>16; h*=0x7feb352du; h^=h>>15; h*=0x846ca68bu; h^=h>>16;
    return h;
}
static int triangle(int n,int period) {
    int t=n%period;
    if(t<0)t+=period;
    return t<period/2?t:period-t;
}
static pixel_t starfield(int x,int y) {
    /* Irregular nebula patches + sparse deterministic stars in deep space. */
    uint32_t n=noise2(x/12,y/10,0x32C0FFEEu);
    int glow=(int)(n%19u);
    int r=4+glow/3,g=5+glow/3,b=18+glow;
    uint32_t star=noise2(x,y,0xEB1994u);
    if((star&0x3ffu)==0) {
        int shine=130+(int)((star>>12)&0x7fu);
        r=shine;g=shine;b=255;
    } else if((star&0x1fffu)==17) {
        r=54;g=112;b=175;
    }
    return PIXEL_RGB(r,g,b);
}
static pixel_t battle_wave(int x,int y,uint32_t t) {
    /* New full-height plasma columns designed for the battle gutters.
     * Two phases flow in opposite directions; colors are quantized to keep
     * the original SNES/pixel-art aesthetic (no edge mirroring). */
    int a=triangle(x*3+y*2+(int)(t/2),96);
    int b=triangle(y*5-x+(int)(t/3),144);
    int v=(a*3+b*2)/5;
    int r=(v*5+48)%192+32;
    int g=(v*3+24)%112+12;
    int blue=96+(v*2)%144;
    return PIXEL_RGB(r,g,blue);
}
static pixel_t cinematic_sky(int x,int y,uint32_t t) {
    /* Composed ambient sky with a horizon and small, slow stars; intended as
     * a flexible placeholder for cinematic gutters, not finished cutscene art. */
    int horizon=(y*70)/224;
    int r=10+horizon/2,g=14+horizon/3,b=30+horizon;
    uint32_t n=noise2(x,y,0xF01A0001u);
    if((n&4095u)==(t/45u)%4096u) {
        r=180;g=190;b=210;
    }
    return PIXEL_RGB(r,g,b);
}

void eb_switch_scene_art_frame(pixel_t *frame,int width,int height) {
    if(!frame || active_scene==EB_SWITCH_SCENE_NONE ||
       width<=SNES_WIDTH || height<=0) return;
    const int left=(width-SNES_WIDTH)/2;
    const int right=left+SNES_WIDTH;
    for(int y=0;y<height;y++) {
        for(int x=0;x<width;x++) {
            if(x>=left && x<right)continue;
            pixel_t px;
            switch(active_scene) {
                case EB_SWITCH_SCENE_TITLE:px=starfield(x,y);break;
                case EB_SWITCH_SCENE_BATTLE:px=battle_wave(x,y,animation_frame);break;
                case EB_SWITCH_SCENE_CINEMATIC:
                    px=cinematic_sky(x,y,animation_frame);break;
                default:continue;
            }
            frame[(size_t)y*(size_t)width+(size_t)x]=px;
        }
    }
    animation_frame++;
}
