add_executable(platform_startup_rollback_test platform_startup_rollback_test.cpp
    "${PROJECT_SOURCE_DIR}/platform/platform_windows.cpp"
    "${PROJECT_SOURCE_DIR}/platform/window_validation.cpp")
target_compile_definitions(platform_startup_rollback_test PRIVATE NOMINMAX)
target_include_directories(platform_startup_rollback_test PRIVATE "${PROJECT_SOURCE_DIR}/engine")
target_link_libraries(platform_startup_rollback_test PRIVATE neuexpengine project_warnings opengl32 gdi32 user32 winmm)
set(WGL_TEST_OVERRIDE "${CMAKE_CURRENT_SOURCE_DIR}/windows_gl_override.h")
if(MSVC)
    target_compile_options(platform_startup_rollback_test PRIVATE "/FI${WGL_TEST_OVERRIDE}")
else()
    target_compile_options(platform_startup_rollback_test PRIVATE -include "${WGL_TEST_OVERRIDE}")
endif()
add_test(NAME platform_startup_rollback COMMAND platform_startup_rollback_test)
set_tests_properties(platform_startup_rollback PROPERTIES TIMEOUT 30 LABELS "display" RUN_SERIAL TRUE)
