#ifndef EB_SWITCH_INPUT_MAPPING_H
#define EB_SWITCH_INPUT_MAPPING_H
#include <stdbool.h>
#include <stdint.h>
/* Controller-neutral state. The SDL2/SDL_GameController backend converts Joy-Con,
 * handheld, and Pro Controller input to these masks before calling the mapper. */
enum {
    EB_SW_BTN_A = 1u<<0, EB_SW_BTN_B = 1u<<1, EB_SW_BTN_X = 1u<<2,
    EB_SW_BTN_Y = 1u<<3, EB_SW_BTN_L = 1u<<4, EB_SW_BTN_R = 1u<<5,
    EB_SW_BTN_PLUS = 1u<<6, EB_SW_BTN_MINUS = 1u<<7,
    EB_SW_BTN_UP = 1u<<8, EB_SW_BTN_DOWN = 1u<<9,
    EB_SW_BTN_LEFT = 1u<<10, EB_SW_BTN_RIGHT = 1u<<11
};
uint16_t eb_switch_map_pad(uint32_t buttons);
uint16_t eb_switch_pad_pressed(uint16_t previous, uint16_t current);
#endif
