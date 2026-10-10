#pragma once
#include "BetterEndfield/ThirdPartyModule.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
namespace EML {
class ModuleRuntime final {
public:
    using Log = std::function<void(const std::string&,const std::string&)>;
    ModuleRuntime();
    ~ModuleRuntime();
    bool Start(const std::filesystem::path& index,const std::string& platform,Log log,
        const BE_HostApiV1* runtime=nullptr,const BE_HookChainApiV1* hooks=nullptr,const std::string& mode="offline");
    // Queues a refreshed index snapshot (Android private index or Windows disk).
    bool UpdateIndex(const std::string& json);
    void SetRuntime(const BE_HostApiV1* runtime);
    void Stop();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
