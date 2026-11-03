#ifndef EB_SWITCH_SCALING_H
#define EB_SWITCH_SCALING_H
#include "layout.h"
typedef enum {
 EB_SCALE_ASPECT_NEAREST=0,
 EB_SCALE_INTEGER=1,
 EB_SCALE_ASPECT_LINEAR=2
} EbScaleMode;
/* Returns largest aspect-correct destination in output. Integer mode floors
 * scale and centers, without cropping; falls back to aspect mode when
 * output is smaller than source. This never changes world-space content. */
EbWsRect eb_scale_destination(EbWsSize source,EbWsSize output,EbScaleMode mode);
const char *eb_scale_mode_name(EbScaleMode mode);
EbScaleMode eb_scale_mode_parse(const char *s);
#endif
