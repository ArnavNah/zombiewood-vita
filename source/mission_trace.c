#include "mission_trace.h"
#ifdef PORT_MISSION_TRACE
#include "config.h"
#include <so_util/so_util.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <vitaGL.h>
#include <malloc.h>
#include <stdlib.h>
#include "reimpl/io.h"
#include <unistd.h>

extern so_module so_mod;
extern SceUID vm_blk;
static void *(*get_app)(void);
static so_hook start_hook,level_hook,loading_render_hook;
static int force_present,hold_empty,held,mission_started,step_loading,step_pending;
static void self_test_paths(void);
static void trace(const char *format,...) {
    char line[512];
    int used=snprintf(line,sizeof(line),"%llu ",sceKernelGetProcessTimeWide()/1000);
    va_list ap; va_start(ap,format);
    vsnprintf(line+used,sizeof(line)-used,format,ap); va_end(ap);
    size_t len=strlen(line); line[len++]='\n';
    SceUID fd=sceIoOpen(DATA_PATH "mission-trace.log",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_APPEND,0666);
    if(fd>=0){sceIoWrite(fd,line,len);sceIoClose(fd);}
}
static void start_mission(void *self) {
    mission_started=1;
    trace("START MISSION enter %p",self);
    SO_CONTINUE(int,start_hook,self);
    trace("START MISSION return");
}
static void loading_render(void *self) {
    if(mission_started){
        uint8_t *normal=*(uint8_t **)((uint8_t *)self+36);
        uint8_t *loading=*(uint8_t **)((uint8_t *)self+40);
        uint8_t *np=normal?*(uint8_t **)(normal+36):NULL;
        uint8_t *lp=loading?*(uint8_t **)(loading+36):NULL;
        trace("FLASH main=%p player=%p root=%p loading=%p player=%p root=%p",normal,np,np?*(void **)(np+92):NULL,loading,lp,lp?*(void **)(lp+92):NULL);
    }
    SO_CONTINUE(int,loading_render_hook,self);
}
static void start_level(void *self,int first,int second) {
    trace("START LEVEL enter %d,%d",first,second);
    SO_CONTINUE(int,level_hook,self,first,second);
    trace("START LEVEL return");
}
void mission_trace_init(void) {
    FILE *setting=fopen(DATA_PATH "force-empty-present.txt","r");
    force_present=setting!=NULL;
    if(setting)fclose(setting);
    setting=fopen(DATA_PATH "step-loading.txt","r");step_loading=setting!=NULL;if(setting)fclose(setting);
    get_app=(void *)so_symbol(&so_mod,"_ZN3glf3App11GetInstanceEv");
    start_hook=hook_addr(so_symbol(&so_mod,"_ZN13GSMissionInfo12StartMissionEv"),(uintptr_t)start_mission);
    level_hook=hook_addr(so_symbol(&so_mod,"_ZN11ZombiesGame14StartLoadLevelEii"),(uintptr_t)start_level);
    loading_render_hook=hook_addr(so_symbol(&so_mod,"_ZN12FlashManager13SWFLoadRenderEv"),(uintptr_t)loading_render);
    trace("TRACE INIT force_empty_present=%d",force_present);
    self_test_paths();
}
static void self_test_paths(void) {
    static const char *paths[] = {
        "/data/data/" PACKAGE_NAME "/port-path-test.tmp",
        "app0:/data/data/" PACKAGE_NAME "/port-path-test.tmp",
        "data/data/" PACKAGE_NAME "/port-path-test.tmp",
        DATA_PATH "data/data/" PACKAGE_NAME "/port-path-test.tmp"
    };
    int passed = 0, metadata_passed = 0;
    for (unsigned i=0; i<sizeof(paths)/sizeof(paths[0]); ++i) {
        FILE *file=fopen_soloader(paths[i],"wb");
        if (!file) continue;
        int good=fwrite("vita-save-path",1,14,file)==14;
        fclose_soloader(file);
        union { uint64_t alignment; unsigned char raw[120]; } metadata;
        memset(metadata.raw,0xA5,sizeof(metadata.raw));
        int metadata_good=stat_soloader(paths[i],(stat64_bionic *)metadata.raw)==0;
        int64_t size;memcpy(&size,metadata.raw+48,sizeof(size));
        metadata_good=metadata_good && size==14;
        for(unsigned b=104;b<sizeof(metadata.raw);++b)metadata_good=metadata_good && metadata.raw[b]==0xA5;
        file=fopen_soloader(paths[i],"rb");
        if(file){
            memset(metadata.raw,0xA5,sizeof(metadata.raw));
            metadata_good=metadata_good && fstat_soloader(fileno(file),(stat64_bionic *)metadata.raw)==0;
            memcpy(&size,metadata.raw+48,sizeof(size));
            metadata_good=metadata_good && size==14;
            for(unsigned b=104;b<sizeof(metadata.raw);++b)metadata_good=metadata_good && metadata.raw[b]==0xA5;
            fclose_soloader(file);
        }else metadata_good=0;
        metadata_passed+=metadata_good;
        good=good && access_soloader(DATA_PATH "port-path-test.tmp",0)==0;
        good=good && rename_soloader(paths[i],"/data/data/" PACKAGE_NAME "/port-path-renamed.tmp")==0;
        file=fopen_soloader("app0:/data/data/" PACKAGE_NAME "/port-path-renamed.tmp","rb");
        char value[15]={0};
        if(file){good=good && fread(value,1,14,file)==14;fclose_soloader(file);}else good=0;
        good=good && !strcmp(value,"vita-save-path");
        good=good && unlink_soloader(DATA_PATH "port-path-renamed.tmp")==0;
        passed+=good;
    }
    trace("SAVE PATH TEST %d/4",passed);
    trace("ANDROID STAT ABI TEST %d/4 size=%u file_size_offset=%u",metadata_passed,
        (unsigned)sizeof(stat64_bionic),(unsigned)offsetof(stat64_bionic,st_size));
}
void mission_trace_frame(unsigned frame,unsigned draws,unsigned vertices,unsigned fbo,int written) {
    if(!get_app)return;
    uint8_t *glf=get_app();
    if(!glf)return;
    // This APK's Application inherits glf::App at offset 48 (Update thunk).
    uint8_t *app=glf-48;
    uintptr_t *node=*(uintptr_t **)(app+36);
    if(!node || node==(uintptr_t *)(app+32))return;
    uint8_t *state=(void *)node[2];
    if(!state)return;
    const char *name=(const char *)state+4;
    static unsigned next_heap_frame;
    if(frame>=next_heap_frame){
        struct mallinfo heap=mallinfo();
        trace("HEAP frame=%u state=%.48s used=%u free=%u arena=%u",frame,name,
            (unsigned)heap.uordblks,(unsigned)heap.fordblks,(unsigned)heap.arena);
        trace("GPU free=%u,%u,%u,%u total=%u,%u,%u,%u",
            (unsigned)vglMemFree(VGL_MEM_VRAM),(unsigned)vglMemFree(VGL_MEM_RAM),
            (unsigned)vglMemFree(VGL_MEM_PHYCONT),(unsigned)vglMemFree(VGL_MEM_BUDGET),
            (unsigned)vglMemTotal(VGL_MEM_VRAM),(unsigned)vglMemTotal(VGL_MEM_RAM),
            (unsigned)vglMemTotal(VGL_MEM_PHYCONT),(unsigned)vglMemTotal(VGL_MEM_BUDGET));
        next_heap_frame=frame+300;
    }
    if(frame%30==0){
        FILE *request=fopen(DATA_PATH "capture-frame.txt","r");
        if(request){
            fclose(request);remove(DATA_PATH "capture-frame.txt");
            uint8_t *pixels=vglMalloc(SCREEN_W*SCREEN_H*4);
            if(pixels){
                GLint bound;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&bound);
                glBindFramebuffer(GL_FRAMEBUFFER,0);glFinish();
                vglReadPixels(0,0,SCREEN_W,SCREEN_H,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
                glBindFramebuffer(GL_FRAMEBUFFER,bound);
                FILE *output=fopen(DATA_PATH "captured-frame.rgba","wb");
                if(output){fwrite(pixels,1,SCREEN_W*SCREEN_H*4,output);fclose(output);}
                vglFree(pixels);trace("CAPTURE frame=%u state=%.48s error=%X",frame,name,glGetError());
            }
        }
    }
    if(mission_started && step_loading && !strcmp(name,"GSLoading"))step_pending=frame;
    if(!strcmp(name,"GSLevel"))mission_started=0;
    static uintptr_t previous;
    if(!held && previous!=(uintptr_t)state && !strcmp(name,"GSLevel")) hold_empty=1;
    if(previous!=(uintptr_t)state || !strncmp(name,"GSLoading",9)) {
        trace("FRAME %u state=%p %.48s draws=%u vertices=%u fbo=%u written=%d time=%u flags=%u,%u,%u,%u,%u",
            frame,state,name,draws,vertices,fbo,written,*(unsigned *)(state+264),state[284],state[285],state[286],state[287],state[288]);
        previous=(uintptr_t)state;
        if(!strncmp(name,"GSLoading",9)) {
            GLint scissor[4],viewport[4],stencil_func,stencil_mask,stencil_ref;
            GLboolean color[4];
            glGetIntegerv(GL_SCISSOR_BOX,scissor);glGetIntegerv(GL_VIEWPORT,viewport);
            glGetIntegerv(GL_STENCIL_FUNC,&stencil_func);glGetIntegerv(GL_STENCIL_VALUE_MASK,&stencil_mask);glGetIntegerv(GL_STENCIL_REF,&stencil_ref);
            glGetBooleanv(GL_COLOR_WRITEMASK,color);
            trace("GL viewport=%d,%d,%d,%d scissor=%d %d,%d,%d,%d stencil=%d func=%X ref=%d mask=%X color=%d%d%d%d err=%X",
                viewport[0],viewport[1],viewport[2],viewport[3],glIsEnabled(GL_SCISSOR_TEST),scissor[0],scissor[1],scissor[2],scissor[3],
                glIsEnabled(GL_STENCIL_TEST),stencil_func,stencil_ref,stencil_mask,color[0],color[1],color[2],color[3],glGetError());
        }
    }
}
int mission_trace_force_present(void){return force_present;}
void mission_trace_after_present(void){
    if(step_pending){
        FILE *ready=fopen(DATA_PATH "frame-ready.txt","w");
        if(ready){fprintf(ready,"%d",step_pending);fclose(ready);}
        step_pending=0;
        uint64_t until=sceKernelGetProcessTimeWide()+30000000;
        while(sceKernelGetProcessTimeWide()<until){
            FILE *next=fopen(DATA_PATH "frame-continue.txt","r");
            if(next){fclose(next);remove(DATA_PATH "frame-continue.txt");break;}
            sceKernelDelayThread(10000);
        }
    }
    if(hold_empty){
        hold_empty=0;held=1;
        trace("EMPTY FRAME HOLD force_empty_present=%d",force_present);
        sceKernelDelayThread(5000000);
        trace("HOLD END");
    }
}
#endif
