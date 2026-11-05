#include "viewport.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  assert(SMW_WIDE_WIDTH == 398);
  assert(SMW_WIDE_MARGIN == 71);
  assert(SMW_WIDE_FRAME_BYTES == 356608);

  SmwWideViewport v = { .camera_x = 1000, .level_width = 4096 };
  assert(smw_wide_world_to_screen_x(v, 1000) == 71);
  assert(smw_wide_world_to_screen_x(v, 1255) == 326);
  assert(smw_wide_screen_to_world_x(v, 0) == 929);
  assert(smw_wide_screen_to_world_x(v, 397) == 1326);
  assert(smw_wide_sprite_visible_x(v, 930, 16));
  assert(!smw_wide_sprite_visible_x(v, 900, 8));
  assert(smw_wide_clamped_camera_x(v) == 929);
  v.camera_x = 4080;
  assert(smw_wide_clamped_camera_x(v) == 4096 - 398);
  v.camera_x = 0;
  assert(smw_wide_clamped_camera_x(v) == 0);
  v.allow_left = true;
  assert(smw_wide_clamped_camera_x(v) == -71);
  v.level_width = 100;
  assert(smw_wide_clamped_camera_x(v) == -71);
  puts("SMW 16:9 viewport geometry: PASS");
  return 0;
}
