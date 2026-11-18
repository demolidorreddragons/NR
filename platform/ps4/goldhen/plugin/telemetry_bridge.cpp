#include "telemetry_bridge.h"
#include "pne/telemetry.hpp"
#include <cstdio>
#include <memory>
#include <string>
static std::unique_ptr<pne::telemetry::Writer> g_writer;
static std::string g_root;
extern "C" int pne_telemetry_start(const char* root) {
 if(!root||!*root)return 0;
 if(g_writer&&g_writer->active())return 1;
 g_root=root;g_writer.reset(new pne::telemetry::Writer(g_root));
 if(!g_writer->start()){g_writer.reset();return 0;}return 1;
}
extern "C" void pne_telemetry_stop(void){if(g_writer)g_writer->stop();g_writer.reset();}
extern "C" void pne_telemetry_on_present(uint64_t t,uint64_t f,uint32_t id,uint32_t b){
 if(g_writer&&g_writer->active())g_writer->append_present({t,f,id,b});
}
extern "C" int pne_adapter_submit_readback(const uint8_t* rgba,uint32_t w,uint32_t h,size_t pitch,uint32_t frame){
 if(!g_writer||!g_writer->active()||!rgba)return 0;
 char name[64];std::snprintf(name,sizeof(name),"frame_%08u.bmp",frame);
 return g_writer->write_rgba8_bmp(g_root+"/"+name,rgba,w,h,pitch)?1:0;
}
