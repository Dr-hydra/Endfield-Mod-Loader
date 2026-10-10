#include <Windows.h>
#include <nlohmann/json.hpp>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
HMODULE proxyModule=nullptr;
INIT_ONCE systemOnce=INIT_ONCE_STATIC_INIT;
std::array<void*,15> targets{};
DWORD WINAPI Unavailable(){return ERROR_DEVICE_NOT_CONNECTED;}
BOOL CALLBACK LoadSystem(PINIT_ONCE,void*,void**) {
    wchar_t folder[32768]{};
    HMODULE library=nullptr;
    if(GetSystemDirectoryW(folder,32768)) {
        const auto path=std::filesystem::path(folder)/L"xinput1_4.dll";
        library=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    constexpr WORD ordinals[]={1,2,3,4,5,7,8,10,100,101,102,103,104,108,109};
    for(size_t i=0;i<targets.size();++i) {
        auto address=library?GetProcAddress(library,MAKEINTRESOURCEA(ordinals[i])):nullptr;
        targets[i]=address?reinterpret_cast<void*>(address):reinterpret_cast<void*>(Unavailable);
    }
    return TRUE;
}
std::filesystem::path UtfPath(const std::string& text) {
    return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(text.data()),text.size()));
}
DWORD WINAPI Bootstrap(void*) {
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Bootstrap),&pinned))return 1;
    try {
        wchar_t path[32768]{};
        if(!GetModuleFileNameW(proxyModule,path,32768))return 2;
        const auto config=std::filesystem::path(path).parent_path()/L"eml-xinput.json";
        if(!std::filesystem::is_regular_file(config)||std::filesystem::file_size(config)>16384)return 3;
        std::ifstream input(config);auto json=nlohmann::json::parse(input);
        if(json.value("product","")!="EndfieldModLoader"||json.value("schema",0)!=1)return 4;
        const auto host=UtfPath(json.at("host_path").get<std::string>());
        const auto index=UtfPath(json.at("index_path").get<std::string>());
        if(!host.is_absolute()||!index.is_absolute()||!std::filesystem::is_regular_file(host)||!std::filesystem::is_regular_file(index))return 5;
        if(!SetEnvironmentVariableW(L"EML_MODULE_INDEX",index.c_str()))return 6;
        if(!LoadLibraryExW(host.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS))return 7;
        return 0;
    }catch(...){OutputDebugStringW(L"E Mod Loader: DLL startup configuration failed.\n");return 8;}
}
}
extern "C" void* EMLProxyTarget(unsigned index) {
    const auto previous=GetLastError();
    InitOnceExecuteOnce(&systemOnce,LoadSystem,nullptr,nullptr);
    auto* target=index<targets.size()?targets[index]:reinterpret_cast<void*>(Unavailable);
    SetLastError(previous);return target;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        proxyModule=instance;DisableThreadLibraryCalls(instance);
        HANDLE thread=CreateThread(nullptr,0,Bootstrap,nullptr,0,nullptr);if(thread)CloseHandle(thread);
    }
    return TRUE;
}
