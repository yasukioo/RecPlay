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
)

add_test(NAME recplay_session_tests COMMAND recplay_session_tests)

add_executable(recplay_webserver_tests
    unit/test_http_server.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/HttpServer.cpp
)

target_include_directories(recplay_webserver_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_webserver_tests
    PRIVATE
        recplay_interfaces
)

add_test(NAME recplay_webserver_tests COMMAND recplay_webserver_tests)

add_executable(recplay_websocket_tests
    unit/test_websocket_server.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/WebSocketServer.cpp
)

target_include_directories(recplay_websocket_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_websocket_tests
    PRIVATE
        recplay_interfaces
)

add_test(NAME recplay_websocket_tests COMMAND recplay_websocket_tests)

add_executable(recplay_runtime_smoke_tests
    integration/test_runtime_smoke.cpp
    ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src/HttpServer.cpp
)

target_include_directories(recplay_runtime_smoke_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/bundles/recplay_webserver/src
        ${PROJECT_SOURCE_DIR}/interfaces
)

target_link_libraries(recplay_runtime_smoke_tests
    PRIVATE
        recplay_interfaces
)

add_test(NAME recplay_runtime_smoke_tests COMMAND recplay_runtime_smoke_tests)
