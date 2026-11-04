set(TEST_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

add_subdirectory(common)
if(UNIX AND NOT APPLE)
    add_subdirectory(linux)
endif()

add_subdirectory(vsync)
