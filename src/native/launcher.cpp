#include <Windows.h>
#include <TlHelp32.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {
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
    std::filesystem::path game,index,host;
    for(int i=1;i+1<argc;i+=2){std::wstring_view option(argv[i]);if(option==L"--game")game=argv[i+1];else if(option==L"--index")index=argv[i+1];else if(option==L"--host")host=argv[i+1];else return 2;}
    if(!std::filesystem::is_regular_file(game)||!std::filesystem::is_regular_file(index)||!std::filesystem::is_regular_file(host)){std::cerr<<"Game, index or Host file missing\n";return 2;}
    game=std::filesystem::absolute(game);index=std::filesystem::absolute(index);host=std::filesystem::absolute(host);
    if(std::filesystem::exists(game.parent_path()/L"xinput1_4.dll")){std::cerr<<"Existing XInput loader conflicts with explicit launch\n";return 3;}
    auto environment=Environment(index);std::wstring command=L"\""+game.wstring()+L"\"";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if(!CreateProcessW(game.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED|CREATE_UNICODE_ENVIRONMENT,environment.data(),game.parent_path().c_str(),&startup,&process)){std::cerr<<"CreateProcess failed: "<<GetLastError()<<'\n';return 4;}
    if(ResumeThread(process.hThread)==DWORD(-1)){CloseHandle(process.hThread);CloseHandle(process.hProcess);return 5;}CloseHandle(process.hThread);
    void* load=nullptr;for(int i=0;i<50&&!load;++i){load=TargetLoadLibrary(process.dwProcessId);if(!load)Sleep(100);}
    if(!load){CloseHandle(process.hProcess);std::cerr<<"Target loader unavailable\n";return 5;}
    const auto library=host.wstring();const SIZE_T bytes=(library.size()+1)*sizeof(wchar_t);
    void* remote=VirtualAllocEx(process.hProcess,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);SIZE_T copied=0;
    if(!remote||!WriteProcessMemory(process.hProcess,remote,library.c_str(),bytes,&copied)||copied!=bytes){if(remote)VirtualFreeEx(process.hProcess,remote,0,MEM_RELEASE);CloseHandle(process.hProcess);return 6;}
    HANDLE thread=CreateRemoteThread(process.hProcess,nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(load),remote,0,nullptr);
    if(!thread){VirtualFreeEx(process.hProcess,remote,0,MEM_RELEASE);CloseHandle(process.hProcess);return 7;}
    const auto wait=WaitForSingleObject(thread,30000);CloseHandle(thread);
    // A timed-out loader may still read the path: leave its allocation alive.
    if(wait==WAIT_OBJECT_0)VirtualFreeEx(process.hProcess,remote,0,MEM_RELEASE);
    const bool loaded=wait==WAIT_OBJECT_0&&ModuleBase(process.dwProcessId,host.filename().c_str());CloseHandle(process.hProcess);
    if(!loaded){std::cerr<<"Host load failed\n";return 8;}
    std::cout<<process.dwProcessId<<'\n';return 0;
}
