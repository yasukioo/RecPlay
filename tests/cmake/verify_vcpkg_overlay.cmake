cmake_minimum_required(VERSION 3.19)

if(NOT DEFINED RECPLAY_SOURCE_DIR)
    message(FATAL_ERROR "RECPLAY_SOURCE_DIR must be provided")
endif()

set(_root_cmake "${RECPLAY_SOURCE_DIR}/CMakeLists.txt")
set(_overlay_dir "${RECPLAY_SOURCE_DIR}/vcpkg-overlay-ports/cppmicroservices")
set(_overlay_portfile "${_overlay_dir}/portfile.cmake")
set(_overlay_manifest "${_overlay_dir}/vcpkg.json")
set(_overlay_patch "${_overlay_dir}/fix-msvc-14.51-spdlog-fmt.patch")

if(NOT EXISTS "${_root_cmake}")
    message(FATAL_ERROR "Missing root CMakeLists.txt at ${_root_cmake}")
endif()

file(READ "${_root_cmake}" _root_cmake_content)

if(NOT _root_cmake_content MATCHES "VCPKG_OVERLAY_PORTS")
    message(FATAL_ERROR "Expected CMakeLists.txt to configure VCPKG_OVERLAY_PORTS")
endif()

if(NOT _root_cmake_content MATCHES "list\\(FILTER _recplay_install_options EXCLUDE REGEX")
    message(FATAL_ERROR "Expected CMakeLists.txt to sanitize stale local VCPKG_INSTALL_OPTIONS entries")
endif()

if(NOT EXISTS "${_overlay_portfile}")
    message(FATAL_ERROR "Missing overlay portfile at ${_overlay_portfile}")
endif()

if(NOT EXISTS "${_overlay_manifest}")
    message(FATAL_ERROR "Missing overlay manifest at ${_overlay_manifest}")
endif()

if(NOT EXISTS "${_overlay_patch}")
    message(FATAL_ERROR "Missing MSVC compatibility patch at ${_overlay_patch}")
endif()

file(READ "${_overlay_portfile}" _overlay_portfile_content)
if(NOT _overlay_portfile_content MATCHES "fix-msvc-14.51-spdlog-fmt.patch")
    message(FATAL_ERROR "Overlay portfile does not apply the MSVC compatibility patch")
endif()

file(READ "${_overlay_patch}" _overlay_patch_content)
if(NOT _overlay_patch_content MATCHES "checked_ptr = T\\*")
    message(FATAL_ERROR "MSVC compatibility patch does not replace checked_ptr with raw pointers")
endif()

message(STATUS "RecPlay vcpkg overlay configuration looks correct")
