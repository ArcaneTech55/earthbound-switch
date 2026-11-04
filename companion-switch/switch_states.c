/* Switch Companion savestate file backend, independent of cartridge SRAM.
 * Each logical channel has two CRC-validated ping-pong files managed by
 * state_dump.c. Manual points and rewind history use separate namespaces.
 */
#include "platform/platform.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#define STATE_DIR "sdmc:/switch/earthbound-companion/states"
#define MAX_SNAPSHOT_BYTES (2u * 1024u * 1024u)

static FILE *writers[2];
static int current_group;     /* 0=manual slots, 1=rewind ring */
static int current_number;

void companion_state_channel(int group, int number) {
    if (group < 0 || group > 1 || number < 0 || number >= 16) return;
    /* Call only at root-loop boundary: no active writer may be switched. */
    if (writers[0] || writers[1]) return;
    current_group=group;
    current_number=number;
}
static bool state_path(char *dst,size_t cap,int slot) {
    int n=snprintf(dst,cap,"%s/%s%d.bin.%d",STATE_DIR,
                   current_group?"rewind":"manual",current_number,slot);
    return n>0 && (size_t)n<cap;
}
bool companion_state_storage_init(void) {
    if (mkdir(STATE_DIR,0777)!=0 && errno!=EEXIST) return false;
    return true;
}
bool platform_savestate_begin(int slot) {
    if(slot<0||slot>=2||writers[slot]) return false;
    char path[256];
    if(!state_path(path,sizeof(path),slot))return false;
    writers[slot]=fopen(path,"wb");
    if(!writers[slot])fprintf(stderr,"state: open failed %s (%d)\n",path,errno);
    return writers[slot]!=NULL;
}
bool platform_savestate_write(int slot,size_t offset,const void *src,size_t size){
    if(slot<0||slot>=2||!writers[slot]||!src||
       offset>MAX_SNAPSHOT_BYTES||size>MAX_SNAPSHOT_BYTES-offset)return false;
    if(fseek(writers[slot],(long)offset,SEEK_SET)!=0)return false;
    return fwrite(src,1,size,writers[slot])==size;
}
bool platform_savestate_commit(int slot){
    if(slot<0||slot>=2||!writers[slot])return false;
    FILE *f=writers[slot];writers[slot]=NULL;
    bool ok=fflush(f)==0;
    if(fclose(f)!=0)ok=false;
    return ok;
}
size_t platform_savestate_read(int slot,size_t offset,void *dst,size_t size) {
    if(slot<0||slot>=2||!dst||
       offset>MAX_SNAPSHOT_BYTES||size>MAX_SNAPSHOT_BYTES-offset)return 0;
    char path[256];
    if(!state_path(path,sizeof(path),slot))return 0;
    FILE *f=fopen(path,"rb");
    if(!f)return 0;
    size_t got=0;
    if(fseek(f,(long)offset,SEEK_SET)==0)got=fread(dst,1,size,f);
    fclose(f);
    return got;
}

/* Tamp compression and decompression use temporary workspace at root boundary. */
void *platform_savestate_scratch(size_t *out_bytes) {
    static uint8_t scratch[16 * 1024];
    if (out_bytes) *out_bytes = sizeof(scratch);
    return scratch;
}
