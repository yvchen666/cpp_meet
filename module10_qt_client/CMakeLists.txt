if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found, skipping module10_qt_client")
    return()
endif()

add_library(module10_qt_client STATIC
    src/yuv_renderer.cpp
    src/pipeline.cpp
)
target_include_directories(module10_qt_client PUBLIC include)
target_link_libraries(module10_qt_client PUBLIC
    module01_network_core
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)
set_target_properties(module10_qt_client PROPERTIES AUTOMOC ON)

add_executable(module10_tests tests/test_pipeline.cpp)
target_link_libraries(module10_tests PRIVATE module10_qt_client GTest::gtest_main)
set_target_properties(module10_tests PROPERTIES AUTOMOC ON)
add_test(NAME module10_tests COMMAND module10_tests)
