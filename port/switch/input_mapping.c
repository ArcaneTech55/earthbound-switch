#include "input_mapping.h"
#include "include/pad.h"

/* Map by Nintendo button labels, not physical positions.
 * Nintendo A=confirm and B=cancel; shoulder buttons retain SNES roles. */
uint16_t eb_switch_map_pad(uint32_t buttons) {
    uint16_t pad=0;
    if(buttons & EB_SW_BTN_A) pad |= PAD_A;
    if(buttons & EB_SW_BTN_B) pad |= PAD_B;
    if(buttons & EB_SW_BTN_X) pad |= PAD_X;
    if(buttons & EB_SW_BTN_Y) pad |= PAD_Y;
    if(buttons & EB_SW_BTN_L) pad |= PAD_L;
    if(buttons & EB_SW_BTN_R) pad |= PAD_R;
    if(buttons & EB_SW_BTN_PLUS) pad |= PAD_START;
    if(buttons & EB_SW_BTN_MINUS) pad |= PAD_SELECT;
    if(buttons & EB_SW_BTN_UP) pad |= PAD_UP;
    if(buttons & EB_SW_BTN_DOWN) pad |= PAD_DOWN;
    if(buttons & EB_SW_BTN_LEFT) pad |= PAD_LEFT;
    if(buttons & EB_SW_BTN_RIGHT) pad |= PAD_RIGHT;
    return pad;
}
uint16_t eb_switch_pad_pressed(uint16_t previous,uint16_t current) {
    return (uint16_t)(current & (uint16_t)~previous);
}
