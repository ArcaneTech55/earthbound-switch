#include "platform/platform.h"
/* Screenshots/VRAM dumping are desktop-only at this stage. */
void platform_debug_dump_ppu(const pixel_t *framebuffer) {
    (void)framebuffer;
}
void platform_debug_dump_vram_image(void) { }
