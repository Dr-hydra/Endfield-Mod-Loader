#include <Windows.h>

// Harmless foreign-loader fixture, used only by project-owned controlled processes.
extern "C" __declspec(dllexport) int EMLForeignXInputMarker() { return 314; }
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
