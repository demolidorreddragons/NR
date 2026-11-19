#include "pne/github_oauth.hpp"
#include "plugin_common.h"
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Net.h>
#include <orbis/Sysmodule.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

namespace {
int g_pool=-1, g_ssl=-1, g_http=-1, g_template=-1;
pthread_mutex_t g_lock=PTHREAD_MUTEX_INITIALIZER;
std::string g_token;
bool g_authorized=false, g_auth_active=false, g_auth_joinable=false;
bool g_upload_active=false, g_upload_joinable=false, g_cancel_auth=false;
pthread_t g_auth_thread, g_upload_thread;
constexpr size_t kMaxResponse=65536;
constexpr size_t kMaxUpload=16*1024*1024;

bool cancelled() {
 pthread_mutex_lock(&g_lock);
 bool value=g_cancel_auth;
 pthread_mutex_unlock(&g_lock);
 return value;
}
bool init_http() {
 if(g_http>=0 && g_template>=0) return true;
 (void)sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);
 (void)sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL);
 (void)sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP);
 (void)sceNetInit();
 g_pool=sceNetPoolCreate("pne_github",64*1024,0);
 if(g_pool<0) return false;
 g_ssl=sceSslInit(304*1024);
 if(g_ssl<0) return false;
 g_http=sceHttpInit(g_pool,g_ssl,128*1024);
 if(g_http<0) return false;
 g_template=sceHttpCreateTemplate(g_http,"PS4-Neural-Engine/1.0",ORBIS_HTTP_VERSION_1_1,0);
 return g_template>=0;
}
void close_http() {
 if(g_template>=0) sceHttpDeleteTemplate(g_template);
 if(g_http>=0) sceHttpTerm(g_http);
 if(g_ssl>=0) sceSslTerm();
 if(g_pool>=0) sceNetPoolDestroy(g_pool);
 g_template=g_http=g_ssl=g_pool=-1;
 (void)sceSysmoduleUnloadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP);
 (void)sceSysmoduleUnloadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL);
 (void)sceSysmoduleUnloadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);
}
bool request(const char* url,const char* method,const char* body,
             const char* bearer,std::string& response,int& status) {
 response.clear(); status=0;
 if(!init_http()) return false;
 int conn=sceHttpCreateConnectionWithURL(g_template,url,0);
 if(conn<0) return false;
 (void)sceHttpSetConnectTimeOut(conn,15000000);
 (void)sceHttpSetSendTimeOut(conn,30000000);
 (void)sceHttpSetResolveTimeOut(conn,15000000);
 size_t body_len=body?strlen(body):0;
 int req=sceHttpCreateRequestWithURL2(conn,method,url,body_len);
 if(req<0){sceHttpDeleteConnection(conn);return false;}
 bool ok=sceHttpAddRequestHeader(req,"Accept","application/json",0)>=0;
 if(body) ok=ok && sceHttpAddRequestHeader(req,"Content-Type",
   strcmp(method,"PUT")==0 ? "application/json" : "application/x-www-form-urlencoded",0)>=0;
 if(strstr(url,"api.github.com")!=nullptr)
  ok=ok && sceHttpAddRequestHeader(req,"X-GitHub-Api-Version","2022-11-28",0)>=0;
 std::string auth_header;
 if(bearer && *bearer) {
  auth_header="Bearer ";auth_header+=bearer;
  ok=ok && sceHttpAddRequestHeader(req,"Authorization",auth_header.c_str(),0)>=0;
 }
 if(ok) ok=sceHttpSendRequest(req,body,body_len)>=0;
 if(ok) ok=sceHttpGetStatusCode(req,&status)>=0;
 if(ok) {
  char buf[4096];
  for(;;){int n=sceHttpReadData(req,buf,sizeof(buf));if(n<0){ok=false;break;}if(n==0)break;
   if(response.size()+(size_t)n>kMaxResponse){ok=false;break;}response.append(buf,(size_t)n);}
 }
 if(!auth_header.empty())memset(&auth_header[0],0,auth_header.size());
 sceHttpDeleteRequest(req);sceHttpDeleteConnection(conn);
 return ok;
}
bool json_string(const std::string& j,const char* key,std::string& out) {
 std::string k="\"";k+=key;k+="\"";
 size_t p=j.find(k);if(p==std::string::npos)return false;
 p=j.find(':',p+k.size());if(p==std::string::npos)return false;
 p=j.find('"',p+1);if(p==std::string::npos)return false;
 size_t e=j.find('"',p+1);if(e==std::string::npos)return false;
 out=j.substr(p+1,e-p-1);return true;
}
int json_int(const std::string& j,const char* key,int fallback) {
 std::string k="\"";k+=key;k+="\"";size_t p=j.find(k);
 if(p==std::string::npos)return fallback;p=j.find(':',p+k.size());
 if(p==std::string::npos)return fallback;return atoi(j.c_str()+p+1);
}
std::string form_encode(const char* s) {
 std::string o;const char* h="0123456789ABCDEF";
 for(;*s;++s){unsigned char c=(unsigned char)*s;
  if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')o+=(char)c;
  else if(c==' ')o+='+';
  else{o+='%';o+=h[c>>4];o+=h[c&15];}}
 return o;
}
std::string base64(const std::vector<unsigned char>& in) {
 static const char t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 std::string o;o.reserve((in.size()+2)/3*4);
 for(size_t i=0;i<in.size();i+=3){unsigned a=in[i],b=i+1<in.size()?in[i+1]:0,c=i+2<in.size()?in[i+2]:0;
  o+=t[a>>2];o+=t[((a&3)<<4)|(b>>4)];o+=i+1<in.size()?t[((b&15)<<2)|(c>>6)]:'=';
  o+=i+2<in.size()?t[c&63]:'=';}
 return o;
}
bool authorize() {
 if(!init_http())return false;
 std::string body="client_id="+form_encode(pne::github::kClientId)+"&scope="+form_encode(pne::github::kUploadScope);
 std::string r;int status=0;
 if(!request(pne::github::kDeviceCodeUrl,"POST",body.c_str(),nullptr,r,status)||status!=200)return false;
 std::string dc,uc,uri;
 if(!json_string(r,"device_code",dc)||!json_string(r,"user_code",uc))return false;
 if(!json_string(r,"verification_uri",uri))uri=pne::github::kVerificationUrl;
 int interval=json_int(r,"interval",5), expires=json_int(r,"expires_in",900);
 if(!r.empty())memset(&r[0],0,r.size());
 char note[256];snprintf(note,sizeof(note),"GitHub: %s | Acesse %s",uc.c_str(),uri.c_str());
 NotifyStatic(TEX_ICON_SYSTEM,note);
 for(int elapsed=0;elapsed<expires&&!cancelled();elapsed+=interval){
  for(int waited=0;waited<interval&&!cancelled();waited++)sceKernelUsleep(1000000ULL);
  if(cancelled())break;
  std::string poll="client_id="+form_encode(pne::github::kClientId)+
   "&device_code="+form_encode(dc.c_str())+
   "&grant_type="+form_encode(pne::github::kDeviceGrantType);
  if(!request(pne::github::kAccessTokenUrl,"POST",poll.c_str(),nullptr,r,status))return false;
  std::string token;
  if(json_string(r,"access_token",token)){
   if(!r.empty())memset(&r[0],0,r.size());
   pthread_mutex_lock(&g_lock);g_token=token;g_authorized=true;pthread_mutex_unlock(&g_lock);
   if(!token.empty())memset(&token[0],0,token.size());
   if(!dc.empty())memset(&dc[0],0,dc.size());
   return true;
  }
  bool pending=r.find("authorization_pending")!=std::string::npos;
  bool slow=r.find("slow_down")!=std::string::npos;
  bool denied=r.find("access_denied")!=std::string::npos||r.find("expired_token")!=std::string::npos;
  if(!r.empty())memset(&r[0],0,r.size());
  if(pending)continue;
  if(slow){interval+=5;continue;}
  if(denied)return false;
 }
 if(!dc.empty())memset(&dc[0],0,dc.size());
 return false;
}
bool upload_one(const char* local,const char* remote,const std::string& token) {
 FILE* f=fopen(local,"rb");if(!f)return false;
 if(fseek(f,0,SEEK_END)!=0){fclose(f);return false;}
 long n=ftell(f);if(n<=0||n>(long)kMaxUpload){fclose(f);return false;}
 rewind(f);std::vector<unsigned char> data((size_t)n);
 bool read=fread(data.data(),1,data.size(),f)==data.size();fclose(f);if(!read)return false;
 std::string encoded=base64(data);
 std::string url="https://api.github.com/repos/";url+=pne::github::kRepository;url+="/contents/";url+=remote;
 std::string old;int status=0;std::string sha;
 if(!request(url.c_str(),"GET",nullptr,token.c_str(),old,status))return false;
 if(status==200 && !json_string(old,"sha",sha))return false;
 if(status!=200 && status!=404)return false;
 std::string json="{\"message\":\"PS4 Neural Engine telemetry upload\",\"content\":\""+encoded+"\"";
 if(!sha.empty())json+=",\"sha\":\""+sha+"\"";
 json+="}";
 std::string response;
 bool ok=request(url.c_str(),"PUT",json.c_str(),token.c_str(),response,status);
 if(!encoded.empty())memset(&encoded[0],0,encoded.size());
 if(!json.empty())memset(&json[0],0,json.size());
 return ok&&(status==201||status==200);
}
bool upload_log() {
 std::string token;
 pthread_mutex_lock(&g_lock);if(g_authorized)token=g_token;pthread_mutex_unlock(&g_lock);
 if(token.empty())return false;
 bool any=false,ok=true;
 const char* csv="/data/PS4NeuralEngine/captures/frametimes.csv";
 if(access(csv,R_OK)==0){any=true;ok=upload_one(csv,"telemetry/frametimes.csv",token)&&ok;}
 DIR* d=opendir("/data/PS4NeuralEngine/captures");
 if(d){
  struct dirent* e;
  while((e=readdir(d))!=nullptr){
   const char* name=e->d_name;size_t len=strlen(name);
   if(len<5 || strcmp(name+len-4,".bmp")!=0)continue;
   bool safe=true;for(size_t i=0;i<len;i++){char c=name[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.')){safe=false;break;}}
   if(!safe)continue;
   char local[512],remote[512];
   if(snprintf(local,sizeof(local),"/data/PS4NeuralEngine/captures/%s",name)>=(int)sizeof(local))continue;
   if(snprintf(remote,sizeof(remote),"telemetry/captures/%s",name)>=(int)sizeof(remote))continue;
   any=true;if(!upload_one(local,remote,token))ok=false;
  }
  closedir(d);
 }
 if(!token.empty())memset(&token[0],0,token.size());
 return any&&ok;
}
void* upload_thread(void*) {
 bool ok=upload_log();
 NotifyStatic(TEX_ICON_SYSTEM,ok?"PNE: lote enviado ao GitHub":"PNE: falha ao enviar lote GitHub");
 pthread_mutex_lock(&g_lock);g_upload_active=false;pthread_mutex_unlock(&g_lock);
 return nullptr;
}
void* auth_thread(void*) {
 bool ok=authorize();
 NotifyStatic(TEX_ICON_SYSTEM,ok?"PNE: GitHub autorizado nesta sessao":(cancelled()?"PNE: OAuth cancelado":"PNE: autorizacao GitHub falhou"));
 pthread_mutex_lock(&g_lock);g_auth_active=false;pthread_mutex_unlock(&g_lock);
 return nullptr;
}
}
extern "C" int pne_github_begin_authorization(void) {
 pthread_mutex_lock(&g_lock);
 if(g_auth_active){pthread_mutex_unlock(&g_lock);return 0;}
 if(g_auth_joinable){pthread_t old=g_auth_thread;pthread_mutex_unlock(&g_lock);pthread_join(old,nullptr);pthread_mutex_lock(&g_lock);g_auth_joinable=false;}
 g_cancel_auth=false;g_auth_active=true;
 if(pthread_create(&g_auth_thread,nullptr,auth_thread,nullptr)!=0){g_auth_active=false;pthread_mutex_unlock(&g_lock);return 0;}
 g_auth_joinable=true;pthread_mutex_unlock(&g_lock);return 1;
}
extern "C" int pne_github_upload_confirmed_log(void) {
 pthread_mutex_lock(&g_lock);
 if(g_upload_active || !g_authorized){pthread_mutex_unlock(&g_lock);return 0;}
 if(g_upload_joinable){pthread_t old=g_upload_thread;pthread_mutex_unlock(&g_lock);pthread_join(old,nullptr);pthread_mutex_lock(&g_lock);g_upload_joinable=false;}
 g_upload_active=true;
 if(pthread_create(&g_upload_thread,nullptr,upload_thread,nullptr)!=0){g_upload_active=false;pthread_mutex_unlock(&g_lock);return 0;}
 g_upload_joinable=true;pthread_mutex_unlock(&g_lock);return 1;
}
extern "C" void pne_github_disconnect(void) {
 pthread_mutex_lock(&g_lock);g_cancel_auth=true;
 bool join=g_auth_joinable, upload_join=g_upload_joinable;
 pthread_t t=g_auth_thread, ut=g_upload_thread;
 pthread_mutex_unlock(&g_lock);
 if(join)pthread_join(t,nullptr);
 if(upload_join)pthread_join(ut,nullptr);
 pthread_mutex_lock(&g_lock);g_auth_joinable=false;g_upload_joinable=false;
 if(!g_token.empty())memset(&g_token[0],0,g_token.size());
 g_token.clear();g_authorized=false;pthread_mutex_unlock(&g_lock);
 close_http();
}
