add_executable(stb_image_test stb_image_test.cpp)
target_link_libraries(stb_image_test PRIVATE stb_image)
target_include_directories(stb_image_test SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/stb")

# Абсолютные пути позволяют запускать CTest из каталога сборки на обеих ОС.
add_test(NAME stb_image_pixels COMMAND stb_image_test pixels
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/rgba_2x2.png")
add_test(NAME stb_image_missing COMMAND stb_image_test missing
    "${CMAKE_CURRENT_BINARY_DIR}/stb_image_missing.png")
add_test(NAME stb_image_corrupt COMMAND stb_image_test corrupt
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/truncated.png")

add_executable(miniaudio_test miniaudio_test.cpp)
target_link_libraries(miniaudio_test PRIVATE miniaudio)
target_include_directories(miniaudio_test SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/miniaudio")

add_test(NAME miniaudio_samples COMMAND miniaudio_test samples
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/pcm_s16_stereo.wav")
add_test(NAME miniaudio_missing COMMAND miniaudio_test missing
    "${CMAKE_CURRENT_BINARY_DIR}/miniaudio_missing.wav")
add_test(NAME miniaudio_corrupt COMMAND miniaudio_test corrupt
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/truncated.wav")

# Платформенные проверки используют настоящую реализацию, без подмены системных вызовов.
# Системные библиотеки нужны для линковки, но графическая сессия при запуске не требуется.
if(WIN32)
    set(PLATFORM_TEST_SOURCE "${PROJECT_SOURCE_DIR}/platform/platform_windows.cpp")
    set(PLATFORM_TEST_LIBRARIES opengl32 gdi32 user32 winmm)
elseif(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    set(PLATFORM_TEST_SOURCE "${PROJECT_SOURCE_DIR}/platform/platform_x11.cpp")
    set(PLATFORM_TEST_LIBRARIES X11::X11 OpenGL::GL)
else()
    message(FATAL_ERROR "Platform tests currently support Windows and Linux/X11.")
endif()

add_executable(platform_cleanup_test platform_cleanup_test.cpp ${PLATFORM_TEST_SOURCE})
target_include_directories(platform_cleanup_test PRIVATE "${PROJECT_SOURCE_DIR}/engine")
target_link_libraries(platform_cleanup_test PRIVATE ${PLATFORM_TEST_LIBRARIES})
add_test(NAME platform_cleanup COMMAND platform_cleanup_test)

set(TEST_TARGETS stb_image_test miniaudio_test platform_cleanup_test)
if(UNIX AND NOT APPLE)
    # Собираем точку входа демо для интеграционной проверки даже при BUILD_DEMOS=OFF.
    add_executable(platform_startup_test
        "${PROJECT_SOURCE_DIR}/demos/circle/main.cpp"
        "${PROJECT_SOURCE_DIR}/demos/circle/game.cpp"
        ${PLATFORM_TEST_SOURCE}
    )
    target_include_directories(platform_startup_test PRIVATE "${PROJECT_SOURCE_DIR}/engine")
    target_link_libraries(platform_startup_test PRIVATE ${PLATFORM_TEST_LIBRARIES})
    list(APPEND TEST_TARGETS platform_startup_test)
    add_test(NAME platform_no_display COMMAND "${CMAKE_COMMAND}"
        "-DAPPLICATION=$<TARGET_FILE:platform_startup_test>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_no_display.cmake")
    set_tests_properties(platform_no_display PROPERTIES TIMEOUT 15)
endif()

# Одинаковый уровень предупреждений для тестовых программ.
foreach(testTarget IN LISTS TEST_TARGETS)
    if(MSVC)
        target_compile_options(${testTarget} PRIVATE /W4)
    else()
        target_compile_options(${testTarget} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
