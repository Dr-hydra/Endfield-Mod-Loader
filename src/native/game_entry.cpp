#include "runtime/module_runtime.h"
#include "runtime/game_services.h"
#include <atomic>
#include <chrono>
#include <thread>

namespace {
std::atomic_bool started{false};
DWORD WINAPI Bootstrap(void* rawModule) {
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(rawModule),&pinned))return 1;
    wchar_t path[32768]{};
    if(!GetEnvironmentVariableW(L"EML_MODULE_INDEX",path,32768))return 2;
    const std::filesystem::path index(path);
    auto log=[root=index.parent_path()](const std::string& id,const std::string& text){
        try{std::ofstream stream(root/"host.log",std::ios::app);stream<<id<<": "<<text<<'\n';}catch(...){} };
    auto* hooks=new EML::HookService;
    if(!hooks->Initialize()){log("host","Hook service unavailable");return 3;}
    auto* host=new EML::ModuleRuntime;
    if(!host->Start(index,"windows-x64",log,nullptr,hooks->Api(),"game")){log("host","Module runtime failed to start");return 4;}
    // As in the original Host, base modules can start before engine helpers.
    // The platform runtime becoming visible precedes domain/GC readiness.
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(90);
    while(!GetModuleHandleW(L"GameAssembly.dll")){
        if(std::chrono::steady_clock::now()>=deadline){log("runtime","GameAssembly readiness timeout; optional helpers unavailable");return 0;}
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));
    auto* services=new EML::GameServices(*hooks,index,log);
    std::string error;
    while(!services->Initialize(error)){
        if(std::chrono::steady_clock::now()>=deadline){log("runtime",error);return 0;}
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    services->Detach();host->SetRuntime(services->Api());log("runtime","Optional engine helpers ready");
    return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH&&!started.exchange(true)){
        DisableThreadLibraryCalls(instance);
        HANDLE thread=CreateThread(nullptr,0,Bootstrap,instance,0,nullptr);if(thread)CloseHandle(thread);
    }
    return TRUE;
}
