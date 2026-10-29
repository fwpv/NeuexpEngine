add_executable(renderer_test renderer_test.cpp)
target_link_libraries(renderer_test PRIVATE engine_renderer platform project_warnings)
if(WIN32)
    target_compile_definitions(renderer_test PRIVATE NOMINMAX)
endif()
foreach(mode primitives transforms views errors state sorting gradient ellipses debug_text frame_errors)
    add_test(NAME renderer_${mode} COMMAND renderer_test ${mode})
    set_tests_properties(renderer_${mode} PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)
endforeach()

add_executable(renderer_lifecycle_test renderer_lifecycle_test.cpp)
target_link_libraries(renderer_lifecycle_test PRIVATE engine_application project_warnings)
if(WIN32)
    target_compile_definitions(renderer_lifecycle_test PRIVATE NOMINMAX)
endif()
add_test(NAME renderer_lifecycle COMMAND renderer_lifecycle_test)
set_tests_properties(renderer_lifecycle PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)

add_executable(camera_test camera_test.cpp)
target_link_libraries(camera_test PRIVATE engine_camera project_warnings)
add_test(NAME camera COMMAND camera_test)

add_executable(ui_test ui_test.cpp)
target_link_libraries(ui_test PRIVATE engine_ui project_warnings)
add_test(NAME ui COMMAND ui_test)

add_executable(platform_dpi_test platform_dpi_test.cpp)
target_link_libraries(platform_dpi_test PRIVATE platform engine_ui project_warnings)
if(WIN32)
    target_compile_definitions(platform_dpi_test PRIVATE NOMINMAX)
else()
    target_link_options(platform_dpi_test PRIVATE "-Wl,--wrap=XGetTextProperty" "-Wl,--wrap=XNextEvent")
endif()
add_test(NAME platform_dpi COMMAND platform_dpi_test)
set_tests_properties(platform_dpi PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)

foreach(group math geometry transform)
    add_executable(${group}_test ${group}_test.cpp)
    target_link_libraries(${group}_test PRIVATE engine_math project_warnings)
    add_test(NAME ${group} COMMAND ${group}_test)
endforeach()

add_executable(event_dispatcher_test event_dispatcher_test.cpp)
target_link_libraries(event_dispatcher_test PRIVATE engine_events engine_input project_warnings)
add_test(NAME event_dispatcher COMMAND event_dispatcher_test)

add_executable(input_state_test input_state_test.cpp)
target_link_libraries(input_state_test PRIVATE engine_input project_warnings)
add_test(NAME input_state COMMAND input_state_test)

add_executable(platform_input_test platform_input_test.cpp)
target_link_libraries(platform_input_test PRIVATE platform project_warnings)
if(WIN32)
    target_compile_definitions(platform_input_test PRIVATE NOMINMAX)
else()
    target_link_options(platform_input_test PRIVATE "-Wl,--wrap=Xutf8LookupString"
        "-Wl,--wrap=XFilterEvent" "-Wl,--wrap=XkbSetDetectableAutoRepeat" "-Wl,--wrap=XCloseDisplay"
        "-Wl,--wrap=XNextEvent")
endif()
add_test(NAME platform_input COMMAND platform_input_test)
set_tests_properties(platform_input PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60 LABELS "display" RUN_SERIAL TRUE)
if(UNIX AND NOT APPLE)
    add_test(NAME platform_input_legacy_repeat COMMAND platform_input_test legacy-repeat)
    add_test(NAME platform_input_constructor_rollback COMMAND platform_input_test constructor-rollback)
    set_tests_properties(platform_input platform_input_legacy_repeat platform_input_constructor_rollback PROPERTIES
        ENVIRONMENT "XMODIFIERS=@im=none" SKIP_RETURN_CODE 77 TIMEOUT 60 LABELS "display" RUN_SERIAL TRUE)
endif()

