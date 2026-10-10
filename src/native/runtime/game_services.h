#pragma once
#include "engine_resolver.h"
#include "module_runtime.h"
#include "../hooks/hook_service.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace EML {
class GameServices {
    EModLoader::Host::EngineResolver engine_;
    HookService& hooks_;
    std::filesystem::path index_;
    ModuleRuntime::Log log_;
    BE_HostApiV1 api_{};
    static GameServices& Self(void* context){return *static_cast<GameServices*>(context);}
    static int Copy(const std::string& value,char* out,size_t capacity){if(!out||!capacity)return 0;auto count=std::min(value.size(),capacity-1);std::memcpy(out,value.data(),count);out[count]=0;return static_cast<int>(count);}
public:
    GameServices(HookService& hooks,std::filesystem::path index,ModuleRuntime::Log log):hooks_(hooks),index_(std::move(index)),log_(std::move(log)) {
        api_.abi_version=1;api_.context=this;
        api_.log=[](void* c,const char* id,const char* message){Self(c).log_(id?id:"module",message?message:"");};
        api_.resolve_method=[](void* c,const BE_MethodDescriptorV1* descriptor,BE_ResolvedMethodV1* result){if(!descriptor||!result)return BE_Result_InvalidArgument;std::string error;return Self(c).engine_.ResolveMethod(*descriptor,*result,error);};
        api_.resolve_field=[](void* c,const BE_FieldDescriptorV1* descriptor,BE_ResolvedFieldV1* result){if(!descriptor||!result)return BE_Result_InvalidArgument;std::string error;return Self(c).engine_.ResolveField(*descriptor,*result,error);};
        api_.create_hook=[](void* c,const char* id,void* target,void* detour,void** next){return Self(c).hooks_.CreateForModule(id,target,detour,next);};
        api_.release_module_hooks=[](void* c,const char* id){auto* api=Self(c).hooks_.Api();return api?api->disable_module(api->context,id):BE_Result_NotReady;};
        api_.copy_module_configuration=[](void* c,const char* id,char* out,size_t capacity){try{if(!id)return 0;std::ifstream input(Self(c).index_);auto index=nlohmann::json::parse(input);for(const auto& item:index.at("modules"))if(item.at("id")==id)return Copy(item.value("configuration",nlohmann::json::object()).dump(),out,capacity);}catch(...){}return 0;};
        api_.copy_catalog_root=[](void* c,char* out,size_t capacity){auto value=(Self(c).index_.parent_path()/"catalog").u8string();return Copy(std::string(reinterpret_cast<const char*>(value.data()),value.size()),out,capacity);};
        api_.copy_managed_string=[](void* c,const void* value,char* out,size_t capacity){return Self(c).engine_.CopyManagedString(value,out,capacity);};
        api_.resolve_class=[](void* c,const char* assembly,const char* ns,const char* name,BE_ResolvedClassV1* result){if(!assembly||!ns||!name||!result)return BE_Result_InvalidArgument;std::string error;return Self(c).engine_.ResolveClass(assembly,ns,name,*result,error);};
        api_.object_new=[](void* c,const void* type){return Self(c).engine_.ObjectNew(type);};
        api_.string_new=[](void* c,const char* text){return Self(c).engine_.StringNew(text);};
        api_.runtime_invoke=[](void* c,const void* method,void* object,void** args,void** error){return Self(c).engine_.RuntimeInvoke(method,object,args,error);};
        api_.object_unbox=[](void* c,void* object){return Self(c).engine_.ObjectUnbox(object);};
        api_.gchandle_new=[](void* c,void* object,int pin){return Self(c).engine_.GCHandleNew(object,pin!=0);};
        api_.gchandle_free=[](void* c,uint32_t handle){Self(c).engine_.GCHandleFree(handle);};
        api_.field_get_value_object=[](void* c,const void* field,void* object){return Self(c).engine_.FieldGetValueObject(field,object);};
    }
    bool Initialize(std::string& error){return engine_.Initialize(error)&&engine_.AttachCurrentThread(error);}
    void Detach(){engine_.DetachCurrentThread();}
    const BE_HostApiV1* Api() const{return &api_;}
};
}
