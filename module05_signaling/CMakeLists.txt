add_library(module05_signaling STATIC
    src/websocket_server.cpp
    src/sdp.cpp
    src/room_manager.cpp
)
target_include_directories(module05_signaling PUBLIC include)
target_link_libraries(module05_signaling PUBLIC
    module01_network_core
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

add_executable(module05_tests
    tests/test_websocket.cpp
    tests/test_sdp.cpp
)
target_link_libraries(module05_tests PRIVATE
    module05_signaling
    GTest::gtest_main
)
add_test(NAME module05_tests COMMAND module05_tests)
