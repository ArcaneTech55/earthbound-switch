#ifndef EB_SWITCH_WIDESCREEN_LAYOUT_H
#define EB_SWITCH_WIDESCREEN_LAYOUT_H
typedef struct { int x,y,width,height; } EbWsRect;
typedef struct { int width,height; } EbWsSize;
typedef enum { EB_WS_CENTER,EB_WS_LEFT,EB_WS_RIGHT,EB_WS_TOP,EB_WS_BOTTOM } EbWsAnchor;
EbWsSize eb_ws_logical_size(void);
EbWsSize eb_ws_output_size(void);
EbWsRect eb_ws_anchor(EbWsSize canvas,EbWsSize object,EbWsAnchor anchor,int margin);
EbWsRect eb_ws_fit(EbWsSize source,EbWsSize output);
int eb_ws_floor_div(int n,int positive_divisor);
#endif
