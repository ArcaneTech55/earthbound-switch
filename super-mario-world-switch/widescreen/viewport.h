#ifndef SMW_WIDESCREEN_VIEWPORT_H
#define SMW_WIDESCREEN_VIEWPORT_H

/*
 * SNES-native pixel viewport geometry for an undistorted 16:9 picture.
 * 224 lines * 16 / 9 = 398.222...; use even width 398.
 * Original 256x224 SNES pixels remain centered, with 71 new pixels on
 * either side. The extra pixels MUST be rendered from the level; they
 * cannot be stretched/copied from the original framebuffer.
 */
#include <stdbool.h>
#include <stdint.h>

enum {
  SMW_NATIVE_WIDTH = 256,
  SMW_NATIVE_HEIGHT = 224,
  SMW_WIDE_WIDTH = 398,
  SMW_WIDE_HEIGHT = 224,
  SMW_WIDE_MARGIN = (SMW_WIDE_WIDTH - SMW_NATIVE_WIDTH) / 2,
  SMW_WIDE_FRAME_BYTES = SMW_WIDE_WIDTH * SMW_WIDE_HEIGHT * 4
};

typedef struct SmwWideViewport {
  int32_t camera_x;        /* world coordinate at original SNES left edge */
  int32_t level_width;     /* level width in world pixels */
  bool allow_left;         /* allow expansion left of level origin */
} SmwWideViewport;

/* World-space x -> position in expanded framebuffer. */
static inline int32_t smw_wide_world_to_screen_x(
    SmwWideViewport viewport, int32_t world_x) {
  return world_x - viewport.camera_x + SMW_WIDE_MARGIN;
}

/* Screen-space x -> world coordinate, including offscreen side strips. */
static inline int32_t smw_wide_screen_to_world_x(
    SmwWideViewport viewport, int32_t screen_x) {
  return screen_x + viewport.camera_x - SMW_WIDE_MARGIN;
}

/* Sprite overlap test; avoids losing sprites in newly exposed strips. */
static inline bool smw_wide_sprite_visible_x(
    SmwWideViewport viewport, int32_t world_x, int32_t sprite_width) {
  int32_t left = smw_wide_world_to_screen_x(viewport, world_x);
  return sprite_width > 0 && left < SMW_WIDE_WIDTH &&
         left + sprite_width > 0;
}

/* Constrain a wide camera to a finite level. A level shorter than the
 * viewport is anchored to its left edge rather than underflowing. */
static inline int32_t smw_wide_clamped_camera_x(
    SmwWideViewport viewport) {
  int32_t left = viewport.allow_left ? -SMW_WIDE_MARGIN : 0;
  int32_t max_left = viewport.level_width - SMW_WIDE_WIDTH;
  int32_t requested = viewport.camera_x - SMW_WIDE_MARGIN;
  if (max_left < left) max_left = left;
  if (requested < left) requested = left;
  if (requested > max_left) requested = max_left;
  return requested;
}

#endif
