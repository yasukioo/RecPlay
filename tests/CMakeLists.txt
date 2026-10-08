add_executable(recplay_stats_tests
    unit/test_stats_collector.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_stats/src/StatsCollector.cpp
)

target_include_directories(recplay_stats_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_stats/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_stats_tests
    PRIVATE
        recplay_interfaces
)

add_test(NAME recplay_stats_tests COMMAND recplay_stats_tests)

add_executable(recplay_udp_tests
    unit/test_udp_protocol_service.cpp
    ${PROJECT_SOURCE_DIR}/bundles/protocol_udp/src/UdpProtocolService.cpp
)

target_include_directories(recplay_udp_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/protocol_udp/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_udp_tests
    PRIVATE
        recplay_interfaces
        recplay_boost_system
        recplay_json
)

add_test(NAME recplay_udp_tests COMMAND recplay_udp_tests)

add_executable(recplay_tcp_tests
    unit/test_tcp_protocol_service.cpp
    ${PROJECT_SOURCE_DIR}/bundles/protocol_tcp/src/TcpProtocolService.cpp
)

target_include_directories(recplay_tcp_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/protocol_tcp/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_tcp_tests
    PRIVATE
        recplay_interfaces
        recplay_boost_system
        recplay_json
)

add_test(NAME recplay_tcp_tests COMMAND recplay_tcp_tests)

add_executable(recplay_session_tests
    unit/test_session_controller.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_session/src/SessionController.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_session/src/SessionStateMachine.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src/CoreEngine.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src/TimelineEngine.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src/PacketScheduler.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src/InjectorBus.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src/IndexBuilder.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src/FilterMappingEngine.cpp
)

target_include_directories(recplay_session_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_session/src
        ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_session_tests
    PRIVATE
        recplay_interfaces
        recplay_json
)

add_test(NAME recplay_session_tests COMMAND recplay_session_tests)

add_executable(recplay_storage_tests
    unit/test_storage_service.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/StorageService.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/RpcapWriter.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/RpcapReader.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/IoBackendFactory.cpp
)

target_include_directories(recplay_storage_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_storage_tests
    PRIVATE
        recplay_interfaces
        recplay_json
        recplay_zstd
)

add_test(NAME recplay_storage_tests COMMAND recplay_storage_tests)

add_executable(recplay_ringbuffer_tests
    unit/test_ringbuffer.cpp
)

target_include_directories(recplay_ringbuffer_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_ringbuffer_tests
    PRIVATE
        recplay_interfaces
)

add_test(NAME recplay_ringbuffer_tests COMMAND recplay_ringbuffer_tests)

add_executable(recplay_timeline_tests
    unit/test_timeline.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src/TimelineEngine.cpp
)

target_include_directories(recplay_timeline_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_timeline_tests
    PRIVATE
        recplay_interfaces
)

add_test(NAME recplay_timeline_tests COMMAND recplay_timeline_tests)

add_executable(recplay_state_machine_tests
    unit/test_state_machine.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_session/src/SessionStateMachine.cpp
)

target_include_directories(recplay_state_machine_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_session/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_state_machine_tests
    PRIVATE
        recplay_interfaces
)

add_test(NAME recplay_state_machine_tests COMMAND recplay_state_machine_tests)

add_executable(recplay_object_pool_tests
    unit/test_object_pool.cpp
)

target_include_directories(recplay_object_pool_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_object_pool_tests
    PRIVATE
        recplay_interfaces
)

add_test(NAME recplay_object_pool_tests COMMAND recplay_object_pool_tests)

add_executable(recplay_index_builder_tests
    unit/test_index_builder.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src/IndexBuilder.cpp
)

target_include_directories(recplay_index_builder_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_index_builder_tests
    PRIVATE
        recplay_interfaces
)

add_test(NAME recplay_index_builder_tests COMMAND recplay_index_builder_tests)

add_executable(recplay_injector_tests
    unit/test_injector.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src/InjectorBus.cpp
)

target_include_directories(recplay_injector_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_core/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_injector_tests
    PRIVATE
        recplay_interfaces
        recplay_json
)

add_test(NAME recplay_injector_tests COMMAND recplay_injector_tests)

add_executable(recplay_rpcap_writer_reader_tests
    unit/test_rpcap_writer_reader.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/RpcapWriter.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/RpcapReader.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/IoBackendFactory.cpp
)

target_include_directories(recplay_rpcap_writer_reader_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_rpcap_writer_reader_tests
    PRIVATE
        recplay_interfaces
        recplay_json
        recplay_zstd
)

add_test(NAME recplay_rpcap_writer_reader_tests COMMAND recplay_rpcap_writer_reader_tests)

add_executable(recplay_webserver_tests
    unit/test_http_server.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/HttpServer.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/WebSocketServer.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/EventLogBuffer.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/StaticFileHandler.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/WebRootLocator.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/RpcapReader.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/IoBackendFactory.cpp
)

target_include_directories(recplay_webserver_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src
        ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_webserver_tests
    PRIVATE
        recplay_interfaces
        recplay_json
        recplay_zstd
        recplay_drogon
)

add_test(NAME recplay_webserver_tests COMMAND recplay_webserver_tests)

add_executable(recplay_websocket_tests
    unit/test_websocket_server.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/WebSocketServer.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/EventLogBuffer.cpp
)

