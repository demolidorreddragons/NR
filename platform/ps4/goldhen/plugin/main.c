// GoldHEN PRX entry point: timing telemetry only.
// Pixel data is accepted only from a title adapter after a completed GNM readback.
#include <Common.h>
#include <stdbool.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <unistd.h>
#include "plugin_common.h"
#include "telemetry_bridge.h"

#define CAPTURE_DIR "/data/PS4NeuralEngine/captures"
attr_public const char *g_pluginName = "ps4_neural_engine";
attr_public const char *g_pluginDesc = "PS4 Neural Engine capture diagnostics";
attr_public const char *g_pluginAuth = "demolidorreddragons";
attr_public u32 g_pluginVersion = 0x00000A00;

int32_t sceGnmSubmitAndFlipCommandBuffers(uint32_t count, void *dcbGpuAddrs[],
 uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes,
 uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg);
HOOK_INIT(sceGnmSubmitAndFlipCommandBuffers);
static volatile bool g_recording=false, g_running=false;
static uint64_t g_frequency=0;

int32_t sceGnmSubmitAndFlipCommandBuffers_hook(uint32_t count, void *dcbGpuAddrs[],
 uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes,
 uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg) {
 if(g_recording) pne_telemetry_on_present(sceKernelGetProcessTimeCounter(),g_frequency,0,displayBufferIndex);
 return HOOK_CONTINUE(sceGnmSubmitAndFlipCommandBuffers,
  int32_t(*)(uint32_t,void**,uint32_t*,void**,uint32_t*,uint32_t,uint32_t,uint32_t,int64_t),
  count,dcbGpuAddrs,dcbSizesInBytes,ccbGpuAddrs,ccbSizesInBytes,videoOutHandle,displayBufferIndex,flipMode,flipArg);
}
static int open_pad(void) {
 int32_t user=0; OrbisUserServiceInitializeParams p; p.priority=ORBIS_KERNEL_PRIO_FIFO_LOWEST;
 sceUserServiceInitialize(&p); if(sceUserServiceGetInitialUser(&user)<0)return 0;
 int h=scePadOpen(user,0,0,0); if(h==ORBIS_PAD_ERROR_ALREADY_OPENED)h=scePadGetHandle(user,0,0);
 return h>0?h:0;
}
static void *input_thread(void *arg) {
 (void)arg; sleep(5); if(scePadInit()<0){g_running=false;return NULL;} bool was_down=false;
 while(g_running){int h=open_pad();OrbisPadData d;
  if(h>0&&scePadReadState(h,&d)==0&&d.connected){
   uint32_t chord=ORBIS_PAD_BUTTON_L3|ORBIS_PAD_BUTTON_L1|ORBIS_PAD_BUTTON_TRIANGLE;
   bool down=(d.buttons&chord)==chord;
   if(down&&!was_down){if(!g_recording){if(pne_telemetry_start(CAPTURE_DIR))g_recording=true;else NotifyStatic(TEX_ICON_SYSTEM,"PNE: capture start failed");}
    else{g_recording=false;pne_telemetry_stop();NotifyStatic(TEX_ICON_SYSTEM,"PNE: logs saved locally");}}
   was_down=down;
   uint32_t quit=ORBIS_PAD_BUTTON_L3|ORBIS_PAD_BUTTON_R3|ORBIS_PAD_BUTTON_L1|ORBIS_PAD_BUTTON_R1|ORBIS_PAD_BUTTON_SQUARE;
   if((d.buttons&quit)==quit)g_running=false;
  } usleep(50000);
 }
 if(g_recording){g_recording=false;pne_telemetry_stop();} scePthreadExit(NULL); return NULL;
}
s32 attr_public plugin_load(s32 argc,const char *argv[]){
 (void)argc;(void)argv;g_frequency=sceKernelGetProcessTimeCounterFrequency();g_running=true;
 OrbisPthread t;scePthreadCreate(&t,NULL,input_thread,NULL,"pne_input");HOOK32(sceGnmSubmitAndFlipCommandBuffers);
 NotifyStatic(TEX_ICON_SYSTEM,"PNE diagnostics ready: L3+L1+Triangle");return 0;
}
s32 attr_public plugin_unload(s32 argc,const char *argv[]){
 (void)argc;(void)argv;g_running=false;if(g_recording){g_recording=false;pne_telemetry_stop();}
 UNHOOK(sceGnmSubmitAndFlipCommandBuffers);return 0;
}
s32 attr_module_hidden module_start(s64 argc,const void *args){(void)argc;(void)args;return 0;}
s32 attr_module_hidden module_stop(s64 argc,const void *args){(void)argc;(void)args;return 0;}
