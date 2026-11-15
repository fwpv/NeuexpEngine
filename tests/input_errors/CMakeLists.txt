add_executable(platform_input_errors_test windows_input_errors_test.cpp
    "${PROJECT_SOURCE_DIR}/platform/platform_windows.cpp"
    "${PROJECT_SOURCE_DIR}/platform/window_validation.cpp")
target_include_directories(platform_input_errors_test PRIVATE "${PROJECT_SOURCE_DIR}/engine")
target_compile_definitions(platform_input_errors_test PRIVATE NOMINMAX)
target_link_libraries(platform_input_errors_test PRIVATE neuexpengine_input project_warnings opengl32 gdi32 user32 winmm)

set(INPUT_OVERRIDE "${CMAKE_CURRENT_SOURCE_DIR}/windows_input_override.h")
if(MSVC)
    target_compile_options(platform_input_errors_test PRIVATE "/FI${INPUT_OVERRIDE}")
else()
    target_compile_options(platform_input_errors_test PRIVATE -include "${INPUT_OVERRIDE}")
endif()

add_test(NAME platform_input_errors COMMAND platform_input_errors_test)
set_tests_properties(platform_input_errors PROPERTIES TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)
