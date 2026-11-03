#ifndef EB_SWITCH_SCENE_ART_H
#define EB_SWITCH_SCENE_ART_H
#include "core/types.h"

/* The mode is transient presentation state, never saved into game SRAM.
 * Artwork exists only in the added viewport columns; the core SNES image,
 * logo sprites, battles and UI are never stretched or cropped. */
typedef enum {
    EB_SWITCH_SCENE_NONE=0,
    EB_SWITCH_SCENE_TITLE,
    EB_SWITCH_SCENE_BATTLE,
    EB_SWITCH_SCENE_CINEMATIC
} EbSwitchScene;
void eb_switch_scene_set(EbSwitchScene scene);
EbSwitchScene eb_switch_scene_get(void);
void eb_switch_scene_art_frame(pixel_t *frame,int width,int height);
#endif
