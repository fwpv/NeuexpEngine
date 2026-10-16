add_executable(stb_image_test stb_image_test.cpp)
target_link_libraries(stb_image_test PRIVATE stb_image)
target_include_directories(stb_image_test SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/stb")
if(MSVC)
    target_compile_options(stb_image_test PRIVATE /W4)
else()
    target_compile_options(stb_image_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Абсолютные пути позволяют запускать CTest из каталога сборки на обеих ОС.
add_test(NAME stb_image_pixels COMMAND stb_image_test pixels
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/rgba_2x2.png")
add_test(NAME stb_image_missing COMMAND stb_image_test missing
    "${CMAKE_CURRENT_BINARY_DIR}/stb_image_missing.png")
add_test(NAME stb_image_corrupt COMMAND stb_image_test corrupt
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/truncated.png")
