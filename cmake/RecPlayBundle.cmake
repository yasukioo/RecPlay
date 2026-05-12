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

    set_target_properties(${bundle_name} PROPERTIES
        US_BUNDLE_NAME ${bundle_name}
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bundles/$<CONFIG>
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bundles/$<CONFIG>
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bundles/$<CONFIG>
    )

    if(COMMAND usFunctionEmbedResources)
        usFunctionEmbedResources(TARGET ${bundle_name} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} FILES manifest.json)
    endif()
endfunction()
