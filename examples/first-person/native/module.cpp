#include "BetterEndfield/ThirdPartyModule.h"
#include "platform.h"
#include "first_person_mesh.h"
#include "first_person_retry.h"
#include "first_person_profiles.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <stdexcept>
#include <thread>
#include <unordered_set>

namespace EModLoader::FirstPerson {
namespace {
using Json = nlohmann::json;
constexpr char kModuleId[] = "example.first-person";
// A process-resident, transparent frame driver drains Unity cleanup even after
// the Host retires the module's effect hooks. It never calls the worker bridge.
constexpr char kFrameOwner[] = "example.first-person.frame";
std::recursive_mutex g_gate;
std::condition_variable_any g_cleanup_changed;
const BE_ThirdPartyHostV1* g_bridge = nullptr;
std::atomic_bool g_bootstrap_stop{false};
std::thread g_bootstrap;
bool g_initialized = false, g_frame_installed = false, g_heartbeat_installed = false;
bool g_resolved = false, g_cleanup_pending = false;
std::string g_phase = "stopped";
std::string g_detail;

#include "camera_runtime.inc"

bool CleanupComplete() {
    return !g_first_person_active && !g_active_camera && !g_first_person.body &&
        g_fp_mesh_session.patches.empty() && !g_first_person.snapshot_exit_owned;
}

void DrainCleanup() {
    if(!g_cleanup_pending || !OnCameraUnityThread(false)) return;
    ExitFirstPerson("module stopped");
    if(CleanupComplete()) {
        g_cleanup_pending = false;
        g_cleanup_changed.notify_all();
    }
}

void PollHotkey() {
#if defined(_WIN32)
    static bool was_down = false;
    DWORD process = 0;
    HWND window = GetForegroundWindow();
    if(window) GetWindowThreadProcessId(window, &process);
    const int key = g_first_person_key.load();
    const bool down = g_initialized && g_first_person_camera_enabled &&
        process == GetCurrentProcessId() && key > 0 && (GetAsyncKeyState(key) & 0x8000);
    if(down && !was_down) g_first_person_toggle_request = true;
    was_down = down;
#endif
}

void Frame() {
    if(!OnCameraUnityThread(true)) return;
    DrainCleanup();
    if(g_initialized && !g_cleanup_pending) {
        PollHotkey();
        PumpFirstPerson();
    }
}

void FP_ENGINE_CALL DetourTailLateTick(void* instance, float delta, void* method) {
    if(g_original_tail_late_tick) g_original_tail_late_tick(instance, delta, method);
    std::lock_guard lock(g_gate);
    Frame();
}

void FP_ENGINE_CALL DetourCameraTick(void* instance, void* method) {
    if(g_original_camera_tick) g_original_camera_tick(instance, method);
    std::lock_guard lock(g_gate);
    Frame();
}

thread_local bool t_in_heartbeat = false;
float FP_ENGINE_CALL DetourTimeUnscaledDelta(void* method) {
    const float result = g_original_time_unscaled_delta ? g_original_time_unscaled_delta(method) : 0;
    std::lock_guard lock(g_gate);
    if(g_time_heartbeat_contract_ready && !t_in_heartbeat && OnCameraUnityThread(false)) {
        t_in_heartbeat = true;
        struct Guard { ~Guard() { t_in_heartbeat = false; } } guard;
        DrainCleanup();
        const double now = NowSeconds();
        if(g_initialized && !g_cleanup_pending && (g_first_person_exit_request ||
            g_first_person_toggle_request || (g_first_person_active && !g_first_person_camera_enabled) ||
            now - g_first_person_last_heartbeat >= 0.003)) {
            PollHotkey();
            PumpFirstPerson(false, false);
            g_first_person_last_heartbeat = now;
        }
    }
    return result;
}

void FP_ENGINE_CALL DetourPushState(void* instance, void* state, void* method) {
    std::lock_guard lock(g_gate);
    g_push_state_calls.fetch_add(1, std::memory_order_relaxed);
    const bool unity_thread = OnCameraUnityThread(true);
    // Unit regression cases also drive this implementation directly.
    if(unity_thread) DrainCleanup();
    if(unity_thread && (g_first_person_toggle_request || g_first_person_exit_request ||
        (g_first_person_active && !g_first_person_camera_enabled))) PumpFirstPerson(true, false);
    if(unity_thread && g_first_person_active) {
        void* camera = Invoke(Contract("unity.camera.main"), nullptr, nullptr);
        if(!camera || camera != g_active_camera || !IsObjectAlive(g_active_camera))
            ExitFirstPerson("active camera changed before push");
    }
    const bool first_person = state && unity_thread && !g_cleanup_pending &&
        g_first_person_active && g_first_person_camera_enabled && !g_first_person_exit_request &&
        FirstPersonTargetIsCurrent() && FirstPersonBrainDrivesCamera(instance);
    ScopedFirstPersonState scope(state, first_person);
    if(first_person && scope.state) ApplyFirstPersonState(state);
    if(g_original_push_state) g_original_push_state(instance, state, method);
}

bool ResolveContracts() {
    if(!g_host || !g_host->resolve_method || !g_host->resolve_field || !g_host->resolve_class ||
        !g_host->runtime_invoke || !g_host->object_unbox || !g_host->string_new ||
        !g_host->gchandle_new || !g_host->gchandle_free || !g_host->copy_managed_string ||
        !g_host->create_hook) return false;
    for(auto& c : g_contracts) {
        BE_ResolvedMethodV1 result{};
        auto status = g_host->resolve_method(g_host->context, &c.descriptor, &result);
        if((status != BE_Result_Ok || !result.method_info || !result.method_pointer) && c.alternate_parameter_types) {
            auto alternative = c.descriptor;
            alternative.parameter_types = c.alternate_parameter_types;
            status = g_host->resolve_method(g_host->context, &alternative, &result);
        }
        c.resolved = status == BE_Result_Ok && result.method_info && result.method_pointer;
        c.method_info = c.resolved ? result.method_info : nullptr;
        c.pointer = c.resolved ? result.method_pointer : nullptr;
        if(!c.resolved) Log(std::string("Optional/missing method: ") + c.key);
    }
    for(auto& field : g_fields) {
        field.ready = g_host->resolve_field(g_host->context, &field.descriptor, &field.resolved) == BE_Result_Ok &&
            field.resolved.offset > 0 && static_cast<size_t>(field.resolved.offset) <= kMaxFieldOffset;
    }
    const auto* header = Field("unity.vector3.x");
    // Unknown boxed layouts must fail instead of guessing camera offsets.
    if(!header || !header->ready || header->resolved.offset > 0x20) return false;
    g_value_type_header = header->resolved.offset;
    const struct { const char* assembly; const char* ns; const char* name; BE_ResolvedClassV1* out; } classes[] {
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.View", "SnapshotCameraController", &g_snapshot_controller_class},
        {"UnityEngine.AnimationModule.dll", "UnityEngine", "Animator", &g_animator_class},
        {"UnityEngine.CoreModule.dll", "UnityEngine", "SkinnedMeshRenderer", &g_skinned_mesh_renderer_class},
        {"UnityEngine.CoreModule.dll", "UnityEngine", "GameObject", &g_fp_game_object_class}
    };
    for(const auto& type : classes) g_host->resolve_class(g_host->context, type.assembly, type.ns, type.name, type.out);
    if(!BuildCameraStateLayout()) return false;
    for(const auto* key : {"cinemachine.push_state", "player_controller.get_main_character", "entity.get_model_com",
        "base_model_component.get_model_go", "unity.game_object.transform", "unity.object.name.get",
        "unity.object.op_equality", "unity.component.game_object", "unity.transform.parent.get",
        "unity.transform.position.get", "unity.transform.child_count.get", "unity.transform.get_child",
        "unity.transform.find", "unity.camera.main", "unity.camera.fov.get"}) {
        if(!Contract(key)->resolved) return false;
    }
    g_first_person_contract_ready = true;
    g_time_heartbeat_contract_ready = Contract("unity.time.unscaled_delta.get")->resolved;
    return true;
}

bool InstallHooks() {
    auto create = [](const char* key, const char* owner, void* detour, void** next) {
        auto* contract = Contract(key);
        return contract && contract->resolved && g_host->create_hook(g_host->context, owner,
            contract->pointer, detour, next) == BE_Result_Ok;
    };
    if(!g_frame_installed) {
        g_frame_installed = create("camera_manager.tail_late_tick", kFrameOwner,
            reinterpret_cast<void*>(DetourTailLateTick), reinterpret_cast<void**>(&g_original_tail_late_tick));
        if(!g_frame_installed) g_frame_installed = create("camera.process_dither", kFrameOwner,
            reinterpret_cast<void*>(DetourCameraTick), reinterpret_cast<void**>(&g_original_camera_tick));
    }
    if(!g_frame_installed) return false;
    if(g_time_heartbeat_contract_ready && !g_heartbeat_installed) {
        g_heartbeat_installed = create("unity.time.unscaled_delta.get", kFrameOwner,
            reinterpret_cast<void*>(DetourTimeUnscaledDelta), reinterpret_cast<void**>(&g_original_time_unscaled_delta));
        if(!g_heartbeat_installed) Log("Unscaled heartbeat unavailable; cleanup waits for a camera frame.");
    }
    g_push_state_hook_ready = create("cinemachine.push_state", kModuleId,
        reinterpret_cast<void*>(DetourPushState), reinterpret_cast<void**>(&g_original_push_state));
    return g_push_state_hook_ready;
}

struct Configuration {
    bool enabled = true, active = false, hide_head = true, fill_neck = true, diagnostics = false;
    float fov = 75, neck_range = 1, look_limit = 90, turn_speed = 360;
    int toggle_key = 0xBD;
};
Configuration g_configuration;

Configuration ParseConfiguration(const char* raw) {
    if(!raw || std::strlen(raw) > 65536) throw std::invalid_argument("Invalid configuration size");
    auto json = Json::parse(raw);
    if(!json.is_object()) throw std::invalid_argument("Configuration must be an object");
    Configuration config;
    config.enabled = json.value("enabled", true);
    config.active = json.value("active", false);
    config.hide_head = json.value("hide_head", true);
    config.fill_neck = json.value("fill_neck", true);
    config.diagnostics = json.value("diagnostics", false);
    auto number = [&](const char* key, float fallback, float minimum, float maximum) {
        const float value = json.value(key, fallback);
        if(!std::isfinite(value) || value < minimum || value > maximum) throw std::invalid_argument(key);
        return value;
    };
    config.fov = number("fov", 75, 30, 120);
    config.neck_range = number("neck_range", 1, 0.2f, 3);
    config.look_limit = number("look_limit", 90, 0, 180);
    config.turn_speed = number("turn_speed", 360, 1, 1440);
    if(json.contains("toggle_key") && !json["toggle_key"].is_number_integer()) throw std::invalid_argument("toggle_key");
    const double key = json.value("toggle_key", double(0xBD));
    if(key < 0 || key > 255) throw std::invalid_argument("toggle_key");
    config.toggle_key = static_cast<int>(key);
    return config;
}

void PublishConfiguration(const Configuration& config, bool initial) {
    const auto previous = g_configuration;
    g_configuration = config;
    g_first_person_camera_enabled = config.enabled;
    g_first_person_hide_head = config.hide_head;
    g_first_person_fill_neck_hole = config.fill_neck;
    g_diagnostics_enabled = config.diagnostics;
    g_first_person_fov = config.fov;
    g_first_person_neck_plug_scale = config.neck_range;
    g_first_person_side_look_limit = config.look_limit;
    g_first_person_turn_speed = config.turn_speed;
    g_first_person_key = config.toggle_key;
    if(!config.enabled || (!config.active && (initial || previous.active))) {
        g_first_person_toggle_request = false;
        g_first_person_exit_request = true;
    } else if(config.active && (initial || !previous.active || !previous.enabled)) {
        g_first_person_exit_request = false;
        if(!g_first_person_active) g_first_person_toggle_request = true;
    }
}

void Bootstrap() {
    while(!g_bootstrap_stop) {
        const auto* helper = g_bridge->get_runtime ? g_bridge->get_runtime(g_bridge->context) : g_bridge->runtime;
        if(!helper) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
        // Metadata resolution creates managed Type objects on this thread.
        // Attach explicitly and detach only the thread attached here.
        void* engine = EngineModule();
        if(!engine) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
        auto domain = reinterpret_cast<void*(*)()>(EngineExport(engine, "il2cpp_domain_get"));
        auto current = reinterpret_cast<void*(*)()>(EngineExport(engine, "il2cpp_thread_current"));
        auto attach = reinterpret_cast<void*(*)(void*)>(EngineExport(engine, "il2cpp_thread_attach"));
        auto detach = reinterpret_cast<void(*)(void*)>(EngineExport(engine, "il2cpp_thread_detach"));
        if(!domain || !current || !attach || !detach) {
            std::lock_guard lock(g_gate); g_phase = "contract_mismatch";
            g_detail = "IL2CPP thread exports are unavailable"; return;
        }
        void* managed_domain = domain();
        if(!managed_domain) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
        void* attached = current() ? nullptr : attach(managed_domain);
        if(!current()) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
        struct Detach { void* thread; void(*fn)(void*); ~Detach(){if(thread)fn(thread);} } scope{attached,detach};
        std::lock_guard lock(g_gate);
        if(g_bootstrap_stop) return;
        g_host = helper;
        try {
            g_resolved = g_resolved || ResolveContracts();
            g_phase = !g_resolved ? "contract_mismatch" : InstallHooks() ? "ready" : "hook_failed";
            g_detail = g_phase == "ready" ? "" : "Game methods, metadata layout or hook installation did not match";
            Log("First-person module: " + g_phase);
        } catch(const std::exception& error) { g_phase = "failed"; g_detail = error.what(); }
        return;
    }
}

BE_Result BE_CALL Configure(const char* json) {
    try {
        auto config = ParseConfiguration(json);
        std::lock_guard lock(g_gate);
        if(!g_initialized) return BE_Result_NotReady;
        PublishConfiguration(config, false);
        return BE_Result_Ok;
    } catch(...) { return BE_Result_InvalidArgument; }
}

BE_Result BE_CALL Initialize(const BE_ThirdPartyHostV1* host, const char* json) {
    if(!host || host->struct_size < sizeof(*host) || host->version != 1 || !host->reply || !host->log ||
        !host->module_id || std::string_view(host->module_id) != kModuleId) return BE_Result_ContractMismatch;
    try {
        auto config = ParseConfiguration(json);
        std::lock_guard lock(g_gate);
        if(g_initialized || g_bootstrap.joinable()) return BE_Result_Conflict;
        g_bridge = host;
        g_initialized = true;
        g_phase = "waiting_runtime";
        g_detail.clear();
        PublishConfiguration(config, true);
        g_bootstrap_stop = false;
        try { g_bootstrap = std::thread(Bootstrap); }
        catch(...) { g_initialized = false; g_bridge = nullptr; g_phase = "failed"; return BE_Result_Failed; }
        host->log(host->context, "First-person example waiting for game helpers; offline mode does not alter a game.");
        return BE_Result_Ok;
    } catch(...) { return BE_Result_InvalidArgument; }
}

BE_Result BE_CALL Message(const char* request, const char* body) {
    if(!request || !body || std::strlen(body) > 65536) return BE_Result_InvalidArgument;
    std::lock_guard lock(g_gate);
    if(!g_initialized || !g_bridge) return BE_Result_NotReady;
    try {
        const auto json = Json::parse(body);
        const auto action = json.at("action").get<std::string>();
        BE_Result result = BE_Result_Ok;
        if(action == "toggle" || action == "exit") {
            if(g_phase != "ready" || g_cleanup_pending || !g_first_person_camera_enabled) result = BE_Result_NotReady;
            else if(action == "exit") { g_first_person_toggle_request = false; g_first_person_exit_request = true; }
            else { g_first_person_exit_request = false; g_first_person_toggle_request = true; }
        } else if(action != "status") result = BE_Result_InvalidArgument;
        auto reply = Json{{"phase",g_phase},{"detail",g_detail},{"active",bool(g_first_person_active)},
            {"enabled",bool(g_first_person_camera_enabled)},{"cleanup_pending",g_cleanup_pending},
            {"target_valid",g_first_person.target_valid},{"model_id",g_first_person.model_id},
            {"patched_frames",g_push_state_patches.load()},{"head_patches",g_fp_mesh_session.patches.size()},
            {"toggle_pending",bool(g_first_person_toggle_request)},{"exit_pending",bool(g_first_person_exit_request)},
            {"game_validation","not_run"}}.dump();
        return g_bridge->reply(g_bridge->context, request, result, reply.c_str());
    } catch(...) { return g_bridge->reply(g_bridge->context, request, BE_Result_InvalidArgument, "{\"error\":\"Invalid command\"}"); }
}

void BE_CALL Shutdown() {
    g_bootstrap_stop = true;
    if(g_bootstrap.joinable()) g_bootstrap.join();
    std::unique_lock lock(g_gate);
    if(!g_initialized) return;
    g_initialized = false;
    g_phase = "stopped";
    g_first_person_camera_enabled = false;
    g_first_person_toggle_request = false;
    g_first_person_exit_request = true;
    g_cleanup_pending = !CleanupComplete();
    g_bridge = nullptr;
    // Never call Unity from the Host worker, nor clear stable next pointers.
    // The process-resident driver can finish when a paused game resumes.
    DrainCleanup();
    if(g_cleanup_pending) g_cleanup_changed.wait_for(lock, std::chrono::milliseconds(500), []{return !g_cleanup_pending;});
    g_push_state_hook_ready = false;
}

const BE_ThirdPartyModuleV1 kApi{sizeof(BE_ThirdPartyModuleV1), 1, kModuleId, Initialize, Configure, Message, Shutdown};
} // namespace
} // namespace EModLoader::FirstPerson

#if defined(__ANDROID__)
extern "C" __attribute__((visibility("default"))) const BE_ThirdPartyModuleV1* BetterEndfield_GetThirdPartyModuleV1() {
    return &EModLoader::FirstPerson::kApi;
}
#else
BE_EXPORT const BE_ThirdPartyModuleV1* BE_CALL BetterEndfield_GetThirdPartyModuleV1() {
    return &EModLoader::FirstPerson::kApi;
}
#endif
