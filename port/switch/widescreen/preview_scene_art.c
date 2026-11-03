/* Generates portable PPM previews of ORIGINAL procedural art in widescreen
 * gutters; the central band is a neutral placeholder, not EarthBound assets.
 * This preview deliberately contains NO copyrighted game graphics. */
#include "scene_art.h"
#include <stdio.h>
#include <stdint.h>
enum { W=400, H=225 };
static pixel_t frame[W*H];
static int save_preview(const char *path,EbSwitchScene scene) {
    FILE *f=fopen(path,"wb");
    if(!f)return 1;
    for(int y=0;y<H;y++)
        for(int x=0;x<W;x++) {
            int c=(x/8+y/8)%2 ? 14:18;
            frame[y*W+x]=PIXEL_RGB(c,c,c+4);
        }
    eb_switch_scene_set(scene);
    eb_switch_scene_art_frame(frame,W,H);
    fprintf(f,"P6\n%d %d\n255\n",W,H);
    for(int i=0;i<W*H;i++) {
        uint32_t c=pixel_to_rgb888(frame[i]);
        unsigned char rgb[3]={
            (unsigned char)(c>>16),(unsigned char)(c>>8),
            (unsigned char)c
        };
        if(fwrite(rgb,1,3,f)!=3){fclose(f);return 2;}
    }
    return fclose(f)!=0;
}
int main(void) {
    if(save_preview("title_art.ppm",EB_SWITCH_SCENE_TITLE))return 1;
    if(save_preview("battle_art.ppm",EB_SWITCH_SCENE_BATTLE))return 2;
    if(save_preview("cinematic_art.ppm",EB_SWITCH_SCENE_CINEMATIC))return 3;
    return 0;
}
