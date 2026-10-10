// Derived from the BE first-person archive; see ../README.md for provenance.
struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct MethodContract {
    const char* key;
    BE_MethodDescriptorV1 descriptor;
    // Some managed signatures differ only in how IL2CPP reports a byref
    // parameter. The alternate text is only consulted when the primary
    // descriptor does not resolve.
    const char* alternate_parameter_types = nullptr;
    void* pointer = nullptr;
    const void* method_info = nullptr;
    bool resolved = false;
};

struct FieldContract {
    const char* key;
    BE_FieldDescriptorV1 descriptor;
    BE_ResolvedFieldV1 resolved{};
    bool ready = false;
};


const BE_HostApiV1* g_host = nullptr;
std::atomic_bool g_first_person_camera_enabled{false};
std::atomic_bool g_first_person_hide_head{true}, g_first_person_fill_neck_hole{true};
std::atomic_bool g_diagnostics_enabled{false};
std::atomic<float> g_first_person_fov{75}, g_first_person_neck_plug_scale{1};
std::atomic<float> g_first_person_side_look_limit{90}, g_first_person_turn_speed{360};
std::atomic_int g_first_person_key{0xBD};
using CameraTickFn = void(FP_ENGINE_CALL*)(void*, void*);
using TailLateTickFn = void(FP_ENGINE_CALL*)(void*, float, void*);
using TimeUnscaledDeltaFn = float(FP_ENGINE_CALL*)(void*);
using PushStateFn = void(FP_ENGINE_CALL*)(void*, void*, void*);
CameraTickFn g_original_camera_tick = nullptr;
TailLateTickFn g_original_tail_late_tick = nullptr;
TimeUnscaledDeltaFn g_original_time_unscaled_delta = nullptr;
PushStateFn g_original_push_state = nullptr;
bool g_push_state_hook_ready = false, g_first_person_contract_ready = false;
bool g_time_heartbeat_contract_ready = false;
std::atomic_uint64_t g_push_state_calls{0}, g_push_state_patches{0};
std::atomic_bool g_first_person_active{false}, g_first_person_toggle_request{false};
std::atomic_bool g_first_person_exit_request{false};
Vector3 g_first_person_view_forward{0,0,1};
bool g_first_person_view_forward_valid = false;
void* g_active_camera = nullptr;
uint32_t g_active_camera_root = 0;
constexpr float kFirstPersonEyeForward = 0.10f;
constexpr float kFirstPersonEyeUp = 0.06f;
constexpr float kFirstPersonNearClip = 0.05f;
constexpr size_t kMaxFieldOffset = 0x400;
constexpr int32_t kHumanBodyBonesHead = 11;

// Value type metadata offsets in IL2CPP are relative to the boxed layout, which
// carries an object header in front of the inline data. The header size is
// probed through UnityEngine.Vector3.x (the first field of a 12 byte struct)
// instead of being hardcoded, so the geometry below survives Unity upgrades.
int32_t g_value_type_header = 0;

struct CameraStateLayout {
    int32_t raw_position = -1;              // CameraState body relative
    int32_t raw_orientation = -1;
    int32_t position_correction = -1;
    int32_t orientation_correction = -1;
    int32_t lens = -1;                      // CameraState.Lens, body relative
    int32_t lens_field_of_view = -1;        // LensSettings body relative
    int32_t lens_near_clip = -1;
    int32_t lens_dutch = -1;
    bool ready = false;
};
CameraStateLayout g_state_layout;

BE_ResolvedClassV1 g_snapshot_controller_class{};
BE_ResolvedClassV1 g_animator_class{};
BE_ResolvedClassV1 g_skinned_mesh_renderer_class{};
// Private zero-scale collapse anchors for the no-geometry palette fallback.
BE_ResolvedClassV1 g_fp_game_object_class{};

// Renderer pointers inside a probe are only valid during the scan that produced
// them. Bound mesh patches keep GC handles separately because the game can
// rebuild character parts (clothes, hair, LOD levels) at any time.
struct HeadPartProbe {
    std::string object_name;
    std::string renderer_name;
    std::string mesh_name;
    int32_t vertex_count = -1;
    bool skinned = false;
    bool matched = false;
    void* renderer = nullptr;
};

struct FirstPersonSession {
    std::string model_id;
    void* character = nullptr;
    uint32_t character_handle = 0;
    void* model = nullptr;
    uint32_t model_handle = 0;
    void* head = nullptr;
    uint32_t head_handle = 0;
    void* body = nullptr;
    uint32_t body_handle = 0;
    void* neck = nullptr;
    uint32_t neck_handle = 0;
    void* snapshot_controller = nullptr;
    uint32_t snapshot_handle = 0;
    bool snapshot_exit_owned = false;
    bool target_valid = false;
    // Entity root rotations; ModelGo may have a separate authored offset.
    Quaternion original_body_rotation{};
    Quaternion written_body_rotation{};
    bool body_rotation_owned = false;
    bool facing_pending = false;
    bool head_hide_applied = false;
    Vector3 last_eye{};
    bool last_eye_valid = false;
};
FirstPersonSession g_first_person;

uint64_t g_first_person_pump_frames = 0;
uint64_t g_first_person_reassert_frames = 0;
bool g_first_person_head_logged = false;
bool g_first_person_health_warned = false;
std::atomic<uint64_t> g_camera_unity_thread_id{0};
uint64_t g_first_person_binding_generation = 0;
double g_first_person_last_heartbeat = -1.0;

