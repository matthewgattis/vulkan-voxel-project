add_executable(voxel_tests
    test_voxel_types.cpp
    test_chunk.cpp
    test_terrain_generator.cpp
    test_chunk_mesh.cpp
    test_chunk_manager.cpp

    # Voxel sources under test (no GPU/Application dependencies)
    ../voxel/src/chunk.cpp
    ../voxel/src/chunk_mesh.cpp
    ../voxel/src/terrain_generator.cpp
)

target_include_directories(voxel_tests PRIVATE
    ../voxel/include
)

target_link_libraries(voxel_tests PRIVATE
    glass
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(voxel_tests)