target_include_directories(recplay_websocket_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_websocket_tests
    PRIVATE
        recplay_interfaces
        recplay_drogon
)

add_test(NAME recplay_websocket_tests COMMAND recplay_websocket_tests)

add_executable(recplay_websocket_transport_smoke_tests
    integration/test_websocket_transport_smoke.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/HttpServer.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/WebSocketServer.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/EventLogBuffer.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/RecPlayWebSocketController.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/StaticFileHandler.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/WebRootLocator.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/RpcapReader.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/IoBackendFactory.cpp
)

target_include_directories(recplay_websocket_transport_smoke_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src
        ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_websocket_transport_smoke_tests
    PRIVATE
        recplay_interfaces
        recplay_json
        recplay_zstd
        recplay_drogon
)

add_test(NAME recplay_websocket_transport_smoke_tests COMMAND recplay_websocket_transport_smoke_tests)

add_executable(recplay_record_playback_tests
    integration/test_record_playback.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/StorageService.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/RpcapWriter.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/RpcapReader.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/IoBackendFactory.cpp
    ${PROJECT_SOURCE_DIR}/bundles/protocol_udp/src/UdpProtocolService.cpp
)

target_include_directories(recplay_record_playback_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src
        ${PROJECT_SOURCE_DIR}/bundles/protocol_udp/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_record_playback_tests
    PRIVATE
        recplay_interfaces
        recplay_boost_system
        recplay_json
        recplay_zstd
)

add_test(NAME recplay_record_playback_tests COMMAND recplay_record_playback_tests)

add_executable(recplay_webserver_integration_tests
    integration/test_webserver.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/HttpServer.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/WebSocketServer.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/EventLogBuffer.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/StaticFileHandler.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/RpcapReader.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/IoBackendFactory.cpp
)

target_include_directories(recplay_webserver_integration_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src
        ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_webserver_integration_tests
    PRIVATE
        recplay_interfaces
        recplay_json
        recplay_zstd
        recplay_drogon
)

add_test(NAME recplay_webserver_integration_tests COMMAND recplay_webserver_integration_tests)

add_executable(recplay_runtime_smoke_tests
    integration/test_runtime_smoke.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/HttpServer.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/WebSocketServer.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/EventLogBuffer.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/StaticFileHandler.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/RpcapReader.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src/IoBackendFactory.cpp
)

target_include_directories(recplay_runtime_smoke_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src
        ${PROJECT_SOURCE_DIR}/bundles/recplay_storage/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_runtime_smoke_tests
    PRIVATE
        recplay_interfaces
        recplay_json
        recplay_zstd
        recplay_drogon
)

add_test(NAME recplay_runtime_smoke_tests COMMAND recplay_runtime_smoke_tests)

add_executable(recplay_bundle_path_resolution_tests
    integration/test_bundle_path_resolution.cpp
    ${PROJECT_SOURCE_DIR}/BundlePathResolver.cpp
)

target_include_directories(recplay_bundle_path_resolution_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/interfaces
)

add_test(NAME recplay_bundle_path_resolution_tests COMMAND recplay_bundle_path_resolution_tests)

add_executable(recplay_vcpkg_overlay_setup_tests
    unit/test_vcpkg_overlay_setup.cpp
)

target_compile_definitions(recplay_vcpkg_overlay_setup_tests
    PRIVATE
        RECPLAY_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_test(NAME recplay_vcpkg_overlay_setup_tests COMMAND recplay_vcpkg_overlay_setup_tests)
add_test(
    NAME recplay_vcpkg_overlay_contract_tests
    COMMAND ${CMAKE_COMMAND}
        -DRECPLAY_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -P ${PROJECT_SOURCE_DIR}/tests/cmake/verify_vcpkg_overlay.cmake
)

# Test targets recompile bundle sources that #include <spdlog/spdlog.h>, which
# pulls in vcpkg's external fmt (v11). Unlike the bundles (which link
# recplay_spdlog), the test executables otherwise miss fmt's compiled symbols
# (e.g. fmt::detail::assert_fail) and fail to link. Link recplay_spdlog into
# every test target so spdlog -> fmt::fmt is resolved transitively; it is an
# INTERFACE target, so it is harmless for the few tests that don't use spdlog.
set(_recplay_test_targets
    recplay_stats_tests
    recplay_udp_tests
    recplay_tcp_tests
    recplay_session_tests
    recplay_storage_tests
    recplay_ringbuffer_tests
    recplay_timeline_tests
    recplay_state_machine_tests
    recplay_object_pool_tests
    recplay_index_builder_tests
    recplay_injector_tests
    recplay_rpcap_writer_reader_tests
    recplay_webserver_tests
    recplay_websocket_tests
    recplay_websocket_transport_smoke_tests
    recplay_record_playback_tests
    recplay_webserver_integration_tests
    recplay_runtime_smoke_tests
    recplay_bundle_path_resolution_tests
    recplay_vcpkg_overlay_setup_tests
)
foreach(_recplay_test_target IN LISTS _recplay_test_targets)
    if(TARGET ${_recplay_test_target})
        target_link_libraries(${_recplay_test_target} PRIVATE recplay_spdlog)
    endif()
endforeach()
