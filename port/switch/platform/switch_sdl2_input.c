#include "platform/platform.h"
#include "../input_mapping.h"
#include <SDL.h>
#include <stdio.h>
#include "game/game_state.h"
#include "game/display_text.h"
#include "include/pad.h"

/* SDL2 game controller driver for Nintendo Switch. SDL2 maps physical
 * controls to the standard controller button layout. The Nintendo button
 * LABEL convention (A right, B bottom) is explicitly accounted for below.
 * Actual SDL2 Switch controller mapping must be validated on hardware. */
static SDL_GameController *controller;
static uint16_t held,previous;
static bool quit_requested;
/* ZL + PLUS opens the native settings and cheats overlay. */
bool eb_switch_menu_open = false;
int eb_switch_menu_selected = 0;
int eb_switch_menu_scale = 0; /* nearest, integer, linear */
bool eb_switch_menu_instant_text = false;
bool eb_switch_menu_auto_heal = false;
static bool menu_combo_latched = false;
static uint32_t frame_counter = 0;
static uint8_t previous_text_speed = 1;
extern void eb_switch_video_set_scale(int mode);
static void heal_party(void) {
    for (int i = 0; i < 4; ++i) {
        CharStruct *p = &party_characters[i];
        if (!p->max_hp) continue;
        p->current_hp = p->current_hp_target = p->max_hp;
        p->current_pp = p->current_pp_target = p->max_pp;
        for (unsigned j = 0; j < sizeof(p->afflictions); ++j) p->afflictions[j] = 0;
    }
}
static void save_scaling(void) {
    FILE *file=fopen("/switch/earthbound-switch/scaling.cfg", "w");
    if(!file)return;
    const char *names[]={"nearest","integer","linear"};
    fprintf(file,"%s\n",names[eb_switch_menu_scale%3]);
    fclose(file);
}
static void menu_action(void) {
    switch (eb_switch_menu_selected) {
        case 0:
            eb_switch_menu_scale = (eb_switch_menu_scale + 1) % 3;
            eb_switch_video_set_scale(eb_switch_menu_scale);
            save_scaling();
            break;
        case 1:
            if(!eb_switch_menu_instant_text) {
                previous_text_speed = dt.selected_text_speed;
                dt.selected_text_speed = 0; /* Fastest native typewriter delay, 60 FPS */
                eb_switch_menu_instant_text = true;
            } else {
                dt.selected_text_speed = previous_text_speed;
                eb_switch_menu_instant_text = false;
            }
            break;
        case 2: eb_switch_menu_auto_heal = !eb_switch_menu_auto_heal; break;
        case 3: heal_party(); break;
        case 4:
            if (game_state.money_carried < 9999000)
                game_state.money_carried += 1000;
            else game_state.money_carried = 9999999;
            break;
        case 5: eb_switch_menu_open = false; break;
    }
}

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
    ++frame_counter;
    if (eb_switch_menu_instant_text) dt.selected_text_speed = 0;
    bool combo = controller &&
        SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSTICK) &&
        SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START);
    /* Also support ZL + PLUS; left-stick click + PLUS is a fallback. */
    if (controller) combo = combo ||
        (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000 &&
         SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START));
    if (combo && !menu_combo_latched) {
        eb_switch_menu_open = !eb_switch_menu_open;
        eb_switch_menu_selected = 0;
    }
    menu_combo_latched = combo;
    uint16_t next = eb_switch_map_pad(buttons);
    if (eb_switch_menu_open) {
        uint16_t pressed = eb_switch_pad_pressed(held, next);
        if (pressed & PAD_UP) eb_switch_menu_selected = (eb_switch_menu_selected + 5) % 6;
        if (pressed & PAD_DOWN) eb_switch_menu_selected = (eb_switch_menu_selected + 1) % 6;
        if (pressed & PAD_A) menu_action();
        if (pressed & PAD_B) eb_switch_menu_open = false;
        next = 0; /* Menu buttons must not also control EarthBound. */
    } else if (eb_switch_menu_auto_heal && frame_counter % 60 == 0) {
        heal_party();
    }
    held = next;
}
uint16_t platform_input_get_pad(void) { return held; }
uint16_t platform_input_get_pad_new(void) { return eb_switch_pad_pressed(previous,held); }
uint16_t platform_input_get_aux(void) { return 0; }
bool platform_input_quit_requested(void) { return quit_requested; }
void platform_request_quit(void) { quit_requested=true; }
