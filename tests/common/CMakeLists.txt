add_executable(stb_image_test stb_image_test.cpp)
target_link_libraries(stb_image_test PRIVATE stb_image)
target_include_directories(stb_image_test SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/stb")

# Абсолютные пути позволяют запускать CTest из каталога сборки на обеих ОС.
add_test(NAME stb_image_pixels COMMAND stb_image_test pixels
    "${TEST_ASSETS_DIR}/rgba_2x2.png")
add_test(NAME stb_image_missing COMMAND stb_image_test missing
    "${CMAKE_CURRENT_BINARY_DIR}/stb_image_missing.png")
add_test(NAME stb_image_corrupt COMMAND stb_image_test corrupt
    "${TEST_ASSETS_DIR}/truncated.png")

add_executable(miniaudio_test miniaudio_test.cpp)
target_link_libraries(miniaudio_test PRIVATE miniaudio)
target_include_directories(miniaudio_test SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/miniaudio")

add_test(NAME miniaudio_samples COMMAND miniaudio_test samples
    "${TEST_ASSETS_DIR}/pcm_s16_stereo.wav")
add_test(NAME miniaudio_missing COMMAND miniaudio_test missing
    "${CMAKE_CURRENT_BINARY_DIR}/miniaudio_missing.wav")
add_test(NAME miniaudio_corrupt COMMAND miniaudio_test corrupt
    "${TEST_ASSETS_DIR}/truncated.wav")

# Проверки подключают настоящую платформенную библиотеку без подмены системных вызовов.
# Графическая сессия для этих сценариев не требуется.
add_executable(platform_cleanup_test platform_cleanup_test.cpp)
target_link_libraries(platform_cleanup_test PRIVATE platform)
add_test(NAME platform_cleanup COMMAND platform_cleanup_test)

add_executable(platform_validation_test platform_validation_test.cpp)
target_link_libraries(platform_validation_test PRIVATE platform)
add_test(NAME platform_validation COMMAND platform_validation_test)

add_executable(platform_window_test platform_window_test.cpp)
target_link_libraries(platform_window_test PRIVATE platform)
if(UNIX AND NOT APPLE)
    target_link_libraries(platform_window_test PRIVATE ${CMAKE_DL_LIBS})
    target_sources(platform_window_test PRIVATE ../linux/keyboard_query_counter.cpp)
    target_link_options(platform_window_test PRIVATE "-Wl,--wrap=XQueryKeymap")
endif()
add_test(NAME platform_window COMMAND platform_window_test)
add_test(NAME platform_window_minimize COMMAND platform_window_test minimize)
add_test(NAME platform_keyboard COMMAND platform_window_test keyboard)
set_tests_properties(platform_window platform_window_minimize platform_keyboard PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60 LABELS "display" RUN_SERIAL TRUE)

set(TEST_TARGETS stb_image_test miniaudio_test platform_cleanup_test platform_validation_test platform_window_test)

# Одинаковый уровень предупреждений для тестовых программ.
foreach(testTarget IN LISTS TEST_TARGETS)
    target_link_libraries(${testTarget} PRIVATE project_warnings)
endforeach()