MethodContract g_contracts[]{
    {"camera.process_dither",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.View", "CameraMono",
            "_ProcessDitherByPitch", nullptr, "System.Void", 0}},
    {"camera_manager.tail_late_tick",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.View", "CameraManager",
            "TailLateTick", nullptr, "System.Void", 1}},
    {"player_controller.get_main_character",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "PlayerController",
            "GetMainCharacter", nullptr, nullptr, 0}},
    {"entity.get_model_com",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "Entity",
            "get_modelCom", nullptr, nullptr, 0}},
    {"base_model_component.get_model_go",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.View", "BaseModelComponent",
            "GetModelGo", nullptr, "UnityEngine.GameObject", 0}},
    // Optional idle-facing contracts. Missing state disables body turning only.
    {"fp.entity.rotation",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "Entity",
            "get_rotation", nullptr, "UnityEngine.Quaternion", 0}},
    {"fp.entity.alive",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "Entity",
            "get_alive", nullptr, "System.Boolean", 0}},
    {"fp.entity.in_cinematic",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "Entity",
            "get_inCinematic", nullptr, "System.Boolean", 0}},
    {"fp.entity.rotator",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "Entity",
            "get_rotateCom", nullptr, "Beyond.Gameplay.Core.RotatorComponent", 0}},
    {"fp.entity.movement",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "Entity",
            "get_movementComponent", nullptr, "Beyond.Gameplay.Core.MovementComponent", 0}},
    {"fp.entity.ability",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "Entity",
            "get_abilityCom", nullptr, "Beyond.Gameplay.Core.AbilitySystem", 0}},
    {"fp.rotator.is_rotating",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "RotatorComponent",
            "get_isRotating", nullptr, "System.Boolean", 0}},
    {"fp.rotator.lock_to_camera",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "RotatorComponent",
            "get_lockToCamera", nullptr, "System.Boolean", 0}},
    {"fp.rotator.set_rotation",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "RotatorComponent",
            "SetRotation", "UnityEngine.Quaternion", "System.Void", 1}},
    {"fp.movement.is_moving",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "MovementComponent",
            "get_isMoving", nullptr, "System.Boolean", 0}},
    {"fp.movement.is_in_air",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "MovementComponent",
            "get_isInAir", nullptr, "System.Boolean", 0}},
    {"fp.movement.root_motion_rotation",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "MovementComponent",
            "get_appliedRootMotionRotLastFrame", nullptr, "System.Boolean", 0}},
    {"fp.movement.root_motion_data",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "MovementComponent",
            "get_rootMotionData", nullptr, "Beyond.Gameplay.Core.MovementComponent.RootMotionData", 0}},
    {"fp.root_motion.has_motion",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "MovementComponent.RootMotionData",
            "get_hasRootMotion", nullptr, "System.Boolean", 0}},
    {"fp.ability.in_skill",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "AbilitySystem",
            "get_inSkill", nullptr, "System.Boolean", 0}},
    {"fp.ability.immobilized",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.Core", "AbilitySystem",
            "get_isImmobilized", nullptr, "System.Boolean", 0}},
    {"cinemachine.push_state",
        {"Cinemachine.dll", "Cinemachine", "CinemachineBrain",
            "PushStateToUnityCamera", "Cinemachine.CameraState&", "System.Void", 1},
        "Cinemachine.CameraState"},
    {"snapshot.set_first_person",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.View", "SnapshotCameraController",
            "SetFirstPerson", "System.Boolean", "System.Void", 1}},
    {"snapshot.show_char",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.View", "SnapshotCameraController",
            "_ShowChar", nullptr, "System.Void", 0}},
    {"snapshot.clear_first_person",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.View", "SnapshotCameraController",
            "_ClearFirstPersonStatus", nullptr, "System.Void", 0}},
    {"unity.camera.main",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Camera", "get_main",
            nullptr, "UnityEngine.Camera", 0}},
    {"unity.camera.fov.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Camera", "get_fieldOfView",
            nullptr, "System.Single", 0}},
    {"unity.component.transform",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Component", "get_transform",
            nullptr, "UnityEngine.Transform", 0}},
    {"unity.component.game_object",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Component", "get_gameObject",
            nullptr, "UnityEngine.GameObject", 0}},
    {"unity.game_object.transform",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "GameObject", "get_transform",
            nullptr, "UnityEngine.Transform", 0}},
    {"unity.game_object.get_component",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "GameObject", "GetComponent",
            "System.Type", "UnityEngine.Component", 1}},
    {"unity.game_object.get_components",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "GameObject", "GetComponents",
            "System.Type", "UnityEngine.Component[]", 1}},
    {"system.array.get_length",
        {"mscorlib.dll", "System", "Array", "GetLength",
            "System.Int32", "System.Int32", 1}},
    {"system.array.get_value",
        {"mscorlib.dll", "System", "Array", "GetValue",
            "System.Int32", "System.Object", 1}},
    {"unity.renderer.enabled.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "get_enabled",
            nullptr, "System.Boolean", 0}},
    {"unity.renderer.enabled.set",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "set_enabled",
            "System.Boolean", "System.Void", 1}},
    {"unity.renderer.shadow_mode.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "get_shadowCastingMode",
            nullptr, "UnityEngine.Rendering.ShadowCastingMode", 0}},
    {"unity.renderer.shadow_mode.set",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "set_shadowCastingMode",
            "UnityEngine.Rendering.ShadowCastingMode", "System.Void", 1}},
    {"unity.object.find_object_of_type",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Object", "FindObjectOfType",
            "System.Type|System.Boolean", "UnityEngine.Object", 2}},
    {"unity.object.name.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Object", "get_name",
            nullptr, "System.String", 0}},
    {"unity.skinned_mesh_renderer.shared_mesh.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "SkinnedMeshRenderer",
            "get_sharedMesh", nullptr, "UnityEngine.Mesh", 0}},
    {"unity.mesh.vertex_count.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "get_vertexCount",
            nullptr, "System.Int32", 0}},
    {"fp.model.id",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.View", "BaseModelComponent",
            "get_modelId", nullptr, "System.String", 0}},
    {"fp.mesh.readable",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "get_isReadable", nullptr, "System.Boolean", 0}},
    {"fp.mesh.vertices",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "get_vertices", nullptr, "UnityEngine.Vector3[]", 0}},
    {"fp.mesh.weights",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "get_boneWeights", nullptr, "UnityEngine.BoneWeight[]", 0}},
    {"fp.mesh.attributes.count",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "get_vertexAttributeCount", nullptr, "System.Int32", 0}},
    {"fp.mesh.attribute",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "GetVertexAttribute", "System.Int32", "UnityEngine.Rendering.VertexAttributeDescriptor", 1}},
    {"fp.mesh.submeshes.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "get_subMeshCount", nullptr, "System.Int32", 0}},
    {"fp.mesh.submeshes.set",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "set_subMeshCount", "System.Int32", "System.Void", 1}},
    {"fp.mesh.index_format.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "get_indexFormat", nullptr, "UnityEngine.Rendering.IndexFormat", 0}},
    {"fp.mesh.index_format.set",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "set_indexFormat", "UnityEngine.Rendering.IndexFormat", "System.Void", 1}},
    {"fp.mesh.submesh",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "GetSubMesh", "System.Int32", "UnityEngine.Rendering.SubMeshDescriptor", 1}},
    {"fp.mesh.indices.set",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "SetIndices", "System.Int32[]|UnityEngine.MeshTopology|System.Int32|System.Boolean|System.Int32", "System.Void", 5}},
    {"fp.mesh.indices.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "GetIndices", "System.Int32|System.Boolean", "System.Int32[]", 2}},
    {"fp.mesh.upload",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "UploadMeshData", "System.Boolean", "System.Void", 1}},
    {"fp.object.clone",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Object", "Internal_CloneSingle", "UnityEngine.Object", "UnityEngine.Object", 1}},
    {"fp.array.create",
        {"mscorlib.dll", "System", "Array", "CreateInstance", "System.Type|System.Int32", "System.Array", 2}},
    // Optional: Android IL2CPP strips Mesh.GetSubMesh. These public wrappers
    // (or GetIndices lengths) recover the same contiguous submesh ranges.
    {"fp.mesh.index_start",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "GetIndexStart", "System.Int32", "System.UInt32", 1}},
    {"fp.mesh.index_count",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "GetIndexCount", "System.Int32", "System.UInt32", 1}},
    {"fp.mesh.base_vertex",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "GetBaseVertex", "System.Int32", "System.UInt32", 1}},
    // Optional palette fallback for mixed renderers whose geometry is unreadable.
    {"fp.renderer.bones.set",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "SkinnedMeshRenderer", "set_bones",
            "UnityEngine.Transform[]", "System.Void", 1}},
    {"fp.array.clone",
        {"mscorlib.dll", "System", "Array", "Clone", nullptr, "System.Object", 0}},
    {"fp.array.set_value",
        {"mscorlib.dll", "System", "Array", "SetValue", "System.Object|System.Int32", "System.Void", 2}},
    {"fp.game_object.ctor",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "GameObject", ".ctor", "System.String", "System.Void", 1}},
    {"fp.transform.set_parent",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Transform", "SetParent",
            "UnityEngine.Transform|System.Boolean", "System.Void", 2}},
    {"fp.transform.local_position.set",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Transform", "set_localPosition",
            "UnityEngine.Vector3", "System.Void", 1}},
    {"fp.transform.local_rotation.set",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Transform", "set_localRotation",
            "UnityEngine.Quaternion", "System.Void", 1}},
    {"fp.transform.local_scale.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Transform", "get_localScale",
            nullptr, "UnityEngine.Vector3", 0}},
    {"fp.transform.local_scale.set",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Transform", "set_localScale",
            "UnityEngine.Vector3", "System.Void", 1}},
    {"unity.object.destroy",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Object", "Destroy",
            "UnityEngine.Object", "System.Void", 1}},
    {"unity.mesh.bindposes.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Mesh", "get_bindposes",
            nullptr, "UnityEngine.Matrix4x4[]", 0}},
    {"unity.skinned_mesh_renderer.bones.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "SkinnedMeshRenderer",
            "get_bones", nullptr, "UnityEngine.Transform[]", 0}},
    {"unity.transform.parent.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Transform", "get_parent",
            nullptr, "UnityEngine.Transform", 0}},
    {"unity.object.op_equality",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Object", "op_Equality",
            "UnityEngine.Object|UnityEngine.Object", "System.Boolean", 2}},
    {"unity.animator.get_bone_transform",
        {"UnityEngine.AnimationModule.dll", "UnityEngine", "Animator",
            "GetBoneTransform", "UnityEngine.HumanBodyBones",
            "UnityEngine.Transform", 1}},
    {"unity.transform.position.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Transform", "get_position",
            nullptr, "UnityEngine.Vector3", 0}},
    {"unity.transform.rotation.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Transform", "get_rotation",
            nullptr, "UnityEngine.Quaternion", 0}},
    {"unity.transform.child_count.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Transform", "get_childCount",
            nullptr, "System.Int32", 0}},
    {"unity.transform.get_child",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Transform", "GetChild",
            "System.Int32", "UnityEngine.Transform", 1}},
    {"unity.transform.find",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Transform", "Find",
            "System.String", "UnityEngine.Transform", 1}},
    {"unity.time.unscaled_delta.get",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Time",
            "get_unscaledDeltaTime", nullptr, "System.Single", 0}},
};

