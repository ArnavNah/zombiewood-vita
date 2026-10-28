#include "performance.h"
#ifdef PORT_PERF_TRACE
#include "config.h"
#include "patch.h"
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <so_util/so_util.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/sysmem.h>
#include <kubridge.h>
#ifdef PORT_TEST_INPUT
#include <vitaGL.h>
#endif
extern so_module so_mod;
static uint64_t begin,input_end,game_end,present_end;
static uint64_t times[4],native[4],draw_time;
static unsigned frames,draws,vertices;
#ifdef PORT_PERF_NATIVE
static int (*original_level)(void *,unsigned);
static int (*original_ui)(void *,int);
static int (*original_scene)(void *,float,int);
static int (*original_render)(void *,void *,int);
static void **settings_instance;
static unsigned samples;
static int sample_frame;
#define MEASURE(slot,expression) ({ int result; if(sample_frame){uint64_t t=sceKernelGetProcessTimeWide();result=(expression);native[slot]+=sceKernelGetProcessTimeWide()-t;}else result=(expression);result; })
static int level_update(void *p,unsigned dt){return MEASURE(0,original_level(p,dt));}
static int ui_update(void *p,int dt){return MEASURE(1,original_ui(p,dt));}
static int scene_update(void *p,float dt,int flag){return MEASURE(2,original_scene(p,dt,flag));}
static int scene_render(void *p,void *driver,int flag){return MEASURE(3,original_render(p,driver,flag));}
static void flush_veneer(void *p){
#ifdef USE_KUBRIDGE
    kuKernelFlushCaches(p,8);
#else
    extern SceUID vm_blk;
    sceKernelSyncVMDomain(vm_blk,p,8);
#endif
}
static int patch_call(uint32_t offset,uint16_t expected0,uint16_t expected1,uintptr_t wrapper){
    uint16_t *instruction=(void *)(so_mod.load_addr+offset);
    if(instruction[0]!=expected0 || instruction[1]!=expected1)return 0;
    uintptr_t veneer=(so_mod.patch_head+3)&~3u;
    if(veneer+8>so_mod.patch_base+so_mod.patch_size)return 0;
    int32_t relative=(int32_t)(veneer-(uintptr_t)instruction-4);
    if(relative<-(1<<24) || relative>=(1<<24))return 0;
    uint16_t code[4]={0xF8DF,0xF000,(uint16_t)wrapper,(uint16_t)(wrapper>>16)};
    sceClibMemcpy((void *)veneer,code,sizeof(code));flush_veneer((void *)veneer);
    so_mod.patch_head=veneer+8;
    uint32_t imm=(uint32_t)relative;
    unsigned s=(imm>>24)&1,j1=(~((imm>>23)^s))&1,j2=(~((imm>>22)^s))&1;
    uint16_t branch[2]={0xF000|(s<<10)|((imm>>12)&1023),0xD000|(j1<<13)|(j2<<11)|((imm>>1)&2047)};
    sceClibMemcpy(instruction,branch,sizeof(branch));return 1;
}
static unsigned replace_vtable(uintptr_t original,uintptr_t replacement){
    uintptr_t *table=(void *)so_symbol(&so_mod,"_ZTVN6glitch5scene13CSceneManagerE");
    unsigned patched=0;
    if(table)for(unsigned i=0;i<0xC8/4;++i)if(table[i]==original){table[i]=replacement;++patched;}
    return patched;
}
#endif
void performance_init(void){
    // Removing first also avoids stale trailing records in emulator filesystems
    // whose truncate implementation keeps a prior file's tail.
    sceIoRemove(DATA_PATH "performance.csv");
    sceIoRemove(DATA_PATH "native-profile-info.txt");
#ifdef PORT_PERF_NATIVE
    original_level=(void *)so_symbol(&so_mod,"_ZN6CLevel6UpdateEj");
    original_ui=(void *)so_symbol(&so_mod,"_ZN7CGameUI6UpdateEi");
    original_scene=(void *)so_symbol(&so_mod,"_ZN6glitch5scene13CSceneManager6updateEfb");
    original_render=(void *)so_symbol(&so_mod,"_ZN6glitch5scene13CSceneManager11renderListsEPNS_5video12IVideoDriverEb");
    settings_instance=(void *)so_symbol(&so_mod,"_ZN13SingletonFastI12GameSettingsE10s_instanceE");
    unsigned patched=patch_call(0x4A1FE4,0xF77F,0xFC32,(uintptr_t)level_update);
    patched+=patch_call(0x421A22,0xF019,0xF95D,(uintptr_t)ui_update);
    patched+=replace_vtable((uintptr_t)original_scene,(uintptr_t)scene_update);
    patched+=replace_vtable((uintptr_t)original_render,(uintptr_t)scene_render);
    FILE *report=fopen(DATA_PATH "native-profile-info.txt","w");
    if(report){fprintf(report,"instrumented=%u\n",patched);fclose(report);}
#endif
    SceUID fd=sceIoOpen(DATA_PATH "performance.csv",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(fd>=0){
        const char *header="milliseconds,state,frames,input_us,game_us,present_us,total_us,level_us,ui_us,indicators_us,render_us,draw_us,draws,vertices\n";
        sceIoWrite(fd,header,strlen(header));sceIoClose(fd);
    }
}
void performance_begin(void){
#ifdef PORT_PERF_NATIVE
    sample_frame=(frames%8)==0;
    if(sample_frame)++samples;
#endif
    begin=sceKernelGetProcessTimeWide();
}
void performance_input_done(void){input_end=sceKernelGetProcessTimeWide();}
void performance_game_done(void){
#ifdef PORT_TEST_INPUT
    if(frames%60==0) {
        FILE *request=fopen(DATA_PATH "capture-frame.txt","r");
        if(request) {
            fclose(request);remove(DATA_PATH "capture-frame.txt");
            void *pixels=vglMalloc(SCREEN_W*SCREEN_H*4);
            if(pixels) {
                GLint fbo;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&fbo);
                glBindFramebuffer(GL_FRAMEBUFFER,0);glFinish();
                vglReadPixels(0,0,SCREEN_W,SCREEN_H,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
                glBindFramebuffer(GL_FRAMEBUFFER,fbo);
                FILE *output=fopen(DATA_PATH "captured-frame.rgba","wb");
                if(output){fwrite(pixels,1,SCREEN_W*SCREEN_H*4,output);fclose(output);}
                vglFree(pixels);
            }
        }
    }
#endif
    game_end=sceKernelGetProcessTimeWide();
}
void performance_present_done(void){present_end=sceKernelGetProcessTimeWide();}
void performance_draw(unsigned us,unsigned count){draw_time+=us;++draws;vertices+=count;}
void performance_frame_done(void){
    uint64_t end=sceKernelGetProcessTimeWide();
    times[0]+=input_end-begin;times[1]+=game_end-input_end;
    times[2]+=present_end-game_end;times[3]+=end-begin;
    if(++frames<300)return;
#ifdef PORT_PERF_NATIVE
    for(unsigned i=0;i<4;++i)native[i]=samples?native[i]*frames/samples:0;
    FILE *report=fopen(DATA_PATH "native-profile-info.txt","a");
    if(report){
        int (*cpu)(void)=(void *)so_symbol(&so_mod,"GetProfileCPU");
        int (*gpu)(void)=(void *)so_symbol(&so_mod,"GetProfileGPU");
        int (*cores)(void)=(void *)so_symbol(&so_mod,"nativeGetNumberOfCPUCores");
        int (*detect)(void *)=(void *)so_symbol(&so_mod,"_ZN12GameSettings17GetAndroidProfileEv");
        void *settings=settings_instance?*settings_instance:NULL;
        fprintf(report,"state=%s android_profile=%d detected=%d cpu=%d gpu=%d cores=%d\n",game_state_name(),settings?*(int *)((uint8_t *)settings+240):-1,settings && detect?detect(settings):-1,cpu?cpu():-1,gpu?gpu():-1,cores?cores():-1);
        fclose(report);
    }
    samples=0;
#endif
    char line[512];
    int len=snprintf(line,sizeof(line),"%llu,%s,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u,%u\n",
        end/1000,game_state_name(),frames,times[0]/frames,times[1]/frames,times[2]/frames,times[3]/frames,
        native[0]/frames,native[1]/frames,native[2]/frames,native[3]/frames,draw_time/frames,draws/frames,vertices/frames);
    SceUID fd=sceIoOpen(DATA_PATH "performance.csv",SCE_O_WRONLY|SCE_O_APPEND,0666);
    if(fd>=0){sceIoWrite(fd,line,len);sceIoClose(fd);}
    frames=draws=vertices=0;draw_time=0;memset(times,0,sizeof(times));memset(native,0,sizeof(native));
}
#endif
