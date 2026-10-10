#pragma once
#include <cstdint>
#if defined(_WIN32)
#include <Windows.h>
#define FP_ENGINE_CALL __fastcall
inline uint64_t CurrentThreadId() { return GetCurrentThreadId(); }
inline void* EngineModule() { return GetModuleHandleW(L"GameAssembly.dll"); }
inline void* EngineExport(void* module, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), name));
}
#else
#include <dlfcn.h>
#include <sys/syscall.h>
#include <unistd.h>
#define FP_ENGINE_CALL
inline uint64_t CurrentThreadId() { return static_cast<uint64_t>(syscall(SYS_gettid)); }
inline void* EngineModule() {
    // Keep one reference for this process-resident library.
    static void* module = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
    return module;
}
inline void* EngineExport(void* module, const char* name) { return dlsym(module, name); }
#endif
