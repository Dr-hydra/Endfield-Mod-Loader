cmake_minimum_required(VERSION 3.25)
project(EModLoaderLoadingProbe LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This research harness verifies the Windows loader on the current workstation.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
get_filename_component(EML_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

add_executable(EModLoaderHostProbe
    "${EML_SOURCE_ROOT}/native/tests/third_party_host_test.cpp"
    "${EML_SOURCE_ROOT}/native/shared/third_party_modules/third_party_host.cpp")
target_link_libraries(EModLoaderHostProbe PRIVATE ws2_32)
add_library(EModLoaderEchoProbe SHARED
    "${EML_SOURCE_ROOT}/tools/ThirdPartyModules/echo/native/echo.cpp")
set_target_properties(EModLoaderEchoProbe PROPERTIES OUTPUT_NAME "example.echo")
foreach(target EModLoaderHostProbe EModLoaderEchoProbe)
    target_include_directories(${target} PRIVATE "${EML_SOURCE_ROOT}/native/shared/include")
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(${target} PRIVATE /W4 /utf-8 /permissive-)
endforeach()
