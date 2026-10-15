#include "../../src/native/official_parent.h"
#include <shellapi.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

int wmain(int argc, wchar_t** argv) {
    using namespace EML::Launch;
    if(argc<2||argc>3)return 2;
    int checks=0;
    const auto check=[&](bool result,const char* message){if(!result)throw std::runtime_error(message);++checks;};
    try{
        const auto root=std::filesystem::absolute(argv[1]);
        std::filesystem::create_directories(root);
        const auto unsignedParent=root/L"Games.exe";
        if(std::filesystem::exists(unsignedParent))throw std::runtime_error("Use an empty fixture directory.");
        {std::ofstream file(unsignedParent);file<<"Inert unsigned fixture, never executable.";}
        VerifiedLauncher launcher;std::wstring error;
        check(!VerifyOfficialLauncher(unsignedParent,launcher,error),"Unsigned Games.exe accepted.");
        check(!FindOfficialLauncher(unsignedParent,launcher,error),"Invalid explicit parent fell back to installed launcher.");
        check(!VerifyOfficialLauncher(root/L"Launcher.exe",launcher,error),"Wrong executable basename accepted.");
        check(!launcher.file.get(),"Rejected verification retained a launcher handle.");
        check(IsOfficialPublisher(L"Shanghai Hypergryph Network Technology Co., Ltd."),"Official publisher rejected.");
        check(IsOfficialPublisher(L"SHANGHAI HYPERGRYPH NETWORK TECHNOLOGY CO., LTD."),"Publisher case handling changed.");
        check(!IsOfficialPublisher(L"Shanghai Hypergryph Network Technology Co., Ltd. Untrusted"),"Publisher suffix accepted.");
        check(!IsOfficialPublisher(L"Microsoft Corporation"),"Different publisher accepted.");
        const std::vector<std::wstring> values{L"",L"with spaces",L"中文",L"quote\"value",L"C:\\trailing \\",L"\\\\server\\share\\"};
        std::wstring command=L"fixture.exe";
        for(const auto& value:values)command+=L" "+QuoteArgument(value);
        int count=0;auto parsed=CommandLineToArgvW(command.c_str(),&count);
        check(parsed&&count==static_cast<int>(values.size())+1,"Quoted argument count changed.");
        for(size_t i=0;i<values.size();++i)check(parsed[i+1]==values[i],"Quoted argument changed.");
        LocalFree(parsed);
        if(argc==3&&std::wstring(argv[2])==L"--verify-installed"){
            check(FindOfficialLauncher({},launcher,error),"Installed official launcher discovery/signature failed.");
            std::wcout<<L"Verified official launcher (not started): "<<launcher.executable.wstring()<<L'\n';
        }
        std::cout<<"Official parent: "<<checks<<" checks passed; no processes started.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
