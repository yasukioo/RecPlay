function(recplay_add_bundle bundle_name)
    set(options)
    set(one_value_args)
    set(multi_value_args EXTRA_LIBS)
    cmake_parse_arguments(RPB "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    file(GLOB BUNDLE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
    add_library(${bundle_name} SHARED ${BUNDLE_SOURCES})

    target_include_directories(${bundle_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${PROJECT_SOURCE_DIR}/interfaces
    )

    target_link_libraries(${bundle_name}
        PRIVATE
            recplay_framework
            recplay_interfaces
            ${RPB_EXTRA_LIBS}
    )

    target_compile_definitions(${bundle_name}
        PRIVATE
            US_BUNDLE_NAME=${bundle_name}
    )

    set_target_properties(${bundle_name} PROPERTIES
        US_BUNDLE_NAME ${bundle_name}
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIG>/bundles
        LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIG>/bundles
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIG>/bundles
    )

    if(COMMAND usFunctionEmbedResources)
        usFunctionGetResourceSource(TARGET ${bundle_name} OUT _bundle_res_src APPEND)
        target_sources(${bundle_name} PRIVATE ${_bundle_res_src})
        usFunctionEmbedResources(TARGET ${bundle_name} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} FILES manifest.json)
    endif()
endfunction()
