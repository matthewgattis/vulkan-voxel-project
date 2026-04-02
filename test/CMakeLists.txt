add_executable(tests
    test_buffer.cpp
    test_pipeline.cpp
    test_vertex.cpp
    test_glass.cpp
    test_xr.cpp
)

target_link_libraries(tests PRIVATE
    steel
    glass
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(tests)
