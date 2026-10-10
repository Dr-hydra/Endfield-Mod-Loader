#include <Windows.h>
#include <cstdint>
#include <cmath>
#include <iostream>
extern "C" uint64_t EMLRelay8(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
extern "C" double EMLRelay9(double,double,double,double,double,double);
struct Result {uint64_t a,b,c;};
extern "C" void EMLRelay10();
int calls=0;
uint64_t Integers(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f){++calls;return a+b*2+c*3+d*4+e*5+f*6;}
double Floats(double a,double b,double c,double d,double e,double f){++calls;return a+b*2+c*3+d*4+e*5+f*6;}
Result HiddenReturn(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e){++calls;return {a+b,c+d,e};}
extern "C" void* EMLProxyTarget(unsigned index){return index==8?reinterpret_cast<void*>(Integers):index==9?reinterpret_cast<void*>(Floats):reinterpret_cast<void*>(HiddenReturn);}
int main(){
    if(EMLRelay8(0x100000000,2,3,4,5,6)!=0x100000000+4+9+16+25+36)return 1;
    if(std::abs(EMLRelay9(1.25,2.5,3.75,4.25,5.5,6.75)-Floats(1.25,2.5,3.75,4.25,5.5,6.75))>1e-9)return 2;
    using StructureFn=Result(*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
    const auto value=reinterpret_cast<StructureFn>(EMLRelay10)(0x200000000,2,3,4,5);
    if(value.a!=0x200000002||value.b!=7||value.c!=5||calls!=4)return 3;
    std::cout<<"PASS XInput relay: Win64 integer/float registers, stack arguments, 64-bit and hidden-structure returns.\n";
}
