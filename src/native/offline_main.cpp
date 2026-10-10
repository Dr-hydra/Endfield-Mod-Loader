#include "runtime/module_runtime.h"
#include "hooks/hook_service.h"
#include <iostream>

int wmain(int argc,wchar_t** argv) {
    std::filesystem::path index;
    if(argc==2)index=argv[1];
    else if(argc==3&&std::wstring_view(argv[1])==L"--index")index=argv[2];
    if(index.empty()){std::cerr<<"Usage: EModLoader.OfflineHost.exe --index <index.json>\n";return 2;}
    auto* hooks=new EML::HookService;
    if(!hooks->Initialize()){std::cerr<<"Hook service unavailable\n";return 3;}
    EML::ModuleRuntime host;
    if(!host.Start(index,"windows-x64",[](const auto& id,const auto& message){std::cerr<<id<<": "<<message<<'\n';},nullptr,hooks->Api(),"offline"))return 1;
    std::cout<<"ready\n"<<std::flush;
    std::string command;
    while(std::getline(std::cin,command)){if(command=="stop")break;}
    host.Stop();
    return 0;
}
