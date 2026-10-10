#pragma once
#include "hook_chain.h"
#if defined(_WIN32)
#include <MinHook.h>
#else
#include <dobby.h>
#include "core/code_patch_posix.h"
#endif

namespace EML {
class HookService {
    std::unique_ptr<EModLoader::Hooks::Chain> chain_;
    BE_HookChainApiV1 api_{};
    static BE_Result BE_CALL Create(void* context,const char* owner,void* target,void* detour,void** next,uint64_t* handle) {
        return static_cast<HookService*>(context)->chain_->Create(owner,target,detour,next,handle);
    }
    static BE_Result BE_CALL Disable(void* context,uint64_t handle) {
        return static_cast<HookService*>(context)->chain_->Disable(handle);
    }
    static BE_Result BE_CALL DisableModule(void* context,const char* owner) {
        return static_cast<HookService*>(context)->chain_->DisableModule(owner);
    }
public:
    bool Initialize() {
#if defined(_WIN32)
        const auto result=MH_Initialize();
        if(result!=MH_OK&&result!=MH_ERROR_ALREADY_INITIALIZED)return false;
        chain_=std::make_unique<EModLoader::Hooks::Chain>(EModLoader::Hooks::Backend{
            [](void* target,void* detour,void** original){return MH_CreateHook(target,detour,original)==MH_OK;},
            [](void* target){return MH_EnableHook(target)==MH_OK;},
            [](void* target){MH_RemoveHook(target);},
            [](void* target){MH_DisableHook(target);}});
#else
        chain_=std::make_unique<EModLoader::Hooks::Chain>(EModLoader::Hooks::Backend{
            [](void* target,void* detour,void** original){ResetCodePatchFailure();bool ready=DobbyPrepare(target,detour,original)==0;if(ready&&CodePatchFailed()){DobbyDestroy(target);return false;}return ready;},
            [](void* target){ResetCodePatchFailure();return DobbyCommit(target)==0&&!CodePatchFailed();},
            [](void* target){DobbyDestroy(target);},[](void*){/* Process-pinned original trampoline. */}});
#endif
        api_={sizeof(api_),1,this,Create,Disable,DisableModule};
        return true;
    }
    const BE_HookChainApiV1* Api() const {return chain_?&api_:nullptr;}
    BE_Result CreateForModule(const char* owner,void* target,void* detour,void** next) {
        if(!chain_)return BE_Result_NotReady;uint64_t handle=0;
        return chain_->Create(owner,target,detour,next,&handle,EModLoader::Hooks::Chain::Duplicate::Reject);
    }
};
}
