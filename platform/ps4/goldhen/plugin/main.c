// GoldHEN PRX entry point: opt-in timing telemetry.
// Frame pixels are accepted only from a title adapter after completed GNM readback.
#include <Common.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <unistd.h>
#include "plugin_common.h"
#include "telemetry_bridge.h"

#define CAPTURE_DIR "/data/PS4NeuralEngine/captures"
attr_public const char *g_pluginName = "ps4_neural_engine";
attr_public const char *g_pluginDesc = "PS4 Neural Engine capture diagnostics";
attr_public const char *g_pluginAuth = "demolidorreddragons";
attr_public u32 g_pluginVersion = 0x00000A01;

int32_t sceGnmSubmitAndFlipCommandBuffers(uint32_t count, void *dcbGpuAddrs[],
 uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes,
 uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg);
HOOK_INIT(sceGnmSubmitAndFlipCommandBuffers);

static atomic_bool g_recording = ATOMIC_VAR_INIT(false);
static atomic_bool g_running = ATOMIC_VAR_INIT(false);
static uint64_t g_frequency = 0;
static OrbisPthread g_input_thread;

int32_t sceGnmSubmitAndFlipCommandBuffers_hook(uint32_t count, void *dcbGpuAddrs[],
 uint32_t *dcbSizesInBytes, void *ccbGpuAddrs[], uint32_t *ccbSizesInBytes,
 uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg) {
 if (atomic_load_explicit(&g_recording, memory_order_relaxed))
  pne_telemetry_on_present(sceKernelGetProcessTimeCounter(), g_frequency, 0, displayBufferIndex);
 return HOOK_CONTINUE(sceGnmSubmitAndFlipCommandBuffers,
  int32_t(*)(uint32_t,void**,uint32_t*,void**,uint32_t*,uint32_t,uint32_t,uint32_t,int64_t),
  count,dcbGpuAddrs,dcbSizesInBytes,ccbGpuAddrs,ccbSizesInBytes,videoOutHandle,displayBufferIndex,flipMode,flipArg);
}

static int open_pad(void) {
 int32_t user = 0;
 if (sceUserServiceGetInitialUser(&user) < 0) return 0;
 int h = scePadOpen(user, 0, 0, 0);
 if (h == ORBIS_PAD_ERROR_ALREADY_OPENED) h = scePadGetHandle(user, 0, 0);
 return h > 0 ? h : 0;
}

static void *input_thread(void *arg) {
 (void)arg;
 sleep(5);
 OrbisUserServiceInitializeParams params = {0};
 params.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
 (void)sceUserServiceInitialize(&params);
 if (scePadInit() < 0) {
  NotifyStatic(TEX_ICON_SYSTEM, "PNE: controller initialization failed");
  atomic_store(&g_running, false);
  return NULL;
 }
 bool was_down = false;
 int handle = 0;
 while (atomic_load(&g_running)) {
  if (handle <= 0) handle = open_pad();
  OrbisPadData data = {0};
  if (handle > 0 && scePadReadState(handle, &data) == 0 && data.connected) {
   const uint32_t toggle = ORBIS_PAD_BUTTON_L3 | ORBIS_PAD_BUTTON_L1 | ORBIS_PAD_BUTTON_TRIANGLE;
   const bool down = (data.buttons & toggle) == toggle;
   if (down && !was_down) {
    if (!atomic_load(&g_recording)) {
     if (pne_telemetry_start(CAPTURE_DIR)) {
      atomic_store(&g_recording, true);
      NotifyStatic(TEX_ICON_SYSTEM, "PNE: capture started");
     } else NotifyStatic(TEX_ICON_SYSTEM, "PNE: capture start failed");
    } else {
     atomic_store(&g_recording, false);
     pne_telemetry_stop();
     NotifyStatic(TEX_ICON_SYSTEM, "PNE: logs saved locally");
    }
   }
   was_down = down;
   const uint32_t quit = ORBIS_PAD_BUTTON_L3 | ORBIS_PAD_BUTTON_R3 |
    ORBIS_PAD_BUTTON_L1 | ORBIS_PAD_BUTTON_R1 | ORBIS_PAD_BUTTON_SQUARE;
   if ((data.buttons & quit) == quit) atomic_store(&g_running, false);
  } else {
   was_down = false;
   if (handle > 0) handle = 0;
  }
  sceKernelUsleep(50000);
 }
 if (atomic_exchange(&g_recording, false)) pne_telemetry_stop();
 return NULL;
}

s32 attr_public plugin_load(s32 argc, const char *argv[]) {
 (void)argc; (void)argv;
 g_frequency = sceKernelGetProcessTimeCounterFrequency();
 if (!g_frequency) return -1;
 atomic_store(&g_running, true);
 HOOK32(sceGnmSubmitAndFlipCommandBuffers);
 if (scePthreadCreate(&g_input_thread, NULL, input_thread, NULL, "pne_input") != 0) {
  atomic_store(&g_running, false);
  UNHOOK(sceGnmSubmitAndFlipCommandBuffers);
  return -1;
 }
 NotifyStatic(TEX_ICON_SYSTEM, "PNE diagnostics ready: L3+L1+Triangle");
 return 0;
}

s32 attr_public plugin_unload(s32 argc, const char *argv[]) {
 (void)argc; (void)argv;
 UNHOOK(sceGnmSubmitAndFlipCommandBuffers);
 atomic_store(&g_running, false);
 scePthreadJoin(g_input_thread, NULL);
 if (atomic_exchange(&g_recording, false)) pne_telemetry_stop();
 return 0;
}

s32 attr_module_hidden module_start(s64 argc, const void *args) {
 (void)argc; (void)args; return 0;
}
s32 attr_module_hidden module_stop(s64 argc, const void *args) {
 (void)argc; (void)args; return 0;
}
