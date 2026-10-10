#pragma once

#include "BetterEndfield/ModuleApi.h"

#include "engine_platform.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace EModLoader::Host {

class EngineResolver {
public:
    bool Initialize(std::string& error);
    bool AttachCurrentThread(std::string& error);
    void DetachCurrentThread();
    bool HasAssembly(const char* assembly_name) const;
    BE_Result ResolveMethod(const BE_MethodDescriptorV1& descriptor,
        BE_ResolvedMethodV1& result, std::string& error) const;
    BE_Result ResolveField(const BE_FieldDescriptorV1& descriptor,
        BE_ResolvedFieldV1& result, std::string& error) const;
    BE_Result ResolveClass(const char* assembly_name, const char* namespace_name,
        const char* class_name, BE_ResolvedClassV1& result, std::string& error) const;
    int CopyManagedString(const void* managed_string, char* destination,
        size_t destination_size) const;
    void* ObjectNew(const void* class_info) const;
    void* StringNew(const char* utf8) const;
    void* RuntimeInvoke(const void* method_info, void* instance, void** parameters,
        void** exception) const;
    void* ObjectUnbox(void* boxed_value) const;
    void* FieldGetValueObject(const void* field_info, void* instance) const;
    uint32_t GCHandleNew(void* object, bool pinned) const;
    void GCHandleFree(uint32_t handle) const;

private:
    using DomainGetFn = void* (EML_ENGINE_CALL*)();
    using DomainGetAssembliesFn = void** (EML_ENGINE_CALL*)(void*, size_t*);
    using AssemblyGetImageFn = void* (EML_ENGINE_CALL*)(void*);
    using ImageGetNameFn = const char* (EML_ENGINE_CALL*)(void*);
    using ClassFromNameFn = void* (EML_ENGINE_CALL*)(void*, const char*, const char*);
    using ClassGetNameFn = const char* (EML_ENGINE_CALL*)(void*);
    using ClassGetNestedTypesFn = void* (EML_ENGINE_CALL*)(void*, void**);
    using ClassGetMethodsFn = void* (EML_ENGINE_CALL*)(void*, void**);
    using MethodGetNameFn = const char* (EML_ENGINE_CALL*)(void*);
    using MethodGetParameterCountFn = uint32_t (EML_ENGINE_CALL*)(void*);
    using MethodGetParameterFn = const void* (EML_ENGINE_CALL*)(void*, uint32_t);
    using MethodGetReturnTypeFn = void* (EML_ENGINE_CALL*)(void*);
    using TypeGetNameFn = const char* (EML_ENGINE_CALL*)(void*);
    using ClassGetFieldFromNameFn = void* (EML_ENGINE_CALL*)(void*, const char*);
    using FieldGetOffsetFn = int32_t (EML_ENGINE_CALL*)(void*);
    using FieldGetTypeFn = void* (EML_ENGINE_CALL*)(void*);
    using StringLengthFn = int32_t (EML_ENGINE_CALL*)(void*);
    using StringCharsFn = EMLManagedChar* (EML_ENGINE_CALL*)(void*);
    using ThreadAttachFn = void* (EML_ENGINE_CALL*)(void*);
    using ThreadDetachFn = void(EML_ENGINE_CALL*)(void*);
    using ClassGetTypeFn = const void* (EML_ENGINE_CALL*)(void*);
    using TypeGetObjectFn = void* (EML_ENGINE_CALL*)(const void*);
    using ObjectNewFn = void* (EML_ENGINE_CALL*)(void*);
    using StringNewFn = void* (EML_ENGINE_CALL*)(const char*);
    using RuntimeInvokeFn = void* (EML_ENGINE_CALL*)(const void*, void*, void**, void**);
    using ObjectUnboxFn = void* (EML_ENGINE_CALL*)(void*);
    using FieldGetValueObjectFn = void* (EML_ENGINE_CALL*)(void*, void*);
    using GCHandleNewFn = uint32_t (EML_ENGINE_CALL*)(void*, bool);
    using GCHandleFreeFn = void(EML_ENGINE_CALL*)(uint32_t);

    void* FindImage(const char* assembly_name) const;
    void* FindClass(void* image, const char* namespace_name,
        const char* class_name) const;
    void* FindNestedClass(void* outer, const char* class_name) const;
    bool MatchesMethod(void* method, const BE_MethodDescriptorV1& descriptor) const;
    void* ResolveMethodEntry(void* method) const;
    bool IsExecutableAddress(uintptr_t address) const;
    void BuildExecutableRanges();

    HMODULE game_assembly_ = nullptr;
    DomainGetFn domain_get_ = nullptr;
    DomainGetAssembliesFn domain_get_assemblies_ = nullptr;
    AssemblyGetImageFn assembly_get_image_ = nullptr;
    ImageGetNameFn image_get_name_ = nullptr;
    ClassFromNameFn class_from_name_ = nullptr;
    ClassGetNameFn class_get_name_ = nullptr;
    ClassGetNestedTypesFn class_get_nested_types_ = nullptr;
    ClassGetMethodsFn class_get_methods_ = nullptr;
    MethodGetNameFn method_get_name_ = nullptr;
    MethodGetParameterCountFn method_get_parameter_count_ = nullptr;
    MethodGetParameterFn method_get_parameter_ = nullptr;
    MethodGetReturnTypeFn method_get_return_type_ = nullptr;
    TypeGetNameFn type_get_name_ = nullptr;
    ClassGetFieldFromNameFn class_get_field_from_name_ = nullptr;
    FieldGetOffsetFn field_get_offset_ = nullptr;
    FieldGetTypeFn field_get_type_ = nullptr;
    StringLengthFn string_length_ = nullptr;
    StringCharsFn string_chars_ = nullptr;
    ThreadAttachFn thread_attach_ = nullptr;
    ThreadDetachFn thread_detach_ = nullptr;
    ClassGetTypeFn class_get_type_ = nullptr;
    TypeGetObjectFn type_get_object_ = nullptr;
    ObjectNewFn object_new_ = nullptr;
    StringNewFn string_new_ = nullptr;
    RuntimeInvokeFn runtime_invoke_ = nullptr;
    ObjectUnboxFn object_unbox_ = nullptr;
    FieldGetValueObjectFn field_get_value_object_ = nullptr;
    GCHandleNewFn gchandle_new_ = nullptr;
    GCHandleFreeFn gchandle_free_ = nullptr;
    std::vector<std::pair<uintptr_t, uintptr_t>> executable_ranges_;
    void* attached_thread_ = nullptr;
};

} // namespace EModLoader::Host
