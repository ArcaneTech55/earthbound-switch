#include <assert.h>
#include "layout.h"
int main(void) {
    EbWsSize screen=eb_ws_logical_size(), out=eb_ws_output_size();
    assert(screen.width*9==screen.height*16);
    assert(out.width==1280 && out.height==720);
    EbWsRect r=eb_ws_fit(screen,out);
    assert(r.x==0 && r.y==0 && r.width==1280 && r.height==720);
    r=eb_ws_anchor(screen,(EbWsSize){100,30},EB_WS_CENTER,0);
    assert(r.x==150 && r.y==97);
    r=eb_ws_anchor(screen,(EbWsSize){380,50},EB_WS_BOTTOM,8);
    assert(r.y==167);
    r=eb_ws_anchor(screen,(EbWsSize){70,100},EB_WS_RIGHT,12);
    assert(r.x==318);
    assert(eb_ws_floor_div(-1,8)==-1 && eb_ws_floor_div(-9,8)==-2);
    assert(eb_ws_floor_div(16,8)==2);
    assert(eb_ws_fit((EbWsSize){0,1},out).width==0);
    return 0;
}
