cmake_minimum_required(VERSION 3.19)

set(_recplay_vcpkg_overlay_dir "${CMAKE_CURRENT_LIST_DIR}/vcpkg-overlay-ports")
set(_recplay_overlay_ports "${_recplay_vcpkg_overlay_dir}")
if(DEFINED VCPKG_OVERLAY_PORTS AND NOT VCPKG_OVERLAY_PORTS STREQUAL "")
    list(APPEND _recplay_overlay_ports ${VCPKG_OVERLAY_PORTS})
endif()
list(REMOVE_DUPLICATES _recplay_overlay_ports)
set(
    VCPKG_OVERLAY_PORTS
    "${_recplay_overlay_ports}"
    CACHE STRING "RecPlay overlay ports"
    FORCE
)

set(_recplay_install_options)
if(DEFINED VCPKG_INSTALL_OPTIONS AND NOT VCPKG_INSTALL_OPTIONS STREQUAL "")
    set(_recplay_install_options ${VCPKG_INSTALL_OPTIONS})
    list(FILTER _recplay_install_options EXCLUDE REGEX "--x-buildtrees-root=.*/\\.vcpkg/buildtrees")
    list(FILTER _recplay_install_options EXCLUDE REGEX "--x-packages-root=.*/\\.vcpkg/packages")
    list(FILTER _recplay_install_options EXCLUDE REGEX "--downloads-root=.*/\\.vcpkg/downloads")
    list(REMOVE_DUPLICATES _recplay_install_options)
    set(
        VCPKG_INSTALL_OPTIONS
        "${_recplay_install_options}"
        CACHE STRING "RecPlay filtered vcpkg install options"
        FORCE
    )
endif()

project(RecPlay VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIG>)
add_compile_definitions(_WIN32_WINNT=0x0601)
enable_testing()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FixCppMicroServicesPackage.cmake)
recplay_fix_cppmicroservices_package_exports()

find_package(CppMicroServices QUIET COMPONENTS Framework)
find_package(Boost QUIET COMPONENTS system)
find_package(nlohmann_json QUIET)
find_package(spdlog QUIET)
find_package(zstd QUIET)
find_package(Drogon QUIET)

if(WIN32 AND TARGET usResourceCompiler AND DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    get_target_property(_recplay_rcc_executable usResourceCompiler IMPORTED_LOCATION_DEBUG)
    if(NOT _recplay_rcc_executable OR _recplay_rcc_executable MATCHES "-NOTFOUND$")
        get_target_property(_recplay_rcc_executable usResourceCompiler IMPORTED_LOCATION_RELEASE)
    endif()

    if(_recplay_rcc_executable AND NOT _recplay_rcc_executable MATCHES "-NOTFOUND$")
        set(_recplay_rcc_wrapper "${CMAKE_BINARY_DIR}/recplay_usResourceCompiler.cmd")
        file(TO_NATIVE_PATH "${_recplay_rcc_executable}" _recplay_rcc_executable)
        file(TO_NATIVE_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin" _recplay_rcc_vcpkg_bin)
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/usResourceCompiler-wrapper.cmd.in
            ${_recplay_rcc_wrapper}
            @ONLY
        )
        foreach(_config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            set_target_properties(usResourceCompiler PROPERTIES
                IMPORTED_LOCATION_${_config} "${_recplay_rcc_wrapper}"
            )
        endforeach()
        set_target_properties(usResourceCompiler PROPERTIES
            IMPORTED_LOCATION "${_recplay_rcc_wrapper}"
        )
    endif()
endif()

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

add_executable(recplay
    main.cpp
    BundlePathResolver.cpp
)
set_target_properties(recplay PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIG>
)
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

add_subdirectory(tests)
