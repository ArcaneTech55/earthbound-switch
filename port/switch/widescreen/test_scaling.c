#include "scaling.h"
#include <assert.h>
#include <string.h>
int main(void) {
    EbWsSize widescreen={400,225}, hd={1280,720}, fullhd={1920,1080};
    EbWsRect r=eb_scale_destination(widescreen,hd,EB_SCALE_ASPECT_NEAREST);
    assert(r.x==0&&r.y==0&&r.width==1280&&r.height==720);
    r=eb_scale_destination(widescreen,hd,EB_SCALE_INTEGER);
    assert(r.x==40&&r.y==22&&r.width==1200&&r.height==675);
    r=eb_scale_destination(widescreen,fullhd,EB_SCALE_INTEGER);
    assert(r.x==160&&r.y==90&&r.width==1600&&r.height==900);
    r=eb_scale_destination((EbWsSize){256,224},hd,EB_SCALE_ASPECT_NEAREST);
    assert(r.width==822&&r.height==720&&r.x==229);
    r=eb_scale_destination((EbWsSize){400,225},(EbWsSize){200,100},EB_SCALE_INTEGER);
    assert(r.width==177&&r.height==100);
    r=eb_scale_destination((EbWsSize){0,225},hd,EB_SCALE_INTEGER);
    assert(r.width==0&&r.height==0);
    assert(eb_scale_mode_parse("integer")==EB_SCALE_INTEGER);
    assert(eb_scale_mode_parse("linear")==EB_SCALE_ASPECT_LINEAR);
    assert(eb_scale_mode_parse(NULL)==EB_SCALE_ASPECT_NEAREST);
    assert(strcmp(eb_scale_mode_name(EB_SCALE_INTEGER),"integer")==0);
    return 0;
}
