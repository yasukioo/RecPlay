function(recplay_fix_cppmicroservices_package_exports)
    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
        return()
    endif()

    set(_cms_cmake_dir
        "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/cppmicroservices/cmake"
    )
    if(NOT EXISTS "${_cms_cmake_dir}")
        return()
    endif()

    file(GLOB _cms_config_files "${_cms_cmake_dir}/*Config.cmake")
    foreach(_config_file IN LISTS _cms_config_files)
        file(READ "${_config_file}" _config_content)
        set(_updated_content "${_config_content}")
        string(REGEX REPLACE
            "get_filename_component\\(PACKAGE_PREFIX_DIR \"\\$\\{CMAKE_CURRENT_LIST_DIR\\}/(\\.\\./)+\" ABSOLUTE\\)"
            "get_filename_component(PACKAGE_PREFIX_DIR \"\${CMAKE_CURRENT_LIST_DIR}/../../../\" ABSOLUTE)"
            _updated_content
            "${_updated_content}"
        )
        if(NOT _updated_content STREQUAL _config_content)
            file(WRITE "${_config_file}" "${_updated_content}")
        endif()
    endforeach()

    file(GLOB _cms_targets_files "${_cms_cmake_dir}/*Targets.cmake")
    foreach(_targets_file IN LISTS _cms_targets_files)
        file(READ "${_targets_file}" _targets_content)
        set(_updated_targets_content "${_targets_content}")
        string(REGEX REPLACE
            "get_filename_component\\(_IMPORT_PREFIX \"\\$\\{CMAKE_CURRENT_LIST_FILE\\}\" PATH\\)[\r\n]+(get_filename_component\\(_IMPORT_PREFIX \"\\$\\{_IMPORT_PREFIX\\}\" PATH\\)[\r\n]+)+"
            "get_filename_component(_IMPORT_PREFIX \"\${CMAKE_CURRENT_LIST_FILE}\" PATH)\nget_filename_component(_IMPORT_PREFIX \"\${_IMPORT_PREFIX}\" PATH)\nget_filename_component(_IMPORT_PREFIX \"\${_IMPORT_PREFIX}\" PATH)\nget_filename_component(_IMPORT_PREFIX \"\${_IMPORT_PREFIX}\" PATH)\n"
            _updated_targets_content
            "${_updated_targets_content}"
        )
        if(NOT _updated_targets_content STREQUAL _targets_content)
            file(WRITE "${_targets_file}" "${_updated_targets_content}")
        endif()
    endforeach()
endfunction()
