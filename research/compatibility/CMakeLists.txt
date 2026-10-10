cmake_minimum_required(VERSION 3.25)
project(EMLPublishedCompatibilityProbe LANGUAGES CXX)
if(NOT WIN32)
    message(FATAL_ERROR "This probe exercises published Windows binaries on the current workstation.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
get_filename_component(EML_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# Keep the imported files byte-for-byte unchanged. The research copy changes
# only entry lookup to test the compatibility adapter with published binaries.
set(GENERATED_SHARED "${CMAKE_CURRENT_BINARY_DIR}/generated/shared")
file(MAKE_DIRECTORY "${GENERATED_SHARED}/third_party_modules" "${GENERATED_SHARED}/third_party/nlohmann")
configure_file("${EML_ROOT}/native/shared/third_party_modules/third_party_host.h"
    "${GENERATED_SHARED}/third_party_modules/third_party_host.h" COPYONLY)
configure_file("${EML_ROOT}/native/shared/third_party/nlohmann/json.hpp"
    "${GENERATED_SHARED}/third_party/nlohmann/json.hpp" COPYONLY)
file(READ "${EML_ROOT}/native/shared/third_party_modules/third_party_host.cpp" HOST_SOURCE)
set(ORIGINAL_LOOKUP "auto entry=reinterpret_cast<BE_GetThirdPartyModuleV1Fn>(GetProcAddress(static_cast<HMODULE>(module.library),\"BetterEndfieldNext_GetThirdPartyModuleV1\"));")
set(COMPATIBLE_LOOKUP "auto entry=reinterpret_cast<BE_GetThirdPartyModuleV1Fn>(GetProcAddress(static_cast<HMODULE>(module.library),\"BetterEndfield_GetThirdPartyModuleV1\"));")
string(FIND "${HOST_SOURCE}" "${ORIGINAL_LOOKUP}" LOOKUP_POSITION)
if(LOOKUP_POSITION LESS 0)
    message(FATAL_ERROR "Imported Host lookup changed; review the research adapter before generating it.")
endif()
string(REPLACE "${ORIGINAL_LOOKUP}" "${COMPATIBLE_LOOKUP}" HOST_SOURCE "${HOST_SOURCE}")
file(WRITE "${GENERATED_SHARED}/third_party_modules/third_party_host.cpp" "${HOST_SOURCE}")

add_executable(EMLCompatibilityHost
    "${EML_ROOT}/native/tests/third_party_host_test.cpp"
    "${GENERATED_SHARED}/third_party_modules/third_party_host.cpp")
target_include_directories(EMLCompatibilityHost PRIVATE "${EML_ROOT}/native/shared/include")
target_compile_definitions(EMLCompatibilityHost PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(EMLCompatibilityHost PRIVATE /W4 /utf-8 /permissive-)
target_link_libraries(EMLCompatibilityHost PRIVATE ws2_32)
