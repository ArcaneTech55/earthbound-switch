#include <assert.h>
#include "input_mapping.h"
#include "include/pad.h"
int main(void) {
    assert(eb_switch_map_pad(0)==0);
    assert(eb_switch_map_pad(EB_SW_BTN_A)==PAD_A);
    assert(eb_switch_map_pad(EB_SW_BTN_B)==PAD_B);
    assert(eb_switch_map_pad(EB_SW_BTN_PLUS)==PAD_START);
    assert(eb_switch_map_pad(EB_SW_BTN_MINUS)==PAD_SELECT);
    assert(eb_switch_map_pad(EB_SW_BTN_UP|EB_SW_BTN_RIGHT)==(PAD_UP|PAD_RIGHT));
    assert(eb_switch_map_pad(EB_SW_BTN_A|EB_SW_BTN_L)==(PAD_A|PAD_L));
    assert(eb_switch_pad_pressed(PAD_A,PAD_A|PAD_B)==PAD_B);
    assert(eb_switch_pad_pressed(PAD_A,PAD_A)==0);
    return 0;
}
