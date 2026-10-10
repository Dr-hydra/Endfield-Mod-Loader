#include <Windows.h>
#include <Xinput.h>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int) {
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    auto* last=wcsrchr(executable,L'\\');if(!last)return 1;last[1]=0;
    if(wcslen(executable)+wcslen(L"xinput1_4.dll")>=32768)return 2;
    wcscat_s(executable,L"xinput1_4.dll");
    HMODULE proxy=LoadLibraryW(executable);if(!proxy)return 3;
    using GetState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
    auto state=reinterpret_cast<GetState>(GetProcAddress(proxy,"XInputGetState"));
    wchar_t systemPath[32768]{};if(!GetSystemDirectoryW(systemPath,32768))return 4;
    wcscat_s(systemPath,L"\\xinput1_4.dll");
    HMODULE system=LoadLibraryExW(systemPath,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    auto original=system?reinterpret_cast<GetState>(GetProcAddress(system,"XInputGetState")):nullptr;
    XINPUT_STATE value{},expected{};
    if(!state||!original||system==proxy)return 4;
    SetLastError(1375);const auto originalResult=original(0xFFFFFFFF,&expected),originalError=GetLastError();
    SetLastError(1375);const auto proxyResult=state(0xFFFFFFFF,&value),proxyError=GetLastError();
    if(proxyResult!=originalResult||proxyError!=originalError)return 4;
    const WORD ordinals[]={100,101,102,103,104,108,109};
    for(WORD ordinal:ordinals)if(!GetProcAddress(proxy,MAKEINTRESOURCEA(ordinal)))return 5;
    wchar_t name[256]{};if(!GetEnvironmentVariableW(L"EML_TEST_STOP",name,256))return 6;
    HANDLE event=CreateEventW(nullptr,TRUE,FALSE,name);if(!event)return 7;
    WaitForSingleObject(event,60000);CloseHandle(event);return 0;
}