FieldContract g_fields[]{
    {"unity.vector3.x",
        {"UnityEngine.CoreModule.dll", "UnityEngine", "Vector3", "x",
            "System.Single"}},
    {"cinemachine.state.raw_position",
        {"Cinemachine.dll", "Cinemachine", "CameraState", "RawPosition",
            "UnityEngine.Vector3"}},
    {"cinemachine.state.raw_orientation",
        {"Cinemachine.dll", "Cinemachine", "CameraState", "RawOrientation",
            "UnityEngine.Quaternion"}},
    {"cinemachine.state.position_correction",
        {"Cinemachine.dll", "Cinemachine", "CameraState", "PositionCorrection",
            "UnityEngine.Vector3"}},
    {"cinemachine.state.orientation_correction",
        {"Cinemachine.dll", "Cinemachine", "CameraState", "OrientationCorrection",
            "UnityEngine.Quaternion"}},
    {"cinemachine.state.lens",
        {"Cinemachine.dll", "Cinemachine", "CameraState", "Lens",
            "Cinemachine.LensSettings"}},
    {"cinemachine.lens.field_of_view",
        {"Cinemachine.dll", "Cinemachine", "LensSettings", "FieldOfView",
            "System.Single"}},
    {"cinemachine.lens.near_clip",
        {"Cinemachine.dll", "Cinemachine", "LensSettings", "NearClipPlane",
            "System.Single"}},
    {"cinemachine.lens.dutch",
        {"Cinemachine.dll", "Cinemachine", "LensSettings", "Dutch",
            "System.Single"}},
    {"snapshot.is_first_person",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.View", "SnapshotCameraController",
            "<isFirstPerson>k__BackingField", "System.Boolean"}},
    {"snapshot.pending_first_person_exit",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.View", "SnapshotCameraController",
            "m_hasPendingFirstPersonExit", "System.Boolean"}},
    {"snapshot.post_rotation",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.View", "SnapshotCameraController",
            "m_postRotation", "Beyond.Gameplay.View.CameraPostRotation"}},
    {"snapshot.camera_offset",
        {"Gameplay.Beyond.dll", "Beyond.Gameplay.View", "SnapshotCameraController",
            "m_cameraOffset", "CinemachineCameraOffset"}},
};

MethodContract* Contract(std::string_view key) {
    for (auto& contract : g_contracts) {
        if (contract.key == key) {
            return &contract;
        }
    }
    return nullptr;
}

FieldContract* Field(std::string_view key) {
    for (auto& field : g_fields) {
        if (field.key == key) {
            return &field;
        }
    }
    return nullptr;
}

void Log(const std::string& message) {
    if (g_host && g_host->log) {
        g_host->log(g_host->context, kModuleId, message.c_str());
    }
}

void* Invoke(const MethodContract* method, void* instance, void** parameters) {
    if (!method || !method->method_info || !g_host || !g_host->runtime_invoke) {
        return nullptr;
    }
    void* exception = nullptr;
    void* result = g_host->runtime_invoke(g_host->context, method->method_info,
        instance, parameters, &exception);
    return exception ? nullptr : result;
}

bool InvokeVoid(const MethodContract* method, void* instance, void** parameters) {
    if (!method || !method->method_info || !g_host || !g_host->runtime_invoke) {
        return false;
    }
    void* exception = nullptr;
    g_host->runtime_invoke(g_host->context, method->method_info,
        instance, parameters, &exception);
    return exception == nullptr;
}

template <typename T>
bool Unbox(void* boxed, T& value) {
    if (!boxed || !g_host || !g_host->object_unbox) {
        return false;
    }
    void* raw = g_host->object_unbox(g_host->context, boxed);
    if (!raw) {
        return false;
    }
    std::memcpy(&value, raw, sizeof(T));
    return true;
}

bool GetValue(const MethodContract* method, void* instance, Vector3& value) {
    return Unbox(Invoke(method, instance, nullptr), value);
}

bool GetValue(const MethodContract* method, void* instance, float& value) {
    return Unbox(Invoke(method, instance, nullptr), value);
}

bool GetValue(const MethodContract* method, void* instance, int& value) {
    return Unbox(Invoke(method, instance, nullptr), value);
}

bool GetValue(const MethodContract* method, void* instance, bool& value) {
    return Unbox(Invoke(method, instance, nullptr), value);
}

int ManagedArrayLength(void* array) {
    if (!array) return 0;
    int dimension = 0;
    void* parameters[1]{&dimension};
    int length = 0;
    return Unbox(Invoke(Contract("system.array.get_length"), array, parameters), length)
        ? length : 0;
}

void* ManagedArrayValue(void* array, int index) {
    if (!array) return nullptr;
    void* parameters[1]{&index};
    return Invoke(Contract("system.array.get_value"), array, parameters);
}

bool SetValue(const MethodContract* method, void* instance, Vector3 value) {
    void* parameters[1]{&value};
    return InvokeVoid(method, instance, parameters);
}

[[maybe_unused]] bool SetValue(const MethodContract* method, void* instance, float value) {
    void* parameters[1]{&value};
    return InvokeVoid(method, instance, parameters);
}

