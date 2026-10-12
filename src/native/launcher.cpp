#include <Windows.h>
#include <TlHelp32.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "proxy/deployment.h"

namespace {
HANDLE resultPipe=INVALID_HANDLE_VALUE;
bool OpenResultPipe(const std::wstring& name) {
    constexpr std::wstring_view prefix=L"EML.Launch.";
    if(!name.starts_with(prefix)||name.size()!=prefix.size()+32||
        !std::all_of(name.begin()+prefix.size(),name.end(),[](wchar_t c){return std::iswxdigit(c)!=0;}))return false;
    const auto path=L"\\\\.\\pipe\\"+name;
    resultPipe=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
    return resultPipe!=INVALID_HANDLE_VALUE;
}
bool WriteAll(const void* bytes,DWORD size) {
    auto* position=static_cast<const unsigned char*>(bytes);
    while(size){DWORD written=0;if(!WriteFile(resultPipe,position,size,&written,nullptr)||!written)return false;size-=written;position+=written;}
    return true;
}
int Finish(int code,DWORD error=0,DWORD pid=0,const std::string& message={}) {
    if(resultPipe!=INVALID_HANDLE_VALUE) {
        auto data=nlohmann::json{{"protocol","eml.launch.v1"},{"exit_code",code},{"win32_error",error},
            {"process_id",pid},{"error",message}}.dump();
        const auto length=static_cast<uint32_t>(data.size());
        if(WriteAll(&length,sizeof(length))&&WriteAll(data.data(),length))FlushFileBuffers(resultPipe);
        CloseHandle(resultPipe);resultPipe=INVALID_HANDLE_VALUE;
    } else if(code)std::cerr<<message<<'\n';
    else std::cout<<pid<<'\n';
    return code;
}
uintptr_t ModuleBase(DWORD pid,const wchar_t* name) {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(snapshot==INVALID_HANDLE_VALUE)return 0;MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);uintptr_t found=0;
    if(Module32FirstW(snapshot,&entry))do{if(_wcsicmp(entry.szModule,name)==0){found=reinterpret_cast<uintptr_t>(entry.modBaseAddr);break;}}while(Module32NextW(snapshot,&entry));
    CloseHandle(snapshot);return found;
}
void* TargetLoadLibrary(DWORD pid) {
    const auto local=reinterpret_cast<FARPROC>(&LoadLibraryW);HMODULE owner=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(local),&owner))return nullptr;
    wchar_t ownerPath[32768]{};if(!GetModuleFileNameW(owner,ownerPath,32768))return nullptr;
    auto base=ModuleBase(pid,std::filesystem::path(ownerPath).filename().c_str());
    return base?reinterpret_cast<void*>(base+reinterpret_cast<uintptr_t>(local)-reinterpret_cast<uintptr_t>(owner)):nullptr;
}
std::vector<wchar_t> Environment(const std::filesystem::path& index) {
    std::map<std::wstring,std::wstring> values;auto* raw=GetEnvironmentStringsW();
    if(raw){for(auto* item=raw;*item;item+=wcslen(item)+1){std::wstring value(item);auto split=value.find('=',value.starts_with('=')?1:0);if(split!=value.npos)values[value.substr(0,split)]=value.substr(split+1);}FreeEnvironmentStringsW(raw);}
    values[L"EML_MODULE_INDEX"]=index.wstring();std::vector<wchar_t> result;
    for(const auto& [key,value]:values){auto item=key+L"="+value;result.insert(result.end(),item.begin(),item.end());result.push_back(0);}result.push_back(0);return result;
}
}
int wmain(int argc,wchar_t** argv) {
    std::filesystem::path game,index,host,proxy;
    std::wstring pipe,operation;
    bool allowXInputConflict=false;
    if((argc-1)%2)return Finish(2,ERROR_INVALID_PARAMETER,0,"Invalid launcher arguments");
    for(int i=1;i+1<argc;i+=2){std::wstring_view option(argv[i]);if(option==L"--game")game=argv[i+1];else if(option==L"--index")index=argv[i+1];else if(option==L"--host")host=argv[i+1];else if(option==L"--result-pipe")pipe=argv[i+1];else if(option==L"--operation")operation=argv[i+1];else if(option==L"--proxy")proxy=argv[i+1];else if(option==L"--allow-xinput-conflict") {
        const std::wstring_view value(argv[i+1]);
        if(value!=L"true"&&value!=L"false")return Finish(2,ERROR_INVALID_PARAMETER,0,"Invalid XInput compatibility setting");
        allowXInputConflict=value==L"true";
    }else return Finish(2,ERROR_INVALID_PARAMETER,0,"Unknown launcher option");}
    // Establish the app-owned response channel before starting any process.
    if(!pipe.empty()&&!OpenResultPipe(pipe))return Finish(9,GetLastError(),0,"Launch result channel unavailable");
    if(!operation.empty()) {
        if(operation!=L"install-proxy"&&operation!=L"remove-proxy")return Finish(2,ERROR_INVALID_PARAMETER,0,"Unknown deployment operation");
        try {EML::Proxy::Deploy(operation==L"install-proxy",game,host,index,proxy);return Finish(0);}
        catch(const EML::Proxy::Failure& error){return Finish(10,error.error,0,error.what());}
        catch(const std::filesystem::filesystem_error& error){return Finish(10,static_cast<DWORD>(error.code().value()),0,"DLL startup file operation failed");}
        catch(const std::exception&){return Finish(10,0,0,"DLL startup configuration failed");}
    }
    if(!std::filesystem::is_regular_file(game)||!std::filesystem::is_regular_file(index)||!std::filesystem::is_regular_file(host))return Finish(2,ERROR_FILE_NOT_FOUND,0,"Game, index or Host file missing");
    game=std::filesystem::absolute(game);index=std::filesystem::absolute(index);host=std::filesystem::absolute(host);
    const bool hasXInput=std::filesystem::exists(game.parent_path()/L"xinput1_4.dll");
    const bool ownedProxy=hasXInput&&EML::Proxy::Owned(game.parent_path());
    if(hasXInput&&!ownedProxy&&!allowXInputConflict)return Finish(3,0,0,"Existing XInput loader conflicts with explicit launch");
    auto environment=Environment(index);std::wstring command=L"\""+game.wstring()+L"\"";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if(!CreateProcessW(game.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED|CREATE_UNICODE_ENVIRONMENT,environment.data(),game.parent_path().c_str(),&startup,&process)) {
        const auto error=GetLastError();return Finish(4,error,0,"CreateProcess failed: "+std::to_string(error));
    }
    if(ResumeThread(process.hThread)==DWORD(-1)){const auto error=GetLastError();CloseHandle(process.hThread);CloseHandle(process.hProcess);return Finish(5,error,0,"Could not resume game process");}CloseHandle(process.hThread);
    // An installed EML proxy owns startup; do not inject a second Host.
    if(ownedProxy){CloseHandle(process.hProcess);return Finish(0,0,process.dwProcessId);}
    void* load=nullptr;for(int i=0;i<50&&!load;++i){load=TargetLoadLibrary(process.dwProcessId);if(!load)Sleep(100);}
    if(!load){CloseHandle(process.hProcess);return Finish(5,0,0,"Target loader unavailable");}
    const auto library=host.wstring();const SIZE_T bytes=(library.size()+1)*sizeof(wchar_t);
    void* remote=VirtualAllocEx(process.hProcess,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);SIZE_T copied=0;
    if(!remote||!WriteProcessMemory(process.hProcess,remote,library.c_str(),bytes,&copied)||copied!=bytes){const auto error=GetLastError();if(remote)VirtualFreeEx(process.hProcess,remote,0,MEM_RELEASE);CloseHandle(process.hProcess);return Finish(6,error,0,"Host path transfer failed: "+std::to_string(error));}
    HANDLE thread=CreateRemoteThread(process.hProcess,nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(load),remote,0,nullptr);
    if(!thread){const auto error=GetLastError();VirtualFreeEx(process.hProcess,remote,0,MEM_RELEASE);CloseHandle(process.hProcess);return Finish(7,error,0,"Host loader thread failed: "+std::to_string(error));}
    const auto wait=WaitForSingleObject(thread,30000);CloseHandle(thread);
    // A timed-out loader may still read the path: leave its allocation alive.
    if(wait==WAIT_OBJECT_0)VirtualFreeEx(process.hProcess,remote,0,MEM_RELEASE);
    const bool loaded=wait==WAIT_OBJECT_0&&ModuleBase(process.dwProcessId,host.filename().c_str());CloseHandle(process.hProcess);
    if(!loaded)return Finish(8,0,0,"Host load failed");
    return Finish(0,0,process.dwProcessId);
}
