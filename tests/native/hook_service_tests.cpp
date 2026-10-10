#include "hooks/hook_service.h"
#include <iostream>
#include <stdexcept>
using Fn=int(*)(int,int);
Fn nextA=nullptr,nextB=nullptr;int first=0,second=0,original=0;
__declspec(noinline) int Target(int a,int b){++original;return a+b;}
__declspec(noinline) int A(int a,int b){++first;return nextA(a,b)+10;}
__declspec(noinline) int B(int a,int b){++second;return nextB(a,b)*2;}
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){
    auto* service=new EML::HookService;Check(service->Initialize(),"initialize");auto* api=service->Api();uint64_t a=0,b=0;
    Check(api->create(api->context,"example.a",reinterpret_cast<void*>(Target),reinterpret_cast<void*>(A),reinterpret_cast<void**>(&nextA),&a)==BE_Result_Ok,"first hook");
    void* rejected=reinterpret_cast<void*>(1);
    Check(service->CreateForModule("example.a",reinterpret_cast<void*>(Target),reinterpret_cast<void*>(A),&rejected)==BE_Result_Conflict&&rejected==reinterpret_cast<void*>(1),"legacy per-module duplicate rejection preserves the live original");
    Check(api->create(api->context,"example.b",reinterpret_cast<void*>(Target),reinterpret_cast<void*>(B),reinterpret_cast<void**>(&nextB),&b)==BE_Result_Ok,"second hook");
    volatile Fn call=Target;Check(call(3,4)==24&&first==1&&second==1&&original==1,"order, arguments, return, exactly one original");
    Check(api->disable(api->context,b)==BE_Result_Ok,"disable second");Check(call(3,4)==17&&first==2&&second==1&&original==2,"disabled relay passthrough");
    Check(api->disable_module(api->context,"example.a")==BE_Result_Ok,"disable owner");Check(call(3,4)==7&&first==2&&second==1&&original==3,"restore passthrough");
    std::cout<<"PASS: product Hook chain registration, ABI forwarding, ordering and owner retirement\n";
}
