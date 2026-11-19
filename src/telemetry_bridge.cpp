#include "pne/telemetry.hpp"
#include "../platform/ps4/goldhen/plugin/telemetry_bridge.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
enum Kind { Present, Frame };
struct Item {
    Kind kind;
    pne::telemetry::FrameInfo info;
    uint32_t width, height, number;
    size_t pitch, bytes;
    uint8_t* pixels;
};
pthread_mutex_t lifecycle = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
pthread_cond_t startup = PTHREAD_COND_INITIALIZER;
pthread_t worker;
Item queue[8];
size_t head=0, count=0, queued_bytes=0;
bool worker_valid=false, running=false, startup_done=false, startup_ok=false;
char root[1024];
const size_t max_queued_bytes=64u*1024u*1024u;

void* run_worker(void*) {
    pne::telemetry::Writer writer(root);
    const bool opened=writer.start();
    pthread_mutex_lock(&lock);
    startup_ok=opened; startup_done=true;
    if(!opened) running=false;
    pthread_cond_broadcast(&startup);
    pthread_mutex_unlock(&lock);
    if(!opened) return nullptr;
    for(;;) {
        pthread_mutex_lock(&lock);
        while(count==0 && running) pthread_cond_wait(&changed,&lock);
        if(count==0 && !running) { pthread_mutex_unlock(&lock); break; }
        Item item=queue[head];
        head=(head+1)%8; --count;
        if(item.kind==Frame) queued_bytes-=item.bytes;
        pthread_mutex_unlock(&lock);
        if(item.kind==Present) (void)writer.append_present(item.info);
        else {
            char path[1200];
            if(std::snprintf(path,sizeof(path),"%s/frame_%08u.bmp",root,item.number)<(int)sizeof(path))
                (void)writer.write_rgba8_bmp(path,item.pixels,item.width,item.height,item.pitch);
            free(item.pixels);
        }
    }
    writer.stop();
    return nullptr;
}

bool enqueue(Item* item) {
    if(pthread_mutex_trylock(&lock)!=0) return false;
    if(!running || count>=8 || (item->kind==Frame && item->bytes>max_queued_bytes-queued_bytes)) {
        pthread_mutex_unlock(&lock); return false;
    }
    queue[(head+count)%8]=*item;
    ++count;
    if(item->kind==Frame) queued_bytes+=item->bytes;
    pthread_cond_signal(&changed);
    pthread_mutex_unlock(&lock);
    item->pixels=nullptr;
    return true;
}
}

extern "C" int pne_telemetry_start(const char* directory) {
    if(!directory || !*directory || strlen(directory)>=sizeof(root)) return 0;
    pthread_mutex_lock(&lifecycle);
    pthread_mutex_lock(&lock);
    if(running) { pthread_mutex_unlock(&lock); pthread_mutex_unlock(&lifecycle); return 0; }
    pthread_mutex_unlock(&lock);
    if(worker_valid) { pthread_join(worker,nullptr); worker_valid=false; }
    pthread_mutex_lock(&lock);
    head=count=queued_bytes=0;
    startup_done=startup_ok=false;
    strcpy(root,directory);
    running=true;
    pthread_mutex_unlock(&lock);
    if(pthread_create(&worker,nullptr,run_worker,nullptr)!=0) {
        pthread_mutex_lock(&lock); running=false; pthread_mutex_unlock(&lock);
        pthread_mutex_unlock(&lifecycle); return 0;
    }
    worker_valid=true;
    pthread_mutex_lock(&lock);
    while(!startup_done) pthread_cond_wait(&startup,&lock);
    const bool ok=startup_ok;
    pthread_mutex_unlock(&lock);
    if(!ok) { pthread_join(worker,nullptr); worker_valid=false; }
    pthread_mutex_unlock(&lifecycle);
    return ok?1:0;
}

extern "C" void pne_telemetry_stop(void) {
    pthread_mutex_lock(&lifecycle);
    pthread_mutex_lock(&lock);
    running=false;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&lock);
    if(worker_valid) { pthread_join(worker,nullptr); worker_valid=false; }
    pthread_mutex_lock(&lock);
    head=count=queued_bytes=0;
    pthread_mutex_unlock(&lock);
    pthread_mutex_unlock(&lifecycle);
}

extern "C" void pne_telemetry_on_present(uint64_t ticks,uint64_t frequency,
    uint32_t title_id,uint32_t display_buffer) {
    Item item={}; item.kind=Present;
    item.info={ticks,frequency,title_id,display_buffer};
    (void)enqueue(&item);
}

extern "C" int pne_adapter_submit_readback(const uint8_t* rgba,uint32_t width,
    uint32_t height,size_t row_pitch,uint32_t frame_number) {
    if(!rgba || !width || !height || width>4096 || height>4096 ||
       row_pitch<(size_t)width*4 || row_pitch>(1u<<24) ||
       height>(max_queued_bytes/row_pitch)) return 0;
    const size_t bytes=row_pitch*(size_t)height;
    uint8_t* copy=(uint8_t*)malloc(bytes);
    if(!copy) return 0;
    memcpy(copy,rgba,bytes);
    Item item={}; item.kind=Frame; item.width=width; item.height=height;
    item.number=frame_number; item.pitch=row_pitch; item.bytes=bytes; item.pixels=copy;
    if(!enqueue(&item)) { free(copy); return 0; }
    return 1;
}
