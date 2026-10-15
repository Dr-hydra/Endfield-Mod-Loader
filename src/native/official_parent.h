#pragma once
#include <Windows.h>
#include <filesystem>
#include <string>
#include <vector>

// Shared signed-launcher discovery and owned-parent lifetime, adapted from BE.
namespace EML::Launch {
std::wstring QuoteArgument(const std::wstring& argument);
std::wstring WindowsError(const wchar_t* operation, DWORD code);
class Handle {
public:
    explicit Handle(HANDLE value = nullptr) : value_(value) {}
    ~Handle();
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept;
    Handle& operator=(Handle&& other) noexcept;
    HANDLE get() const { return value_; }
    void reset(HANDLE value = nullptr);
private:
    HANDLE value_ = nullptr;
};
struct VerifiedLauncher {
    std::filesystem::path executable;
    // Keep the verified file protected against replacement until parent creation.
    Handle file;
};
bool IsOfficialPublisher(const std::wstring& organization);
bool VerifyOfficialLauncher(const std::filesystem::path& path, VerifiedLauncher& result, std::wstring& error);
bool FindOfficialLauncher(const std::filesystem::path& requested, VerifiedLauncher& result, std::wstring& error);
bool ResolveToGamesCore(const std::filesystem::path& input, std::filesystem::path& resolved);
class OfficialParent {
public:
    OfficialParent() = default;
    ~OfficialParent();
    OfficialParent(const OfficialParent&) = delete;
    OfficialParent& operator=(const OfficialParent&) = delete;
    bool Start(const VerifiedLauncher& launcher, std::wstring& error, DWORD& code);
    bool CreateGame(const std::filesystem::path& game, const std::vector<std::wstring>& arguments,
        DWORD creation_flags, PROCESS_INFORMATION& process, std::wstring& error, DWORD& code,
        void* environment = nullptr) const;
private:
    Handle process_;
};
bool WaitForGame(HANDLE process, DWORD& exit_code, std::wstring& error);
}