Vector3 Add(Vector3 left, Vector3 right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vector3 Subtract(Vector3 left, Vector3 right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vector3 Scale(Vector3 value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vector3 Normalize(Vector3 value) {
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
    return length > 0.0001f ? Scale(value, 1.0f / length) : Vector3{};
}

float Magnitude(Vector3 value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

bool IsFinite(Vector3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool IsUnitQuaternion(Quaternion value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y) ||
        !std::isfinite(value.z) || !std::isfinite(value.w)) {
        return false;
    }
    const float length = value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w;
    return length > 0.5f && length < 1.5f;
}

// Rotates a vector by a unit quaternion without touching managed code.
Vector3 RotateVector(Quaternion rotation, Vector3 vector) {
    const Vector3 axis{rotation.x, rotation.y, rotation.z};
    const Vector3 doubled_cross{
        2.0f * (axis.y * vector.z - axis.z * vector.y),
        2.0f * (axis.z * vector.x - axis.x * vector.z),
        2.0f * (axis.x * vector.y - axis.y * vector.x)};
    return {
        vector.x + rotation.w * doubled_cross.x +
            (axis.y * doubled_cross.z - axis.z * doubled_cross.y),
        vector.y + rotation.w * doubled_cross.y +
            (axis.z * doubled_cross.x - axis.x * doubled_cross.z),
        vector.z + rotation.w * doubled_cross.z +
            (axis.x * doubled_cross.y - axis.y * doubled_cross.x)};
}

std::string ReadManagedString(void* managed_string) {
    if (!managed_string || !g_host || !g_host->copy_managed_string) return {};
    char buffer[256]{};
    g_host->copy_managed_string(g_host->context, managed_string, buffer, sizeof(buffer));
    return std::string(buffer);
}

std::string ObjectName(void* object) {
    if (!object) return "<null>";
    return ReadManagedString(Invoke(Contract("unity.object.name.get"), object, nullptr));
}

// UnityEngine.Object::op_Equality is the safe way to tell whether a scene
// object is still alive: it compares the cached native pointers instead of
// dereferencing them, so a destroyed object cannot crash the check.
enum class ObjectLiveness { Alive, Destroyed, Unknown };
ObjectLiveness QueryObjectLiveness(void* object) {
    if (!object) {
        return ObjectLiveness::Destroyed;
    }
    void* parameters[2]{object, nullptr};
    bool destroyed = false;
    if (!Unbox(Invoke(Contract("unity.object.op_equality"), nullptr, parameters),
            destroyed)) {
        return ObjectLiveness::Unknown;
    }
    return destroyed ? ObjectLiveness::Destroyed : ObjectLiveness::Alive;
}
bool IsObjectAlive(void* object) { return QueryObjectLiveness(object) == ObjectLiveness::Alive; }


Quaternion Multiply(Quaternion a, Quaternion b) {
    return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y, a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w, a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
[[maybe_unused]] Quaternion Conjugate(Quaternion q) { return {-q.x,-q.y,-q.z,q.w}; }
Quaternion NormalizeRotation(Quaternion q) {
    float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    return std::isfinite(n)&&n>0.0001f?Quaternion{q.x/n,q.y/n,q.z/n,q.w/n}:Quaternion{};
}
double NowSeconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
void ReleaseCameraRoot() {
    if(g_active_camera_root&&g_host)g_host->gchandle_free(g_host->context,g_active_camera_root);
    g_active_camera_root=0;g_active_camera=nullptr;
}
bool ReadBytes(const void* base, int32_t offset, void* destination, size_t size) {
    if (!base || offset < 0 || static_cast<size_t>(offset) > kMaxFieldOffset) {
        return false;
    }
    std::memcpy(destination, static_cast<const uint8_t*>(base) + offset, size);
    return true;
}

bool WriteBytes(void* base, int32_t offset, const void* source, size_t size) {
    if (!base || offset < 0 || static_cast<size_t>(offset) > kMaxFieldOffset) {
        return false;
    }
    std::memcpy(static_cast<uint8_t*>(base) + offset, source, size);
    return true;
}

// Normalizes one metadata field offset into a struct body relative offset. The
// arithmetic is intentionally repeated per nesting level: every value type
// carries its own boxed header, so CameraState.Lens needs one subtraction and
// LensSettings.FieldOfView needs another.
bool StructFieldOffset(std::string_view key, int32_t& offset) {
    const FieldContract* field = Field(key);
    if (!field || !field->ready || field->resolved.offset <= 0) {
        return false;
    }
    const int32_t normalized = field->resolved.offset - g_value_type_header;
    if (normalized < 0 || static_cast<size_t>(normalized) > kMaxFieldOffset) {
        return false;
    }
    offset = normalized;
    return true;
}

bool BuildCameraStateLayout() {
    CameraStateLayout layout;
    const bool complete =
        StructFieldOffset("cinemachine.state.raw_position", layout.raw_position) &&
        StructFieldOffset("cinemachine.state.raw_orientation", layout.raw_orientation) &&
        StructFieldOffset("cinemachine.state.position_correction", layout.position_correction) &&
        StructFieldOffset("cinemachine.state.orientation_correction", layout.orientation_correction) &&
        StructFieldOffset("cinemachine.state.lens", layout.lens) &&
        StructFieldOffset("cinemachine.lens.field_of_view", layout.lens_field_of_view) &&
        StructFieldOffset("cinemachine.lens.near_clip", layout.lens_near_clip) &&
        StructFieldOffset("cinemachine.lens.dutch", layout.lens_dutch);
    if (!complete) {
        Log("First person: CameraState layout could not be resolved on this build.");
        return false;
    }
    layout.ready = true;
    g_state_layout = layout;
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
        "First person: CameraState layout resolved (header=0x%X rawPos=0x%X rawRot=0x%X "
        "lens=0x%X fov=0x%X nearClip=0x%X dutch=0x%X)",
        g_value_type_header, layout.raw_position, layout.raw_orientation,
        layout.lens, layout.lens_field_of_view, layout.lens_near_clip,
        layout.lens_dutch);
    Log(buffer);
    return true;
}

bool ReadBoolField(void* instance, std::string_view key, bool& value) {
    const FieldContract* field = Field(key);
    if (!instance || !field || !field->ready || field->resolved.offset <= 0) {
        return false;
    }
    value = *reinterpret_cast<const uint8_t*>(
        static_cast<const uint8_t*>(instance) + field->resolved.offset) != 0;
    return true;
}

void* ReadObjectField(void* instance, std::string_view key) {
    const FieldContract* field = Field(key);
    if (!instance || !field || !field->ready || field->resolved.offset <= 0 ||
        static_cast<size_t>(field->resolved.offset) > kMaxFieldOffset) return nullptr;
    void* value = nullptr;
    std::memcpy(&value, static_cast<const uint8_t*>(instance) + field->resolved.offset,
        sizeof(value));
    return value;
}

bool OnCameraUnityThread(bool allow_capture) {
    const uint64_t thread = CurrentThreadId();
    uint64_t expected = 0;
    if (allow_capture) {
        g_camera_unity_thread_id.compare_exchange_strong(expected, thread,
            std::memory_order_acq_rel);
    }
    return g_camera_unity_thread_id.load(std::memory_order_acquire) == thread;
}

void* FindSnapshotCameraController() {
    if (g_first_person.snapshot_controller) {
        if (IsObjectAlive(g_first_person.snapshot_controller)) {
            return g_first_person.snapshot_controller;
        }
        if (g_first_person.snapshot_handle && g_host && g_host->gchandle_free) {
            g_host->gchandle_free(g_host->context, g_first_person.snapshot_handle);
        }
        g_first_person.snapshot_handle = 0;
        g_first_person.snapshot_controller = nullptr;
    }
    if (!g_snapshot_controller_class.type_object) {
        return nullptr;
    }
    bool include_inactive = true;
    void* parameters[2]{g_snapshot_controller_class.type_object, &include_inactive};
    void* controller = Invoke(Contract("unity.object.find_object_of_type"),
        nullptr, parameters);
    if (!controller) {
        return nullptr;
    }
    g_first_person.snapshot_controller = controller;
    g_first_person.snapshot_handle = g_host && g_host->gchandle_new
        ? g_host->gchandle_new(g_host->context, controller, 0)
        : 0;
    Log("First person: SnapshotCameraController found (" + ObjectName(controller) + ").");
    return controller;
}

// ---------------------------------------------------------------------------
// First-person head hiding
//
// Classify the live skeleton and drawn skin influences. Standalone head/tail
// renderers keep casting shadows; mixed meshes retain their complete shadow
// source while a private index buffer removes only confirmed head triangles.
// Names are scan hints, never permission to hide a mixed body/clothing mesh.
// ---------------------------------------------------------------------------

constexpr const char* kHeadPartTokens[]{
    "head", "face", "hair", "hair_base", "front_hair",
    "back_hair", "side_hair", "brow", "eyelid", "eyelash", "eyes", "iris",
    "pupil", "mouth", "lip", "teeth", "tongue", "horn"};

constexpr int kMaxPartScanDepth = 16;
constexpr int kMaxPartScanNodes = 4096;
constexpr int kMaxPartLogEntries = 128;

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

bool MatchesHeadPartToken(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    const std::string lower = LowerAscii(name);
    for (const char* token : kHeadPartTokens) {
        if (lower.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void ReadMeshProbe(void* mesh, HeadPartProbe& probe) {
    if (!mesh) {
        return;
    }
    probe.mesh_name = ObjectName(mesh);
    const MethodContract* vertex_count = Contract("unity.mesh.vertex_count.get");
    int count = 0;
    if (vertex_count && vertex_count->resolved &&
        GetValue(vertex_count, mesh, count)) {
        probe.vertex_count = count;
    }
}

void AddRendererProbe(void* renderer, const std::string& object_name,
    std::vector<HeadPartProbe>& parts) {
    if (!renderer) return;
    HeadPartProbe probe;
    probe.object_name = object_name;
    probe.renderer_name = ObjectName(renderer);
    probe.skinned = true;
    probe.renderer = renderer;
    ReadMeshProbe(Invoke(
        Contract("unity.skinned_mesh_renderer.shared_mesh.get"), renderer, nullptr),
        probe);
    probe.matched = MatchesHeadPartToken(probe.object_name) ||
        MatchesHeadPartToken(probe.renderer_name) ||
        MatchesHeadPartToken(probe.mesh_name);
    parts.push_back(std::move(probe));
}

// GetComponents(Type) is used instead of GetComponent(Type), because a custom
// model or prefab can put multiple SkinnedMeshRenderers on one GameObject.
void ReadPartComponents(void* game_object, const std::string& object_name,
    std::vector<HeadPartProbe>& parts) {
    const MethodContract* get_component = Contract("unity.game_object.get_component");
    if (!game_object || !get_component || !get_component->resolved ||
        !g_skinned_mesh_renderer_class.type_object) {
        return;
    }
    const MethodContract* get_components = Contract("unity.game_object.get_components");
    if (get_components && get_components->resolved &&
        Contract("system.array.get_length")->resolved &&
        Contract("system.array.get_value")->resolved) {
        void* parameters[1]{g_skinned_mesh_renderer_class.type_object};
        void* array = Invoke(get_components, game_object, parameters);
        const int count = std::min(ManagedArrayLength(array), 32);
        for (int i = 0; i < count; ++i) {
            AddRendererProbe(ManagedArrayValue(array, i), object_name, parts);
        }
        if (count > 0) return;
    }
    void* parameters[1]{g_skinned_mesh_renderer_class.type_object};
    void* renderer = Invoke(get_component, game_object, parameters);
    AddRendererProbe(renderer, object_name, parts);
}

void ScanPartNodes(void* transform, int depth, int& visited,
    std::vector<HeadPartProbe>& parts, std::string& tree, int& logged) {
    if (!transform || depth > kMaxPartScanDepth || visited >= kMaxPartScanNodes) {
        return;
    }
    ++visited;
    const std::string object_name = ObjectName(transform);
    const size_t before = parts.size();
    ReadPartComponents(Invoke(Contract("unity.component.game_object"), transform, nullptr),
        object_name, parts);

    if (parts.size() > before) {
        if (logged < kMaxPartLogEntries) {
            ++logged;
            std::string line(static_cast<size_t>(depth) * 2, ' ');
            line += object_name + " [renderers=" +
                std::to_string(parts.size() - before) + "]";
            tree += "\n  " + line;
        } else if (logged == kMaxPartLogEntries) {
            ++logged;
            tree += "\n  ...";
        }
    }
    const MethodContract* child_count = Contract("unity.transform.child_count.get");
    const MethodContract* get_child = Contract("unity.transform.get_child");
    int children = 0;
    if (!child_count || !get_child || !GetValue(child_count, transform, children)) {
        return;
    }
    for (int index = 0; index < children; ++index) {
        void* parameters[1]{&index};
        ScanPartNodes(Invoke(get_child, transform, parameters), depth + 1, visited,
            parts, tree, logged);
    }
}

[[maybe_unused]] void* FindModelComponent() {
    void* entity = Invoke(Contract("player_controller.get_main_character"),
        nullptr, nullptr);
    return entity ? Invoke(Contract("entity.get_model_com"), entity, nullptr)
                  : nullptr;
}

void* FindModelObjectForCharacter(void* character) {
    void* model_component = character
        ? Invoke(Contract("entity.get_model_com"), character, nullptr) : nullptr;
    return model_component
        ? Invoke(Contract("base_model_component.get_model_go"), model_component,
            nullptr)
        : nullptr;
}

void* FindModelObject() {
    return FindModelObjectForCharacter(Invoke(
        Contract("player_controller.get_main_character"), nullptr, nullptr));
}

[[maybe_unused]] void* FindModelTransform() {
    void* model_object = FindModelObject();
    return model_object
        ? Invoke(Contract("unity.game_object.transform"), model_object, nullptr)
        : nullptr;
}

#include "first_person_runtime.inc"
bool FirstPersonFacingIsIdle(void*& rotator) {
    if (!g_first_person.character) return false;
    bool alive = false;
    if (!GetValue(Contract("fp.entity.alive"), g_first_person.character, alive) || !alive)
        return false;
    rotator = Invoke(Contract("fp.entity.rotator"), g_first_person.character, nullptr);
    void* movement = Invoke(Contract("fp.entity.movement"), g_first_person.character, nullptr);
    void* ability = Invoke(Contract("fp.entity.ability"), g_first_person.character, nullptr);
    const auto* setter = Contract("fp.rotator.set_rotation");
    if (!rotator || !movement || !ability || !setter || !setter->resolved) return false;
    const struct { const char* key; void* instance; } checks[]{
        {"fp.entity.in_cinematic", g_first_person.character},
        {"fp.rotator.is_rotating", rotator},
        {"fp.rotator.lock_to_camera", rotator},
        {"fp.movement.is_moving", movement},
        {"fp.movement.is_in_air", movement},
        {"fp.movement.root_motion_rotation", movement},
        {"fp.ability.in_skill", ability},
        {"fp.ability.immobilized", ability},
    };
    for (const auto& check : checks) {
        bool active = false;
        if (!GetValue(Contract(check.key), check.instance, active) || active) return false;
    }
    // A null data object is idle; a failed getter is unknown, not idle.
    const auto* getter = Contract("fp.movement.root_motion_data");
    if (!getter || !getter->method_info || !g_host || !g_host->runtime_invoke) return false;
    void* exception = nullptr;
    void* root_motion = g_host->runtime_invoke(g_host->context, getter->method_info,
        movement, nullptr, &exception);
    if (exception) return false;
    bool has_motion = false;
    return !root_motion || (GetValue(Contract("fp.root_motion.has_motion"), root_motion,
        has_motion) && !has_motion);
}

bool FirstPersonSameRotation(Quaternion a, Quaternion b) {
    if (!IsUnitQuaternion(a) || !IsUnitQuaternion(b)) return false;
    a = NormalizeRotation(a);
    b = NormalizeRotation(b);
    return std::abs(a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w) >= 0.999999f;
}

void RestoreFirstPersonFacing() {
    if(!g_first_person.body_rotation_owned)return;
    g_first_person.body_rotation_owned=false;
    if(!IsObjectAlive(g_first_person.body))return;
    void* rotator = nullptr;
    if (!FirstPersonFacingIsIdle(rotator)) return;
    Quaternion current{};
    if(!Unbox(Invoke(Contract("fp.entity.rotation"),g_first_person.character,nullptr),current))return;
    // The game may have taken over facing during movement or an action.
    if(!FirstPersonSameRotation(current,g_first_person.written_body_rotation))return;
    void* parameters[]{&g_first_person.original_body_rotation};
    InvokeVoid(Contract("fp.rotator.set_rotation"),rotator,parameters);
}

void ReleaseHeadTransform() {
    g_first_person.model_id.clear();
    g_first_person.target_valid=false;
    g_first_person_view_forward_valid=false;
    g_first_person.facing_pending=false;
    RestoreFirstPersonFacing();
    if (g_first_person.head_handle && g_host && g_host->gchandle_free) {
        g_host->gchandle_free(g_host->context, g_first_person.head_handle);
    }
    g_first_person.head_handle = 0;
    g_first_person.head = nullptr;
    if (g_first_person.body_handle && g_host && g_host->gchandle_free) {
        g_host->gchandle_free(g_host->context, g_first_person.body_handle);
    }
    g_first_person.body_handle = 0;
    g_first_person.body = nullptr;
    if (g_first_person.neck_handle && g_host && g_host->gchandle_free) {
        g_host->gchandle_free(g_host->context, g_first_person.neck_handle);
    }
    g_first_person.neck_handle = 0;
    g_first_person.neck = nullptr;
    if(g_first_person.model_handle&&g_host&&g_host->gchandle_free)
        g_host->gchandle_free(g_host->context,g_first_person.model_handle);
    if(g_first_person.character_handle&&g_host&&g_host->gchandle_free)
        g_host->gchandle_free(g_host->context,g_first_person.character_handle);
    g_first_person.model_handle=0;
    g_first_person.model=nullptr;
    g_first_person.character_handle=0;
    g_first_person.character=nullptr;
    g_first_person.last_eye_valid = false;
    g_first_person_head_logged = false;
}

// The neck bone defines the search frame for the real opening. Missing neck
// metadata disables cap detection; head hiding can still use the head palette.
void BindNeckBone(void* head) {
    if (g_first_person.neck) {
        return;
    }
    void* parent = head
        ? Invoke(Contract("unity.transform.parent.get"), head, nullptr)
        : nullptr;
    if (!parent) {
        return;
    }
    std::string name = LowerAscii(ObjectName(parent));
    if (name.find("neck") == std::string::npos) {
        return;
    }
    g_first_person.neck = parent;
    g_first_person.neck_handle = g_host && g_host->gchandle_new
        ? g_host->gchandle_new(g_host->context, parent, 0)
        : 0;
}

bool LooksLikeHeadBone(void* transform) {
    const std::string name = ObjectName(transform);
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower.find("head") != std::string::npos;
}

void* FindHeadBoneRecursive(void* current, int depth) {
    if (!current || depth > 12 || !IsObjectAlive(current)) {
        return nullptr;
    }
    if (LooksLikeHeadBone(current)) {
        return current;
    }
    int child_count = 0;
    if (!GetValue(Contract("unity.transform.child_count.get"), current, child_count)) {
        return nullptr;
    }
    for (int index = 0; index < child_count; ++index) {
        void* parameters[1]{&index};
        void* child = Invoke(Contract("unity.transform.get_child"), current, parameters);
        if (child) {
            if (void* found = FindHeadBoneRecursive(child, depth + 1)) {
                return found;
            }
        }
    }
    return nullptr;
}

void* TryHumanoidHeadBone(void* model_root) {
    if (!model_root || !g_animator_class.type_object) {
        return nullptr;
    }
    void* game_object = Invoke(Contract("unity.component.game_object"), model_root, nullptr);
    if (!game_object) {
        return nullptr;
    }
    void* find_parameters[1]{g_animator_class.type_object};
    void* animator = Invoke(Contract("unity.game_object.get_component"),
        game_object, find_parameters);
    if (!animator) {
        return nullptr;
    }
    int32_t bone = kHumanBodyBonesHead;
    void* parameters[1]{&bone};
    void* head = Invoke(Contract("unity.animator.get_bone_transform"), animator, parameters);
    // The humanoid enum value is verified by name instead of being trusted, so a
    // shifted enum on another game build degrades into the name search below.
    return head && LooksLikeHeadBone(head) ? head : nullptr;
}

void* ResolvePlayerHeadTransform(void* model_root) {
    if (!model_root || !IsObjectAlive(model_root)) {
        return nullptr;
    }

    if (void* head = TryHumanoidHeadBone(model_root)) {
        return head;
    }
    void* head_path = g_host->string_new(g_host->context,
        "Root/Bip001/Bip001_Pelvis/Bip001_Spine/Bip001_Spine1/Bip001_Spine2/Bip001_Neck/Bip001_Head");
    void* find_parameters[1]{head_path};
    if (void* head = Invoke(Contract("unity.transform.find"), model_root, find_parameters)) {
        return head;
    }
    if (void* head = FindHeadBoneRecursive(model_root, 0)) {
        return head;
    }
    // A model-root fallback would classify every skinned bone as head-owned.
    // Wait for the actual skeleton rather than binding a different player tag.
    return nullptr;
}

void TryBindHeadTransform() {
    if (g_first_person.head) {
        return;
    }
    void* head = ResolvePlayerHeadTransform(g_first_person.body);
    if (!head) {
        if (!g_first_person_head_logged) {
            g_first_person_head_logged = true;
            Log("First person: the player model is not loaded yet; retrying.");
        }
        return;
    }
    g_first_person.head = head;
    g_first_person.head_handle = g_host && g_host->gchandle_new
        ? g_host->gchandle_new(g_host->context, head, 0)
        : 0;
    BindNeckBone(head);
    Log("First person: head anchor bound to \"" + ObjectName(head) + "\".");
}

float NormalizeAngle(float angle) {
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

void ApplyFirstPersonFacing() {
    if (!g_first_person.target_valid || !g_first_person.body ||
        !g_first_person_view_forward_valid ||
        !g_first_person_camera_enabled.load(std::memory_order_acquire) ||
        g_first_person_exit_request.load(std::memory_order_acquire) ||
        !IsObjectAlive(g_first_person.body)) return;
    Vector3 desired = g_first_person_view_forward;
    desired.y = 0.0f;
    desired = Normalize(desired);
    if (Magnitude(desired) < 0.001f) return;

    Quaternion current_rotation{};
    void* rotator = nullptr;
    if (!FirstPersonFacingIsIdle(rotator)) {
        g_first_person.body_rotation_owned = false;
        return;
    }
    if (!Unbox(Invoke(Contract("fp.entity.rotation"),
            g_first_person.character, nullptr), current_rotation) ||
        !IsUnitQuaternion(current_rotation)) return;
    if (g_first_person.body_rotation_owned &&
        !FirstPersonSameRotation(current_rotation, g_first_person.written_body_rotation)) {
        g_first_person.body_rotation_owned = false;
        return;
    }
    current_rotation = NormalizeRotation(current_rotation);
    Vector3 current = RotateVector(current_rotation, {0.0f, 0.0f, 1.0f});
    current.y = 0.0f;
    current = Normalize(current);
    if (Magnitude(current) < 0.001f) return;

    const float current_yaw = std::atan2(current.x, current.z) * 57.2957795f;
    const float desired_yaw = std::atan2(desired.x, desired.z) * 57.2957795f;
    const float delta = NormalizeAngle(desired_yaw - current_yaw);
    const float limit = g_first_person_side_look_limit.load(std::memory_order_relaxed);
    if (std::abs(delta) <= limit) return;

    float dt = 1.0f / 60.0f;
    GetValue(Contract("unity.time.unscaled_delta.get"), nullptr, dt);
    if (!std::isfinite(dt)) return;
    dt = std::clamp(dt, 0.001f, 0.1f);
    const float max_step = g_first_person_turn_speed.load(std::memory_order_relaxed) * dt;
    const float step = std::clamp(delta, -max_step, max_step);
    const float yaw = step * 0.01745329252f;
    const Quaternion delta_rotation{0.0f, std::sin(yaw * 0.5f), 0.0f, std::cos(yaw * 0.5f)};
    const Quaternion next = Multiply(delta_rotation, current_rotation);
    void* parameters[1]{const_cast<Quaternion*>(&next)};
    // Rotator updates the entity root and its forward, preserving model offsets.
    // SetRotation also stops a rotating task, hence the mandatory idle checks.
    if (InvokeVoid(Contract("fp.rotator.set_rotation"), rotator, parameters)) {
        if(!g_first_person.body_rotation_owned)
            g_first_person.original_body_rotation=current_rotation;
        g_first_person.written_body_rotation=next;
        g_first_person.body_rotation_owned=true;
        if (g_diagnostics_enabled.load(std::memory_order_relaxed)) {
            Log("First person: body turned toward camera (delta=" +
                std::to_string(delta) + ", step=" + std::to_string(step) + ").");
        }
    }
}

// Rewrites the CameraState that Cinemachine is about to push to the Unity
// camera. The orientation is kept exactly as the game produced it (so aim
// offsets and look input stay authoritative) and only the position is moved to
// the eye anchor. Only position correction is cleared; orientation correction
// and Dutch remain game-owned and define the final eye basis.
void ApplyFirstPersonState(void* state) {
    g_first_person_view_forward_valid = false;
    if (!g_state_layout.ready || !g_first_person.target_valid ||
        !g_first_person_camera_enabled.load(std::memory_order_acquire) ||
        g_first_person_exit_request.load(std::memory_order_acquire) ||
        !IsObjectAlive(g_first_person.head)) {
        return;
    }

    Quaternion raw{}, correction{};
    float dutch = 0;
    if (!ReadBytes(state, g_state_layout.raw_orientation, &raw, sizeof(raw)) ||
        !ReadBytes(state, g_state_layout.orientation_correction, &correction, sizeof(correction)) ||
        !ReadBytes(state, g_state_layout.lens + g_state_layout.lens_dutch, &dutch, sizeof(dutch)) ||
        !IsUnitQuaternion(raw) || !IsUnitQuaternion(correction) || !std::isfinite(dutch)) {
        return;
    }
    const float roll = dutch * 0.00872664626f;
    const Quaternion orientation = NormalizeRotation(Multiply(Multiply(raw, correction),
        Quaternion{0, 0, std::sin(roll), std::cos(roll)}));
    const Vector3 forward = RotateVector(orientation, Vector3{0.0f, 0.0f, 1.0f});
    const Vector3 up = RotateVector(orientation, Vector3{0.0f, 1.0f, 0.0f});
    g_first_person_view_forward = forward;
    g_first_person_view_forward_valid = IsFinite(forward);
    if (g_first_person.facing_pending) {
        g_first_person.facing_pending = false;
        ApplyFirstPersonFacing();
    }
    // Turning the entity moves its head in world space. Read the anchor after
    // facing, in the same push, rather than turning after the camera was placed.
    Vector3 head{};
    if (!GetValue(Contract("unity.transform.position.get"),
            g_first_person.head, head) || !IsFinite(head)) {
        return;
    }

    const Vector3 eye = Add(head,
        Add(Scale(forward, kFirstPersonEyeForward), Scale(up, kFirstPersonEyeUp)));

    const float field_of_view = g_first_person_fov.load(std::memory_order_relaxed);
    const float near_clip = kFirstPersonNearClip;
    const Vector3 no_correction{};
    const int32_t lens = g_state_layout.lens;

    WriteBytes(state, g_state_layout.raw_position, &eye, sizeof(eye));
    WriteBytes(state, g_state_layout.position_correction, &no_correction,
        sizeof(no_correction));
    WriteBytes(state, lens + g_state_layout.lens_field_of_view, &field_of_view,
        sizeof(field_of_view));
    WriteBytes(state, lens + g_state_layout.lens_near_clip, &near_clip,
        sizeof(near_clip));

    g_first_person.last_eye = eye;
    g_first_person.last_eye_valid = true;
    g_push_state_patches.fetch_add(1, std::memory_order_relaxed);
}

// PushState takes a ref to Cinemachine's cached state. Restore the input after
// the original push so leaving first person cannot retain our lens/corrections.
struct ScopedFirstPersonState {
    void* state=nullptr;
    Vector3 position{},correction{};
    Quaternion orientation_correction{};
    float fov=0,near_clip=0;
    explicit ScopedFirstPersonState(void* value,bool capture) {
        if(!capture||!g_state_layout.ready)return;
        const int lens=g_state_layout.lens;
        if(ReadBytes(value,g_state_layout.raw_position,&position,sizeof(position))&&
           ReadBytes(value,g_state_layout.position_correction,&correction,sizeof(correction))&&
           ReadBytes(value,g_state_layout.orientation_correction,&orientation_correction,sizeof(orientation_correction))&&
           ReadBytes(value,lens+g_state_layout.lens_field_of_view,&fov,sizeof(fov))&&
           ReadBytes(value,lens+g_state_layout.lens_near_clip,&near_clip,sizeof(near_clip)))state=value;
    }
    ~ScopedFirstPersonState() {
        if(!state)return;
        const int lens=g_state_layout.lens;
        WriteBytes(state,g_state_layout.raw_position,&position,sizeof(position));
        WriteBytes(state,g_state_layout.position_correction,&correction,sizeof(correction));
        WriteBytes(state,g_state_layout.orientation_correction,&orientation_correction,sizeof(orientation_correction));
        WriteBytes(state,lens+g_state_layout.lens_field_of_view,&fov,sizeof(fov));
        WriteBytes(state,lens+g_state_layout.lens_near_clip,&near_clip,sizeof(near_clip));
    }
};

// Confirms that the patched state actually reached the Unity camera. A large
// distance means the offsets or the hook are wrong on this game build, which is
// reported instead of silently producing a broken view.
void FirstPersonHealthCheck() {
    if (!g_first_person.last_eye_valid) {
        return;
    }
    void* camera = Invoke(Contract("unity.camera.main"), nullptr, nullptr);
    void* transform = camera
        ? Invoke(Contract("unity.component.transform"), camera, nullptr)
        : nullptr;
    Vector3 camera_position{};
    if (!transform ||
        !GetValue(Contract("unity.transform.position.get"), transform, camera_position)) {
        return;
    }
    const float distance = Magnitude(Subtract(camera_position, g_first_person.last_eye));
    const uint64_t patches = g_push_state_patches.load(std::memory_order_relaxed);
    const uint64_t calls = g_push_state_calls.load(std::memory_order_relaxed);
    if (g_diagnostics_enabled.load(std::memory_order_relaxed)) {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer),
            "First person pose check: eyeDistance=%.2f (pushCalls=%llu patches=%llu)",
            distance, static_cast<unsigned long long>(calls),
            static_cast<unsigned long long>(patches));
        Log(buffer);
    }
    if (calls == 0) {
        if (!g_first_person_health_warned) {
            g_first_person_health_warned = true;
            Log("First person: the CameraState hook is installed but never called on "
                "this build; the camera may be driven by a different pipeline.");
        }
    } else if (distance > 1.5f && !g_first_person_health_warned) {
        g_first_person_health_warned = true;
        Log("First person: the Unity camera is not following the pushed state; the "
            "CameraState offsets are likely wrong on this build.");
    }
}

void RefreshFirstPersonTarget(bool queue_facing = true);

bool EnterFirstPerson() {
    if (!g_state_layout.ready || !g_push_state_hook_ready) {
        Log("First person camera is unavailable: the Cinemachine CameraState hook "
            "is not active on this build.");
        return false;
    }
    void* camera = Invoke(Contract("unity.camera.main"), nullptr, nullptr);
    if (!camera || !IsObjectAlive(camera)) {
        Log("First person camera could not capture the active Unity camera.");
        return false;
    }
    g_active_camera = camera;
    g_active_camera_root = g_host && g_host->gchandle_new
        ? g_host->gchandle_new(g_host->context, camera, 0)
        : 0;
    float original_fov = 0.0f;
    GetValue(Contract("unity.camera.fov.get"), camera, original_fov);

    g_first_person.character = nullptr;
    g_first_person_head_logged = false;
    g_first_person_pump_frames = 0;
    g_first_person_active.store(true, std::memory_order_release);
    RefreshFirstPersonTarget(false);
    if (g_first_person_hide_head.load(std::memory_order_relaxed)) {
        ExitGameFirstPersonMode();
        if(g_first_person.target_valid) {
            ApplyHeadPartHide();
            EnsureNeckCap();
        }
    } else {
        Log("First person: head hiding is disabled in the configuration.");
    }

    g_first_person.last_eye_valid = false;
    g_first_person_view_forward_valid = false;
    g_first_person_reassert_frames = 0;
    g_first_person_health_warned = false;
    g_first_person_active.store(true, std::memory_order_release);
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
        "First person camera enabled (hotkey=0x%X, fov=%.1f, sourceFov=%.1f, "
        "eyeForward=%.2f, eyeUp=%.2f, nearClip=%.2f).",
        g_first_person_key.load(std::memory_order_relaxed),
        g_first_person_fov.load(std::memory_order_relaxed), original_fov,
        kFirstPersonEyeForward, kFirstPersonEyeUp, kFirstPersonNearClip);
    Log(buffer);
    return true;
}

void ExitFirstPerson(const char* reason) {
    const bool was_active=g_first_person_active.exchange(false,std::memory_order_acq_rel);
    FinishGameFirstPersonExit();
    if(!was_active) {
        if(!g_fp_mesh_session.patches.empty())ReleaseNeckCap();
        if(!g_first_person.snapshot_exit_owned)ReleaseSnapshotController();
        return;
    }
    ReleaseHeadPartHide();
    ReleaseNeckCap();
    if(!g_first_person.snapshot_exit_owned)ReleaseSnapshotController();
    ReleaseHeadTransform();
    g_first_person.character = nullptr;
    g_first_person_view_forward_valid = false;
    const uint64_t patches = g_push_state_patches.load(std::memory_order_relaxed);
    ReleaseCameraRoot();
    Log(std::string("First person camera disabled: ") + reason +
        " (patched frames=" + std::to_string(patches) + ").");
}

// Never invoke methods on a cached Entity after a leader change. Entity is not
// a Component; the verified Entity -> ModelCom -> ModelGo route supplies bones.
bool FirstPersonBoneBelongsToBody(void* bone, void* body) {
    if (!bone || !body) return false;
    for (int depth = 0; bone && depth < 48; ++depth) {
        if (!IsObjectAlive(bone)) return false;
        if (bone == body) return true;
        bone = Invoke(Contract("unity.transform.parent.get"), bone, nullptr);
    }
    return false;
}

void RefreshFirstPersonTarget(bool queue_facing) {
    void* character = Invoke(Contract("player_controller.get_main_character"),
        nullptr, nullptr);
    void* model=FindModelObjectForCharacter(character);
    if(model&&!IsObjectAlive(model))model=nullptr;
    const bool anchors_lost=(g_first_person.head&&!IsObjectAlive(g_first_person.head))||
        (g_first_person.body&&!IsObjectAlive(g_first_person.body))||
        (g_first_person.neck&&!IsObjectAlive(g_first_person.neck))||
        (g_first_person.head&&!FirstPersonBoneBelongsToBody(g_first_person.head,g_first_person.body))||
        (g_first_person.neck&&!FirstPersonBoneBelongsToBody(g_first_person.neck,g_first_person.body));
    if(character!=g_first_person.character||model!=g_first_person.model||anchors_lost) {
        g_first_person.target_valid=false;
        ReleaseHeadPartHide();
        ReleaseHeadTransform();
        ++g_first_person_binding_generation;
        if(g_diagnostics_enabled.load(std::memory_order_relaxed))
            Log("First person: target invalidated/rebound, generation="+
                std::to_string(g_first_person_binding_generation));
    }
    if(!character||!model) return;
    if(!g_first_person.model) {
        void* body=Invoke(Contract("unity.game_object.transform"),model,nullptr);
        if(!body||!IsObjectAlive(body))return;
        g_first_person.character=character;
        g_first_person.model=model;
        g_first_person.body=body;
        if(g_host&&g_host->gchandle_new) {
            g_first_person.character_handle=g_host->gchandle_new(g_host->context,character,0);
            g_first_person.model_handle=g_host->gchandle_new(g_host->context,model,0);
            g_first_person.body_handle=g_host->gchandle_new(g_host->context,body,0);
        }
        if(g_first_person_hide_head.load(std::memory_order_relaxed))ExitGameFirstPersonMode();
    }
    TryBindHeadTransform();
    g_first_person.target_valid=g_first_person.head&&IsObjectAlive(g_first_person.head)&&
        IsObjectAlive(g_first_person.body)&&
        FirstPersonBoneBelongsToBody(g_first_person.head,g_first_person.body)&&
        (!g_first_person.neck||FirstPersonBoneBelongsToBody(g_first_person.neck,g_first_person.body));
    if(g_first_person.target_valid&&queue_facing)g_first_person.facing_pending=true;
}

bool FirstPersonTargetIsCurrent() {
    if(!g_first_person.target_valid)return false;
    void* character=Invoke(Contract("player_controller.get_main_character"),nullptr,nullptr);
    const bool valid=character&&character==g_first_person.character&&
        FindModelObjectForCharacter(character)==g_first_person.model&&
        IsObjectAlive(g_first_person.model)&&IsObjectAlive(g_first_person.head)&&
        IsObjectAlive(g_first_person.body)&&
        FirstPersonBoneBelongsToBody(g_first_person.head,g_first_person.body)&&
        (!g_first_person.neck||FirstPersonBoneBelongsToBody(g_first_person.neck,g_first_person.body));
    if(!valid) {
        g_first_person.target_valid=false;
        g_first_person_view_forward_valid=false;
    }
    return valid;
}

bool FirstPersonBrainDrivesCamera(void* brain) {
    const MethodContract* game_object=Contract("unity.component.game_object");
    if(!brain||!game_object||!game_object->resolved||!IsObjectAlive(brain))return false;
    void* brain_object=Invoke(game_object,brain,nullptr);
    void* camera_object=Invoke(game_object,g_active_camera,nullptr);
    return brain_object&&brain_object==camera_object;
}

thread_local bool t_in_first_person_pump=false;
void PumpFirstPerson(bool allow_thread_capture = true,bool full_frame = true) {
    if(t_in_first_person_pump||!OnCameraUnityThread(allow_thread_capture))return;
    struct PumpGuard {
        PumpGuard(){t_in_first_person_pump=true;}
        ~PumpGuard(){t_in_first_person_pump=false;}
    } guard;
    FinishGameFirstPersonExit();
    if(!g_first_person_active.load(std::memory_order_acquire)) {
        if(!g_fp_mesh_session.patches.empty())ReleaseNeckCap();
        if(!g_first_person.snapshot_exit_owned)ReleaseSnapshotController();
    }
    const bool allowed = g_first_person_camera_enabled.load(std::memory_order_acquire) &&
        g_first_person_contract_ready;
    const bool exit_requested=g_first_person_exit_request.exchange(false,std::memory_order_acq_rel);
    if (!allowed || exit_requested) {
        g_first_person_toggle_request.store(false, std::memory_order_release);
        ExitFirstPerson(exit_requested?"requested exit":"feature disabled");
        return;
    }
    if (g_first_person_toggle_request.exchange(false, std::memory_order_acq_rel)) {
        if (g_first_person_active.load(std::memory_order_acquire)) {
            ExitFirstPerson("toggle hotkey");
        } else {
            EnterFirstPerson();
        }
    }
    if (!g_first_person_active.load(std::memory_order_acquire)) {
        return;
    }
    void* camera=Invoke(Contract("unity.camera.main"),nullptr,nullptr);
    if(!camera||camera!=g_active_camera||!IsObjectAlive(g_active_camera)) {
        ExitFirstPerson("active camera changed");
        return;
    }
    RefreshFirstPersonTarget(full_frame);
    if(!g_first_person.target_valid)return;
    if (!g_first_person_hide_head.load(std::memory_order_relaxed)) {
        if (g_first_person.head_hide_applied) ReleaseHeadPartHide();
    } else if (!full_frame) {
        return;
    } else if (!g_first_person.head_hide_applied) {
        ApplyHeadPartHide();
    }
    if (g_first_person.head_hide_applied) {
        EnsureNeckCap();
        if (++g_first_person_reassert_frames % 30 == 0) {
            ReapplyHeadPartHide();
        }
    }
    if (++g_first_person_pump_frames % 30 == 0) {
        FirstPersonHealthCheck();
    }
}
