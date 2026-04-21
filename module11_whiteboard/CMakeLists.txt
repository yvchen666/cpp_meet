if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found, skipping module11_whiteboard")
    return()
endif()

add_library(module11_whiteboard STATIC src/crdt_whiteboard.cpp)
target_include_directories(module11_whiteboard PUBLIC include)
target_link_libraries(module11_whiteboard PUBLIC
    module10_qt_client
    nlohmann_json::nlohmann_json
)

add_executable(module11_tests tests/test_crdt.cpp)
target_link_libraries(module11_tests PRIVATE module11_whiteboard GTest::gtest_main)
add_test(NAME module11_tests COMMAND module11_tests)
