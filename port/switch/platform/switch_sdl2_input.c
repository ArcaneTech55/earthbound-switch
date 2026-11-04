#include "platform/platform.h"
#include "../input_mapping.h"
#include "game/game_state.h"
#include "game/display_text.h"
#include "include/pad.h"
#include <SDL.h>
#include <stdio.h>

/* Switch-only QoL configuration: settings are never serialized inside SRAM.
 * Keep the player's original save format and gameplay state layout intact. */
static SDL_GameController *controller;
static uint16_t held, previous;
static uint16_t raw_previous;
static bool quit_requested;
bool eb_switch_menu_open;
int eb_switch_menu_selected;
int eb_switch_menu_scale;
bool eb_switch_menu_fast_text;
bool eb_switch_menu_no_dad;
bool eb_switch_menu_no_homesick;
bool eb_switch_menu_auto_heal;
bool eb_switch_menu_y_text;
bool eb_switch_menu_run; /* reserved until movement engine adaptation */
static bool menu_combo_latched;
static uint32_t frame_counter;
static bool quick_text_active;
static uint8_t original_text_speed;
extern void eb_switch_video_set_scale(int mode);
bool platform_headless=false;
bool platform_skip_intro=false;
int platform_max_frames=0;

#define MENU_ITEMS 8

