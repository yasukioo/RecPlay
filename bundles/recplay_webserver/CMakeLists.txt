recplay_add_bundle(recplay_webserver EXTRA_LIBS recplay_drogon recplay_spdlog recplay_json)

target_compile_definitions(recplay_webserver
    PRIVATE
        RECPLAY_BUILD_ROOT="${CMAKE_BINARY_DIR}"
        RECPLAY_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
)
