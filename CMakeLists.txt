cmake_minimum_required(VERSION 3.19)

project(RecPlay VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_definitions(_WIN32_WINNT=0x0601)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FixCppMicroServicesPackage.cmake)
recplay_fix_cppmicroservices_package_exports()

find_package(CppMicroServices QUIET COMPONENTS Framework)
find_package(Boost QUIET COMPONENTS system)
find_package(nlohmann_json QUIET)
find_package(spdlog QUIET)
find_package(zstd QUIET)
find_package(Drogon QUIET)

add_library(recplay_interfaces INTERFACE)
target_include_directories(recplay_interfaces
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
)

add_library(recplay_framework INTERFACE)
if(TARGET CppMicroServices)
    target_link_libraries(recplay_framework INTERFACE CppMicroServices)
elseif(TARGET CppMicroServices::Framework)
    target_link_libraries(recplay_framework INTERFACE CppMicroServices::Framework)
endif()
if(DEFINED VCPKG_TARGET_TRIPLET)
    target_include_directories(recplay_framework
        INTERFACE
            ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/cppmicroservices3
    )
endif()

add_library(recplay_boost_system INTERFACE)
if(TARGET Boost::system)
    target_link_libraries(recplay_boost_system INTERFACE Boost::system)
endif()

add_library(recplay_json INTERFACE)
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(recplay_json INTERFACE nlohmann_json::nlohmann_json)
endif()

add_library(recplay_spdlog INTERFACE)
if(TARGET spdlog::spdlog)
    target_link_libraries(recplay_spdlog INTERFACE spdlog::spdlog)
elseif(TARGET spdlog::spdlog_header_only)
    target_link_libraries(recplay_spdlog INTERFACE spdlog::spdlog_header_only)
endif()

add_library(recplay_zstd INTERFACE)
if(TARGET zstd::libzstd_shared)
    target_link_libraries(recplay_zstd INTERFACE zstd::libzstd_shared)
elseif(TARGET zstd::libzstd_static)
    target_link_libraries(recplay_zstd INTERFACE zstd::libzstd_static)
endif()

add_library(recplay_drogon INTERFACE)
if(TARGET Drogon::Drogon)
    target_link_libraries(recplay_drogon INTERFACE Drogon::Drogon)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/RecPlayBundle.cmake)

add_subdirectory(bundles/recplay_core)
add_subdirectory(bundles/recplay_session)
add_subdirectory(bundles/recplay_webserver)
add_subdirectory(bundles/recplay_storage)
add_subdirectory(bundles/recplay_stats)
add_subdirectory(bundles/protocol_udp)
add_subdirectory(bundles/protocol_tcp)
add_subdirectory(bundles/codec_zstd)

add_executable(recplay main.cpp)
target_link_libraries(recplay
    PRIVATE
        recplay_framework
        recplay_interfaces
        recplay_json
        recplay_spdlog
)

find_program(NPM_EXECUTABLE NAMES npm npm.cmd)
if(NPM_EXECUTABLE)
    add_custom_target(
        web_build
        COMMAND ${NPM_EXECUTABLE} install
        COMMAND ${NPM_EXECUTABLE} run build
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/web
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/web/dist ${CMAKE_BINARY_DIR}/web
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/web
        COMMENT "Building web frontend"
    )
else()
    add_custom_target(
        web_build
        COMMAND ${CMAKE_COMMAND} -E echo "npm was not found; skipping web build."
    )
endif()
