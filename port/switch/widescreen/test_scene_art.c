#include <assert.h>
#include <string.h>
#include "scene_art.h"
enum { W=400, H=225 };
static pixel_t pixels[W*H];
static void test_scene(EbSwitchScene scene) {
    for(int i=0;i<W*H;i++)pixels[i]=PIXEL_RGB(255,0,255);
    eb_switch_scene_set(scene);
    eb_switch_scene_art_frame(pixels,W,H);
    /* Central 256px SNES composition stays byte-for-byte untouched. */
    for(int y=0;y<H;y++)
        for(int x=72;x<328;x++)
            assert(pixels[y*W+x]==PIXEL_RGB(255,0,255));
    /* Both new gutters contain actual generated pixel art. */
    assert(pixels[0]!=PIXEL_RGB(255,0,255));
    assert(pixels[W-1]!=PIXEL_RGB(255,0,255));
}
int main(void) {
    test_scene(EB_SWITCH_SCENE_TITLE);
    test_scene(EB_SWITCH_SCENE_BATTLE);
    test_scene(EB_SWITCH_SCENE_CINEMATIC);
    eb_switch_scene_set(EB_SWITCH_SCENE_NONE);
    assert(eb_switch_scene_get()==EB_SWITCH_SCENE_NONE);
    return 0;
}
