#pragma once
#if defined(_WIN32)
#include <Windows.h>
#define EML_ENGINE_CALL __fastcall
using EMLManagedChar=wchar_t;
#else
#include <dlfcn.h>
#include <strings.h>
#define _stricmp strcasecmp
#define EML_ENGINE_CALL
using HMODULE=void*;
using EMLManagedChar=char16_t;
inline HMODULE GetModuleHandleW(const wchar_t*){return dlopen("libil2cpp.so",RTLD_NOW|RTLD_NOLOAD);}
inline void* GetProcAddress(HMODULE module,const char* name){return dlsym(module,name);}
#endif