static void heal_party(void) {
    for (int i=0; i<4; ++i) {
        CharStruct *p=&party_characters[i];
        if (!p->max_hp) continue;
        p->current_hp = p->current_hp_target = p->max_hp;
        p->current_pp = p->current_pp_target = p->max_pp;
        for (unsigned j=0; j<sizeof(p->afflictions); ++j)
            p->afflictions[j]=0;
    }
}
static void load_preferences(void) {
    FILE *f=fopen("/switch/earthbound-switch/qol.cfg","r");
    if (!f) return;
    char line[128];
    while (fgets(line,sizeof(line),f)) {
        int v;
        if (sscanf(line,"fast_text=%d",&v)==1) eb_switch_menu_fast_text=(v!=0);
        else if (sscanf(line,"no_dad=%d",&v)==1) eb_switch_menu_no_dad=(v!=0);
        else if (sscanf(line,"no_homesick=%d",&v)==1) eb_switch_menu_no_homesick=(v!=0);
        else if (sscanf(line,"auto_heal=%d",&v)==1) eb_switch_menu_auto_heal=(v!=0);
    }
    fclose(f);
}
static void persist_preferences(void) {
    FILE *f=fopen("/switch/earthbound-switch/qol.cfg.tmp","w");
    if(!f) return;
    fprintf(f,"fast_text=%d\nno_dad=%d\nno_homesick=%d\nauto_heal=%d\n",
            eb_switch_menu_fast_text,eb_switch_menu_no_dad,
            eb_switch_menu_no_homesick,eb_switch_menu_auto_heal);
    if (fclose(f)==0)
        rename("/switch/earthbound-switch/qol.cfg.tmp",
               "/switch/earthbound-switch/qol.cfg");
}
static void persist_scaling(void) {
    static const char *modes[]={"nearest","integer","linear"};
    FILE *f=fopen("/switch/earthbound-switch/scaling.cfg","w");
    if (!f) return;
    fprintf(f,"%s\n",modes[eb_switch_menu_scale%3]);
    fclose(f);
}
static void menu_activate(void) {
    switch (eb_switch_menu_selected) {
    case 0:
        eb_switch_menu_scale=(eb_switch_menu_scale+1)%3;
        eb_switch_video_set_scale(eb_switch_menu_scale);
        persist_scaling();
        return;
    case 1: eb_switch_menu_fast_text=!eb_switch_menu_fast_text; break;
    case 2: eb_switch_menu_no_dad=!eb_switch_menu_no_dad; break;
    case 3: eb_switch_menu_no_homesick=!eb_switch_menu_no_homesick; break;
    case 4: eb_switch_menu_auto_heal=!eb_switch_menu_auto_heal; break;
    case 5: heal_party(); return;
    case 6:
        game_state.money_carried =
            game_state.money_carried > 9998999 ? 9999999 : game_state.money_carried+1000;
        return;
    case 7: eb_switch_menu_open=false; return;
    }
    persist_preferences();
}
bool platform_input_init(void) {
    held=previous=raw_previous=0;
    quit_requested=false;
    load_preferences();
    if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)<0)return false;
    for(int i=0;i<SDL_NumJoysticks();++i)
        if(SDL_IsGameController(i) && (controller=SDL_GameControllerOpen(i)))break;
    return true;
}
void platform_input_shutdown(void) {
    if(controller){SDL_GameControllerClose(controller);controller=NULL;}
}
void platform_input_poll(void) {
    SDL_Event ev;
    previous=held;
    while(SDL_PollEvent(&ev)) {
        if(ev.type==SDL_QUIT)quit_requested=true;
        if(ev.type==SDL_CONTROLLERDEVICEADDED && !controller &&
           SDL_IsGameController(ev.cdevice.which))
            controller=SDL_GameControllerOpen(ev.cdevice.which);
        if(ev.type==SDL_CONTROLLERDEVICEREMOVED && controller &&
           ev.cdevice.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
            SDL_GameControllerClose(controller);controller=NULL;
        }
    }
    uint32_t buttons=0;
    if(controller) {
        const struct {SDL_GameControllerButton button;uint32_t bit;} mapping[]={
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
        for(unsigned i=0;i<sizeof(mapping)/sizeof(mapping[0]);++i)
            if(SDL_GameControllerGetButton(controller,mapping[i].button))buttons|=mapping[i].bit;
    }
    bool combo=controller && SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_START) &&
       (SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>16000 ||
        SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSTICK));
    if(combo && !menu_combo_latched) {
        eb_switch_menu_open=!eb_switch_menu_open;
        eb_switch_menu_selected=0;
    }
    menu_combo_latched=combo;
    uint16_t raw=eb_switch_map_pad(buttons);
    uint16_t pressed=eb_switch_pad_pressed(raw_previous,raw);
    raw_previous=raw;
    if(eb_switch_menu_open) {
        if(pressed&PAD_UP)eb_switch_menu_selected=(eb_switch_menu_selected+MENU_ITEMS-1)%MENU_ITEMS;
        if(pressed&PAD_DOWN)eb_switch_menu_selected=(eb_switch_menu_selected+1)%MENU_ITEMS;
        if(pressed&PAD_A)menu_activate();
        if(pressed&PAD_B)eb_switch_menu_open=false;
        held=0;
    } else {
        /* Don't leak the menu-opening PLUS button into EarthBound. */
        held=combo?0:raw;
    }

    ++frame_counter;
    if(!eb_switch_menu_open && eb_switch_menu_auto_heal && frame_counter%60==0)
        heal_party();

    /* Y is unused by vanilla gameplay; while held, use its minimum text delay.
     * This never accelerates the 60 Hz simulation or audio clock. */
    eb_switch_menu_y_text = !eb_switch_menu_open && (raw&PAD_Y)!=0;
    bool quick=eb_switch_menu_fast_text || eb_switch_menu_y_text;
    if (quick && !quick_text_active) {
        original_text_speed=dt.selected_text_speed;
        quick_text_active=true;
    }
    if (quick) dt.selected_text_speed=0;
    else if (quick_text_active) {
        dt.selected_text_speed=original_text_speed;
        quick_text_active=false;
    }
}
uint16_t platform_input_get_pad(void){return held;}
uint16_t platform_input_get_pad_new(void){return eb_switch_pad_pressed(previous,held);}
uint16_t platform_input_get_aux(void){return 0;}
bool platform_input_quit_requested(void){return quit_requested;}
void platform_request_quit(void){quit_requested=true;}
