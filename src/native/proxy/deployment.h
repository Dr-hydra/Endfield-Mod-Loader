#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <bcrypt.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace EML::Proxy {
using Path=std::filesystem::path;
struct Failure:std::runtime_error {DWORD error;Failure(DWORD code,const char* text):std::runtime_error(text),error(code){}};
inline std::string Utf(const Path& path){auto value=path.u8string();return {reinterpret_cast<const char*>(value.data()),value.size()};}
inline std::string Hash(const Path& path) {
    std::ifstream input(path,std::ios::binary);if(!input)throw Failure(ERROR_ACCESS_DENIED,"Cannot read proxy DLL");
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw Failure(0,"SHA-256 unavailable");
    struct Cleanup {BCRYPT_ALG_HANDLE& a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);}} cleanup{algorithm,hash};
    if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0)throw Failure(0,"SHA-256 initialization failed");
    std::vector<unsigned char> chunk(65536);size_t total=0;
    while(input){input.read(reinterpret_cast<char*>(chunk.data()),chunk.size());const auto bytes=input.gcount();total+=static_cast<size_t>(bytes);
        if(total>64*1024*1024)throw Failure(0,"Proxy DLL exceeds size limit");
        if(bytes&&BCryptHashData(hash,chunk.data(),static_cast<ULONG>(bytes),0)<0)throw Failure(0,"SHA-256 read failed");}
    if(!input.eof())throw Failure(0,"Proxy DLL read failed");
    unsigned char digest[32]{};if(BCryptFinishHash(hash,digest,32,0)<0)throw Failure(0,"SHA-256 finish failed");
    constexpr char hex[]="0123456789abcdef";std::string output;
    for(auto byte:digest){output+=hex[byte>>4];output+=hex[byte&15];}return output;
}
inline nlohmann::json Receipt(const Path& directory) {
    const auto file=directory/L"eml-xinput.json";
    if(!std::filesystem::is_regular_file(file)||std::filesystem::file_size(file)>16384)throw Failure(0,"DLL startup ownership record missing");
    std::ifstream input(file);auto json=nlohmann::json::parse(input);
    if(json.value("product","")!="EndfieldModLoader"||json.value("schema",0)!=1)throw Failure(0,"DLL startup ownership record differs");
    return json;
}
inline bool Owned(const Path& directory) {
    try{return std::filesystem::is_regular_file(directory/L"xinput1_4.dll")&&Receipt(directory).at("proxy_sha256").get<std::string>()==Hash(directory/L"xinput1_4.dll");}
    catch(...){return false;}
}
inline void RequireGameClosed(const Path& game) {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)throw Failure(GetLastError(),"Cannot check running game");
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);bool running=false;
    if(Process32FirstW(snapshot,&entry))do{
        if(_wcsicmp(entry.szExeFile,game.filename().c_str()))continue;
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
        wchar_t path[32768]{};DWORD size=32768;
        if(!process||!QueryFullProcessImageNameW(process,0,path,&size))running=true;
        else if(std::filesystem::equivalent(game,Path(path)))running=true;
        if(process)CloseHandle(process);
    }while(!running&&Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);if(running)throw Failure(0,"Close the game before changing DLL startup");
}
inline void Deploy(bool install,const Path& game,const Path& host,const Path& index,const Path& source) {
    if(!std::filesystem::is_regular_file(game)||_wcsicmp(game.filename().c_str(),L"Endfield.exe"))throw Failure(ERROR_INVALID_PARAMETER,"Select a valid Endfield.exe");
    RequireGameClosed(game);
    const auto directory=std::filesystem::canonical(game).parent_path();
    const auto proxy=directory/L"xinput1_4.dll",record=directory/L"eml-xinput.json";
    // An exclusive marker serializes deployment; it carries no game settings.
    const auto lockPath=directory/L"eml-xinput.lock";
    HANDLE lock=CreateFileW(lockPath.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE,nullptr);
    if(lock==INVALID_HANDLE_VALUE)throw Failure(GetLastError(),"Cannot lock DLL startup deployment");
    struct Unlock {HANDLE handle;~Unlock(){CloseHandle(handle);}} unlock{lock};
    if(std::filesystem::exists(proxy)&&!Owned(directory))throw Failure(0,"Existing XInput DLL belongs to another tool or was modified");
    if(!install) {
        if(!std::filesystem::exists(proxy)) {
            if(std::filesystem::exists(record))Receipt(directory);
            if(std::filesystem::exists(record)&&!DeleteFileW(record.c_str()))throw Failure(GetLastError(),"Cannot remove DLL startup record");
            return;
        }
        if(!DeleteFileW(proxy.c_str()))throw Failure(GetLastError(),"Cannot remove owned proxy DLL");
        if(!DeleteFileW(record.c_str()))throw Failure(GetLastError(),"Cannot remove DLL startup record");
        return;
    }
    if(std::filesystem::exists(record))Receipt(directory);
    if(!std::filesystem::is_regular_file(host)||!std::filesystem::is_regular_file(index)||!std::filesystem::is_regular_file(source))throw Failure(ERROR_FILE_NOT_FOUND,"DLL startup payload is missing");
    const auto suffix=std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    const auto pendingDll=directory/(L"eml-xinput-"+suffix+L".pending"),pendingJson=directory/(L"eml-xinput-"+suffix+L".json.pending");
    struct Temporary {Path a,b;~Temporary(){std::error_code error;std::filesystem::remove(a,error);std::filesystem::remove(b,error);}} temporary{pendingDll,pendingJson};
    const auto digest=Hash(source);
    if(!CopyFileW(source.c_str(),pendingDll.c_str(),TRUE))throw Failure(GetLastError(),"Cannot copy DLL startup payload");
    if(Hash(pendingDll)!=digest)throw Failure(0,"Copied proxy DLL checksum differs");
    auto json=nlohmann::json{{"product","EndfieldModLoader"},{"schema",1},{"proxy_sha256",digest},
        {"host_path",Utf(std::filesystem::absolute(host))},{"index_path",Utf(std::filesystem::absolute(index))}};
    {std::ofstream output(pendingJson,std::ios::binary);if(!output||!(output<<json.dump(2)))throw Failure(ERROR_ACCESS_DENIED,"Cannot save DLL startup record");}
    // Publish the configuration first; if DLL replacement fails, restore it.
    const auto previous=std::filesystem::exists(record)?Receipt(directory):nlohmann::json();
    if(!MoveFileExW(pendingJson.c_str(),record.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw Failure(GetLastError(),"Cannot publish DLL startup record");
    if(!MoveFileExW(pendingDll.c_str(),proxy.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        const auto error=GetLastError();
        if(previous.is_null())DeleteFileW(record.c_str());
        else {std::ofstream restore(record,std::ios::binary);restore<<previous.dump(2);}
        throw Failure(error,"Cannot install proxy DLL");
    }
}
}
