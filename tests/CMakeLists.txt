set(TEST_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

add_subdirectory(common)
if(UNIX AND NOT APPLE)
    add_subdirectory(linux)
elseif(WIN32)
    add_subdirectory(input_errors)
endif()

add_subdirectory(vsync)
add_subdirectory(cleanup_errors)
