#include "scaling.h"
#include <string.h>
EbWsRect eb_scale_destination(EbWsSize source,EbWsSize output,EbScaleMode mode) {
    if(source.width<=0||source.height<=0||output.width<=0||output.height<=0)
        return (EbWsRect){0,0,0,0};
    if(mode==EB_SCALE_INTEGER) {
        int sx=output.width/source.width, sy=output.height/source.height;
        int scale=sx<sy?sx:sy;
        if(scale>=1) {
            int w=source.width*scale, h=source.height*scale;
            return (EbWsRect){(output.width-w)/2,(output.height-h)/2,w,h};
        }
    }
    return eb_ws_fit(source,output);
}
const char *eb_scale_mode_name(EbScaleMode mode) {
    switch(mode) {
      case EB_SCALE_INTEGER:return "integer";
      case EB_SCALE_ASPECT_LINEAR:return "linear";
      default:return "nearest";
    }
}
EbScaleMode eb_scale_mode_parse(const char *s) {
    if(!s)return EB_SCALE_ASPECT_NEAREST;
    if(strcmp(s,"integer")==0)return EB_SCALE_INTEGER;
    if(strcmp(s,"linear")==0)return EB_SCALE_ASPECT_LINEAR;
    return EB_SCALE_ASPECT_NEAREST;
}
