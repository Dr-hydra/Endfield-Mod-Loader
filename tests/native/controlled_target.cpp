#include <Windows.h>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int) {
    wchar_t name[256]{};if(!GetEnvironmentVariableW(L"EML_TEST_STOP",name,256))return 1;
    HANDLE event=CreateEventW(nullptr,TRUE,FALSE,name);if(!event)return 2;
    WaitForSingleObject(event,60000);CloseHandle(event);return 0;
}
