/* Companion manual suspend points + finite step-back rewind history.
 * Never modifies the game's 7680-byte battery SRAM (earthbound.srm).
 * All calls that serialize or restore must happen at root-loop boundary.
 */
#include "platform/platform.h"
#include "core/state_dump.h"
#include "game/audio.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define POINTS 4
#define HISTORY 10
#define THUMB_W 160
#define THUMB_H 90
#define DIR "sdmc:/switch/earthbound-companion/states"

void companion_state_channel(int group,int number);
bool companion_state_storage_init(void);
static bool storage_ready;
static int history_head;
static int history_count;
static unsigned frames_until_capture;
static uint16_t thumb[THUMB_W*THUMB_H];

void companion_suspend_init(void) {
    storage_ready=companion_state_storage_init();
    history_head=history_count=0;
    frames_until_capture=180;
}
bool companion_suspend_available(void) { return storage_ready; }
int companion_rewind_available(void) { return history_count; }

/* Store a small independent RGB565 visual preview, not a fake placeholder. */
static void thumb_build(const uint16_t *frame,int w,int h) {
    for(int y=0;y<THUMB_H;y++){
        int src_y=y*h/THUMB_H;
        for(int x=0;x<THUMB_W;x++){
            int src_x=x*w/THUMB_W;
            thumb[y*THUMB_W+x]=frame[src_y*w+src_x];
        }
    }
}
static bool thumb_path(char *path,size_t capacity,int slot) {
    int n=snprintf(path,capacity,DIR "/manual%d.thumb",slot);
    return n>0 && (size_t)n<capacity;
}
static bool thumb_store(int slot,const uint16_t *frame,int w,int h) {
    if(!frame||w<=0||h<=0)return false;
    thumb_build(frame,w,h);
    char path[256];if(!thumb_path(path,sizeof(path),slot))return false;
    char tmp[275];int n=snprintf(tmp,sizeof(tmp),"%s.tmp",path);
    if(n<0||(size_t)n>=sizeof(tmp))return false;
    FILE*f=fopen(tmp,"wb");if(!f)return false;
    static const unsigned char magic[8]={'E','B','T','H',160,90,0,1};
    bool ok=fwrite(magic,1,sizeof(magic),f)==sizeof(magic) &&
        fwrite(thumb,sizeof(thumb),1,f)==1 && fflush(f)==0;
    if(fclose(f)!=0)ok=false;
    if(!ok){remove(tmp);return false;}
    remove(path);
    if(rename(tmp,path)!=0){remove(tmp);return false;}
    return true;
}
bool companion_suspend_preview(int slot,uint16_t *out){
    if(slot<0||slot>=POINTS||!out)return false;
    char path[256];if(!thumb_path(path,sizeof(path),slot))return false;
    FILE *f=fopen(path,"rb");if(!f)return false;
    unsigned char hdr[8];
    bool ok=fread(hdr,1,8,f)==8 && memcmp(hdr,"EBTH",4)==0 &&
            hdr[4]==THUMB_W && hdr[5]==THUMB_H && hdr[7]==1;
    if(ok)ok=fread(out,sizeof(thumb),1,f)==1;
    fclose(f);return ok;
}
bool companion_suspend_slot_exists(int slot){
    if(slot<0||slot>=POINTS)return false;
    char path[256];FILE*f=NULL;
    int n=snprintf(path,sizeof(path),DIR "/manual%d.bin.0",slot);
    if(n>0&&(size_t)n<sizeof(path))f=fopen(path,"rb");
    if(!f){
        n=snprintf(path,sizeof(path),DIR "/manual%d.bin.1",slot);
        if(n>0&&(size_t)n<sizeof(path))f=fopen(path,"rb");
    }
    if(f){fclose(f);return true;}
    return false;
}

static bool capture(int group,int slot){
    if(!storage_ready)return false;
    companion_state_channel(group,slot);
    platform_savestate_freeze_audio(true);
    bool ok=state_dump_save_slots();
    platform_savestate_freeze_audio(false);
    if(!ok)fprintf(stderr,"Companion: snapshot write failed g=%d slot=%d\n",group,slot);
    return ok;
}
static bool restore(int group,int slot){
    if(!storage_ready)return false;
    companion_state_channel(group,slot);
    platform_savestate_freeze_audio(true);
    bool ok=state_dump_load_slots();
    if(ok)audio_resync_after_load();
    platform_savestate_freeze_audio(false);
    if(!ok)fprintf(stderr,"Companion: snapshot restore failed g=%d slot=%d\n",group,slot);
    return ok;
}
/* action=1 create suspend; 2 load suspend; 3 rewind one snapshot */
bool companion_suspend_action(int action,int slot,
                              const uint16_t *frame,int w,int h){
    if(!storage_ready)return false;
    if(action==1 && slot>=0 && slot<POINTS){
        if(!capture(0,slot))return false;
        /* Thumbnail is best-effort; never report save failure for it. */
        thumb_store(slot,frame,w,h);
        return true;
    }
    if(action==2 && slot>=0 && slot<POINTS){
        bool ok=restore(0,slot);
        if(ok){history_count=history_head=0;frames_until_capture=180;}
        return ok;
    }
    if(action==3 && history_count>0){
        int index=(history_head+HISTORY-1)%HISTORY;
        if(!restore(1,index))return false;
        history_head=index;
        history_count--;
        frames_until_capture=180;
        return true;
    }
    return false;
}
/* Once every three gameplay seconds. Never called from paused UI. */
void companion_suspend_frame_tick(void){
    if(!storage_ready)return;
    if(--frames_until_capture>0)return;
    frames_until_capture=180;
    if(capture(1,history_head)){
        history_head=(history_head+1)%HISTORY;
        if(history_count<HISTORY)history_count++;
    }
}