add_executable(stb_image_test stb_image_test.cpp)
target_link_libraries(stb_image_test PRIVATE stb_image)
target_include_directories(stb_image_test SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/stb")

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

add_executable(platform_cleanup_test platform_cleanup_test.cpp)
target_link_libraries(platform_cleanup_test PRIVATE platform)
add_test(NAME platform_cleanup COMMAND platform_cleanup_test)

add_executable(platform_validation_test platform_validation_test.cpp)
target_link_libraries(platform_validation_test PRIVATE platform)
add_test(NAME platform_validation COMMAND platform_validation_test)

add_executable(platform_window_test platform_window_test.cpp)
target_include_directories(platform_window_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
if(WIN32)
    set(WINDOW_PLATFORM_SOURCE "${PROJECT_SOURCE_DIR}/platform/platform_windows.cpp")
    set(PLATFORM_OVERRIDE "${PROJECT_SOURCE_DIR}/tests/windows/windows_platform_override.h")
    if(MSVC)
        set_source_files_properties("${WINDOW_PLATFORM_SOURCE}" PROPERTIES COMPILE_OPTIONS "/FI${PLATFORM_OVERRIDE}")
    else()
        set_source_files_properties("${WINDOW_PLATFORM_SOURCE}" PROPERTIES COMPILE_OPTIONS "-include;${PLATFORM_OVERRIDE}")
    endif()
    target_sources(platform_window_test PRIVATE "${WINDOW_PLATFORM_SOURCE}" ../windows/keyboard_input.cpp ../windows/presentation_counter.cpp)
    target_include_directories(platform_window_test PRIVATE "${PROJECT_SOURCE_DIR}/engine")
    target_compile_definitions(platform_window_test PRIVATE NOMINMAX)
    target_link_libraries(platform_window_test PRIVATE engine_input opengl32 gdi32 user32 winmm)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(platform_window_test PRIVATE platform)
    target_sources(platform_window_test PRIVATE ../linux/keyboard_query_counter.cpp ../linux/presentation_counter.cpp)
    target_link_options(platform_window_test PRIVATE "-Wl,--wrap=XQueryKeymap" "-Wl,--wrap=glXSwapBuffers")
endif()
add_test(NAME platform_window COMMAND platform_window_test)
add_test(NAME platform_window_minimize COMMAND platform_window_test minimize)
add_test(NAME platform_keyboard COMMAND platform_window_test keyboard)
set_tests_properties(platform_window platform_window_minimize platform_keyboard PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60 LABELS "display" RUN_SERIAL TRUE)

add_executable(platform_rendering_test platform_rendering_test.cpp)
target_link_libraries(platform_rendering_test PRIVATE platform project_warnings)
if(WIN32)
    target_compile_definitions(platform_rendering_test PRIVATE NOMINMAX)
endif()
add_test(NAME platform_rendering COMMAND platform_rendering_test)
set_tests_properties(platform_rendering PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)

add_executable(platform_viewport_test platform_viewport_test.cpp)
target_link_libraries(platform_viewport_test PRIVATE platform)
if(WIN32)
    target_compile_definitions(platform_viewport_test PRIVATE NOMINMAX)
endif()
add_test(NAME platform_viewport COMMAND platform_viewport_test)
set_tests_properties(platform_viewport PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)

set(TEST_TARGETS stb_image_test miniaudio_test platform_cleanup_test platform_validation_test platform_window_test platform_viewport_test)

foreach(testTarget IN LISTS TEST_TARGETS)
    target_link_libraries(${testTarget} PRIVATE project_warnings)
endforeach()

add_executable(frame_clock_test frame_clock_test.cpp)
target_link_libraries(frame_clock_test PRIVATE engine_time project_warnings)
add_test(NAME frame_clock COMMAND frame_clock_test)

# Тестовая копия исходника заменяет только часы и ожидание, без привязки к ABI стандартной библиотеки.
set(APPLICATION_SOURCE "${PROJECT_SOURCE_DIR}/engine/application.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${APPLICATION_SOURCE}")
file(READ "${APPLICATION_SOURCE}" APPLICATION_TEST_SOURCE)
foreach(dependency "using Clock = FrameClock::Clock;" "std::this_thread::sleep_until(")
    string(FIND "${APPLICATION_TEST_SOURCE}" "${dependency}" dependencyPosition)
    if(dependencyPosition EQUAL -1)
        message(FATAL_ERROR "Application test timing substitution no longer matches engine/application.cpp: ${dependency}")
    endif()
endforeach()
string(REPLACE "using Clock = FrameClock::Clock;" "using Clock = ApplicationTest::Clock;"
    APPLICATION_TEST_SOURCE "${APPLICATION_TEST_SOURCE}")
string(REPLACE "std::this_thread::sleep_until(" "ApplicationTest::sleepUntil("
    APPLICATION_TEST_SOURCE "${APPLICATION_TEST_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/application_under_test.cpp"
    "#include \"application_runtime.h\"\n\n${APPLICATION_TEST_SOURCE}\n")

add_executable(application_test application_test.cpp application_runtime.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/application_under_test.cpp")
target_include_directories(application_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(application_test PRIVATE engine_time engine_events engine_input engine_renderer project_warnings)

foreach(group lifecycle pause failures vsync limiter statistics)
    add_test(NAME application_${group} COMMAND application_test ${group})
    set_tests_properties(application_${group} PROPERTIES TIMEOUT 15)
endforeach()

add_executable(resources_test resources_test.cpp)
target_link_libraries(resources_test PRIVATE engine_resources project_warnings)
add_custom_command(TARGET resources_test POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${TEST_ASSETS_DIR}" "$<TARGET_FILE_DIR:resources_test>/assets")
add_test(NAME resources COMMAND resources_test)

add_executable(texture_test texture_test.cpp)
target_link_libraries(texture_test PRIVATE engine_renderer platform project_warnings)
if(WIN32)
    target_compile_definitions(texture_test PRIVATE NOMINMAX)
endif()
add_custom_command(TARGET texture_test POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${TEST_ASSETS_DIR}" "$<TARGET_FILE_DIR:texture_test>/assets")
foreach(mode pixels sampling integration resources)
    add_test(NAME texture_${mode} COMMAND texture_test ${mode})
    set_tests_properties(texture_${mode} PROPERTIES
        SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)
endforeach()
