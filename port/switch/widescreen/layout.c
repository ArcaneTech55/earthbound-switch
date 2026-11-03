#include "layout.h"
#include <stdint.h>
EbWsSize eb_ws_logical_size(void) { return (EbWsSize){400,225}; }
EbWsSize eb_ws_output_size(void) { return (EbWsSize){1280,720}; }
EbWsRect eb_ws_anchor(EbWsSize c,EbWsSize o,EbWsAnchor a,int margin) {
    EbWsRect r={(c.width-o.width)/2,(c.height-o.height)/2,o.width,o.height};
    switch(a) {
      case EB_WS_LEFT:r.x=margin;break;
      case EB_WS_RIGHT:r.x=c.width-o.width-margin;break;
      case EB_WS_TOP:r.y=margin;break;
      case EB_WS_BOTTOM:r.y=c.height-o.height-margin;break;
      default:break;
    }
    return r;
}
EbWsRect eb_ws_fit(EbWsSize source,EbWsSize output) {
    if(source.width<=0 || source.height<=0 || output.width<=0 || output.height<=0)
        return (EbWsRect){0,0,0,0};
    if((int64_t)output.width*source.height<=(int64_t)output.height*source.width) {
        int h=(int)((int64_t)output.width*source.height/source.width);
        return (EbWsRect){0,(output.height-h)/2,output.width,h};
    }
    int w=(int)((int64_t)output.height*source.width/source.height);
    return (EbWsRect){(output.width-w)/2,0,w,output.height};
}
int eb_ws_floor_div(int n,int divisor) {
    if(divisor<=0)return 0;
    int q=n/divisor;
    return (n%divisor<0)?q-1:q;
}
