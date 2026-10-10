#include "../native/module.cpp"
#include "test_support.h"
using namespace EModLoader::FirstPerson;
namespace {
Json reply;
BE_Result replyResult=BE_Result_Ok;
BE_Result BE_CALL Reply(void*,const char*,BE_Result code,const char* body){replyResult=code;reply=Json::parse(body);return BE_Result_Ok;}
void BE_CALL LogFake(void*,const char*){}
void FP_ENGINE_CALL TailFake(void*,float,void*){}
void FP_ENGINE_CALL PushFake(void*,void*,void*){}
float FP_ENGINE_CALL DeltaFake(void*){return 0.016f;}
bool missing_header=false,missing_player=false;
int driver_registrations=0,effect_registrations=0;
bool effect_registered=false;
BE_Result BE_CALL ResolveMethodFake(void*,const BE_MethodDescriptorV1* descriptor,BE_ResolvedMethodV1* out){
    for(const auto& contract:g_contracts) {
        if(std::string_view(descriptor->method_name)!=contract.descriptor.method_name||
            std::string_view(descriptor->class_name)!=contract.descriptor.class_name||
            std::string_view(descriptor->namespace_name)!=contract.descriptor.namespace_name)continue;
        if(missing_player&&std::string_view(contract.key)=="player_controller.get_main_character")return BE_Result_NotFound;
        out->method_info=contract.key;
        out->method_pointer=std::string_view(contract.key)=="camera_manager.tail_late_tick"?reinterpret_cast<void*>(TailFake):
            std::string_view(contract.key)=="cinemachine.push_state"?reinterpret_cast<void*>(PushFake):
            std::string_view(contract.key)=="unity.time.unscaled_delta.get"?reinterpret_cast<void*>(DeltaFake):reinterpret_cast<void*>(uintptr_t(1));
        return BE_Result_Ok;
    }
    return BE_Result_NotFound;
}
BE_Result BE_CALL ResolveFieldFake(void*,const BE_FieldDescriptorV1* descriptor,BE_ResolvedFieldV1* out){
    if(std::string_view(descriptor->class_name)=="Vector3") {
        if(missing_header)return BE_Result_NotFound;
        out->offset=16;out->field_info=descriptor;return BE_Result_Ok;
    }
    const std::string_view name=descriptor->field_name;
    out->offset=16+(name=="RawPosition"?0:name=="RawOrientation"?12:name=="PositionCorrection"?28:
        name=="OrientationCorrection"?40:name=="Lens"?56:name=="FieldOfView"?0:name=="NearClipPlane"?4:name=="Dutch"?8:96);
    out->field_info=descriptor;return BE_Result_Ok;
}
BE_Result BE_CALL HookFake(void*,const char* owner,void* target,void*,void** next){
    if(std::string_view(owner)==kFrameOwner)++driver_registrations;
    else {
        CHECK(std::string_view(owner)==kModuleId);
        if(effect_registered)return BE_Result_Conflict;
        effect_registered=true;++effect_registrations;
    }
    *next=target;return BE_Result_Ok;
}
void SetupResolver(BE_HostApiV1& runtime){
    runtime.resolve_method=ResolveMethodFake;runtime.resolve_field=ResolveFieldFake;
    runtime.resolve_class=[](void*,const char*,const char*,const char*,BE_ResolvedClassV1* out){*out={reinterpret_cast<void*>(1),reinterpret_cast<void*>(1),reinterpret_cast<void*>(1)};return BE_Result_Ok;};
    runtime.runtime_invoke=[](void*,const void*,void*,void**,void** exception)->void*{*exception=nullptr;return nullptr;};
    runtime.object_unbox=[](void*,void* value){return value;};
    runtime.string_new=[](void*,const char*)->void*{return nullptr;};
    runtime.gchandle_new=[](void*,void*,int){return uint32_t(1);};
    runtime.gchandle_free=[](void*,uint32_t){};
    runtime.copy_managed_string=[](void*,const void*,char*,size_t){return 0;};
    runtime.create_hook=HookFake;
}
}
int main(){
    BE_ThirdPartyHostV1 host{sizeof(host),1,nullptr,kModuleId,"windows-x64",".",nullptr,nullptr,LogFake,Reply,nullptr,nullptr};
    const auto* api=BetterEndfield_GetThirdPartyModuleV1();
    CHECK(api->version==1&&std::string_view(api->id)==kModuleId);
    CHECK(api->initialize(&host,"[]")==BE_Result_InvalidArgument);
    CHECK(api->initialize(&host,"{\"fov\":999}")==BE_Result_InvalidArgument);
    CHECK(api->initialize(&host,"{}")==BE_Result_Ok);
    CHECK(api->initialize(&host,"{}")==BE_Result_Conflict);
    CHECK(api->configuration_changed("{\"hide_head\":\"yes\"}")==BE_Result_InvalidArgument);
    CHECK(api->configuration_changed("{\"fov\":null}")==BE_Result_InvalidArgument);
    CHECK(api->configuration_changed("{\"toggle_key\":1.5}")==BE_Result_InvalidArgument);
    CHECK(api->configuration_changed("{\"toggle_key\":4294967485}")==BE_Result_InvalidArgument);
    CHECK(g_first_person_fov==75&&g_first_person_hide_head);
    CHECK(api->configuration_changed("{\"fov\":92,\"toggle_key\":0,\"hide_head\":false}")==BE_Result_Ok);
    CHECK(g_first_person_fov==92&&!g_first_person_hide_head&&g_first_person_key==0);
    CHECK(api->on_message("1","{\"action\":\"status\"}")==BE_Result_Ok);
    CHECK(replyResult==BE_Result_Ok&&reply["phase"]=="waiting_runtime"&&!reply["active"].get<bool>());
    CHECK(api->on_message("2","{\"action\":\"toggle\"}")==BE_Result_Ok&&replyResult==BE_Result_NotReady);
    CHECK(api->on_message("3","{\"action\":\"unknown\"}")==BE_Result_Ok&&replyResult==BE_Result_InvalidArgument);
    api->shutdown();
    CHECK(!g_initialized&&!g_bootstrap.joinable());
    CHECK(api->on_message("4","{}")==BE_Result_NotReady);
    CHECK(api->initialize(&host,"{\"active\":true}")==BE_Result_Ok);
    CHECK(g_first_person_toggle_request&&!g_first_person_exit_request);
    api->shutdown();
    // A pending game-owned post-rotation exit survives worker shutdown.
    // The resident frame driver drains it only from the captured Unity thread.
    BE_HostApiV1 runtime{};g_host=&runtime;
    g_initialized=true;g_first_person_active=false;
    g_first_person.snapshot_exit_owned=true;
    g_first_person.snapshot_controller=nullptr;
    g_camera_unity_thread_id=CurrentThreadId();
    std::thread worker([]{Shutdown();});worker.join();
    CHECK(g_cleanup_pending&&g_first_person.snapshot_exit_owned);
    CHECK(!g_first_person_camera_enabled&&!g_bridge);
    g_original_tail_late_tick=TailFake;
    DetourTailLateTick(nullptr,0,nullptr);
    CHECK(!g_cleanup_pending&&!g_first_person.snapshot_exit_owned);
    CHECK(g_original_tail_late_tick==TailFake); // resident next never cleared
    // Exercise the real resolution and hook installer against fake metadata,
    // including absent headers/contracts and a second module generation.
    SetupResolver(runtime);g_host=&runtime;
    missing_header=true;CHECK(!ResolveContracts());missing_header=false;
    missing_player=true;CHECK(!ResolveContracts());missing_player=false;
    CHECK(ResolveContracts()&&g_first_person_contract_ready&&g_state_layout.ready);
    CHECK(g_state_layout.raw_orientation==12&&g_state_layout.lens==56&&g_state_layout.lens_near_clip==4);
    CHECK(InstallHooks()&&driver_registrations==2&&effect_registrations==1);
    CHECK(!InstallHooks()&&driver_registrations==2&&effect_registrations==1);
    effect_registered=false; // Host retirement of the ordinary effect owner
    CHECK(InstallHooks()&&driver_registrations==2&&effect_registrations==2);
    g_initialized=true;g_bridge=&host;g_phase="ready";g_first_person_camera_enabled=true;
    CHECK(Message("5","{\"action\":\"toggle\"}")==BE_Result_Ok&&replyResult==BE_Result_Ok&&g_first_person_toggle_request);
    CHECK(Message("6","{\"action\":\"exit\"}")==BE_Result_Ok&&replyResult==BE_Result_Ok&&!g_first_person_toggle_request&&g_first_person_exit_request);
    CHECK(Configure("{\"enabled\":false}")==BE_Result_Ok&&!g_first_person_camera_enabled);
    CHECK(Message("7","{\"action\":\"toggle\"}")==BE_Result_Ok&&replyResult==BE_Result_NotReady);
    g_initialized=false;g_bridge=nullptr;
    g_host=nullptr;
    std::cout<<"PASS first-person V1 configuration/offline/lifecycle: "<<checks<<" checks\n";
}
