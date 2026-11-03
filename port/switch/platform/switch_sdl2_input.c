#include "platform/platform.h"
#include "../input_mapping.h"
#include <SDL.h>

/* SDL2 game controller driver for Nintendo Switch. SDL2 maps physical
 * controls to the standard controller button layout. The Nintendo button
 * LABEL convention (A right, B bottom) is explicitly accounted for below.
 * Actual SDL2 Switch controller mapping must be validated on hardware. */
static SDL_GameController *controller;
static uint16_t held,previous;
static bool quit_requested;
bool platform_headless=false;
bool platform_skip_intro=false;
int platform_max_frames=0;

bool platform_input_init(void) {
    held=previous=0;quit_requested=false;
    if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)<0) return false;
    for(int i=0;i<SDL_NumJoysticks();i++) {
        if(SDL_IsGameController(i)) {
            controller=SDL_GameControllerOpen(i);
            if(controller) break;
        }
    }
    return true;
}
void platform_input_shutdown(void) {
    if(controller) { SDL_GameControllerClose(controller);controller=NULL; }
}
void platform_input_poll(void) {
    SDL_Event ev;
    previous=held;
    while(SDL_PollEvent(&ev)) {
        if(ev.type==SDL_QUIT) quit_requested=true;
        if(ev.type==SDL_CONTROLLERDEVICEADDED && !controller && SDL_IsGameController(ev.cdevice.which))
            controller=SDL_GameControllerOpen(ev.cdevice.which);
        if(ev.type==SDL_CONTROLLERDEVICEREMOVED && controller &&
           ev.cdevice.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
            SDL_GameControllerClose(controller);controller=NULL;
        }
    }
    uint32_t buttons=0;
    if(controller) {
        /* SDL button enums describe south/east/west/north positions.
         * Nintendo labels: A=east, B=south, X=north, Y=west. */
        const struct { SDL_GameControllerButton button; uint32_t bit; } mapping[]={
            {SDL_CONTROLLER_BUTTON_B,EB_SW_BTN_A},
            {SDL_CONTROLLER_BUTTON_A,EB_SW_BTN_B},
            {SDL_CONTROLLER_BUTTON_Y,EB_SW_BTN_X},
            {SDL_CONTROLLER_BUTTON_X,EB_SW_BTN_Y},
            {SDL_CONTROLLER_BUTTON_LEFTSHOULDER,EB_SW_BTN_L},
            {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,EB_SW_BTN_R},
            {SDL_CONTROLLER_BUTTON_START,EB_SW_BTN_PLUS},
            {SDL_CONTROLLER_BUTTON_BACK,EB_SW_BTN_MINUS},
            {SDL_CONTROLLER_BUTTON_DPAD_UP,EB_SW_BTN_UP},
            {SDL_CONTROLLER_BUTTON_DPAD_DOWN,EB_SW_BTN_DOWN},
            {SDL_CONTROLLER_BUTTON_DPAD_LEFT,EB_SW_BTN_LEFT},
            {SDL_CONTROLLER_BUTTON_DPAD_RIGHT,EB_SW_BTN_RIGHT}
        };
        for(unsigned i=0;i<sizeof(mapping)/sizeof(mapping[0]);i++)
            if(SDL_GameControllerGetButton(controller,mapping[i].button))
                buttons|=mapping[i].bit;
    }
    held=eb_switch_map_pad(buttons);
}
uint16_t platform_input_get_pad(void) { return held; }
uint16_t platform_input_get_pad_new(void) { return eb_switch_pad_pressed(previous,held); }
uint16_t platform_input_get_aux(void) { return 0; }
bool platform_input_quit_requested(void) { return quit_requested; }
void platform_request_quit(void) { quit_requested=true; }
